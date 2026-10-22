/*
AE_MAP_FOLDERS.H

Map folders (queue 142, notes/specs/2026-10-17-map-folders.md): the maps in the folders of the maps folder (one
level: maps\<folder>\, and for Custom Edition maps maps_ce\<folder>\ and maps\ce\<folder>\) are found and played by
their names as the loose ones are, and the PC menus' map lists show them as pages of their first row's chooser:
SINGLEPLAYER, STOCK, the folders A-Z, MULTIPLAYER (ae_map_folders_rules.h). Included after cseries.h (boolean).
*/

#ifndef AE_MAP_FOLDERS_H
#define AE_MAP_FOLDERS_H

struct widget_instance;

/* ---------- finding the maps (custom_maps.c, cache_files.c, map_families.c, ui_map_list.c) */

/* custom_maps.c's list: each page folder of the maps folder scanned (A-Z), after the maps folder's own maps */
void ae_map_folders_scan(void (*scan)(char const *directory));
/* cache_files_map_file_path's: an Xbox map's file in its folder when the mod's and the maps folder's are not there */
void ae_map_folders_xbox_path(char const *map_name, char *path, long size);
/* cache_files_map_file_path's: TRUE for a mod's main menu map the main menu's cache file cannot hold (then the game's
own is used; logged once) */
boolean ae_map_folders_mod_map_refused(char const *path, char const *map_name);
/* map_families.c's: a Custom Edition map's file in a page folder of maps_ce\ or maps\ce\ (a cache file of the version
given), after the family's own places; TRUE: path is it */
boolean ae_map_folders_ce_find(char const *file, long version, char *path, long size);
/* ... and its list: the folders' Custom Edition multiplayer maps not listed yet, each told to found (as
map_family_list's places are; version: the family's cache files'); listed, *listed_count: the files listed so far (each MAP_FAMILY_FILE_LENGTH bytes) */
void ae_map_folders_ce_list(short family, long version, void (*found)(char const *file, void *context), void *context,
	char *listed, long *listed_count, long maximum, long file_length);
/* map sharing's host (ae_map_share_all.c): each page folder of maps_ce\ and maps\ce\ (A-Z) */
void ae_map_folders_ce_scan(void (*scan)(char const *directory));
/* the map list's pages logged ("[maps] page <NAME>: N maps") when they are not the ones logged last: the stock maps
first (HALO_STOCK_MULTIPLAYER_MAP_COUNT of them), then the others */
void ae_map_folders_log_pages(char const *const *names, long count);
/* the file a map's picture and layout are looked for beside when its own folder has none: the maps folder's (a folder
map's root); FALSE when map_path is not in a page folder */
boolean ae_map_folders_root_path(char const *map_path, char *path, long size);

/* (the game setup spike's CHANGE MAP) the page of a list row's map: its folder's page name, "" when it has none (a
stock or loose one) */
void ae_map_folders_row_page(char const *name, char *page, unsigned int size);

/* ---------- the PC menus' map lists (menu_functions.c) */

enum
{
	AE_MAP_PAGES_NEW_GAME,
	AE_MAP_PAGES_MAP_SCREEN,
	NUMBER_OF_AE_MAP_PAGES_LISTS
};

/* a list opened (its chooser's spinner; the list's names and count; whether SINGLEPLAYER is offered; the list index
of the map used last, NONE for none): its pages made, the chooser's values set; the entry of the page to open on, or
NONE to open on SINGLEPLAYER (New Game's start; the Map screen's, with no spinner, the map used last) */
short ae_map_pages_begin(struct widget_instance *spinner, short list, char const *const *names, short count,
	boolean singleplayer, short last_used);
/* whether the pages are this spinner's (the list's open) */
boolean ae_map_pages_on(struct widget_instance *spinner);
/* the chooser's value read: the kind it shows (0 SINGLEPLAYER, 1 MULTIPLAYER: menu_functions.c's MAP_KIND_*), its page
kept */
short ae_map_pages_kind(struct widget_instance *spinner);
/* the chooser set to a kind (SINGLEPLAYER, or the page shown of MULTIPLAYER) */
void ae_map_pages_set(struct widget_instance *spinner, short kind);
/* TRUE once after the chooser turned to another page (the list's rows start at its top) */
boolean ae_map_pages_turned(void);
/* the page's count of maps (count, the list's, when no page is shown) */
short ae_map_pages_count(short count);
/* the list index of a page's entry (entry itself when no page is shown; NONE for none) */
short ae_map_pages_map(short entry);
/* ... and that map remembered as the one shown on its page, for the next time the list opens this session */
short ae_map_pages_shown_map(short entry);
/* ui_widget.c's spinner_string_list_get_string: the chooser's value's name (the page's), or NULL for another string
list's */
wchar_t const *ae_map_pages_spinner_string(long string_list_index, short string_index);

#endif
