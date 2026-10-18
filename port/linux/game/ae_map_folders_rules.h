/*
AE_MAP_FOLDERS_RULES.H

The pages of the PC menus' map lists (queue 142, notes/specs/2026-10-17-map-folders.md), as rules with no game in
them (port/linux/tests/ae_map_folders_rules_test.c). Each folder of the maps folder (one level) is a page, named
after the folder in capitals; the stock maps have a STOCK page wherever their files sit; then the folders' pages
A-Z; then MULTIPLAYER, the maps loose in the maps folder (and the mod's, the downloads, the HaloMD and Halo PC ones).
A page with no maps is not shown. A map's identity stays its name: a folder is only where this machine keeps it.
Folder pages after GulchCE's package folders (chestahh/GulchCE, CC0: docs/mcc_maps.md, mcc_maps.c).
*/

#ifndef AE_MAP_FOLDERS_RULES_H
#define AE_MAP_FOLDERS_RULES_H

enum
{
	/* a folder's name, and a map's (a file's name without .map) */
	AE_MAP_FOLDER_NAME_SIZE = 64,
	AE_MAP_NAME_SIZE = 64,
	/* a page's name, upper-cased (the chooser's spinner shows it) */
	AE_MAP_PAGE_NAME_SIZE = 25,
	/* the pages a list shows: STOCK, the folders', MULTIPLAYER */
	AE_MAP_PAGES_MAXIMUM = 32,
	/* the maps a list's pages hold */
	AE_MAP_PAGES_MAPS = 1024,
	/* the maps a folder index keeps */
	AE_MAP_FOLDER_INDEX_SIZE = 512
};

#define AE_MAP_PAGE_STOCK "STOCK"
#define AE_MAP_PAGE_LOOSE "MULTIPLAYER"
#define AE_MAP_PAGE_SINGLEPLAYER "SINGLEPLAYER"

/* whether a folder of the maps folder (or of a Custom Edition maps folder) is a page: not ce, previews, layouts,
_shared, nor one starting with . or _, and a plain name (no separator, not too long) */
int ae_map_folders_folder_is_page(char const *folder);

/* a folder's page name: its name upper-cased (ASCII letters; other bytes past 0x7E shown as ?), cut to the page name's
size; a folder whose name is a page of the list's own (STOCK, MULTIPLAYER, SINGLEPLAYER) has none (its maps are loose):
0. 1: written */
int ae_map_folders_page_name(char const *folder, char *out, unsigned int size);

/* folders in the order of their pages: A-Z, letters' case aside (then by case, so the order is the same each time) */
int ae_map_folders_compare(char const *a, char const *b);
void ae_map_folders_sort(char (*folders)[AE_MAP_FOLDER_NAME_SIZE], int count);

/* the first of a name wins (the stock maps, the mod's, the maps folder's, the folders A-Z, the downloads): the maps
found in the folders, by name, and each one's folder */
struct ae_map_folder_index
{
	int count;
	char names[AE_MAP_FOLDER_INDEX_SIZE][AE_MAP_NAME_SIZE];
	char folders[AE_MAP_FOLDER_INDEX_SIZE][AE_MAP_FOLDER_NAME_SIZE];
};

void ae_map_folder_index_clear(struct ae_map_folder_index *index);
/* 1: added; 0: the name is there already (a duplicate: the first stays); -1: no room, or a name or folder that does
not fit */
int ae_map_folder_index_add(struct ae_map_folder_index *index, char const *name, char const *folder);
/* the folder of a map's name (letters' case aside), or NULL */
char const *ae_map_folder_index_find(struct ae_map_folder_index const *index, char const *name);

/* a list's pages: each page's name and its maps (the list's indices, in the list's order) */
struct ae_map_pages
{
	int page_count;
	char names[AE_MAP_PAGES_MAXIMUM][AE_MAP_PAGE_NAME_SIZE];
	int first[AE_MAP_PAGES_MAXIMUM];
	int count[AE_MAP_PAGES_MAXIMUM];
	int maps[AE_MAP_PAGES_MAPS];
};

/* the pages of map_count maps: map i is a stock one if stock[i], else on its folder's page (folder_pages[i], a page
name; NULL or "" when it is loose). STOCK first, then the folders' pages A-Z, then MULTIPLAYER; pages with no maps
left out; a folder page past AE_MAP_PAGES_MAXIMUM - 2 others shows its maps on MULTIPLAYER; maps past
AE_MAP_PAGES_MAPS are left out */
void ae_map_pages_build(struct ae_map_pages *pages, int map_count, unsigned char const *stock,
	char const *const *folder_pages);
/* a page by its name, or -1 */
int ae_map_pages_find(struct ae_map_pages const *pages, char const *name);
/* the page a map (a list index) is on, and its entry there; -1 for none */
int ae_map_pages_page_of(struct ae_map_pages const *pages, int map);
int ae_map_pages_entry_of(struct ae_map_pages const *pages, int page, int map);
/* the list index of a page's entry, or -1 */
int ae_map_pages_map_at(struct ae_map_pages const *pages, int page, int entry);

/* the chooser (the first row's spinner): SINGLEPLAYER at 0 when it is offered (not on split screen), then the pages.
Its values' count (at least 1); the page of a value (-1: SINGLEPLAYER; a value out of range is taken as the nearest);
the value of a page (-1: SINGLEPLAYER, or the first page when it is not offered) */
int ae_map_chooser_count(int page_count, int singleplayer);
int ae_map_chooser_page(int value, int page_count, int singleplayer);
int ae_map_chooser_value(int page, int page_count, int singleplayer);

#endif
