/*
AE_GLUE_MCC_MAPS.H

(Arena Evolved 0.6.0) MCC's maps read in place, for the menus: the stock maps the player's MCC has
(<name>@mcc) and the Workshop maps it found (<name>@ws<item id>, ae_map_kind.h). The game's map list gets the
playable ones (ui_map_list.c's refresh, an AE hook), so the menus pick them, name them and keep them in RECENT as
any map; CHANGE MAP's glue (ae_glue_setup_maps.c) also lists the Workshop maps Steam is updating, dimmed. The catalog
and the files are port/linux/src/ae_mcc_platform.c's.
*/

#ifndef __AE_GLUE_MCC_MAPS_H
#define __AE_GLUE_MCC_MAPS_H

#include <wchar.h>

enum { AE_MCC_MENU_MAP_IDENTITY = 64, AE_MCC_MENU_MAP_NAME = 64, AE_MCC_MENU_MAP_ITEM = 24,
	AE_MCC_MENU_MAP_PATH = 1024,
	/* (MCC's 19 stock maps and up to 1024 of a Steam library's Workshop maps) */
	AE_MCC_MENU_MAPS_MAXIMUM = 1024 + 32 };
enum { AE_MCC_MENU_STOCK, AE_MCC_MENU_WORKSHOP };

struct ae_mcc_menu_map
{
	char identity[AE_MCC_MENU_MAP_IDENTITY];   /* <name>@mcc, <name>@ws<item id> */
	char name[AE_MCC_MENU_MAP_NAME];           /* shown (UTF-8): the stock map's, the Workshop map's title */
	char item_id[AE_MCC_MENU_MAP_ITEM];        /* a Workshop map's, "" a stock one */
	char item_title[AE_MCC_MENU_MAP_NAME];     /* its Workshop item's title (an item of several maps is a section), "" none */
	char thumbnail[AE_MCC_MENU_MAP_PATH];      /* host path, "" none */
	int kind;                                  /* AE_MCC_MENU_* */
	int updating;                              /* Steam is writing its file: listed, never picked */
};

/* the game's list's add, ui_map_list.c's add_entry */
typedef void (*ae_mcc_maps_add_function)(char const *map_name, wchar_t const *display_name, wchar_t const *lobby_name,
	wchar_t const *description, short xbox_index, short picture_index, char const *picture_map);
/* (ui_map_list.c's refresh, after the packs') the playable MCC maps added; the Workshop catalog looked at again first
(ae_mcc_workshop_rescan: cheap when nothing changed). Nothing without MCC or with game.mcc_use "no" */
void ae_mcc_maps_list(ae_mcc_maps_add_function add);
/* the menus' MCC maps, the stock ones first (in MCC's order), then the Workshop's (the catalog's), the ones Steam is
updating among them; at most maximum, how many. A Workshop identity of 64 characters or more is never listed (the
loader's level names hold 63) */
int ae_mcc_menu_maps(struct ae_mcc_menu_map *maps, int maximum);
/* an MCC identity's file there now (ae_stage_map.c: a map gone since the list was made is not started): 1 yes, 0 no
or not an MCC identity */
int ae_mcc_map_present(char const *identity);
/* (0.7.1) a game list's MCC Workshop map (its name perhaps cut to the list's 32 characters, the item id whole:
ae_map_kind_listing_name) found among this PC's playable Workshop maps: 1 and its whole identity; 0 none, or not a
Workshop name */
int ae_mcc_listed_map(char const *listed, char *identity, int size);
/* (0.7.1, the host's game list lines, AE hooks in network_server_manager.c) a map's name as the list carries it */
char const *ae_mcc_listing_map(char const *map);

#endif
