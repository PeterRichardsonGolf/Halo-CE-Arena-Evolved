/*
AE_HOOKS.H

What the AE hook lines in upstream files call (ae_hooks.txt lists those lines;
notes/ae-hooks.md says where each goes). Included after cseries.h (boolean).
*/

#ifndef __AE_HOOKS_H
#define __AE_HOOKS_H

/* AE_SAVED_GAMES_MAXIMUM (the multiplayer settings list's size) */
#include "ae_saved_limits.h"
/* (0.4.0) the pause menu's hooks: ui_check_for_pause_game's START */
#include "ae_pause.h"

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
/* (0.7.0) the game's main menu not drawn while AE's is about to take its place (ui_widget.c render_ui_widgets) */
boolean ae_hooks_classic_main_menu_hidden(void);
/* (0.4.0, display_error) TRUE: AE shows the game's error as its own error dialog (its menus on, at the main menu or
over an AE screen; port_text the port's own text, else the game's for error_code), and the game's error widget is not
loaded (ae_glue_errors.c) */
boolean ae_errors_take(short error_code, wchar_t const *port_text);
/* (ae_ui_process) an error taken before AE's screens were there opens once they are */
void ae_errors_tick(void);
/* TRUE: AE's menus handled this frame (process_ui_widgets returns) */
boolean ae_ui_process(void);
/* draws AE's screens over the widgets of one view (render_ui_widgets) */
void ae_ui_render(short local_player_index, union rectangle2d const *window_bounds);
/* TRUE: AE's menus took the pointer (ui_widgets_process_mouse drops its clicks) */
boolean ae_ui_pointer(struct halo_ui_pointer const *pointer);
/* TRUE while AE's screens are up (the flag, a screen open, a renderer, no server browser): upstream's menus then
keep the pointer and the keys (early check A) */
boolean ae_ui_takes_pointer(void);
/* TEST RUN: closes AE's screens and reads debug.ae_test_screen again as the main menu next shows (ae_hooks.c) */
void ae_ui_test_screen_rearm(void);
/* TEST RUN: the test screen debug.ae_test_screen asked for is up (1) */
int ae_ui_test_screen_open(void);
/* closes upstream's widgets because an AE screen replaces them (gallery, lobby drives, AE's lobby session) */
void ae_ui_replace_menus(void);
/* the game loaded its main menu (main_screen_shell_load, through ae_community_main_menu_loaded): AE's main menu may
take its place (ae_hooks.c) */
void ae_ui_game_main_menu_loaded(void);
/* a level or game was just started from AE's screens (CAMPAIGN): the main menu stays closed while the game still
shows its main menu for a frame or more, until it leaves it (or a few seconds pass) */
void ae_ui_game_starting(void);
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
/* (0.5.0, network_client_manager.c) a joining client's player additions map sharing held, asked for once the hold
is over (ae_held_players.c): note an ask held or not (TRUE, as add_player returns for a held one), send the held
ones from the client's idle, forget them all at the client's reset */
struct network_game_client;
struct network_game;
boolean ae_held_player_note(short local_player_index, boolean is_held);
/* (ready: the client in the pregame or in a game in progress it has loaded) */
void ae_held_players_send(struct network_game_client *client, boolean ready, long machine_index,
	struct network_game const *game);
void ae_held_players_clear(void);

#endif
