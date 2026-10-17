/*
AE_SPLIT_SCREEN.H

Multiplayer > SPLIT SCREEN in the PC menus (queue 117, notes/specs/2026-10-16-split-screen-pc.md): a game on this PC
only, as the Xbox's split screen makes (network_game_is_splitscreen_local: a server that takes no other machine), up
to four people and up to 15 bots. The PC menus' own screens all the way: SPLIT SCREEN hosts as Create Game's LAN
does, but takes no remote machine; the map, then the gametype, then straight to the lobby (no Server Setup: its
name, listing and password are a network game's); the lobby's last four rows are BOTS, BOT SKILL, PLAYFIGHT and
BOT TEAMS, the same settings as Settings > Game Options > BOTS. Another controller's START joins in the lobby, as in a LAN
lobby.
*/

#ifndef AE_SPLIT_SCREEN_H
#define AE_SPLIT_SCREEN_H

struct widget_instance;

/* the lobby's player rows in a split screen game (its last four rows are the bots'); 11 in the others */
#define AE_SPLIT_SCREEN_LOBBY_ROWS 7
/* the PC menus' name of the lobby screen */
#define AE_SPLIT_SCREEN_LOBBY "pc\\main_menu\\multiplayer_type_select\\lobby\\lobby_screen"

/* (menu_functions.c's multiplayer_mode_set) the Multiplayer item chosen, by its instance's name: SPLIT SCREEN's or
another */
void ae_split_screen_chosen(char const *item_name);
/* (ui_widget_event_handler_functions.c's network_game_start_new_server) whether the game being hosted is SPLIT
SCREEN's, which takes no remote machine */
unsigned char ae_split_screen_hosting(void);
/* (menu_functions.c's lobby) the lobby's player rows: AE_SPLIT_SCREEN_LOBBY_ROWS in a split screen game, else rows */
short ae_split_screen_lobby_rows(short rows);
/* (menu_functions.c, "mp profile set for game" in a split screen game) the gametype chosen (chosen: it was), the
game's most players set and the lobby opened in place of the gametype list; TRUE when the lobby opened */
unsigned char ae_split_screen_lobby_open(struct widget_instance *widget, unsigned char chosen,
	unsigned char *widget_deleted);
/* (menu_functions.c's lobby update) the bot rows: shown in a split screen game's lobby of a Slayer gametype (BOT
TEAMS in a team game only), their values written to the settings as they change */
void ae_split_screen_lobby_update(struct widget_instance *list, unsigned char teams, unsigned char slayer);

#endif
