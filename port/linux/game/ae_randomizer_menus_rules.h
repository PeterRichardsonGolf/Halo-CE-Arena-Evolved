/*
AE_RANDOMIZER_MENUS_RULES.H

The words and rules behind the campaign randomizer's screens (ae_screen_randomizer.c): the rows of its pages and when each shows,
the values a row steps through, the hub's summary, and the eight saved setups' text format (the config's, so a setup written by
another build reads back). Pure: no engine includes (unit tests: port/linux/tests/ae_randomizer_menus_rules_test.c).

Every setting these screens write is a randomizer key (ae_randomizer_menus_key), a saved setup's slot or the item set's two
keys. None of them is read by a multiplayer game: the randomizer plays only a campaign level alone or in split screen, or a
Firefight game (ae_randomizer_rules.h).
*/

#ifndef __AE_RANDOMIZER_MENUS_RULES_H
#define __AE_RANDOMIZER_MENUS_RULES_H

#include <stddef.h>

/* ---------- a page's rows */

enum { AE_RND_PAGE_AI, AE_RND_PAGE_WEAPONS, AE_RND_PAGE_PICKUPS, AE_RND_PAGE_LOADOUT, AE_RND_PAGES };

enum
{
	AE_RND_ROW_HEAD,       /* a header: a word, no value */
	AE_RND_ROW_SWITCH,     /* "true" / "false" */
	AE_RND_ROW_CHOICE,     /* one of the row's choices */
	AE_RND_ROW_CUSTOM      /* one of the item set's types (the engine's list, not the table's) */
};

/* what a row waits on (a row whose condition does not hold is hidden: a family of dependent rows is not drawn) */
enum
{
	AE_RND_WHEN_ALWAYS,
	AE_RND_WHEN_ENEMIES,           /* game.randomizer_enemies */
	AE_RND_WHEN_ENEMY_ONE,         /* ... and the pool is ONE TYPE */
	AE_RND_WHEN_ENEMY_GRUNT, AE_RND_WHEN_ENEMY_ELITE, AE_RND_WHEN_ENEMY_JACKAL, AE_RND_WHEN_ENEMY_HUNTER, AE_RND_WHEN_ENEMY_FLOOD,
	AE_RND_WHEN_ENEMY_CUSTOM,      /* ... and the type is ITEM SET */
	AE_RND_WHEN_ALLIES,
	AE_RND_WHEN_ALLY_ONE,
	AE_RND_WHEN_ALLY_GRUNT, AE_RND_WHEN_ALLY_ELITE, AE_RND_WHEN_ALLY_JACKAL, AE_RND_WHEN_ALLY_HUNTER, AE_RND_WHEN_ALLY_FLOOD,
	AE_RND_WHEN_ALLY_CUSTOM,
	AE_RND_WHEN_WEAPONS, AE_RND_WHEN_WEAPON_ONE, AE_RND_WHEN_WEAPON_CUSTOM,
	AE_RND_WHEN_PICKUPS, AE_RND_WHEN_PICKUP_ONE, AE_RND_WHEN_PICKUP_CUSTOM,
	AE_RND_WHEN_LOADOUT_SET
};

struct ae_rnd_choice { const char *label, *value; };

struct ae_rnd_row
{
	int page, kind, when;
	const char *label;      /* "" for none */
	const char *key;        /* the setting; NULL for a header */
	const char *help;
	struct ae_rnd_choice const *choices;
	int choice_count;
	/* a custom row's engine list is the weapons' (else the actors') */
	int weapons;
};

int ae_rnd_row_count(void);
struct ae_rnd_row const *ae_rnd_row(int index);

/* what the rows read of the game and this PC */
struct ae_rnd_facts
{
	const char *(*get)(void *context, const char *key);   /* a setting's text as the config holds it ("true", "one") */
	void *context;
	int actor_types;      /* the item set's actor types on this PC (0: no item set) */
	int weapon_types;     /* ... and weapon types */
	int has_item_set;     /* a set is played (game.randomizer_tag_profile names a set this PC has) */
};

/* 1 when the row shows with the settings as they are */
int ae_rnd_row_shown(struct ae_rnd_row const *row, struct ae_rnd_facts const *facts);
/* 1 when a choice is offered now: ITEM SET only with a set that has what the row picks from, ENGINEER and SENTINEL with GO CRAZY's
ENGINEERS & SENTINELS, FLAMETHROWER with GO CRAZY's FLAMETHROWER */
int ae_rnd_choice_offered(struct ae_rnd_row const *row, int choice, struct ae_rnd_facts const *facts);
/* the index of a setting's value among the row's choices, 0 when none (a switch: 1 for "true") */
int ae_rnd_choice_index(struct ae_rnd_row const *row, const char *value);
/* the next offered choice from the one at index in direction (-1 / 1), the same index when none else is offered */
int ae_rnd_choice_step(struct ae_rnd_row const *row, int index, int direction, struct ae_rnd_facts const *facts);
/* the row's value as shown ("ON", "ELITE") */
const char *ae_rnd_row_value_text(struct ae_rnd_row const *row, struct ae_rnd_facts const *facts);

/* ---------- the hub */

/* the hub's AI REPLACEMENT value: "OFF", "ENEMIES", "ALLIES" or "ALL", then " \xE2\x80\xA2 nX" for extra enemies above 1 */
void ae_rnd_ai_summary(struct ae_rnd_facts const *facts, char *text, size_t size);
/* the hub's AI WEAPONS / WEAPON PICKUPS value: "OFF", "ON" or "ONE TYPE: <TYPE>" */
void ae_rnd_weapon_summary(struct ae_rnd_facts const *facts, int pickups, char *text, size_t size);
/* PLAYER LOADOUT's weapon values (game.randomizer_loadout_primary / _secondary): a stock kind of Content Evolved's ("pistol"), "none", one of
AE's by registry id ("ae:12": AE WEAPONS IN LEVELS' table, seven), or a weapon of the item set that plays ("tag:<hash>") */
enum { AE_RND_LOADOUT_AE_COUNT = 7, AE_RND_LOADOUT_SET_SLOTS = 16 };
enum { AE_RND_LOADOUT_VALUE_STOCK, AE_RND_LOADOUT_VALUE_NONE, AE_RND_LOADOUT_VALUE_AE, AE_RND_LOADOUT_VALUE_SET, AE_RND_LOADOUT_VALUE_UNKNOWN };
int ae_rnd_loadout_value_kind(char const *value);
/* the registry id an "ae:<id>" value names, 0 for a value that is not one of AE's seven */
unsigned int ae_rnd_loadout_ae_id(char const *value);
/* AE's weapons in the picker's order: their registry ids (0 past the seven) */
unsigned int ae_rnd_loadout_ae_id_at(int index);
/* the picker's last slots: the weapons of the item set that plays (label, "tag:<hash>"), `count` of them (more than the slots are left out) */
void ae_rnd_loadout_set_weapons(char const labels[][48], char const values[][24], int count);
/* a SET value the level could not load (found 0): 1 when the game is to fall back to the level's own loadout, which is when the
primary asked for is one of AE's or the item set's and is missing (a stock kind of Content Evolved's missing stays as it was) */
int ae_rnd_loadout_primary_missing(char const *requested, int found);

/* PLAYER LOADOUT's value: "DEFAULT", "RANDOM" or "SET" */
const char *ae_rnd_loadout_summary(struct ae_rnd_facts const *facts);
/* the lobby row's value: "OFF" while the randomizer is off, else the AI part then the parts that are on ("ENEMIES \xE2\x80\xA2 PICKUPS");
more than three parts become "n CHANGES" */
void ae_rnd_lobby_summary(struct ae_rnd_facts const *facts, char *text, size_t size);

/* ---------- the settings these screens write */

/* 1 when key is a randomizer setting, a saved setup's slot, or the item set's keys */
int ae_randomizer_menus_key(const char *key);

/* ---------- the saved setups (config.toml's game.randomizer_profile_1 to _8) */

enum { AE_RND_SETUPS = 8, AE_RND_SETUP_NAME = 24, AE_RND_SETUP_KEYS = 31, AE_RND_SETUP_VALUE = 32 };
/* the setup's settings in the order its text holds them ("" for the slot Go Crazy's once had: it is not part of a setup) */
const char *ae_rnd_setup_key(int index);

struct ae_rnd_setup
{
	char name[AE_RND_SETUP_NAME + 1];
	char value[AE_RND_SETUP_KEYS][AE_RND_SETUP_VALUE];
};
struct ae_rnd_setups
{
	struct ae_rnd_setup setup[AE_RND_SETUPS];
	int count;
};
/* letters, digits, space, _ and - kept, leading and trailing spaces dropped; 1 when anything is left */
int ae_rnd_setup_name_clean(char *name);
/* the setup written as its text: the name, then each value after a semicolon; the text's length, 0 when it does not fit */
size_t ae_rnd_setup_format(struct ae_rnd_setup const *setup, char *text, size_t size);
/* a setup read from its text; a field missing (a setup saved before it) takes defaults(key) when given, else stays empty. 1 when it has a name */
int ae_rnd_setup_parse(const char *text, struct ae_rnd_setup *setup, const char *(*defaults)(const char *key));
/* a setup kept under a name: the one of that name (any case) is replaced, else it goes last and, past eight, the oldest goes. Returns its index */
int ae_rnd_setups_keep(struct ae_rnd_setups *setups, struct ae_rnd_setup const *setup);
/* a setup removed, the later ones moving up; 0 when the index is none */
int ae_rnd_setups_remove(struct ae_rnd_setups *setups, int index);
/* 0 (the oldest setup's index) when saving a setup of this name would drop the oldest: eight are kept and none has the name; else -1 */
int ae_rnd_setups_would_drop(struct ae_rnd_setups const *setups, const char *name);
/* a setup's value for a key, NULL when the setup has no such key */
const char *ae_rnd_setup_value(struct ae_rnd_setup const *setup, const char *key);

#endif
