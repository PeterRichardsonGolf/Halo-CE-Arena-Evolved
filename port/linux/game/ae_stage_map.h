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

/* whether a map of the menus' map list (a scenario path, levels\test\<map>\<map>, a file name or <file>@<family>)
has its file here, as the game's loader looks for it */
int ae_stage_map_file_present(char const *list_name);

/* the menus' default map: preferred (the map used last) when it has a file, else the first map of names that has
one; -1 when none has */
short ae_stage_map_playable_index(char const *const *names, short count, short preferred);

#endif
