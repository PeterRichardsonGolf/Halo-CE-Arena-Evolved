/*
AE_MENUS_SWAP.H

MENUS on the main menu (item 81; ae_menus_swap.c): the main menu's rows turn,
in place, into the menu styles (PC, XBOX; ARENA EVOLVED in a build with
Arena Evolved's own menus), and picking one switches to it live. Include
after cseries.h.
*/

#ifndef __AE_MENUS_SWAP_H
#define __AE_MENUS_SWAP_H

struct widget_instance;

/* (cache_files.c, after menu_tags_loaded) a map's tags loaded: on ui.map, the
Xbox's menus get MENUS in their main menu's fourth slot (and its rows), the
PC version's lose the ARENA EVOLVED row when there are no AE menus */
void ae_menus_swap_tags_loaded(char const *map_name);
/* (menu_tags_unloaded) the tags made for the Xbox's menus let go, the map's
main menu list as it was */
void ae_menus_swap_tags_unloaded(void);
/* (pc_menu_tag) whether the tag is one of those: its rows run the port's
functions */
boolean ae_menus_swap_tag(long tag_index);
/* (menu_functions.c) the menus' functions "ae menus open" (MENUS: the
styles opened, through its custom activation, or refused quietly away from
the plain main menu; widget_deleted set when the screen went), "ae menus
pick" (a style's row), "ae menus focus" (the styles' list, created: the style
in use focused) and "ae menus item" (MENUS, created: where focus goes after a
switch) */
boolean ae_menus_swap_event(char const *name, struct widget_instance *widget, short controller,
	boolean *widget_deleted);
/* (ui_widget.c process_ui_widgets, once a frame, after the pointer's
targets are let go: ui_widgets_process_mouse) a switch under way: the fade
out, the menus built again in the style picked, the fade in */
void ae_menus_swap_update(void);

#endif
