/*
AE_GLUE_CE_MAPS.H

What the AE hook lines for Custom Edition maps call (ae_hooks.txt lists them):
cache_files.c's scenario_tags_load, menu_tags.c's memory, ce_map_checks.c's
tag reads. Plain types only.
*/

#ifndef __AE_GLUE_CE_MAPS_H
#define __AE_GLUE_CE_MAPS_H

/* (scenario_tags_load, a Custom Edition map's tags loaded and repaired, just
before it returns) what an Xbox map's tags get there and a Custom Edition
map's did not: the menus' tags (the pause menu's SETTINGS) and the high-res
HUD's bitmaps */
void ae_ce_map_tags_loaded(char const *map_name, long scenario_tag_index);

/* (menu_tags.c's allocate) a block of the menus' tags, size bytes, which the
game reads as tags: noted only while a Custom Edition map's are made;
(menu_tags_release) all of them let go */
void ae_ce_map_menu_block(void const *block, long size);
void ae_ce_map_menu_blocks_forget(void);
/* (ce_map_checks.c's ce_tags_pointer, a read outside a Custom Edition map's
tag cache) whether the size bytes at the Xbox address lie in one block of
the menus' tags of the Custom Edition map loaded: 1, read as they are; else 0 */
int ae_ce_map_menu_memory(unsigned long address, long size);

#endif
