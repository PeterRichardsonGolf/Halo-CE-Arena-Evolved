/*
NETWORK_REPLAY_PLAYBACK.C

A replay's playback: the game a host recorded (network_replay.c), watched through any player's eyes or from a free
camera.

It plays as a client of the distributed netcode does, with no player of its own and no host. The whole file is read
and its records inflated into memory first, and every record is checked (ae_replay_stream_check) before any is
played: a file that fails is refused with one line, and none of its bytes reaches a handler. Then the global client is
made one joined to the game the replay's first record holds (network_game_client_port_replay_begin), which loads it as
a client loads the game it joins; its clock starts at the replay's first tick, and before each tick the records of the
ticks reached go to what a client hands its host's messages to: the distributed netcode's
(network_distributed_handle_message, the datagrams marked unreliable and the rest reliable, as they came) and the
game's own (network_game_client_handle_message). The distributed netcode's messages pass through a denylist: every
message a client takes from its host is handed over (to handlers written for a hostile host) but those of map and
package sharing, voice, the map vote, MCC admission and loading, the server's admin pages, the map check and aim assist
(network_distributed_replay_allows); of the game's own, an allowlist: only a player joining or leaving, the game's
update and its end (playback_game_message_allowed). The client sends
nothing (network_game_client_write).

The view (network_replay_camera, window 0's: main.c) is the watched player's eyes (where their unit's camera is,
looking where it looks) or a free camera placed behind and above them. Speed (game_time_set_speed), pause, a seek
(ahead: played to at speed; back: the replay begun again and played to it) and the end, back to the menus, are the
DEV MODE replay command's (ae_dev_cmd.c). Medals and service records are not counted while it plays
(network_replay_playing).
*/

#include "cseries.h"
#include "cseries/errors.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "main/main.h"
#include "camera/observer.h"
#include "networking/network_game_globals.h"
#include "networking/network_game_manager.h"
#include "networking/network_client_manager.h"
#include "networking/network_client_message_handler.h"
#include "bungie_net/common/message_header.h"
#include "bungie_net/network/transport.h"
#include "networking/network_messages.h"
#include "networking/network_connection.h"
#include "cache/cache_files.h"
#include "objects/objects.h"
#include "physics/collisions.h"
#include "scenario/scenario.h"
#include "scenario/scenario_definitions.h"
#include "units/units.h"
#include "units/unit_definitions.h"
#include "network_distributed.h"
#include "network_replay.h"
#include "ae_weapon_cache.h"
#include "halo_port_limits.h"
#include "../src/posix.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "zlib.h"

#undef fopen

void platform_log(char const *format, ...);
void network_distributed_handle_message(long machine_index, word const *message, word size);
const char *platform_data_root(void);
boolean network_distributed_replay_allows(word const *message, word size);
boolean network_game_client_port_game_check(struct network_game *game);
short ui_widget_port_multiplayer_maps(char const *const **names, short *last_used);
struct game_input_state *input_abstraction_get_input_state(short local_player_index);
int halo_linux_mouse_look(short gamepad_index, float *yaw, float *pitch);
int ae_seats_keyboard_controller(void);
float main_get_seconds_elapsed(void);
void first_person_weapon_port_state(short local_player_index, long *unit_index, long *weapon_index, boolean *visible);
short ui_widget_port_gametypes(long *indices, short maximum, short *last_used);
void network_connection_port_set_last_read_unreliable(boolean unreliable);
boolean network_game_client_port_replay_begin(struct network_game const *game);
boolean network_game_client_port_in_postgame(struct network_game_client *client);
void network_game_abort(void);
boolean create_global_network_game_client(void);
void dispose_global_network_game_client(void);
void dispose_global_network_game_server(void);

/* the level setup record's bytes (network_replay.c) */
#define PLAYBACK_SETUP_SIZE AE_REPLAY_SETUP_SIZE
/* the most of a replay's records held in memory: the format's most, half that in a 32-bit build */
#define PLAYBACK_INFLATED_MOST (sizeof(void *) > 4 ? (unsigned long)AE_REPLAY_INFLATED_MOST : \
	(unsigned long)AE_REPLAY_INFLATED_MOST / 2)
/* the most records handed over in one feed: the rest go in the next (a file whose records all carry one tick plays
on, slowly, instead of stopping the game while they all go) */
#define PLAYBACK_RECORDS_A_FEED 4096
/* how long the playback goes on after its last record, before it ends (ticks) */
#define PLAYBACK_END_TICKS (2 * TICKS_PER_SECOND)
/* a seek's speed: the game played to the second asked for at 4 times */
#define PLAYBACK_SEEK_SPEED 4.0f
/* the free camera: behind and above the watched player when placed (world units) */
#define PLAYBACK_FREE_BACK 3.0f
#define PLAYBACK_FREE_UP 1.5f
/* how far past the camera's place the way to it must be clear (world units) */
#define PLAYBACK_FREE_MARGIN 0.3f
#define PLAYBACK_FIELD_OF_VIEW 1.2217305f
#define PLAYBACK_PATH_SIZE 1024

enum
{
	_playback_view_eyes,
	_playback_view_free
};

static struct
{
	/* the records, inflated, and what the check found in them */
	byte *records;
	unsigned long size;
	struct ae_replay_stream stream;
	struct ae_replay_header header;
	char name[AE_REPLAY_NAME_SIZE];
	/* where the next record is */
	unsigned long next;
	boolean starting;
	boolean playing;
	boolean setting_up;
	boolean synchronized;
	boolean leaving;
	boolean paused;
	long speed_percent;
	/* a seek's tick (NONE: none), played to at the seek's speed */
	long seek_tick;
	/* the tick the last record was played at, and the end's */
	long last_fed_tick;
	/* the view: whose eyes (a player's absolute index, NONE: the first with a unit), or the free camera's place */
	short view;
	short watched;
	real_point3d free_position;
	real_vector3d free_forward;
	struct observer_result camera;
	boolean camera_valid;
	/* the watched unit's eyes: the unit, the last tick its aim was read on and the aim read then; the turn drawn (from
	the aim drawn when the last new aim came to it, over as many ticks as came between the two before: the recording
	holds a player's aim as the host sent it, a new one every few ticks), and the aim the last frame drew */
	long eyes_unit;
	long eyes_tick;
	real_vector3d eyes_aim_seen;
	real_vector3d eyes_aim_from;
	real_vector3d eyes_aim_to;
	long eyes_aim_tick;
	long eyes_aim_span;
	real_vector3d eyes_aim_drawn;
	real_vector3d eyes_aim_raw;
	/* for the status line: frames drawn through moving eyes and those of them the view did not move in; frames drawn
	while the aim turned and those of them it did not turn in; ticks the aim was read on and those it was new on */
	unsigned long eyes_moving_frames;
	unsigned long eyes_still_frames;
	unsigned long eyes_turning_frames;
	unsigned long eyes_unturned_frames;
	/* ... and those the aim as recorded (the view taken as the last tick left it) did not turn in */
	unsigned long eyes_raw_unturned_frames;
	unsigned long eyes_ticks;
	unsigned long eyes_turns;
	/* the message handed over, aligned for the handlers */
	word message[(AE_REPLAY_RECORD_MOST + 1) / sizeof(word) + 1];
} playback = { NULL };

/* a replay stopped: for the menus to open the Theater again */
static boolean playback_theater_wanted;

/* the menus' multiplayer map name while a replay plays (the replay's map's is set, as a client joining a game has it
set, for the scoreboard's header and the postgame's name): put back when it stops */
static struct
{
	boolean saved;
	char map_name[256];
} playback_menus;

static void playback_menus_restore(
	void)
{
	if (playback_menus.saved)
		main_set_multiplayer_map_name(playback_menus.map_name);
	playback_menus.saved = FALSE;
}

/* the main menu's LAST GAME card's map and game type (CREATE GAME's last picks), as a replay found them: a replay
must leave them as they were, which its end logs */
static struct
{
	boolean noted;
	short map;
	short gametype;
} playback_last_game = { FALSE, NONE, NONE };

static void playback_last_game_read(
	short *map,
	short *gametype)
{
	char const *const *names = NULL;
	long gametypes[64];

	*map = NONE;
	*gametype = NONE;
	ui_widget_port_multiplayer_maps(&names, map);
	ui_widget_port_gametypes(gametypes, (short)NUMBEROF(gametypes), gametype);
}

static void playback_free(
	void)
{
	if (playback.records)
		free(playback.records);
	csmemset(&playback, 0, sizeof(playback));
	playback.seek_tick = NONE;
	playback.watched = NONE;
	playback.eyes_unit = NONE;
}

/* the file's bytes after its header, inflated into playback.records; FALSE and why if not */
static boolean playback_inflate(
	FILE *file,
	char *why,
	long why_size)
{
	z_stream stream;
	byte input[0x10000];
	unsigned long capacity = 0x100000;
	int result = Z_OK;

	csmemset(&stream, 0, sizeof(stream));
	if (!(playback.records = malloc(capacity)))
	{
		snprintf(why, (size_t)why_size, "there is not memory enough to play it");
		return FALSE;
	}
	if (inflateInit(&stream) != Z_OK)
	{
		snprintf(why, (size_t)why_size, "there is not memory enough to play it");
		return FALSE;
	}
	playback.size = 0;
	while (result != Z_STREAM_END)
	{
		size_t read = fread(input, 1, sizeof(input), file);

		if (!read)
			break;
		stream.next_in = input;
		stream.avail_in = (uInt)read;
		while (stream.avail_in && result != Z_STREAM_END)
		{
			if (capacity - playback.size < 0x10000)
			{
				byte *grown;

				if (capacity >= PLAYBACK_INFLATED_MOST ||
					!(grown = realloc(playback.records, MIN(capacity * 2, PLAYBACK_INFLATED_MOST))))
				{
					inflateEnd(&stream);
					snprintf(why, (size_t)why_size, "it is too large to play");
					return FALSE;
				}
				playback.records = grown;
				capacity = MIN(capacity * 2, PLAYBACK_INFLATED_MOST);
			}
			stream.next_out = playback.records + playback.size;
			stream.avail_out = (uInt)(capacity - playback.size);
			result = inflate(&stream, Z_SYNC_FLUSH);
			playback.size = capacity - stream.avail_out;
			/* (a file cut short, or damaged past its last sync: played to what inflated) */
			if (result != Z_OK && result != Z_STREAM_END && result != Z_BUF_ERROR)
			{
				platform_log("replay: its stream ends damaged after %lu bytes: played to there", playback.size);
				result = Z_STREAM_END;
			}
		}
	}
	inflateEnd(&stream);
	return TRUE;
}

/* the recorded game record: the checks a host's settings record gets on a client (network_client_manager.c's, which
also ends its strings in their fields), and this file's own (ae_replay_rules: the counts, each machine's players, a
map name the maps folders may be searched for); FALSE and why if it fails one */
static boolean playback_game_check(
	struct network_game *game,
	char *why,
	long why_size)
{
	long machines[NUMBEROF(game->players)], controllers[NUMBEROF(game->players)];
	short index;

	if (!ae_replay_map_name_safe(game->map.name, sizeof(game->map.name)))
	{
		snprintf(why, (size_t)why_size, "its game's map name is not one the game loads");
		return FALSE;
	}
	if (!ae_replay_game_counts_check(game->machine_count, game->player_count, game->maximum_players,
		game->difficulty, HALO_PORT_MAXIMUM_NETWORK_MACHINES, HALO_PORT_MAXIMUM_NETWORK_PLAYERS, NUMBER_OF_GAME_DIFFICULTY_LEVELS,
		why, (size_t)why_size))
		return FALSE;
	for (index = 0; index < (short)NUMBEROF(game->players); index++)
	{
		machines[index] = game->players[index].machine_index;
		controllers[index] = game->players[index].controller_index;
	}
	if (!ae_replay_players_check(machines, controllers, (int)NUMBEROF(game->players), HALO_PORT_MAXIMUM_NETWORK_MACHINES,
		MAXIMUM_LOCAL_PLAYERS, why, (size_t)why_size))
		return FALSE;
	if (!network_game_client_port_game_check(game))
	{
		snprintf(why, (size_t)why_size, "its game record is damaged");
		return FALSE;
	}
	return TRUE;
}

/* the map the replay's game plays is on this machine; FALSE and why (with the map's name) if not */
static boolean playback_map_here(
	struct network_game const *game,
	byte const *setup,
	char *why,
	long why_size)
{
	char const *base = strrchr(game->map.name, '\\');
	char path[PLAYBACK_PATH_SIZE];
	char const *root = platform_data_root();
	struct posix_file_information information;
	static char const *const folders[] = { "maps", "custom_maps", "maps_dev", NULL };
	short index;

	base = base ? base + 1 : game->map.name;
	/* (never a path that leaves the maps folders: no '/', "..", ':' or device name, whoever calls this) */
	if (!ae_replay_map_name_safe(game->map.name, sizeof(game->map.name)))
	{
		snprintf(why, (size_t)why_size, "its game's map name is not one the game loads");
		return FALSE;
	}
	/* (a map of a pack, MCC's or the Workshop's: not played back yet. The level setup's byte 1 says MCC tags) */
	if (strchr(game->map.name, '@') || (setup && setup[1]))
	{
		snprintf(why, (size_t)why_size, "its map %s is a map pack's or MCC's, which the playback does not load", base);
		return FALSE;
	}
	/* (its weapons: the weapon cache it played with must be this machine's, or every pack weapon would be another) */
	if (setup && !ae_replay_setup_cache_matches(setup, ae_weapon_cache_serial(), ae_weapon_cache_id(), why,
		(size_t)why_size))
		return FALSE;
	for (index = 0; folders[index]; index++)
	{
		snprintf(path, sizeof(path), "%s/%s/%s.map", root ? root : ".", folders[index], base);
		if (!posix_stat(path, &information))
		{
			char build[0x20];

			/* (as a client joining checks its host's map: this machine's of the version recorded, of a build that
			plays with others) */
			if (!cache_files_map_present(game->map.name, (unsigned long)game->map.version))
			{
				snprintf(why, (size_t)why_size, "its map %s is another version than this machine's", base);
				return FALSE;
			}
			if (!cache_files_map_plays_multiplayer(game->map.name, build))
			{
				snprintf(why, (size_t)why_size, "its map %s is of a build that does not play with others", base);
				return FALSE;
			}
			return TRUE;
		}
	}
	snprintf(why, (size_t)why_size, "its map %s is not on this machine", base);
	return FALSE;
}

static boolean playback_play(
	char const *name,
	char *why,
	long why_size)
{
	char path[PLAYBACK_PATH_SIZE];
	unsigned char head[AE_REPLAY_HEADER_SIZE];
	struct posix_file_information information;
	struct network_game game;
	unsigned long long bytes;
	FILE *file;

	why[0] = 0;
	playback_free();
	if (!ae_replay_name_is_safe(name))
	{
		snprintf(why, (size_t)why_size, "no such recording");
		return FALSE;
	}
	network_replay_folder(path, sizeof(path));
	snprintf(path + strlen(path), sizeof(path) - strlen(path), "/%s", name);
	if (posix_stat(path, &information) != 0)
	{
		snprintf(why, (size_t)why_size, "no such recording");
		return FALSE;
	}
	bytes = (unsigned long long)information.size_high << 32 | information.size_low;
	if (bytes > AE_REPLAY_FILE_MOST)
	{
		snprintf(why, (size_t)why_size, "it is too large to play");
		return FALSE;
	}
	file = fopen(path, "rb");
	if (!file)
	{
		snprintf(why, (size_t)why_size, "it cannot be opened");
		return FALSE;
	}
	if (ae_replay_header_read(head, fread(head, 1, sizeof(head), file), HALO_PORT_NETWORK_VERSION, &playback.header,
		why, (size_t)why_size) != AE_REPLAY_HEADER_OK || !playback_inflate(file, why, why_size))
	{
		fclose(file);
		playback_free();
		return FALSE;
	}
	fclose(file);
	/* (every record checked before any is played) */
	if (!ae_replay_stream_check(playback.records, playback.size, playback.header.start_tick,
		sizeof(struct network_game), PLAYBACK_SETUP_SIZE, sizeof(message_header), DATAGRAM_MAXIMUM_SIZE,
		HALO_PORT_MAXIMUM_NETWORK_MESSAGE_SIZE, &playback.stream, why,
		(size_t)why_size))
	{
		playback_free();
		return FALSE;
	}
	csmemcpy(&game, playback.records + playback.stream.game_at, sizeof(game));
	if (!playback_game_check(&game, why, why_size) || !playback_map_here(&game, playback.stream.play_from > playback.stream.game_at + sizeof(game) ?
		playback.records + playback.stream.game_at + sizeof(game) + AE_REPLAY_RECORD_HEADER_SIZE : NULL, why, why_size))
	{
		playback_free();
		return FALSE;
	}
	playback.next = (unsigned long)playback.stream.play_from;
	snprintf(playback.name, sizeof(playback.name), "%s", name);
	playback.speed_percent = 100;
	/* (as a client joins a game: none of this machine's own network game is left) */
	playback.setting_up = TRUE;
	dispose_global_network_game_server();
	dispose_global_network_game_client();
	if (!create_global_network_game_client() || !network_game_client_port_replay_begin(&game))
	{
		snprintf(why, (size_t)why_size, "its game cannot be set up");
		dispose_global_network_game_client();
		playback.setting_up = FALSE;
		playback_free();
		return FALSE;
	}
	playback.setting_up = FALSE;
	playback.starting = TRUE;
	if (!playback_last_game.noted)
	{
		playback_last_game_read(&playback_last_game.map, &playback_last_game.gametype);
		playback_last_game.noted = TRUE;
	}
	if (!playback_menus.saved)
	{
		snprintf(playback_menus.map_name, sizeof(playback_menus.map_name), "%s", main_get_multiplayer_map_name());
		playback_menus.saved = TRUE;
	}
	main_set_multiplayer_map_name(game.map.name);
	{
		char const *last = strrchr(game.map.name, '\\');

		platform_log("replay: the scoreboard and the postgame name its map, %s (the menus' own put back when it stops)",
			last ? last + 1 : game.map.name);
	}
	platform_log("replay: playing %s (%s, %s), %lu records from tick %lu%s", name, game.map.name,
		playback.header.variant, playback.stream.records, playback.header.start_tick,
		playback.stream.cut ? ", cut short" : "");
	return TRUE;
}

boolean network_replay_play(
	char const *name,
	char *why,
	long why_size)
{
	boolean was_playing = network_replay_playing();

	if (playback_play(name, why, why_size))
		return TRUE;
	/* (a replay asked for over one playing, a seek back among them, that fails: the old one's records are gone, so its
	game is left too, back to the menus, not kept with the playback's hooks off) */
	if (was_playing)
	{
		platform_log("replay: %s refused (%s): the replay playing ends", name, why);
		/* (back to the Theater, as a replay that stops is) */
		playback_last_game.noted = FALSE;
		playback_theater_wanted = TRUE;
		playback_menus_restore();
		game_time_set_speed(1.0f);
		network_game_abort();
	}
	return FALSE;
}

boolean network_replay_playing(
	void)
{
	return playback.records != NULL && (playback.starting || playback.playing);
}

boolean network_replay_playback_starting(
	void)
{
	return playback.records != NULL && playback.starting;
}

void network_replay_playback_started(
	void)
{
	playback.starting = FALSE;
	playback.playing = TRUE;
	game_time_set_distributed((long)playback.header.start_tick);
	/* (a seek back begun it again: played to the seek's second at the seek's speed) */
	game_time_set_speed(playback.seek_tick != NONE ? PLAYBACK_SEEK_SPEED :
		playback.paused ? 0.0f : (real)playback.speed_percent / 100.0f);
	platform_log("replay: the game is loaded; playing from tick %lu", playback.header.start_tick);
}

/* whether a game message of this kind is one a playback hands over: a game in progress's (players joining and
leaving, the game's end), not the lobby's, the connection's or a new game's */
static boolean playback_game_message_allowed(
	word const *message,
	long size)
{
	byte type;

	if (size <= (long)sizeof(message_header) || GET_MESSAGE_SIZE(message[0]) != size ||
		GET_MESSAGE_TYPE(message[0]) != _message_type_packet)
	{
		return FALSE;
	}
	type = ((byte const *)message)[size - 1];
	return type == _message_server_add_player_ingame || type == _message_server_remove_player_ingame ||
		type == _message_server_game_over || type == _message_server_game_update;
}

/* the records of the ticks the clock has reached, to the client's handlers as it reads them from its host */
static void playback_feed(
	struct network_game_client *client)
{
	struct transport_address host;
	long fed = 0;

	csmemset(&host, 0, sizeof(host));
	while (playback.next < playback.stream.end && fed++ < PLAYBACK_RECORDS_A_FEED)
	{
		struct ae_replay_record record;
		byte const *bytes = playback.records + playback.next;

		/* (checked whole before playing: a record here is one) */
		if (ae_replay_record_take(bytes, playback.stream.end - playback.next, 0, 0xFFFFFFFFUL, &record) !=
			AE_REPLAY_RECORD_OK)
		{
			playback.next = (unsigned long)playback.stream.end;
			break;
		}
		if ((long)record.tick > game_time_get())
			break;
		/* (the game over: none of the records after it, the host's next pregame among them) */
		if (network_game_client_port_in_postgame(client))
			break;
		playback.next += AE_REPLAY_RECORD_HEADER_SIZE + record.size;
		playback.last_fed_tick = (long)record.tick;
		csmemcpy(playback.message, bytes + AE_REPLAY_RECORD_HEADER_SIZE, record.size);
		switch (record.kind)
		{
		case _network_replay_datagram:
		case _network_replay_reliable:
		case _network_replay_reliable_to_machine:
			network_connection_port_set_last_read_unreliable(record.kind == _network_replay_datagram);
			network_distributed_handle_message(NONE, playback.message, (word)record.size);
			break;
		case _network_replay_game_message:
			if (!playback_game_message_allowed(playback.message, (long)record.size))
				break;
			network_connection_port_set_last_read_unreliable(FALSE);
			network_game_client_handle_message(client, playback.message, (short)record.size, &host);
			break;
		}
	}
	network_connection_port_set_last_read_unreliable(FALSE);
}

void network_replay_playback_tick(
	void)
{
	struct network_game_client *client = global_network_game_client_get();

	if (playback.playing && client)
		playback_feed(client);
}

/* the player of an absolute index (a seek back makes the players again: their indices stay, their datums' salts
need not); NONE: none */
static long playback_player_at(
	short absolute)
{
	struct data_iterator iterator;

	data_iterator_new(&iterator, player_data);
	while (data_iterator_next(&iterator) != NULL)
	{
		if (DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index) == absolute)
			return iterator.datum_index;
	}
	return NONE;
}

/* the player watched: the one asked for while they have a unit (NONE while they wait to spawn or to respawn), else
the first with a living unit; NONE when none has one */
static long playback_watched(
	void)
{
	struct data_iterator iterator;
	struct player_datum *player;

	if (playback.watched != NONE)
	{
		long player_index = playback_player_at(playback.watched);

		player = player_index != NONE ? player_try_and_get(player_index) : NULL;
		return player && player->unit_index != NONE ? player_index : NONE;
	}
	data_iterator_new(&iterator, player_data);
	while ((player = data_iterator_next(&iterator)) != NULL)
	{
		if (player->unit_index != NONE)
			return iterator.datum_index;
	}
	return NONE;
}

/* whether a unit rides in a seat its player sees from behind (the seat's third person camera, as the director has
it: a vehicle's driver or passenger) */
static boolean playback_rides_seen_from_behind(
	struct unit_datum const *unit)
{
	struct object_datum const *parent;
	struct unit_seat const *seat;
	struct unit_definition const *definition;

	if (!unit || unit->object.parent_object_index == NONE)
		return FALSE;
	parent = object_get(unit->object.parent_object_index);
	if (!TEST_FLAG(_object_mask_unit, parent->object.type))
		return FALSE;
	definition = unit_definition_get(parent->definition_index);
	if (unit->unit.parent_seat_index < 0 || unit->unit.parent_seat_index >= definition->unit.seats.count)
		return FALSE;
	seat = TAG_BLOCK_GET_ELEMENT(&definition->unit.seats, unit->unit.parent_seat_index, struct unit_seat);
	return TEST_FLAG(seat->flags, _unit_seat_third_person_camera_bit);
}

/* the player seen through their eyes: NONE in the free camera, with none watched, or while the one watched rides in a
seat seen from behind (their body drawn, no arms) */
long network_replay_eyes_player(
	void)
{
	long player_index;
	struct player_datum *player;

	if (!playback.playing || playback.view != _playback_view_eyes || !game_in_progress())
		return NONE;
	player_index = playback_watched();
	player = player_index != NONE ? player_try_and_get(player_index) : NULL;
	if (!player || player->unit_index == NONE || playback_rides_seen_from_behind(unit_try_and_get(player->unit_index)))
		return NONE;
	return player_index;
}

/* the game's input for a controller this frame, as input_abstraction.c keeps it (its controls bound by the player's
own settings, the keyboard's keys among them): twelve buttons (each the ticks held; jump the first, crouch the
eleventh), then the movement and look sticks */
struct playback_input
{
	byte buttons[12];
	real forward;
	real strafe;
	real yaw;
	real pitch;
};

#define PLAYBACK_INPUT_JUMP 0
#define PLAYBACK_INPUT_CROUCH 10
/* world units a second at full stick, radians a second at full look */
#define PLAYBACK_FREE_SPEED 6.0f
#define PLAYBACK_FREE_TURN 2.5f
#define PLAYBACK_FREE_PITCH_MOST 1.5f

/* the free camera moved by controller 0's (and the keyboard's) movement, turned by its look and the mouse, raised by
jump and lowered by crouch, for the frame's seconds (none while the game's own menus take the input) */
static void playback_free_controls(
	void)
{
	struct playback_input const *input =
		(struct playback_input const *)input_abstraction_get_input_state(0);
	real seconds = main_get_seconds_elapsed();
	real yaw, pitch, length, cos_pitch;
	float mouse_yaw = 0.0f, mouse_pitch = 0.0f;
	real_vector3d forward = playback.free_forward;
	real_vector3d left;

	if (!input || seconds <= 0.0f || seconds > 0.25f)
		return;
	length = (real)sqrt(forward.i * forward.i + forward.j * forward.j + forward.k * forward.k);
	if (length <= 0.0f)
		return;
	yaw = (real)atan2(forward.j, forward.i);
	pitch = (real)asin(PIN(forward.k / length, -1.0f, 1.0f));
	halo_linux_mouse_look((short)ae_seats_keyboard_controller(), &mouse_yaw, &mouse_pitch);
	yaw += input->yaw * PLAYBACK_FREE_TURN * seconds + mouse_yaw;
	pitch += input->pitch * PLAYBACK_FREE_TURN * seconds + mouse_pitch;
	pitch = PIN(pitch, -PLAYBACK_FREE_PITCH_MOST, PLAYBACK_FREE_PITCH_MOST);
	cos_pitch = (real)cos(pitch);
	forward.i = cos_pitch * (real)cos(yaw);
	forward.j = cos_pitch * (real)sin(yaw);
	forward.k = (real)sin(pitch);
	left.i = -(real)sin(yaw);
	left.j = (real)cos(yaw);
	left.k = 0.0f;
	playback.free_position.x += (forward.i * input->forward + left.i * input->strafe) * PLAYBACK_FREE_SPEED * seconds;
	playback.free_position.y += (forward.j * input->forward + left.j * input->strafe) * PLAYBACK_FREE_SPEED * seconds;
	playback.free_position.z += forward.k * input->forward * PLAYBACK_FREE_SPEED * seconds;
	if (input->buttons[PLAYBACK_INPUT_JUMP])
		playback.free_position.z += PLAYBACK_FREE_SPEED * seconds;
	if (input->buttons[PLAYBACK_INPUT_CROUCH])
		playback.free_position.z -= PLAYBACK_FREE_SPEED * seconds;
	playback.free_forward = forward;
}

/* frames drawn while playing (window 0's camera asked), and the last of them the watched player's body was left out of
(render_objects.c asks network_replay_hides_object), for the status line */
static unsigned long playback_body_hidden_frame, playback_frames;

/* the place a view takes with no unit to take it from: the map's first player starting location, at a standing
player's eyes, looking its way; FALSE (the origin) for a map with none */
#define PLAYBACK_EYE_HEIGHT 0.62f

static boolean playback_start_place(
	real_point3d *position,
	real_vector3d *forward)
{
	struct scenario *scenario = global_scenario_get();
	struct player_starting_location const *location;

	csmemset(position, 0, sizeof(*position));
	forward->i = 1.0f;
	forward->j = 0.0f;
	forward->k = 0.0f;
	if (!scenario || scenario->players.count <= 0)
		return FALSE;
	location = TAG_BLOCK_GET_ELEMENT(&scenario->players, 0, struct player_starting_location);
	*position = location->position;
	position->z += PLAYBACK_EYE_HEIGHT;
	forward->i = (real)cos(location->facing);
	forward->j = (real)sin(location->facing);
	return TRUE;
}

/* the watched unit's eyes as this frame draws them: where its camera is, moved with the unit to where the frame draws
it (render_interpolation_object_point), as the world around it is drawn. Its aim turns to each new aim the recording
holds over as many ticks as came between the last two (render_interpolation_fraction into the tick), from the aim
drawn when it came. (Taken as the last tick left it, the view stepped once a tick, and once every new aim, a few
ticks apart, while the world, the unit's body and weapon among it, glided.) */
#define PLAYBACK_AIM_SPAN_MOST 6
/* a seat seen from behind: how far behind and above where the rider sits the view is (world units) */
#define PLAYBACK_RIDE_BACK 4.5f
#define PLAYBACK_RIDE_UP 1.2f

static void playback_eyes_camera(
	long unit_index,
	struct unit_datum const *unit,
	struct observer_result *camera)
{
	long now = game_time_get();
	real u;
	real_vector3d forward;
	real length;
	struct object_datum const *object = object_get(unit_index);
	real_point3d drawn_before = playback.camera.position;
	real_vector3d aim_before = playback.eyes_aim_drawn;
	real_vector3d raw_before = playback.eyes_aim_raw;
	real_vector3d const *looking = &unit->unit.looking_vector;
	boolean counted = playback.camera_valid && unit_index == playback.eyes_unit && game_time_get_speed() > 0.0f;

	if (unit_index != playback.eyes_unit)
	{
		/* (another unit: from its aim now, no turn from the one before's) */
		playback.eyes_unit = unit_index;
		playback.eyes_tick = now;
		playback.eyes_aim_seen = *looking;
		playback.eyes_aim_from = *looking;
		playback.eyes_aim_to = *looking;
		playback.eyes_aim_drawn = *looking;
		playback.eyes_aim_raw = *looking;
		playback.eyes_aim_tick = now;
		playback.eyes_aim_span = 1;
	}
	else if (now != playback.eyes_tick)
	{
		playback.eyes_tick = now;
		playback.eyes_ticks++;
		if (looking->i != playback.eyes_aim_seen.i || looking->j != playback.eyes_aim_seen.j ||
			looking->k != playback.eyes_aim_seen.k)
		{
			playback.eyes_turns++;
			playback.eyes_aim_span = PIN(now - playback.eyes_aim_tick, 1, PLAYBACK_AIM_SPAN_MOST);
			playback.eyes_aim_tick = now;
			playback.eyes_aim_seen = *looking;
			playback.eyes_aim_from = playback.eyes_aim_drawn;
			playback.eyes_aim_to = *looking;
		}
	}
	unit_get_camera_position(unit_index, &camera->position);
	render_interpolation_object_point(unit_index, &camera->position);
	u = ((real)(now - playback.eyes_aim_tick) + render_interpolation_fraction()) / (real)playback.eyes_aim_span;
	u = PIN(u, 0.0f, 1.0f);
	forward.i = playback.eyes_aim_from.i + (playback.eyes_aim_to.i - playback.eyes_aim_from.i) * u;
	forward.j = playback.eyes_aim_from.j + (playback.eyes_aim_to.j - playback.eyes_aim_from.j) * u;
	forward.k = playback.eyes_aim_from.k + (playback.eyes_aim_to.k - playback.eyes_aim_from.k) * u;
	length = (real)sqrt(forward.i * forward.i + forward.j * forward.j + forward.k * forward.k);
	if (length > 0.001f)
	{
		forward.i /= length;
		forward.j /= length;
		forward.k /= length;
	}
	else
		forward = playback.eyes_aim_to;
	camera->forward = forward;
	playback.eyes_aim_drawn = forward;
	playback.eyes_aim_raw = *looking;
	/* (riding in a seat seen from behind, as the player saw it: behind and above where they sit, along their aim, pulled
	in short of the level and its scenery) */
	if (playback_rides_seen_from_behind(unit))
	{
		real_point3d seat = camera->position;
		real_vector3d back, ray;
		struct collision_result collision;
		real flat = (real)sqrt(forward.i * forward.i + forward.j * forward.j);

		back.i = flat > 0.001f ? -forward.i / flat : -1.0f;
		back.j = flat > 0.001f ? -forward.j / flat : 0.0f;
		ray.i = back.i * PLAYBACK_RIDE_BACK;
		ray.j = back.j * PLAYBACK_RIDE_BACK;
		ray.k = PLAYBACK_RIDE_UP;
		if (collision_test_vector(FLAG(_collision_test_structure_bit) | FLAG(_collision_test_objects_bit) |
			FLAG(_collision_test_objects_scenery_bit), &seat, &ray, unit->object.parent_object_index, &collision))
		{
			real kept = MAX(0.0f, collision.t - PLAYBACK_FREE_MARGIN / PLAYBACK_RIDE_BACK);

			ray.i *= kept;
			ray.j *= kept;
			ray.k *= kept;
		}
		camera->position.x = seat.x + ray.i;
		camera->position.y = seat.y + ray.j;
		camera->position.z = seat.z + ray.k;
	}
	/* (the status line's: frames drawn while the unit moves and those the view stood still in; frames drawn while a
	new aim is turned to and those the view did not turn in) */
	if (!counted)
		return;
	if (object->object.translational_velocity.i * object->object.translational_velocity.i +
		object->object.translational_velocity.j * object->object.translational_velocity.j > 0.0001f)
	{
		playback.eyes_moving_frames++;
		if (drawn_before.x == camera->position.x && drawn_before.y == camera->position.y &&
			drawn_before.z == camera->position.z)
		{
			playback.eyes_still_frames++;
		}
	}
	if (now - playback.eyes_aim_tick < playback.eyes_aim_span &&
		(playback.eyes_aim_from.i != playback.eyes_aim_to.i || playback.eyes_aim_from.j != playback.eyes_aim_to.j ||
		playback.eyes_aim_from.k != playback.eyes_aim_to.k))
	{
		playback.eyes_turning_frames++;
		if (aim_before.i == forward.i && aim_before.j == forward.j && aim_before.k == forward.k)
			playback.eyes_unturned_frames++;
		if (raw_before.i == looking->i && raw_before.j == looking->j && raw_before.k == looking->k)
			playback.eyes_raw_unturned_frames++;
	}
}

/* the view for this frame (window 0's) */
static void playback_camera_make(
	void)
{
	long player_index = playback_watched();
	struct player_datum *player = player_index != NONE ? player_try_and_get(player_index) : NULL;
	struct unit_datum *unit = player && player->unit_index != NONE ? unit_try_and_get(player->unit_index) : NULL;
	struct observer_result camera;

	if (playback.view == _playback_view_eyes && !unit && playback.camera_valid)
	{
		/* (the player watched not in the game yet, or dead: the view held where it was till they are) */
		return;
	}
	csmemset(&camera, 0, sizeof(camera));
	camera.up.k = 1.0f;
	camera.field_of_view = PLAYBACK_FIELD_OF_VIEW;
	if (playback.view == _playback_view_eyes && unit)
		playback_eyes_camera(player->unit_index, unit, &camera);
	else if (playback.view == _playback_view_eyes)
	{
		/* (no one in the game yet: where the map's first player starts, till someone is) */
		playback_start_place(&camera.position, &camera.forward);
		playback.eyes_unit = NONE;
	}
	else
	{
		playback_free_controls();
		camera.position = playback.free_position;
		camera.forward = playback.free_forward;
	}
	if (camera.forward.i == 0.0f && camera.forward.j == 0.0f && camera.forward.k == 0.0f)
		camera.forward.i = 1.0f;
	scenario_location_from_point(&camera.location, &camera.position);
	playback.camera = camera;
	playback.camera_valid = TRUE;
}

struct observer_result const *network_replay_camera(
	void)
{
	if (!playback.playing || !game_in_progress())
		return NULL;
	playback_frames++;
	playback_camera_make();
	return playback.camera_valid ? &playback.camera : NULL;
}

boolean network_replay_playback_idle(
	struct network_game_client *client)
{
	if (!playback.playing || playback.leaving)
		return TRUE;
	playback_feed(client);
	/* (the host's objects, all of them, came at the game's first tick: as a client is told once the host has sent it
	them all, its own objects the host has not are let go) */
	if (!playback.synchronized && game_in_progress())
	{
		playback.synchronized = TRUE;
		network_objects_handle_synchronized();
	}
	/* (a seek: played to at its speed, then at the speed asked) */
	if (playback.seek_tick != NONE && game_time_get() >= playback.seek_tick)
	{
		playback.seek_tick = NONE;
		game_time_set_speed(playback.paused ? 0.0f : (real)playback.speed_percent / 100.0f);
		platform_log("replay: at tick %ld", game_time_get());
	}
	/* (its end: the records played and a moment more, or the game over) */
	if ((playback.next >= playback.stream.end && game_time_get() > playback.last_fed_tick + PLAYBACK_END_TICKS) ||
		network_game_client_port_in_postgame(client))
	{
		platform_log("replay: %s ended at tick %ld", playback.name, game_time_get());
		playback.leaving = TRUE;
		network_game_abort();
	}
	return TRUE;
}

void network_replay_playback_stop(
	void)
{
	if (playback.setting_up)
		return;
	if (playback.records)
	{
		short map, gametype;

		playback_last_game_read(&map, &gametype);
		platform_log("replay: through the eyes, %lu of %lu frames drawn while the unit moved stood still, %lu of %lu "
			"drawn while its aim turned did not turn (as recorded, %lu would not have); a new aim on %lu of %lu ticks",
			playback.eyes_still_frames, playback.eyes_moving_frames, playback.eyes_unturned_frames,
			playback.eyes_turning_frames, playback.eyes_raw_unturned_frames, playback.eyes_turns, playback.eyes_ticks);
		platform_log("replay: stopped; the LAST GAME card's map and game type %s (map %d, game type %d; before %d, %d)",
			map == playback_last_game.map && gametype == playback_last_game.gametype ? "are as before" : "CHANGED",
			map, gametype, playback_last_game.map, playback_last_game.gametype);
		playback_last_game.noted = FALSE;
		playback_theater_wanted = TRUE;
		playback_menus_restore();
		game_time_set_speed(1.0f);
	}
	playback_free();
}

boolean network_replay_theater_wanted(
	void)
{
	boolean wanted = playback_theater_wanted;

	playback_theater_wanted = FALSE;
	return wanted;
}

/* ---------- the controls (DEV MODE's replay command; the Theater screen's) */

boolean network_replay_view_eyes(
	char const *player_name)
{
	struct data_iterator iterator;
	struct player_datum *player;

	if (!playback.playing)
		return FALSE;
	if (!player_name || !player_name[0])
	{
		playback.view = _playback_view_eyes;
		return TRUE;
	}
	data_iterator_new(&iterator, player_data);
	while ((player = data_iterator_next(&iterator)) != NULL)
	{
		char name[NUMBEROF(player->name) + 1];
		short index;

		for (index = 0; index < (short)NUMBEROF(player->name) && player->name[index]; index++)
			name[index] = (char)tolower(player_name_character_ascii(player->name[index]));
		name[index] = 0;
		if (!strcmp(name, player_name) || (player_name[0] >= '1' && player_name[0] <= '9' && !player_name[1] &&
			DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index) == player_name[0] - '1'))
		{
			/* (not yet in the game, or dead: watched once they are, the view held till then) */
			playback.watched = (short)DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index);
			playback.view = _playback_view_eyes;
			return TRUE;
		}
	}
	return FALSE;
}

void network_replay_view_free(
	void)
{
	long player_index = playback_watched();
	struct player_datum *player = player_index != NONE ? player_try_and_get(player_index) : NULL;
	struct unit_datum *unit = player && player->unit_index != NONE ? unit_try_and_get(player->unit_index) : NULL;
	long ignored = unit ? player->unit_index : NONE;
	real_point3d target;
	real length;

	if (!playback.playing)
		return;
	playback.view = _playback_view_free;
	/* (placed by the player watched; with no unit in the game yet, by where the map's first player starts) */
	if (unit)
	{
		unit_get_camera_position(player->unit_index, &target);
		playback.free_forward = unit->unit.looking_vector;
	}
	else if (!playback_start_place(&target, &playback.free_forward))
	{
		playback.free_position = target;
		platform_log("replay: free camera: no player has a unit and the map no starting location to place it by");
		return;
	}
	playback.free_forward.k = 0.0f;
	length = (real)sqrt(playback.free_forward.i * playback.free_forward.i +
		playback.free_forward.j * playback.free_forward.j);
	if (length < 0.001f)
	{
		playback.free_forward.i = 1.0f;
		playback.free_forward.j = 0.0f;
		length = 1.0f;
	}
	playback.free_forward.i /= length;
	playback.free_forward.j /= length;
	playback.free_position.x = target.x - playback.free_forward.i * PLAYBACK_FREE_BACK;
	playback.free_position.y = target.y - playback.free_forward.j * PLAYBACK_FREE_BACK;
	playback.free_position.z = target.z + PLAYBACK_FREE_UP;
	/* (looking down at them) */
	playback.free_forward.i *= PLAYBACK_FREE_BACK;
	playback.free_forward.j *= PLAYBACK_FREE_BACK;
	playback.free_forward.k = -PLAYBACK_FREE_UP;
	length = (real)sqrt(playback.free_forward.i * playback.free_forward.i +
		playback.free_forward.j * playback.free_forward.j + playback.free_forward.k * playback.free_forward.k);
	playback.free_forward.i /= length;
	playback.free_forward.j /= length;
	playback.free_forward.k /= length;
	/* (a place outside the level, or anything between the player and it, a margin past it included (a rock, a
	wall, the ground): over them instead, looking down, as high as the space over them lets) */
	{
		struct location location;
		struct collision_result collision;
		real_vector3d ray;
		real reach;

		ray.i = playback.free_position.x - target.x;
		ray.j = playback.free_position.y - target.y;
		ray.k = playback.free_position.z - target.z;
		reach = (real)sqrt(ray.i * ray.i + ray.j * ray.j + ray.k * ray.k);
		scale_vector3d(&ray, (reach + PLAYBACK_FREE_MARGIN) / reach, &ray);
		scenario_location_from_point(&location, &playback.free_position);
		if (location.cluster_index == NONE ||
			collision_test_vector(_collision_test_for_projectiles_flags, &target, &ray, ignored, &collision))
		{
			real up = PLAYBACK_FREE_UP;

			ray.i = 0.0f;
			ray.j = 0.0f;
			ray.k = PLAYBACK_FREE_UP + PLAYBACK_FREE_MARGIN;
			if (collision_test_vector(_collision_test_for_projectiles_flags, &target, &ray, ignored, &collision))
				up = MAX(0.0f, collision.t * ray.k - PLAYBACK_FREE_MARGIN);
			playback.free_position = target;
			playback.free_position.z += up;
			playback.free_forward.i = 0.01f;
			playback.free_forward.j = 0.0f;
			playback.free_forward.k = -1.0f;
			platform_log("replay: free camera: behind the %s is blocked; over it, %.2f up",
				unit ? "player" : "starting location", up);
		}
	}
	if (unit)
		platform_log("replay: free camera at (%.2f %.2f %.2f) looking (%.2f %.2f %.2f), player %ld at (%.2f %.2f %.2f)",
			playback.free_position.x, playback.free_position.y, playback.free_position.z, playback.free_forward.i,
			playback.free_forward.j, playback.free_forward.k, (long)DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index) + 1,
			target.x, target.y, target.z);
	else
		platform_log("replay: free camera at (%.2f %.2f %.2f) looking (%.2f %.2f %.2f), by the first starting location "
			"(no player has a unit yet)", playback.free_position.x, playback.free_position.y, playback.free_position.z,
			playback.free_forward.i, playback.free_forward.j, playback.free_forward.k);
}

void network_replay_set_paused(
	boolean paused)
{
	if (!playback.playing)
		return;
	playback.paused = paused;
	game_time_set_speed(paused ? 0.0f : (real)playback.speed_percent / 100.0f);
}

void network_replay_set_speed(
	long percent)
{
	if (!playback.playing)
		return;
	playback.speed_percent = percent < 25 ? 25 : percent > 400 ? 400 : percent;
	if (!playback.paused && playback.seek_tick == NONE)
		game_time_set_speed((real)playback.speed_percent / 100.0f);
}

boolean network_replay_paused(
	void)
{
	return network_replay_playing() && playback.paused;
}

long network_replay_speed_percent(
	void)
{
	return network_replay_playing() ? playback.speed_percent : 100;
}

boolean network_replay_view_is_free(
	void)
{
	return network_replay_playing() && playback.view == _playback_view_free;
}

long network_replay_position_seconds(
	void)
{
	return playback.playing ? (game_time_get() - (long)playback.header.start_tick) / TICKS_PER_SECOND : 0;
}

long network_replay_length_seconds(
	void)
{
	return playback.records ? (long)(playback.stream.last_tick - playback.stream.first_tick) / TICKS_PER_SECOND : 0;
}

/* a seek to a second of the game: ahead, played to at 4 times; back, the replay begun again and played to it */
boolean network_replay_seek(
	long seconds)
{
	long target;

	if (!playback.playing)
		return FALSE;
	if (seconds < 0)
		seconds = 0;
	if (seconds > network_replay_length_seconds())
		seconds = network_replay_length_seconds();
	target = (long)playback.header.start_tick + seconds * TICKS_PER_SECOND;
	if (target < game_time_get())
	{
		char name[AE_REPLAY_NAME_SIZE];
		char why[96];
		/* (the view, its place and the speed kept: the replay begun again is the same one, watched the same way) */
		short view = playback.view, watched = playback.watched;
		real_point3d free_position = playback.free_position;
		real_vector3d free_forward = playback.free_forward;
		long speed_percent = playback.speed_percent;
		boolean paused = playback.paused;

		snprintf(name, sizeof(name), "%s", playback.name);
		if (!network_replay_play(name, why, sizeof(why)))
			return FALSE;
		playback.view = view;
		playback.watched = watched;
		playback.free_position = free_position;
		playback.free_forward = free_forward;
		playback.speed_percent = speed_percent;
		playback.paused = paused;
	}
	playback.seek_tick = target;
	game_time_set_speed(PLAYBACK_SEEK_SPEED);
	return TRUE;
}

boolean network_replay_seeking(
	void)
{
	return network_replay_playing() && playback.seek_tick != NONE;
}

void network_replay_end(
	void)
{
	if (!network_replay_playing())
		return;
	platform_log("replay: left at tick %ld", game_time_get());
	playback.leaving = TRUE;
	network_game_abort();
}

boolean network_replay_hides_object(
	long object_index)
{
	long player_index = network_replay_eyes_player();
	struct player_datum *player = player_index != NONE ? player_try_and_get(player_index) : NULL;

	if (!player || player->unit_index == NONE || player->unit_index != object_index)
		return FALSE;
	playback_body_hidden_frame = playback_frames;
	return TRUE;
}

/* what window 0 draws in first person, short, for the status line: the arms of each local player that has any (its
unit, weapon, shown or hidden), which player local player 0 is (0: none), and whether the watched player's body was
left out of the last frame */
static void playback_first_person_describe(
	char *text,
	size_t size)
{
	short local;
	size_t used = 0;
	long local_player = local_player_get_player_index(0);

	text[0] = 0;
	for (local = 0; local < 4 && used < size; local++)
	{
		long unit_index = NONE, weapon_index = NONE;
		boolean visible = FALSE;

		first_person_weapon_port_state(local, &unit_index, &weapon_index, &visible);
		if (unit_index == NONE)
			continue;
		used += (size_t)snprintf(text + used, size - used, "; arms%d %lx/%lx %s", local, (unsigned long)unit_index,
			(unsigned long)weapon_index, visible ? "on" : "off");
	}
	if (used < size)
		snprintf(text + used, size - used, "; local0 p%ld; body %s",
			local_player == NONE ? 0L : (long)DATUM_INDEX_TO_ABSOLUTE_INDEX(local_player) + 1,
			playback_frames && playback_body_hidden_frame + 1 >= playback_frames ? "hidden" : "drawn");
}

void network_replay_playback_status(
	char *text,
	long size)
{
	char drawn[96];
	long player_index = playback.playing ? playback_watched() : NONE;
	char watched[24];

	if (player_index != NONE)
	{
		struct player_datum *player = player_try_and_get(player_index);

		snprintf(watched, sizeof(watched), "%ld%s", (long)DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index) + 1,
			player && player->unit_index != NONE &&
			playback_rides_seen_from_behind(unit_try_and_get(player->unit_index)) ? " (riding)" : "");
	}
	else if (playback.watched != NONE)
		snprintf(watched, sizeof(watched), "%d (waiting)", playback.watched + 1);
	else
		snprintf(watched, sizeof(watched), "none");
	if (!playback.records)
		snprintf(text, (size_t)size, "not playing");
	else if (playback.starting)
		snprintf(text, (size_t)size, "loading %s", playback.name);
	else
	{
		playback_first_person_describe(drawn, sizeof(drawn));
		snprintf(text, (size_t)size, "%s %ld/%ld s, %s%s, %ld%%%s%s%s; still %lu/%lu frames, unturned %lu/%lu, new aim "
			"%lu/%lu ticks", playback.name, network_replay_position_seconds(), network_replay_length_seconds(),
			playback.view == _playback_view_free ? "free" : "eyes ", playback.view == _playback_view_free ? "" : watched,
			playback.speed_percent, playback.paused ? ", paused" : "", playback.seek_tick != NONE ? ", seeking" : "",
			drawn, playback.eyes_still_frames, playback.eyes_moving_frames, playback.eyes_unturned_frames,
			playback.eyes_turning_frames, playback.eyes_turns, playback.eyes_ticks);
	}
}
