/*
AE_GLUE_SETUP_MAPS.H

The game setup spike's CHANGE MAP glue (ae_glue_setup_maps.c): the CHANGE MAP model's maps (ae_setup_maps.h) from the
game's multiplayer map list (ui_widget_port_multiplayer_maps), AE's map facts (port/assets/ae/maps.txt, embedded:
ae_maps_data.c), the map folders' pages (ae_map_folders.h) and the active disc; each map's facts read from its file
a few at a time (ae_mapinfo.h); its picture (maps\previews\<map>.png beside its file); the pick. Menus' thread only.
Include after cseries.h.

The groups, in order: FAVOURITES (this session's: not saved yet), HALO CE (the 13 stock maps), WARM UP, COMP (maps.txt's
competitive), BTB (large: maps.txt's size, else the file's play area), the folders' pages A-Z, the active disc's (NHE,
CE+ X: its name), MOD MAPS (loose community maps), CUSTOM EDITION, HALOMD.

Log: "ae change map: maps N, groups ..." as they load, "ae change map: facts N read (M unreadable)" when the reading
is done, "ae change map: picture '<map>' WxH|none: <why>", "ae change map: pick '<map>'".
*/

#ifndef __AE_GLUE_SETUP_MAPS_H
#define __AE_GLUE_SETUP_MAPS_H

#include "ae_setup_maps.h"

enum { AE_SETUP_GROUP_FAVOURITES = 0 };

/* the maps the game lists now; their facts not read yet (maps.txt's are there) */
void ae_setup_maps_load(struct ae_setup_maps *maps);
/* reads the facts of up to count maps not read yet; returns how many are left */
short ae_setup_maps_read_facts(struct ae_setup_maps *maps, short count);
/* the group a map comes from (-1 none) */
int ae_setup_map_source(short index);
/* the group a map comes from (its badge: HALO CE, MOD MAP, a folder's, a disc's ...) and how players get it */
const char *ae_setup_map_badge(struct ae_setup_maps const *maps, short index);
const char *ae_setup_map_sharing(struct ae_setup_maps const *maps, short index);
/* the map's picture as an ae_draw image (0: none), its size in width / height; loaded once a map, kept */
int ae_setup_map_picture(struct ae_setup_maps const *maps, short index, int *width, int *height);
/* FAVOURITE: toggled for this session (group FAVOURITES) */
void ae_setup_map_favourite(struct ae_setup_maps *maps, short index);
/* the map for the next game (as the PC menus' map list picks it); 1 when taken */
int ae_setup_map_pick(struct ae_setup_maps const *maps, short index);
/* the list index of the map used last, -1 for none */
short ae_setup_maps_last_used(void);

#endif
