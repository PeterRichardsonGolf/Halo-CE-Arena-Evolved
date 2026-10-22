/*
AE_SETUP_OPTIONS.H

The game setup spike's GAME OPTIONS model (ae_setup_options.c; notes/plans/2026-10-21-game-setup-spike.md): every
gametype rule as a row in Halo 2's variant editor groups (MATCH, PLAYERS, TEAMS, EQUIPMENT, VEHICLES, the mode's own,
then AE's ARENA, SOUND and NHE), its label, value texts and helps, which rows a gametype shows, and the one list the
screen scrolls (group headers in it), its groups' counts and changes and the LB / RB jumps between them. Pure C, no
engine includes (unit test: port/linux/tests/ae_setup_options_test.c).

The model deals in value INDEXES (a row's texts, in order); the glue (ae_glue_setup_options.c) maps an index to the
gametype's fields, in the same order, and checks the counts match. Ideas from the shelved M4's ae_game_options.c.
*/

#ifndef __AE_SETUP_OPTIONS_H
#define __AE_SETUP_OPTIONS_H

enum ae_setup_group { AE_SG_MATCH, AE_SG_PLAYERS, AE_SG_TEAMS, AE_SG_EQUIPMENT, AE_SG_VEHICLES, AE_SG_MODE,
	AE_SG_ARENA, AE_SG_SOUND, AE_SG_NHE, AE_SETUP_GROUPS };

/* the vehicle kinds (game_engine.h's NUMBER_OF_VARIANT_VEHICLES: the glue checks it) */
enum { AE_SETUP_VEHICLE_KINDS = 6 };

enum ae_setup_option
{
	/* MATCH (the score to win is the mode's: one row per engine) */
	AE_SO_KILLS_TO_WIN, AE_SO_CAPTURES_TO_WIN, AE_SO_HILL_SCORE, AE_SO_BALL_SCORE, AE_SO_LAPS_TO_WIN,
	AE_SO_TIME_LIMIT, AE_SO_COUNTDOWN, AE_SO_NAME,
	/* PLAYERS */
	AE_SO_LIVES, AE_SO_RESPAWN_TIME, AE_SO_RESPAWN_GROWTH, AE_SO_SUICIDE_PENALTY, AE_SO_SHIELDS,
	AE_SO_MAXIMUM_HEALTH, AE_SO_ACTIVE_CAMO, AE_SO_MOTION_SENSOR, AE_SO_SENSOR_RANGE, AE_SO_ODD_MAN_OUT,
	AE_SO_CONTROLLER_CROUCH, AE_SO_MELEE_UNZOOMS, AE_SO_MOUSE_AIM_ASSIST,
	/* TEAMS */
	AE_SO_TEAM_PLAY, AE_SO_FRIENDLY_FIRE, AE_SO_BETRAYAL_PENALTY, AE_SO_FORCE_EVEN_TEAMS, AE_SO_FRIENDS_ON_SCREEN,
	/* EQUIPMENT */
	AE_SO_WEAPON_SET, AE_SO_MAP_WEAPONS, AE_SO_GRENADES_ON_MAP,
	AE_SO_PACK_BATTLE_RIFLE, AE_SO_PACK_SMG, AE_SO_PACK_SMG_SILENCED, AE_SO_PACK_CARBINE, AE_SO_PACK_AR_SILENCED,
	AE_SO_PACKS_ON_MAP, AE_SO_STARTING_EQUIPMENT, AE_SO_STARTING_GRENADES, AE_SO_LOADOUT,
	AE_SO_PRIMARY, AE_SO_SECONDARY, AE_SO_INFINITE_GRENADES, AE_SO_INFINITE_AMMO, AE_SO_BOTTOMLESS_CLIP,
	/* VEHICLES (a side's set, then its counts; blue's in team games only) */
	AE_SO_VEHICLE_RESPAWN, AE_SO_VEHICLE_SET_RED,
	AE_SO_VEHICLE_COUNT_RED,                                   /* + kind */
	AE_SO_VEHICLE_SET_BLUE = AE_SO_VEHICLE_COUNT_RED + AE_SETUP_VEHICLE_KINDS,
	AE_SO_VEHICLE_COUNT_BLUE,                                  /* + kind */
	/* the mode's own */
	AE_SO_DEATH_BONUS = AE_SO_VEHICLE_COUNT_BLUE + AE_SETUP_VEHICLE_KINDS, AE_SO_KILL_PENALTY, AE_SO_KILL_IN_ORDER,
	AE_SO_ASSAULT, AE_SO_SINGLE_FLAG, AE_SO_FLAG_MUST_RESET, AE_SO_FLAG_AT_HOME,
	AE_SO_MOVING_HILL,
	AE_SO_TRAIT_WITH_BALL, AE_SO_TRAIT_WITHOUT_BALL, AE_SO_SPEED_WITH_BALL, AE_SO_BALL_TYPE, AE_SO_RANDOM_START,
	AE_SO_BALL_COUNT, AE_SO_BALL_MELEE,
	AE_SO_RACE_TYPE, AE_SO_TEAM_SCORING,
	AE_SO_OBJECTIVES,
	/* ARENA */
	AE_SO_HEALTH, AE_SO_FALL_DAMAGE, AE_SO_NO_SPREAD, AE_SO_COMBAT_FEEDBACK, AE_SO_DROP_SECONDARY, AE_SO_PRACTICE,
	AE_SO_TIMERS, AE_SO_TRAINING, AE_SO_SPAWN_HEAT,
	/* SOUND */
	AE_SO_SOUND_PRESET, AE_SO_SILENT_MOVEMENT, AE_SO_SILENT_WEAPONS,
	/* NHE */
	AE_SO_NHE_MODE, AE_SO_NHE_EXTRAS,
	AE_SETUP_OPTIONS
};

/* the weapon packs' rows are AE_SO_PACK_FIRST + (slot - 1): slots as ae_weapon_slots.h numbers them */
#define AE_SO_PACK_FIRST AE_SO_PACK_BATTLE_RIFLE
enum { AE_SETUP_PACKS = 5 };
/* PRIMARY / SECONDARY: values 0-9 are the game's weapons, then one per pack slot (AE_SETUP_WEAPON_PACK_FIRST + slot - 1) */
enum { AE_SETUP_WEAPON_STOCK = 10, AE_SETUP_WEAPON_PACK_FIRST = 10 };

/* the engines as game_engine.h numbers them (the glue checks): ctf 1, slayer 2, oddball 3, king 4, race 5 */
enum { AE_SETUP_ENGINE_CTF = 1, AE_SETUP_ENGINE_SLAYER = 2, AE_SETUP_ENGINE_ODDBALL = 3, AE_SETUP_ENGINE_KING = 4,
	AE_SETUP_ENGINE_RACE = 5 };
/* value indexes the model reads to decide what shows */
enum { AE_SETUP_TEAM_PLAY_YES = 0, AE_SETUP_LOADOUT_CUSTOM = 1, AE_SETUP_VEHICLE_SET_CUSTOM = 9,
	AE_SETUP_SOUND_SPLIT_SCREEN = 1 };

/* the gametype being edited, as the glue gives it (tests: a fake) */
struct ae_setup_access
{
	int (*get)(void *context, int option);       /* the value index now; -1: not in this build */
	int (*changed)(void *context, int option);   /* differs from the gametype the edit started from */
	void *context;
	int engine;                                  /* game_engine.h's number (fixed for an edit) */
	int ae_rules;                                /* arena_gametype_has_ae_rules (WEAPON SET with a custom loadout) */
};

struct ae_setup_row
{
	const char *label;
	short group;
	short engines;                     /* a mask of 1 << engine; 0: every engine */
	short team_only;
	short ae;                          /* one of AE's own rules (the AE badge) */
	const char *const *values;
	short value_count;
	const char *const *helps;          /* one per value, or one for every value (help_count 1) */
	short help_count;
};

/* NULL for a number out of range */
struct ae_setup_row const *ae_setup_row(int option);
/* NAME's value text: the edit copy's name (the model keeps a copy; the screen sets it each frame) */
void ae_setup_name_set(const char *name);
/* "" out of range */
const char *ae_setup_value_text(int option, int value_index);
const char *ae_setup_help(int option, int value_index);
/* the label as shown (in a free-for-all game red's vehicle set is the only one: VEHICLE SET) */
const char *ae_setup_label(struct ae_setup_access const *access, int option);
/* the group's name; the mode's is the engine's (SLAYER, CAPTURE THE FLAG, ...) */
const char *ae_setup_group_name(struct ae_setup_access const *access, int group);

/* whether the gametype has the option at all (its engine), and whether the screen shows it now */
int ae_setup_applies(struct ae_setup_access const *access, int option);
int ae_setup_visible(struct ae_setup_access const *access, int option);
int ae_setup_team(struct ae_setup_access const *access);

/* the screen's list: the visible rows in order, each group's under its header (an entry < 0 is a header:
-1 - group); a group with no visible row has no header. Returns the entries written (at most maximum) */
short ae_setup_entries(struct ae_setup_access const *access, short *entries, short maximum);
static inline int ae_setup_entry_is_header(short entry) { return entry < 0; }
static inline int ae_setup_entry_group(short entry) { return -1 - entry; }

/* the rail: per group, its header's entry (-1 none shown), its visible rows and its changed rows (shown or not) */
struct ae_setup_group_info { short header, rows, changed; };
void ae_setup_groups(struct ae_setup_access const *access, short const *entries, short count,
	struct ae_setup_group_info *groups);
/* the group an entry is in (a header's own), -1 for none */
short ae_setup_group_at(short const *entries, short count, short index);
/* LB / RB: the first row of the next (direction > 0) or previous shown group from the one index is in, wrapping;
index when there is only one group */
short ae_setup_group_step(short const *entries, short count, short index, short direction);
/* the first row of a group (the rail's click), -1 when it shows none */
short ae_setup_group_first_row(short const *entries, short count, int group);
/* a row next to index past headers (direction -1 / 1), or index at the list's end */
short ae_setup_row_step(short const *entries, short count, short index, short direction);
/* N CHANGED: the options the gametype has (shown now or not) that differ from where the edit started */
short ae_setup_changes(struct ae_setup_access const *access);

#endif
