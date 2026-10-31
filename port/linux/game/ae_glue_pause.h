/*
AE_GLUE_PAUSE.H

The AE pause menu's contact with the game (ae_glue_pause.c), for its screen (ae_screen_pause.c): what the game is,
where a player's view is, the game card's facts, and each row's action, which calls the engine function the classic
pause menu's button calls (no new game logic):

- CHANGE TEAMS: team_change_ask (menu_functions.c "port pause change teams");
- END GAME: game_engine_end_game (menu_functions.c "port pause end game", the host's, while the game runs);
- QUIT: network_game_client_local_player_quit, and player_ui_local_player_left_multiplayer_game in split screen
  (ui_widget_event_handler_functions.c "mp game player quit"); a multiplayer map played alone: main_goto_main_menu;
- co-op's QUIT: network_game_client_local_player_quit for every controller ("pause game quit to main menu" in co-op);
- SAVE AND QUIT: game_state_save_to_persistent_storage (a campaign level), main_goto_main_menu (the same function);
- REVERT TO SAVED: main_revert_map ("pause game restart at checkpoint");
- FRIENDLY FIRE: network.coop_own_friendly_fire switched (menu_functions.c "port coop friendly fire");
- a campaign's pause stops the game's time and sound as a pause_game widget does (ui_widget.c: game_time_set_paused,
  sound_pause), and a closing pause lets go of the buttons held as ui_widget_delete does
  (player_control_inhibit_buttons).
*/

#ifndef __AE_GLUE_PAUSE_H
#define __AE_GLUE_PAUSE_H

#include "ae_pause_rows.h"

/* the facts of the game a local player pauses in (game: AE_PAUSE_*, as the pause opened) */
void ae_glue_pause_facts(short local_player, int game, struct ae_pause_facts *facts);
/* the local player's view: its index among the views (left to right, top to bottom) and their count; 0 when the
player has none */
int ae_glue_pause_view(short local_player, short *view, short *views, short *spans_two_columns);
/* whether the local player still plays a game a pause may be over (the game runs, no post-game, not the main menu) */
int ae_glue_pause_still_on(short local_player, int game);
/* CHANGE TEAMS' value: the player's team ("RED", "BLUE"), "" when none */
void ae_glue_pause_team(short local_player, char *text, int size);
/* GAME OPTIONS' value: the running gametype's name (UTF-8, capitals) */
void ae_glue_pause_gametype(char *text, int size);

/* the game card (one player, multiplayer): the running game's facts */
enum { AE_PAUSE_CARD_ROWS = 2 };
struct ae_pause_card
{
	char map[48], title[48], mode[64];
	short rows;                         /* team scores or FFA leaders: 0 when the game has none to show */
	char names[AE_PAUSE_CARD_ROWS][32], scores[AE_PAUSE_CARD_ROWS][16];
	unsigned int colors[AE_PAUSE_CARD_ROWS];
	int mine[AE_PAUSE_CARD_ROWS];       /* the row is the player's own team, or the player */
	char status[32], clock[16];         /* "TIME LEFT" "1:42", "ROUND 4" "1:42", "NO TIME LIMIT" "" */
};
void ae_glue_pause_card(short local_player, struct ae_pause_card *card);

/* a campaign pause: the game's time and sound stopped (TRUE) or going again (FALSE) */
/* an OFFLINE game (upstream's split screen session): the pause stops it */
int ae_glue_pause_offline(void);
void ae_glue_pause_game_time(int paused);
/* a closing pause: the player's held buttons do nothing until let go of */
void ae_glue_pause_closed(short local_player);
/* the rows' actions; 0: refused (logged) */
int ae_glue_pause_change_teams(short local_player);
int ae_glue_pause_end_game(void);
/* (0.7.0) RESTART GAME: END GAME now, the next game started from the lobby once it is back; each frame its steps
(the post-game left at once); the lobby's take (1 once: start the next game) */
int ae_glue_pause_restart_game(void);
void ae_glue_pause_restart_tick(void);
int ae_glue_pause_restart_take(void);
/* (0.7.0) RESTART ROUND: a round game's round again, no one scoring it */
int ae_glue_pause_restart_round(void);
void ae_glue_pause_quit(short local_player, int game);
void ae_glue_pause_revert(void);
int ae_glue_pause_friendly_fire(void);           /* ON now */
void ae_glue_pause_friendly_fire_switch(void);

#endif
