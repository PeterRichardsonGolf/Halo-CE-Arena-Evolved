/*
AE_HOOKS.H

What the AE hook lines in upstream files call (ae_hooks.txt lists those lines;
notes/ae-hooks.md says where each goes). Included after cseries.h (boolean).
*/

#ifndef __AE_HOOKS_H
#define __AE_HOOKS_H

/* AE_SAVED_GAMES_MAXIMUM (the multiplayer settings list's size) */
#include "ae_saved_limits.h"

struct halo_ui_pointer;
struct event_record;
struct widget_instance;
union rectangle2d;
union point2d;

/* (map folders, ui_widget.c's spinner_string_list_get_string) the PC map lists' chooser's value's name, a page's, or
NULL for another string list's (ae_map_folders.c) */
wchar_t const *ae_map_pages_spinner_string(long string_list_index, short string_index);
/* display.arena_menus (HALO_ARENA_MENUS), read once */
boolean ae_menus_active(void);
/* TRUE: AE's menus handled this frame (process_ui_widgets returns) */
boolean ae_ui_process(void);
/* draws AE's screens over the widgets of one view (render_ui_widgets) */
void ae_ui_render(short local_player_index, union rectangle2d const *window_bounds);
/* TRUE: AE's menus took the pointer (ui_widgets_process_mouse drops its clicks) */
boolean ae_ui_pointer(struct halo_ui_pointer const *pointer);
/* TRUE while AE's screens are up (the flag, a screen open, a renderer, no server browser): upstream's menus then
keep the pointer and the keys (early check A) */
boolean ae_ui_takes_pointer(void);
/* closes upstream's widgets because an AE screen replaces them (gallery, lobby drives, AE's lobby session) */
void ae_ui_replace_menus(void);
/* (M2, ui_widget.c network_game_reset_to_pregame_ui, preflight P19) TRUE when AE shows its own lobby for its own
session back from a game, instead of upstream's pregame / SELECT MAP screens (ae_glue_lobby.c) */
boolean ae_lobby_take_pregame_ui(void);
/* (M2, network_client_manager.c where an advertisement's network version is stored) a LAN host advertising: its
network version and netcode flags (HALO_PORT_ADVERTISED_*), for AE's join reasons (ae_glue_lobby.c) */
void ae_lobby_advertised(unsigned short version, unsigned char flags);
/* the settings the widgets read, cached; ae_settings_refresh re-reads config (M3 calls it after a change) */
float ae_settings_ui_scale(void);
boolean ae_settings_reduce_motion(void);
float ae_settings_menu_volume(void);
void ae_settings_refresh(void);
/* a validated CE split-pause screen opened, for its regression log */
void ae_ce_map_widget_opened(long tag_index, short local_player_index);
/* a CE split-pause screen's centring in its view: its offset moved by the view's centring less the one the menus
add, its clip (the bounds) opened out by it (any other widget: nothing) */
void ae_ce_map_view_place(long tag_index, union rectangle2d const *window_bounds, union point2d *offset,
	union rectangle2d *bounds);
/* a validated CE pause tag's Halo PC-only function, before the Xbox table */
int ae_ce_map_event_function(struct widget_instance *widget, struct event_record *event,
	word function_index, boolean *widget_deleted, boolean *result);

#endif
