/*
AE_ANNOUNCER.C

The announcer (ae_announcer.h). Log: "announcer: ...".

game.announcer: "auto" (the default: with an MCC installation, the announcer of its game whose sound banks hold the most
of the lines below, picked by the loader; else, or when none holds enough, the game's own), "classic", the game's own (the map's multiplayer sounds), or a game folder of the player's MCC
installation (ae_mcc_bank_games): that game's announcer, its lines read from the installation's sound banks (the
folder's fmod/pc/english.fsb, then its sfx.fsb), all decoded by a thread of their own when it is picked (below), kept. It
speaks the medals that have a line (multikills, sprees, ...; ae_medals.c, with MEDALS on) and, in the map's lines'
place, the game's events it has a line for (the modes' names, flag and hill events, the clock to win, game over); an
event it has no line for keeps the map's own. Nothing of MCC's is shipped.

A pack of WAVs: <data root>/announcers/<pack>/<key>.wav, 16 bit; game.announcer "pack:<pack>" (the ANNOUNCER row lists
each folder of announcers/), or HALO_ANNOUNCER_PACK (kept; it comes before game.announcer). Its keys are the medals' keys
and the names of the tables below (sound_lines, bomb_lines, extra_lines): a pack speaks the medals it has a file for
and, in the map's lines' place, the events it has a file for; the rest stay the map's. A key may have more files:
<key>.1.wav, <key>.2.wav are takes that share the key evenly with <key>.wav, and <key>.x1.wav, <key>.x2.wav ... more of the
key (ae_announcer_rules.h).

The lines play one at a time through the mixer's UI tone (platform_ui_tone_start), the oldest waiting let go when
three wait, so the voice keeps up with the game.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cseries/cseries.h"
#include "cseries/cseries_windows.h"

#include "game/game_engine.h"
#include "game/game.h"

#include "ae_announcer.h"
#include "ae_announcer_rules.h"
#include "ae_juggernaut.h"
#include "ae_round.h"
#include "ae_round_internal.h"
#include "ae_round_rules.h"
#include "ae_variant_ext.h"
#include "ae_mcc_music.h"
#include "ae_medals.h"
#include "../src/ae_worker.h"

/* (a path of the host, not one of the game's drives) */
#undef fopen

void platform_log(const char *format, ...);
const char *platform_data_root(void);
const char *config_string(const char *name);
int ae_mcc_copy_root(char *root, size_t size);
void platform_ui_tone_start(short const *samples, unsigned long frames, unsigned long channels,
	unsigned long sample_rate);

#define QUEUE_MOST 3
#define GAP_MILLISECONDS 120
#define CLIP_BYTES_MOST (8 * 1024 * 1024)
#define FOLDER_MOST 64
/* a pack's key: its takes (the key's file and its numbered ones) and its more takes (x files) */
#define TAKES_MOST 3
#define OTHERS_MOST 6
#define PACK_PREFIX "pack:"
/* the map's multiplayer sounds (game_engine_multiplayer_sounds.c's enum) */
#define MULTIPLAYER_SOUNDS 44

struct clip
{
	int ready;                                 /* an MCC clip's: decoded (the loader's release, the game's acquire) */
	boolean tried;
	short *samples;
	unsigned long frames;
	unsigned long channels;
	unsigned long rate;
};

/* a pack's key: its takes, read when first asked for */
struct voice
{
	boolean tried;
	int clean;
	int others;
	struct clip takes[TAKES_MOST];
	struct clip other_takes[OTHERS_MOST];
};

static struct
{
	struct voice pack_medals[AE_MEDAL_COUNT];  /* the WAV pack's */
	struct voice pack_sounds[MULTIPLAYER_SOUNDS];
	struct voice pack_bomb_sounds[MULTIPLAYER_SOUNDS];
	struct voice pack_extra[5];
	struct voice pack_named[2];
	char pack[FOLDER_MOST];                    /* the pack the voices above are of */
	unsigned long draw;                        /* the takes' random draw: this machine's own, never the game's */
	int run[4];
	struct ae_announcer_burst burst;
	struct clip mcc_medals[AE_MEDAL_COUNT];
	struct clip mcc_sounds[MULTIPLAYER_SOUNDS];
	struct clip mcc_bomb_sounds[MULTIPLAYER_SOUNDS];  /* the same events in a Bomb game: the bomb's lines */
	struct clip mcc_extra[5];                  /* lines no map sound has: extra_lines' (owner 11-08) */
	boolean started;                           /* this game's start line said (Bomb's engine has none of its own) */
	int round_phase;                           /* the round modes' phase last frame (a round's end: round over) */
	struct clip *queue[QUEUE_MOST];
	int queued;
	unsigned long free_at;
	/* the MCC announcer read now: its game folder, its banks */
	char folder[FOLDER_MOST];
	int loading;                               /* the loader runs (it owns folder and the MCC clips) */
} announcer;

/* the map's multiplayer sounds: the MCC announcer's line for each, best first; NULL: the map's own */
static char const *const sound_lines[MULTIPLAYER_SOUNDS][3] = {
	{ "play_ball" },                                  /* play ball */
	{ "game_over" },
	{ "one_min_to_win" },
	{ "thirty_secs_to_win" },
	{ "one_min_to_win" },                             /* red team, a minute to win */
	{ "thirty_secs_to_win" },
	{ "one_min_to_win" },                             /* blue team's */
	{ "thirty_secs_to_win" },
	{ "flag_taken" },                                 /* blue team has the flag */
	{ "flag_recovered", "flag_returned", "flag_reset" },
	{ "flag_captured" },                              /* blue team scores */
	{ "flag_taken" },                                 /* red team's */
	{ "flag_recovered", "flag_returned", "flag_reset" },
	{ "flag_captured" },
	{ "double_kill" },
	{ "triple_kill" },
	{ "killtacular" },
	{ "running_riot" },
	{ "killing_spree" },
	{ "oddball" },
	{ "race" },
	{ "slayer" },
	{ "capture_the_flag" },
	{ NULL }, { NULL }, { NULL },                      /* warthog, ghost, scorpion (race) */
	{ NULL },                                         /* countdown timer */
	{ NULL },                                         /* teleporter */
	{ NULL },                                         /* flag failure */
	{ NULL },                                         /* countdown for respawn */
	{ "hill_moved" },
	{ NULL },                                         /* player respawn */
	{ "team_king_of_the_hill", "king_of_the_hill" },
	{ "team_oddball", "oddball" },
	{ "team_race", "race" },
	{ "team_slayer", "slayer" },
	{ "king_of_the_hill" },
	{ "capture_the_flag" },                           /* blue team's capture the flag */
	{ "capture_the_flag" },
	{ "hill_contested" },
	{ "hill_controlled" },
	{ "hill_controlled" },                            /* hill occupied */
	{ NULL },                                         /* countdown end */
	{ NULL }                                          /* ting */
};

/* in a Bomb game (game_engine_bomb.c gives its events the flag's sounds): the bomb's lines; NULL: as in other games */
static char const *const bomb_lines[MULTIPLAYER_SOUNDS][2] = {
	[8] = { "bomb_taken" }, [11] = { "bomb_taken" },        /* a team has the bomb */
	[9] = { "bomb_reset", "bomb_returned" }, [12] = { "bomb_reset", "bomb_returned" },
	[10] = { "bomb_detonated" }, [13] = { "bomb_detonated" } /* a team scores: the bomb went off */
};

/* the flag's events outside CTF and Bomb keep the map's lines (its words are a team's, not a flag's) */
#define SOUND_SLAYER 21
#define SOUND_TEAM_SLAYER 35
#define SOUND_ODDBALL 19
#define SOUND_TEAM_ODDBALL 33
#define SOUND_KING 36
#define SOUND_TEAM_KING 32
#define START_TICKS (2 * 30)                       /* when a game names itself, as Oddball's does */

/* the lines no map sound stands for: a mode's name where the map says its engine's, Bomb's start, a round's end */
enum { EXTRA_EXTERMINATION, EXTRA_TERRITORIES, EXTRA_JUGGERNAUT, EXTRA_ASSAULT, EXTRA_ROUND_OVER, EXTRA_COUNT };
static char const *const extra_lines[EXTRA_COUNT] = { "extermination", "territories", "juggernaut", "assault",
	"round_over" };
static char const *const named_keys[2] = { "streak_8", "multi_b2" };
#define FLAG_FIRST 8
#define FLAG_LAST 13

/* medals whose key, less "medal_", is not the line's name */
static char const *const medal_lines[][2] = {
	{ "medal_triple_of_each_kill", "triple_kill" },
	{ "medal_untouchable_row_wit_untouchable", "untouchable" },
	{ "medal_sniper_rifle_w_spree", "sniper_spree" },
	{ "medal_sharpshooter_rifle_w_sharpshooter", "sharpshooter" },
	{ "medal_shotgun_shotgun_spree", "shotgun_spree" },
	{ "medal_splatter_row_wit_spree", "splatter_spree" },
	{ "medal_vehicle_row_wit_manslaughter", "vehicular_manslaughter" },
	{ "medal_unfriggenbelievable_row_wit_unfriggenbelievable", "unfrigginbelievable" }
};

static boolean pack_name_ok(char const *pack)
{
	return pack && *pack && strlen(pack) < FOLDER_MOST && !strchr(pack, '/') && !strchr(pack, '\\') &&
		!strstr(pack, "..") && !strchr(pack, ':');
}

/* the pack that speaks: HALO_ANNOUNCER_PACK's, else game.announcer's "pack:<folder>"; NULL for none (a folder of that name
is looked for when its lines are) */
static char const *pack_name(void)
{
	char const *pack = getenv("HALO_ANNOUNCER_PACK"), *value;

	if (pack_name_ok(pack))
		return pack;
	value = config_string("game.announcer");
	if (value && !strncmp(value, PACK_PREFIX, strlen(PACK_PREFIX)) && pack_name_ok(value + strlen(PACK_PREFIX)))
		return value + strlen(PACK_PREFIX);
	return NULL;
}

/* game.announcer's MCC game folder, NULL for the game's own */
static char const *mcc_folder(void)
{
	char const *value = config_string("game.announcer");
	char root[8];

	if (pack_name())
		return NULL;
	/* ("auto" is a folder only with an MCC installation; the loader picks the game) */
	if (value && !strcmp(value, "auto"))
		return ae_mcc_copy_root(root, sizeof(root)) ? "auto" : NULL;
	if (!value || !value[0] || !strcmp(value, "classic") || strlen(value) >= FOLDER_MOST || strchr(value, '/') ||
		strchr(value, '\\') || strstr(value, ".."))
		return NULL;
	return value;
}

static unsigned long little(unsigned char const *at, int bytes)
{
	unsigned long value = 0;
	int index;

	for (index = 0; index < bytes; index++)
		value |= (unsigned long)at[index] << (index * 8);
	return value;
}

static void clip_parse(struct clip *clip, unsigned char const *bytes, long size)
{
	long at = 12;
	int bits = 0;

	if (size < 44 || memcmp(bytes, "RIFF", 4) || memcmp(bytes + 8, "WAVE", 4))
		return;
	while (at + 8 <= size)
	{
		unsigned long length = little(bytes + at + 4, 4);

		if (length > (unsigned long)(size - at - 8))
			return;
		if (!memcmp(bytes + at, "fmt ", 4) && length >= 16)
		{
			clip->channels = little(bytes + at + 10, 2);
			clip->rate = little(bytes + at + 12, 4);
			bits = (int)little(bytes + at + 22, 2);
		}
		else if (!memcmp(bytes + at, "data", 4))
		{
			if (bits != 16 || clip->channels < 1 || clip->channels > 2 || !clip->rate || length < 2)
				return;
			clip->samples = malloc(length);
			if (clip->samples)
			{
				memcpy(clip->samples, bytes + at + 8, length);
				clip->frames = length / (2 * clip->channels);
			}
			return;
		}
		at += 8 + (long)length + (long)(length & 1);
	}
}

/* a clip read from a WAV file of the pack (false: none there, or not a 16 bit WAV of one or two channels) */
static boolean pack_read(struct clip *clip, char const *path)
{
	FILE *file = fopen(path, "rb");
	long size;
	unsigned char *bytes;

	memset(clip, 0, sizeof(*clip));
	if (!file)
		return FALSE;
	if (!fseek(file, 0, SEEK_END) && (size = ftell(file)) > 0 && size <= CLIP_BYTES_MOST && !fseek(file, 0, SEEK_SET) &&
		(bytes = malloc((size_t)size)) != NULL)
	{
		if (fread(bytes, 1, (size_t)size, file) == (size_t)size)
			clip_parse(clip, bytes, size);
		free(bytes);
	}
	fclose(file);
	return clip->samples != NULL;
}

static void pack_clear(struct voice *voice)
{
	int index;

	/* (the game's free is debug_free, which reads a header before the pointer: never hand it NULL, and a voice never
	loaded, or a take it lacks, has none) */
	for (index = 0; index < TAKES_MOST; index++)
		if (voice->takes[index].samples)
			free(voice->takes[index].samples);
	for (index = 0; index < OTHERS_MOST; index++)
		if (voice->other_takes[index].samples)
			free(voice->other_takes[index].samples);
	memset(voice, 0, sizeof(*voice));
}

/* the pack's takes of the first of names it has a file for */
static void pack_load(struct voice *voice, char const *pack, char const *const *names, int count)
{
	char path[1024];
	int name, take;

	voice->tried = TRUE;
	for (name = 0; name < count && names[name]; name++)
	{
		snprintf(path, sizeof(path), "%s/announcers/%s/%s.wav", platform_data_root(), pack, names[name]);
		if (!pack_read(&voice->takes[0], path))
			continue;
		voice->clean = 1;
		for (take = 1; take < TAKES_MOST; take++)
		{
			snprintf(path, sizeof(path), "%s/announcers/%s/%s.%d.wav", platform_data_root(), pack, names[name], take);
			if (!pack_read(&voice->takes[voice->clean], path))
				break;
			voice->clean++;
		}
		for (take = 1; take <= OTHERS_MOST; take++)
		{
			snprintf(path, sizeof(path), "%s/announcers/%s/%s.x%d.wav", platform_data_root(), pack, names[name], take);
			if (!pack_read(&voice->other_takes[voice->others], path))
				break;
			voice->others++;
		}
		return;
	}
}

/* a draw of this machine's own (never the game's: no part of the game's state depends on it) */
static unsigned long next_draw(void)
{
	if (!announcer.draw)
		announcer.draw = system_milliseconds() | 1;
	announcer.draw ^= announcer.draw << 13;
	announcer.draw &= 0xFFFFFFFFul;
	announcer.draw ^= announcer.draw >> 17;
	announcer.draw ^= announcer.draw << 5;
	announcer.draw &= 0xFFFFFFFFul;
	return announcer.draw;
}

/* the voice's take to speak, NULL when it has none */
static struct clip *pack_take(struct voice *voice)
{
	int take = ae_announcer_take(voice->clean, voice->others, next_draw());

	if (take < 0)
		return NULL;
	return take < voice->clean ? &voice->takes[take] : &voice->other_takes[take - voice->clean];
}

/* the pack's voices let go when another pack (or none) is the one now: the queue too, which points into them */
static void pack_sync(void)
{
	char const *pack = pack_name();
	int index;

	if (!pack ? !announcer.pack[0] : !strcmp(pack, announcer.pack))
		return;
	for (index = 0; index < AE_MEDAL_COUNT; index++)
		pack_clear(&announcer.pack_medals[index]);
	for (index = 0; index < MULTIPLAYER_SOUNDS; index++)
	{
		pack_clear(&announcer.pack_sounds[index]);
		pack_clear(&announcer.pack_bomb_sounds[index]);
	}
	for (index = 0; index < EXTRA_COUNT; index++)
		pack_clear(&announcer.pack_extra[index]);
	for (index = 0; index < 2; index++)
		pack_clear(&announcer.pack_named[index]);
	announcer.queued = 0;
	snprintf(announcer.pack, sizeof(announcer.pack), "%s", pack ? pack : "");
}

static struct voice *pack_voice(struct voice *voice, char const *const *names, int count)
{
	if (!voice->tried)
		pack_load(voice, announcer.pack, names, count);
	return voice;
}

static struct clip *pack_medal_clip(int medal)
{
	char const *key;

	if (!pack_name() || medal < 0 || medal >= AE_MEDAL_COUNT || !(key = ae_medal_key(medal)) ||
		!ae_announcer_pack_speaks_medal(key))
		return NULL;
	pack_sync();
	return pack_take(pack_voice(&announcer.pack_medals[medal], &key, 1));
}

/* ---------- the MCC announcer */

/* An MCC announcer's lines are decoded by a loader thread of their own (ae_worker.h), never on the game's: the banks
may be on a network share that stops answering for minutes (a game thread waiting on it froze a game, 11-07). The game
thread starts the loader when game.announcer names a folder, then uses a line only once the loader marked it ready;
until then (and for a line the announcer has not) the map's own line plays. The loader owns the clips and the folder
while it runs; the game thread lets them go only when it is done. */

/* an MCC clip let go (its samples are the C library's: ae_mcc_bank_free, not cseries' free) */
static void clip_free(struct clip *clip)
{
	if (clip->samples)
		ae_mcc_bank_free(clip->samples);
	memset(clip, 0, sizeof(*clip));
}

static int loader_running(void)
{
	return __atomic_load_n(&announcer.loading, __ATOMIC_ACQUIRE);
}

static void mcc_close(void)
{
	int index;

	for (index = 0; index < AE_MEDAL_COUNT; index++)
		clip_free(&announcer.mcc_medals[index]);
	for (index = 0; index < MULTIPLAYER_SOUNDS; index++)
	{
		clip_free(&announcer.mcc_sounds[index]);
		clip_free(&announcer.mcc_bomb_sounds[index]);
	}
	for (index = 0; index < EXTRA_COUNT; index++)
		clip_free(&announcer.mcc_extra[index]);
	announcer.folder[0] = 0;
}

/* a medal's line: its key less "medal_", or the table's name; NULL for a medal the game's events say (flag, ball, hill) */
static char const *medal_line(int medal)
{
	char const *key = ae_medal_key(medal), *line;
	size_t index;

	if (!key || strncmp(key, "medal_", 6) || !strncmp(key, "medal_flag_", 11) || !strncmp(key, "medal_ball_", 11) ||
		!strncmp(key, "medal_hill_", 11))
		return NULL;
	line = key + 6;
	for (index = 0; index < sizeof(medal_lines) / sizeof(medal_lines[0]); index++)
		if (!strcmp(key, medal_lines[index][0]))
			line = medal_lines[index][1];
	return line;
}

/* (the loader) a clip decoded: the first of names (count) a bank has; ready once its samples are there */
static void load_clip(struct ae_mcc_bank **banks, struct clip *clip, char const *const *names, int count)
{
	int name, bank;

	for (name = 0; name < count && names[name]; name++)
	{
		for (bank = 0; bank < 2; bank++)
		{
			int stream = ae_mcc_bank_find(banks[bank], names[name]);

			if (stream >= 0 && ae_mcc_bank_decode(banks[bank], stream, &clip->samples, &clip->frames, &clip->channels,
				&clip->rate))
			{
				__atomic_store_n(&clip->ready, 1, __ATOMIC_RELEASE);
				return;
			}
		}
	}
}

/* (the loader) how many of the lines below an MCC game folder's banks hold */
static int lines_held(char const *folder)
{
	struct ae_mcc_bank *banks[2];
	int index, bank, held = 0;

	banks[0] = ae_mcc_bank_open(folder, "english.fsb");
	banks[1] = ae_mcc_bank_open(folder, "sfx.fsb");
	for (index = 0; index < AE_MEDAL_COUNT + MULTIPLAYER_SOUNDS + EXTRA_COUNT; index++)
	{
		char const *line = index < AE_MEDAL_COUNT ? medal_line(index) :
			index < AE_MEDAL_COUNT + MULTIPLAYER_SOUNDS ? sound_lines[index - AE_MEDAL_COUNT][0] :
			extra_lines[index - AE_MEDAL_COUNT - MULTIPLAYER_SOUNDS];

		for (bank = 0; line && line[0] && bank < 2; bank++)
		{
			if (ae_mcc_bank_find(banks[bank], line) >= 0)
			{
				held++;
				break;
			}
		}
	}
	ae_mcc_bank_close(banks[0]);
	ae_mcc_bank_close(banks[1]);
	return held;
}

/* (the loader) "auto"'s game: of the MCC game folders with a spoken-lines bank, the one whose banks hold the most of
the lines (ae_announcer_auto_pick); FALSE for none */
static boolean auto_pick(char *folder, size_t size)
{
	char names[16][64];
	int held[16];
	int games = ae_mcc_bank_games(names, 16), index, pick;

	for (index = 0; index < games; index++)
	{
		held[index] = lines_held(names[index]);
		platform_log("announcer: auto: %s's banks hold %d lines", names[index], held[index]);
	}
	pick = ae_announcer_auto_pick(held, games);
	if (pick < 0)
		return FALSE;
	snprintf(folder, size, "%s", names[pick]);
	return TRUE;
}

static void *loader(void *argument)
{
	struct ae_mcc_bank *banks[2];
	char folder[FOLDER_MOST];
	int index, count = 0;

	(void)argument;
	snprintf(folder, sizeof(folder), "%s", announcer.folder);
	if (!strcmp(folder, "auto") && !auto_pick(folder, sizeof(folder)))
	{
		platform_log("announcer: auto: no MCC game here has enough of the lines; the game's own speaks");
		__atomic_store_n(&announcer.loading, 0, __ATOMIC_RELEASE);
		return NULL;
	}
	banks[0] = ae_mcc_bank_open(folder, "english.fsb");
	banks[1] = ae_mcc_bank_open(folder, "sfx.fsb");
	if (!banks[0] && !banks[1])
		platform_log("announcer: %s has no sound banks here; the game's own speaks", folder);
	else
	{
		for (index = 0; index < AE_MEDAL_COUNT; index++)
		{
			char const *line = medal_line(index);

			if (line)
				load_clip(banks, &announcer.mcc_medals[index], &line, 1);
			count += announcer.mcc_medals[index].ready;
		}
		for (index = 0; index < MULTIPLAYER_SOUNDS; index++)
		{
			if (sound_lines[index][0])
				load_clip(banks, &announcer.mcc_sounds[index], sound_lines[index], 3);
			if (bomb_lines[index][0])
				load_clip(banks, &announcer.mcc_bomb_sounds[index], bomb_lines[index], 2);
			count += announcer.mcc_sounds[index].ready + announcer.mcc_bomb_sounds[index].ready;
		}
		for (index = 0; index < EXTRA_COUNT; index++)
		{
			load_clip(banks, &announcer.mcc_extra[index], extra_lines + index, 1);
			count += announcer.mcc_extra[index].ready;
		}
		platform_log("announcer: %s's, from MCC: %d lines", folder, count);
	}
	ae_mcc_bank_close(banks[0]);
	ae_mcc_bank_close(banks[1]);
	__atomic_store_n(&announcer.loading, 0, __ATOMIC_RELEASE);
	return NULL;
}

/* game.announcer's folder made the one loaded (the loader started for a new one); whether it is (lines may still be
loading: each says when it is ready) */
static boolean mcc_open(void)
{
	char const *folder = mcc_folder();

	if (loader_running())
		return folder && !strcmp(folder, announcer.folder);
	if (!folder)
	{
		if (announcer.folder[0])
			mcc_close();
		return FALSE;
	}
	if (strcmp(folder, announcer.folder))
	{
		mcc_close();
		snprintf(announcer.folder, sizeof(announcer.folder), "%s", folder);
		__atomic_store_n(&announcer.loading, 1, __ATOMIC_RELEASE);
		if (!ae_worker_start(loader, NULL))
		{
			__atomic_store_n(&announcer.loading, 0, __ATOMIC_RELEASE);
			platform_log("announcer: no thread to read %s's lines; the game's own speaks", folder);
			return FALSE;
		}
	}
	return TRUE;
}

static struct clip *ready_clip(struct clip *clip)
{
	return __atomic_load_n(&clip->ready, __ATOMIC_ACQUIRE) ? clip : NULL;
}

/* whether an announcer other than the game's own is the one set (a pack, or the MCC one: its loader started) */
static boolean speaking(void)
{
	return pack_name() ? TRUE : mcc_open();
}

static struct clip *mcc_medal_clip(int medal)
{
	if (medal < 0 || medal >= AE_MEDAL_COUNT || !mcc_open())
		return NULL;
	return ready_clip(&announcer.mcc_medals[medal]);
}

/* the tables' lines: a map sound's, a Bomb game's, an extra */
enum { LINE_SOUND, LINE_BOMB, LINE_EXTRA };

/* the line to speak: the pack's take of it, else the MCC clip when the loader has it ready; NULL when there is none */
static struct clip *line_clip(int kind, int index)
{
	struct voice *voice;
	struct clip *clip;

	if (pack_name())
	{
		pack_sync();
		if (kind == LINE_SOUND)
			voice = pack_voice(&announcer.pack_sounds[index], sound_lines[index], 3);
		else if (kind == LINE_BOMB)
			voice = pack_voice(&announcer.pack_bomb_sounds[index], bomb_lines[index], 2);
		else
			voice = pack_voice(&announcer.pack_extra[index], extra_lines + index, 1);
		return pack_take(voice);
	}
	clip = kind == LINE_SOUND ? &announcer.mcc_sounds[index] : kind == LINE_BOMB ? &announcer.mcc_bomb_sounds[index] :
		&announcer.mcc_extra[index];
	return ready_clip(clip);
}

/* whether a line has a clip at all (a pack's file, or the MCC clip ready), without a take drawn */
static boolean line_has(int kind, int index)
{
	struct clip *clip;

	if (pack_name())
	{
		struct voice *voice;

		pack_sync();
		if (kind == LINE_SOUND)
			voice = pack_voice(&announcer.pack_sounds[index], sound_lines[index], 3);
		else if (kind == LINE_BOMB)
			voice = pack_voice(&announcer.pack_bomb_sounds[index], bomb_lines[index], 2);
		else
			voice = pack_voice(&announcer.pack_extra[index], extra_lines + index, 1);
		return voice->clean > 0 || voice->others > 0;
	}
	clip = kind == LINE_SOUND ? &announcer.mcc_sounds[index] : kind == LINE_BOMB ? &announcer.mcc_bomb_sounds[index] :
		&announcer.mcc_extra[index];
	return ready_clip(clip) != NULL;
}

static struct clip *mcc_sound_clip(short sound)
{
	struct game_variant const *variant = game_engine_get_variant();
	long engine = variant ? variant->game_engine_index : NONE;

	if (sound < 0 || sound >= MULTIPLAYER_SOUNDS || !speaking())
		return NULL;
	/* (a mode's own name where the map says its engine's: EXTERMINATION for slayer, TERRITORIES for king of the hill,
	JUGGERNAUT for oddball; an announcer without the line, the engine's) */
	if ((sound == SOUND_SLAYER || sound == SOUND_TEAM_SLAYER) && ae_round_kind() == AE_ROUND_KIND_EXTERMINATION &&
		line_has(LINE_EXTRA, EXTRA_EXTERMINATION))
		return line_clip(LINE_EXTRA, EXTRA_EXTERMINATION);
	if ((sound == SOUND_KING || sound == SOUND_TEAM_KING) && ae_variant_ext_game() &&
		ae_variant_ext_game()->mode == AE_EXT_MODE_TERRITORIES && line_has(LINE_EXTRA, EXTRA_TERRITORIES))
		return line_clip(LINE_EXTRA, EXTRA_TERRITORIES);
	if ((sound == SOUND_ODDBALL || sound == SOUND_TEAM_ODDBALL) && ae_juggernaut_active() &&
		line_has(LINE_EXTRA, EXTRA_JUGGERNAUT))
		return line_clip(LINE_EXTRA, EXTRA_JUGGERNAUT);
	/* (the flag's events: a Bomb game's are the bomb's; in a game neither CTF nor Bomb, the map's own words) */
	if (sound >= FLAG_FIRST && sound <= FLAG_LAST)
	{
		if (engine == game_engine_bomb)
			return line_clip(LINE_BOMB, sound);
		if (engine != game_engine_ctf)
			return NULL;
	}
	return line_clip(LINE_SOUND, sound);
}

/* ---------- the queue */

static void queue_add(struct clip *clip)
{
	if (!clip)
		return;
	/* (full: the oldest waiting goes, so the voice does not fall behind the game) */
	if (announcer.queued == QUEUE_MOST)
	{
		memmove(announcer.queue, announcer.queue + 1, sizeof(announcer.queue[0]) * (QUEUE_MOST - 1));
		announcer.queued--;
	}
	announcer.queue[announcer.queued++] = clip;
}

void ae_announcer_new_map(void)
{
	int index, count = 0;

	announcer.queued = 0;
	announcer.started = FALSE;
	announcer.round_phase = -1;
	for (index = 0; index < 4; index++)
		ae_announcer_run_end(&announcer.run[index]);
	memset(&announcer.burst, 0, sizeof(announcer.burst));
	if (pack_name())
	{
		pack_sync();
		/* (every line read now, in the map's load: not a stall at the first kill) */
		for (index = 0; index < AE_MEDAL_COUNT; index++)
		{
			char const *key = ae_medal_key(index);

			if (key && pack_voice(&announcer.pack_medals[index], &key, 1)->clean)
				count++;
		}
		for (index = 0; index < MULTIPLAYER_SOUNDS; index++)
		{
			if (sound_lines[index][0])
				count += pack_voice(&announcer.pack_sounds[index], sound_lines[index], 3)->clean > 0;
			if (bomb_lines[index][0])
				count += pack_voice(&announcer.pack_bomb_sounds[index], bomb_lines[index], 2)->clean > 0;
		}
		for (index = 0; index < EXTRA_COUNT; index++)
			count += pack_voice(&announcer.pack_extra[index], extra_lines + index, 1)->clean > 0;
		for (index = 0; index < 2; index++)
		{
			char const *key = named_keys[index];

			count += pack_voice(&announcer.pack_named[index], &key, 1)->clean > 0;
		}
		platform_log("announcer: pack %s: %d lines", announcer.pack, count);
	}
	else
		mcc_open();
}

void ae_announcer_medal(int medal)
{
	if (pack_name())
		queue_add(pack_medal_clip(medal));
	else
		queue_add(mcc_medal_clip(medal));
}

static void named_speak(int which)
{
	char const *key = named_keys[which];

	if (!pack_name())
		return;
	pack_sync();
	queue_add(pack_take(pack_voice(&announcer.pack_named[which], &key, 1)));
	ae_announcer_update();
}

void ae_announcer_count_kill(int local_player, int victim_local, int betrayal, int area, long source, long tick)
{
	if (victim_local >= 0 && victim_local < 4)
		ae_announcer_run_end(&announcer.run[victim_local]);
	if (local_player < 0 || local_player >= 4)
		return;
	if (betrayal)
	{
		if (area && ae_announcer_burst_kill(&announcer.burst, tick, local_player, source))
			named_speak(1);
	}
	else if (ae_announcer_run_kill(&announcer.run[local_player]))
		named_speak(0);
}

/* (each frame) the lines no map sound stands for: a Bomb game's name at its start (Bomb's engine says none), "round over"
when a round of the round modes ends, on every machine (ae_round_view is the host's and its guests') */
static void mcc_events(void)
{
	if ((!pack_name() && !mcc_folder()) || !game_engine_running() || !speaking())
		return;
	if (!announcer.started && game_time_get() >= START_TICKS)
	{
		struct game_variant const *variant = game_engine_get_variant();

		announcer.started = TRUE;
		if (variant && variant->game_engine_index == game_engine_bomb)
			queue_add(line_clip(LINE_EXTRA, EXTRA_ASSAULT));
	}
	if (ae_round_on() && ae_round_view())
	{
		int phase = ae_round_view()->phase;

		if (phase == AE_ROUND_PHASE_POST && announcer.round_phase == AE_ROUND_PHASE_LIVE)
			queue_add(line_clip(LINE_EXTRA, EXTRA_ROUND_OVER));
		announcer.round_phase = phase;
	}
}

void ae_announcer_update(void)
{
	unsigned long now;
	struct clip *clip;

	mcc_events();

	if (!announcer.queued)
		return;
	now = system_milliseconds();
	if ((long)(now - announcer.free_at) < 0)
		return;
	clip = announcer.queue[0];
	announcer.queued--;
	memmove(announcer.queue, announcer.queue + 1, sizeof(announcer.queue[0]) * (size_t)announcer.queued);
	if (!clip || !clip->samples)
		return;
	platform_ui_tone_start(clip->samples, clip->frames, clip->channels, clip->rate);
	announcer.free_at = now + (unsigned long)((unsigned long long)clip->frames * 1000 / clip->rate) + GAP_MILLISECONDS;
}

int ae_announcer_replaces(short multiplayer_sound)
{
	struct clip *clip;

	/* (a multikill or spree line comes here only for a local player with MEDALS off: one with them on has the game's
	line taken by the kill feed, ae_killfeed_replaces_message, and its medal says it. So the line is spoken, each
	split-screen player by their own MEDALS, not player 1's) */
	if (!pack_name() && !mcc_folder())
		return 0;
	/* (a line the announcer has not: the map's own) */
	clip = mcc_sound_clip(multiplayer_sound);
	if (!clip)
		return 0;
	queue_add(clip);
	ae_announcer_update();
	return 1;
}
