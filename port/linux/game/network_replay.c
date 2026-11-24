/*
NETWORK_REPLAY.C

The host's replay recording: with network.record_replays on, the host of a multiplayer network game (a player's or a
dedicated server) writes what a client that sees every player would be sent to a file in the save root's recordings
folder, from the game's first tick until its map is unloaded. Campaigns are not recorded.

That client is the recorder: a machine index past the session's (network_distributed.h's
DISTRIBUTED_RECORDER_MACHINE), which no client or bot has, with no players, so the distributed netcode sends it every
player every tick and the objects and actors as a client whose players are never alive is sent them. Its datagrams are
written here instead of sent; so are the distributed messages sent every client reliably, those sent it alone, and the
game's own messages to every machine. Nothing goes on the wire for it: the clients are sent what they were.

The records go through one zlib stream (its fastest level) after the header, synced every few seconds so that a file
a crash cut short plays to its last sync. A thread of its own writes the file: the game thread only deflates, and hands
the bytes over at a sync. The folder keeps at most network.replays_kept of the recorder's own files and stays under
network.replays_budget_mb (the oldest are removed as a recording starts, other files never); a recording that grows
past the budget, or finds the disk under its reserve, stops there. A game shorter than 15 seconds is removed as it
ends.
*/

#include "cseries.h"
#include "cseries/errors.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "memory/data.h"
#include "game/players.h"
#include "networking/network_game_globals.h"
#include "networking/network_game_manager.h"
#include "networking/network_server_manager.h"
#include "networking/network_server_manager_internal.h"
#include "network_distributed.h"
#include "network_replay.h"
#include "ae_weapon_cache.h"
#include "halo_port_limits.h"
#include "../src/posix.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <xtl.h>

#include "zlib.h"

/* (stdio.h's fopen and remove take Xbox paths under the data root, as the game's code uses them: a recording's path
is a host path in the save root, so these are the C library's) */
#undef fopen
#undef remove

int config_boolean(char const *name);
long config_integer(char const *name);
const char *platform_save_root(void);
void platform_log(char const *format, ...);
void main_build_identity(long *number, char *flavor, size_t flavor_size, char *repository, size_t repository_size,
	char *version, size_t version_size);
boolean mcc_cache_tags_loaded(void);

/* a recording shorter than this (ticks) is removed as it ends */
#define REPLAY_MINIMUM_TICKS (15 * TICKS_PER_SECOND)
/* the file written out this often (ticks): what a crash loses */
#define REPLAY_FLUSH_TICKS (5 * TICKS_PER_SECOND)
#define REPLAY_BUFFER_SIZE 0x10000
/* what the stream has made, kept until the next sync hands it to the writer; past the most, the recording stops
rather than take the memory */
#define REPLAY_PENDING_MINIMUM 0x40000
#define REPLAY_PENDING_MAXIMUM 0x4000000
#define REPLAY_PATH_SIZE 512
/* the folder's files looked at for the caps */
#define REPLAY_FOLDER_MOST 1024

static struct
{
	FILE *file;
	/* the game's first tick has been and network.record_replays was read then: a game is recorded whole or not at
	all, so the setting turned on in a game applies from the next */
	boolean considered;
	long start_time;
	long last_flush_time;
	char path[REPLAY_PATH_SIZE];
	char name[AE_REPLAY_NAME_SIZE];
	/* the folder's budget, and the stream's bytes so far (what the file will be, past the header) */
	unsigned long long budget_bytes;
	unsigned long long stream_bytes;
	/* what was written, for the log at its end */
	unsigned long records[NUMBER_OF_NETWORK_REPLAY_RECORDS];
	unsigned long bytes[NUMBER_OF_NETWORK_REPLAY_RECORDS];
	unsigned long total_bytes;
	char buffer[REPLAY_BUFFER_SIZE];
	z_stream stream;
	byte *pending;
	unsigned long pending_size;
	unsigned long pending_capacity;
	/* the game thread's time in the recorder: this tick's so far (and which tick), and every tick's */
	long cost_tick;
	double cost_tick_seconds;
	struct ae_replay_cost cost;
} replay;

/* The writer: the game thread deflates into replay.pending and hands that over at a sync; the writer writes it while
the game goes on, then looks at the disk's room. Without the thread the hand-over writes it there and then. */
static struct
{
	/* work: something to write; idle: the writer has nothing (manual reset) */
	HANDLE thread, work, idle;
	FILE *file;
	byte *data;
	unsigned long size;
	unsigned long capacity;
	/* the game's length so far, for the header */
	unsigned long duration;
	char folder[REPLAY_PATH_SIZE];
	/* a write failed, or the disk is under its reserve: the recording stops at the next hand-over */
	boolean failed;
	boolean disk_low;
} replay_writer = { 0 };

static double replay_now(
	void)
{
	LARGE_INTEGER count, frequency;

	QueryPerformanceCounter(&count);
	QueryPerformanceFrequency(&frequency);
	return frequency.QuadPart ? (double)count.QuadPart / (double)frequency.QuadPart : 0.0;
}

/* the time spent since `began` counted in this tick's cost */
static void replay_cost_note(
	double began)
{
	long now_tick = game_time_get();

	if (now_tick != replay.cost_tick)
	{
		if (replay.cost_tick != NONE)
			ae_replay_cost_add(&replay.cost, (unsigned long)(replay.cost_tick_seconds * 1000000.0));
		replay.cost_tick = now_tick;
		replay.cost_tick_seconds = 0.0;
	}
	replay.cost_tick_seconds += replay_now() - began;
}

/* the free bytes where the folder is (0 when it cannot be read) */
static unsigned long long replay_disk_free(
	char const *folder)
{
	posix_ulong free_low, free_high, total_low, total_high;

	if (posix_disk_space(folder, &free_low, &free_high, &total_low, &total_high) != 0)
		return 0;
	return (unsigned long long)free_high << 32 | free_low;
}

static void replay_put_long(
	byte *destination,
	unsigned long value)
{
	destination[0] = (byte)value;
	destination[1] = (byte)(value >> 8);
	destination[2] = (byte)(value >> 16);
	destination[3] = (byte)(value >> 24);
}

static void replay_writer_write(
	void)
{
	byte duration[4];
	boolean written = !replay_writer.size ||
		fwrite(replay_writer.data, replay_writer.size, 1, replay_writer.file) == 1;

	/* (its length in the header, for a file a crash cuts short) */
	replay_put_long(duration, replay_writer.duration);
	if (written && fseek(replay_writer.file, AE_REPLAY_LENGTH_OFFSET, SEEK_SET) == 0)
		fwrite(duration, sizeof(duration), 1, replay_writer.file);
	if (written)
		written = fseek(replay_writer.file, 0, SEEK_END) == 0 && fflush(replay_writer.file) == 0;
	replay_writer.size = 0;
	if (!written)
		replay_writer.failed = TRUE;
	if (!ae_replay_disk_allows(replay_disk_free(replay_writer.folder)))
		replay_writer.disk_low = TRUE;
}

static DWORD WINAPI replay_writer_main(
	LPVOID unused)
{
	(void)unused;
	for (;;)
	{
		WaitForSingleObject(replay_writer.work, INFINITE);
		replay_writer_write();
		SetEvent(replay_writer.idle);
	}
	return 0;
}

/* the writer started; FALSE if it cannot be, and the hand-over writes */
static boolean replay_writer_ready(
	void)
{
	if (replay_writer.thread)
		return TRUE;
	replay_writer.work = CreateEventA(NULL, FALSE, FALSE, NULL);
	replay_writer.idle = CreateEventA(NULL, TRUE, TRUE, NULL);
	if (replay_writer.work && replay_writer.idle)
		replay_writer.thread = CreateThread(NULL, 0, replay_writer_main, NULL, 0, NULL);
	if (!replay_writer.thread)
	{
		if (replay_writer.work)
			CloseHandle(replay_writer.work);
		if (replay_writer.idle)
			CloseHandle(replay_writer.idle);
		replay_writer.work = replay_writer.idle = NULL;
		return FALSE;
	}
	return TRUE;
}

/* until the writer has written what it was given: then the file is the game thread's again */
static void replay_writer_wait(
	void)
{
	if (replay_writer.thread)
		WaitForSingleObject(replay_writer.idle, INFINITE);
}

/* the writer still writing what it was given last */
static boolean replay_writer_busy(
	void)
{
	return replay_writer.thread && WaitForSingleObject(replay_writer.idle, 0) == WAIT_TIMEOUT;
}

/* what the stream has made, to the writer (the buffers swapped, nothing copied), with the game's length; FALSE if an
earlier write failed or the disk is under its reserve */
static boolean replay_hand_off(
	void)
{
	byte *data;
	unsigned long capacity;

	replay_writer_wait();
	if (replay_writer.failed || replay_writer.disk_low)
		return FALSE;
	data = replay_writer.data;
	capacity = replay_writer.capacity;
	replay_writer.data = replay.pending;
	replay_writer.capacity = replay.pending_capacity;
	replay_writer.size = replay.pending_size;
	replay_writer.file = replay.file;
	replay_writer.duration = (unsigned long)(game_time_get() - replay.start_time);
	replay.pending = data;
	replay.pending_capacity = capacity;
	replay.pending_size = 0;
	if (replay_writer_ready())
	{
		ResetEvent(replay_writer.idle);
		SetEvent(replay_writer.work);
	}
	else
		replay_writer_write();
	return TRUE;
}

/* the recording given up: the file closed once the writer is done with it, and kept (it plays to its last sync) */
static void replay_abandon(
	char const *why)
{
	platform_log("replay: %s; the recording of %s stops", why, replay.path);
	replay_writer_wait();
	replay_writer.failed = FALSE;
	replay_writer.disk_low = FALSE;
	deflateEnd(&replay.stream);
	fclose(replay.file);
	replay.file = NULL;
	replay.pending_size = 0;
}

/* room for at least another REPLAY_BUFFER_SIZE of the stream's bytes */
static boolean replay_pending_grow(
	void)
{
	unsigned long capacity = replay.pending_capacity ? replay.pending_capacity * 2 : REPLAY_PENDING_MINIMUM;
	byte *pending;

	if (replay.pending_capacity - replay.pending_size >= REPLAY_BUFFER_SIZE)
		return TRUE;
	if (capacity > REPLAY_PENDING_MAXIMUM || !(pending = realloc(replay.pending, capacity)))
		return FALSE;
	replay.pending = pending;
	replay.pending_capacity = capacity;
	return TRUE;
}

/* through the stream into replay.pending: Z_NO_FLUSH, Z_SYNC_FLUSH (all of it made, readable once written) or
Z_FINISH; FALSE if the stream failed or the bytes have no room */
static boolean replay_deflate(
	void const *data,
	unsigned long size,
	int flush)
{
	replay.stream.next_in = (Bytef *)data;
	replay.stream.avail_in = (uInt)size;
	for (;;)
	{
		int result;
		unsigned long room;
		unsigned long made;

		if (!replay_pending_grow())
			return FALSE;
		room = replay.pending_capacity - replay.pending_size;
		replay.stream.next_out = replay.pending + replay.pending_size;
		replay.stream.avail_out = (uInt)room;
		result = deflate(&replay.stream, flush);
		made = room - replay.stream.avail_out;
		replay.pending_size += made;
		replay.stream_bytes += made;
		if (result != Z_OK && result != Z_STREAM_END && result != Z_BUF_ERROR)
			return FALSE;
		/* (done: everything taken, and for a flush all of it made) */
		if (result == Z_STREAM_END || (!replay.stream.avail_in && replay.stream.avail_out))
			return TRUE;
	}
}

void network_replay_folder(
	char *path,
	long size)
{
	char const *root = platform_save_root();

	snprintf(path, (size_t)size, "%s/replays", root ? root : ".");
}

/* the folder's recordings over the caps removed, the oldest of the recorder's own first, with room for one of
incoming_bytes */
static void replay_prune(
	long kept,
	unsigned long long budget_bytes,
	unsigned long long incoming_bytes)
{
	static char names[REPLAY_FOLDER_MOST][AE_REPLAY_NAME_SIZE];
	static struct ae_replay_file files[REPLAY_FOLDER_MOST];
	static int picked[REPLAY_FOLDER_MOST];
	char folder[REPLAY_PATH_SIZE];
	char name[256];
	void *directory;
	int count = 0, picks, index, removed = 0;

	network_replay_folder(folder, sizeof(folder));
	directory = posix_directory_open(folder);
	while (directory && count < REPLAY_FOLDER_MOST && posix_directory_next(directory, name, sizeof(name)))
	{
		char path[REPLAY_PATH_SIZE + 256];
		struct posix_file_information information;

		if (!ae_replay_name_is_own(name))
			continue;
		snprintf(path, sizeof(path), "%s/%s", folder, name);
		if (posix_stat(path, &information) != 0 || (information.flags & _posix_file_is_directory))
			continue;
		snprintf(names[count], AE_REPLAY_NAME_SIZE, "%s", name);
		files[count].name = names[count];
		files[count].bytes = (unsigned long long)information.size_high << 32 | information.size_low;
		count++;
	}
	if (directory)
		posix_directory_close(directory);
	picks = ae_replay_pick_removals(files, count, kept, budget_bytes, incoming_bytes, picked, REPLAY_FOLDER_MOST);
	for (index = 0; index < picks; index++)
	{
		char path[REPLAY_PATH_SIZE + 256];

		snprintf(path, sizeof(path), "%s/%s", folder, files[picked[index]].name);
		if (remove(path) == 0)
			removed++;
	}
	if (picks)
		platform_log("replay: %d old recordings removed (network.replays_kept %ld, network.replays_budget_mb %lu)%s",
			removed, kept, (unsigned long)(budget_bytes / (1024ULL * 1024ULL)), removed < picks ? ", some could not be" : "");
}

/* the game's text (UTF-16, as the game has names) in UTF-8, at most size bytes with its end */
static void replay_utf8(
	char *destination,
	long size,
	wchar_t const *source,
	long length)
{
	long used = 0;
	long index;

	for (index = 0; index < length && source[index]; index++)
	{
		unsigned long character = (unsigned short)source[index];
		char bytes[3];
		long count;

		if (character < 0x80)
		{
			bytes[0] = (char)character;
			count = 1;
		}
		else if (character < 0x800)
		{
			bytes[0] = (char)(0xC0 | (character >> 6));
			bytes[1] = (char)(0x80 | (character & 0x3F));
			count = 2;
		}
		else
		{
			bytes[0] = (char)(0xE0 | (character >> 12));
			bytes[1] = (char)(0x80 | ((character >> 6) & 0x3F));
			bytes[2] = (char)(0x80 | (character & 0x3F));
			count = 3;
		}
		if (used + count >= size)
			break;
		csmemcpy(destination + used, bytes, (size_t)count);
		used += count;
	}
	destination[used] = 0;
}

/* how the level was made: byte 0 says this layout (2), byte 1 whether the map's tags are MCC's */
static void replay_level_setup_build(
	byte *record)
{
	csmemset(record, 0, AE_REPLAY_SETUP_SIZE);
	record[0] = 2;
	record[1] = mcc_cache_tags_loaded() ? 1 : 0;
	/* (the weapon cache the game plays with: a playback on another refuses) */
	ae_replay_setup_cache_write(record, ae_weapon_cache_serial(), ae_weapon_cache_id());
}

static boolean replay_start(
	void)
{
	struct network_game_server *server = global_network_game_server_get();
	struct network_game *game = server ? network_game_server_get_game(server) : NULL;
	struct ae_replay_header fields;
	byte header[AE_REPLAY_HEADER_SIZE];
	char map[0x80] = "";
	char const *base_name;
	char flavor[32], repository[64];
	char when[32] = "";
	char folder[REPLAY_PATH_SIZE];
	long number = 0;
	long kept = ae_replay_kept(config_integer("network.replays_kept"));
	time_t now = time(NULL);
	struct tm *local = localtime(&now);
	short index;

	if (!game)
		return FALSE;
	replay.budget_bytes = (unsigned long long)ae_replay_budget_mb(config_integer("network.replays_budget_mb")) *
		1024ULL * 1024ULL;
	network_replay_folder(folder, sizeof(folder));
	posix_make_directory(folder);
	if (!ae_replay_disk_allows(replay_disk_free(folder)))
	{
		platform_log("replay: under %d MB free where %s is: this game is not recorded", AE_REPLAY_DISK_RESERVE_MB, folder);
		return FALSE;
	}
	/* (room for this one: as many as network.replays_kept with it, and a few minutes' bytes under the budget) */
	replay_prune(kept, replay.budget_bytes, 16ULL * 1024ULL * 1024ULL);
	csmemset(&fields, 0, sizeof(fields));
	for (index = 0; index < (short)sizeof(map) - 1 && game->map.name[index]; index++)
		map[index] = game->map.name[index];
	map[index] = 0;
	for (index = 0; index < AE_REPLAY_VARIANT_SIZE - 1 && game->variant.human_readable_game_description[index]; index++)
		fields.variant[index] = player_name_character_ascii(game->variant.human_readable_game_description[index]);
	fields.variant[index] = 0;
	/* (a game type with no name of its own: its engine's number) */
	if (!fields.variant[0])
		snprintf(fields.variant, sizeof(fields.variant), "engine %ld", (long)game->variant.game_engine_index);
	main_build_identity(&number, flavor, sizeof(flavor), repository, sizeof(repository), fields.version,
		sizeof(fields.version));
	/* (the map's name past its folders, as levels\test\bloodgulch\bloodgulch) */
	base_name = strrchr(map, '\\');
	base_name = base_name ? base_name + 1 : map;
	if (!local || !strftime(when, sizeof(when), "%Y%m%d-%H%M%S", local) ||
		!ae_replay_name_make(replay.name, sizeof(replay.name), when, base_name))
	{
		platform_log("replay: no name for a recording of %s", map);
		return FALSE;
	}
	snprintf(replay.path, sizeof(replay.path), "%s/%s", folder, replay.name);
	replay.file = fopen(replay.path, "wb");
	if (!replay.file)
	{
		platform_log("replay: could not open %s to record the game", replay.path);
		return FALSE;
	}
	setvbuf(replay.file, replay.buffer, _IOFBF, sizeof(replay.buffer));
	snprintf(replay_writer.folder, sizeof(replay_writer.folder), "%s", folder);
	fields.format = AE_REPLAY_FORMAT;
	fields.network_version = HALO_PORT_NETWORK_VERSION;
	fields.build = (unsigned long)number;
	fields.ticks_per_second = TICKS_PER_SECOND;
	fields.start_tick = (unsigned long)game_time_get();
	fields.time = (unsigned long long)now;
	snprintf(fields.map, sizeof(fields.map), "%s", map);
	replay_utf8(fields.server, sizeof(fields.server), game->name, NUMBEROF(game->name));
	{
		/* (this machine's first player: a dedicated server has none) */
		long player_index = local_player_get_player_index(local_player_get_next(NONE));
		struct player_datum *player = player_index != NONE ? player_try_and_get(player_index) : NULL;

		if (player)
			replay_utf8(fields.player, sizeof(fields.player), player->name, NUMBEROF(player->name));
	}
	ae_replay_header_write(header, &fields);
	csmemset(&replay.stream, 0, sizeof(replay.stream));
	/* (the header on the disk at once: the folder's list reads it while the game is recorded) */
	if (fwrite(header, sizeof(header), 1, replay.file) != 1 || fflush(replay.file) != 0 ||
		deflateInit(&replay.stream, 1) != Z_OK)
	{
		platform_log("replay: could not write to %s", replay.path);
		fclose(replay.file);
		replay.file = NULL;
		return FALSE;
	}
	replay.start_time = game_time_get();
	replay.last_flush_time = replay.start_time;
	replay.stream_bytes = 0;
	replay.cost_tick = NONE;
	replay.cost_tick_seconds = 0.0;
	csmemset(&replay.cost, 0, sizeof(replay.cost));
	csmemset(replay.records, 0, sizeof(replay.records));
	csmemset(replay.bytes, 0, sizeof(replay.bytes));
	replay.total_bytes = AE_REPLAY_HEADER_SIZE;
	/* (the game itself first, which a playback loads; then how the level was made) */
	network_replay_record(_network_replay_game, game, sizeof(*game));
	{
		byte setup[AE_REPLAY_SETUP_SIZE];

		replay_level_setup_build(setup);
		network_replay_record(_network_replay_level_setup, setup, sizeof(setup));
	}
	if (!replay.file)
		return FALSE;
	platform_log("replay: recording %s (%s) to %s", map, fields.variant, replay.path);
	return TRUE;
}

boolean network_replay_recording(
	void)
{
	return replay.file != NULL;
}

void network_replay_host_tick(
	void)
{
	if (replay.file)
	{
		/* (not while the writer is still at the last sync: the hand-over would wait out a slow disk on the game
		thread; the stream goes on into replay.pending, and is handed over a tick after the writer is done) */
		if (game_time_get() - replay.last_flush_time >= REPLAY_FLUSH_TICKS && !replay_writer_busy())
		{
			double began = replay_now();

			replay.last_flush_time = game_time_get();
			if (replay_writer.disk_low)
				replay_abandon("the disk is under its reserve");
			else if (AE_REPLAY_HEADER_SIZE + replay.stream_bytes > replay.budget_bytes)
				replay_abandon("it is larger than network.replays_budget_mb");
			else if (!replay_deflate(NULL, 0, Z_SYNC_FLUSH) || !replay_hand_off())
				replay_abandon("it could not be written");
			if (replay.file)
				replay_cost_note(began);
		}
		return;
	}
	if (replay.considered)
		return;
	replay.considered = TRUE;
	/* (multiplayer only: a campaign's co-op game is not recorded) */
	if (config_boolean("network.record_replays") && game_engine_running())
		replay_start();
}

void network_replay_record(
	byte kind,
	void const *data,
	long size)
{
	byte header[AE_REPLAY_RECORD_HEADER_SIZE];
	double began;

	if (!replay.file || size < 0 || !ae_replay_record_header_write(header, kind, (unsigned long)size,
		(unsigned long)game_time_get()))
	{
		return;
	}
	began = replay_now();
	if (!replay_deflate(header, sizeof(header), Z_NO_FLUSH) ||
		(size && !replay_deflate(data, (unsigned long)size, Z_NO_FLUSH)))
	{
		replay_abandon("its stream failed");
		return;
	}
	replay.records[kind]++;
	replay.bytes[kind] += (unsigned long)size;
	replay.total_bytes += AE_REPLAY_RECORD_HEADER_SIZE + (unsigned long)size;
	replay_cost_note(began);
}

void network_replay_record_game_message(
	void const *message,
	long size)
{
	network_replay_record(_network_replay_game_message, message, size);
}

void network_replay_stop(
	void)
{
	if (replay.file)
	{
		long ticks = game_time_get() - replay.start_time;
		boolean written = replay_deflate(NULL, 0, Z_FINISH) && replay_hand_off();
		long file_size;

		deflateEnd(&replay.stream);
		/* (the last of it written before the file is closed) */
		replay_writer_wait();
		written = written && !replay_writer.failed;
		replay_writer.failed = FALSE;
		replay_writer.disk_low = FALSE;
		replay.pending_size = 0;
		file_size = ftell(replay.file);
		written = fclose(replay.file) == 0 && written;
		replay.file = NULL;
		if (replay.cost_tick != NONE)
			ae_replay_cost_add(&replay.cost, (unsigned long)(replay.cost_tick_seconds * 1000000.0));
		replay.cost_tick = NONE;
		/* (a game of a few seconds, left at once, is no replay) */
		if (ticks < REPLAY_MINIMUM_TICKS)
		{
			platform_log("replay: %s was %ld ticks long, under %d seconds: %s", replay.path, ticks,
				REPLAY_MINIMUM_TICKS / TICKS_PER_SECOND, remove(replay.path) == 0 ? "removed" : "NOT removed");
			replay.considered = FALSE;
			replay.name[0] = 0;
			return;
		}
		platform_log("replay: %s %s: %ld seconds, %ld KB (%lu KB before compression: %lu datagrams %lu KB, "
			"%lu reliable %lu KB, %lu to the recorder %lu KB, %lu game messages %lu KB)",
			written ? "recorded" : "could not finish", replay.path, ticks / TICKS_PER_SECOND, file_size / 1024,
			replay.total_bytes / 1024,
			replay.records[_network_replay_datagram], replay.bytes[_network_replay_datagram] / 1024,
			replay.records[_network_replay_reliable], replay.bytes[_network_replay_reliable] / 1024,
			replay.records[_network_replay_reliable_to_machine], replay.bytes[_network_replay_reliable_to_machine] / 1024,
			replay.records[_network_replay_game_message], replay.bytes[_network_replay_game_message] / 1024);
		platform_log("replay: game thread cost %lu us a tick on average, %lu us at most, over %lu ticks",
			ae_replay_cost_mean(&replay.cost), replay.cost.most_us, replay.cost.ticks);
		/* (none of the ticks' datagrams: nothing went to the recorder's machine) */
		if (!replay.records[_network_replay_datagram])
			platform_log("replay: no datagrams were recorded: the replay will play black");
		replay.name[0] = 0;
	}
	replay.considered = FALSE;
}

long network_replay_list(
	char (*names)[AE_REPLAY_NAME_SIZE],
	long maximum)
{
	char folder[REPLAY_PATH_SIZE];
	char name[256];
	void *directory;
	long count = 0;

	network_replay_folder(folder, sizeof(folder));
	directory = posix_directory_open(folder);
	while (directory && posix_directory_next(directory, name, sizeof(name)))
	{
		long place;

		if (!ae_replay_name_is_safe(name))
			continue;
		/* (in order, newest first: an insertion, the folder's few) */
		for (place = count; place > 0 && strcmp(names[place - 1], name) < 0; place--)
		{
			if (place < maximum)
				strcpy(names[place], names[place - 1]);
		}
		if (place < maximum)
		{
			strcpy(names[place], name);
			if (count < maximum)
				count++;
		}
	}
	if (directory)
		posix_directory_close(directory);
	return count;
}

int network_replay_describe(
	char const *name,
	struct ae_replay_header *header,
	unsigned long long *bytes,
	char *why,
	long why_size)
{
	char path[REPLAY_PATH_SIZE + AE_REPLAY_NAME_SIZE];
	unsigned char head[AE_REPLAY_HEADER_SIZE];
	size_t read = 0;
	FILE *file;
	struct posix_file_information information;

	if (bytes)
		*bytes = 0;
	if (!ae_replay_name_is_safe(name))
	{
		if (why && why_size > 0)
			snprintf(why, (size_t)why_size, "no such recording");
		return AE_REPLAY_HEADER_NOT_REPLAY;
	}
	network_replay_folder(path, REPLAY_PATH_SIZE);
	snprintf(path + strlen(path), sizeof(path) - strlen(path), "/%s", name);
	if (bytes && posix_stat(path, &information) == 0)
		*bytes = (unsigned long long)information.size_high << 32 | information.size_low;
	file = fopen(path, "rb");
	if (file)
	{
		read = fread(head, 1, sizeof(head), file);
		fclose(file);
	}
	return ae_replay_header_read(file ? head : NULL, read, HALO_PORT_NETWORK_VERSION, header, why,
		why_size > 0 ? (size_t)why_size : 0);
}

void network_replay_status(
	char *name,
	long size,
	struct ae_replay_cost *cost)
{
	if (name && size > 0)
		snprintf(name, (size_t)size, "%s", replay.file ? replay.name : "");
	if (cost)
		*cost = replay.cost;
}
