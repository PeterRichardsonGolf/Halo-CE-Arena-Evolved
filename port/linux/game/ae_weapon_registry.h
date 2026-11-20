/*
AE_WEAPON_REGISTRY.H

The weapon cache's id registry as the engine's consumers read it (design 4.2): one row per weapon with the columns every
slot-keyed switch used to hard-code (kill feed word and picture, bot values, medal role, DualSense trigger family, layout
words, the stock fallback). The rows are generated (ae_weapon_registry.inc, tools/weapon_cache/registry.py gen) and held
equal to tools/weapon_cache/registry.json by a test. These are code behaviour, not tag data: nothing here reads the cache
file. No engine includes (port/linux/tests/ae_weapon_registry_test.c).

A weapon's id is permanent; ids 1-24 are the legacy pack slots 1-24 (legacy_slot equal to the id), which is what lets
ae_weapon_slots.h stay a facade. A held or retired row still has its columns (an id a gametype names plays the stock
fallback), but `active` says whether the cache carries it.
*/

#ifndef AE_WEAPON_REGISTRY_H
#define AE_WEAPON_REGISTRY_H

enum
{
	/* the most layout words (the key included) a row has (0.8.0: 4, the PROTOMAG's starter_pistol, digsite_pistol,
	digsite_scoped and protomag) */
	AE_WEAPON_REGISTRY_ALIASES_MAXIMUM = 4,
	/* the last id that is also a legacy slot */
	AE_WEAPON_REGISTRY_LEGACY_LAST = 24
};

struct ae_weapon_registry_row
{
	unsigned int id;
	char const *key;
	char const *display;
	unsigned int legacy_slot; /* 1-24, 0 none */
	char const *status; /* "active", "held", "retired" */
	char const *stock_fallback; /* "pistol" ... "none" */
	char const *feed_word; /* the kill feed's word ("BR") */
	char const *feed_picture; /* the kill feed picture's key ("BATTLE_RIFLE"), "text" (the word, no picture), or a pack tag path */
	short bot_value, bot_tap_ticks; /* tap_ticks -1: the weapon's rifle profile decides */
	float bot_range;
	unsigned char bot_precision, bot_scoped;
	char const *columns_from; /* "code", "name_search", "proposed" */
	char const *medal_role; /* "battle_rifle" ... "other" */
	char const *trigger_family; /* the DualSense trigger family ("carbine"), "none" */
	unsigned char power;
	unsigned char active; /* status "active" */
	char const *replaces_map_own; /* "", "map_battle_rifle", "map_needler" */
	char const *menu_kind; /* "PRECISION", "AUTOMATIC", "PISTOL", "LONG RANGE", "HEAVY", "MELEE", "CLOSE" */
	char const *aliases[AE_WEAPON_REGISTRY_ALIASES_MAXIMUM + 1]; /* the layout words: the key first, NULL ended */
};

/* the table: its rows ascending by id */
unsigned int ae_weapon_registry_count(void);
struct ae_weapon_registry_row const *ae_weapon_registry_at(unsigned int index);
/* a row by id (NULL for an id the registry does not know) */
struct ae_weapon_registry_row const *ae_weapon_registry_row(unsigned int id);
/* a row by its legacy pack slot (slot n is id n for 1-24; NULL otherwise) */
struct ae_weapon_registry_row const *ae_weapon_registry_row_of_slot(int slot);
/* a key or layout word to a weapon id, case aside; 0 for none */
unsigned int ae_weapon_registry_id_of_word(char const *word);
/* the stock fallback word as ae_weapon_slots.h's enum ae_weapon_stock value (assault_rifle 2, pistol 3, shotgun 4,
sniper_rifle 5, rocket_launcher 6, needler 9, none 0) */
int ae_weapon_registry_stock(char const *fallback);

#endif /* AE_WEAPON_REGISTRY_H */
