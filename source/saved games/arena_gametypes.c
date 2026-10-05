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
	respawns */
	{ "AE SLAYER", build_game_variant_slayer, ARENA_GAMETYPE_FLAGS | ARENA_CASUAL_FLAGS,
		ARENA_FFA_SCORE, ARENA_TIME_LIMIT, ARENA_STOCK, ARENA_STOCK, ARENA_RADAR_ON, ARENA_VEHICLES_STOCK, TRUE },
	{ "AE TEAM SLY", build_game_variant_team_slayer, ARENA_GAMETYPE_FLAGS | ARENA_CASUAL_FLAGS,
		ARENA_TEAM_SLAYER_SCORE, ARENA_TIME_LIMIT, ARENA_STOCK, ARENA_STOCK, ARENA_RADAR_ON, ARENA_VEHICLES_STOCK, TRUE },
	{ "AE CTF", build_game_variant_ctf, ARENA_GAMETYPE_FLAGS | ARENA_CASUAL_FLAGS,
		ARENA_CTF_SCORE, ARENA_TIME_LIMIT, ARENA_STOCK, ARENA_STOCK, ARENA_RADAR_ON, ARENA_VEHICLES_STOCK, TRUE },
	{ "AE KING", build_game_variant_team_king, ARENA_GAMETYPE_FLAGS | ARENA_CASUAL_FLAGS,
		ARENA_TIMED_SCORE, ARENA_TIME_LIMIT, ARENA_STOCK, ARENA_STOCK, ARENA_RADAR_ON, ARENA_VEHICLES_STOCK, TRUE },
	{ "AE ODDBALL", build_game_variant_oddball, ARENA_GAMETYPE_FLAGS | ARENA_CASUAL_FLAGS,
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

static char const arena_gametypes_record_path[] = "z:\\saved\\playlists\\arena_gametypes.txt";

/* ---------- prototypes */

static boolean arena_gametype_write(
	struct arena_gametype const *gametype,
	boolean *written);
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

	profile_index = playlist_profile_new(NONE, name);
	if (profile_index == NONE)
	{
		error(_error_silent, "failed to create arena gametype '%s'", gametype->name);
		return FALSE;
	}
	playlist_profile_save_with_options(profile_index, &variant, &options);
	*written = TRUE;
	arena_gametype_log(gametype, &variant, &options);

	return TRUE;
}

/* a seeded gametype's rules, in the log */
static void arena_gametype_log(
	struct arena_gametype const *gametype,
	struct game_variant const *variant,
	struct game_variant_options const *options)
{
	static char const *const engines[] = { "none", "ctf", "slayer", "oddball", "king", "race", "terminator", "stub" };
	static char const *const health_styles[] = { "classic", "reach", "halo 3", "halo 2" };
	static char const *const no_spread_levels[] = { "off", "nhe", "full" };
	struct universal_variant const *universal = &variant->universal_variant;
	unsigned long flags = universal->flags;
	short no_spread = TEST_FLAG(flags, _game_variant_no_spread_full_bit) ? _no_spread_full :
		TEST_FLAG(flags, _game_variant_no_spread_bit) ? _no_spread_nhe : _no_spread_off;
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
		health_styles[(flags & GAME_VARIANT_HEALTH_STYLE_MASK) >> _game_variant_health_style_first_bit],
		TEST_FLAG(flags, _game_variant_no_falling_damage_bit) ? "off" : "on",
		no_spread_levels[no_spread],
		TEST_FLAG(flags, _game_variant_pregame_countdown_bit) ? "on" : "off",
		TEST_FLAG(flags, _game_variant_item_timers_bit) ? "on" : "off",
		TEST_FLAG(flags, _game_variant_training_bit) ? "on" : "off",
		TEST_FLAG(flags, _game_variant_practice_bit) ? "on" : "off");

	return;
}
