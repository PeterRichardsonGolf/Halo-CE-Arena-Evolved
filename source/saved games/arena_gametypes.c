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

#include <string.h>

/* ---------- constants */

enum
{
	/* a gametype's name: 11 characters and its NUL (game_variant's
	human_readable_game_description; playlist_profile.c's
	MAXIMUM_GAME_VARIANT_NAME_LENGTH) */
	ARENA_GAMETYPE_NAME_LENGTH = 12,
	/* the record of those seeded: their names, a line each */
	ARENA_GAMETYPES_RECORD_SIZE = 1024,
};

/* what every AE gametype adds to its stock one: NHE's rules (NO SPREAD, the
PRE-GAME COUNTDOWN), no landing damage, HALO 2 health (health back as the
shields recharge), and the generic starting equipment (the stock gametypes
take the map's, Blood Gulch's a plasma pistol); its loadout is a pistol in
hand and an assault rifle (arena_gametype.custom_loadout) */
#define ARENA_GAMETYPE_FLAGS \
	(FLAG(_game_variant_no_falling_damage_bit) | \
	(_health_style_halo2 << _game_variant_health_style_first_bit) | \
	FLAG(_game_variant_generic_starting_equipment_bit) | \
	NHE_RULES_FLAGS)

/* NHE's competitive rules, as ARENA OPTIONS has them: NO SPREAD and the
PRE-GAME COUNTDOWN (on NHE's maps their scripts count down instead) */
#define NHE_RULES_FLAGS \
	(FLAG(_game_variant_no_spread_bit) | \
	FLAG(_game_variant_pregame_countdown_bit))

/* NHE's: the generic starting equipment (stock rules otherwise); each adds
NHE_RULES_FLAGS but VANILLA (NHE's Vanilla had neither) */
#define NHE_GAMETYPE_FLAGS FLAG(_game_variant_generic_starting_equipment_bit)

/* TRAINING's aids: the item TIMERS and TRAINING's waypoints */
#define ARENA_TRAINING_FLAGS \
	(FLAG(_game_variant_item_timers_bit) | \
	FLAG(_game_variant_training_bit))

/* AE TRAINING's score to win: the most the gametype editor offers (its
KILLS TO WIN, menu_functions.c), as near "no limit" as a variant goes
(slayer ends the game at a score >= score_to_win, so 0 would end it at
once); the time limit is the PC options' default, none */
#define ARENA_TRAINING_SCORE_TO_WIN 500

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
	{ "AE SLAYER", build_game_variant_slayer, ARENA_GAMETYPE_FLAGS, 0, ARENA_VEHICLES_STOCK, TRUE },
	{ "AE TEAM SLY", build_game_variant_team_slayer, ARENA_GAMETYPE_FLAGS, 0, ARENA_VEHICLES_STOCK, TRUE },
	{ "AE CTF", build_game_variant_ctf, ARENA_GAMETYPE_FLAGS, 0, ARENA_VEHICLES_STOCK, TRUE },
	{ "AE KING", build_game_variant_team_king, ARENA_GAMETYPE_FLAGS, 0, ARENA_VEHICLES_STOCK, TRUE },
	{ "AE ODDBALL", build_game_variant_oddball, ARENA_GAMETYPE_FLAGS, 0, ARENA_VEHICLES_STOCK, TRUE },
	/* TRAINING: free for all slayer with no practical score limit, the
	item timers and TRAINING's waypoints */
	{ "AE TRAINING", build_game_variant_slayer, ARENA_GAMETYPE_FLAGS | ARENA_TRAINING_FLAGS,
		ARENA_TRAINING_SCORE_TO_WIN, ARENA_VEHICLES_DEFAULT, TRUE },
	/* Halo 1: NHE's competitive play (the mods/NHE maps): stock rules (fall
	damage, classic health) with NHE's NO SPREAD and PRE-GAME COUNTDOWN, its
	mode from the vehicle set; on stock maps the set only picks the
	vehicles. PRACTICE is race, which NHE's maps turn into
	practice (every weapon and powerup each 30 seconds). */
	{ "NHE 1V1", build_game_variant_slayer, NHE_GAMETYPE_FLAGS | NHE_RULES_FLAGS, 0, ARENA_VEHICLES_GHOST, FALSE },
	{ "NHE 2V2 TS", build_game_variant_team_slayer, NHE_GAMETYPE_FLAGS | NHE_RULES_FLAGS, 0, ARENA_VEHICLES_GHOST, FALSE },
	{ "NHE CTF", build_game_variant_ctf, NHE_GAMETYPE_FLAGS | NHE_RULES_FLAGS, 0, ARENA_VEHICLES_GHOST, FALSE },
	{ "NHE POWERUP", build_game_variant_team_slayer, NHE_GAMETYPE_FLAGS | NHE_RULES_FLAGS, 0, ARENA_VEHICLES_TANK, FALSE },
	{ "NHE VANILLA", build_game_variant_team_slayer, NHE_GAMETYPE_FLAGS, 0, ARENA_VEHICLES_NONE, FALSE },
	{ "NHE TRAIN", build_game_variant_slayer, NHE_GAMETYPE_FLAGS | NHE_RULES_FLAGS, ARENA_TRAINING_SCORE_TO_WIN, ARENA_VEHICLES_DEFAULT, FALSE },
	{ "PRACTICE", build_game_variant_race,
		NHE_GAMETYPE_FLAGS | FLAG(_game_variant_no_spread_bit) | FLAG(_game_variant_practice_bit), 0, ARENA_VEHICLES_GHOST, FALSE },
};

static char const arena_gametypes_record_path[] = "z:\\saved\\playlists\\arena_gametypes.txt";

/* ---------- prototypes */

static boolean arena_gametype_write(
	struct arena_gametype const *gametype,
	boolean *written);

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

	for (index = 0; index < NUMBEROF(arena_gametypes); index++)
	{
		struct arena_gametype const *gametype = &arena_gametypes[index];
		char line[ARENA_GAMETYPE_NAME_LENGTH + 2];
		unsigned long line_length;

		_snprintf(line, sizeof(line), "\n%s\n", gametype->name);
		line[sizeof(line) - 1] = 0;
		/* (seeded before: deleted, renamed or edited since, it stays so) */
		if (strstr(record, line))
			continue;
		if (!arena_gametype_write(gametype, &written))
			continue;
		line_length = (unsigned long)strlen(line + 1);
		if (record_length + line_length <= ARENA_GAMETYPES_RECORD_SIZE)
		{
			csmemcpy(record + record_length, line + 1, line_length);
			record_length += line_length;
			record[record_length] = 0;
			record_changed = TRUE;
		}
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

/* ---------- private code */

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
	struct game_variant temporary;
	struct game_variant variant;
	struct game_variant_options options;
	wchar_t name[ARENA_GAMETYPE_NAME_LENGTH];
	long profile_index;
	short index;

	csmemset(name, 0, sizeof(name));
	for (index = 0; index < ARENA_GAMETYPE_NAME_LENGTH - 1 && gametype->name[index]; index++)
		name[index] = (wchar_t)gametype->name[index];

	/* (a saved game of that name, a gametype or a player profile: not
	ours to replace) */
	if (!saved_game_file_name_unique(name))
	{
		error(_error_silent, "arena gametype '%s' not seeded: a saved game of that name exists", gametype->name);
		return TRUE;
	}

	variant = *gametype->build(&temporary);
	csmemcpy(variant.human_readable_game_description, name, sizeof(name));
	/* (a custom gametype, not one of the system's defaults: playlist_profile.c's
	_game_variant_is_system_default_bit, and their string index above it) */
	variant.flags = 0;
	variant.universal_variant.flags |= gametype->flags;
	if (gametype->score_to_win)
		variant.universal_variant.score_to_win = gametype->score_to_win;
	if (gametype->vehicle_set != ARENA_VEHICLES_STOCK)
		variant.universal_variant.vehicle_set = gametype->vehicle_set;
	/* (the PC options a new custom gametype gets: both teams' vehicle sets
	the variant's) */
	game_variant_options_default(&variant, &options);
	if (gametype->custom_loadout)
	{
		options.loadout = _loadout_custom;
		options.primary_weapon = _loadout_weapon_pistol;
		options.secondary_weapon = _loadout_weapon_assault_rifle;
	}

	profile_index = playlist_profile_new(NONE, name);
	if (profile_index == NONE)
	{
		error(_error_silent, "failed to create arena gametype '%s'", gametype->name);
		return FALSE;
	}
	playlist_profile_save_with_options(profile_index, &variant, &options);
	*written = TRUE;
	error(_error_silent, "seeded arena gametype '%s'", gametype->name);

	return TRUE;
}
