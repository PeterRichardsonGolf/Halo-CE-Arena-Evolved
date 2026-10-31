/*
AE_GLUE_SETUP_MAPS.H

The game setup spike's CHANGE MAP glue (ae_glue_setup_maps.c): the CHANGE MAP model's maps (ae_setup_maps.h) from the
game's multiplayer map list (ui_widget_port_multiplayer_maps), AE's map facts (port/assets/ae/maps.txt, embedded:
ae_maps_data.c), the map folders' pages (ae_map_folders.h) and the active disc; each map's facts read from its file
a few at a time (ae_mapinfo.h); its picture (maps\previews\<map>.png beside its file); the pick. Menus' thread only.
Include after cseries.h.

The groups, in order (AE menus Phase 2, plan .superpowers/sdd/2026-10-27/menus-p2-pickers.md): FAVOURITES and the
player's own folders A-Z (ae_glue_picker_stores.h: saved), then by source: STOCK OG (the 13 Xbox maps), STOCK GEARBOX
(the PC maps, maps\ce\gearbox), NHE (NHE's maps: maps.txt's nhe-vehicles, or the NHE disc's), COMMUNITY COMPETITIVE (the rest of the
40-map community set: maps.txt's halo-og), another disc's (CE+ X: its name), each map pack's
(maps_<pack>: its name, HALO CE+ for maps_cex), then WARM UP and BTB (large: maps.txt's
size, else the file's play area), which list a map in its source's group too, the folders' pages A-Z, MOD MAPS (loose
community maps), CUSTOM EDITION, HALOMD.

Log: "ae change map: maps N, groups ..." as they load, "ae change map: facts N read (M unreadable)" when the reading
is done, "ae change map: picture '<map>' WxH|none: <why>", "ae change map: pick '<map>'".
*/

#ifndef __AE_GLUE_SETUP_MAPS_H
#define __AE_GLUE_SETUP_MAPS_H

#include "ae_setup_maps.h"

enum { AE_SETUP_GROUP_FAVOURITES = 0 };
/* (the player's folders are the first groups: a group of theirs is its folder's index) */

/* the maps the game lists now; their facts not read yet (maps.txt's are there) */
void ae_setup_maps_load(struct ae_setup_maps *maps);
/* the game's maps queued for the facts' worker (CREATE GAME opening: they are read before CHANGE MAP is) */
void ae_setup_maps_prefetch(void);
/* the facts the worker has read copied into the maps not read yet (count: unused, never blocks); returns how many
are left */
short ae_setup_maps_read_facts(struct ae_setup_maps *maps, short count);
/* the group a map comes from (-1 none) */
int ae_setup_map_source(short index);
/* the group a map comes from (its badge: STOCK OG, MOD MAP, a folder's, a disc's ...) and how players get it */
const char *ae_setup_map_badge(struct ae_setup_maps const *maps, short index);
const char *ae_setup_map_sharing(struct ae_setup_maps const *maps, short index);
/* the map's picture as an ae_draw image (0: none), its size in width / height; loaded once a map, kept */
int ae_setup_map_picture(struct ae_setup_maps const *maps, short index, int *width, int *height);
/* a map's id (the player's folders' and RECENT's) from its list name: the name's last part */
void ae_setup_map_id(char const *list_name, char *id, int size);
/* the player's folders (ae_picker_store's) made the maps' first groups again: their names, flags and maps */
void ae_setup_maps_folders_apply(struct ae_setup_maps *maps);
/* a map put in or taken out of one of the player's folders (FAVOURITES 0), saved: 1 now in it, 0 now out, -1 refused.
Log "ae change map: '<map>' in|out of <folder>" */
int ae_setup_map_folder_toggle(struct ae_setup_maps *maps, short index, short folder);
/* the map for the next game (as the PC menus' map list picks it); 1 when taken */
int ae_setup_map_pick(struct ae_setup_maps const *maps, short index);
/* the list index of the map used last, -1 for none */
short ae_setup_maps_last_used(void);
/* whether the map the game will use is an NHE map (maps.txt's nhe-vehicles), or the NHE disc is loaded: GAME OPTIONS
shows NHE's rows then */
int ae_setup_current_map_is_nhe(void);

#endif
