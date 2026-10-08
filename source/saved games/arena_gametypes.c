/*
ARENA_GAMETYPES.C

port: Halo CE: Arena Evolved (AE), our modern-gameplay set: gametypes made
from the stock ones plus the port's gametype options, saved as ordinary
custom gametypes, so every gametype list shows them, the PC menus edit them
and players may delete them.

They are seeded once per save root, when the game first lists the gametypes
(playlist_profiles_enumerate_available_to_local_player_index): each one not
yet seeded is written, unless a saved game of its name already exists
(never overwritten), and its name is added to the record
z:\saved\playlists\arena_gametypes.txt. One the player deletes or renames
is not written again; deleting the record brings back the missing ones.

A gametype seeded under a name since changed (arena_gametype_renames) is
renamed in place while its file is still exactly as seeded; one the player
changed is left as it is and the new one written beside it; one the player
deleted stays deleted.

The gametype lists show the custom gametypes in order
(arena_gametypes_sort): by set (AE, AE COMP, Halo 1: NHE's, TRAINING:
arena_gametype_names.c), each in its table's order, then the player's own.
*/

/* ---------- headers */

#include "cseries/cseries.h"
#include "saved games/arena_gametypes.h"
#include "cseries/errors.h"
#include "game/game_engine.h"
#include "game/game_engine_playlist.h"
#include "saved games/playlist_profile.h"
#include "saved games/saved_game_files.h"
#include "tag_files/files.h"
#include "text/unicode.h"

#include <string.h>
/* (MAX_GAMENAME: a saved game's display name) */
#include <xtl.h>

/* ---------- constants */

enum
{
	/* a gametype's name: 11 characters and its NUL (game_variant's
	human_readable_game_description; playlist_profile.c's
	MAXIMUM_GAME_VARIANT_NAME_LENGTH) */
	ARENA_GAMETYPE_NAME_LENGTH = 12,
	/* the record of those seeded: their names, a line each */
	ARENA_GAMETYPES_RECORD_SIZE = 1024,
	/* the custom gametypes looked through (saved_game_files.c lists at
	most 100 saved games) */
	ARENA_GAMETYPES_MAXIMUM_SAVED = 128,
};


/* what every AE gametype adds to its stock one: AE's rules (NO SPREAD FULL,
the PRE-GAME COUNTDOWN), no landing damage, HALO 2 health (health back as
the shields recharge), and the generic starting equipment (the stock
gametypes take the map's, Blood Gulch's a plasma pistol); its loadout is a
pistol in hand and an assault rifle (arena_gametype.custom_loadout) */
#define ARENA_GAMETYPE_FLAGS \
	(FLAG(_game_variant_no_falling_damage_bit) | \
	(_health_style_halo2 << _game_variant_health_style_first_bit) | \
	FLAG(_game_variant_generic_starting_equipment_bit) | \
	ARENA_RULES_FLAGS)

/* AE's competitive rules, as ARENA OPTIONS has them: NO SPREAD FULL (no
spread and no bloom for the pistol and the sniper rifle) and the PRE-GAME
COUNTDOWN */
#define ARENA_RULES_FLAGS \
	(GAME_VARIANT_NO_SPREAD_FULL | \
	FLAG(_game_variant_pregame_countdown_bit))

/* NHE's competitive rules, as ARENA OPTIONS has them: NO SPREAD NHE and the
PRE-GAME COUNTDOWN (on NHE's maps their scripts count down instead) */
#define NHE_RULES_FLAGS \
	(GAME_VARIANT_NO_SPREAD_NHE | \
	FLAG(_game_variant_pregame_countdown_bit))

/* NHE's: the generic starting equipment (stock rules otherwise); each adds
NHE_RULES_FLAGS but VANILLA (NHE's Vanilla had neither) */
#define NHE_GAMETYPE_FLAGS FLAG(_game_variant_generic_starting_equipment_bit)

/* AE's casual gametypes' aid: the power items' TIMERS */
#define ARENA_CASUAL_FLAGS FLAG(_game_variant_item_timers_bit)

/* TRAINING's aids: the item TIMERS and TRAINING's waypoints */
#define ARENA_TRAINING_FLAGS \
	(FLAG(_game_variant_item_timers_bit) | \
	FLAG(_game_variant_training_bit))

/* AE TRAINING's score to win: the most the gametype editor offers (its
KILLS TO WIN, menu_functions.c), as near "no limit" as a variant goes
(slayer ends the game at a score >= score_to_win, so 0 would end it at
once); the time limit is the PC options' default, none */
#define ARENA_TRAINING_SCORE_TO_WIN 500

/* the competitive sets' numbers (notes/competitive-settings.md: Beach LAN
15, the CE community's 2v2 rules, MLG Halo 3). Respawn and suicide penalty
are ticks (the gametype editor's 0, 5, 10, 15 seconds); the time limit is
the PC options' minutes. King's and Oddball's score is minutes held
(game_engine_king.c: score_to_win * TICKS_PER_MINUTE; game_engine_oddball.c:
* ODDBALL_SCORE_TICKS_PER_UNIT, a minute, but for terminator balls): MLG's
250 points (seconds) is 4 minutes 10; 5, the editor's nearest value */
enum
{
	/* (a number the stock gametype keeps) */
	ARENA_STOCK = -1,
	/* Beach LAN's respawn and suicide penalty: 5 seconds */
	ARENA_COMPETITIVE_RESPAWN = 5 * TICKS_PER_SECOND,
	/* AE's casual gametypes and the objective ones' time limit, minutes */
	ARENA_TIME_LIMIT = 15,
	ARENA_FFA_SCORE = 25,
	ARENA_TEAM_SLAYER_SCORE = 50,
	ARENA_CTF_SCORE = 3,
	/* King's and Oddball's minutes held */
	ARENA_TIMED_SCORE = 5,
};

/* the gametype's motion sensor: other players on it or not (the variant's
draw object in motion sensor bit, and the PC options' RADAR PLAYERS from
it: game_variant_options_default) */
enum
{
	ARENA_RADAR_STOCK = 0,
	ARENA_RADAR_ON,
	ARENA_RADAR_OFF,
};

/* the vehicle sets a gametype picks Halo 1: NHE's mode with (game_engine.c's
_game_engine_vehicles_*: NHE's maps create a ghost, a warthog and a scorpion
off the map and read which the set keeps; notes: ~/src/nhe-re/REPORT.md) */
enum
{
	/* (the stock gametype's own) */
	ARENA_VEHICLES_STOCK = -1,
	/* all: NHE's TRAINING (spawn markers, random zones, the countdown) */
	ARENA_VEHICLES_DEFAULT = 0,
	/* none: NHE's VANILLA (no timer) */
	ARENA_VEHICLES_NONE,
	/* warthogs: NHE & TIMER */
	ARENA_VEHICLES_WARTHOG,
	/* ghosts: TIMER ONLY (the talking and on-screen timers) */
	ARENA_VEHICLES_GHOST,
	/* scorpions: NHE & POWERUPS (the timers and the powerups called) */
	ARENA_VEHICLES_TANK,
};

/* ---------- structures */

struct arena_gametype
{
	/* (ASCII: the record's line, and the gametype's name) */
	char name[ARENA_GAMETYPE_NAME_LENGTH];
	struct game_variant *(*build)(struct game_variant *variant);
	unsigned long flags;
	/* score to win; 0: the stock gametype's */
	long score_to_win;
	/* the PC options' time limit, minutes; 0: none */
	short time_limit;
	/* ticks; ARENA_STOCK: the stock gametype's */
	long respawn_time;
	long suicide_penalty;
	/* ARENA_RADAR_* */
	short radar;
	/* the vehicle set (ARENA_VEHICLES_*): on NHE's maps, its mode */
	long vehicle_set;
	/* the PC options' loadout: TRUE, a pistol in hand and an assault rifle
	(AE's); FALSE, the weapon set's (NHE's: an assault rifle in hand) */
	boolean custom_loadout;
};

/* ---------- globals */

/* the AE gametypes: the stock gametype each is built from, and the
gametype options it adds (game_engine.h's universal_variant flags) */
static struct arena_gametype const arena_gametypes[] =
{
	/* AE's casual set: TIMERS, the motion sensor, 15 minutes, the stock
	respawns. The free for all ones say FFA (AE PRO FFA too); the rest
	are team games */
	{ "AE FFA SLAY", build_game_variant_slayer, ARENA_GAMETYPE_FLAGS | ARENA_CASUAL_FLAGS,
		ARENA_FFA_SCORE, ARENA_TIME_LIMIT, ARENA_STOCK, ARENA_STOCK, ARENA_RADAR_ON, ARENA_VEHICLES_STOCK, TRUE },
	{ "AE TEAM SLY", build_game_variant_team_slayer, ARENA_GAMETYPE_FLAGS | ARENA_CASUAL_FLAGS,
		ARENA_TEAM_SLAYER_SCORE, ARENA_TIME_LIMIT, ARENA_STOCK, ARENA_STOCK, ARENA_RADAR_ON, ARENA_VEHICLES_STOCK, TRUE },
	{ "AE CTF", build_game_variant_ctf, ARENA_GAMETYPE_FLAGS | ARENA_CASUAL_FLAGS,
		ARENA_CTF_SCORE, ARENA_TIME_LIMIT, ARENA_STOCK, ARENA_STOCK, ARENA_RADAR_ON, ARENA_VEHICLES_STOCK, TRUE },
	{ "AE KING", build_game_variant_team_king, ARENA_GAMETYPE_FLAGS | ARENA_CASUAL_FLAGS,
		ARENA_TIMED_SCORE, ARENA_TIME_LIMIT, ARENA_STOCK, ARENA_STOCK, ARENA_RADAR_ON, ARENA_VEHICLES_STOCK, TRUE },
	{ "AE FFA BALL", build_game_variant_oddball, ARENA_GAMETYPE_FLAGS | ARENA_CASUAL_FLAGS,
		ARENA_TIMED_SCORE, ARENA_TIME_LIMIT, ARENA_STOCK, ARENA_STOCK, ARENA_RADAR_ON, ARENA_VEHICLES_STOCK, TRUE },
	/* the casual set for two against two (split-screen on one machine):
	the same rules; the host's player limit (4) is a server setting, not
	the gametype's */
	{ "AE 2V2 SLY", build_game_variant_team_slayer, ARENA_GAMETYPE_FLAGS | ARENA_CASUAL_FLAGS,
		25, ARENA_TIME_LIMIT, ARENA_STOCK, ARENA_STOCK, ARENA_RADAR_ON, ARENA_VEHICLES_STOCK, TRUE },
	{ "AE 2V2 CTF", build_game_variant_ctf, ARENA_GAMETYPE_FLAGS | ARENA_CASUAL_FLAGS,
		ARENA_CTF_SCORE, ARENA_TIME_LIMIT, ARENA_STOCK, ARENA_STOCK, ARENA_RADAR_ON, ARENA_VEHICLES_STOCK, TRUE },
	{ "AE 2V2 KING", build_game_variant_team_king, ARENA_GAMETYPE_FLAGS | ARENA_CASUAL_FLAGS,
		ARENA_TIMED_SCORE, ARENA_TIME_LIMIT, ARENA_STOCK, ARENA_STOCK, ARENA_RADAR_ON, ARENA_VEHICLES_STOCK, TRUE },
	{ "AE 2V2 BALL", build_game_variant_team_oddball, ARENA_GAMETYPE_FLAGS | ARENA_CASUAL_FLAGS,
		ARENA_TIMED_SCORE, ARENA_TIME_LIMIT, ARENA_STOCK, ARENA_STOCK, ARENA_RADAR_ON, ARENA_VEHICLES_STOCK, TRUE },
	/* TRAINING: free for all slayer with no practical score limit, the
	item timers and TRAINING's waypoints */
	{ "AE TRAINING", build_game_variant_slayer, ARENA_GAMETYPE_FLAGS | ARENA_TRAINING_FLAGS,
		ARENA_TRAINING_SCORE_TO_WIN, 0, ARENA_STOCK, ARENA_STOCK, ARENA_RADAR_STOCK, ARENA_VEHICLES_DEFAULT, TRUE },
	/* AE PRO, modern NHE: Beach LAN's 5 second respawn and suicide penalty,
	no motion sensor (but free for all's), no TIMERS; the slayers without a
	time limit, the objective games 15 minutes; team King and Oddball */
	{ "AE PRO FFA", build_game_variant_slayer, ARENA_GAMETYPE_FLAGS,
		ARENA_FFA_SCORE, 0, ARENA_COMPETITIVE_RESPAWN, ARENA_COMPETITIVE_RESPAWN, ARENA_RADAR_ON,
		ARENA_VEHICLES_STOCK, TRUE },
	{ "AE PRO TS", build_game_variant_team_slayer, ARENA_GAMETYPE_FLAGS,
		ARENA_TEAM_SLAYER_SCORE, 0, ARENA_COMPETITIVE_RESPAWN, ARENA_COMPETITIVE_RESPAWN, ARENA_RADAR_OFF,
		ARENA_VEHICLES_STOCK, TRUE },
	{ "AE PRO CTF", build_game_variant_ctf, ARENA_GAMETYPE_FLAGS,
		ARENA_CTF_SCORE, ARENA_TIME_LIMIT, ARENA_COMPETITIVE_RESPAWN, ARENA_COMPETITIVE_RESPAWN, ARENA_RADAR_OFF,
		ARENA_VEHICLES_STOCK, TRUE },
	{ "AE PRO KING", build_game_variant_team_king, ARENA_GAMETYPE_FLAGS,
		ARENA_TIMED_SCORE, ARENA_TIME_LIMIT, ARENA_COMPETITIVE_RESPAWN, ARENA_COMPETITIVE_RESPAWN, ARENA_RADAR_OFF,
		ARENA_VEHICLES_STOCK, TRUE },
	{ "AE PRO BALL", build_game_variant_team_oddball, ARENA_GAMETYPE_FLAGS,
		ARENA_TIMED_SCORE, ARENA_TIME_LIMIT, ARENA_COMPETITIVE_RESPAWN, ARENA_COMPETITIVE_RESPAWN, ARENA_RADAR_OFF,
		ARENA_VEHICLES_STOCK, TRUE },
	/* Halo 1: NHE's competitive play (the mods/NHE maps): stock rules (fall
	damage, classic health) with NHE's NO SPREAD and PRE-GAME COUNTDOWN, its
	mode from the vehicle set; on stock maps the set only picks the
	vehicles. 1V1, 2V2 TS, CTF and POWERUP are Beach LAN 15's (5 second
	respawn and suicide penalty, no motion sensor, no time limit: NHE's
	timer counts up instead) with NHE & TIMER, POWERUP NHE & POWERUPS.
	PRACTICE is race, which NHE's maps turn into practice (every weapon and
	powerup each 30 seconds). */
	{ "NHE 1V1", build_game_variant_slayer, NHE_GAMETYPE_FLAGS | NHE_RULES_FLAGS,
		ARENA_FFA_SCORE, 0, ARENA_COMPETITIVE_RESPAWN, ARENA_COMPETITIVE_RESPAWN, ARENA_RADAR_OFF,
		ARENA_VEHICLES_WARTHOG, FALSE },
	{ "NHE 2V2 TS", build_game_variant_team_slayer, NHE_GAMETYPE_FLAGS | NHE_RULES_FLAGS,
		ARENA_TEAM_SLAYER_SCORE, 0, ARENA_COMPETITIVE_RESPAWN, ARENA_COMPETITIVE_RESPAWN, ARENA_RADAR_OFF,
		ARENA_VEHICLES_WARTHOG, FALSE },
	{ "NHE CTF", build_game_variant_ctf, NHE_GAMETYPE_FLAGS | NHE_RULES_FLAGS,
		ARENA_CTF_SCORE, 0, ARENA_COMPETITIVE_RESPAWN, ARENA_COMPETITIVE_RESPAWN, ARENA_RADAR_OFF,
		ARENA_VEHICLES_WARTHOG, FALSE },
	{ "NHE POWERUP", build_game_variant_team_slayer, NHE_GAMETYPE_FLAGS | NHE_RULES_FLAGS,
		ARENA_TEAM_SLAYER_SCORE, 0, ARENA_COMPETITIVE_RESPAWN, ARENA_COMPETITIVE_RESPAWN, ARENA_RADAR_OFF,
		ARENA_VEHICLES_TANK, FALSE },
	{ "NHE VANILLA", build_game_variant_team_slayer, NHE_GAMETYPE_FLAGS,
		0, 0, ARENA_STOCK, ARENA_STOCK, ARENA_RADAR_STOCK, ARENA_VEHICLES_NONE, FALSE },
	{ "NHE TRAIN", build_game_variant_slayer, NHE_GAMETYPE_FLAGS | NHE_RULES_FLAGS,
		ARENA_TRAINING_SCORE_TO_WIN, 0, ARENA_STOCK, ARENA_STOCK, ARENA_RADAR_STOCK, ARENA_VEHICLES_DEFAULT, FALSE },
	{ "PRACTICE", build_game_variant_race,
		NHE_GAMETYPE_FLAGS | GAME_VARIANT_NO_SPREAD_NHE | FLAG(_game_variant_practice_bit),
		0, 0, ARENA_STOCK, ARENA_STOCK, ARENA_RADAR_STOCK, ARENA_VEHICLES_GHOST, FALSE },
};

/* the AE gametypes seeded before under other names (free for all ones
whose names did not say so) */
static struct
{
	char const *old_name;
	char const *name;
} const arena_gametype_renames[] =
{
	{ "AE SLAYER", "AE FFA SLAY" },
	{ "AE ODDBALL", "AE FFA BALL" },
};

static char const arena_gametypes_record_path[] = "z:\\saved\\playlists\\arena_gametypes.txt";

/* ---------- prototypes */

static boolean arena_gametype_write(
	struct arena_gametype const *gametype,
	boolean *written);
static void arena_gametype_build(
	struct arena_gametype const *gametype,
	wchar_t const *name,
	struct game_variant *variant,
	struct game_variant_options *options);
static void arena_gametype_wide_name(
	char const *name,
	wchar_t wide_name[ARENA_GAMETYPE_NAME_LENGTH]);
static boolean arena_gametypes_record_add(
	char *record,
	unsigned long *record_length,
	char const *name);
static void arena_gametypes_rename(
	char *record,
	unsigned long *record_length,
	boolean *record_changed,
	boolean *written);
static int arena_gametype_name_compare(
	wchar_t const *a,
	wchar_t const *b);
static void arena_gametype_log(
	struct arena_gametype const *gametype,
	struct game_variant const *variant,
	struct game_variant_options const *options);

/* ---------- public code */

boolean arena_gametypes_seed(
	void)
{
	struct file_reference file;
	/* the record, after a newline (each name is looked for as "\nNAME\n") */
	char record[ARENA_GAMETYPES_RECORD_SIZE + 2];
	unsigned long record_length = 1;
	boolean record_changed = FALSE;
	boolean written = FALSE;
	short index;

	csmemset(record, 0, sizeof(record));
	record[0] = '\n';
	if (file_reference_create_from_path(&file, arena_gametypes_record_path, FALSE) &&
		file_exists(&file) &&
		file_open(&file, FLAG(_permission_read_bit)))
	{
		unsigned long length = file_get_eof(&file);

		if (length <= ARENA_GAMETYPES_RECORD_SIZE && file_read(&file, length, record + 1))
		{
			unsigned long character;

			record_length = 1 + length;
			record[record_length] = 0;
			/* (a record edited by hand: its line ends, and its last line's) */
			for (character = 1; character < record_length; character++)
			{
				if (record[character] == '\r')
					record[character] = '\n';
			}
			if (record[record_length - 1] != '\n')
			{
				record[record_length++] = '\n';
				record[record_length] = 0;
			}
		}
		file_close(&file);
	}

	/* (those seeded under their old names: renamed if unchanged) */
	arena_gametypes_rename(record, &record_length, &record_changed, &written);

	for (index = 0; index < NUMBEROF(arena_gametypes); index++)
	{
		struct arena_gametype const *gametype = &arena_gametypes[index];
		char line[ARENA_GAMETYPE_NAME_LENGTH + 2];

		_snprintf(line, sizeof(line), "\n%s\n", gametype->name);
		line[sizeof(line) - 1] = 0;
		/* (seeded before: deleted, renamed or edited since, it stays so) */
		if (strstr(record, line))
			continue;
		if (!arena_gametype_write(gametype, &written))
			continue;
		if (arena_gametypes_record_add(record, &record_length, gametype->name))
			record_changed = TRUE;
	}

	if (record_changed &&
		file_reference_create_from_path(&file, arena_gametypes_record_path, FALSE) &&
		(file_exists(&file) || file_create(&file)) &&
		file_open(&file, FLAG(_permission_write_bit)))
	{
		file_set_eof(&file, 0);
		file_write(&file, record_length - 1, record + 1);
		file_close(&file);
	}

	return written;
}

/* the custom gametypes among count gametypes (playlist_profile.c's list, the
built-in ones read only) in order: the seeded ones by set (AE, AE COMP,
Halo 1: NHE's, TRAINING), each in the names table's order (an old name
beside its successor), then the player's own, alphabetical (case aside);
they take the places the custom ones had, so the built-in ones keep theirs
and their order */
void arena_gametypes_sort(
	word count,
	long *indices)
{
	struct arena_sort_entry
	{
		long index;
		short group;
		short order;
		wchar_t name[ARENA_GAMETYPE_NAME_LENGTH * 2];
	};
	/* (static, not on the stack: one list is sorted at a time, the
	menus' or the start's file system thread's, which the menus wait for) */
	static struct arena_sort_entry entries[ARENA_GAMETYPES_MAXIMUM_SAVED];
	short places[ARENA_GAMETYPES_MAXIMUM_SAVED];
	short entry_count = 0;
	short index;

	for (index = 0; index < (short)count && entry_count < ARENA_GAMETYPES_MAXIMUM_SAVED; index++)
	{
		struct arena_sort_entry *entry = &entries[entry_count];
		wchar_t display_name[MAX_GAMENAME];

		if (indices[index] == NONE || TEST_FLAG(indices[index], _saved_game_file_index_read_only_bit))
			continue;
		display_name[0] = 0;
		playlist_profile_get_display_name(indices[index], display_name);
		ustrncpy(entry->name, display_name, NUMBEROF(entry->name) - 1);
		entry->name[NUMBEROF(entry->name) - 1] = 0;
		entry->index = indices[index];
		{
			struct arena_gametype_info info;

			/* (the sets in their order, then the player's own) */
			if (arena_gametype_info(entry->name, &info))
			{
				entry->group = info.set;
				entry->order = info.order;
			}
			else
			{
				entry->group = NUMBER_OF_ARENA_GAMETYPE_SETS;
				entry->order = 0;
			}
		}
		places[entry_count++] = index;
	}

	/* (an insertion sort, which keeps equal names in the order they were) */
	for (index = 1; index < entry_count; index++)
	{
		struct arena_sort_entry entry = entries[index];
		short other = index;

		while (other > 0 &&
			(entries[other - 1].group > entry.group ||
			(entries[other - 1].group == entry.group && entries[other - 1].order > entry.order) ||
			(entries[other - 1].group == entry.group && entries[other - 1].order == entry.order &&
			arena_gametype_name_compare(entries[other - 1].name, entry.name) > 0)))
		{
			entries[other] = entries[other - 1];
			other--;
		}
		entries[other] = entry;
	}

	for (index = 0; index < entry_count; index++)
		indices[places[index]] = entries[index].index;

	return;
}

/* the seeded gametype of that stored name still exactly as this build seeds
it: its file's variant and PC options (FALSE: edited, not on disk, or not
one of this build's seeds) */
boolean arena_gametype_as_seeded(
	wchar_t const *stored_name)
{
	long saved[ARENA_GAMETYPES_MAXIMUM_SAVED];
	word saved_count = NUMBEROF(saved);
	struct arena_gametype const *gametype = NULL;
	wchar_t wide_name[ARENA_GAMETYPE_NAME_LENGTH];
	short index;

	if (!stored_name)
		return FALSE;
	for (index = 0; index < NUMBEROF(arena_gametypes) && !gametype; index++)
	{
		arena_gametype_wide_name(arena_gametypes[index].name, wide_name);
		if (!arena_gametype_name_compare(stored_name, wide_name))
			gametype = &arena_gametypes[index];
	}
	if (!gametype)
		return FALSE;
	saved_game_files_enumerate_available_to_local_player_index(NONE,
		_saved_game_file_type_game_variant, &saved_count, saved, FALSE);
	for (index = 0; index < (short)saved_count; index++)
	{
		wchar_t display_name[MAX_GAMENAME];
		struct game_variant variant;
		struct game_variant_options options;

		display_name[0] = 0;
		if (!playlist_profile_get_display_name(saved[index], display_name) ||
			arena_gametype_name_compare(display_name, wide_name))
		{
			continue;
		}
		/* (the name as seeded: NUL-filled) */
		arena_gametype_build(gametype, wide_name, &variant, &options);
		return playlist_profile_matches(saved[index], &variant, &options);
	}
	return FALSE;
}

/* ---------- private code */

/* a name's line added to the record, if it fits: TRUE when added */
static boolean arena_gametypes_record_add(
	char *record,
	unsigned long *record_length,
	char const *name)
{
	unsigned long line_length = (unsigned long)strlen(name) + 1;

	if (*record_length + line_length > ARENA_GAMETYPES_RECORD_SIZE)
		return FALSE;
	csmemcpy(record + *record_length, name, line_length - 1);
	record[*record_length + line_length - 1] = '\n';
	*record_length += line_length;
	record[*record_length] = 0;
	return TRUE;
}

/* the AE gametypes seeded under an old name (in the record, the new name
not yet): renamed in place while the file is exactly as seeded (the new
name recorded, so not seeded again); the player's changed one kept, the new
one then seeded beside it; one the player deleted (or renamed) stays so,
its new name recorded unseeded */
static void arena_gametypes_rename(
	char *record,
	unsigned long *record_length,
	boolean *record_changed,
	boolean *written)
{
	long saved[ARENA_GAMETYPES_MAXIMUM_SAVED];
	word saved_count = 0;
	short rename_index;

	for (rename_index = 0; rename_index < NUMBEROF(arena_gametype_renames); rename_index++)
	{
		char const *old_name = arena_gametype_renames[rename_index].old_name;
		char const *new_name = arena_gametype_renames[rename_index].name;
		struct arena_gametype const *gametype = NULL;
		char old_line[ARENA_GAMETYPE_NAME_LENGTH + 2];
		char new_line[ARENA_GAMETYPE_NAME_LENGTH + 2];
		wchar_t old_wide[ARENA_GAMETYPE_NAME_LENGTH];
		wchar_t new_wide[ARENA_GAMETYPE_NAME_LENGTH];
		long profile_index = NONE;
		struct game_variant variant;
		struct game_variant_options options;
		short index;

		_snprintf(old_line, sizeof(old_line), "\n%s\n", old_name);
		old_line[sizeof(old_line) - 1] = 0;
		_snprintf(new_line, sizeof(new_line), "\n%s\n", new_name);
		new_line[sizeof(new_line) - 1] = 0;
		if (!strstr(record, old_line) || strstr(record, new_line))
			continue;
		for (index = 0; index < NUMBEROF(arena_gametypes); index++)
		{
			if (!strcmp(arena_gametypes[index].name, new_name))
				gametype = &arena_gametypes[index];
		}
		if (!gametype)
			continue;

		/* (the custom gametypes, listed once) */
		if (!saved_count)
		{
			saved_count = NUMBEROF(saved);
			saved_game_files_enumerate_available_to_local_player_index(NONE,
				_saved_game_file_type_game_variant, &saved_count, saved, FALSE);
		}
		arena_gametype_wide_name(old_name, old_wide);
		arena_gametype_wide_name(new_name, new_wide);
		for (index = 0; index < (short)saved_count && profile_index == NONE; index++)
		{
			wchar_t display_name[MAX_GAMENAME];

			display_name[0] = 0;
			if (playlist_profile_get_display_name(saved[index], display_name) &&
				!ustrcmp(display_name, old_wide))
			{
				profile_index = saved[index];
			}
		}

		if (profile_index == NONE)
		{
			/* (deleted or renamed by the player: not brought back) */
			error(_error_silent, "arena gametype '%s' (once '%s') not seeded: the player removed '%s'",
				new_name, old_name, old_name);
			if (arena_gametypes_record_add(record, record_length, new_name))
				*record_changed = TRUE;
			continue;
		}

		arena_gametype_build(gametype, old_wide, &variant, &options);
		if (!playlist_profile_matches(profile_index, &variant, &options))
		{
			/* (the player's own now: kept, and the new one seeded beside it) */
			error(_error_silent, "arena gametype '%s' kept as it is (changed since it was seeded); '%s' seeded beside it",
				old_name, new_name);
			continue;
		}
		if (!saved_game_file_name_unique(new_wide))
		{
			/* (a saved game has the new name: the seeding leaves both) */
			error(_error_silent, "arena gametype '%s' not renamed: a saved game named '%s' exists", old_name, new_name);
			continue;
		}

		{
			char old_directory[MAXIMUM_FILENAME_LENGTH + 1];
			char last_used[MAXIMUM_FILENAME_LENGTH + 1];
			boolean was_last_used = FALSE;

			/* (the gametype used last, if this one: still so after) */
			if (saved_game_file_get_path_to_enclosing_directory(profile_index, old_directory) &&
				saved_game_file_retrieve_last_used_multiplayer_variant_directory(last_used) &&
				!strcmp(old_directory, last_used))
			{
				was_last_used = TRUE;
			}

			/* (renamed as the PC menus' rename does: the variant saved under
			its new name, which moves its saved game's name with it) */
			csmemcpy(variant.human_readable_game_description, new_wide, sizeof(new_wide));
			playlist_profile_save_with_options(profile_index, &variant, &options);
			playlist_profile_wait_for_write();
			/* (the files changed either way: listed again) */
			*written = TRUE;

			/* (a write that failed deletes the gametype, playlist_profile.c's
			write thread, or leaves it under its old name: the new name is
			recorded only once a saved game has it, else the seeding below
			writes a new one) */
			if (saved_game_file_name_unique(new_wide))
			{
				error(_error_silent, "failed to rename arena gametype '%s' to '%s'; '%s' seeded instead",
					old_name, new_name, new_name);
				continue;
			}

			if (was_last_used)
			{
				char new_directory[MAXIMUM_FILENAME_LENGTH + 1];

				if (saved_game_file_get_path_to_enclosing_directory(profile_index, new_directory))
					saved_game_file_remember_last_used_multiplayer_variant_directory(new_directory);
			}
		}
		error(_error_silent, "renamed arena gametype '%s' to '%s'", old_name, new_name);
		if (arena_gametypes_record_add(record, record_length, new_name))
			*record_changed = TRUE;
	}

	return;
}

/* names compared as the lists sort them: letters' case aside */
static int arena_gametype_name_compare(
	wchar_t const *a,
	wchar_t const *b)
{
	for (;; a++, b++)
	{
		wchar_t x = *a >= 'a' && *a <= 'z' ? (wchar_t)(*a - 'a' + 'A') : *a;
		wchar_t y = *b >= 'a' && *b <= 'z' ? (wchar_t)(*b - 'a' + 'A') : *b;

		if (x != y)
			return x < y ? -1 : 1;
		if (!x)
			return 0;
	}
}

/* an ASCII name as a gametype's (wide, NUL-filled) */
static void arena_gametype_wide_name(
	char const *name,
	wchar_t wide_name[ARENA_GAMETYPE_NAME_LENGTH])
{
	short index;

	csmemset(wide_name, 0, ARENA_GAMETYPE_NAME_LENGTH * sizeof(wchar_t));
	for (index = 0; index < ARENA_GAMETYPE_NAME_LENGTH - 1 && name[index]; index++)
		wide_name[index] = (wchar_t)name[index];

	return;
}

/* a gametype saved as the PC menus' Edit Gametypes saves one
(player_ui_save_profile): a new custom gametype file
(playlist_profile_new), then the variant and its PC options
(playlist_profile_save_with_options, with the checksum and the 'GPVO'
options block). TRUE when it is on disk (written now, or already there by
that name: left as it is); *written set when written now */
static boolean arena_gametype_write(
	struct arena_gametype const *gametype,
	boolean *written)
{
	struct game_variant variant;
	struct game_variant_options options;
	wchar_t name[ARENA_GAMETYPE_NAME_LENGTH];
	long profile_index;

	arena_gametype_wide_name(gametype->name, name);

	/* (a saved game of that name, a gametype or a player profile: not
	ours to replace) */
	if (!saved_game_file_name_unique(name))
	{
		error(_error_silent, "arena gametype '%s' not seeded: a saved game of that name exists", gametype->name);
		return TRUE;
	}

	arena_gametype_build(gametype, name, &variant, &options);
	profile_index = playlist_profile_new(NONE, name);
	if (profile_index == NONE)
	{
		error(_error_silent, "failed to create arena gametype '%s'", gametype->name);
		return FALSE;
	}
	playlist_profile_save_with_options(profile_index, &variant, &options);
	/* (and written before the next gametype's file is made: the write's
	thread opens the saved games' mapfile, which making a file
	(create_enumerated_saved_game_file's count_enumerated_profiles_in_mapfile)
	resets without taking its mutex; a player saving gametypes is never
	that quick) */
	playlist_profile_wait_for_write();
	*written = TRUE;
	arena_gametype_log(gametype, &variant, &options);

	return TRUE;
}

/* a gametype's variant and PC options as seeded, named name (the variant
before a save's clean-up) */
static void arena_gametype_build(
	struct arena_gametype const *gametype,
	wchar_t const *name,
	struct game_variant *variant_out,
	struct game_variant_options *options_out)
{
	struct game_variant temporary;
	struct game_variant variant;
	struct game_variant_options options;

	variant = *gametype->build(&temporary);
	csmemcpy(variant.human_readable_game_description, name, ARENA_GAMETYPE_NAME_LENGTH * sizeof(wchar_t));
	/* (a custom gametype, not one of the system's defaults: playlist_profile.c's
	_game_variant_is_system_default_bit, and their string index above it) */
	variant.flags = 0;
	variant.universal_variant.flags |= gametype->flags;
	if (gametype->score_to_win)
		variant.universal_variant.score_to_win = gametype->score_to_win;
	if (gametype->respawn_time != ARENA_STOCK)
		variant.universal_variant.respawn_time = gametype->respawn_time;
	if (gametype->suicide_penalty != ARENA_STOCK)
		variant.universal_variant.suicide_penalty = gametype->suicide_penalty;
	if (gametype->radar != ARENA_RADAR_STOCK)
	{
		SET_FLAG(variant.universal_variant.flags, _game_variant_draw_object_in_motion_sensor_bit,
			gametype->radar == ARENA_RADAR_ON);
	}
	if (gametype->vehicle_set != ARENA_VEHICLES_STOCK)
		variant.universal_variant.vehicle_set = gametype->vehicle_set;
	/* (the PC options a new custom gametype gets: both teams' vehicle sets
	the variant's, RADAR PLAYERS from its motion sensor bit) */
	game_variant_options_default(&variant, &options);
	options.time_limit = gametype->time_limit;
	if (gametype->custom_loadout)
	{
		options.loadout = _loadout_custom;
		options.primary_weapon = _loadout_weapon_pistol;
		options.secondary_weapon = _loadout_weapon_assault_rifle;
	}
	*variant_out = variant;
	*options_out = options;

	return;
}

/* a seeded gametype's rules, in the log */
static void arena_gametype_log(
	struct arena_gametype const *gametype,
	struct game_variant const *variant,
	struct game_variant_options const *options)
{
	static char const *const engines[] = { "none", "ctf", "slayer", "oddball", "king", "race", "terminator", "stub" };
	struct universal_variant const *universal = &variant->universal_variant;
	unsigned long flags = universal->flags;
	long engine = variant->game_engine_index;

	error(_error_silent, "seeded arena gametype '%s': %s%s, score to win %ld, time limit %d min, "
		"respawn %ld s, suicide penalty %ld s, motion sensor %s, vehicle set %ld, loadout %s, "
		"equipment %s, health %s, fall damage %s, no spread %s, pre-game countdown %s, timers %s, "
		"training %s, practice %s",
		gametype->name,
		engine >= 0 && engine < (long)NUMBEROF(engines) ? engines[engine] : "?",
		universal->teams ? " (teams)" : "",
		universal->score_to_win,
		(int)options->time_limit,
		universal->respawn_time / TICKS_PER_SECOND,
		universal->suicide_penalty / TICKS_PER_SECOND,
		TEST_FLAG(flags, _game_variant_draw_object_in_motion_sensor_bit) ? "on" : "off",
		universal->vehicle_set,
		options->loadout == _loadout_custom ? "pistol + assault rifle" : "the weapon set's",
		TEST_FLAG(flags, _game_variant_generic_starting_equipment_bit) ? "generic" : "the map's",
		game_variant_health_style_name(flags),
		TEST_FLAG(flags, _game_variant_no_falling_damage_bit) ? "off" : "on",
		game_variant_no_spread_name(flags),
		TEST_FLAG(flags, _game_variant_pregame_countdown_bit) ? "on" : "off",
		TEST_FLAG(flags, _game_variant_item_timers_bit) ? "on" : "off",
		TEST_FLAG(flags, _game_variant_training_bit) ? "on" : "off",
		TEST_FLAG(flags, _game_variant_practice_bit) ? "on" : "off");

	return;
}
