/*
AE_GLUE_CE_MAPS.H

What the AE hook lines for Custom Edition maps call (ae_hooks.txt lists them):
cache_files.c's scenario_tags_load, menu_tags.c's memory, ce_map_checks.c's
tag reads. Plain types only.
*/

#ifndef __AE_GLUE_CE_MAPS_H
#define __AE_GLUE_CE_MAPS_H

struct event_record;
struct widget_instance;

/* (scenario_tags_load, a Custom Edition map's tags loaded and repaired, just
before it returns) what an Xbox map's tags get there and a Custom Edition
map's did not: the menus' tags (the pause menu's SETTINGS) and the high-res
HUD's bitmaps */
void ae_ce_map_tags_loaded(char const *map_name, long scenario_tag_index);
/* an Xbox map with Halo PC's pause menus: the same repair; TRUE when it added the menus' tags itself */
int ae_xbox_map_menu_tags_loaded(char const *map_name);

/* (menu_tags_loaded) whether it is making a validated Custom Edition map's
pause support, rather than ui.map's or an Xbox map's tags */
int ae_ce_map_building_menu_tags(void);

/* (ui_widget_event_handler_function_invoke) the few Halo PC pause functions
this engine can perform correctly; 1 when handled, with result set */
int ae_ce_map_event_function(struct widget_instance *widget, struct event_record *event,
	word function_index, boolean *widget_deleted, boolean *result);
/* (ae_hooks.h) ae_ce_map_view_place is defined here */
/* advances RESTART GAME through postgame, the lobby and START */
void ae_ce_map_update(void);
/* (menu_tags_loaded, AE hook) the maps' own button prompts widened where the port's text outgrows them */
void ae_ce_map_prompts_fit(void);

/* (menu_tags.c's allocate) a block of the menus' tags, size bytes, which the
game reads as tags: noted only while a Custom Edition map's are made;
(menu_tags_release) all of them let go */
void ae_ce_map_menu_block(void const *block, long size);
void ae_ce_map_menu_blocks_forget(void);
/* (ce_map_checks.c's ce_tags_pointer, a read outside a Custom Edition map's
tag cache) whether the size bytes at the Xbox address lie in one block of
the menus' tags of the Custom Edition map loaded, in a map layout's item
list (ae_layout_memory), or in a pack's own tags added to the map
(ae_weapon_pack_memory): 1, read as they are; else 0 */
int ae_ce_map_menu_memory(unsigned long address, long size);

/* (ce_repairs.c's ce_repairs_tags_loaded, AE hook) a Custom Edition map's projectiles faster than 32-bit floats can
carry (1e31 world units a second) scaled down to 1e12 a tick, their ratio kept (ae_glue_ce_maps.c) */
void ae_ce_projectile_speeds(void *tag_instances, long tag_count);

#endif
