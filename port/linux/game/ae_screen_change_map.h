/*
AE_SCREEN_CHANGE_MAP.H

The game setup spike's CHANGE MAP screen (ae_screen_change_map.c; notes/plans/2026-10-21-game-setup-spike.md): the
maps in folders that open in place, filtered by AE's gametypes and NHE. A debug screen for now:
debug.ae_test_screen 31.
*/

#ifndef __AE_SCREEN_CHANGE_MAP_H
#define __AE_SCREEN_CHANGE_MAP_H

/* opens the screen over the stack on the filter given (ae_setup_maps.h's AE_MF_*; out of range: ALL); closes
upstream's menus. 0 when the stack is full */
int ae_screen_change_map_open(int filter);
/* opens it over the stack for a caller (CREATE GAME): the folder of the map current (a list index; -1: the one used
last) open; A on a map tells picked (its list index and name) and closes, choosing nothing itself */
int ae_screen_change_map_pick(int filter, short current, void (*picked)(short list_index, char const *name));

#endif
