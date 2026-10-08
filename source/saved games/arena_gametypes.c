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

The record's first line is "#revision N", the seeds' revision it was
last brought to (none: 0). Each later revision's migrations
(arena_gametype_migrations) run once, in order: a gametype seeded under an
old name, its file still exactly as that revision's predecessor seeded it
(a frozen copy of its row), is backed up (z:\saved\playlists_backup\
revision_N\) and rewritten in place under its new name from the
revision's frozen new row; one the player changed is kept as it is and the
new one written beside it; one the player deleted stays deleted. The record
is written through a temporary file, so a crash leaves the old one or the
new one, never half of one.

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
#include "saved games/playlist_display_name.h"
#include "saved games/playlist_profile.h"
#include "saved games/saved_game_files.h"
#include "tag_files/files.h"
#include "text/unicode.h"

#include <ctype.h>
#include <stdlib.h>
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
	/* the record of those seeded: their names, a line each, after its
	"#revision N" line. Builds before the migrations (a17102e2) read at
	most 1024 bytes, and one that finds it longer reads it as empty and
	seeds again: the record past ARENA_GAMETYPES_RECORD_OLD_LIMIT logs a
	warning (run only builds since the migrations on a migrated root) */
	ARENA_GAMETYPES_RECORD_SIZE = 2048,
	ARENA_GAMETYPES_RECORD_OLD_LIMIT = 1000,
	/* the most a "#revision N" line takes */
	ARENA_GAMETYPES_REVISION_LINE = 32,
	/* the seeds' revision now (the last of arena_gametype_migrations) */
	ARENA_GAMETYPES_REVISION = 6,
	/* a memory unit holds at most 100 saved games (saved_game_files.c):
	the self-check warns past this many */
	ARENA_GAMETYPES_SAVED_WARNING = 90,
	/* a gametype directory's files backed up (blam.lst, SaveMeta.xbx) */
	ARENA_GAMETYPES_BACKUP_FILES = 8,
	/* the custom gametypes looked through (saved_game_files.c lists at
	most 100 saved games) */
	ARENA_GAMETYPES_MAXIMUM_SAVED = 128,
};

/* what every AE gametype adds to its stock one: AE's rules (NO SPREAD NHE,
the PRE-GAME COUNTDOWN), no landing damage, HALO 2 health (Halo 2's shield and
body timers), and the generic starting equipment (the stock
gametypes take the map's, Blood Gulch's a plasma pistol); its loadout is a
pistol in hand and an assault rifle (arena_gametype.custom_loadout) */
#define ARENA_GAMETYPE_FLAGS \
	(FLAG(_game_variant_no_falling_damage_bit) | \
	(_health_style_halo2 << _game_variant_health_style_first_bit) | \
	FLAG(_game_variant_generic_starting_equipment_bit) | \
	ARENA_RULES_FLAGS)

/* AE's competitive rules, as ARENA OPTIONS has them: NO SPREAD NHE (the
pistol's first shot from rest and the unzoomed sniper rifle exact, as Halo
1: NHE's; owner, 2026-10-08: FULL makes the auto pistol spam too strong;
before revision 5, FULL) and the PRE-GAME COUNTDOWN */
#define ARENA_RULES_FLAGS \
	(GAME_VARIANT_NO_SPREAD_NHE | \
	FLAG(_game_variant_pregame_countdown_bit))

/* NHE's competitive rules, as ARENA OPTIONS has them: NO SPREAD NHE and the
PRE-GAME COUNTDOWN (on NHE's maps their scripts count down instead) */
#define NHE_RULES_FLAGS \
	(GAME_VARIANT_NO_SPREAD_NHE | \
	FLAG(_game_variant_pregame_countdown_bit))

/* NHE's: the generic starting equipment (stock rules otherwise); each adds
NHE_RULES_FLAGS but VANILLA (NHE's Vanilla had neither) */
#define NHE_GAMETYPE_FLAGS FLAG(_game_variant_generic_starting_equipment_bit)

/* Halo 1: NHE's own 23 gametypes (revision 4; notes: NHE 1.0 Gametypes,
typed in, never read from NHE's files): the generic starting equipment,
NHE's rules (NO SPREAD NHE, never FULL; the PRE-GAME COUNTDOWN) and NHE
EXTRAS (all join red, no team swap, the own dead camera, the clock counting
up); no TIMERS, DROP SECONDARY CE, classic health, fall damage */
#define NHE_SET_FLAGS \
	(NHE_GAMETYPE_FLAGS | NHE_RULES_FLAGS | FLAG(_game_variant_nhe_extras_bit))

/* TS PRACTICE's: infinite grenades, PRACTICE MODE (every weapon and
powerup each 30 seconds) and TRAINING's markers (not on NHE's maps, whose
scripts have their own) */
#define NHE_PRACTICE_FLAGS \
	(NHE_SET_FLAGS | FLAG(_game_variant_infinite_grenades_bit) | FLAG(_game_variant_practice_bit) | \
	FLAG(_game_variant_training_bit))

/* DROP SECONDARY ALWAYS (both weapons drop and stay 30 seconds): every AE
gametype's (revision 5) */
#define ARENA_DROP_ALWAYS (_drop_secondary_always << _game_variant_drop_secondary_first_bit)

/* AE's casual gametypes' aids (revision 5; before, the strip only): TIMERS
HUD + WAYPOINTS (the strip and the waypoints through walls) and DROP
SECONDARY ALWAYS */
#define ARENA_CASUAL_FLAGS \
	(FLAG(_game_variant_item_timers_bit) | FLAG(_game_variant_item_waypoints_bit) | ARENA_DROP_ALWAYS)

/* the AE gametypes added later (AE TEAM OB, AE PRACTICE, the special
modes): AE's rules and the casual set's aids */
#define ARENA_LATER_FLAGS (ARENA_GAMETYPE_FLAGS | ARENA_CASUAL_FLAGS)

/* AE COMP's (revision 5): TIMERS LINE OF SIGHT (the power items' waypoints
only while in view; no strip, item calls muted) and DROP SECONDARY ALWAYS;
the objective games also their objective only in line of sight (with nav
points) */
#define ARENA_COMP_FLAGS \
	(ARENA_GAMETYPE_FLAGS | FLAG(_game_variant_item_waypoints_bit) | ARENA_DROP_ALWAYS)
#define ARENA_COMP_OBJECTIVE_FLAGS (ARENA_COMP_FLAGS | FLAG(_game_variant_objective_in_sight_bit))

/* AE COMP's starting frag grenades: 2 (universal_variant.starting_frags; the
game's rule under 5 players is the globals' 4) */
#define ARENA_COMP_STARTING_FRAGS 2

/* SWAT's: those, but classic health (a health pack's) and no shields: a
headshot kills */
#define ARENA_SWAT_FLAGS \
	((ARENA_LATER_FLAGS & ~GAME_VARIANT_HEALTH_STYLE_MASK) | FLAG(_game_variant_no_shields_bit))

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
	/* Halo 1: NHE's respawns and suicide penalties, ticks: 5 seconds, its
	objective gametypes' "hacked" 7.5 (the stock editor cannot set it; the
	gametype editor shows it: menu_functions.c's respawn spinner) and 10 */
	NHE_RESPAWN_5 = 5 * TICKS_PER_SECOND,
	NHE_RESPAWN_7_5 = 225,
	NHE_RESPAWN_10 = 10 * TICKS_PER_SECOND,
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

/* frozen copies of the flags the seeds of revisions 1 and 2 have, as
literals (the macros above may change; a migration compares a file with the
row its predecessor seeded): ARENA_GAMETYPE_FLAGS (no landing damage, HALO
2 health, the generic starting equipment, NO SPREAD FULL, the PRE-GAME
COUNTDOWN) and ARENA_CASUAL_FLAGS (TIMERS) */
#define ARENA_REV1_GAMETYPE_FLAGS 0x01790020UL
#define ARENA_REV1_CASUAL_FLAGS 0x00020000UL
/* (and the NHE seeds' as revisions 1-2 have them: the generic starting
equipment, then NHE_RULES_FLAGS (NO SPREAD NHE, the PRE-GAME COUNTDOWN), and
PRACTICE's NO SPREAD NHE and PRACTICE MODE) */
#define ARENA_REV1_NHE_FLAGS 0x00000020UL
#define ARENA_REV1_NHE_RULES_FLAGS 0x00600020UL
#define ARENA_REV1_NHE_PRACTICE_FLAGS 0x00A00020UL
/* (revision 4's NHE set: NHE_SET_FLAGS (the generic starting equipment, NO
SPREAD NHE, the PRE-GAME COUNTDOWN, NHE EXTRAS) and NHE_PRACTICE_FLAGS (and
infinite grenades, PRACTICE MODE, TRAINING)) */
#define ARENA_REV4_NHE_SET_FLAGS 0x10600020UL
#define ARENA_REV4_NHE_PRACTICE_FLAGS 0x10E40024UL
/* (Task 12's AE seeds as revision 4 seeded them: AE's rules with NO SPREAD
FULL, TIMERS HUD + WAYPOINTS, DROP SECONDARY ALWAYS; AE PRACTICE's with
PRACTICE MODE; AE SWAT's classic health and no shields; and AE TRAINING's,
the timers and TRAINING) */
#define ARENA_REV4_LATER_FLAGS 0x257B0020UL
#define ARENA_REV4_PRACTICE_FLAGS 0x25FB0020UL
#define ARENA_REV4_SWAT_FLAGS 0x25630028UL
#define ARENA_REV4_TRAINING_FLAGS 0x017F0020UL
/* (revision 5's: AE's rules with NO SPREAD NHE; the casual set's aids
(TIMERS HUD + WAYPOINTS, DROP SECONDARY ALWAYS) and the later ones'; AE
TRAINING's (and DROP SECONDARY ALWAYS); AE COMP's (TIMERS LINE OF SIGHT,
DROP SECONDARY ALWAYS) and its objective games' (objective in sight)) */
#define ARENA_REV5_CASUAL_FLAGS 0x247B0020UL
#define ARENA_REV5_PRACTICE_FLAGS 0x24FB0020UL
#define ARENA_REV5_SWAT_FLAGS 0x24630028UL
#define ARENA_REV5_TRAINING_FLAGS 0x207F0020UL
#define ARENA_REV5_COMP_FLAGS 0x24790020UL
#define ARENA_REV5_COMP_OBJECTIVE_FLAGS 0x2C790020UL
/* (revision 6's AE VANILLA and AE POWERUPS: the casual set's, as
ARENA_REV5_CASUAL_FLAGS) */

/* the seed fields whose 0 means the stock gametype's (or AE's default):
a value is given as value + 1 */
#define ARENA_SET(value) ((value) + 1)

/* the objectives indicator (universal_variant.goal_radar, game_engine.c's
enum goal_radar) and the weapon sets (enum game_engine_weapons), as
ARENA_SET values */
enum
{
	ARENA_GOAL_MOTION_TRACKER = ARENA_SET(0),
	ARENA_GOAL_NAV_POINTS = ARENA_SET(1),
	ARENA_GOAL_NONE = ARENA_SET(2),
	ARENA_WEAPONS_PISTOLS = ARENA_SET(1),
	ARENA_WEAPONS_SNIPING = ARENA_SET(4),
	ARENA_WEAPONS_ROCKET_LAUNCHERS = ARENA_SET(6),
	ARENA_WEAPONS_SHOTGUNS = ARENA_SET(7),
	ARENA_WEAPONS_HEAVY = ARENA_SET(13),
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
	(AE's; others with loadout_primary / loadout_secondary); FALSE, the
	weapon set's (NHE's: an assault rifle in hand) */
	boolean custom_loadout;
	/* (the fields below: 0, as before them; rows list them as needed) */
	/* the weapon set: 0 the stock gametype's, else
	ARENA_SET(_game_engine_weapons_*) */
	short weapon_set;
	/* a custom loadout's weapons: 0 AE's (pistol, assault rifle), else
	ARENA_SET(_loadout_weapon_*) */
	byte loadout_primary;
	byte loadout_secondary;
	/* the PC options' NO MAP WEAPONS */
	boolean no_map_weapons;
	/* the PC options' FRIENDLY FIRE: 0 the default (on), else
	ARENA_SET(_friendly_fire_*) */
	short friendly_fire;
	/* the objectives indicator: 0 the stock gametype's, else
	ARENA_SET(goal radar: 0 motion tracker, 1 nav points, 2 none) */
	short goal_radar;
	/* NHE MODE (universal_variant.nhe_mode: enum nhe_mode; 0 BY VEHICLES) */
	byte nhe_mode;
	/* the frag grenades each player starts with (universal_variant.
	starting_frags; 0 the game's rule) */
	byte starting_frags;
	/* anything else, after the rest (a shipped one is never edited: a
	changed one is a new function); NULL for none */
	void (*adjust)(struct game_variant *variant, struct game_variant_options *options);
};

/* a revision's change of a seeded gametype: its frozen rows, the old one
as the revision before seeded it (under its old name) and the new one as
this revision seeds it (under its new name); rows are never edited once a
build ships them */
struct arena_gametype_migration
{
	short revision;
	struct arena_gametype old_row;
	struct arena_gametype new_row;
	/* the old row's file as the builds before seeded it: a SHA-1 (hex) of
	the content signature's key and its 512-byte block (saved_game_file_
	generate_checksum's, xbox_xapi.c; tools/ae_test/golden_seeds_a17102e2.json's
	"block"). The start's self-check builds the old row and compares: a
	build whose pipeline (the stock builders, the PC options' defaults, the
	save's clean-up) changed skips the migration rather than miss every
	file (or match a wrong one) */
	char const *old_block_hash;
};

/* ---------- prototypes */

static void arena_gametype_nhe_slayer(
	struct game_variant *variant,
	struct game_variant_options *options);
static void arena_gametype_nhe_ctf(
	struct game_variant *variant,
	struct game_variant_options *options);

/* ---------- globals */

/* the AE gametypes: the stock gametype each is built from, and the
gametype options it adds (game_engine.h's universal_variant flags) */
static struct arena_gametype const arena_gametypes[] =
{
	/* AE's casual set: TIMERS HUD + WAYPOINTS, DROP SECONDARY ALWAYS, the
	motion sensor, 15 minutes, the stock respawns, friendly fire on; King
	and Oddball by nav points. The free for all ones say FFA (AE COMP FFA too); the rest
	are team games */
	{ "AE FFA SLAY", build_game_variant_slayer, ARENA_GAMETYPE_FLAGS | ARENA_CASUAL_FLAGS,
		ARENA_FFA_SCORE, ARENA_TIME_LIMIT, ARENA_STOCK, ARENA_STOCK, ARENA_RADAR_ON, ARENA_VEHICLES_STOCK, TRUE },
	{ "AE TEAM SLY", build_game_variant_team_slayer, ARENA_GAMETYPE_FLAGS | ARENA_CASUAL_FLAGS,
		ARENA_TEAM_SLAYER_SCORE, ARENA_TIME_LIMIT, ARENA_STOCK, ARENA_STOCK, ARENA_RADAR_ON, ARENA_VEHICLES_STOCK, TRUE },
	{ "AE CTF", build_game_variant_ctf, ARENA_GAMETYPE_FLAGS | ARENA_CASUAL_FLAGS,
		ARENA_CTF_SCORE, ARENA_TIME_LIMIT, ARENA_STOCK, ARENA_STOCK, ARENA_RADAR_ON, ARENA_VEHICLES_STOCK, TRUE },
	{ "AE KING", build_game_variant_team_king, ARENA_GAMETYPE_FLAGS | ARENA_CASUAL_FLAGS,
		ARENA_TIMED_SCORE, ARENA_TIME_LIMIT, ARENA_STOCK, ARENA_STOCK, ARENA_RADAR_ON, ARENA_VEHICLES_STOCK, TRUE,
		.goal_radar = ARENA_GOAL_NAV_POINTS },
	{ "AE FFA BALL", build_game_variant_oddball, ARENA_GAMETYPE_FLAGS | ARENA_CASUAL_FLAGS,
		ARENA_TIMED_SCORE, ARENA_TIME_LIMIT, ARENA_STOCK, ARENA_STOCK, ARENA_RADAR_ON, ARENA_VEHICLES_STOCK, TRUE,
		.goal_radar = ARENA_GOAL_NAV_POINTS },
	/* the casual set for two against two (split-screen on one machine):
	the same rules; the host's player limit (4) is a server setting, not
	the gametype's */
	{ "AE 2V2 SLY", build_game_variant_team_slayer, ARENA_GAMETYPE_FLAGS | ARENA_CASUAL_FLAGS,
		25, ARENA_TIME_LIMIT, ARENA_STOCK, ARENA_STOCK, ARENA_RADAR_ON, ARENA_VEHICLES_STOCK, TRUE },
	{ "AE 2V2 CTF", build_game_variant_ctf, ARENA_GAMETYPE_FLAGS | ARENA_CASUAL_FLAGS,
		ARENA_CTF_SCORE, ARENA_TIME_LIMIT, ARENA_STOCK, ARENA_STOCK, ARENA_RADAR_ON, ARENA_VEHICLES_STOCK, TRUE },
	{ "AE 2V2 KING", build_game_variant_team_king, ARENA_GAMETYPE_FLAGS | ARENA_CASUAL_FLAGS,
		ARENA_TIMED_SCORE, ARENA_TIME_LIMIT, ARENA_STOCK, ARENA_STOCK, ARENA_RADAR_ON, ARENA_VEHICLES_STOCK, TRUE,
		.goal_radar = ARENA_GOAL_NAV_POINTS },
	{ "AE 2V2 BALL", build_game_variant_team_oddball, ARENA_GAMETYPE_FLAGS | ARENA_CASUAL_FLAGS,
		ARENA_TIMED_SCORE, ARENA_TIME_LIMIT, ARENA_STOCK, ARENA_STOCK, ARENA_RADAR_ON, ARENA_VEHICLES_STOCK, TRUE,
		.goal_radar = ARENA_GOAL_NAV_POINTS },
	/* the casual set's later ones (ARENA_LATER_FLAGS: TIMERS HUD +
	WAYPOINTS, DROP SECONDARY ALWAYS), friendly fire on, the stock
	respawns. Team Oddball, its objective shown by nav points */
	{ "AE TEAM OB", build_game_variant_team_oddball, ARENA_LATER_FLAGS,
		ARENA_TIMED_SCORE, ARENA_TIME_LIMIT, ARENA_STOCK, ARENA_STOCK, ARENA_RADAR_ON, ARENA_VEHICLES_STOCK, TRUE,
		.goal_radar = ARENA_GOAL_NAV_POINTS },
	/* PRACTICE: free for all slayer with PRACTICE MODE (every weapon and
	powerup each 30 seconds), no practical score limit, no time limit, the
	map's vehicles */
	{ "AE PRACTICE", build_game_variant_slayer, ARENA_LATER_FLAGS | FLAG(_game_variant_practice_bit),
		ARENA_TRAINING_SCORE_TO_WIN, 0, ARENA_STOCK, ARENA_STOCK, ARENA_RADAR_ON, ARENA_VEHICLES_DEFAULT, TRUE },
	/* the special modes: team slayer to 50 (HEAVIES 75), each with its
	loadout and weapon set */
	{ "AE SNIPERS", build_game_variant_team_slayer, ARENA_LATER_FLAGS,
		ARENA_TEAM_SLAYER_SCORE, ARENA_TIME_LIMIT, ARENA_STOCK, ARENA_STOCK, ARENA_RADAR_OFF, ARENA_VEHICLES_STOCK, TRUE,
		.weapon_set = ARENA_WEAPONS_SNIPING, .loadout_primary = ARENA_SET(_loadout_weapon_sniper_rifle),
		.loadout_secondary = ARENA_SET(_loadout_weapon_pistol) },
	{ "AE SHOTSNIP", build_game_variant_team_slayer, ARENA_LATER_FLAGS,
		ARENA_TEAM_SLAYER_SCORE, ARENA_TIME_LIMIT, ARENA_STOCK, ARENA_STOCK, ARENA_RADAR_OFF, ARENA_VEHICLES_STOCK, TRUE,
		.loadout_primary = ARENA_SET(_loadout_weapon_shotgun), .loadout_secondary = ARENA_SET(_loadout_weapon_sniper_rifle),
		.no_map_weapons = TRUE },
	{ "AE SWAT", build_game_variant_team_slayer, ARENA_SWAT_FLAGS,
		ARENA_TEAM_SLAYER_SCORE, ARENA_TIME_LIMIT, ARENA_STOCK, ARENA_STOCK, ARENA_RADAR_OFF, ARENA_VEHICLES_STOCK, TRUE,
		.weapon_set = ARENA_WEAPONS_PISTOLS, .loadout_primary = ARENA_SET(_loadout_weapon_pistol),
		.loadout_secondary = ARENA_SET(_loadout_weapon_none), .starting_frags = STARTING_GRENADES_NONE },
	{ "AE ROCKETS", build_game_variant_team_slayer, ARENA_LATER_FLAGS,
		ARENA_TEAM_SLAYER_SCORE, ARENA_TIME_LIMIT, ARENA_STOCK, ARENA_STOCK, ARENA_RADAR_ON, ARENA_VEHICLES_STOCK, TRUE,
		.weapon_set = ARENA_WEAPONS_ROCKET_LAUNCHERS, .loadout_primary = ARENA_SET(_loadout_weapon_rocket_launcher),
		.loadout_secondary = ARENA_SET(_loadout_weapon_none) },
	{ "AE SHOTGUNS", build_game_variant_team_slayer, ARENA_LATER_FLAGS,
		ARENA_TEAM_SLAYER_SCORE, ARENA_TIME_LIMIT, ARENA_STOCK, ARENA_STOCK, ARENA_RADAR_ON, ARENA_VEHICLES_STOCK, TRUE,
		.weapon_set = ARENA_WEAPONS_SHOTGUNS, .loadout_primary = ARENA_SET(_loadout_weapon_shotgun),
		.loadout_secondary = ARENA_SET(_loadout_weapon_pistol) },
	{ "AE HEAVIES", build_game_variant_team_slayer, ARENA_LATER_FLAGS,
		75, ARENA_TIME_LIMIT, ARENA_STOCK, ARENA_STOCK, ARENA_RADAR_ON, ARENA_VEHICLES_DEFAULT, TRUE,
		.weapon_set = ARENA_WEAPONS_HEAVY, .loadout_primary = ARENA_SET(_loadout_weapon_rocket_launcher),
		.loadout_secondary = ARENA_SET(_loadout_weapon_assault_rifle) },
	/* the casual set on Halo 1: NHE's maps' own modes (revision 6; NHE
	VANILLA and NHE POWERUP before): team slayer on the casual set's values,
	NHE's VANILLA and NHE & POWERUPS modes on its maps */
	{ "AE VANILLA", build_game_variant_team_slayer, ARENA_LATER_FLAGS,
		ARENA_TEAM_SLAYER_SCORE, ARENA_TIME_LIMIT, ARENA_STOCK, ARENA_STOCK, ARENA_RADAR_ON, ARENA_VEHICLES_STOCK, TRUE,
		.nhe_mode = _nhe_mode_vanilla },
	{ "AE POWERUPS", build_game_variant_team_slayer, ARENA_LATER_FLAGS,
		ARENA_TEAM_SLAYER_SCORE, ARENA_TIME_LIMIT, ARENA_STOCK, ARENA_STOCK, ARENA_RADAR_ON, ARENA_VEHICLES_STOCK, TRUE,
		.nhe_mode = _nhe_mode_nhe_and_powerups },
	/* TRAINING: free for all slayer with no practical score limit, the
	item timers and TRAINING's waypoints */
	{ "AE TRAINING", build_game_variant_slayer, ARENA_GAMETYPE_FLAGS | ARENA_TRAINING_FLAGS | ARENA_DROP_ALWAYS,
		ARENA_TRAINING_SCORE_TO_WIN, 0, ARENA_STOCK, ARENA_STOCK, ARENA_RADAR_STOCK, ARENA_VEHICLES_DEFAULT, TRUE },
	/* AE COMP (AE PRO before revision 2), modern NHE (revision 5): Beach LAN's
	5 second respawn and suicide penalty, no motion sensor, no vehicles, 2
	frag grenades, TIMERS LINE OF SIGHT, DROP SECONDARY ALWAYS; the slayers
	without a time limit, the objective games 15 minutes with their objective
	only in line of sight; team King and Oddball */
	{ "AE COMP FFA", build_game_variant_slayer, ARENA_COMP_FLAGS,
		ARENA_FFA_SCORE, 0, ARENA_COMPETITIVE_RESPAWN, ARENA_COMPETITIVE_RESPAWN, ARENA_RADAR_OFF,
		ARENA_VEHICLES_NONE, TRUE, .starting_frags = ARENA_COMP_STARTING_FRAGS },
	{ "AE COMP TS", build_game_variant_team_slayer, ARENA_COMP_FLAGS,
		ARENA_TEAM_SLAYER_SCORE, 0, ARENA_COMPETITIVE_RESPAWN, ARENA_COMPETITIVE_RESPAWN, ARENA_RADAR_OFF,
		ARENA_VEHICLES_NONE, TRUE, .starting_frags = ARENA_COMP_STARTING_FRAGS },
	{ "AE COMP CTF", build_game_variant_ctf, ARENA_COMP_OBJECTIVE_FLAGS,
		ARENA_CTF_SCORE, ARENA_TIME_LIMIT, ARENA_COMPETITIVE_RESPAWN, ARENA_COMPETITIVE_RESPAWN, ARENA_RADAR_OFF,
		ARENA_VEHICLES_NONE, TRUE, .goal_radar = ARENA_GOAL_NAV_POINTS, .starting_frags = ARENA_COMP_STARTING_FRAGS },
	{ "AE COMP KOH", build_game_variant_team_king, ARENA_COMP_OBJECTIVE_FLAGS,
		ARENA_TIMED_SCORE, ARENA_TIME_LIMIT, ARENA_COMPETITIVE_RESPAWN, ARENA_COMPETITIVE_RESPAWN, ARENA_RADAR_OFF,
		ARENA_VEHICLES_NONE, TRUE, .goal_radar = ARENA_GOAL_NAV_POINTS, .starting_frags = ARENA_COMP_STARTING_FRAGS },
	{ "AE COMP OB", build_game_variant_team_oddball, ARENA_COMP_OBJECTIVE_FLAGS,
		ARENA_TIMED_SCORE, ARENA_TIME_LIMIT, ARENA_COMPETITIVE_RESPAWN, ARENA_COMPETITIVE_RESPAWN, ARENA_RADAR_OFF,
		ARENA_VEHICLES_NONE, TRUE, .goal_radar = ARENA_GOAL_NAV_POINTS, .starting_frags = ARENA_COMP_STARTING_FRAGS },
	/* Halo 1: NHE's own 23 gametypes (revision 4), in NHE's order: on its
	maps (mods/NHE) each plays its NHE MODE; on other maps no vehicles.
	Classic health, fall damage, shields, infinite lives, NHE_SET_FLAGS, the
	weapon set's loadout (an assault rifle in hand), friendly fire on, no
	time limit. Respawn and suicide penalty in ticks; King's and Oddball's
	score minutes held; the slayers without death bonus or kill penalty */
	{ "TS 50", build_game_variant_team_slayer, NHE_SET_FLAGS, 50, 0, NHE_RESPAWN_5, NHE_RESPAWN_5, ARENA_RADAR_OFF,
		ARENA_VEHICLES_NONE, FALSE, .goal_radar = ARENA_GOAL_NONE, .nhe_mode = _nhe_mode_nhe_and_timer,
		.adjust = arena_gametype_nhe_slayer },
	{ "TS 100", build_game_variant_team_slayer, NHE_SET_FLAGS, 100, 0, NHE_RESPAWN_5, NHE_RESPAWN_10, ARENA_RADAR_OFF,
		ARENA_VEHICLES_NONE, FALSE, .goal_radar = ARENA_GOAL_NONE, .nhe_mode = _nhe_mode_nhe_and_timer,
		.adjust = arena_gametype_nhe_slayer },
	{ "TS ON-OFF", build_game_variant_team_slayer, NHE_SET_FLAGS, 50, 0, NHE_RESPAWN_5, NHE_RESPAWN_5, ARENA_RADAR_OFF,
		ARENA_VEHICLES_NONE, FALSE, .goal_radar = ARENA_GOAL_NONE, .nhe_mode = _nhe_mode_timer_only,
		.adjust = arena_gametype_nhe_slayer },
	/* (and TRAINING's markers on other maps) */
	{ "TS TRAINING", build_game_variant_team_slayer, NHE_SET_FLAGS | FLAG(_game_variant_training_bit), 50, 0,
		NHE_RESPAWN_5, NHE_RESPAWN_5, ARENA_RADAR_OFF, ARENA_VEHICLES_NONE, FALSE, .goal_radar = ARENA_GOAL_NONE,
		.nhe_mode = _nhe_mode_training, .adjust = arena_gametype_nhe_slayer },
	/* (team race to 1 lap, which NHE's maps turn into practice; on other
	maps PRACTICE MODE does, and keeps a lap from ending it:
	game_engine_race.c) */
	{ "TS PRACTICE", build_game_variant_team_race, NHE_PRACTICE_FLAGS, 1, 0, 0, 0, ARENA_RADAR_OFF,
		ARENA_VEHICLES_NONE, FALSE, .goal_radar = ARENA_GOAL_NONE, .nhe_mode = _nhe_mode_training },
	/* (invisible players, as NHE's file has them) */
	{ "TS SNIPERS", build_game_variant_team_slayer, NHE_SET_FLAGS | FLAG(_game_variant_always_invisible_bit), 50, 0,
		NHE_RESPAWN_5, NHE_RESPAWN_5, ARENA_RADAR_OFF, ARENA_VEHICLES_NONE, FALSE,
		.weapon_set = ARENA_WEAPONS_SNIPING, .goal_radar = ARENA_GOAL_NONE, .nhe_mode = _nhe_mode_nhe_and_timer,
		.adjust = arena_gametype_nhe_slayer },
	{ "FFA 50 NR", build_game_variant_slayer, NHE_SET_FLAGS, 50, 0, NHE_RESPAWN_5, NHE_RESPAWN_5, ARENA_RADAR_OFF,
		ARENA_VEHICLES_NONE, FALSE, .goal_radar = ARENA_GOAL_NONE, .nhe_mode = _nhe_mode_nhe_and_timer,
		.adjust = arena_gametype_nhe_slayer },
	{ "FFA 50 R", build_game_variant_slayer, NHE_SET_FLAGS, 50, 0, NHE_RESPAWN_5, NHE_RESPAWN_5, ARENA_RADAR_ON,
		ARENA_VEHICLES_NONE, FALSE, .goal_radar = ARENA_GOAL_MOTION_TRACKER, .nhe_mode = _nhe_mode_nhe_and_timer,
		.adjust = arena_gametype_nhe_slayer },
	{ "1 V 1 NR", build_game_variant_slayer, NHE_SET_FLAGS, 15, 0, NHE_RESPAWN_5, NHE_RESPAWN_5, ARENA_RADAR_OFF,
		ARENA_VEHICLES_NONE, FALSE, .goal_radar = ARENA_GOAL_NONE, .nhe_mode = _nhe_mode_nhe_and_timer,
		.adjust = arena_gametype_nhe_slayer },
	{ "1 V 1 R", build_game_variant_slayer, NHE_SET_FLAGS, 15, 0, NHE_RESPAWN_5, NHE_RESPAWN_5, ARENA_RADAR_ON,
		ARENA_VEHICLES_NONE, FALSE, .goal_radar = ARENA_GOAL_MOTION_TRACKER, .nhe_mode = _nhe_mode_nhe_and_timer,
		.adjust = arena_gametype_nhe_slayer },
	/* (team King, its stock moving hill) */
	{ "KOTH 5M 7S", build_game_variant_team_king, NHE_SET_FLAGS, ARENA_TIMED_SCORE, 0, NHE_RESPAWN_7_5, NHE_RESPAWN_10,
		ARENA_RADAR_OFF, ARENA_VEHICLES_NONE, FALSE, .goal_radar = ARENA_GOAL_NAV_POINTS,
		.nhe_mode = _nhe_mode_nhe_and_timer },
	{ "KOTH 5M 10S", build_game_variant_team_king, NHE_SET_FLAGS, ARENA_TIMED_SCORE, 0, NHE_RESPAWN_10, NHE_RESPAWN_10,
		ARENA_RADAR_OFF, ARENA_VEHICLES_NONE, FALSE, .goal_radar = ARENA_GOAL_NAV_POINTS,
		.nhe_mode = _nhe_mode_nhe_and_timer },
	/* (team Oddball, its stock ball: slow with it, no traits, one normal
	ball, no random start or spawn delay) */
	{ "BALL 5M 7S", build_game_variant_team_oddball, NHE_SET_FLAGS, ARENA_TIMED_SCORE, 0, NHE_RESPAWN_7_5,
		NHE_RESPAWN_10, ARENA_RADAR_OFF, ARENA_VEHICLES_NONE, FALSE, .goal_radar = ARENA_GOAL_NAV_POINTS,
		.nhe_mode = _nhe_mode_nhe_and_timer },
	{ "BALL 5M 10S", build_game_variant_team_oddball, NHE_SET_FLAGS, ARENA_TIMED_SCORE, 0, NHE_RESPAWN_10,
		NHE_RESPAWN_10, ARENA_RADAR_OFF, ARENA_VEHICLES_NONE, FALSE, .goal_radar = ARENA_GOAL_NAV_POINTS,
		.nhe_mode = _nhe_mode_nhe_and_timer },
	/* (CTF: the flag at home to score; no assault, reset on capture, flag
	must reset or single flag, the stock's) */
	{ "CTF 3C 7S", build_game_variant_ctf, NHE_SET_FLAGS, 3, 0, NHE_RESPAWN_7_5, NHE_RESPAWN_10, ARENA_RADAR_OFF,
		ARENA_VEHICLES_NONE, FALSE, .goal_radar = ARENA_GOAL_NAV_POINTS, .nhe_mode = _nhe_mode_nhe_and_timer,
		.adjust = arena_gametype_nhe_ctf },
	{ "CTF 3C 7S R", build_game_variant_ctf, NHE_SET_FLAGS, 3, 0, NHE_RESPAWN_7_5, NHE_RESPAWN_10, ARENA_RADAR_ON,
		ARENA_VEHICLES_NONE, FALSE, .goal_radar = ARENA_GOAL_NAV_POINTS, .nhe_mode = _nhe_mode_nhe_and_timer,
		.adjust = arena_gametype_nhe_ctf },
	{ "CTF 3C 10S", build_game_variant_ctf, NHE_SET_FLAGS, 3, 0, NHE_RESPAWN_10, NHE_RESPAWN_10, ARENA_RADAR_OFF,
		ARENA_VEHICLES_NONE, FALSE, .goal_radar = ARENA_GOAL_NAV_POINTS, .nhe_mode = _nhe_mode_nhe_and_timer,
		.adjust = arena_gametype_nhe_ctf },
	{ "CTF 3 10S R", build_game_variant_ctf, NHE_SET_FLAGS, 3, 0, NHE_RESPAWN_10, NHE_RESPAWN_10, ARENA_RADAR_ON,
		ARENA_VEHICLES_NONE, FALSE, .goal_radar = ARENA_GOAL_NAV_POINTS, .nhe_mode = _nhe_mode_nhe_and_timer,
		.adjust = arena_gametype_nhe_ctf },
	{ "CTF 5C 7S", build_game_variant_ctf, NHE_SET_FLAGS, 5, 0, NHE_RESPAWN_7_5, NHE_RESPAWN_10, ARENA_RADAR_OFF,
		ARENA_VEHICLES_NONE, FALSE, .goal_radar = ARENA_GOAL_NAV_POINTS, .nhe_mode = _nhe_mode_nhe_and_timer,
		.adjust = arena_gametype_nhe_ctf },
	{ "CTF 5C 7S R", build_game_variant_ctf, NHE_SET_FLAGS, 5, 0, NHE_RESPAWN_7_5, NHE_RESPAWN_10, ARENA_RADAR_ON,
		ARENA_VEHICLES_NONE, FALSE, .goal_radar = ARENA_GOAL_NAV_POINTS, .nhe_mode = _nhe_mode_nhe_and_timer,
		.adjust = arena_gametype_nhe_ctf },
	{ "CTF 5C 10S", build_game_variant_ctf, NHE_SET_FLAGS, 5, 0, NHE_RESPAWN_10, NHE_RESPAWN_10, ARENA_RADAR_OFF,
		ARENA_VEHICLES_NONE, FALSE, .goal_radar = ARENA_GOAL_NAV_POINTS, .nhe_mode = _nhe_mode_nhe_and_timer,
		.adjust = arena_gametype_nhe_ctf },
	{ "CTF 5 10S R", build_game_variant_ctf, NHE_SET_FLAGS, 5, 0, NHE_RESPAWN_10, NHE_RESPAWN_10, ARENA_RADAR_ON,
		ARENA_VEHICLES_NONE, FALSE, .goal_radar = ARENA_GOAL_NAV_POINTS, .nhe_mode = _nhe_mode_nhe_and_timer,
		.adjust = arena_gametype_nhe_ctf },
	/* (pistols, the motion tracker for players and objectives) */
	{ "CTF WIZARD", build_game_variant_ctf, NHE_SET_FLAGS, 5, 0, NHE_RESPAWN_10, NHE_RESPAWN_10, ARENA_RADAR_ON,
		ARENA_VEHICLES_NONE, FALSE, .weapon_set = ARENA_WEAPONS_PISTOLS, .goal_radar = ARENA_GOAL_MOTION_TRACKER,
		.nhe_mode = _nhe_mode_nhe_and_timer, .adjust = arena_gametype_nhe_ctf },
	/* (NHE TRAIN, a seed of before, is folded into AE TRAINING (revision 6):
	no longer seeded; a root's file stays as it is, listed as AE TRAINING's
	old name; NHE VANILLA and NHE POWERUP became AE VANILLA and AE POWERUPS) */
};

/* the seeds' migrations, by revision (the record's "#revision N": each
above it runs once). Revision 1: the free for all ones whose names did not
say so; revision 2: AE PRO -> AE COMP (values unchanged); revision 3: the
NHE seeds' mode from NHE MODE, no vehicles (on stock maps); revision 4:
Halo 1: NHE's own set (NHE 2V2 TS -> TS 50, NHE 1V1 -> 1 V 1 NR, NHE CTF ->
CTF 3C 7S, PRACTICE -> TS PRACTICE, with NHE's values). A row whose old
and new names are the same is a value update (in place, under its name).
Designated initialisers: a field added to struct arena_gametype never
shifts a frozen row */
static struct arena_gametype_migration const arena_gametype_migrations[] =
{
	{ .revision = 1,
		.old_row = { .name = "AE SLAYER", .build = build_game_variant_slayer, .flags = ARENA_REV1_GAMETYPE_FLAGS | ARENA_REV1_CASUAL_FLAGS,
			.score_to_win = 25, .time_limit = 15, .respawn_time = ARENA_STOCK, .suicide_penalty = ARENA_STOCK,
			.radar = ARENA_RADAR_ON, .vehicle_set = ARENA_VEHICLES_STOCK, .custom_loadout = TRUE },
		.new_row = { .name = "AE FFA SLAY", .build = build_game_variant_slayer, .flags = ARENA_REV1_GAMETYPE_FLAGS | ARENA_REV1_CASUAL_FLAGS,
			.score_to_win = 25, .time_limit = 15, .respawn_time = ARENA_STOCK, .suicide_penalty = ARENA_STOCK,
			.radar = ARENA_RADAR_ON, .vehicle_set = ARENA_VEHICLES_STOCK, .custom_loadout = TRUE },
		.old_block_hash = "1af3518f47808e14fe5f89035329e4456791038f" },
	{ .revision = 1,
		.old_row = { .name = "AE ODDBALL", .build = build_game_variant_oddball, .flags = ARENA_REV1_GAMETYPE_FLAGS | ARENA_REV1_CASUAL_FLAGS,
			.score_to_win = 5, .time_limit = 15, .respawn_time = ARENA_STOCK, .suicide_penalty = ARENA_STOCK,
			.radar = ARENA_RADAR_ON, .vehicle_set = ARENA_VEHICLES_STOCK, .custom_loadout = TRUE },
		.new_row = { .name = "AE FFA BALL", .build = build_game_variant_oddball, .flags = ARENA_REV1_GAMETYPE_FLAGS | ARENA_REV1_CASUAL_FLAGS,
			.score_to_win = 5, .time_limit = 15, .respawn_time = ARENA_STOCK, .suicide_penalty = ARENA_STOCK,
			.radar = ARENA_RADAR_ON, .vehicle_set = ARENA_VEHICLES_STOCK, .custom_loadout = TRUE },
		.old_block_hash = "7997544acaadb767cb6c8cff13c59496a4ffdf7f" },
	{ .revision = 2,
		.old_row = { .name = "AE PRO FFA", .build = build_game_variant_slayer, .flags = ARENA_REV1_GAMETYPE_FLAGS,
			.score_to_win = 25, .time_limit = 0, .respawn_time = 150, .suicide_penalty = 150,
			.radar = ARENA_RADAR_ON, .vehicle_set = ARENA_VEHICLES_STOCK, .custom_loadout = TRUE },
		.new_row = { .name = "AE COMP FFA", .build = build_game_variant_slayer, .flags = ARENA_REV1_GAMETYPE_FLAGS,
			.score_to_win = 25, .time_limit = 0, .respawn_time = 150, .suicide_penalty = 150,
			.radar = ARENA_RADAR_ON, .vehicle_set = ARENA_VEHICLES_STOCK, .custom_loadout = TRUE },
		.old_block_hash = "73c9d3149afe26d931ae3c6cef67affa3d70beb5" },
	{ .revision = 2,
		.old_row = { .name = "AE PRO TS", .build = build_game_variant_team_slayer, .flags = ARENA_REV1_GAMETYPE_FLAGS,
			.score_to_win = 50, .time_limit = 0, .respawn_time = 150, .suicide_penalty = 150,
			.radar = ARENA_RADAR_OFF, .vehicle_set = ARENA_VEHICLES_STOCK, .custom_loadout = TRUE },
		.new_row = { .name = "AE COMP TS", .build = build_game_variant_team_slayer, .flags = ARENA_REV1_GAMETYPE_FLAGS,
			.score_to_win = 50, .time_limit = 0, .respawn_time = 150, .suicide_penalty = 150,
			.radar = ARENA_RADAR_OFF, .vehicle_set = ARENA_VEHICLES_STOCK, .custom_loadout = TRUE },
		.old_block_hash = "0ae13b0e49dce95ae82923e71fca9bbcea3ff6dc" },
	{ .revision = 2,
		.old_row = { .name = "AE PRO CTF", .build = build_game_variant_ctf, .flags = ARENA_REV1_GAMETYPE_FLAGS,
			.score_to_win = 3, .time_limit = 15, .respawn_time = 150, .suicide_penalty = 150,
			.radar = ARENA_RADAR_OFF, .vehicle_set = ARENA_VEHICLES_STOCK, .custom_loadout = TRUE },
		.new_row = { .name = "AE COMP CTF", .build = build_game_variant_ctf, .flags = ARENA_REV1_GAMETYPE_FLAGS,
			.score_to_win = 3, .time_limit = 15, .respawn_time = 150, .suicide_penalty = 150,
			.radar = ARENA_RADAR_OFF, .vehicle_set = ARENA_VEHICLES_STOCK, .custom_loadout = TRUE },
		.old_block_hash = "aed1d44654a5d81e3be9281098ccf1898ccfaf14" },
	{ .revision = 2,
		.old_row = { .name = "AE PRO KING", .build = build_game_variant_team_king, .flags = ARENA_REV1_GAMETYPE_FLAGS,
			.score_to_win = 5, .time_limit = 15, .respawn_time = 150, .suicide_penalty = 150,
			.radar = ARENA_RADAR_OFF, .vehicle_set = ARENA_VEHICLES_STOCK, .custom_loadout = TRUE },
		.new_row = { .name = "AE COMP KOH", .build = build_game_variant_team_king, .flags = ARENA_REV1_GAMETYPE_FLAGS,
			.score_to_win = 5, .time_limit = 15, .respawn_time = 150, .suicide_penalty = 150,
			.radar = ARENA_RADAR_OFF, .vehicle_set = ARENA_VEHICLES_STOCK, .custom_loadout = TRUE },
		.old_block_hash = "177eea3c609c50b05e73c166a3c729da0fd5dbfc" },
	{ .revision = 2,
		.old_row = { .name = "AE PRO BALL", .build = build_game_variant_team_oddball, .flags = ARENA_REV1_GAMETYPE_FLAGS,
			.score_to_win = 5, .time_limit = 15, .respawn_time = 150, .suicide_penalty = 150,
			.radar = ARENA_RADAR_OFF, .vehicle_set = ARENA_VEHICLES_STOCK, .custom_loadout = TRUE },
		.new_row = { .name = "AE COMP OB", .build = build_game_variant_team_oddball, .flags = ARENA_REV1_GAMETYPE_FLAGS,
			.score_to_win = 5, .time_limit = 15, .respawn_time = 150, .suicide_penalty = 150,
			.radar = ARENA_RADAR_OFF, .vehicle_set = ARENA_VEHICLES_STOCK, .custom_loadout = TRUE },
		.old_block_hash = "e0a3e990e36fff499fc569206556e70b3b4d0eb6" },
	{ .revision = 3,
		.old_row = { .name = "NHE 1V1", .build = build_game_variant_slayer, .flags = ARENA_REV1_NHE_RULES_FLAGS,
			.score_to_win = 25, .time_limit = 0, .respawn_time = 150, .suicide_penalty = 150,
			.radar = ARENA_RADAR_OFF, .vehicle_set = ARENA_VEHICLES_WARTHOG, .custom_loadout = FALSE },
		.new_row = { .name = "NHE 1V1", .build = build_game_variant_slayer, .flags = ARENA_REV1_NHE_RULES_FLAGS,
			.score_to_win = 25, .time_limit = 0, .respawn_time = 150, .suicide_penalty = 150,
			.radar = ARENA_RADAR_OFF, .vehicle_set = ARENA_VEHICLES_NONE, .custom_loadout = FALSE, .nhe_mode = _nhe_mode_nhe_and_timer },
		.old_block_hash = "e24634c5390238a33b15d1dea6b4c38f035d969c" },
	{ .revision = 3,
		.old_row = { .name = "NHE 2V2 TS", .build = build_game_variant_team_slayer, .flags = ARENA_REV1_NHE_RULES_FLAGS,
			.score_to_win = 50, .time_limit = 0, .respawn_time = 150, .suicide_penalty = 150,
			.radar = ARENA_RADAR_OFF, .vehicle_set = ARENA_VEHICLES_WARTHOG, .custom_loadout = FALSE },
		.new_row = { .name = "NHE 2V2 TS", .build = build_game_variant_team_slayer, .flags = ARENA_REV1_NHE_RULES_FLAGS,
			.score_to_win = 50, .time_limit = 0, .respawn_time = 150, .suicide_penalty = 150,
			.radar = ARENA_RADAR_OFF, .vehicle_set = ARENA_VEHICLES_NONE, .custom_loadout = FALSE, .nhe_mode = _nhe_mode_nhe_and_timer },
		.old_block_hash = "a5f393f3d148e662420562af940168ff3093ab13" },
	{ .revision = 3,
		.old_row = { .name = "NHE CTF", .build = build_game_variant_ctf, .flags = ARENA_REV1_NHE_RULES_FLAGS,
			.score_to_win = 3, .time_limit = 0, .respawn_time = 150, .suicide_penalty = 150,
			.radar = ARENA_RADAR_OFF, .vehicle_set = ARENA_VEHICLES_WARTHOG, .custom_loadout = FALSE },
		.new_row = { .name = "NHE CTF", .build = build_game_variant_ctf, .flags = ARENA_REV1_NHE_RULES_FLAGS,
			.score_to_win = 3, .time_limit = 0, .respawn_time = 150, .suicide_penalty = 150,
			.radar = ARENA_RADAR_OFF, .vehicle_set = ARENA_VEHICLES_NONE, .custom_loadout = FALSE, .nhe_mode = _nhe_mode_nhe_and_timer },
		.old_block_hash = "9cccce809aeb47880add40b50f94580ddb727c4b" },
	{ .revision = 3,
		.old_row = { .name = "NHE POWERUP", .build = build_game_variant_team_slayer, .flags = ARENA_REV1_NHE_RULES_FLAGS,
			.score_to_win = 50, .time_limit = 0, .respawn_time = 150, .suicide_penalty = 150,
			.radar = ARENA_RADAR_OFF, .vehicle_set = ARENA_VEHICLES_TANK, .custom_loadout = FALSE },
		.new_row = { .name = "NHE POWERUP", .build = build_game_variant_team_slayer, .flags = ARENA_REV1_NHE_RULES_FLAGS,
			.score_to_win = 50, .time_limit = 0, .respawn_time = 150, .suicide_penalty = 150,
			.radar = ARENA_RADAR_OFF, .vehicle_set = ARENA_VEHICLES_NONE, .custom_loadout = FALSE, .nhe_mode = _nhe_mode_nhe_and_powerups },
		.old_block_hash = "bd92053b4e25991b354b9b3c49eefe35b2a4009d" },
	{ .revision = 3,
		.old_row = { .name = "NHE VANILLA", .build = build_game_variant_team_slayer, .flags = ARENA_REV1_NHE_FLAGS,
			.score_to_win = 0, .time_limit = 0, .respawn_time = ARENA_STOCK, .suicide_penalty = ARENA_STOCK,
			.radar = ARENA_RADAR_STOCK, .vehicle_set = ARENA_VEHICLES_NONE, .custom_loadout = FALSE },
		.new_row = { .name = "NHE VANILLA", .build = build_game_variant_team_slayer, .flags = ARENA_REV1_NHE_FLAGS,
			.score_to_win = 0, .time_limit = 0, .respawn_time = ARENA_STOCK, .suicide_penalty = ARENA_STOCK,
			.radar = ARENA_RADAR_STOCK, .vehicle_set = ARENA_VEHICLES_NONE, .custom_loadout = FALSE, .nhe_mode = _nhe_mode_vanilla },
		.old_block_hash = "00db29a1cca5ffcbd2c4e240a01c61a297794d74" },
	{ .revision = 3,
		.old_row = { .name = "NHE TRAIN", .build = build_game_variant_slayer, .flags = ARENA_REV1_NHE_RULES_FLAGS,
			.score_to_win = 500, .time_limit = 0, .respawn_time = ARENA_STOCK, .suicide_penalty = ARENA_STOCK,
			.radar = ARENA_RADAR_STOCK, .vehicle_set = ARENA_VEHICLES_DEFAULT, .custom_loadout = FALSE },
		.new_row = { .name = "NHE TRAIN", .build = build_game_variant_slayer, .flags = ARENA_REV1_NHE_RULES_FLAGS,
			.score_to_win = 500, .time_limit = 0, .respawn_time = ARENA_STOCK, .suicide_penalty = ARENA_STOCK,
			.radar = ARENA_RADAR_STOCK, .vehicle_set = ARENA_VEHICLES_NONE, .custom_loadout = FALSE, .nhe_mode = _nhe_mode_training },
		.old_block_hash = "fd45a0a441d073d759a839090232cbbee4bc2d18" },
	{ .revision = 3,
		.old_row = { .name = "PRACTICE", .build = build_game_variant_race, .flags = ARENA_REV1_NHE_PRACTICE_FLAGS,
			.score_to_win = 0, .time_limit = 0, .respawn_time = ARENA_STOCK, .suicide_penalty = ARENA_STOCK,
			.radar = ARENA_RADAR_STOCK, .vehicle_set = ARENA_VEHICLES_GHOST, .custom_loadout = FALSE },
		.new_row = { .name = "PRACTICE", .build = build_game_variant_race, .flags = ARENA_REV1_NHE_PRACTICE_FLAGS,
			.score_to_win = 0, .time_limit = 0, .respawn_time = ARENA_STOCK, .suicide_penalty = ARENA_STOCK,
			.radar = ARENA_RADAR_STOCK, .vehicle_set = ARENA_VEHICLES_NONE, .custom_loadout = FALSE, .nhe_mode = _nhe_mode_training },
		.old_block_hash = "bbdd586bb412eb7b7be0300deaf43d3da25b3ae6" },
	/* revision 4: Halo 1: NHE's own gametypes; four seeds of before become
	the NHE ones they were near (the old rows: revision 3's new ones; the
	other 19 are seeded new) */
	{ .revision = 4,
		.old_row = { .name = "NHE 2V2 TS", .build = build_game_variant_team_slayer, .flags = ARENA_REV1_NHE_RULES_FLAGS,
			.score_to_win = 50, .time_limit = 0, .respawn_time = 150, .suicide_penalty = 150,
			.radar = ARENA_RADAR_OFF, .vehicle_set = ARENA_VEHICLES_NONE, .custom_loadout = FALSE, .nhe_mode = _nhe_mode_nhe_and_timer },
		.new_row = { .name = "TS 50", .build = build_game_variant_team_slayer, .flags = ARENA_REV4_NHE_SET_FLAGS,
			.score_to_win = 50, .time_limit = 0, .respawn_time = 150, .suicide_penalty = 150,
			.radar = ARENA_RADAR_OFF, .vehicle_set = ARENA_VEHICLES_NONE, .custom_loadout = FALSE, .goal_radar = ARENA_SET(2),
			.nhe_mode = _nhe_mode_nhe_and_timer, .adjust = arena_gametype_nhe_slayer },
		.old_block_hash = "085f93df63f0c49c323c732c3529537179910f98" },
	{ .revision = 4,
		.old_row = { .name = "NHE 1V1", .build = build_game_variant_slayer, .flags = ARENA_REV1_NHE_RULES_FLAGS,
			.score_to_win = 25, .time_limit = 0, .respawn_time = 150, .suicide_penalty = 150,
			.radar = ARENA_RADAR_OFF, .vehicle_set = ARENA_VEHICLES_NONE, .custom_loadout = FALSE, .nhe_mode = _nhe_mode_nhe_and_timer },
		.new_row = { .name = "1 V 1 NR", .build = build_game_variant_slayer, .flags = ARENA_REV4_NHE_SET_FLAGS,
			.score_to_win = 15, .time_limit = 0, .respawn_time = 150, .suicide_penalty = 150,
			.radar = ARENA_RADAR_OFF, .vehicle_set = ARENA_VEHICLES_NONE, .custom_loadout = FALSE, .goal_radar = ARENA_SET(2),
			.nhe_mode = _nhe_mode_nhe_and_timer, .adjust = arena_gametype_nhe_slayer },
		.old_block_hash = "b516a0d00a444984d4e535735e8314a1cc00ed56" },
	{ .revision = 4,
		.old_row = { .name = "NHE CTF", .build = build_game_variant_ctf, .flags = ARENA_REV1_NHE_RULES_FLAGS,
			.score_to_win = 3, .time_limit = 0, .respawn_time = 150, .suicide_penalty = 150,
			.radar = ARENA_RADAR_OFF, .vehicle_set = ARENA_VEHICLES_NONE, .custom_loadout = FALSE, .nhe_mode = _nhe_mode_nhe_and_timer },
		.new_row = { .name = "CTF 3C 7S", .build = build_game_variant_ctf, .flags = ARENA_REV4_NHE_SET_FLAGS,
			.score_to_win = 3, .time_limit = 0, .respawn_time = 225, .suicide_penalty = 300,
			.radar = ARENA_RADAR_OFF, .vehicle_set = ARENA_VEHICLES_NONE, .custom_loadout = FALSE, .goal_radar = ARENA_SET(1),
			.nhe_mode = _nhe_mode_nhe_and_timer, .adjust = arena_gametype_nhe_ctf },
		.old_block_hash = "708be132828b3b69f2c11c39944f8eda2e2786ae" },
	{ .revision = 4,
		.old_row = { .name = "PRACTICE", .build = build_game_variant_race, .flags = ARENA_REV1_NHE_PRACTICE_FLAGS,
			.score_to_win = 0, .time_limit = 0, .respawn_time = ARENA_STOCK, .suicide_penalty = ARENA_STOCK,
			.radar = ARENA_RADAR_STOCK, .vehicle_set = ARENA_VEHICLES_NONE, .custom_loadout = FALSE, .nhe_mode = _nhe_mode_training },
		.new_row = { .name = "TS PRACTICE", .build = build_game_variant_team_race, .flags = ARENA_REV4_NHE_PRACTICE_FLAGS,
			.score_to_win = 1, .time_limit = 0, .respawn_time = 0, .suicide_penalty = 0,
			.radar = ARENA_RADAR_OFF, .vehicle_set = ARENA_VEHICLES_NONE, .custom_loadout = FALSE, .goal_radar = ARENA_SET(2),
			.nhe_mode = _nhe_mode_training },
		.old_block_hash = "bfeebb5af53cd227c97147297762dadb737833b2" },
	/* revision 5: every AE gametype's final values (owner, 2026-10-08): NO
	SPREAD NHE instead of FULL; the casual set TIMERS HUD + WAYPOINTS and
	DROP SECONDARY ALWAYS; AE TRAINING DROP SECONDARY ALWAYS; AE COMP no
	motion sensor (FFA too), no vehicles, 2 frag grenades, TIMERS LINE OF
	SIGHT, its objective games' objective in line of sight, DROP SECONDARY
	ALWAYS. Value updates in place, under the same names (the old rows:
	revision 4's seeds) */
	{ .revision = 5,
		.old_row = { .name = "AE FFA SLAY", .build = build_game_variant_slayer, .flags = ARENA_REV1_GAMETYPE_FLAGS | ARENA_REV1_CASUAL_FLAGS,
			.score_to_win = 25, .time_limit = 15, .respawn_time = ARENA_STOCK, .suicide_penalty = ARENA_STOCK,
			.radar = ARENA_RADAR_ON, .vehicle_set = ARENA_VEHICLES_STOCK, .custom_loadout = TRUE },
		.new_row = { .name = "AE FFA SLAY", .build = build_game_variant_slayer, .flags = ARENA_REV5_CASUAL_FLAGS,
			.score_to_win = 25, .time_limit = 15, .respawn_time = ARENA_STOCK, .suicide_penalty = ARENA_STOCK,
			.radar = ARENA_RADAR_ON, .vehicle_set = ARENA_VEHICLES_STOCK, .custom_loadout = TRUE },
		.old_block_hash = "c8f070c6822f102f3247d56e67ece3861e0d3d69" },
	{ .revision = 5,
		.old_row = { .name = "AE TEAM SLY", .build = build_game_variant_team_slayer, .flags = ARENA_REV1_GAMETYPE_FLAGS | ARENA_REV1_CASUAL_FLAGS,
			.score_to_win = 50, .time_limit = 15, .respawn_time = ARENA_STOCK, .suicide_penalty = ARENA_STOCK,
			.radar = ARENA_RADAR_ON, .vehicle_set = ARENA_VEHICLES_STOCK, .custom_loadout = TRUE },
		.new_row = { .name = "AE TEAM SLY", .build = build_game_variant_team_slayer, .flags = ARENA_REV5_CASUAL_FLAGS,
			.score_to_win = 50, .time_limit = 15, .respawn_time = ARENA_STOCK, .suicide_penalty = ARENA_STOCK,
			.radar = ARENA_RADAR_ON, .vehicle_set = ARENA_VEHICLES_STOCK, .custom_loadout = TRUE },
		.old_block_hash = "a21d779566cbf1c979743cfdc0af7a6feb8e4845" },
	{ .revision = 5,
		.old_row = { .name = "AE CTF", .build = build_game_variant_ctf, .flags = ARENA_REV1_GAMETYPE_FLAGS | ARENA_REV1_CASUAL_FLAGS,
			.score_to_win = 3, .time_limit = 15, .respawn_time = ARENA_STOCK, .suicide_penalty = ARENA_STOCK,
			.radar = ARENA_RADAR_ON, .vehicle_set = ARENA_VEHICLES_STOCK, .custom_loadout = TRUE },
		.new_row = { .name = "AE CTF", .build = build_game_variant_ctf, .flags = ARENA_REV5_CASUAL_FLAGS,
			.score_to_win = 3, .time_limit = 15, .respawn_time = ARENA_STOCK, .suicide_penalty = ARENA_STOCK,
			.radar = ARENA_RADAR_ON, .vehicle_set = ARENA_VEHICLES_STOCK, .custom_loadout = TRUE },
		.old_block_hash = "404b1b1dc1ebe66e932a15286b802860601cef22" },
	{ .revision = 5,
		.old_row = { .name = "AE KING", .build = build_game_variant_team_king, .flags = ARENA_REV1_GAMETYPE_FLAGS | ARENA_REV1_CASUAL_FLAGS,
			.score_to_win = 5, .time_limit = 15, .respawn_time = ARENA_STOCK, .suicide_penalty = ARENA_STOCK,
			.radar = ARENA_RADAR_ON, .vehicle_set = ARENA_VEHICLES_STOCK, .custom_loadout = TRUE },
		.new_row = { .name = "AE KING", .build = build_game_variant_team_king, .flags = ARENA_REV5_CASUAL_FLAGS,
			.score_to_win = 5, .time_limit = 15, .respawn_time = ARENA_STOCK, .suicide_penalty = ARENA_STOCK,
			.radar = ARENA_RADAR_ON, .vehicle_set = ARENA_VEHICLES_STOCK, .custom_loadout = TRUE,
			.goal_radar = ARENA_SET(1) },
		.old_block_hash = "19b06079fea70429be74a66b4005088508faf9e1" },
	{ .revision = 5,
		.old_row = { .name = "AE FFA BALL", .build = build_game_variant_oddball, .flags = ARENA_REV1_GAMETYPE_FLAGS | ARENA_REV1_CASUAL_FLAGS,
			.score_to_win = 5, .time_limit = 15, .respawn_time = ARENA_STOCK, .suicide_penalty = ARENA_STOCK,
			.radar = ARENA_RADAR_ON, .vehicle_set = ARENA_VEHICLES_STOCK, .custom_loadout = TRUE },
		.new_row = { .name = "AE FFA BALL", .build = build_game_variant_oddball, .flags = ARENA_REV5_CASUAL_FLAGS,
			.score_to_win = 5, .time_limit = 15, .respawn_time = ARENA_STOCK, .suicide_penalty = ARENA_STOCK,
			.radar = ARENA_RADAR_ON, .vehicle_set = ARENA_VEHICLES_STOCK, .custom_loadout = TRUE,
			.goal_radar = ARENA_SET(1) },
		.old_block_hash = "a2875b6d759840b2a92fccced03e505a33204f62" },
	{ .revision = 5,
		.old_row = { .name = "AE 2V2 SLY", .build = build_game_variant_team_slayer, .flags = ARENA_REV1_GAMETYPE_FLAGS | ARENA_REV1_CASUAL_FLAGS,
			.score_to_win = 25, .time_limit = 15, .respawn_time = ARENA_STOCK, .suicide_penalty = ARENA_STOCK,
			.radar = ARENA_RADAR_ON, .vehicle_set = ARENA_VEHICLES_STOCK, .custom_loadout = TRUE },
		.new_row = { .name = "AE 2V2 SLY", .build = build_game_variant_team_slayer, .flags = ARENA_REV5_CASUAL_FLAGS,
			.score_to_win = 25, .time_limit = 15, .respawn_time = ARENA_STOCK, .suicide_penalty = ARENA_STOCK,
			.radar = ARENA_RADAR_ON, .vehicle_set = ARENA_VEHICLES_STOCK, .custom_loadout = TRUE },
		.old_block_hash = "9b31914b85e211d747c3bb29781057a9ef1d256b" },
	{ .revision = 5,
		.old_row = { .name = "AE 2V2 CTF", .build = build_game_variant_ctf, .flags = ARENA_REV1_GAMETYPE_FLAGS | ARENA_REV1_CASUAL_FLAGS,
			.score_to_win = 3, .time_limit = 15, .respawn_time = ARENA_STOCK, .suicide_penalty = ARENA_STOCK,
			.radar = ARENA_RADAR_ON, .vehicle_set = ARENA_VEHICLES_STOCK, .custom_loadout = TRUE },
		.new_row = { .name = "AE 2V2 CTF", .build = build_game_variant_ctf, .flags = ARENA_REV5_CASUAL_FLAGS,
			.score_to_win = 3, .time_limit = 15, .respawn_time = ARENA_STOCK, .suicide_penalty = ARENA_STOCK,
			.radar = ARENA_RADAR_ON, .vehicle_set = ARENA_VEHICLES_STOCK, .custom_loadout = TRUE },
		.old_block_hash = "bf99ffe812341143168ee9e6d8d9d6afe76de261" },
	{ .revision = 5,
		.old_row = { .name = "AE 2V2 KING", .build = build_game_variant_team_king, .flags = ARENA_REV1_GAMETYPE_FLAGS | ARENA_REV1_CASUAL_FLAGS,
			.score_to_win = 5, .time_limit = 15, .respawn_time = ARENA_STOCK, .suicide_penalty = ARENA_STOCK,
			.radar = ARENA_RADAR_ON, .vehicle_set = ARENA_VEHICLES_STOCK, .custom_loadout = TRUE },
		.new_row = { .name = "AE 2V2 KING", .build = build_game_variant_team_king, .flags = ARENA_REV5_CASUAL_FLAGS,
			.score_to_win = 5, .time_limit = 15, .respawn_time = ARENA_STOCK, .suicide_penalty = ARENA_STOCK,
			.radar = ARENA_RADAR_ON, .vehicle_set = ARENA_VEHICLES_STOCK, .custom_loadout = TRUE,
			.goal_radar = ARENA_SET(1) },
		.old_block_hash = "346c601729f9307efbad5b0e33baeb19943ffed8" },
	{ .revision = 5,
		.old_row = { .name = "AE 2V2 BALL", .build = build_game_variant_team_oddball, .flags = ARENA_REV1_GAMETYPE_FLAGS | ARENA_REV1_CASUAL_FLAGS,
			.score_to_win = 5, .time_limit = 15, .respawn_time = ARENA_STOCK, .suicide_penalty = ARENA_STOCK,
			.radar = ARENA_RADAR_ON, .vehicle_set = ARENA_VEHICLES_STOCK, .custom_loadout = TRUE },
		.new_row = { .name = "AE 2V2 BALL", .build = build_game_variant_team_oddball, .flags = ARENA_REV5_CASUAL_FLAGS,
			.score_to_win = 5, .time_limit = 15, .respawn_time = ARENA_STOCK, .suicide_penalty = ARENA_STOCK,
			.radar = ARENA_RADAR_ON, .vehicle_set = ARENA_VEHICLES_STOCK, .custom_loadout = TRUE,
			.goal_radar = ARENA_SET(1) },
		.old_block_hash = "8f34e1990df8f57a876c3c300fbfb45021a4cf99" },
	{ .revision = 5,
		.old_row = { .name = "AE TRAINING", .build = build_game_variant_slayer, .flags = ARENA_REV4_TRAINING_FLAGS,
			.score_to_win = 500, .time_limit = 0, .respawn_time = ARENA_STOCK, .suicide_penalty = ARENA_STOCK,
			.radar = ARENA_RADAR_STOCK, .vehicle_set = ARENA_VEHICLES_DEFAULT, .custom_loadout = TRUE },
		.new_row = { .name = "AE TRAINING", .build = build_game_variant_slayer, .flags = ARENA_REV5_TRAINING_FLAGS,
			.score_to_win = 500, .time_limit = 0, .respawn_time = ARENA_STOCK, .suicide_penalty = ARENA_STOCK,
			.radar = ARENA_RADAR_STOCK, .vehicle_set = ARENA_VEHICLES_DEFAULT, .custom_loadout = TRUE },
		.old_block_hash = "712f8cb5dfa5753d175ed994bb60ee8f446766dd" },
	{ .revision = 5,
		.old_row = { .name = "AE COMP FFA", .build = build_game_variant_slayer, .flags = ARENA_REV1_GAMETYPE_FLAGS,
			.score_to_win = 25, .time_limit = 0, .respawn_time = 150, .suicide_penalty = 150,
			.radar = ARENA_RADAR_ON, .vehicle_set = ARENA_VEHICLES_STOCK, .custom_loadout = TRUE },
		.new_row = { .name = "AE COMP FFA", .build = build_game_variant_slayer, .flags = ARENA_REV5_COMP_FLAGS,
			.score_to_win = 25, .time_limit = 0, .respawn_time = 150, .suicide_penalty = 150,
			.radar = ARENA_RADAR_OFF, .vehicle_set = ARENA_VEHICLES_NONE, .custom_loadout = TRUE,
			.starting_frags = 2 },
		.old_block_hash = "847e55c5c8eb0f86fbde7dd371137324f880be97" },
	{ .revision = 5,
		.old_row = { .name = "AE COMP TS", .build = build_game_variant_team_slayer, .flags = ARENA_REV1_GAMETYPE_FLAGS,
			.score_to_win = 50, .time_limit = 0, .respawn_time = 150, .suicide_penalty = 150,
			.radar = ARENA_RADAR_OFF, .vehicle_set = ARENA_VEHICLES_STOCK, .custom_loadout = TRUE },
		.new_row = { .name = "AE COMP TS", .build = build_game_variant_team_slayer, .flags = ARENA_REV5_COMP_FLAGS,
			.score_to_win = 50, .time_limit = 0, .respawn_time = 150, .suicide_penalty = 150,
			.radar = ARENA_RADAR_OFF, .vehicle_set = ARENA_VEHICLES_NONE, .custom_loadout = TRUE,
			.starting_frags = 2 },
		.old_block_hash = "b25d34b2752866bb09622e06e0d764a9399b5c76" },
	{ .revision = 5,
		.old_row = { .name = "AE COMP CTF", .build = build_game_variant_ctf, .flags = ARENA_REV1_GAMETYPE_FLAGS,
			.score_to_win = 3, .time_limit = 15, .respawn_time = 150, .suicide_penalty = 150,
			.radar = ARENA_RADAR_OFF, .vehicle_set = ARENA_VEHICLES_STOCK, .custom_loadout = TRUE },
		.new_row = { .name = "AE COMP CTF", .build = build_game_variant_ctf, .flags = ARENA_REV5_COMP_OBJECTIVE_FLAGS,
			.score_to_win = 3, .time_limit = 15, .respawn_time = 150, .suicide_penalty = 150,
			.radar = ARENA_RADAR_OFF, .vehicle_set = ARENA_VEHICLES_NONE, .custom_loadout = TRUE,
			.goal_radar = ARENA_SET(1), .starting_frags = 2 },
		.old_block_hash = "395315d4327dbf299a69ad0930a156764b52c46c" },
	{ .revision = 5,
		.old_row = { .name = "AE COMP KOH", .build = build_game_variant_team_king, .flags = ARENA_REV1_GAMETYPE_FLAGS,
			.score_to_win = 5, .time_limit = 15, .respawn_time = 150, .suicide_penalty = 150,
			.radar = ARENA_RADAR_OFF, .vehicle_set = ARENA_VEHICLES_STOCK, .custom_loadout = TRUE },
		.new_row = { .name = "AE COMP KOH", .build = build_game_variant_team_king, .flags = ARENA_REV5_COMP_OBJECTIVE_FLAGS,
			.score_to_win = 5, .time_limit = 15, .respawn_time = 150, .suicide_penalty = 150,
			.radar = ARENA_RADAR_OFF, .vehicle_set = ARENA_VEHICLES_NONE, .custom_loadout = TRUE,
			.goal_radar = ARENA_SET(1), .starting_frags = 2 },
		.old_block_hash = "293e8e5c01828efc30a5ce5d940c2f5b69f8d87c" },
	{ .revision = 5,
		.old_row = { .name = "AE COMP OB", .build = build_game_variant_team_oddball, .flags = ARENA_REV1_GAMETYPE_FLAGS,
			.score_to_win = 5, .time_limit = 15, .respawn_time = 150, .suicide_penalty = 150,
			.radar = ARENA_RADAR_OFF, .vehicle_set = ARENA_VEHICLES_STOCK, .custom_loadout = TRUE },
		.new_row = { .name = "AE COMP OB", .build = build_game_variant_team_oddball, .flags = ARENA_REV5_COMP_OBJECTIVE_FLAGS,
			.score_to_win = 5, .time_limit = 15, .respawn_time = 150, .suicide_penalty = 150,
			.radar = ARENA_RADAR_OFF, .vehicle_set = ARENA_VEHICLES_NONE, .custom_loadout = TRUE,
			.goal_radar = ARENA_SET(1), .starting_frags = 2 },
		.old_block_hash = "36b8064cee361f91c7f095fbc9a3a1b466af5052" },
	{ .revision = 5,
		.old_row = { .name = "AE TEAM OB", .build = build_game_variant_team_oddball, .flags = ARENA_REV4_LATER_FLAGS,
			.score_to_win = 5, .time_limit = 15, .respawn_time = ARENA_STOCK, .suicide_penalty = ARENA_STOCK,
			.radar = ARENA_RADAR_ON, .vehicle_set = ARENA_VEHICLES_STOCK, .custom_loadout = TRUE,
			.goal_radar = ARENA_SET(1) },
		.new_row = { .name = "AE TEAM OB", .build = build_game_variant_team_oddball, .flags = ARENA_REV5_CASUAL_FLAGS,
			.score_to_win = 5, .time_limit = 15, .respawn_time = ARENA_STOCK, .suicide_penalty = ARENA_STOCK,
			.radar = ARENA_RADAR_ON, .vehicle_set = ARENA_VEHICLES_STOCK, .custom_loadout = TRUE,
			.goal_radar = ARENA_SET(1) },
		.old_block_hash = "e68edf81dbb93ab66b9c6b7bf85620bac318645a" },
	{ .revision = 5,
		.old_row = { .name = "AE PRACTICE", .build = build_game_variant_slayer, .flags = ARENA_REV4_PRACTICE_FLAGS,
			.score_to_win = 500, .time_limit = 0, .respawn_time = ARENA_STOCK, .suicide_penalty = ARENA_STOCK,
			.radar = ARENA_RADAR_ON, .vehicle_set = ARENA_VEHICLES_DEFAULT, .custom_loadout = TRUE },
		.new_row = { .name = "AE PRACTICE", .build = build_game_variant_slayer, .flags = ARENA_REV5_PRACTICE_FLAGS,
			.score_to_win = 500, .time_limit = 0, .respawn_time = ARENA_STOCK, .suicide_penalty = ARENA_STOCK,
			.radar = ARENA_RADAR_ON, .vehicle_set = ARENA_VEHICLES_DEFAULT, .custom_loadout = TRUE },
		.old_block_hash = "14ffdc5522d9ed8c55b83dc1e0d871e5bd56bde3" },
	{ .revision = 5,
		.old_row = { .name = "AE SNIPERS", .build = build_game_variant_team_slayer, .flags = ARENA_REV4_LATER_FLAGS,
			.score_to_win = 50, .time_limit = 15, .respawn_time = ARENA_STOCK, .suicide_penalty = ARENA_STOCK,
			.radar = ARENA_RADAR_OFF, .vehicle_set = ARENA_VEHICLES_STOCK, .custom_loadout = TRUE,
			.weapon_set = ARENA_SET(4), .loadout_primary = ARENA_SET(5), .loadout_secondary = ARENA_SET(3) },
		.new_row = { .name = "AE SNIPERS", .build = build_game_variant_team_slayer, .flags = ARENA_REV5_CASUAL_FLAGS,
			.score_to_win = 50, .time_limit = 15, .respawn_time = ARENA_STOCK, .suicide_penalty = ARENA_STOCK,
			.radar = ARENA_RADAR_OFF, .vehicle_set = ARENA_VEHICLES_STOCK, .custom_loadout = TRUE,
			.weapon_set = ARENA_SET(4), .loadout_primary = ARENA_SET(5), .loadout_secondary = ARENA_SET(3) },
		.old_block_hash = "b1877224d04812b9d899730b82a7828281e89bcd" },
	{ .revision = 5,
		.old_row = { .name = "AE SHOTSNIP", .build = build_game_variant_team_slayer, .flags = ARENA_REV4_LATER_FLAGS,
			.score_to_win = 50, .time_limit = 15, .respawn_time = ARENA_STOCK, .suicide_penalty = ARENA_STOCK,
			.radar = ARENA_RADAR_OFF, .vehicle_set = ARENA_VEHICLES_STOCK, .custom_loadout = TRUE,
			.loadout_primary = ARENA_SET(4), .loadout_secondary = ARENA_SET(5), .no_map_weapons = TRUE },
		.new_row = { .name = "AE SHOTSNIP", .build = build_game_variant_team_slayer, .flags = ARENA_REV5_CASUAL_FLAGS,
			.score_to_win = 50, .time_limit = 15, .respawn_time = ARENA_STOCK, .suicide_penalty = ARENA_STOCK,
			.radar = ARENA_RADAR_OFF, .vehicle_set = ARENA_VEHICLES_STOCK, .custom_loadout = TRUE,
			.loadout_primary = ARENA_SET(4), .loadout_secondary = ARENA_SET(5), .no_map_weapons = TRUE },
		.old_block_hash = "514fbcf2f960bfc7c4d7bd2420470567afcfdc4f" },
	{ .revision = 5,
		.old_row = { .name = "AE SWAT", .build = build_game_variant_team_slayer, .flags = ARENA_REV4_SWAT_FLAGS,
			.score_to_win = 50, .time_limit = 15, .respawn_time = ARENA_STOCK, .suicide_penalty = ARENA_STOCK,
			.radar = ARENA_RADAR_OFF, .vehicle_set = ARENA_VEHICLES_STOCK, .custom_loadout = TRUE,
			.weapon_set = ARENA_SET(1), .loadout_primary = ARENA_SET(3), .loadout_secondary = ARENA_SET(0) },
		.new_row = { .name = "AE SWAT", .build = build_game_variant_team_slayer, .flags = ARENA_REV5_SWAT_FLAGS,
			.score_to_win = 50, .time_limit = 15, .respawn_time = ARENA_STOCK, .suicide_penalty = ARENA_STOCK,
			.radar = ARENA_RADAR_OFF, .vehicle_set = ARENA_VEHICLES_STOCK, .custom_loadout = TRUE,
			.weapon_set = ARENA_SET(1), .loadout_primary = ARENA_SET(3), .loadout_secondary = ARENA_SET(0) },
		.old_block_hash = "745fe6a2cc653deb823a76a49e981d463acce170" },
	{ .revision = 5,
		.old_row = { .name = "AE ROCKETS", .build = build_game_variant_team_slayer, .flags = ARENA_REV4_LATER_FLAGS,
			.score_to_win = 50, .time_limit = 15, .respawn_time = ARENA_STOCK, .suicide_penalty = ARENA_STOCK,
			.radar = ARENA_RADAR_ON, .vehicle_set = ARENA_VEHICLES_STOCK, .custom_loadout = TRUE,
			.weapon_set = ARENA_SET(6), .loadout_primary = ARENA_SET(6), .loadout_secondary = ARENA_SET(0) },
		.new_row = { .name = "AE ROCKETS", .build = build_game_variant_team_slayer, .flags = ARENA_REV5_CASUAL_FLAGS,
			.score_to_win = 50, .time_limit = 15, .respawn_time = ARENA_STOCK, .suicide_penalty = ARENA_STOCK,
			.radar = ARENA_RADAR_ON, .vehicle_set = ARENA_VEHICLES_STOCK, .custom_loadout = TRUE,
			.weapon_set = ARENA_SET(6), .loadout_primary = ARENA_SET(6), .loadout_secondary = ARENA_SET(0) },
		.old_block_hash = "3171dd59012243d4c1d93b215232601d45c86970" },
	{ .revision = 5,
		.old_row = { .name = "AE SHOTGUNS", .build = build_game_variant_team_slayer, .flags = ARENA_REV4_LATER_FLAGS,
			.score_to_win = 50, .time_limit = 15, .respawn_time = ARENA_STOCK, .suicide_penalty = ARENA_STOCK,
			.radar = ARENA_RADAR_ON, .vehicle_set = ARENA_VEHICLES_STOCK, .custom_loadout = TRUE,
			.weapon_set = ARENA_SET(7), .loadout_primary = ARENA_SET(4), .loadout_secondary = ARENA_SET(3) },
		.new_row = { .name = "AE SHOTGUNS", .build = build_game_variant_team_slayer, .flags = ARENA_REV5_CASUAL_FLAGS,
			.score_to_win = 50, .time_limit = 15, .respawn_time = ARENA_STOCK, .suicide_penalty = ARENA_STOCK,
			.radar = ARENA_RADAR_ON, .vehicle_set = ARENA_VEHICLES_STOCK, .custom_loadout = TRUE,
			.weapon_set = ARENA_SET(7), .loadout_primary = ARENA_SET(4), .loadout_secondary = ARENA_SET(3) },
		.old_block_hash = "4510d923b68db2d9caa978526dac95cc61f87fc9" },
	{ .revision = 5,
		.old_row = { .name = "AE HEAVIES", .build = build_game_variant_team_slayer, .flags = ARENA_REV4_LATER_FLAGS,
			.score_to_win = 75, .time_limit = 15, .respawn_time = ARENA_STOCK, .suicide_penalty = ARENA_STOCK,
			.radar = ARENA_RADAR_ON, .vehicle_set = ARENA_VEHICLES_DEFAULT, .custom_loadout = TRUE,
			.weapon_set = ARENA_SET(13), .loadout_primary = ARENA_SET(6), .loadout_secondary = ARENA_SET(2) },
		.new_row = { .name = "AE HEAVIES", .build = build_game_variant_team_slayer, .flags = ARENA_REV5_CASUAL_FLAGS,
			.score_to_win = 75, .time_limit = 15, .respawn_time = ARENA_STOCK, .suicide_penalty = ARENA_STOCK,
			.radar = ARENA_RADAR_ON, .vehicle_set = ARENA_VEHICLES_DEFAULT, .custom_loadout = TRUE,
			.weapon_set = ARENA_SET(13), .loadout_primary = ARENA_SET(6), .loadout_secondary = ARENA_SET(2) },
		.old_block_hash = "01ae17ad57f6bd2e20d59864e2af6fc250dda7d7" },
	/* revision 6: owner answer A (NHE VANILLA -> AE VANILLA, NHE POWERUP -> AE
	POWERUPS, both on the casual set's values, their NHE modes kept; NHE
	TRAIN folded into AE TRAINING: not seeded, left as it is) and AE SWAT
	without grenades (owner, 2026-10-08). The old rows: revision 3's / 5's */
	{ .revision = 6,
		.old_row = { .name = "NHE VANILLA", .build = build_game_variant_team_slayer, .flags = ARENA_REV1_NHE_FLAGS,
			.score_to_win = 0, .time_limit = 0, .respawn_time = ARENA_STOCK, .suicide_penalty = ARENA_STOCK,
			.radar = ARENA_RADAR_STOCK, .vehicle_set = ARENA_VEHICLES_NONE, .custom_loadout = FALSE, .nhe_mode = _nhe_mode_vanilla },
		.new_row = { .name = "AE VANILLA", .build = build_game_variant_team_slayer, .flags = ARENA_REV5_CASUAL_FLAGS,
			.score_to_win = 50, .time_limit = 15, .respawn_time = ARENA_STOCK, .suicide_penalty = ARENA_STOCK,
			.radar = ARENA_RADAR_ON, .vehicle_set = ARENA_VEHICLES_STOCK, .custom_loadout = TRUE, .nhe_mode = _nhe_mode_vanilla },
		.old_block_hash = "8a9d058b6f15b05a0faed94cf3792aedb4b76806" },
	{ .revision = 6,
		.old_row = { .name = "NHE POWERUP", .build = build_game_variant_team_slayer, .flags = ARENA_REV1_NHE_RULES_FLAGS,
			.score_to_win = 50, .time_limit = 0, .respawn_time = 150, .suicide_penalty = 150,
			.radar = ARENA_RADAR_OFF, .vehicle_set = ARENA_VEHICLES_NONE, .custom_loadout = FALSE, .nhe_mode = _nhe_mode_nhe_and_powerups },
		.new_row = { .name = "AE POWERUPS", .build = build_game_variant_team_slayer, .flags = ARENA_REV5_CASUAL_FLAGS,
			.score_to_win = 50, .time_limit = 15, .respawn_time = ARENA_STOCK, .suicide_penalty = ARENA_STOCK,
			.radar = ARENA_RADAR_ON, .vehicle_set = ARENA_VEHICLES_STOCK, .custom_loadout = TRUE, .nhe_mode = _nhe_mode_nhe_and_powerups },
		.old_block_hash = "5983b3c2cac78def220bd4f42f03e4eb924a054e" },
	{ .revision = 6,
		.old_row = { .name = "AE SWAT", .build = build_game_variant_team_slayer, .flags = ARENA_REV5_SWAT_FLAGS,
			.score_to_win = 50, .time_limit = 15, .respawn_time = ARENA_STOCK, .suicide_penalty = ARENA_STOCK,
			.radar = ARENA_RADAR_OFF, .vehicle_set = ARENA_VEHICLES_STOCK, .custom_loadout = TRUE,
			.weapon_set = ARENA_SET(1), .loadout_primary = ARENA_SET(3), .loadout_secondary = ARENA_SET(0) },
		.new_row = { .name = "AE SWAT", .build = build_game_variant_team_slayer, .flags = ARENA_REV5_SWAT_FLAGS,
			.score_to_win = 50, .time_limit = 15, .respawn_time = ARENA_STOCK, .suicide_penalty = ARENA_STOCK,
			.radar = ARENA_RADAR_OFF, .vehicle_set = ARENA_VEHICLES_STOCK, .custom_loadout = TRUE,
			.weapon_set = ARENA_SET(1), .loadout_primary = ARENA_SET(3), .loadout_secondary = ARENA_SET(0),
			.starting_frags = 0xFF },
		.old_block_hash = "55b0a0eee3f300a6789734e71851be8b4ac930df" },
};

/* debug.arena_test_migration: a same-name revision past the current one
(AE TEAM SLY's score 50 -> 51), for the automated tests of value updates:
the machinery's revisions 3 and 5 kind */
static struct arena_gametype_migration const arena_gametype_test_migration =
{
	.revision = ARENA_GAMETYPES_REVISION + 1,
	.old_row = { .name = "AE TEAM SLY", .build = build_game_variant_team_slayer,
		.flags = ARENA_REV5_CASUAL_FLAGS,
		.score_to_win = 50, .time_limit = 15, .respawn_time = ARENA_STOCK, .suicide_penalty = ARENA_STOCK,
		.radar = ARENA_RADAR_ON, .vehicle_set = ARENA_VEHICLES_STOCK, .custom_loadout = TRUE },
	.new_row = { .name = "AE TEAM SLY", .build = build_game_variant_team_slayer,
		.flags = ARENA_REV5_CASUAL_FLAGS,
		.score_to_win = 51, .time_limit = 15, .respawn_time = ARENA_STOCK, .suicide_penalty = ARENA_STOCK,
		.radar = ARENA_RADAR_ON, .vehicle_set = ARENA_VEHICLES_STOCK, .custom_loadout = TRUE },
	.old_block_hash = NULL
};

/* the migrations whose old row no longer builds as its old_block_hash says
(arena_gametypes_self_check): skipped */
static boolean arena_gametype_migration_unsafe[NUMBEROF(arena_gametype_migrations) + 1];

/* port_config.c's */
long config_integer(char const *name);


static char const arena_gametypes_record_path[] = "z:\\saved\\playlists\\arena_gametypes.txt";
/* (the record being written: renamed over the record once complete) */
static char const arena_gametypes_record_new_path[] = "z:\\saved\\playlists\\arena_gametypes.new";
static char const arena_gametypes_backup_path[] = "z:\\saved\\playlists_backup";


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
static void arena_gametypes_migrate(
	char *record,
	unsigned long *record_length,
	short record_revision,
	boolean *record_changed,
	boolean *written,
	short *deferred_revision,
	char *deferred_names);
static void arena_gametypes_self_check(
	void);
static void arena_gametypes_remove_stray_updates(
	void);
static boolean arena_gametypes_record_read(
	char *record,
	unsigned long *record_length,
	short *revision,
	boolean *unreadable);
static void arena_gametypes_record_write(
	char const *record,
	unsigned long record_length,
	short revision);
static short arena_gametypes_migration_count(
	void);
static struct arena_gametype_migration const *arena_gametypes_migration(
	short index);
static short arena_gametypes_revision(
	void);
static boolean arena_gametypes_row_begun(
	short revision,
	char const *old_name,
	boolean mark);
static int arena_gametype_name_compare(
	wchar_t const *a,
	wchar_t const *b);
static void arena_gametype_log(
	struct arena_gametype const *gametype,
	struct game_variant const *variant,
	struct game_variant_options const *options);

const char *config_string(const char *name);
int config_boolean(const char *name);

/* ---------- public code */

boolean arena_gametypes_seed(
	void)
{
	/* the record's names, after a newline (each name is looked for as
	"\nNAME\n"), without its "#revision N" line */
	char record[ARENA_GAMETYPES_RECORD_SIZE + 3];
	unsigned long record_length = 1;
	short record_revision = 0;
	boolean record_changed = FALSE;
	boolean record_unreadable = FALSE;
	boolean written = FALSE;
	/* (the lowest revision with a migration put off to the next start, and
	the new names it is not to seed this start, "\nNAME\n" each) */
	short deferred_revision = NONE;
	char deferred_names[ARENA_GAMETYPES_RECORD_SIZE];
	short index;

	csmemset(record, 0, sizeof(record));
	record[0] = '\n';
	csmemset(deferred_names, 0, sizeof(deferred_names));
	deferred_names[0] = '\n';
	record_changed = arena_gametypes_record_read(record, &record_length, &record_revision, &record_unreadable);
	/* (a record there but not read: nothing seeded or migrated, which
	would bring back what the player deleted; tried again next start) */
	if (record_unreadable)
		return FALSE;

	/* (the frozen rows checked first: a migration whose old row no longer
	builds as seeded is skipped) */
	arena_gametypes_self_check();
	arena_gametypes_remove_stray_updates();
	/* (those seeded under their old names: migrated if unchanged) */
	arena_gametypes_migrate(record, &record_length, record_revision, &record_changed, &written, &deferred_revision,
		deferred_names);

	for (index = 0; index < NUMBEROF(arena_gametypes); index++)
	{
		struct arena_gametype const *gametype = &arena_gametypes[index];
		char line[ARENA_GAMETYPE_NAME_LENGTH + 2];

		_snprintf(line, sizeof(line), "\n%s\n", gametype->name);
		line[sizeof(line) - 1] = 0;
		/* (seeded before: deleted, renamed or edited since, it stays so; or
		its migration put off: not seeded beside the old one) */
		if (strstr(record, line) || strstr(deferred_names, line))
			continue;
		if (!arena_gametype_write(gametype, &written))
			continue;
		if (arena_gametypes_record_add(record, &record_length, gametype->name))
			record_changed = TRUE;
	}

	/* (never a lower revision: a record a newer build wrote keeps its own,
	so its revisions do not run again after a rollback; a migration put off
	keeps its revision unrecorded, so it runs again next start: the others
	of it and after it find themselves done) */
	{
		short revision = deferred_revision != NONE ? (short)(deferred_revision - 1) : arena_gametypes_revision();

		revision = MAX(record_revision, revision);
		if (record_changed || record_revision < revision)
			arena_gametypes_record_write(record, record_length, revision);
	}
	if (record_length > ARENA_GAMETYPES_RECORD_OLD_LIMIT)
	{
		error(_error_silent, "arena gametypes: warning: the record is %lu bytes; builds before the migrations "
			"(a17102e2) read at most 1024 and would seed again", record_length);
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

/* a saved gametype by its stored name (letters' case aside): its profile index, else NONE; the
nth match when the names are the same but for case (the save directories tell them apart) */
static long arena_gametype_saved_by_name(
	wchar_t const *stored_name,
	short nth)
{
	long saved[ARENA_GAMETYPES_MAXIMUM_SAVED];
	word saved_count = NUMBEROF(saved);
	short index;

	saved_game_files_enumerate_available_to_local_player_index(NONE,
		_saved_game_file_type_game_variant, &saved_count, saved, FALSE);
	for (index = 0; index < (short)saved_count; index++)
	{
		wchar_t display_name[MAX_GAMENAME];

		display_name[0] = 0;
		if (playlist_profile_get_display_name(saved[index], display_name) &&
			!arena_gametype_name_compare(display_name, stored_name) && nth-- == 0)
		{
			return saved[index];
		}
	}
	return NONE;
}

/* debug.display_name_log: a line each time a gametype file is read for an own display name */
static void arena_gametype_own_name_log(
	char const *what,
	long profile_index)
{
	if (config_boolean("debug.display_name_log"))
	{
		error(_error_silent, "arena gametypes: own display name read (%s, profile 0x%lX): file read %ld",
			what, profile_index, playlist_profile_block_reads_get());
	}
}

boolean arena_gametype_own_display_name(
	long profile_index,
	wchar_t *display_name,
	short size)
{
	/* (the lists ask for every row every frame: what a file held is kept until a gametype file changes) */
	enum { CACHE_SIZE = 128 };
	static struct
	{
		long profile_index;
		boolean found;
		wchar_t name[PLAYLIST_DISPLAY_NAME_MAXIMUM_LENGTH + 1];
	} cache[CACHE_SIZE];
	static short cache_count = 0;
	static long cache_generation = -1;
	wchar_t stored_name[MAX_GAMENAME];
	short entry;
	short index;

	if (!display_name || size <= 0)
		return FALSE;
	display_name[0] = 0;
	/* (a seeded gametype has none: its file is never read) */
	if (!playlist_profile_get_display_name(profile_index, stored_name) || arena_gametype_info(stored_name, NULL))
		return FALSE;
	if (cache_generation != playlist_profile_content_generation_get())
	{
		cache_generation = playlist_profile_content_generation_get();
		cache_count = 0;
	}
	for (entry = 0; entry < cache_count && cache[entry].profile_index != profile_index; entry++)
		;
	if (entry == cache_count)
	{
		if (cache_count == CACHE_SIZE)
			cache_count = 0;
		entry = cache_count++;
		cache[entry].profile_index = profile_index;
		cache[entry].name[0] = 0;
		arena_gametype_own_name_log("list", profile_index);
		cache[entry].found = playlist_profile_get_own_display_name(profile_index, cache[entry].name);
	}
	if (!cache[entry].found)
		return FALSE;
	for (index = 0; index < size - 1 && cache[entry].name[index]; index++)
		display_name[index] = cache[entry].name[index];
	display_name[index] = 0;
	return index > 0;
}

boolean arena_gametype_own_display_name_for_stored_name(
	wchar_t const *stored_name,
	struct game_variant const *variant,
	wchar_t *display_name,
	short size)
{
	/* (the last one asked: a lobby or a scoreboard asks every frame; it holds while the variant is the same
	bytes and no gametype file changed) */
	static wchar_t memo_stored[ARENA_GAMETYPE_NAME_LENGTH];
	static byte memo_variant[sizeof(struct game_variant)];
	static wchar_t memo_name[PLAYLIST_DISPLAY_NAME_MAXIMUM_LENGTH + 1];
	static boolean memo_found = FALSE;
	static long memo_generation = -1;
	static boolean memo_valid = FALSE;
	wchar_t key[ARENA_GAMETYPE_NAME_LENGTH];
	short index;

	if (!display_name || size <= 0)
		return FALSE;
	display_name[0] = 0;
	if (!stored_name || !stored_name[0] || !variant)
		return FALSE;
	csmemset(key, 0, sizeof(key));
	for (index = 0; index < ARENA_GAMETYPE_NAME_LENGTH - 1 && stored_name[index]; index++)
		key[index] = stored_name[index];
	if (!memo_valid || memo_generation != playlist_profile_content_generation_get() ||
		arena_gametype_name_compare(memo_stored, key) || csmemcmp(memo_variant, variant, sizeof(memo_variant)))
	{
		short nth;

		memo_generation = playlist_profile_content_generation_get();
		csmemcpy(memo_stored, key, sizeof(memo_stored));
		csmemcpy(memo_variant, variant, sizeof(memo_variant));
		memo_name[0] = 0;
		memo_found = FALSE;
		memo_valid = TRUE;
		/* (a seeded gametype has none; an own one counts only when its file is the variant in play:
		a joiner's gametype of the same name but other rules is not the host's) */
		if (!arena_gametype_info(key, NULL))
		{
			for (nth = 0; !memo_found; nth++)
			{
				long profile_index = arena_gametype_saved_by_name(key, nth);

				if (profile_index == NONE)
					break;
				arena_gametype_own_name_log("stored name", profile_index);
				memo_found = playlist_profile_get_own_display_name_for_variant(profile_index, variant, memo_name);
			}
		}
	}
	if (!memo_found)
		return FALSE;
	for (index = 0; index < size - 1 && memo_name[index]; index++)
		display_name[index] = memo_name[index];
	display_name[index] = 0;
	return index > 0;
}

boolean arena_gametype_set_own_display_name(
	long profile_index,
	wchar_t const *display_name)
{
	wchar_t stored_name[MAX_GAMENAME];

	stored_name[0] = 0;
	if (!playlist_profile_get_display_name(profile_index, stored_name) ||
		arena_gametype_info(stored_name, NULL))
	{
		return FALSE;
	}
	return playlist_profile_set_own_display_name(profile_index, display_name);
}

/* debug.set_display_name ("<stored name>=<display name>"): once at the start,
that own gametype gets that display name (the automated tests' way to make
one before the AE menus can); logged. Never on a real save root */
void arena_gametypes_debug_set_display_name(
	void)
{
	char const *value = config_string("debug.set_display_name");
	char const *equals;
	wchar_t stored_name[ARENA_GAMETYPE_NAME_LENGTH];
	wchar_t display_name[PLAYLIST_DISPLAY_NAME_MAXIMUM_LENGTH + 1];
	short index;
	long profile_index;

	if (!value || !value[0] || !(equals = strchr(value, '=')))
		return;
	csmemset(stored_name, 0, sizeof(stored_name));
	for (index = 0; index < ARENA_GAMETYPE_NAME_LENGTH - 1 && value + index < equals; index++)
		stored_name[index] = (wchar_t)(unsigned char)value[index];
	csmemset(display_name, 0, sizeof(display_name));
	for (index = 0; index < PLAYLIST_DISPLAY_NAME_MAXIMUM_LENGTH && equals[1 + index]; index++)
		display_name[index] = (wchar_t)(unsigned char)equals[1 + index];
	profile_index = arena_gametype_saved_by_name(stored_name, 0);
	if (profile_index == NONE)
	{
		error(_error_silent, "arena gametypes: debug.set_display_name: no saved gametype '%.*s'",
			(int)(equals - value), value);
		return;
	}
	error(_error_silent, "arena gametypes: debug.set_display_name: '%.*s' -> '%s': %s", (int)(equals - value), value,
		equals + 1, arena_gametype_set_own_display_name(profile_index, display_name) ? "set" : "refused");
}

/* ---------- private code */

/* a name's line added to the record, if it fits: TRUE when added */
static boolean arena_gametypes_record_add(
	char *record,
	unsigned long *record_length,
	char const *name)
{
	unsigned long line_length = (unsigned long)strlen(name) + 1;

	/* (room left for the "#revision N" line: the file stays within what
	the reader takes) */
	if (*record_length + line_length > ARENA_GAMETYPES_RECORD_SIZE - ARENA_GAMETYPES_REVISION_LINE)
		return FALSE;
	csmemcpy(record + *record_length, name, line_length - 1);
	record[*record_length + line_length - 1] = '\n';
	*record_length += line_length;
	record[*record_length] = 0;
	return TRUE;
}

/* a record file read into the record's buffer (after its first newline),
its "#revision N" line taken out: FALSE when it is not there or too long */
static boolean arena_gametypes_record_read_file(
	char const *path,
	char *record,
	unsigned long *record_length,
	short *revision)
{
	struct file_reference file;
	unsigned long length;
	unsigned long character;
	boolean success = FALSE;

	if (!file_reference_create_from_path(&file, path, FALSE) || !file_exists(&file) ||
		!file_open(&file, FLAG(_permission_read_bit)))
	{
		return FALSE;
	}
	length = file_get_eof(&file);
	if (length <= ARENA_GAMETYPES_RECORD_SIZE && file_read(&file, length, record + 1))
	{
		success = TRUE;
		*record_length = 1 + length;
		record[*record_length] = 0;
		/* (a record edited by hand: its line ends, and its last line's) */
		for (character = 1; character < *record_length; character++)
		{
			if (record[character] == '\r')
				record[character] = '\n';
		}
		if (record[*record_length - 1] != '\n')
		{
			record[(*record_length)++] = '\n';
			record[*record_length] = 0;
		}
		/* (its revision: the first line's; none, 0) */
		*revision = 0;
		if (!strncmp(record + 1, "#revision ", 10))
		{
			char *end = strchr(record + 1, '\n');
			unsigned long line_length = (unsigned long)(end - (record + 1)) + 1;

			*revision = (short)atol(record + 11);
			csmemmove(record + 1, record + 1 + line_length, *record_length - 1 - line_length + 1);
			*record_length -= line_length;
		}
	}
	else
	{
		error(_error_silent, "arena gametypes: the record '%s' is not read (%lu bytes)", path, length);
	}
	file_close(&file);

	return success;
}

/* the record, else (a start that ended between replacing the record and
putting the new one in its place: no record there) the new one: TRUE then,
to be put in place. A new one beside the record is a start's that ended
before it was put in place: dropped (the record is whole). A record there
that is not read: *unreadable, and both left as they are */
static boolean arena_gametypes_record_read(
	char *record,
	unsigned long *record_length,
	short *revision,
	boolean *unreadable)
{
	struct file_reference record_file;
	struct file_reference record_new;
	boolean record_there = file_reference_create_from_path(&record_file, arena_gametypes_record_path, FALSE) &&
		file_exists(&record_file);
	boolean new_there = file_reference_create_from_path(&record_new, arena_gametypes_record_new_path, FALSE) &&
		file_exists(&record_new);

	*revision = 0;
	*unreadable = FALSE;
	if (record_there)
	{
		if (!arena_gametypes_record_read_file(arena_gametypes_record_path, record, record_length, revision))
		{
			error(_error_silent, "arena gametypes: the record '%s' is there but not read: nothing seeded this start",
				arena_gametypes_record_path);
			*unreadable = TRUE;
			return FALSE;
		}
		if (new_there)
		{
			error(_error_silent, "arena gametypes: a record not put in place '%s' dropped", arena_gametypes_record_new_path);
			file_delete(&record_new);
		}
		return FALSE;
	}
	if (new_there)
	{
		if (arena_gametypes_record_read_file(arena_gametypes_record_new_path, record, record_length, revision))
		{
			error(_error_silent, "arena gametypes: the record recovered from '%s'", arena_gametypes_record_new_path);
			return TRUE;
		}
		error(_error_silent, "arena gametypes: '%s' is there but not read: nothing seeded this start",
			arena_gametypes_record_new_path);
		*unreadable = TRUE;
	}

	return FALSE;
}

/* port/linux/src/xbox_files.c's: a file put in another's place in one step
(the other replaced at once: rename(2); MoveFileEx on Windows) */
int platform_replace_file(char const *path, char const *new_path);

/* the record written (its "#revision N" line, then the names) to a new
file, then put in the old one's place in one step: a start that ends at any
moment leaves a whole record */
static void arena_gametypes_record_write(
	char const *record,
	unsigned long record_length,
	short revision)
{
	struct file_reference file;
	char revision_line[ARENA_GAMETYPES_REVISION_LINE];
	boolean success = FALSE;

	_snprintf(revision_line, sizeof(revision_line) - 1, "#revision %d\n", (int)revision);
	revision_line[sizeof(revision_line) - 1] = 0;
	if (file_reference_create_from_path(&file, arena_gametypes_record_new_path, FALSE) &&
		(file_exists(&file) || file_create(&file)) &&
		file_open(&file, FLAG(_permission_write_bit)))
	{
		success = file_set_eof(&file, 0) &&
			file_write(&file, (unsigned long)strlen(revision_line), revision_line) &&
			(record_length <= 1 || file_write(&file, record_length - 1, record + 1));
		file_close(&file);
	}
	if (!success)
	{
		error(_error_silent, "arena gametypes: failed to write the record '%s'", arena_gametypes_record_new_path);
		return;
	}
	if (!platform_replace_file(arena_gametypes_record_new_path, arena_gametypes_record_path))
		error(_error_silent, "arena gametypes: failed to put '%s' in place", arena_gametypes_record_new_path);

	return;
}

/* a saved gametype of that name (letters' case aside) among the saved ones,
else NONE */
static long arena_gametype_find_saved(
	wchar_t const *name,
	long const *saved,
	word saved_count)
{
	short index;

	for (index = 0; index < (short)saved_count; index++)
	{
		wchar_t display_name[MAX_GAMENAME];

		display_name[0] = 0;
		if (playlist_profile_get_display_name(saved[index], display_name) &&
			!arena_gametype_name_compare(display_name, name))
		{
			return saved[index];
		}
	}
	return NONE;
}

/* a directory made if it is not there: TRUE when it is */
static boolean arena_gametypes_directory(
	char const *path)
{
	struct file_reference directory;

	return file_reference_create_from_path(&directory, path, TRUE) &&
		(file_exists(&directory) || file_create(&directory));
}

/* a file copied into a directory under its own name, never over a file
there (one there already is kept: an earlier backup's): TRUE when the copy
is there. must_match: one there already counts only with the same bytes (a
gametype's file); else it counts as it is (the record's, from the
revision's first start: the record before the revision) */
static boolean arena_gametypes_backup_file(
	struct file_reference *source,
	char const *directory_path,
	boolean must_match)
{
	char name[MAXIMUM_FILENAME_LENGTH + 1];
	char path[MAXIMUM_FILENAME_LENGTH + 1];
	struct file_reference destination;
	unsigned long size = 0;
	void *data;
	boolean success = FALSE;

	name[0] = 0;
	file_reference_get_name(source, FLAG(_name_filename_bit) | FLAG(_name_extension_bit), name);
	if (!name[0])
		return FALSE;
	_snprintf(path, sizeof(path) - 1, "%s\\%s", directory_path, name);
	path[sizeof(path) - 1] = 0;
	if (!file_reference_create_from_path(&destination, path, FALSE))
		return FALSE;
	data = file_read_into_memory(source, &size);
	if (!data)
		return FALSE;
	if (file_exists(&destination) && !must_match)
	{
		unsigned long backup_size = 0;
		char *backup = file_read_into_memory(&destination, &backup_size);
		boolean parses = backup && backup_size > 0 && backup_size <= ARENA_GAMETYPES_RECORD_SIZE;
		unsigned long character;

		/* (the record's backup from this revision's first start: the
		record as it was before the revision, which is what to keep; the
		record has changed since. Only a whole one, though: not empty, a
		record's size, its lines text; else it is made again, from the
		record as it is now: after a put-off revision's earlier start, a
		later state than before the revision) */
		for (character = 0; parses && character < backup_size; character++)
		{
			char c = backup[character];

			parses = (c >= ' ' && c <= '~') || c == '\n' || c == '\r';
		}
		if (backup)
			free(backup);
		if (parses)
		{
			error(_error_silent, "arena gametypes: backup '%s' is there already: kept (the earlier one)", path);
			free(data);
			return TRUE;
		}
		error(_error_silent, "arena gametypes: backup '%s' is there but not a record: made again from the record "
			"as it is now (perhaps later than before the revision)", path);
		file_delete(&destination);
	}
	if (file_exists(&destination))
	{
		unsigned long backup_size = 0;
		void *backup = file_read_into_memory(&destination, &backup_size);

		/* (never written over: an earlier backup of the same bytes will do;
		any other, a half-written one, fails this one) */
		success = backup && backup_size == size && !csmemcmp(backup, data, size);
		if (backup)
			free(backup);
		free(data);
		error(_error_silent, "arena gametypes: backup '%s' is there already: %s", path,
			success ? "the same, kept" : "different, kept (this gametype not migrated)");
		return success;
	}
	/* (written to a temporary file, then put in place: a start that ends
	at any moment leaves no half backup) */
	{
		char temporary_path[MAXIMUM_FILENAME_LENGTH + 1];
		struct file_reference temporary;

		_snprintf(temporary_path, sizeof(temporary_path) - 1, "%s.tmp", path);
		temporary_path[sizeof(temporary_path) - 1] = 0;
		if (file_reference_create_from_path(&temporary, temporary_path, FALSE) &&
			(file_exists(&temporary) || file_create(&temporary)) &&
			file_open(&temporary, FLAG(_permission_write_bit)))
		{
			success = file_set_eof(&temporary, 0) && file_write(&temporary, size, data);
			file_close(&temporary);
			success = success && platform_replace_file(temporary_path, path);
			if (!success && file_exists(&temporary))
				file_delete(&temporary);
		}
	}
	free(data);
	if (success)
		error(_error_silent, "arena gametypes: backed up '%s'", path);
	return success;
}

/* before a revision's first rewrite, the record (once), then the gametype's
directory's files, into z:\saved\playlists_backup\revision_N\ (its
directory under its own name): TRUE when all are there */
static boolean arena_gametypes_backup(
	short revision,
	long profile_index,
	boolean *record_backed_up)
{
	char revision_path[MAXIMUM_FILENAME_LENGTH + 1];
	char gametype_path[MAXIMUM_FILENAME_LENGTH + 1];
	char directory_path[MAXIMUM_FILENAME_LENGTH + 1];
	struct file_reference files[ARENA_GAMETYPES_BACKUP_FILES];
	struct file_reference directory;
	char *directory_name;
	long file_count;
	long index;

	_snprintf(revision_path, sizeof(revision_path) - 1, "%s\\revision_%d", arena_gametypes_backup_path, (int)revision);
	revision_path[sizeof(revision_path) - 1] = 0;
	if (!arena_gametypes_directory(arena_gametypes_backup_path) || !arena_gametypes_directory(revision_path))
	{
		error(_error_silent, "arena gametypes: no backup directory '%s'", revision_path);
		return FALSE;
	}
	if (!*record_backed_up)
	{
		struct file_reference record;

		if (file_reference_create_from_path(&record, arena_gametypes_record_path, FALSE) && file_exists(&record) &&
			!arena_gametypes_backup_file(&record, revision_path, FALSE))
		{
			return FALSE;
		}
		*record_backed_up = TRUE;
	}

	/* (its directory: u:\UDATA\<name>\, the last part of the path) */
	if (!saved_game_file_get_path_to_enclosing_directory(profile_index, directory_path))
		return FALSE;
	while (directory_path[0] && directory_path[strlen(directory_path) - 1] == '\\')
		directory_path[strlen(directory_path) - 1] = 0;
	directory_name = strrchr(directory_path, '\\');
	directory_name = directory_name ? directory_name + 1 : directory_path;
	if (!directory_name[0])
		return FALSE;
	_snprintf(gametype_path, sizeof(gametype_path) - 1, "%s\\%s", revision_path, directory_name);
	gametype_path[sizeof(gametype_path) - 1] = 0;
	if (!arena_gametypes_directory(gametype_path) ||
		!file_reference_create_from_path(&directory, directory_path, TRUE))
	{
		return FALSE;
	}
	file_count = find_files(0, &directory, NUMBEROF(files), files);
	/* (none, or as many as the list holds: perhaps more not backed up) */
	if (file_count <= 0 || file_count >= NUMBEROF(files))
	{
		error(_error_silent, "arena gametypes: '%s' holds %ld files: not backed up", directory_path, file_count);
		return FALSE;
	}
	for (index = 0; index < file_count; index++)
	{
		if (!arena_gametypes_backup_file(&files[index], gametype_path, TRUE))
			return FALSE;
	}
	return TRUE;
}

char const *arena_gametypes_migrated_name(
	char const *name)
{
	short pass;

	/* (each rename followed, the table's order, a few hops at most) */
	for (pass = 0; pass < (short)NUMBEROF(arena_gametype_migrations); pass++)
	{
		short index;
		boolean renamed = FALSE;

		for (index = 0; index < (short)NUMBEROF(arena_gametype_migrations) && !renamed; index++)
		{
			char const *old_name = arena_gametype_migrations[index].old_row.name;
			char const *new_name = arena_gametype_migrations[index].new_row.name;
			short character;

			if (!strcmp(old_name, new_name))
				continue;
			for (character = 0; old_name[character] && name[character] &&
				toupper((unsigned char)old_name[character]) == toupper((unsigned char)name[character]); character++)
				;
			if (!old_name[character] && !name[character])
			{
				name = new_name;
				renamed = TRUE;
			}
		}
		if (!renamed)
			break;
	}
	return name;
}

/* the migrations: the table's, and debug.arena_test_migration's after them */
static short arena_gametypes_migration_count(
	void)
{
	return (short)(NUMBEROF(arena_gametype_migrations) +
		(config_integer("debug.arena_test_migration") ? 1 : 0));
}

static struct arena_gametype_migration const *arena_gametypes_migration(
	short index)
{
	return index < (short)NUMBEROF(arena_gametype_migrations) ? &arena_gametype_migrations[index] :
		&arena_gametype_test_migration;
}

/* the seeds' revision now: the last migration's */
static short arena_gametypes_revision(
	void)
{
	return arena_gametypes_migration(arena_gametypes_migration_count() - 1)->revision;
}

/* a row's mark that a start began rewriting it (after its backup, before
its rewrite): z:\saved\playlists_backup\revision_N\begun <old name>.txt.
mark: made; else whether it is there */
static boolean arena_gametypes_row_begun(
	short revision,
	char const *old_name,
	boolean mark)
{
	char path[MAXIMUM_FILENAME_LENGTH + 1];
	struct file_reference file;

	_snprintf(path, sizeof(path) - 1, "%s\\revision_%d\\begun %s.txt", arena_gametypes_backup_path, (int)revision,
		old_name);
	path[sizeof(path) - 1] = 0;
	if (!file_reference_create_from_path(&file, path, FALSE))
		return FALSE;
	if (file_exists(&file))
		return TRUE;
	return mark && file_create(&file);
}

/* a name's line taken out of the record (a seeded gametype to write again) */
static void arena_gametypes_record_remove(
	char *record,
	unsigned long *record_length,
	char const *name)
{
	char line[ARENA_GAMETYPE_NAME_LENGTH + 2];
	char *found;
	unsigned long line_length;

	_snprintf(line, sizeof(line), "\n%s\n", name);
	line[sizeof(line) - 1] = 0;
	found = strstr(record, line);
	if (!found)
		return;
	/* (its name and newline; the newline before it stays) */
	line_length = (unsigned long)strlen(line) - 1;
	csmemmove(found + 1, found + 1 + line_length, *record_length - (unsigned long)(found + 1 + line_length - record) + 1);
	*record_length -= line_length;
}

/* a value update's file written beside it (blam.new) and put in its place
in one step: any failure leaves the old file whole (the saved game's name
and folder are the same, so nothing else changes). debug.arena_test_migration
2 fails it on purpose, for the tests */
static boolean arena_gametype_update_in_place(
	long profile_index,
	struct game_variant const *variant,
	struct game_variant_options const *options)
{
	char directory[MAXIMUM_FILENAME_LENGTH + 1];
	char path[MAXIMUM_FILENAME_LENGTH + 1];
	char new_path[MAXIMUM_FILENAME_LENGTH + 1];
	unsigned char block[SAVED_GAME_FILE_BLOCK_SIZE];
	struct file_reference file;
	boolean written = FALSE;
	boolean success = FALSE;

	if (!saved_game_file_get_path_to_enclosing_directory(profile_index, directory))
		return FALSE;
	while (directory[0] && directory[strlen(directory) - 1] == '\\')
		directory[strlen(directory) - 1] = 0;
	_snprintf(path, sizeof(path) - 1, "%s\\blam.lst", directory);
	path[sizeof(path) - 1] = 0;
	_snprintf(new_path, sizeof(new_path) - 1, "%s\\blam.new", directory);
	new_path[sizeof(new_path) - 1] = 0;
	playlist_profile_expected_block(variant, options, block);
	if (!saved_game_files_take_mutex())
		return FALSE;
	/* (a display name block the file has stays: no write drops it) */
	if (file_reference_create_from_path(&file, path, FALSE) && file_open(&file, FLAG(_permission_read_bit)))
	{
		unsigned char old_block[SAVED_GAME_FILE_BLOCK_SIZE];

		if (file_read(&file, sizeof(old_block), old_block))
			playlist_display_name_carry(block, old_block);
		file_close(&file);
	}
	if (file_reference_create_from_path(&file, new_path, FALSE) &&
		(file_exists(&file) || file_create(&file)) &&
		file_open(&file, FLAG(_permission_write_bit)))
	{
		written = file_set_eof(&file, 0) && file_write(&file, sizeof(block), block);
		file_close(&file);
	}
	if (written && config_integer("debug.arena_test_migration") == 2)
	{
		error(_error_silent, "arena gametypes: debug.arena_test_migration 2: the update's write fails");
		written = FALSE;
	}
	success = written && platform_replace_file(new_path, path);
	if (success)
		playlist_profile_content_changed();
	if (!success && file_reference_create_from_path(&file, new_path, FALSE) && file_exists(&file))
		file_delete(&file);
	saved_game_files_release_mutex();
	return success;
}

/* a value update's blam.new left in a saved gametype's folder (a start that
ended between writing it and putting it in place) taken away, only while
the gametype's own blam.lst is there and whole (its block, its signature
right); else left, for whoever recovers it */
static void arena_gametypes_remove_stray_updates(
	void)
{
	long saved[ARENA_GAMETYPES_MAXIMUM_SAVED];
	word saved_count = NUMBEROF(saved);
	short index;

	saved_game_files_enumerate_available_to_local_player_index(NONE,
		_saved_game_file_type_game_variant, &saved_count, saved, FALSE);
	for (index = 0; index < (short)saved_count; index++)
	{
		char directory[MAXIMUM_FILENAME_LENGTH + 1];
		char path[MAXIMUM_FILENAME_LENGTH + 1];
		struct file_reference stray;
		struct file_reference file;
		unsigned long size = 0;
		unsigned char *data;
		boolean whole = FALSE;

		if (!saved_game_file_get_path_to_enclosing_directory(saved[index], directory))
			continue;
		while (directory[0] && directory[strlen(directory) - 1] == '\\')
			directory[strlen(directory) - 1] = 0;
		_snprintf(path, sizeof(path) - 1, "%s\\blam.new", directory);
		path[sizeof(path) - 1] = 0;
		if (!file_reference_create_from_path(&stray, path, FALSE) || !file_exists(&stray))
			continue;
		_snprintf(path, sizeof(path) - 1, "%s\\blam.lst", directory);
		path[sizeof(path) - 1] = 0;
		if (file_reference_create_from_path(&file, path, FALSE) && file_exists(&file) &&
			(data = file_read_into_memory(&file, &size)) != NULL)
		{
			XCALCSIG_SIGNATURE signature;

			if (size >= SAVED_GAME_FILE_BLOCK_SIZE)
			{
				saved_game_file_generate_checksum(data, sizeof(struct game_variant), &signature);
				whole = !csmemcmp(&signature, data + sizeof(struct game_variant), sizeof(signature));
			}
			free(data);
		}
		if (whole)
		{
			file_delete(&stray);
			error(_error_silent, "arena gametypes: a stray update '%s\\blam.new' taken away", directory);
		}
		else
		{
			error(_error_silent, "arena gametypes: '%s\\blam.new' left: its blam.lst is not whole", directory);
		}
	}
}

/* a migration put off to the next start: its revision unrecorded, and its
new name not seeded beside the old one meanwhile */
static void arena_gametypes_defer(
	struct arena_gametype_migration const *migration,
	short *deferred_revision,
	char *deferred_names)
{
	unsigned long length = (unsigned long)strlen(deferred_names);

	if (*deferred_revision == NONE || migration->revision < *deferred_revision)
		*deferred_revision = migration->revision;
	if (length + strlen(migration->new_row.name) + 2 < ARENA_GAMETYPES_RECORD_SIZE)
	{
		csstrcat(deferred_names, migration->new_row.name);
		csstrcat(deferred_names, "\n");
	}
}

/* the seeds' migrations of each revision above the record's, in order (a
migration acts only while its old name is in the record and its new name
not): the gametype of the old name exactly as the revision before seeded it
is backed up and rewritten in place under its new name from the new row
(the new name recorded, so not seeded again); the player's changed one is
kept, the new one then seeded beside it; one the player deleted (or
renamed) stays so, its new name recorded unseeded */
static void arena_gametypes_migrate(
	char *record,
	unsigned long *record_length,
	short record_revision,
	boolean *record_changed,
	boolean *written,
	short *deferred_revision,
	char *deferred_names)
{
	long saved[ARENA_GAMETYPES_MAXIMUM_SAVED];
	word saved_count = 0;
	boolean listed = FALSE;
	boolean record_backed_up = FALSE;
	short backed_up_revision = NONE;
	short migration_index;

	for (migration_index = 0; migration_index < arena_gametypes_migration_count(); migration_index++)
	{
		struct arena_gametype_migration const *migration = arena_gametypes_migration(migration_index);
		char const *old_name = migration->old_row.name;
		char const *new_name = migration->new_row.name;
		/* (a value update in place, under the same name, else a rename) */
		boolean same_name = !strcmp(old_name, new_name);
		char old_line[ARENA_GAMETYPE_NAME_LENGTH + 2];
		char new_line[ARENA_GAMETYPE_NAME_LENGTH + 2];
		wchar_t old_wide[ARENA_GAMETYPE_NAME_LENGTH];
		wchar_t new_wide[ARENA_GAMETYPE_NAME_LENGTH];
		long profile_index;
		struct game_variant variant;
		struct game_variant_options options;

		if (migration->revision <= record_revision)
			continue;
		_snprintf(old_line, sizeof(old_line), "\n%s\n", old_name);
		old_line[sizeof(old_line) - 1] = 0;
		_snprintf(new_line, sizeof(new_line), "\n%s\n", new_name);
		new_line[sizeof(new_line) - 1] = 0;
		if (!strstr(record, old_line) || (!same_name && strstr(record, new_line)))
			continue;
		/* (its old row does not build as the builds before seeded it: no
		file could match it rightly, so none is touched; put off, for a
		build whose row does) */
		if (arena_gametype_migration_unsafe[migration_index])
		{
			error(_error_silent, "arena gametype '%s' not migrated (revision %d): its frozen row no longer builds "
				"as seeded (self-check); put off to the next start", old_name, (int)migration->revision);
			arena_gametypes_defer(migration, deferred_revision, deferred_names);
			continue;
		}

		/* (the custom gametypes, listed once) */
		if (!listed)
		{
			saved_count = NUMBEROF(saved);
			saved_game_files_enumerate_available_to_local_player_index(NONE,
				_saved_game_file_type_game_variant, &saved_count, saved, FALSE);
			listed = TRUE;
		}
		arena_gametype_wide_name(old_name, old_wide);
		arena_gametype_wide_name(new_name, new_wide);
		profile_index = arena_gametype_find_saved(old_wide, saved, saved_count);

		if (profile_index == NONE)
		{
			if (!saved_game_file_name_unique(old_wide))
			{
				/* (there, but past the list's limit: put off, its revision
				unrecorded, so it runs again next start) */
				error(_error_silent, "arena gametype '%s' not migrated (revision %d): not in the saved games' list; "
					"put off to the next start", old_name, (int)migration->revision);
				arena_gametypes_defer(migration, deferred_revision, deferred_names);
				continue;
			}
			if (same_name)
			{
				/* (deleted by the player: stays so; nothing to record) */
				error(_error_silent, "arena gametype '%s' not updated (revision %d): the player removed it", old_name,
					(int)migration->revision);
				continue;
			}
			if (!saved_game_file_name_unique(new_wide))
			{
				/* (renamed already, by a start that ended before its record) */
				error(_error_silent, "arena gametype '%s' is there already (revision %d)", new_name, (int)migration->revision);
			}
			else if (arena_gametypes_row_begun(migration->revision, old_name, FALSE))
			{
				/* (a start began rewriting this one and did not finish:
				perhaps it was lost then; the seeding writes the new one,
				not a deletion recorded) */
				error(_error_silent, "arena gametype '%s' missing after an earlier start's migration (revision %d); "
					"'%s' seeded", old_name, (int)migration->revision, new_name);
				continue;
			}
			else
			{
				/* (deleted or renamed by the player: not brought back) */
				error(_error_silent, "arena gametype '%s' (once '%s') not seeded: the player removed '%s'",
					new_name, old_name, old_name);
			}
			if (arena_gametypes_record_add(record, record_length, new_name))
				*record_changed = TRUE;
			continue;
		}

		arena_gametype_build(&migration->old_row, old_wide, &variant, &options);
		if (!playlist_profile_matches(profile_index, &variant, &options))
		{
			if (same_name)
			{
				struct game_variant new_variant;
				struct game_variant_options new_options;

				/* (updated already, by a start that ended before its record) */
				arena_gametype_build(&migration->new_row, new_wide, &new_variant, &new_options);
				if (playlist_profile_matches(profile_index, &new_variant, &new_options))
					continue;
				error(_error_silent, "arena gametype '%s' kept as it is (changed since seeded; revision %d)", old_name,
					(int)migration->revision);
				continue;
			}
			/* (the player's own now: kept, and the new one seeded beside it) */
			error(_error_silent, "arena gametype '%s' kept as it is (changed since seeded); '%s' seeded beside it",
				old_name, new_name);
			continue;
		}
		if (!same_name && !saved_game_file_name_unique(new_wide))
		{
			/* (a saved game has the new name: the seeding leaves both) */
			error(_error_silent, "arena gametype '%s' not migrated: a saved game named '%s' exists", old_name, new_name);
			continue;
		}
		if (backed_up_revision != migration->revision)
		{
			record_backed_up = FALSE;
			backed_up_revision = migration->revision;
		}
		if (!arena_gametypes_backup(migration->revision, profile_index, &record_backed_up))
		{
			/* (no backup, no rewrite: kept, the new one seeded beside it) */
			error(_error_silent, "arena gametype '%s' kept as it is: its backup failed; '%s' seeded beside it",
				old_name, new_name);
			continue;
		}
		/* (a rename's mark: a later start that finds the old one missing
		knows this one was begun, not deleted by the player) */
		if (!same_name)
			arena_gametypes_row_begun(migration->revision, old_name, TRUE);

		if (same_name)
		{
			/* (a value update: written beside the file and put in its place,
			so a failure leaves it whole; put off then, to try again) */
			arena_gametype_build(&migration->new_row, new_wide, &variant, &options);
			if (!arena_gametype_update_in_place(profile_index, &variant, &options))
			{
				if (saved_game_file_name_unique(old_wide))
				{
					/* (gone after all: seeded again, its name out of the record) */
					error(_error_silent, "failed to update arena gametype '%s' (revision %d), and it is gone: seeded "
						"again", old_name, (int)migration->revision);
					arena_gametypes_record_remove(record, record_length, old_name);
					*record_changed = TRUE;
				}
				else
				{
					error(_error_silent, "failed to update arena gametype '%s' (revision %d): kept as it is; put off "
						"to the next start", old_name, (int)migration->revision);
					arena_gametypes_defer(migration, deferred_revision, deferred_names);
				}
				continue;
			}
			*written = TRUE;
			error(_error_silent, "updated arena gametype '%s' (revision %d)", old_name, (int)migration->revision);
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

			/* (rewritten as the PC menus' save does, in its own slot: the
			new row under its new name, which moves its saved game's name
			with it) */
			arena_gametype_build(&migration->new_row, new_wide, &variant, &options);
			playlist_profile_save_with_options(profile_index, &variant, &options);
			playlist_profile_wait_for_write();
			/* (the files changed either way: listed again) */
			*written = TRUE;
			listed = FALSE;

			/* (a write that failed deletes the gametype, playlist_profile.c's
			write thread, or leaves it under its old name: the new name is
			recorded only once a saved game has it, else the seeding below
			writes a new one) */
			if (saved_game_file_name_unique(new_wide))
			{
				error(_error_silent, "failed to migrate arena gametype '%s' to '%s'; '%s' seeded instead",
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
		error(_error_silent, "migrated arena gametype '%s' to '%s' (revision %d)", old_name, new_name,
			(int)migration->revision);
		if (arena_gametypes_record_add(record, record_length, new_name))
			*record_changed = TRUE;
	}

	return;
}

/* the start's self-check: each name's row now is its last migration's new
row (else a migration rewrites to one thing and the seeding another), and
the saved games are not near the memory unit's limit */
static void arena_gametypes_self_check(
	void)
{
	long saved[ARENA_GAMETYPES_MAXIMUM_SAVED];
	word gametype_count = NUMBEROF(saved);
	word profile_count = NUMBEROF(saved);
	short migration_index;

	for (migration_index = 0; migration_index < arena_gametypes_migration_count(); migration_index++)
	{
		struct arena_gametype_migration const *migration = arena_gametypes_migration(migration_index);
		struct arena_gametype const *new_row = &migration->new_row;
		struct arena_gametype const *row = NULL;
		wchar_t wide_name[ARENA_GAMETYPE_NAME_LENGTH];
		struct game_variant variant[2];
		struct game_variant_options options[2];
		short index;

		arena_gametype_migration_unsafe[migration_index] = FALSE;
		/* (the old row as the builds before seeded it: its block's hash) */
		if (migration->old_block_hash)
		{
			unsigned char block[SAVED_GAME_FILE_BLOCK_SIZE];
			XCALCSIG_SIGNATURE signature;
			char hash[2 * sizeof(signature.Signature) + 1];

			arena_gametype_wide_name(migration->old_row.name, wide_name);
			csmemset(variant, 0, sizeof(variant));
			csmemset(options, 0, sizeof(options));
			arena_gametype_build(&migration->old_row, wide_name, &variant[0], &options[0]);
			playlist_profile_expected_block(&variant[0], &options[0], block);
			saved_game_file_generate_checksum(block, sizeof(block), &signature);
			for (index = 0; index < (short)sizeof(signature.Signature); index++)
				_snprintf(hash + 2 * index, 3, "%02x", (unsigned int)signature.Signature[index]);
			hash[sizeof(hash) - 1] = 0;
			if (strcmp(hash, migration->old_block_hash))
			{
				arena_gametype_migration_unsafe[migration_index] = TRUE;
				error(_error_silent, "arena gametypes self-check: ERROR: revision %d's old row '%s' builds as %s, "
					"not as seeded (%s): its migration is skipped", (int)migration->revision, migration->old_row.name,
					hash, migration->old_block_hash);
			}
		}

		/* (only a name's last migration, and not the test's) */
		if (migration == &arena_gametype_test_migration)
			continue;
		for (index = migration_index + 1; index < (short)NUMBEROF(arena_gametype_migrations); index++)
		{
			/* (a later one moves it on: updates it, or renames it) */
			if (!strcmp(arena_gametype_migrations[index].new_row.name, new_row->name) ||
				!strcmp(arena_gametype_migrations[index].old_row.name, new_row->name))
				break;
		}
		if (index < (short)NUMBEROF(arena_gametype_migrations))
			continue;
		for (index = 0; index < NUMBEROF(arena_gametypes) && !row; index++)
		{
			if (!strcmp(arena_gametypes[index].name, new_row->name))
				row = &arena_gametypes[index];
		}
		if (!row)
		{
			struct arena_gametype_info info;

			/* (a seed folded into another, no longer seeded: an old name of
			the names table's, NHE TRAIN) */
			arena_gametype_wide_name(new_row->name, wide_name);
			if (arena_gametype_info(wide_name, &info) && !info.description)
				continue;
			error(_error_silent, "arena gametypes self-check: '%s' (revision %d's) is not seeded",
				new_row->name, (int)migration->revision);
			continue;
		}
		arena_gametype_wide_name(new_row->name, wide_name);
		csmemset(variant, 0, sizeof(variant));
		csmemset(options, 0, sizeof(options));
		arena_gametype_build(new_row, wide_name, &variant[0], &options[0]);
		arena_gametype_build(row, wide_name, &variant[1], &options[1]);
		if (csmemcmp(&variant[0], &variant[1], sizeof(variant[0])) || csmemcmp(&options[0], &options[1], sizeof(options[0])))
		{
			error(_error_silent, "arena gametypes self-check: '%s' is seeded otherwise than revision %d's migration "
				"rewrites it", new_row->name, (int)migration->revision);
		}
	}

	saved_game_files_enumerate_available_to_local_player_index(NONE,
		_saved_game_file_type_game_variant, &gametype_count, saved, FALSE);
	saved_game_files_enumerate_available_to_local_player_index(NONE,
		_saved_game_file_type_player_profile, &profile_count, saved, FALSE);
	if (gametype_count + profile_count > ARENA_GAMETYPES_SAVED_WARNING)
	{
		error(_error_silent, "arena gametypes: warning: %d saved games (%d gametypes, %d profiles); a memory unit "
			"holds at most 100", (int)(gametype_count + profile_count), (int)gametype_count, (int)profile_count);
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
	/* (the fields whose 0 is the stock gametype's: ARENA_SET) */
	if (gametype->weapon_set)
		variant.universal_variant.weapon_set = gametype->weapon_set - 1;
	if (gametype->goal_radar)
		variant.universal_variant.goal_radar = gametype->goal_radar - 1;
	if (gametype->nhe_mode)
		variant.universal_variant.nhe_mode = gametype->nhe_mode;
	if (gametype->starting_frags)
		variant.universal_variant.starting_frags = gametype->starting_frags;
	/* (the PC options a new custom gametype gets: both teams' vehicle sets
	the variant's, RADAR PLAYERS from its motion sensor bit) */
	game_variant_options_default(&variant, &options);
	options.time_limit = gametype->time_limit;
	if (gametype->custom_loadout)
	{
		options.loadout = _loadout_custom;
		options.primary_weapon = gametype->loadout_primary ? (byte)(gametype->loadout_primary - 1) :
			_loadout_weapon_pistol;
		options.secondary_weapon = gametype->loadout_secondary ? (byte)(gametype->loadout_secondary - 1) :
			_loadout_weapon_assault_rifle;
	}
	if (gametype->no_map_weapons)
		options.no_map_weapons = TRUE;
	if (gametype->friendly_fire)
		options.friendly_fire = (short)(gametype->friendly_fire - 1);
	if (gametype->adjust)
		gametype->adjust(&variant, &options);
	*variant_out = variant;
	*options_out = options;

	return;
}

/* Halo 1: NHE's slayers (revision 4): no death bonus, no kill penalty (the
stock gametypes' speed changes on a death or a kill), not in order.
Shipped: never edited (a change is a new function) */
static void arena_gametype_nhe_slayer(
	struct game_variant *variant,
	struct game_variant_options *options)
{
	(void)options;
	variant->game_engine_variant.slayer.no_death_bonus = TRUE;
	variant->game_engine_variant.slayer.no_kill_penalty = TRUE;
	variant->game_engine_variant.slayer.kill_in_order = FALSE;

	return;
}

/* Halo 1: NHE's CTF (revision 4): your flag at home to score; no assault,
reset on capture, flag must reset or single flag. Shipped: never edited */
static void arena_gametype_nhe_ctf(
	struct game_variant *variant,
	struct game_variant_options *options)
{
	(void)options;
	variant->game_engine_variant.ctf.flag_at_home_to_score = TRUE;
	variant->game_engine_variant.ctf.assault = FALSE;
	variant->game_engine_variant.ctf.reset_on_capture = FALSE;
	variant->game_engine_variant.ctf.flag_must_reset = FALSE;
	variant->game_engine_variant.ctf.single_flag_time = 0;

	return;
}

/* a seeded gametype's rules, in the log */
static void arena_gametype_log(
	struct arena_gametype const *gametype,
	struct game_variant const *variant,
	struct game_variant_options const *options)
{
	static char const *const engines[] = { "none", "ctf", "slayer", "oddball", "king", "race", "terminator", "stub" };
	/* (game_engine.h's _loadout_weapon_*) */
	static char const *const weapons[] =
	{
		"none", "random", "assault rifle", "pistol", "shotgun", "sniper rifle", "rocket launcher",
		"plasma pistol", "plasma rifle", "needler"
	};
	char loadout[48];
	struct universal_variant const *universal = &variant->universal_variant;
	unsigned long flags = universal->flags;
	long engine = variant->game_engine_index;

	if (options->loadout == _loadout_custom)
	{
		_snprintf(loadout, sizeof(loadout) - 1, "%s + %s",
			options->primary_weapon < NUMBEROF(weapons) ? weapons[options->primary_weapon] : "?",
			options->secondary_weapon < NUMBEROF(weapons) ? weapons[options->secondary_weapon] : "?");
		loadout[sizeof(loadout) - 1] = 0;
	}
	else
		csstrcpy(loadout, "the weapon set's");
	error(_error_silent, "seeded arena gametype '%s': %s%s, score to win %ld, time limit %d min, "
		"respawn %g s, suicide penalty %g s, motion sensor %s, vehicle set %ld, loadout %s, "
		"equipment %s, health %s, fall damage %s, no spread %s, pre-game countdown %s, timers %s, "
		"training %s, practice %s, nhe mode %s",
		gametype->name,
		engine >= 0 && engine < (long)NUMBEROF(engines) ? engines[engine] : "?",
		universal->teams ? " (teams)" : "",
		universal->score_to_win,
		(int)options->time_limit,
		(double)universal->respawn_time / TICKS_PER_SECOND,
		(double)universal->suicide_penalty / TICKS_PER_SECOND,
		TEST_FLAG(flags, _game_variant_draw_object_in_motion_sensor_bit) ? "on" : "off",
		universal->vehicle_set,
		loadout,
		TEST_FLAG(flags, _game_variant_generic_starting_equipment_bit) ? "generic" : "the map's",
		game_variant_health_style_name(flags),
		TEST_FLAG(flags, _game_variant_no_falling_damage_bit) ? "off" : "on",
		game_variant_no_spread_name(flags),
		TEST_FLAG(flags, _game_variant_pregame_countdown_bit) ? "on" : "off",
		game_variant_timers_name(flags),
		TEST_FLAG(flags, _game_variant_training_bit) ? "on" : "off",
		TEST_FLAG(flags, _game_variant_practice_bit) ? "on" : "off",
		game_variant_nhe_mode_name(universal->nhe_mode));

	return;
}
