/*
AE_ITEM_SETS_DATA.H

What the ITEM SETS page reads and does, as one table of functions (struct ae_is_backend): the engine's (ae_glue_item_sets.c, over
the tag injector and its profiles) or the made-up one DEV MODE's `menu maps sets` stages (ae_item_sets_data.c: it keeps its sets
in memory, so adding, removing and playing work on it and touch no file). Plain C, no engine headers.
*/

#ifndef AE_ITEM_SETS_DATA_H
#define AE_ITEM_SETS_DATA_H

#include "ae_item_sets_rules.h"

enum { AE_IS_SETS_MOST = 32, AE_IS_MAPS_MOST = 1200, AE_IS_ENTRIES_MOST = 1024, AE_IS_ITEMS_MOST = 1024 };

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
};

/* the engine's table (ae_glue_item_sets.c) */
struct ae_is_backend const *ae_is_backend_engine(void);
/* the made-up table: a fresh copy of its sets each time it is asked for */
struct ae_is_backend const *ae_is_backend_staged(int empty);

/* (staged) a Custom Edition map is "being played": maps of that kind can't open */
void ae_is_staged_block_ce(int block);

#endif
