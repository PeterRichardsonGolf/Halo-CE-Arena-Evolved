/*
AE_ANNOUNCER.C

The announcer (ae_announcer.h). Log: "announcer: ...".

game.announcer: "classic", the game's own (the map's multiplayer sounds), or a game folder of the player's MCC
installation (ae_mcc_bank_games): that game's announcer, its lines read from the installation's sound banks (the
folder's fmod/pc/english.fsb, then its sfx.fsb), all decoded by a thread of their own when it is picked (below), kept. It
speaks the medals that have a line (multikills, sprees, ...; ae_medals.c, with MEDALS on) and, in the map's lines'
place, the game's events it has a line for (the modes' names, flag and hill events, the clock to win, game over); an
event it has no line for keeps the map's own. Nothing of MCC's is shipped.

(spike, kept) HALO_ANNOUNCER_PACK: a folder of WAVs, <data root>/announcers/<pack>/<medal's key>.wav, 16 bit, for
the medals; it comes before game.announcer.

The lines play one at a time through the mixer's UI tone (platform_ui_tone_start), the oldest waiting let go when
three wait, so the voice keeps up with the game.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cseries/cseries.h"
#include "cseries/cseries_windows.h"

#include "game/game_engine.h"

#include "ae_announcer.h"
#include "ae_round.h"
#include "ae_round_rules.h"
#include "ae_mcc_music.h"
#include "ae_medals.h"
#include "../src/ae_config_players.h"
#include "../src/ae_worker.h"

/* (a path of the host, not one of the game's drives) */
#undef fopen

void platform_log(const char *format, ...);
const char *platform_data_root(void);
const char *config_string(const char *name);
void platform_ui_tone_start(short const *samples, unsigned long frames, unsigned long channels,
	unsigned long sample_rate);

#define QUEUE_MOST 3
#define GAP_MILLISECONDS 120
#define CLIP_BYTES_MOST (8 * 1024 * 1024)
#define FOLDER_MOST 64
/* the map's multiplayer sounds (game_engine_multiplayer_sounds.c's enum) */
#define MULTIPLAYER_SOUNDS 44
#define MULTIKILL_FIRST 0xE            /* double kill, triple kill, killtacular, running riot, killing spree */
#define MULTIKILL_LAST 0x12

struct clip
{
	int ready;                                 /* an MCC clip's: decoded (the loader's release, the game's acquire) */
	boolean tried;
	short *samples;
	unsigned long frames;
	unsigned long channels;
	unsigned long rate;
};

static struct
{
	struct clip clips[AE_MEDAL_COUNT];         /* the WAV pack's */
	struct clip mcc_medals[AE_MEDAL_COUNT];
	struct clip mcc_sounds[MULTIPLAYER_SOUNDS];
	struct clip mcc_bomb_sounds[MULTIPLAYER_SOUNDS];  /* the same events in a Bomb game: the bomb's lines */
	struct clip mcc_extermination;             /* EXTERMINATION's name at its start, in Slayer's place (owner 11-08) */
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

static char const *pack_name(void)
{
	char const *pack = getenv("HALO_ANNOUNCER_PACK");
	/* (a pack only when one is named; no folder of that name, no pack) */
	return pack && *pack && !strchr(pack, '/') && !strchr(pack, '\\') ? pack : NULL;
}

/* game.announcer's MCC game folder, NULL for the game's own */
static char const *mcc_folder(void)
{
	char const *value = config_string("game.announcer");

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

static struct clip *pack_clip_of(int medal)
{
	struct clip *clip;
	char path[1024];
	char const *pack = pack_name();
	char const *key;
	FILE *file;
	long size;
	unsigned char *bytes;

	if (!pack || medal < 0 || medal >= AE_MEDAL_COUNT)
		return NULL;
	clip = &announcer.clips[medal];
	if (clip->tried)
		return clip->samples ? clip : NULL;
	clip->tried = TRUE;
	key = ae_medal_key(medal);
	if (!key)
		return NULL;
	snprintf(path, sizeof(path), "%s/announcers/%s/%s.wav", platform_data_root(), pack, key);
	file = fopen(path, "rb");
	if (!file)
		return NULL;
	if (!fseek(file, 0, SEEK_END) && (size = ftell(file)) > 0 && size <= CLIP_BYTES_MOST && !fseek(file, 0, SEEK_SET) &&
		(bytes = malloc((size_t)size)) != NULL)
	{
		if (fread(bytes, 1, (size_t)size, file) == (size_t)size)
			clip_parse(clip, bytes, size);
		free(bytes);
	}
	fclose(file);
	return clip->samples ? clip : NULL;
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
	clip_free(&announcer.mcc_extermination);
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

static void *loader(void *argument)
{
	struct ae_mcc_bank *banks[2];
	int index, count = 0;

	(void)argument;
	banks[0] = ae_mcc_bank_open(announcer.folder, "english.fsb");
	banks[1] = ae_mcc_bank_open(announcer.folder, "sfx.fsb");
	if (!banks[0] && !banks[1])
		platform_log("announcer: %s has no sound banks here; the game's own speaks", announcer.folder);
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
		{
			static char const *const extermination[] = { "extermination" };

			load_clip(banks, &announcer.mcc_extermination, extermination, 1);
			count += announcer.mcc_extermination.ready;
		}
		platform_log("announcer: %s's, from MCC: %d lines", announcer.folder, count);
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

static struct clip *mcc_medal_clip(int medal)
{
	if (medal < 0 || medal >= AE_MEDAL_COUNT || !mcc_open())
		return NULL;
	return ready_clip(&announcer.mcc_medals[medal]);
}

static struct clip *mcc_sound_clip(short sound)
{
	struct game_variant const *variant = game_engine_get_variant();
	long engine = variant ? variant->game_engine_index : NONE;

	if (sound < 0 || sound >= MULTIPLAYER_SOUNDS || !mcc_open())
		return NULL;
	/* (an EXTERMINATION game's start names it, where the map would say slayer; an announcer without the line, slayer) */
	if ((sound == SOUND_SLAYER || sound == SOUND_TEAM_SLAYER) && ae_round_kind() == AE_ROUND_KIND_EXTERMINATION &&
		ready_clip(&announcer.mcc_extermination))
		return &announcer.mcc_extermination;
	/* (the flag's events: a Bomb game's are the bomb's; in a game neither CTF nor Bomb, the map's own words) */
	if (sound >= FLAG_FIRST && sound <= FLAG_LAST)
	{
		if (engine == game_engine_bomb)
			return ready_clip(&announcer.mcc_bomb_sounds[sound]);
		if (engine != game_engine_ctf)
			return NULL;
	}
	return ready_clip(&announcer.mcc_sounds[sound]);
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
	int medal, count = 0;

	announcer.queued = 0;
	if (pack_name())
	{
		for (medal = 0; medal < AE_MEDAL_COUNT; medal++)
			count += pack_clip_of(medal) != NULL;
		platform_log("announcer: pack %s: %d clips of %d medals", pack_name(), count, AE_MEDAL_COUNT);
	}
	else
		mcc_open();
}

void ae_announcer_medal(int medal)
{
	if (pack_name())
		queue_add(pack_clip_of(medal));
	else
		queue_add(mcc_medal_clip(medal));
}

void ae_announcer_update(void)
{
	unsigned long now;
	struct clip *clip;

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

	/* (the WAV pack: its medals say the multikills) */
	if (pack_name())
		return multiplayer_sound >= MULTIKILL_FIRST && multiplayer_sound <= MULTIKILL_LAST;
	if (!mcc_folder())
		return 0;
	/* (a line the announcer has not: the map's own) */
	clip = mcc_sound_clip(multiplayer_sound);
	if (!clip)
		return 0;
	/* (with MEDALS on, the medal of the same name says a multikill: the map's line would be a second) */
	if (multiplayer_sound >= MULTIKILL_FIRST && multiplayer_sound <= MULTIKILL_LAST &&
		ae_config_player_boolean(0, "display.medals"))
		return 1;
	queue_add(clip);
	ae_announcer_update();
	return 1;
}
