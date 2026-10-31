/*
AE_SCREEN_CHANGE_MAP.H

The game setup spike's CHANGE MAP screen (ae_screen_change_map.c; notes/plans/2026-10-21-game-setup-spike.md): the
maps in folders that open in place, filtered by AE's gametypes and NHE. A debug screen for now:
debug.ae_test_screen 31.
*/

#ifndef __AE_SCREEN_CHANGE_MAP_H
#define __AE_SCREEN_CHANGE_MAP_H

/* opens the screen over the stack on the filter given (ae_setup_maps.h's AE_MF_*; out of range: ALL) and a search
(NULL: none); closes upstream's menus. 0 when the stack is full */
int ae_screen_change_map_open(int filter, const char *query);
/* opens it over the stack for a caller (CREATE GAME's MAP row, the pause menu's next map): the folder of the map
current (a list index; -1: the one used last) open; A on a map tells picked (its list index and name) and closes,
choosing nothing itself. RECENT (the maps played last: the picker's own, owner 2026-10-28) shows here too: A on one
tells picked its map, as on any map (the gametype stays) */
int ae_screen_change_map_pick(int filter, short current, void (*picked)(short list_index, char const *name));

#endif
