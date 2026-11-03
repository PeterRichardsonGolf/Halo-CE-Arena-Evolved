/*
AE_SCREEN_PAUSE.H

The AE pause menu (0.4.0; ae_screen_pause.c; the approved design: B-1p, A-2p, A-4p). With AE's menus on, a player's START in game opens it (ae_glue_pause.c's hook in ui_check_for_pause_game)
in place of the classic pause widget.

One screen holds every local player's pause: each split-screen player pauses alone (their own panel, in their own
view; the others play on: their START opens their own), and a network game keeps running; a campaign's pause stops the
game (as the classic's does). One player alone: the list on the left (the view panel: the player's stripe, PLAYER n,
PAUSED) and the game card on the right (map, gametype, mode line, team scores or FFA leaders, round or time left);
split screen: the panel centred in the player's own view, no card. Prompts: A Select, B Resume.

Its log: "ae pause: open player N (<game>)", "ae pause: player N <ROW>", "ae pause: player N resumes",
"ae pause: player N scoreboard", "ae pause: player N bots page", "ae pause: closed (the game ended)".
*/

#ifndef __AE_SCREEN_PAUSE_H
#define __AE_SCREEN_PAUSE_H

/* opens the local player's pause over the game (game: ae_pause_rows.h's AE_PAUSE_*); 0 when another AE screen is
up or the stack is full */
int ae_screen_pause_open(short local_player, int game);
/* whether the local player's pause is open (its panel, its scoreboard, or a screen it opened over it) */
int ae_screen_pause_holds(short local_player);
/* whether the local player's pause shows the in-game scoreboard (SCOREBOARD) */
int ae_screen_pause_scoreboard(short local_player);
/* once a frame (ae_ui_process, AE's menus on): the pauses of a game that ended or a player who left close, with
whatever they opened; a map picked for the next game asks before it ends this one */
void ae_screen_pause_tick(void);
/* whether the pointer stays the game's: the pause is the top screen and player 1 (the keyboard's and the mouse's)
has none open */
int ae_screen_pause_leaves_pointer(void);
/* (ae_ui_render, before AE's screens draw) a full screen the pause opened over the game (SETTINGS, GAME OPTIONS, CHANGE
MAP) draws over a backdrop, not over the game and its HUD */
void ae_screen_pause_backdrop(void);
/* (split screen) the view (layout units: x, y, width, height) a screen over the pause draws in: the player's who
opened it (ae_draw_inset; before_draw, the input and the pointer); 0 for the pause itself, or with one view. The
other players' pauses stay drawn (ae_screen_pause_backdrop) and theirs (ae_ui_set_unowned) */
int ae_screen_pause_inset(void const *screen_data, float rect[4]);

#endif
