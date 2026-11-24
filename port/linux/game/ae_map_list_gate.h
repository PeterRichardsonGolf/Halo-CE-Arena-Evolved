/*
AE_MAP_LIST_GATE.H

Skips a remaking of the menus' map list while what it lists is unchanged (the
rules: ae_map_list_gate_rules.h). Called from the AE hooks of ui_map_list.c
(the making), ui_widget_event_handler_functions.c (a name looked up) and
xbox_files.c (a folder listed), and from custom_edition_maps_look_again.
*/

#ifndef __AE_MAP_LIST_GATE_H
#define __AE_MAP_LIST_GATE_H

/* (ui_map_list_refresh, first) whether the list as made last is still the list: its folders and inputs unchanged after
a Workshop rescan, and the making less than 10 s old */
int ae_map_list_gate_unchanged(void);
/* (ui_map_list_refresh) a full making starts and ends; in between, the folders listed on this thread are noted */
void ae_map_list_gate_begin(void);
void ae_map_list_gate_end(void);
/* (custom_edition_maps_look_again, any thread) the next remaking is a full one, and no name is taken as missing */
void ae_map_list_gate_forget(void);
/* (FindFirstFileA) a folder listed: its host path, and whether it opened */
void ae_map_list_gate_note_directory(const char *host_path, int opened);

/* (ui_map_list_lookup) a name not found since the last full making, while the folders and inputs are unchanged (no
rescan, no age limit); while the gate is not trusted, within 2 s of that making */
int ae_map_list_missing_known(const char *name);
void ae_map_list_missing_add(const char *name);

#endif
