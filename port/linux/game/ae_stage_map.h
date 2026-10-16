/*
AE_STAGE_MAP.H

The next multiplayer game's map when the one the stage names has no file
(ae_stage_map.c).
*/

#ifndef __AE_STAGE_MAP_H
#define __AE_STAGE_MAP_H

/* (game_engine.c's game_engine_playlist_next, AE hook) map_name (a scenario path, levels\test\<map>\<map>) changed to
the first map of the menus' map list that has a file when its own has none; left as it is otherwise */
void ae_stage_map_check(char *map_name, long size);

#endif
