/*
AE_MCC_MUSIC.C

The menus' music from the player's MCC installation (ae_mcc_music.h). audio.menu_music: "classic" (the title music),
"mcc_shuffle" (MCC's songs in a new order each start of the game), "mcc_order" (in the banks' order), "off".

A worker thread of its own (ae_worker.h) reads the banks' tables once, the first time the menus want a song, then
reads each piece of the song playing, wraps its Vorbis packets as an Ogg stream (ae_mcc_music_ogg) and decodes it
(stb_vorbis, this unit's own instance) into a ring of about two seconds, ahead of the mixer. The mixer's thread only
takes from the ring (ae_mcc_music_mix), fading in when the menus' music starts and out when it stops; stopped, the
song waits where it was, and plays on from there the next time the menus show. The ring is the one thing the two
threads share, with the play/stop wish and the volume: plain atomics, no lock on the mixer's side.

The banks: each game folder's fmod/pc/sfx.fsb in the installation (an FMOD sound bank of Vorbis streams; the folders
in name order), the songs of each (ae_mcc_music_songs) less a song listed already (a bank that carries another game's
music again, or one song twice). A game whose sounds are kept another way has no such bank, and no songs here.
Only streams whose Vorbis setup header fsbex's table holds are played (port/third_party/fsbex). No other thread reads
the banks, so a bank's FILE is the worker's alone.
*/

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "ae_mcc_music.h"
#include "ae_mcc_music_rules.h"
#include "../src/ae_worker.h"
#include "../src/ae_mcc_workshop.h"
#include "ae_mcc_music_fsbex.h"
/* (the banks are host paths, not the game's Xbox paths: the C library's own fopen, as ae_announcer.c's) */
#undef fopen

#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_NO_PUSHDATA_API
#define stb_vorbis_get_info ae_music_stb_get_info
#define stb_vorbis_get_comment ae_music_stb_get_comment
#define stb_vorbis_get_error ae_music_stb_get_error
#define stb_vorbis_close ae_music_stb_close
#define stb_vorbis_get_sample_offset ae_music_stb_get_sample_offset
#define stb_vorbis_get_file_offset ae_music_stb_get_file_offset
#define stb_vorbis_decode_memory ae_music_stb_decode_memory
#define stb_vorbis_open_memory ae_music_stb_open_memory
#define stb_vorbis_seek_frame ae_music_stb_seek_frame
#define stb_vorbis_seek ae_music_stb_seek
#define stb_vorbis_seek_start ae_music_stb_seek_start
#define stb_vorbis_stream_length_in_samples ae_music_stb_stream_length_in_samples
#define stb_vorbis_stream_length_in_seconds ae_music_stb_stream_length_in_seconds
#define stb_vorbis_get_frame_float ae_music_stb_get_frame_float
#define stb_vorbis_get_frame_short_interleaved ae_music_stb_get_frame_short_interleaved
#define stb_vorbis_get_frame_short ae_music_stb_get_frame_short
#define stb_vorbis_get_samples_float_interleaved ae_music_stb_get_samples_float_interleaved
#define stb_vorbis_get_samples_float ae_music_stb_get_samples_float
#define stb_vorbis_get_samples_short_interleaved ae_music_stb_get_samples_short_interleaved
#define stb_vorbis_get_samples_short ae_music_stb_get_samples_short
/* (port/third_party/stb, a game include directory) */
#include "stb_vorbis.c"

void platform_log(char const *format, ...);
char const *config_string(char const *name);
double config_real(char const *name);
unsigned long config_changes(void);
int ae_mcc_copy_root(char *root, size_t size);
/* the main menu's music (ui_widget.c; boolean is an unsigned char) */
unsigned char ui_main_menu_music_active(void);
void ui_start_main_menu_music(void);
void ui_stop_main_menu_music(void);
/* (ae_announcer.h; its header brings in no game types, but this unit takes none) */
void ae_announcer_update(void);

#define BANKS 16                      /* game folders with a bank */
#define BANK_FOLDER 64
#define BANK_FILE "fmod/pc/sfx.fsb"
#define STREAMS_MAXIMUM 65536          /* a bank's streams (16 000 to 33 000) */
#define SONGS_MAXIMUM 1024
#define PIECES_MAXIMUM 4096            /* the songs' streams, all banks */
#define RING_FRAMES 262144u            /* about five seconds at 48 kHz (a power of two): a bank on a network share
                                          can be slow to answer when the next piece is read */
#define DECODE_FRAMES 2048
#define FADE_FRAMES 36000.0f           /* 0.75 s */
#define WORKER_IDLE_MS 15
#define FAILURES_MAXIMUM 8             /* songs in a row that do not play: the worker gives up */
#define ROOT_SIZE 1024
#define GAPS_LOG_SECONDS 10

enum
{
	MODE_CLASSIC,
	MODE_SHUFFLE,
	MODE_ORDER,
	MODE_OFF
};


struct piece
{
	unsigned char bank;
	unsigned channels;
	uint32_t frames, setup_crc;
	uint64_t offset, size;
	char name[AE_MCC_MUSIC_NAME];
};

struct song
{
	unsigned first, count;             /* pieces */
	uint32_t frames;
};

static struct
{
	/* the game's thread */
	int mode;                          /* audio.menu_music's */
	int owner;                         /* the menus' music playing now is this (or OFF's silence) */
	int worker_started;
	int config_read;                   /* the settings read once (config_read_at holds) */
	unsigned long config_read_at;
	/* shared: atomics */
	int playing;                       /* the menus want the music */
	int wanted_mode;                   /* MODE_SHUFFLE or MODE_ORDER, for the worker */
	int volume_milli;                  /* audio.music_volume x 1000 */
	int dead;                          /* the worker gave up (no banks, no songs) */
	unsigned write_frames, read_frames;
	unsigned flush_frames;             /* the worker's: what was written before this is a song no longer wanted */
	unsigned gaps;                     /* the mixer's: times the ring ran dry while a song played */
	/* the mixer's */
	float gain;
	int dry;                           /* the ring ran dry, counted once */
	/* the worker's */
	char root[ROOT_SIZE];
	FILE *banks[BANKS];
	char bank_names[BANKS][BANK_FOLDER];  /* the game folder */
	int bank_count;
	struct piece *pieces;
	int piece_count;
	struct song songs[SONGS_MAXIMUM];
	int song_count;
	int order[SONGS_MAXIMUM];
	int order_mode, order_index;
	unsigned piece_index;              /* of the song playing */
	stb_vorbis *decoder;
	unsigned char *ogg;
	/* the ring: frames of two channels */
	float ring[RING_FRAMES * 2];
} music;

static unsigned atomic_load_unsigned(unsigned *value)
{
	return __atomic_load_n(value, __ATOMIC_ACQUIRE);
}

static void atomic_store_unsigned(unsigned *value, unsigned new_value)
{
	__atomic_store_n(value, new_value, __ATOMIC_RELEASE);
}

static int atomic_load_int(int *value)
{
	return __atomic_load_n(value, __ATOMIC_ACQUIRE);
}

static void atomic_store_int(int *value, int new_value)
{
	__atomic_store_n(value, new_value, __ATOMIC_RELEASE);
}

static int mode_read(void)
{
	char const *value = config_string("audio.menu_music");

	if (!value)
		return MODE_CLASSIC;
	if (!strcmp(value, "mcc_shuffle"))
		return MODE_SHUFFLE;
	if (!strcmp(value, "mcc_order"))
		return MODE_ORDER;
	if (!strcmp(value, "off"))
		return MODE_OFF;
	return MODE_CLASSIC;
}

static void settings_read(void)
{
	double volume;

	if (music.config_read && music.config_read_at == config_changes())
		return;
	music.config_read = 1;
	music.config_read_at = config_changes();
	music.mode = mode_read();
	if (music.mode == MODE_SHUFFLE || music.mode == MODE_ORDER)
		atomic_store_int(&music.wanted_mode, music.mode);
	volume = config_real("audio.music_volume");
	if (volume < 0.0)
		volume = 0.0;
	if (volume > 1.0)
		volume = 1.0;
	atomic_store_int(&music.volume_milli, (int)(volume * 1000.0 + 0.5));
}

/* ---------- the worker */

static uint32_t const known_setups[2] = { 0xAD11D38Du, 0x632B1FACu };

static unsigned char const *setup_for(uint32_t crc, size_t *size)
{
	if (crc == 0xAD11D38Du)
	{
		*size = sizeof(fsbex_vorbis_setup_ad11d38d);
		return fsbex_vorbis_setup_ad11d38d;
	}
	if (crc == 0x632B1FACu)
	{
		*size = sizeof(fsbex_vorbis_setup_632b1fac);
		return fsbex_vorbis_setup_632b1fac;
	}
	*size = 0;
	return NULL;
}

static int read_at(FILE *file, uint64_t offset, void *bytes, size_t size)
{
	/* (the banks are under 2 GB: a long offset, on every build) */
	if (offset > (uint64_t)0x7FFFFFFFL || fseek(file, (long)offset, SEEK_SET) != 0)
		return 0;
	return fread(bytes, 1, size, file) == size;
}

static uint64_t file_size(FILE *file)
{
	long size;

	if (fseek(file, 0, SEEK_END) != 0)
		return 0;
	size = ftell(file);
	return size > 0 ? (uint64_t)size : 0;
}

/* one bank's songs added, less those already listed */
static void catalog_bank(int bank, struct ae_mcc_music_stream *streams)
{
	static struct ae_mcc_music_song songs[SONGS_MAXIMUM];
	unsigned char head[AE_MCC_MUSIC_FSB5_HEADER];
	unsigned char *table;
	size_t table_size;
	uint64_t size;
	int count, song_count, index, added = 0;
	char path[ROOT_SIZE + 64];
	FILE *file;

	snprintf(path, sizeof(path), "%s/%s/%s", music.root, music.bank_names[bank], BANK_FILE);
	file = fopen(path, "rb");
	if (!file)
		return;
	size = file_size(file);
	if (!read_at(file, 0, head, sizeof(head)) || !(table_size = ae_mcc_music_fsb5_table_size(head, sizeof(head))) ||
		table_size > size || !(table = (unsigned char *)malloc(table_size)))
	{
		platform_log("mcc music: %s is not a sound bank this reads", music.bank_names[bank]);
		fclose(file);
		return;
	}
	count = read_at(file, 0, table, table_size) ?
		ae_mcc_music_fsb5_streams(table, table_size, size, streams, STREAMS_MAXIMUM) : -1;
	free(table);
	if (count < 0)
	{
		platform_log("mcc music: %s's table is damaged", music.bank_names[bank]);
		fclose(file);
		return;
	}
	/* (the bank's .info, when there is one: only the streams whose source is music, not a level's ambience) */
	{
		char info_path[ROOT_SIZE + 80];
		FILE *info;

		snprintf(info_path, sizeof(info_path), "%s.info", path);
		if ((info = fopen(info_path, "rb")) != NULL)
		{
			unsigned char record[280];
			int stream, kept = 0;

			if (file_size(info) == (uint64_t)count * sizeof(record) && !fseek(info, 0, SEEK_SET))
			{
				for (stream = 0; stream < count && fread(record, 1, sizeof(record), info) == sizeof(record); stream++)
				{
					if (ae_mcc_music_path_is_music((char const *)record + 24, sizeof(record) - 24))
						kept++;
					else
						streams[stream].name[0] = 0;
				}
				platform_log("mcc music: %s: %d of %d streams are music (its .info)", music.bank_names[bank], kept, count);
			}
			fclose(info);
		}
	}
	song_count = ae_mcc_music_songs(streams, count, known_setups, 2, songs, SONGS_MAXIMUM);
	for (index = 0; index < song_count; index++)
	{
		struct ae_mcc_music_song const *song = songs + index;
		int other, repeated = 0;
		unsigned piece;

		/* (a song already listed, from an earlier bank or this one: the same pieces' lengths and sizes) */
		for (other = 0; other < music.song_count && !repeated; other++)
		{
			struct song const *known = music.songs + other;

			if (known->count != song->count || known->frames != song->frames)
				continue;
			repeated = 1;
			for (piece = 0; piece < song->count && repeated; piece++)
				repeated = music.pieces[known->first + piece].frames == streams[song->first + piece].frames &&
					music.pieces[known->first + piece].size == streams[song->first + piece].size;
		}
		if (repeated || music.song_count >= SONGS_MAXIMUM || music.piece_count + (int)song->count > PIECES_MAXIMUM)
			continue;
		music.songs[music.song_count].first = (unsigned)music.piece_count;
		music.songs[music.song_count].count = song->count;
		music.songs[music.song_count].frames = song->frames;
		for (piece = 0; piece < song->count; piece++)
		{
			struct ae_mcc_music_stream const *stream = streams + song->first + piece;
			struct piece *out = music.pieces + music.piece_count++;

			out->bank = (unsigned char)bank;
			out->channels = stream->channels;
			out->frames = stream->frames;
			out->setup_crc = stream->setup_crc;
			out->offset = stream->offset;
			out->size = stream->size;
			memcpy(out->name, stream->name, sizeof(out->name));
		}
		music.song_count++;
		added++;
	}
	platform_log("mcc music: %s: %d streams, %d songs (%d new)", music.bank_names[bank], count, song_count, added);
	music.banks[bank] = file;
}

/* a game folder of the installation's with a bank (ae_mcc_workshop_list_folder), kept in name order */
static int bank_folder(void *context, char const *name, int is_folder, int is_regular)
{
	char path[ROOT_SIZE + BANK_FOLDER + 32];
	FILE *file;
	int index;

	(void)context;
	(void)is_regular;
	if (!is_folder || !name[0] || name[0] == '.' || strlen(name) >= BANK_FOLDER || music.bank_count >= BANKS)
		return 0;
	/* (only a folder with a bank: the installation's other folders are many) */
	snprintf(path, sizeof(path), "%s/%s/%s", music.root, name, BANK_FILE);
	if (!(file = fopen(path, "rb")))
		return 0;
	fclose(file);
	for (index = music.bank_count; index > 0 && strcmp(music.bank_names[index - 1], name) > 0; index--)
		memcpy(music.bank_names[index], music.bank_names[index - 1], BANK_FOLDER);
	memcpy(music.bank_names[index], name, strlen(name) + 1);
	music.bank_count++;
	return 0;
}

static int catalog(void)
{
	struct ae_mcc_music_stream *streams;
	int bank;

	if (!ae_mcc_copy_root(music.root, sizeof(music.root)) || !music.root[0])
	{
		platform_log("mcc music: no MCC installation found");
		return 0;
	}
	streams = (struct ae_mcc_music_stream *)malloc(sizeof(*streams) * STREAMS_MAXIMUM);
	music.pieces = (struct piece *)malloc(sizeof(*music.pieces) * PIECES_MAXIMUM);
	if (!streams || !music.pieces)
	{
		free(streams);
		return 0;
	}
	ae_mcc_workshop_list_folder(music.root, bank_folder, NULL);
	for (bank = 0; bank < music.bank_count; bank++)
		catalog_bank(bank, streams);
	free(streams);
	platform_log("mcc music: %d songs", music.song_count);
	return music.song_count > 0;
}

static void decoder_close(void)
{
	if (music.decoder)
		stb_vorbis_close(music.decoder);
	music.decoder = NULL;
	free(music.ogg);
	music.ogg = NULL;
}

static void order_make(int mode)
{
	ae_mcc_music_order(music.order, music.song_count, mode == MODE_SHUFFLE, (uint32_t)time(NULL) ^ 0x5A17u);
	music.order_mode = mode;
	music.order_index = 0;
	music.piece_index = 0;
}

static struct song const *song_now(void)
{
	return music.songs + music.order[music.order_index];
}

static void song_next(void)
{
	decoder_close();
	music.piece_index = 0;
	if (++music.order_index >= music.song_count)
		order_make(music.order_mode);
}

/* the decoder opened on the song's next piece; 0 when it cannot be */
static int piece_open(void)
{
	struct song const *song = song_now();
	struct piece const *piece;
	unsigned char *data;
	unsigned char const *setup;
	size_t setup_size, capacity, size;
	int error = 0;

	if (music.piece_index >= song->count)
		return 0;
	piece = music.pieces + song->first + music.piece_index;
	setup = setup_for(piece->setup_crc, &setup_size);
	if (!setup || !music.banks[piece->bank] || piece->size > (64u << 20))
		return 0;
	data = (unsigned char *)malloc((size_t)piece->size);
	capacity = ae_mcc_music_ogg_capacity((size_t)piece->size, setup_size);
	music.ogg = (unsigned char *)malloc(capacity);
	if (!data || !music.ogg || !read_at(music.banks[piece->bank], piece->offset, data, (size_t)piece->size))
	{
		free(data);
		decoder_close();
		return 0;
	}
	size = ae_mcc_music_ogg(data, (size_t)piece->size, piece->channels, AE_MCC_MUSIC_RATE, piece->frames, setup,
		setup_size, music.ogg, capacity);
	free(data);
	if (size && size <= INT_MAX)
		music.decoder = stb_vorbis_open_memory(music.ogg, (int)size, &error, NULL);
	if (!music.decoder)
	{
		platform_log("mcc music: %s (%s) does not decode (%d)", piece->name, music.bank_names[piece->bank], error);
		decoder_close();
		return 0;
	}
	if (!music.piece_index)
		platform_log("mcc music: playing %s (%s), %u s, song %d of %d", piece->name, music.bank_names[piece->bank],
			(unsigned)(song->frames / AE_MCC_MUSIC_RATE), music.order_index + 1, music.song_count);
	return 1;
}

/* frames decoded into the ring, at most room; 0 at a piece's end (the next is opened), -1 when the song fails */
static int decode_some(unsigned room)
{
	float frames[DECODE_FRAMES * 2];
	unsigned write = atomic_load_unsigned(&music.write_frames), index;
	int got;

	if (!music.decoder && !piece_open())
		return -1;
	if (room > DECODE_FRAMES)
		room = DECODE_FRAMES;
	got = stb_vorbis_get_samples_float_interleaved(music.decoder, 2, frames, (int)room * 2);
	if (got <= 0)
	{
		decoder_close();
		music.piece_index++;
		if (music.piece_index >= song_now()->count)
			song_next();
		return 0;
	}
	for (index = 0; index < (unsigned)got; index++)
	{
		unsigned slot = (write + index) & (RING_FRAMES - 1);

		music.ring[slot * 2] = frames[index * 2];
		music.ring[slot * 2 + 1] = frames[index * 2 + 1];
	}
	atomic_store_unsigned(&music.write_frames, write + (unsigned)got);
	return got;
}

static void *worker(void *argument)
{
	int failures = 0;
	unsigned logged_gaps = 0;
	time_t logged_at = 0;

	(void)argument;
	if (!catalog())
	{
		atomic_store_int(&music.dead, 1);
		return NULL;
	}
	order_make(atomic_load_int(&music.wanted_mode));
	for (;;)
	{
		unsigned room = RING_FRAMES - (atomic_load_unsigned(&music.write_frames) -
			atomic_load_unsigned(&music.read_frames));
		int mode = atomic_load_int(&music.wanted_mode), got;

		/* (a new order: the next song is the new order's first, at once: the mixer drops what the ring holds of the
		last, and fades the new one in) */
		if (mode != music.order_mode)
		{
			decoder_close();
			order_make(mode);
			atomic_store_unsigned(&music.flush_frames, atomic_load_unsigned(&music.write_frames));
		}
		/* (the ring ran dry while a song played: logged, at most every GAPS_LOG_SECONDS) */
		{
			unsigned gaps = atomic_load_unsigned(&music.gaps);
			time_t now = time(NULL);

			if (gaps != logged_gaps && now - logged_at >= GAPS_LOG_SECONDS)
			{
				platform_log("mcc music: %u gaps so far (the banks were read too slowly to keep the music going)", gaps);
				logged_gaps = gaps;
				logged_at = now;
			}
		}
		if (room < DECODE_FRAMES)
		{
			ae_worker_sleep(WORKER_IDLE_MS);
			continue;
		}
		got = decode_some(room);
		if (got < 0)
		{
			if (++failures >= FAILURES_MAXIMUM)
			{
				platform_log("mcc music: %d songs in a row do not play; the menus' music stops", failures);
				atomic_store_int(&music.dead, 1);
				decoder_close();
				return NULL;
			}
			song_next();
		}
		else if (got > 0)
			failures = 0;
	}
}

/* ---------- the game's thread */

int ae_mcc_music_menu_start(void)
{
	settings_read();
	music.owner = 0;
	if (music.mode == MODE_CLASSIC)
		return 0;
	if (music.mode == MODE_OFF)
	{
		music.owner = 1;
		return 1;
	}
	/* (no installation, or a worker that found nothing: the title music) */
	if (atomic_load_int(&music.dead))
		return 0;
	if (!music.worker_started)
	{
		char root[ROOT_SIZE];

		if (!ae_mcc_copy_root(root, sizeof(root)))
			return 0;
		music.worker_started = ae_worker_start(worker, NULL);
		if (!music.worker_started)
			return 0;
	}
	atomic_store_int(&music.playing, 1);
	music.owner = 1;
	return 1;
}

int ae_mcc_music_menu_stop(void)
{
	int owner = music.owner;

	atomic_store_int(&music.playing, 0);
	music.owner = 0;
	return owner;
}

void ae_mcc_music_update(void)
{
	int mode = music.mode;

	/* (the announcer's next line, each frame: the HUD's draw, its other caller, stops at a game's end) */
	ae_announcer_update();
	/* (the worker found no song after the menus' music was given to it: the title music instead, once) */
	if (music.owner && atomic_load_int(&music.dead) && music.mode != MODE_OFF && ui_main_menu_music_active())
	{
		ui_stop_main_menu_music();
		ui_start_main_menu_music();
	}

	if (music.config_read && music.config_read_at == config_changes())
		return;
	settings_read();
	/* (a changed audio.menu_music while the menus' music plays: the new one now) */
	if (music.mode != mode && ui_main_menu_music_active())
	{
		ui_stop_main_menu_music();
		ui_start_main_menu_music();
	}
}

/* ---------- the mixer's thread */

void ae_mcc_music_mix(float *output, unsigned long frames, unsigned long channels, float master_volume)
{
	int playing = atomic_load_int(&music.playing);
	unsigned read, available, frame, flush;
	float volume, step = 1.0f / FADE_FRAMES;

	if (!playing && music.gain <= 0.0f)
		return;
	read = atomic_load_unsigned(&music.read_frames);
	/* (a song no longer wanted: its rest dropped, the next faded in) */
	flush = atomic_load_unsigned(&music.flush_frames);
	if ((int)(flush - read) > 0)
	{
		read = flush;
		music.gain = 0.0f;
		music.dry = 1;
	}
	available = atomic_load_unsigned(&music.write_frames) - read;
	/* (a gap: the ring running dry mid-song, once each time; not the wait for a song's first piece) */
	if (available >= frames)
		music.dry = 0;
	else if (playing && !music.dry && atomic_load_unsigned(&music.write_frames))
	{
		music.dry = 1;
		__atomic_add_fetch(&music.gaps, 1, __ATOMIC_RELAXED);
	}
	volume = master_volume * (float)atomic_load_int(&music.volume_milli) / 1000.0f;
	for (frame = 0; frame < frames && frame < available; frame++)
	{
		unsigned slot = (read + frame) & (RING_FRAMES - 1);

		music.gain += playing ? step : -step;
		if (music.gain > 1.0f)
			music.gain = 1.0f;
		if (music.gain <= 0.0f)
		{
			music.gain = 0.0f;
			break;
		}
		output[frame * channels] += music.ring[slot * 2] * music.gain * volume;
		output[frame * channels + 1] += music.ring[slot * 2 + 1] * music.gain * volume;
	}
	atomic_store_unsigned(&music.read_frames, read + frame);
}

/* ---------- MCC's sound banks for other units */

#define BANK_CLIP_SECONDS 30           /* a spoken line is a few seconds */

struct ae_mcc_bank
{
	FILE *file;
	struct ae_mcc_music_stream *streams;
	int count;
	/* each stream's language, from the bank's .info (one 280-byte record a stream, in the streams' order, its source
	path at +24: ...\multiplayer_en\..., _de, _fr ...): LANGUAGE_*; none when there is no such file */
	unsigned char *languages;
};

#define INFO_RECORD 280
#define INFO_PATH 24
enum { LANGUAGE_UNKNOWN, LANGUAGE_ENGLISH, LANGUAGE_OTHER };

/* a source path's language: a folder ending _en is English, one ending in another language's code is not */
static unsigned char path_language(char const *path, size_t size)
{
	static char const *const others[] = { "de", "fr", "it", "sp", "pt", "mx", "cht", "chs", "jpn", "kor", "ru", "pl" };
	size_t at, index;

	for (at = 0; at + 1 < size && path[at]; at++)
	{
		size_t length;

		if (path[at] != '_')
			continue;
		for (length = 1; at + length < size && path[at + length] && path[at + length] != '\\' && path[at + length] != '_';
			length++)
			;
		if (at + length >= size || path[at + length] != '\\')
			continue;
		if (length == 3 && !strncmp(path + at + 1, "en", 2))
			return LANGUAGE_ENGLISH;
		for (index = 0; index < sizeof(others) / sizeof(others[0]); index++)
			if (strlen(others[index]) == length - 1 && !strncmp(path + at + 1, others[index], length - 1))
				return LANGUAGE_OTHER;
	}
	return LANGUAGE_UNKNOWN;
}

/* the bank's .info read for its streams' languages; nothing when it is missing or not of the bank */
static void bank_languages(struct ae_mcc_bank *bank, char const *bank_path)
{
	char path[ROOT_SIZE + 168];
	unsigned char record[INFO_RECORD];
	FILE *file;
	int index, english = 0, other = 0;

	snprintf(path, sizeof(path), "%s.info", bank_path);
	if (!(file = fopen(path, "rb")))
		return;
	if (file_size(file) != (uint64_t)bank->count * INFO_RECORD || fseek(file, 0, SEEK_SET) != 0 ||
		!(bank->languages = (unsigned char *)calloc((size_t)bank->count, 1)))
	{
		fclose(file);
		return;
	}
	for (index = 0; index < bank->count && fread(record, 1, sizeof(record), file) == sizeof(record); index++)
	{
		bank->languages[index] = path_language((char const *)record + INFO_PATH, INFO_RECORD - INFO_PATH);
		english += bank->languages[index] == LANGUAGE_ENGLISH;
		other += bank->languages[index] == LANGUAGE_OTHER;
	}
	fclose(file);
	platform_log("mcc sounds: %s.info: %d English lines, %d of other languages", bank_path, english, other);
}

struct ae_mcc_bank *ae_mcc_bank_open(char const *folder, char const *file_name)
{
	char root[ROOT_SIZE], path[ROOT_SIZE + 160];
	unsigned char head[AE_MCC_MUSIC_FSB5_HEADER];
	unsigned char *table = NULL;
	size_t table_size;
	uint64_t size;
	struct ae_mcc_bank *bank;

	if (!folder || !file_name || strchr(folder, '/') || strchr(folder, '\\') || strstr(folder, "..") ||
		!ae_mcc_copy_root(root, sizeof(root)))
		return NULL;
	snprintf(path, sizeof(path), "%s/%s/fmod/pc/%s", root, folder, file_name);
	bank = (struct ae_mcc_bank *)calloc(1, sizeof(*bank));
	if (!bank || !(bank->file = fopen(path, "rb")))
	{
		free(bank);
		return NULL;
	}
	size = file_size(bank->file);
	if (read_at(bank->file, 0, head, sizeof(head)) && (table_size = ae_mcc_music_fsb5_table_size(head, sizeof(head))) &&
		table_size <= size && (table = (unsigned char *)malloc(table_size)) && read_at(bank->file, 0, table, table_size) &&
		(bank->streams = (struct ae_mcc_music_stream *)malloc(sizeof(*bank->streams) * STREAMS_MAXIMUM)))
		bank->count = ae_mcc_music_fsb5_streams(table, table_size, size, bank->streams, STREAMS_MAXIMUM);
	free(table);
	if (bank->count <= 0)
	{
		platform_log("mcc sounds: %s/%s is not a sound bank this reads", folder, file_name);
		ae_mcc_bank_close(bank);
		return NULL;
	}
	platform_log("mcc sounds: %s/%s: %d streams", folder, file_name, bank->count);
	bank_languages(bank, path);
	return bank;
}

void ae_mcc_bank_close(struct ae_mcc_bank *bank)
{
	if (!bank)
		return;
	if (bank->file)
		fclose(bank->file);
	free(bank->streams);
	free(bank->languages);
	free(bank);
}

int ae_mcc_bank_find(struct ae_mcc_bank const *bank, char const *name)
{
	int index;

	int found = -1;

	if (!bank || !name)
		return -1;
	/* (an English line first; one of no known language next; never another language's) */
	for (index = 0; index < bank->count; index++)
	{
		unsigned char language = bank->languages ? bank->languages[index] : LANGUAGE_UNKNOWN;

		if (strcmp(bank->streams[index].name, name) || language == LANGUAGE_OTHER)
			continue;
		if (language == LANGUAGE_ENGLISH)
			return index;
		if (found < 0)
			found = index;
	}
	return found;
}

int ae_mcc_bank_decode(struct ae_mcc_bank *bank, int stream, short **samples, unsigned long *frames,
	unsigned long *channels, unsigned long *rate)
{
	struct ae_mcc_music_stream const *in;
	unsigned char const *setup;
	unsigned char *data = NULL, *ogg = NULL;
	size_t setup_size, capacity, size;
	stb_vorbis *decoder = NULL;
	short *out = NULL;
	int error = 0, got, total = 0;

	*samples = NULL;
	if (!bank || stream < 0 || stream >= bank->count)
		return 0;
	in = bank->streams + stream;
	setup = setup_for(in->setup_crc, &setup_size);
	/* (the bank's rate is file data: one a sound can have, so the length's cap cannot overflow) */
	if (!setup || !in->size || in->size > (8u << 20) || !in->channels || in->channels > 2 || in->rate < 8000 ||
		in->rate > 96000 || in->frames > (uint32_t)BANK_CLIP_SECONDS * in->rate)
		return 0;
	capacity = ae_mcc_music_ogg_capacity((size_t)in->size, setup_size);
	data = (unsigned char *)malloc((size_t)in->size);
	ogg = (unsigned char *)malloc(capacity);
	out = (short *)malloc(sizeof(short) * ((size_t)in->frames + 4096) * in->channels);
	if (data && ogg && out && read_at(bank->file, in->offset, data, (size_t)in->size) &&
		(size = ae_mcc_music_ogg(data, (size_t)in->size, in->channels, in->rate, in->frames, setup, setup_size, ogg,
			capacity)) && size <= INT_MAX)
		decoder = stb_vorbis_open_memory(ogg, (int)size, &error, NULL);
	if (decoder)
	{
		while ((uint32_t)total < in->frames + 4096 &&
			(got = stb_vorbis_get_samples_short_interleaved(decoder, (int)in->channels,
				out + (size_t)total * in->channels, (int)((in->frames + 4096 - (uint32_t)total) * in->channels))) > 0)
			total += got;
		stb_vorbis_close(decoder);
	}
	free(data);
	free(ogg);
	if (total <= 0)
	{
		free(out);
		return 0;
	}
	*samples = out;
	*frames = (unsigned long)total;
	*channels = in->channels;
	*rate = in->rate;
	return 1;
}

void ae_mcc_bank_free(short *samples)
{
	free(samples);
}

struct bank_games
{
	char root[ROOT_SIZE];
	char (*names)[64];
	int maximum, count;
};

static int bank_game(void *context, char const *name, int is_folder, int is_regular)
{
	struct bank_games *games = (struct bank_games *)context;
	char path[ROOT_SIZE + 160];
	FILE *file;
	int index;

	(void)is_regular;
	if (!is_folder || !name[0] || name[0] == '.' || strlen(name) >= 64 || games->count >= games->maximum)
		return 0;
	snprintf(path, sizeof(path), "%s/%s/fmod/pc/english.fsb", games->root, name);
	if (!(file = fopen(path, "rb")))
		return 0;
	fclose(file);
	for (index = games->count; index > 0 && strcmp(games->names[index - 1], name) > 0; index--)
		memcpy(games->names[index], games->names[index - 1], 64);
	memcpy(games->names[index], name, strlen(name) + 1);
	games->count++;
	return 0;
}

int ae_mcc_bank_games(char (*names)[64], int maximum)
{
	struct bank_games games;

	memset(&games, 0, sizeof(games));
	if (!names || maximum <= 0 || !ae_mcc_copy_root(games.root, sizeof(games.root)))
		return 0;
	games.names = names;
	games.maximum = maximum;
	ae_mcc_workshop_list_folder(games.root, bank_game, &games);
	return games.count;
}
