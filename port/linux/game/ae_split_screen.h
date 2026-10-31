/*
AE_SPLIT_SCREEN.H

Multiplayer > SPLIT SCREEN in the PC menus (queue 117, notes/specs/2026-10-16-split-screen-pc.md): a game on this PC
only, as the Xbox's split screen makes (network_game_is_splitscreen_local: a server that takes no other machine), up
to four people and up to 15 bots. The PC menus' own screens all the way: SPLIT SCREEN hosts as Create Game's LAN
does, but takes no remote machine; the map, the gametype, Server Setup with only the gametype's rows (its name,
players, invite, listing, password and map sharing are a network game's: hidden, the gametype's rows moved up to the
top), then the lobby, whose last four rows are BOTS, BOT SKILL, PLAYFIGHT and BOT TEAMS, the same settings as Settings
> Game Options > BOTS. Another controller's START joins in the lobby, as in a LAN lobby.
*/

#ifndef AE_SPLIT_SCREEN_H
#define AE_SPLIT_SCREEN_H

struct widget_instance;

/* the lobby's player rows in a split screen game (its last four rows are the bots'); 11 in the others */
#define AE_SPLIT_SCREEN_LOBBY_ROWS 7

/* (menu_functions.c's multiplayer_mode_set) the Multiplayer item chosen, by its instance's name: SPLIT SCREEN's or
another */
void ae_split_screen_chosen(char const *item_name);
/* (ui_widget_event_handler_functions.c's network_game_start_new_server) whether the game being hosted is SPLIT
SCREEN's, which takes no remote machine */
unsigned char ae_split_screen_hosting(void);
/* (menu_functions.c's lobby) the lobby's player rows: AE_SPLIT_SCREEN_LOBBY_ROWS in a split screen game, else rows */
short ae_split_screen_lobby_rows(short rows);
/* (menu_functions.c's server_settings_update) in a split screen game, Server Setup shows only the gametype's rows, moved
up to the top, and the focus is on a row that shows */
void ae_split_screen_server_settings(struct widget_instance *list);
/* (menu_functions.c, "ss start game", after Server Setup's own) in a split screen game, the game's most players are
the four people and the most bots; TRUE */
unsigned char ae_split_screen_server_start(void);
/* (menu_functions.c's lobby update) the bot rows: shown in a split screen game's lobby of a gametype bots play
(ae_bots_engine_played: Slayer, CTF, Oddball, King, Race, Bomb; BOT TEAMS in a team game only), their values written to
the settings as they change */
void ae_split_screen_lobby_update(struct widget_instance *list, unsigned char teams, long game_engine_index);

/* ---------- the JOIN screen (split screen step 2: spec 4.2, 5, 7): who plays, on which device, with which profile
(port/linux/src/ae_seats.c is the seat map) */

/* (menu_functions.c's pc_menu_function, an AE hook) the JOIN screen's "ae split ..." events; FALSE stops the event's
other handlers (and a back) */
unsigned char ae_split_screen_event(char const *name, struct widget_instance *widget, short controller,
	unsigned char *widget_deleted);
/* (menu_functions.c's data inputs, an AE hook) "ae split join update": every frame of the JOIN screen */
void ae_split_screen_join_update(struct widget_instance *screen);
/* (ae_seats.c) whether the seat map is still wanted: the JOIN screen is up, or a split screen game is */
int ae_split_screen_seats_wanted(void);

#endif
