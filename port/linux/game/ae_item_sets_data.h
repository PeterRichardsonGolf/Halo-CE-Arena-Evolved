/*
AE_ITEM_SETS_DATA.H

What the ITEM SETS page reads and does, as one table of functions (struct ae_is_backend): the engine's (ae_glue_item_sets.c, over
the tag injector and its profiles) or the made-up one DEV MODE's `menu maps sets` stages (ae_item_sets_data.c: it keeps its sets
in memory, so adding, removing and playing work on it and touch no file). Plain C, no engine headers.
*/

#ifndef AE_ITEM_SETS_DATA_H
#define AE_ITEM_SETS_DATA_H

#include "ae_item_sets_rules.h"
#include "ae_item_swaps_rules.h"

enum { AE_IS_SETS_MOST = 64, AE_IS_MAPS_MOST = 1200, AE_IS_ENTRIES_MOST = 1024, AE_IS_ITEMS_MOST = 1024 };

/* where a map is from */
enum { AE_IS_SOURCE_CE, AE_IS_SOURCE_MCC, AE_IS_SOURCE_WORKSHOP };

struct ae_is_set
{
	char name[48];
	/* the folder the engine names it by (game.randomizer_tag_profile) */
	char folder[64];
	int playing;
	long items, on;
	unsigned long long bytes;
	long maps;
	/* the first map it was taken from, and how many items of each kind are on */
	char first_map[48];
	long kinds[AE_IS_KINDS];
};

/* a campaign swap: Content Evolved's campaign profile (a file of tag_injector/campaign) */
enum { AE_IS_SWAPS_MOST = 64 };
struct ae_is_swap
{
	char name[48];
	/* the file the engine names it by (game.campaign_tag_profile; without .toml) */
	char file[64];
	/* the set it plays: its folder, its name when this PC has it; its items on */
	char set_folder[64], set_name[48];
	int set_found;
	long set_items_on;
	/* the one the lobby plays */
	int in_use;
	/* the file's "weapon_replace" and "enemy_replace" ("random", "manual") */
	char weapon_replace[8], enemy_replace[8];
};

struct ae_is_map
{
	/* the name the engine opens it by (CE: the file's stem; MCC and Workshop: <name>@mcc, <name>@ws<id>) */
	char level[96];
	char name[64];
	/* the map's file name without .map (the level name; what a set keeps as the map an item came from) */
	char file[64];
	/* where it sits, as a line ("maps_ce > tsce"); "" none */
	char where[96];
	int source;
	int campaign;
	unsigned long long size;
	/* items of it in the set shown (0: none) */
	long in_set;
};

/* an item of the open map */
struct ae_is_item
{
	/* the engine's index of it among the open map's items of its kind */
	long index;
	char path[256];
	/* the path is made up (its map hides its names) */
	int made_up;
	int in_set;
	/* looked up (parts -1: not yet): the tags it takes with it, whether it has every part it needs, or why it can't be added */
	long parts;
	unsigned long long bytes;
	int works;
	int refused;
	char reason[200];
};

/* an item of a set */
struct ae_is_entry
{
	/* the engine's index in the set (kept as given back) */
	long tag;
	char path[256];
	char map[48];
	int kind;
	int on;
	char failed[24];
	unsigned long long bytes;
	/* what is known about it: it has every part it needs (1), else 0 and what is missing */
	int works;
	char missing[120];
	int fits_player;
	/* the name typed for it ("": its own, from the map's path) */
	char name[48];
};

struct ae_is_added
{
	int ok;
	long carried;
	char carried_names[160];
	unsigned long long kept, read;
	char reason[200];
};

struct ae_is_backend
{
	/* the sets, playing one marked */
	int (*sets)(struct ae_is_set *sets, int maximum);
	/* plays the set (its folder), or none (NULL folder): 1 when done */
	int (*play)(char const *folder);
	/* a new set named so: its folder, or "" */
	int (*create)(char const *name, char *folder, int size);
	int (*copy)(char const *folder, char const *name, char *new_folder, int size);
	int (*rename)(char const *folder, char const *name, char *new_folder, int size);
	int (*remove_set)(char const *folder);
	/* a set's items, by kind then name; each set's `tag` is the engine's */
	int (*entries)(char const *folder, struct ae_is_entry *entries, int maximum);
	int (*entry_enable)(char const *folder, long tag, int on);
	int (*entry_remove)(char const *folder, long tag);

	/* the maps on this PC; `in_set` filled for the set shown */
	int (*maps)(char const *folder, struct ae_is_map *maps, int maximum);
	/* NULL: maps of that source open now; else why not */
	char const *(*maps_reason)(int source);
	/* opens the map (a row of maps()'s list): 1, else the reason */
	int (*map_open)(int map, char *reason, int size);
	void (*map_close)(void);
	/* the open map's kind: how many, and its items (sorted by name) with `in_set` for the set */
	long (*kind_count)(int kind);
	int (*kind_items)(int kind, char const *folder, struct ae_is_item *items, int maximum);
	/* an item's lookup: its parts, bytes, or why not */
	void (*item_look)(int kind, int index, struct ae_is_item *item);
	/* whether a kind can be added at all */
	int (*kind_addable)(int kind);
	/* adds, then fills what came of it */
	void (*item_add)(char const *folder, int kind, int index, struct ae_is_added *added);
	/* takes the item of the open map out of the set */
	int (*item_remove)(char const *folder, int kind, int index);
	/* the randomizer is on (the set plays through it) */
	int (*randomizer_on)(void);
	/* an item's setting by its key in the set's file (name, faction, type, class, custom_type, hud, replace: "" when not set); written
	back with set (the engine keeps letters, digits, spaces, - and _): 1 when done */
	int (*entry_option)(char const *folder, long tag, char const *key, char *value, int size);
	int (*entry_set_option)(char const *folder, long tag, char const *key, char const *value);
	/* the set's body (player_biped: an item's path; player_hands: "path#map"; player_movement: "own"; "" none), read and written */
	int (*body_option)(char const *folder, char const *key, char *value, int size);
	int (*body_set_option)(char const *folder, char const *key, char const *value);
	/* what the fitness check says of a biped of the set (known 0: no level has read it) */
	void (*entry_fit)(char const *folder, long tag, struct ae_is_fit *fit);
	/* whether the campaign's tags are known here, so TAKES THE CAMPAIGN'S PLACE can be offered */
	int (*replace_known)(void);
	/* whether an item of the set is of a path a campaign mission has (TAKES THE CAMPAIGN'S PLACE is offered for it) */
	int (*entry_stock)(char const *folder, long tag);

	/* the campaign swaps, the one the lobby plays marked; a new swap named so, playing the set of that folder: its file; removed;
	renamed (the name it is shown by) */
	int (*swaps)(struct ae_is_swap *swaps, int maximum);
	int (*swap_create)(char const *name, char const *set_folder, char *file, int size);
	int (*swap_remove)(char const *file);
	int (*swap_rename)(char const *file, char const *name);
	/* a swap's setting by its key in its file ("profile", "weapon_rotation", "player_biped", ...: "" when not set), and written */
	int (*swap_option)(char const *file, char const *key, char *value, int size);
	int (*swap_set_option)(char const *file, char const *key, char const *value);
	/* a swap's lists: a slot's picks (section "weapons", "enemies" or "loadout"; key a slot's), each a path ("original", "stock:<weapon>",
	or an item of the set) */
	int (*swap_list_count)(char const *file, char const *section, char const *key);
	int (*swap_list_get)(char const *file, char const *section, char const *key, int entry, char *path, int size);
	int (*swap_list_has)(char const *file, char const *section, char const *key, char const *path);
	int (*swap_list_set)(char const *file, char const *section, char const *key, char const *path, int on);
	/* the lobby's choice, AE_IS_PICK_*: what is picked now (its swap file or its set folder in id), and a pick written (id: the swap's
	file or the set's folder; ignored for NONE): 1 when done */
	int (*picked)(char *id, int size);
	int (*pick)(int pick, char const *id);
	/* the firing range: whether it can start at all (the map it is built on is here), and a start with the set of that folder (0 and why
	when it does not) */
	int (*range_ready)(void);
	int (*range_start)(char const *folder, char *why, int size);
};

/* the engine's table (ae_glue_item_sets.c) */
struct ae_is_backend const *ae_is_backend_engine(void);
/* the made-up table: a fresh copy of its sets each time it is asked for */
struct ae_is_backend const *ae_is_backend_staged(int empty);

/* (staged) a Custom Edition map is "being played": maps of that kind can't open */
void ae_is_staged_block_ce(int block);

#endif
