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
#include "saved games/playlist_profile.h"
#include "saved games/saved_game_files.h"
#include "tag_files/files.h"
#include "text/unicode.h"

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
	"#revision N" line */
	ARENA_GAMETYPES_RECORD_SIZE = 2048,
	/* the seeds' revision now (the last of arena_gametype_migrations) */
	ARENA_GAMETYPES_REVISION = 2,
	/* a memory unit holds at most 100 saved games (saved_game_files.c):
	the self-check warns past this many */
	ARENA_GAMETYPES_SAVED_WARNING = 90,
	/* a gametype directory's files backed up (blam.lst, SaveMeta.xbx) */
	ARENA_GAMETYPES_BACKUP_FILES = 8,
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

/* frozen copies of the flags the seeds of revisions 1 and 2 have, as
literals (the macros above may change; a migration compares a file with the
row its predecessor seeded): ARENA_GAMETYPE_FLAGS (no landing damage, HALO
2 health, the generic starting equipment, NO SPREAD FULL, the PRE-GAME
COUNTDOWN) and ARENA_CASUAL_FLAGS (TIMERS) */
#define ARENA_REV1_GAMETYPE_FLAGS 0x01790020UL
#define ARENA_REV1_CASUAL_FLAGS 0x00020000UL

/* the seed fields whose 0 means the stock gametype's (or AE's default):
a value is given as value + 1 */
#define ARENA_SET(value) ((value) + 1)

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
};

/* ---------- globals */

/* the AE gametypes: the stock gametype each is built from, and the
gametype options it adds (game_engine.h's universal_variant flags) */
static struct arena_gametype const arena_gametypes[] =
{
	/* AE's casual set: TIMERS, the motion sensor, 15 minutes, the stock
	respawns. The free for all ones say FFA (AE COMP FFA too); the rest
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
	/* AE COMP (AE PRO before revision 2), modern NHE: Beach LAN's 5 second
	respawn and suicide penalty,
	no motion sensor (but free for all's), no TIMERS; the slayers without a
	time limit, the objective games 15 minutes; team King and Oddball */
	{ "AE COMP FFA", build_game_variant_slayer, ARENA_GAMETYPE_FLAGS,
		ARENA_FFA_SCORE, 0, ARENA_COMPETITIVE_RESPAWN, ARENA_COMPETITIVE_RESPAWN, ARENA_RADAR_ON,
		ARENA_VEHICLES_STOCK, TRUE },
	{ "AE COMP TS", build_game_variant_team_slayer, ARENA_GAMETYPE_FLAGS,
		ARENA_TEAM_SLAYER_SCORE, 0, ARENA_COMPETITIVE_RESPAWN, ARENA_COMPETITIVE_RESPAWN, ARENA_RADAR_OFF,
		ARENA_VEHICLES_STOCK, TRUE },
	{ "AE COMP CTF", build_game_variant_ctf, ARENA_GAMETYPE_FLAGS,
		ARENA_CTF_SCORE, ARENA_TIME_LIMIT, ARENA_COMPETITIVE_RESPAWN, ARENA_COMPETITIVE_RESPAWN, ARENA_RADAR_OFF,
		ARENA_VEHICLES_STOCK, TRUE },
	{ "AE COMP KOH", build_game_variant_team_king, ARENA_GAMETYPE_FLAGS,
		ARENA_TIMED_SCORE, ARENA_TIME_LIMIT, ARENA_COMPETITIVE_RESPAWN, ARENA_COMPETITIVE_RESPAWN, ARENA_RADAR_OFF,
		ARENA_VEHICLES_STOCK, TRUE },
	{ "AE COMP OB", build_game_variant_team_oddball, ARENA_GAMETYPE_FLAGS,
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

/* the seeds' migrations, by revision (the record's "#revision N": each
above it runs once). Revision 1: the free for all ones whose names did not
say so; revision 2: AE PRO -> AE COMP (values unchanged) */
static struct arena_gametype_migration const arena_gametype_migrations[] =
{
	{ 1,
		{ "AE SLAYER", build_game_variant_slayer, ARENA_REV1_GAMETYPE_FLAGS | ARENA_REV1_CASUAL_FLAGS,
			25, 15, ARENA_STOCK, ARENA_STOCK, ARENA_RADAR_ON, ARENA_VEHICLES_STOCK, TRUE },
		{ "AE FFA SLAY", build_game_variant_slayer, ARENA_REV1_GAMETYPE_FLAGS | ARENA_REV1_CASUAL_FLAGS,
			25, 15, ARENA_STOCK, ARENA_STOCK, ARENA_RADAR_ON, ARENA_VEHICLES_STOCK, TRUE } },
	{ 1,
		{ "AE ODDBALL", build_game_variant_oddball, ARENA_REV1_GAMETYPE_FLAGS | ARENA_REV1_CASUAL_FLAGS,
			5, 15, ARENA_STOCK, ARENA_STOCK, ARENA_RADAR_ON, ARENA_VEHICLES_STOCK, TRUE },
		{ "AE FFA BALL", build_game_variant_oddball, ARENA_REV1_GAMETYPE_FLAGS | ARENA_REV1_CASUAL_FLAGS,
			5, 15, ARENA_STOCK, ARENA_STOCK, ARENA_RADAR_ON, ARENA_VEHICLES_STOCK, TRUE } },
	{ 2,
		{ "AE PRO FFA", build_game_variant_slayer, ARENA_REV1_GAMETYPE_FLAGS,
			25, 0, 150, 150, ARENA_RADAR_ON, ARENA_VEHICLES_STOCK, TRUE },
		{ "AE COMP FFA", build_game_variant_slayer, ARENA_REV1_GAMETYPE_FLAGS,
			25, 0, 150, 150, ARENA_RADAR_ON, ARENA_VEHICLES_STOCK, TRUE } },
	{ 2,
		{ "AE PRO TS", build_game_variant_team_slayer, ARENA_REV1_GAMETYPE_FLAGS,
			50, 0, 150, 150, ARENA_RADAR_OFF, ARENA_VEHICLES_STOCK, TRUE },
		{ "AE COMP TS", build_game_variant_team_slayer, ARENA_REV1_GAMETYPE_FLAGS,
			50, 0, 150, 150, ARENA_RADAR_OFF, ARENA_VEHICLES_STOCK, TRUE } },
	{ 2,
		{ "AE PRO CTF", build_game_variant_ctf, ARENA_REV1_GAMETYPE_FLAGS,
			3, 15, 150, 150, ARENA_RADAR_OFF, ARENA_VEHICLES_STOCK, TRUE },
		{ "AE COMP CTF", build_game_variant_ctf, ARENA_REV1_GAMETYPE_FLAGS,
			3, 15, 150, 150, ARENA_RADAR_OFF, ARENA_VEHICLES_STOCK, TRUE } },
	{ 2,
		{ "AE PRO KING", build_game_variant_team_king, ARENA_REV1_GAMETYPE_FLAGS,
			5, 15, 150, 150, ARENA_RADAR_OFF, ARENA_VEHICLES_STOCK, TRUE },
		{ "AE COMP KOH", build_game_variant_team_king, ARENA_REV1_GAMETYPE_FLAGS,
			5, 15, 150, 150, ARENA_RADAR_OFF, ARENA_VEHICLES_STOCK, TRUE } },
	{ 2,
		{ "AE PRO BALL", build_game_variant_team_oddball, ARENA_REV1_GAMETYPE_FLAGS,
			5, 15, 150, 150, ARENA_RADAR_OFF, ARENA_VEHICLES_STOCK, TRUE },
		{ "AE COMP OB", build_game_variant_team_oddball, ARENA_REV1_GAMETYPE_FLAGS,
			5, 15, 150, 150, ARENA_RADAR_OFF, ARENA_VEHICLES_STOCK, TRUE } },
};

static char const arena_gametypes_record_path[] = "z:\\saved\\playlists\\arena_gametypes.txt";
/* (the record being written: renamed over the record once complete) */
static char const arena_gametypes_record_new_path[] = "z:\\saved\\playlists\\arena_gametypes.new";
static char const arena_gametypes_record_new_name[] = "arena_gametypes.new";
static char const arena_gametypes_record_name[] = "arena_gametypes.txt";
static char const arena_gametypes_backup_path[] = "z:\\saved\\playlists_backup";

/* files_windows.c's (not in files.h): renames a file in its directory, FALSE
when a file has the new name */
boolean file_rename(struct file_reference *file, const char *new_name);

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
	boolean *written);
static void arena_gametypes_self_check(
	void);
static boolean arena_gametypes_record_read(
	char *record,
	unsigned long *record_length,
	short *revision);
static void arena_gametypes_record_write(
	char const *record,
	unsigned long record_length);
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
	/* the record's names, after a newline (each name is looked for as
	"\nNAME\n"), without its "#revision N" line */
	char record[ARENA_GAMETYPES_RECORD_SIZE + 3];
	unsigned long record_length = 1;
	short record_revision = 0;
	boolean record_changed = FALSE;
	boolean written = FALSE;
	short index;

	csmemset(record, 0, sizeof(record));
	record[0] = '\n';
	record_changed = arena_gametypes_record_read(record, &record_length, &record_revision);

	/* (those seeded under their old names: migrated if unchanged) */
	arena_gametypes_migrate(record, &record_length, record_revision, &record_changed, &written);
	arena_gametypes_self_check();

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

	if (record_changed || record_revision != ARENA_GAMETYPES_REVISION)
		arena_gametypes_record_write(record, record_length);

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

/* the record, else (a start that ended between writing a new record and
putting it in the old one's place) the new one: TRUE then, to be put in
place; a new one beside the record is a start's that ended before it was
complete: dropped */
static boolean arena_gametypes_record_read(
	char *record,
	unsigned long *record_length,
	short *revision)
{
	struct file_reference record_new;
	boolean new_there = file_reference_create_from_path(&record_new, arena_gametypes_record_new_path, FALSE) &&
		file_exists(&record_new);

	*revision = 0;
	if (arena_gametypes_record_read_file(arena_gametypes_record_path, record, record_length, revision))
	{
		if (new_there)
		{
			error(_error_silent, "arena gametypes: an unfinished record '%s' dropped", arena_gametypes_record_new_path);
			file_delete(&record_new);
		}
		return FALSE;
	}
	if (new_there &&
		arena_gametypes_record_read_file(arena_gametypes_record_new_path, record, record_length, revision))
	{
		error(_error_silent, "arena gametypes: the record recovered from '%s'", arena_gametypes_record_new_path);
		return TRUE;
	}

	return FALSE;
}

/* the record written (its "#revision N" line, the current revision, then
the names) to a new file, then put in the old one's place: a start that
ends at any moment leaves a whole record */
static void arena_gametypes_record_write(
	char const *record,
	unsigned long record_length)
{
	struct file_reference file;
	struct file_reference record_file;
	char revision_line[32];
	boolean success = FALSE;

	_snprintf(revision_line, sizeof(revision_line) - 1, "#revision %d\n", (int)ARENA_GAMETYPES_REVISION);
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
	if (file_reference_create_from_path(&record_file, arena_gametypes_record_path, FALSE) &&
		file_exists(&record_file) && !file_delete(&record_file))
	{
		error(_error_silent, "arena gametypes: failed to replace the record '%s'", arena_gametypes_record_path);
		return;
	}
	if (!file_rename(&file, arena_gametypes_record_name))
		error(_error_silent, "arena gametypes: failed to rename '%s'", arena_gametypes_record_new_path);

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
is there */
static boolean arena_gametypes_backup_file(
	struct file_reference *source,
	char const *directory_path)
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
	if (file_exists(&destination))
	{
		error(_error_silent, "arena gametypes: backup '%s' is there already: kept", path);
		return TRUE;
	}
	data = file_read_into_memory(source, &size);
	if (!data)
		return FALSE;
	if (file_create(&destination) && file_open(&destination, FLAG(_permission_write_bit)))
	{
		success = file_write(&destination, size, data);
		file_close(&destination);
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
			!arena_gametypes_backup_file(&record, revision_path))
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
	if (file_count <= 0)
		return FALSE;
	for (index = 0; index < file_count; index++)
	{
		if (!arena_gametypes_backup_file(&files[index], gametype_path))
			return FALSE;
	}
	return TRUE;
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
	boolean *written)
{
	long saved[ARENA_GAMETYPES_MAXIMUM_SAVED];
	word saved_count = 0;
	boolean listed = FALSE;
	boolean record_backed_up = FALSE;
	short backed_up_revision = NONE;
	short migration_index;

	for (migration_index = 0; migration_index < NUMBEROF(arena_gametype_migrations); migration_index++)
	{
		struct arena_gametype_migration const *migration = &arena_gametype_migrations[migration_index];
		char const *old_name = migration->old_row.name;
		char const *new_name = migration->new_row.name;
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
		if (!strstr(record, old_line) || strstr(record, new_line))
			continue;

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
			/* (deleted or renamed by the player: not brought back; or
			renamed already, by a start that ended before its record) */
			if (!saved_game_file_name_unique(new_wide))
				error(_error_silent, "arena gametype '%s' is there already (revision %d)", new_name, (int)migration->revision);
			else
				error(_error_silent, "arena gametype '%s' (once '%s') not seeded: the player removed '%s'",
					new_name, old_name, old_name);
			if (arena_gametypes_record_add(record, record_length, new_name))
				*record_changed = TRUE;
			continue;
		}

		arena_gametype_build(&migration->old_row, old_wide, &variant, &options);
		if (!playlist_profile_matches(profile_index, &variant, &options))
		{
			/* (the player's own now: kept, and the new one seeded beside it) */
			error(_error_silent, "arena gametype '%s' kept as it is (changed since seeded); '%s' seeded beside it",
				old_name, new_name);
			continue;
		}
		if (!saved_game_file_name_unique(new_wide))
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

	for (migration_index = 0; migration_index < NUMBEROF(arena_gametype_migrations); migration_index++)
	{
		struct arena_gametype const *new_row = &arena_gametype_migrations[migration_index].new_row;
		struct arena_gametype const *row = NULL;
		wchar_t wide_name[ARENA_GAMETYPE_NAME_LENGTH];
		struct game_variant variant[2];
		struct game_variant_options options[2];
		short index;

		/* (only a name's last migration) */
		for (index = migration_index + 1; index < NUMBEROF(arena_gametype_migrations); index++)
		{
			if (!strcmp(arena_gametype_migrations[index].new_row.name, new_row->name))
				break;
		}
		if (index < NUMBEROF(arena_gametype_migrations))
			continue;
		for (index = 0; index < NUMBEROF(arena_gametypes) && !row; index++)
		{
			if (!strcmp(arena_gametypes[index].name, new_row->name))
				row = &arena_gametypes[index];
		}
		if (!row)
		{
			error(_error_silent, "arena gametypes self-check: '%s' (revision %d's) is not seeded",
				new_row->name, (int)arena_gametype_migrations[migration_index].revision);
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
				"rewrites it", new_row->name, (int)arena_gametype_migrations[migration_index].revision);
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
