/*
AE_MCC_MUSIC.C

The menus' music (ae_mcc_music.h). audio.menu_music: "game_menus" (each game's menu song and Halo CE's title theme),
"one_song" (audio.menu_music_song over and over), "my_playlist" (audio.menu_music_playlist), "off"; audio.menu_music_shuffle
orders them anew. An old value ("classic", "mcc_shuffle", "mcc_order") is written as the new settings the first time. What
plays is a sequence (ae_menu_music_sequence) the game's thread gives the worker; Halo CE's title theme is the game's own
music, so its turn is the game's thread's: its stop and start through the hooks, for TITLE_SECONDS.

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
#include "ae_menu_music_rules.h"
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
int config_boolean(char const *name);
int config_write(char const *name, char const *value);
int config_write_boolean(char const *name, int value);
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
#define STEM_SIZE 32                   /* a piece's file name, with its 0 */
#define PIECES_MAXIMUM 4096            /* the songs' streams, all banks */
#define RING_FRAMES 262144u            /* about five seconds at 48 kHz (a power of two): a bank on a network share
                                          can be slow to answer when the next piece is read */
#define DECODE_FRAMES 2048
#define FADE_FRAMES 36000.0f           /* 0.75 s */
#define WORKER_IDLE_MS 15
#define FAILURES_MAXIMUM 8             /* songs in a row that do not play: the worker gives up */
#define ROOT_SIZE 1024
#define GAPS_LOG_SECONDS 10

#define RESTART_SECONDS 3
#define TITLE_SECONDS 90               /* the title theme's turn in a sequence: it loops, it has no end */
#define PLAYLIST_TEXT 8200
#define NOW_NEXT 3
#define FAKE_DEFAULT "halo3:main_menu_delta/delta_menu=370,halo3:misc/halo3_trench/track1=372," \
	"halo3:misc/perilous_full/track1=241,halo3:misc/t&r_piano/track1=177,halo3:end_credits=357," \
	"halo3:numbers/9m_full/track1=218,halo3odst:atlas/menu/track1=362,halo3odst:atlas/misc/originalrain_nl/track1=189," \
	"halo3odst:atlas/misc/cp_driving_plus/track1=213,haloreach:reach_menu/track1=743," \
	"haloreach:heroic/10_kurz02_nobrass/track1=140,haloreach:poignant/cv/track1=358"

enum
{
	CATEGORY_STOCK,                    /* the game's own title music plays */
	CATEGORY_OFF,
	CATEGORY_SONGS                     /* this unit's */
};

enum
{
	CATALOG_NOT_ASKED,
	CATALOG_LOADING,
	CATALOG_READY,
	CATALOG_NONE                       /* no installation, or no songs in it */
};

struct piece
{
	unsigned char bank;
	unsigned channels;
	uint32_t frames, setup_crc;
	uint64_t offset, size;
	char name[AE_MCC_MUSIC_NAME];
};

/* a song of the catalog: catalog entry n is song n - 1 (entry 0 is Halo CE's title theme, the game's own) */
struct song
{
	unsigned first, count;             /* pieces (count 0: a stand-in song of the test table, silence for its length) */
	uint32_t frames;
	char id[AE_MUSIC_ID];
};

/* one song being decoded: the menus' music, or a preview */
struct player
{
	stb_vorbis *decoder;
	unsigned char *ogg;
	unsigned piece_index;
	int entry;                         /* the catalog entry (1 and up); -1 none */
	unsigned long decoded;             /* frames of the song decoded so far */
};

static struct
{
	/* the game's thread */
	int owner;                         /* the menus' music playing now is this (or OFF's silence) */
	int worker_started;
	int config_read;                   /* the settings read once (config_read_at holds) */
	unsigned long config_read_at;
	int mode, shuffle;                 /* audio.menu_music, audio.menu_music_shuffle (an old value's mapping) */
	char song[AE_MUSIC_ID];
	char playlist_text[PLAYLIST_TEXT];
	int legacy;                        /* audio.menu_music holds an old value (classic, mcc_shuffle, mcc_order) */
	struct ae_menu_music_legacy legacy_to;
	int category;
	int published, published_mode, published_shuffle, published_state, seen_state;
	char published_song[AE_MUSIC_ID], published_playlist[PLAYLIST_TEXT];
	int title_active;                  /* the game's own title music plays, this unit's turn at it */
	time_t title_started, title_ended;
	time_t restart_until;              /* a stop and start of the menus' music that the game's fade refused: asked again until then */
	int preview_wanted;
	int names_set;                     /* the catalog's ids are with the song names (ae_menu_music_catalog_set) */
	int preview_pause;                 /* what the preview paused (AE_MCC_MUSIC_PAUSE_*) */
	/* shared: atomics */
	int playing;                       /* the menus want the music */
	int volume_milli;                  /* audio.music_volume x 1000 */
	int dead;                          /* the worker gave up (no banks, no songs) */
	int catalog_state;                 /* CATALOG_*: the catalog below is read after it says READY */
	int dev_source;                    /* DEV MODE's: 0 the installation, 1 stand-in songs, 2 none */
	unsigned reload_count;             /* the catalog to be read again (LOOK AGAIN, DEV MODE's source) */
	unsigned write_frames, read_frames;
	unsigned flush_frames;             /* the worker's: what was written before this is a song no longer wanted */
	unsigned gaps;                     /* the mixer's: times the ring ran dry while a song played */
	unsigned wish_lock, wish_generation; /* the game's thread's new sequence (a seqlock) */
	int wish_count, wish_shuffle, wish_order[SONGS_MAXIMUM + 1];
	unsigned next_count;               /* X: next song */
	unsigned preview_generation;
	int preview_request;               /* the entry to preview, -1 none */
	int preview_on;                    /* a preview plays (the mixer plays it though the menus' music is the game's) */
	unsigned title_done_count;         /* the game's thread: the title theme's turn is over */
	int title_turn;                    /* the worker: the sequence is at the title theme */
	int now_entry, now_index;          /* the worker's: the menus' song playing, its place in the sequence */
	unsigned now_decoded, preview_decoded;
	int preview_entry;
	unsigned shown_lock;               /* the worker's order, for the game's thread to show (a seqlock) */
	int shown_count, shown_order[SONGS_MAXIMUM + 1];
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
	struct ae_menu_music_entry entries[SONGS_MAXIMUM + 1];   /* the catalog: the title theme, then the songs */
	int entry_count;
	int order[SONGS_MAXIMUM + 1];
	int order_count, order_index, order_shuffle;
	struct player main_player, preview_player;
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

static char const *test_songs(void)
{
	char const *value = getenv("HALO_AE_TEST_MUSIC_SONGS");

	if (atomic_load_int(&music.dev_source) == 1)
		return FAKE_DEFAULT;
	if (!value || !value[0] || !strcmp(value, "0"))
		return NULL;
	return strcmp(value, "1") ? value : FAKE_DEFAULT;
}

/* an installation to read, or a test table of stand-in songs */
static int source_present(void)
{
	char root[ROOT_SIZE];

	if (atomic_load_int(&music.dev_source) == 2)
		return 0;
	return test_songs() != NULL || ae_mcc_copy_root(root, sizeof(root));
}

static void settings_read(void)
{
	double volume;
	char const *value;

	if (music.config_read && music.config_read_at == config_changes())
		return;
	music.config_read = 1;
	music.config_read_at = config_changes();
	value = config_string("audio.menu_music");
	music.legacy = ae_menu_music_legacy(value, &music.legacy_to);
	if (music.legacy)
	{
		music.mode = music.legacy_to.mode;
		music.shuffle = music.legacy_to.shuffle;
		snprintf(music.song, sizeof(music.song), "%s", AE_MUSIC_TITLE_ID);
	}
	else
	{
		music.mode = ae_menu_music_mode_parse(value);
		if (music.mode < 0)
			music.mode = AE_MUSIC_MODE_GAME_MENUS;
		music.shuffle = config_boolean("audio.menu_music_shuffle");
		value = config_string("audio.menu_music_song");
		snprintf(music.song, sizeof(music.song), "%s", ae_menu_music_id_valid(value) ? value : AE_MUSIC_TITLE_ID);
	}
	snprintf(music.playlist_text, sizeof(music.playlist_text), "%s", config_string("audio.menu_music_playlist"));
	volume = config_real("audio.music_volume");
	if (volume < 0.0)
		volume = 0.0;
	if (volume > 1.0)
		volume = 1.0;
	atomic_store_int(&music.volume_milli, (int)(volume * 1000.0 + 0.5));
}

/* who plays the menus' music with these settings */
static int category_now(void)
{
	if (music.mode == AE_MUSIC_MODE_OFF)
		return CATEGORY_OFF;
	if ((music.mode == AE_MUSIC_MODE_ONE_SONG && !strcmp(music.song, AE_MUSIC_TITLE_ID)) || atomic_load_int(&music.dead))
		return CATEGORY_STOCK;
	return CATEGORY_SONGS;
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

/* a song added to the catalog, unless its id is there already; 1 when added */
static int song_add_checked(char const *id, unsigned first, unsigned count, uint32_t frames)
{
	int index;

	if (music.song_count >= SONGS_MAXIMUM || !ae_menu_music_id_valid(id))
		return 0;
	for (index = 0; index < music.song_count; index++)
		if (!strcmp(music.songs[index].id, id))
			return 0;
	music.songs[music.song_count].first = first;
	music.songs[music.song_count].count = count;
	music.songs[music.song_count].frames = frames;
	snprintf(music.songs[music.song_count].id, sizeof(music.songs[music.song_count].id), "%s", id);
	music.song_count++;
	return 1;
}

/* one bank's songs added, less those already listed */
static void catalog_bank(int bank, struct ae_mcc_music_stream *streams)
{
	static struct ae_mcc_music_song songs[SONGS_MAXIMUM];
	unsigned char head[AE_MCC_MUSIC_FSB5_HEADER];
	unsigned char *table;
	size_t table_size;
	uint64_t size;
	int count, song_count, index, added = 0, first_song = music.song_count;
	char path[ROOT_SIZE + 64];
	char (*keys)[AE_MUSIC_ID] = NULL;
	char (*stems)[STEM_SIZE] = NULL;   /* each stream's file name (its permutation) */
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
	/* (the bank's .info: only the streams whose source is a song, not a level's ambience or a stem; each stream's
	song key is what makes a song's id, so a bank with none has no songs here) */
	{
		char info_path[ROOT_SIZE + 80];
		FILE *info;

		snprintf(info_path, sizeof(info_path), "%s.info", path);
		keys = (char (*)[AE_MUSIC_ID])calloc((size_t)(count > 0 ? count : 1), AE_MUSIC_ID);
		stems = (char (*)[STEM_SIZE])calloc((size_t)(count > 0 ? count : 1), STEM_SIZE);
		if (keys && stems && (info = fopen(info_path, "rb")) != NULL)
		{
			unsigned char record[280];
			int stream, kept = 0;

			if (file_size(info) == (uint64_t)count * sizeof(record) && !fseek(info, 0, SEEK_SET))
			{
				for (stream = 0; stream < count && fread(record, 1, sizeof(record), info) == sizeof(record); stream++)
				{
					char key[AE_MUSIC_ID];

					if (ae_mcc_music_path_is_music((char const *)record + 24, sizeof(record) - 24) &&
						ae_menu_music_song_key((char const *)record + 24, sizeof(record) - 24, key, sizeof(key)) &&
						ae_menu_music_key_is_song(key))
					{
						snprintf(keys[stream], AE_MUSIC_ID, "%s", key);
						if (!ae_menu_music_piece_stem((char const *)record + 24, sizeof(record) - 24, stems[stream],
							STEM_SIZE))
							stems[stream][0] = 0;
						kept++;
					}
					else
						streams[stream].name[0] = 0;
				}
				platform_log("mcc music: %s: %d of %d streams are music (its .info)", music.bank_names[bank], kept, count);
			}
			else
				kept = -1;
			if (kept < 0)
			{
				free(keys);
				free(stems);
				keys = stems = NULL;
			}
			fclose(info);
		}
		else
		{
			free(keys);
			free(stems);
			keys = NULL;
			stems = NULL;
		}
	}
	if (!keys)
	{
		platform_log("mcc music: %s has no .info this reads: no songs from it", music.bank_names[bank]);
		fclose(file);
		return;
	}
	song_count = ae_mcc_music_songs(streams, count, known_setups, 2, songs, SONGS_MAXIMUM);
	for (index = 0; index < song_count; index++)
	{
		struct ae_mcc_music_song const *song = songs + index;
		int other, repeated = 0, key_at = -1;
		unsigned piece;
		char id[AE_MUSIC_ID];

		/* (the song's id: its first piece with a key) */
		for (piece = 0; piece < song->count && key_at < 0; piece++)
			if (keys[song->first + piece][0])
				key_at = (int)(song->first + piece);
		if (key_at < 0 || !ae_menu_music_make_id(music.bank_names[bank], keys[key_at], id, sizeof(id)))
			continue;
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
		if (repeated || music.piece_count + (int)song->count > PIECES_MAXIMUM ||
			!song_add_checked(id, (unsigned)music.piece_count, song->count, song->frames))
			continue;
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
		added++;
		/* (a menu sound listed by its pieces: each piece is a song of its own too, its id the sound's and its permutation) */
		if (ae_menu_music_lists_pieces(id))
		{
			/* (the whole sound's first piece, taken once: each piece song added below becomes the newest song) */
			unsigned base = music.songs[music.song_count - 1].first;

			for (piece = 0; piece < song->count; piece++)
			{
				char piece_id[AE_MUSIC_ID];
				struct piece const *whole = music.pieces + base + piece;

				if (stems[song->first + piece][0] &&
					ae_menu_music_piece_id(id, stems[song->first + piece], piece_id, sizeof(piece_id)))
					song_add_checked(piece_id, base + piece, 1, whole->frames);
			}
		}
	}
	free(keys);
	free(stems);
	/* (the game's own menu song first among its songs) */
	for (index = first_song + 1; index < music.song_count; index++)
		if (ae_menu_music_is_menu_song(music.songs[index].id))
		{
			struct song menu = music.songs[index];

			memmove(music.songs + first_song + 1, music.songs + first_song, (size_t)(index - first_song) * sizeof(menu));
			music.songs[first_song] = menu;
			break;
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

/* the test table's stand-in songs: "<id>=<seconds>,..." */
static void catalog_test_songs(char const *table)
{
	while (*table)
	{
		char const *end = strchr(table, ','), *equals;
		size_t span = end ? (size_t)(end - table) : strlen(table);
		char id[AE_MUSIC_ID];

		equals = (char const *)memchr(table, '=', span);
		if (equals && (size_t)(equals - table) < sizeof(id))
		{
			memcpy(id, table, (size_t)(equals - table));
			id[equals - table] = 0;
			song_add_checked(id, 0, 0, (uint32_t)(atoi(equals + 1) * AE_MCC_MUSIC_RATE));
		}
		table = end ? end + 1 : table + span;
	}
}

/* the catalog of entries the rules and the screens use: the title theme, then the songs */
static void entries_make(void)
{
	int index;

	snprintf(music.entries[0].id, sizeof(music.entries[0].id), "%s", AE_MUSIC_TITLE_ID);
	music.entries[0].menu = 1;
	for (index = 0; index < music.song_count; index++)
	{
		snprintf(music.entries[index + 1].id, sizeof(music.entries[index + 1].id), "%s", music.songs[index].id);
		music.entries[index + 1].menu = ae_menu_music_is_menu_song(music.songs[index].id);
	}
	music.entry_count = music.song_count + 1;
}

static int catalog(void)
{
	struct ae_mcc_music_stream *streams;
	int bank;
	char const *test = test_songs();

	if (atomic_load_int(&music.dev_source) == 2)
	{
		platform_log("mcc music: no MCC installation (DEV MODE's source)");
		return 0;
	}
	if (test)
	{
		catalog_test_songs(test);
		platform_log("mcc music: %d stand-in songs (HALO_AE_TEST_MUSIC_SONGS)", music.song_count);
		entries_make();
		return music.song_count > 0;
	}
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
	entries_make();
	return music.song_count > 0;
}

static struct song const *song_of(int entry)
{
	return music.songs + entry - 1;
}

static void player_close(struct player *player)
{
	if (player->decoder)
		stb_vorbis_close(player->decoder);
	player->decoder = NULL;
	free(player->ogg);
	player->ogg = NULL;
}

/* the decoder opened on the song's next piece; 0 when it cannot be */
static int piece_open(struct player *player)
{
	struct song const *song = song_of(player->entry);
	struct piece const *piece;
	unsigned char *data;
	unsigned char const *setup;
	size_t setup_size, capacity, size;
	int error = 0;

	if (player->piece_index >= song->count)
		return 0;
	piece = music.pieces + song->first + player->piece_index;
	setup = setup_for(piece->setup_crc, &setup_size);
	if (!setup || !music.banks[piece->bank] || piece->size > (64u << 20))
		return 0;
	data = (unsigned char *)malloc((size_t)piece->size);
	capacity = ae_mcc_music_ogg_capacity((size_t)piece->size, setup_size);
	player->ogg = (unsigned char *)malloc(capacity);
	if (!data || !player->ogg || !read_at(music.banks[piece->bank], piece->offset, data, (size_t)piece->size))
	{
		free(data);
		player_close(player);
		return 0;
	}
	size = ae_mcc_music_ogg(data, (size_t)piece->size, piece->channels, AE_MCC_MUSIC_RATE, piece->frames, setup,
		setup_size, player->ogg, capacity);
	free(data);
	if (size && size <= INT_MAX)
		player->decoder = stb_vorbis_open_memory(player->ogg, (int)size, &error, NULL);
	if (!player->decoder)
	{
		platform_log("mcc music: %s (%s) does not decode (%d)", piece->name, music.bank_names[piece->bank], error);
		player_close(player);
		return 0;
	}
	return 1;
}

/* frames decoded into the ring, at most room; *finished when the song has no more; -1 when the song fails */
static int decode_some(struct player *player, unsigned room, int *finished)
{
	float frames[DECODE_FRAMES * 2];
	struct song const *song = song_of(player->entry);
	unsigned write = atomic_load_unsigned(&music.write_frames), index;
	int got;

	*finished = 0;
	if (room > DECODE_FRAMES)
		room = DECODE_FRAMES;
	if (!song->count)
	{
		/* (a stand-in song: silence for its length) */
		unsigned long left = song->frames > player->decoded ? song->frames - player->decoded : 0;

		got = (int)(left < room ? left : room);
		for (index = 0; index < (unsigned)got; index++)
		{
			unsigned slot = (write + index) & (RING_FRAMES - 1);

			music.ring[slot * 2] = 0.0f;
			music.ring[slot * 2 + 1] = 0.0f;
		}
		atomic_store_unsigned(&music.write_frames, write + (unsigned)got);
		player->decoded += (unsigned long)got;
		*finished = player->decoded >= song->frames;
		return got;
	}
	if (!player->decoder && !piece_open(player))
		return -1;
	got = stb_vorbis_get_samples_float_interleaved(player->decoder, 2, frames, (int)room * 2);
	if (got <= 0)
	{
		player_close(player);
		player->piece_index++;
		if (player->piece_index >= song->count)
			*finished = 1;
		return 0;
	}
	for (index = 0; index < (unsigned)got; index++)
	{
		unsigned slot = (write + index) & (RING_FRAMES - 1);

		music.ring[slot * 2] = frames[index * 2];
		music.ring[slot * 2 + 1] = frames[index * 2 + 1];
	}
	atomic_store_unsigned(&music.write_frames, write + (unsigned)got);
	player->decoded += (unsigned long)got;
	return got;
}

static void ring_flush(void)
{
	atomic_store_unsigned(&music.flush_frames, atomic_load_unsigned(&music.write_frames));
}

/* the worker's order shown to the game's thread */
static void shown_publish(void)
{
	unsigned lock = music.shown_lock;
	int index;

	atomic_store_unsigned(&music.shown_lock, lock + 1);
	__atomic_thread_fence(__ATOMIC_SEQ_CST);
	music.shown_count = music.order_count;
	for (index = 0; index < music.order_count; index++)
		music.shown_order[index] = music.order[index];
	atomic_store_unsigned(&music.shown_lock, lock + 2);
}

/* the menus' music's player on the song now at order_index */
static void main_begin(void)
{
	struct player *player = &music.main_player;

	player_close(player);
	player->piece_index = 0;
	player->decoded = 0;
	player->entry = music.order_count > 0 ? music.order[music.order_index] : -1;
	atomic_store_int(&music.now_entry, player->entry);
	atomic_store_int(&music.now_index, music.order_index);
	atomic_store_int(&music.title_turn, player->entry == 0);
	if (player->entry >= 0)
		platform_log("ae menu music: now %s", music.entries[player->entry].id);
}

static void main_advance(void)
{
	int last = music.order_count > 0 ? music.order[music.order_index] : -1;

	if (music.order_count > 0 && ++music.order_index >= music.order_count)
	{
		music.order_index = 0;
		/* (a new order each pass, and never the song that just played first) */
		if (music.order_shuffle && music.order_count > 1)
		{
			ae_menu_music_reshuffle(music.order, music.order_count, (uint32_t)time(NULL) ^ 0xC3A5u, last);
			shown_publish();
		}
	}
	main_begin();
}

/* the game's thread's new sequence taken up: the next song is its first, at once */
static void wish_adopt(void)
{
	for (;;)
	{
		unsigned before = atomic_load_unsigned(&music.wish_lock);
		int count, index;

		if (before & 1)
		{
			ae_worker_sleep(1);
			continue;
		}
		count = music.wish_count;
		if (count < 0 || count > SONGS_MAXIMUM + 1)
			count = 0;
		for (index = 0; index < count; index++)
			music.order[index] = music.wish_order[index];
		music.order_shuffle = music.wish_shuffle;
		__atomic_thread_fence(__ATOMIC_ACQUIRE);
		if (atomic_load_unsigned(&music.wish_lock) == before)
		{
			music.order_count = count;
			break;
		}
	}
	/* (an entry past the catalog is dropped) */
	{
		int index, kept = 0;

		for (index = 0; index < music.order_count; index++)
			if (music.order[index] >= 0 && music.order[index] < music.entry_count)
				music.order[kept++] = music.order[index];
		music.order_count = kept;
	}
	music.order_index = 0;
	shown_publish();
	main_begin();
	ring_flush();
}

static void preview_begin(int entry)
{
	player_close(&music.preview_player);
	music.preview_player.piece_index = 0;
	music.preview_player.decoded = 0;
	music.preview_player.entry = entry >= 1 && entry < music.entry_count ? entry : -1;
	atomic_store_int(&music.preview_entry, music.preview_player.entry);
	atomic_store_int(&music.preview_on, music.preview_player.entry >= 1);
	if (music.preview_player.entry >= 1)
		platform_log("ae menu music: preview %s", music.entries[music.preview_player.entry].id);
	ring_flush();
}

/* the catalog made empty (the worker's, before it is read again) */
static void catalog_reset(void)
{
	int bank;

	music.song_count = 0;
	music.piece_count = 0;
	music.bank_count = 0;
	music.entry_count = 0;
	free(music.pieces);
	music.pieces = NULL;
	for (bank = 0; bank < BANKS; bank++)
		if (music.banks[bank])
		{
			fclose(music.banks[bank]);
			music.banks[bank] = NULL;
		}
}

/* the catalog read; the state it ends in */
static void catalog_load(void)
{
	int ok;

	atomic_store_int(&music.catalog_state, CATALOG_LOADING);
	player_close(&music.main_player);
	player_close(&music.preview_player);
	music.main_player.entry = -1;
	music.preview_player.entry = -1;
	music.order_count = 0;
	atomic_store_int(&music.now_entry, -1);
	atomic_store_int(&music.preview_entry, -1);
	atomic_store_int(&music.preview_on, 0);
	atomic_store_int(&music.title_turn, 0);
	catalog_reset();
	ok = catalog();
	atomic_store_int(&music.dead, !ok);
	atomic_store_int(&music.catalog_state, ok ? CATALOG_READY : CATALOG_NONE);
	ring_flush();
}

static void *worker(void *argument)
{
	int failures = 0;
	unsigned logged_gaps = 0, seen_generation, seen_next, seen_preview, seen_title_done;
	time_t logged_at = 0;

	unsigned seen_reload = atomic_load_unsigned(&music.reload_count);

	(void)argument;
	catalog_load();
	seen_generation = atomic_load_unsigned(&music.wish_generation);
	seen_next = atomic_load_unsigned(&music.next_count);
	seen_preview = atomic_load_unsigned(&music.preview_generation);
	seen_title_done = atomic_load_unsigned(&music.title_done_count);
	for (;;)
	{
		unsigned room = RING_FRAMES - (atomic_load_unsigned(&music.write_frames) -
			atomic_load_unsigned(&music.read_frames));
		struct player *player;
		int got, finished = 0;

		/* (the catalog to be read again: LOOK AGAIN, DEV MODE's source) */
		if (atomic_load_unsigned(&music.reload_count) != seen_reload)
		{
			seen_reload = atomic_load_unsigned(&music.reload_count);
			catalog_load();
			seen_generation = atomic_load_unsigned(&music.wish_generation);
			continue;
		}
		/* (nothing to play from: waits for the catalog to be read again) */
		if (atomic_load_int(&music.dead))
		{
			ae_worker_sleep(WORKER_IDLE_MS);
			continue;
		}
		/* (a new sequence, X, the title theme's turn over, a preview asked or stopped) */
		if (atomic_load_unsigned(&music.wish_generation) != seen_generation)
		{
			seen_generation = atomic_load_unsigned(&music.wish_generation);
			wish_adopt();
		}
		if (atomic_load_unsigned(&music.next_count) != seen_next)
		{
			seen_next = atomic_load_unsigned(&music.next_count);
			main_advance();
			ring_flush();
		}
		if (atomic_load_unsigned(&music.title_done_count) != seen_title_done)
		{
			seen_title_done = atomic_load_unsigned(&music.title_done_count);
			if (music.main_player.entry == 0)
				main_advance();
		}
		if (atomic_load_unsigned(&music.preview_generation) != seen_preview)
		{
			seen_preview = atomic_load_unsigned(&music.preview_generation);
			preview_begin(atomic_load_int(&music.preview_request));
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
		player = music.preview_player.entry >= 1 ? &music.preview_player : &music.main_player;
		if (room < DECODE_FRAMES || player->entry < 1)
		{
			/* (nothing to play: no sequence yet, OFF, or the title theme's turn, the game's own) */
			ae_worker_sleep(WORKER_IDLE_MS);
			continue;
		}
		got = decode_some(player, room, &finished);
		if (player == &music.preview_player)
			atomic_store_unsigned(&music.preview_decoded, (unsigned)player->decoded);
		else
			atomic_store_unsigned(&music.now_decoded, (unsigned)player->decoded);
		if (got < 0)
		{
			if (player == &music.preview_player)
			{
				preview_begin(-1);
				continue;
			}
			if (++failures >= FAILURES_MAXIMUM)
			{
				platform_log("mcc music: %d songs in a row do not play; the menus' music stops", failures);
				atomic_store_int(&music.dead, 1);
				player_close(&music.main_player);
				failures = 0;
				continue;
			}
			main_advance();
		}
		else if (got > 0)
			failures = 0;
		if (finished)
		{
			if (player == &music.preview_player)
				preview_begin(-1);
			else
				main_advance();
		}
	}
	return NULL;
}

/* ---------- the game's thread */

static void worker_start(void)
{
	if (music.worker_started || atomic_load_int(&music.dead))
		return;
	atomic_store_int(&music.catalog_state, CATALOG_LOADING);
	music.worker_started = ae_worker_start(worker, NULL);
	if (!music.worker_started)
		atomic_store_int(&music.catalog_state, CATALOG_NONE);
}

int ae_mcc_music_menu_start(void)
{
	settings_read();
	music.owner = 0;
	music.category = category_now();
	if (music.title_active)
		return 0;
	if (music.category == CATEGORY_OFF)
	{
		music.owner = 1;
		return 1;
	}
	/* (the title theme only, no installation, or a worker that found nothing: the game's own music) */
	if (music.category == CATEGORY_STOCK)
		return 0;
	if (!music.worker_started)
	{
		if (!source_present())
			return 0;
		worker_start();
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
	/* (a preview ends with the menus: the game's own music is not started again by it) */
	music.preview_pause = AE_MCC_MUSIC_PAUSE_NONE;
	ae_mcc_music_preview(-1);
	return owner;
}

/* the sequence the settings ask for, built and given to the worker once the catalog is read */
static void sequence_publish(void)
{
	static struct ae_menu_music_playlist list;
	int order[SONGS_MAXIMUM + 1], effective = music.mode, missing = 0, count, index;
	int state = atomic_load_int(&music.catalog_state);

	if (state != CATALOG_READY)
		return;
	if (music.published && music.published_mode == music.mode && music.published_shuffle == music.shuffle &&
		music.published_state == state && !strcmp(music.published_song, music.song) &&
		!strcmp(music.published_playlist, music.playlist_text))
		return;
	ae_menu_music_playlist_parse(music.playlist_text, &list);
	count = ae_menu_music_sequence(music.mode, music.shuffle, music.entries, music.entry_count, music.song, &list,
		(uint32_t)time(NULL) ^ 0x5A17u, atomic_load_int(&music.now_entry), order, SONGS_MAXIMUM + 1, &effective,
		&missing);
	platform_log("ae menu music: mode %s shuffle %d songs %d%s", ae_menu_music_mode_label(effective), music.shuffle, count,
		effective != music.mode ? " (the saved choice has no songs: GAME MENUS)" : "");
	if (music.mode == AE_MUSIC_MODE_MY_PLAYLIST)
		platform_log("ae menu music: playlist %d (%d not found)", list.count, missing);
	atomic_store_unsigned(&music.wish_lock, music.wish_lock + 1);
	__atomic_thread_fence(__ATOMIC_SEQ_CST);
	music.wish_count = count;
	music.wish_shuffle = music.shuffle;
	for (index = 0; index < count; index++)
		music.wish_order[index] = order[index];
	atomic_store_unsigned(&music.wish_lock, music.wish_lock + 1);
	atomic_store_unsigned(&music.wish_generation, music.wish_generation + 1);
	music.published = 1;
	music.published_mode = music.mode;
	music.published_shuffle = music.shuffle;
	music.published_state = state;
	snprintf(music.published_song, sizeof(music.published_song), "%s", music.song);
	snprintf(music.published_playlist, sizeof(music.published_playlist), "%s", music.playlist_text);
}

/* an old audio.menu_music value written as the new settings: classic as ONE SONG (the title theme); the MCC ones as MY
PLAYLIST holding every song in bank order (shuffled or not as before), once the catalog is read (with none: classic's) */
static void legacy_migrate(void)
{
	static struct ae_menu_music_playlist list;
	int state = atomic_load_int(&music.catalog_state), index;
	char text[PLAYLIST_TEXT];

	if (!music.legacy)
		return;
	if (state == CATALOG_NOT_ASKED && !source_present())
		state = CATALOG_NONE;
	if (music.legacy_to.all_songs)
	{
		if (state == CATALOG_NOT_ASKED || state == CATALOG_LOADING)
			return;
		if (state == CATALOG_READY)
		{
			list.count = 0;
			for (index = 0; index < music.song_count && list.count < AE_MUSIC_PLAYLIST_MAXIMUM; index++)
				ae_menu_music_playlist_add(&list, music.songs[index].id);
			while (list.count > 0 && !ae_menu_music_playlist_serialize(&list, text, sizeof(text)))
				list.count--;
			config_write("audio.menu_music_playlist", text);
			config_write("audio.menu_music", ae_menu_music_mode_value(AE_MUSIC_MODE_MY_PLAYLIST));
			config_write_boolean("audio.menu_music_shuffle", music.legacy_to.shuffle);
			platform_log("ae menu music: an old setting moved to MY PLAYLIST with %d songs (shuffle %d)", list.count,
				music.legacy_to.shuffle);
			return;
		}
		/* (no installation: the title music is what they heard) */
	}
	config_write("audio.menu_music_song", AE_MUSIC_TITLE_ID);
	config_write("audio.menu_music", ae_menu_music_mode_value(AE_MUSIC_MODE_ONE_SONG));
	platform_log("ae menu music: an old setting moved to ONE SONG, the title theme");
}

/* the title theme's turn in the sequence: the game's own music plays (its stop and start through the hooks), then the
sequence goes on */
static void title_turn_update(void)
{
	int active = ui_main_menu_music_active();

	if (music.title_active)
	{
		if (!active)
			music.title_active = 0;       /* (the game stopped it: a map loads) */
		else if (!atomic_load_int(&music.title_turn) || time(NULL) - music.title_started >= TITLE_SECONDS)
		{
			int over = atomic_load_int(&music.title_turn);

			ui_stop_main_menu_music();
			music.title_active = 0;
			music.title_ended = time(NULL);
			music.restart_until = music.title_ended + RESTART_SECONDS;
			if (over)
				atomic_store_unsigned(&music.title_done_count, music.title_done_count + 1);
			ui_start_main_menu_music();
		}
		return;
	}
	if (music.owner && active && atomic_load_int(&music.title_turn) && time(NULL) - music.title_ended >= 2)
	{
		music.title_active = 1;
		music.title_started = time(NULL);
		music.restart_until = music.title_started + RESTART_SECONDS;
		ui_stop_main_menu_music();
		ui_start_main_menu_music();
	}
}

void ae_mcc_music_update(void)
{
	int state, category_before = music.category, mode_before = music.mode;

	/* (the announcer's next line, each frame: the HUD's draw, its other caller, stops at a game's end) */
	ae_announcer_update();
	/* (the worker found no song after the menus' music was given to it: the title music instead, once) */
	if (music.owner && atomic_load_int(&music.dead) && music.mode != AE_MUSIC_MODE_OFF && ui_main_menu_music_active())
	{
		ui_stop_main_menu_music();
		ui_start_main_menu_music();
	}
	state = atomic_load_int(&music.catalog_state);
	/* (the game's fade refused a start of the menus' music: asked again for a moment) */
	if (music.restart_until)
	{
		if (ui_main_menu_music_active() || time(NULL) > music.restart_until)
			music.restart_until = 0;
		else
			ui_start_main_menu_music();
	}
	if (music.owner || music.title_active)
		title_turn_update();
	/* (the songs' layer names number from the catalog: given to the names once it is read, taken back when it is not) */
	if (state == CATALOG_READY && !music.names_set)
	{
		char const **ids = (char const **)malloc((size_t)(music.entry_count + 1) * sizeof(*ids));
		int index;

		if (ids)
		{
			for (index = 0; index < music.entry_count; index++)
				ids[index] = music.entries[index].id;
			ae_menu_music_catalog_set(ids, music.entry_count);
			free(ids);
			music.names_set = 1;
		}
	}
	else if (state != CATALOG_READY && music.names_set)
	{
		ae_menu_music_catalog_set(NULL, 0);
		music.names_set = 0;
	}
	if (music.config_read && music.config_read_at == config_changes() && music.seen_state == state && !music.legacy)
		return;
	music.seen_state = state;
	settings_read();
	/* (an old value: written as the new settings, taken up at once) */
	if (music.legacy)
	{
		if (music.legacy_to.all_songs && !music.worker_started && source_present())
			worker_start();
		legacy_migrate();
		settings_read();
	}
	sequence_publish();
	music.category = category_now();
	/* (a changed setting while the menus' music plays: who plays it changes, or the sequence did: the worker took it) */
	/* (and the game's own music playing where the songs are wanted, the source having come since: DEV MODE's) */
	if ((music.category != category_before || (music.mode != mode_before && music.category != CATEGORY_SONGS) ||
		(music.category == CATEGORY_SONGS && !music.owner && !music.title_active &&
		atomic_load_int(&music.catalog_state) == CATALOG_READY)) && ui_main_menu_music_active())
	{
		/* (the game's own title music handed back first: the stop below is its) */
		music.title_active = 0;
		music.restart_until = time(NULL) + RESTART_SECONDS;
		ui_stop_main_menu_music();
		ui_start_main_menu_music();
	}
}

/* ---------- for the screens */

void ae_mcc_music_catalog_request(void)
{
	if (music.worker_started || atomic_load_int(&music.dead))
		return;
	if (!source_present())
	{
		atomic_store_int(&music.catalog_state, CATALOG_NONE);
		return;
	}
	worker_start();
}

int ae_mcc_music_catalog_state(void)
{
	int state = atomic_load_int(&music.catalog_state);

	return state == CATALOG_READY ? AE_MCC_MUSIC_CATALOG_READY : state == CATALOG_LOADING ?
		AE_MCC_MUSIC_CATALOG_LOADING : state == CATALOG_NONE ? AE_MCC_MUSIC_CATALOG_NONE : AE_MCC_MUSIC_CATALOG_NOT_ASKED;
}

void ae_mcc_music_catalog_retry(void)
{
	/* (LOOK AGAIN: the detection run once more, then the banks read again) */
	if (atomic_load_int(&music.catalog_state) != CATALOG_NONE || !source_present())
		return;
	music.published = 0;
	if (!music.worker_started)
	{
		worker_start();
		return;
	}
	atomic_store_int(&music.catalog_state, CATALOG_LOADING);
	atomic_store_unsigned(&music.reload_count, music.reload_count + 1);
}

/* DEV MODE's source of songs (menu music <source>): 0 the installation, 1 stand-in songs, 2 none; the catalog read again */
void ae_mcc_music_dev_source(int which)
{
	atomic_store_int(&music.dev_source, which);
	music.published = 0;
	if (!music.worker_started)
	{
		ae_mcc_music_catalog_request();
		return;
	}
	atomic_store_int(&music.catalog_state, CATALOG_LOADING);
	atomic_store_unsigned(&music.reload_count, music.reload_count + 1);
}

int ae_mcc_music_catalog_count(void)
{
	return atomic_load_int(&music.catalog_state) == CATALOG_READY ? music.entry_count : 1;
}

char const *ae_mcc_music_catalog_id(int index)
{
	if (index <= 0 || atomic_load_int(&music.catalog_state) != CATALOG_READY || index >= music.entry_count)
		return AE_MUSIC_TITLE_ID;
	return music.entries[index].id;
}

long ae_mcc_music_catalog_seconds(int index)
{
	if (index <= 0 || atomic_load_int(&music.catalog_state) != CATALOG_READY || index >= music.entry_count)
		return 0;
	return (long)(song_of(index)->frames / AE_MCC_MUSIC_RATE);
}

int ae_mcc_music_catalog_find(char const *id)
{
	int index;

	if (!id || atomic_load_int(&music.catalog_state) != CATALOG_READY)
		return !strcmp(id ? id : "", AE_MUSIC_TITLE_ID) ? 0 : -1;
	for (index = 0; index < music.entry_count; index++)
		if (!strcmp(music.entries[index].id, id))
			return index;
	return -1;
}

void ae_mcc_music_now(struct ae_mcc_music_now *now)
{
	unsigned before;
	int index, at;

	memset(now, 0, sizeof(*now));
	now->entry = -1;
	now->preview = -1;
	if (atomic_load_int(&music.catalog_state) != CATALOG_READY)
		return;
	now->preview = atomic_load_int(&music.preview_entry);
	if (now->preview >= 1)
	{
		long position = (long)atomic_load_unsigned(&music.preview_decoded) -
			(long)(atomic_load_unsigned(&music.write_frames) - atomic_load_unsigned(&music.read_frames));

		now->preview_position = position > 0 ? position / AE_MCC_MUSIC_RATE : 0;
	}
	if (!music.owner && !music.title_active)
		return;
	now->entry = atomic_load_int(&music.now_entry);
	if (now->entry == 0)
		now->position = music.title_active ? (long)(time(NULL) - music.title_started) : 0;
	else if (now->entry > 0)
	{
		long backlog = (long)(atomic_load_unsigned(&music.write_frames) - atomic_load_unsigned(&music.read_frames));
		long position = (long)atomic_load_unsigned(&music.now_decoded) - backlog;

		now->position = position > 0 ? position / AE_MCC_MUSIC_RATE : 0;
		now->length = (long)(song_of(now->entry)->frames / AE_MCC_MUSIC_RATE);
	}
	/* (what follows: the worker's order, read whole) */
	for (;;)
	{
		before = atomic_load_unsigned(&music.shown_lock);
		now->next_count = 0;
		if (!(before & 1) && music.shown_count > 1 && now->entry >= 0)
		{
			at = atomic_load_int(&music.now_index);
			for (index = 1; index <= AE_MCC_MUSIC_NEXT && index < music.shown_count; index++)
				now->next[now->next_count++] = music.shown_order[(at + index) % music.shown_count];
		}
		__atomic_thread_fence(__ATOMIC_ACQUIRE);
		if (!(before & 1) && atomic_load_unsigned(&music.shown_lock) == before)
			break;
	}
}

void ae_mcc_music_next(void)
{
	if ((!music.owner && !music.title_active) || atomic_load_int(&music.catalog_state) != CATALOG_READY)
		return;
	atomic_store_unsigned(&music.next_count, music.next_count + 1);
	platform_log("ae menu music: next song");
}

void ae_mcc_music_preview(int entry)
{
	if (entry >= 1 && (atomic_load_int(&music.catalog_state) != CATALOG_READY || entry >= music.entry_count))
		return;
	if (entry < 1 && !music.preview_wanted)
		return;
	if (entry >= 1 && !music.preview_wanted)
	{
		/* (a preview starting: the menu song pauses. The ring's own song is dropped by the worker's flush when it takes
		the request; the game's own music (the title theme, the stock choice) is not in the ring, so it is stopped here,
		and started again when the preview ends) */
		music.preview_pause = ae_mcc_music_preview_pause(music.owner, music.title_active, ui_main_menu_music_active());
		if (music.preview_pause == AE_MCC_MUSIC_PAUSE_GAME)
		{
			ui_stop_main_menu_music();   /* (the hook: ae_mcc_music_menu_stop, owner off, nothing wanted yet) */
			music.title_active = 0;
			music.restart_until = 0;
		}
		platform_log("ae menu music: preview starts, the menu song is %s",
			music.preview_pause == AE_MCC_MUSIC_PAUSE_GAME ? "stopped (the game's own music, started again after)" :
			music.preview_pause == AE_MCC_MUSIC_PAUSE_RING ? "paused (the ring is flushed, it goes on after)" :
			"not playing");
	}
	music.preview_wanted = entry >= 1;
	atomic_store_int(&music.preview_request, entry >= 1 ? entry : -1);
	atomic_store_unsigned(&music.preview_generation, music.preview_generation + 1);
	if (entry < 1)
	{
		atomic_store_int(&music.preview_on, 0);
		platform_log("ae menu music: preview ends, the menu song %s",
			music.preview_pause == AE_MCC_MUSIC_PAUSE_GAME ? "starts again (the game's own music)" :
			music.preview_pause == AE_MCC_MUSIC_PAUSE_RING ? "goes on (the ring)" : "was not playing");
		if (music.preview_pause == AE_MCC_MUSIC_PAUSE_GAME)
		{
			music.restart_until = time(NULL) + RESTART_SECONDS;
			ui_start_main_menu_music();
		}
		music.preview_pause = AE_MCC_MUSIC_PAUSE_NONE;
	}
	else
		atomic_store_int(&music.preview_on, 1);
}

/* ---------- the mixer's thread */

void ae_mcc_music_mix(float *output, unsigned long frames, unsigned long channels, float master_volume)
{
	int playing = atomic_load_int(&music.playing) || atomic_load_int(&music.preview_on);
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
