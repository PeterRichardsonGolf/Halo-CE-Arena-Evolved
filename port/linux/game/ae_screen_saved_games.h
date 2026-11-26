/*
AE_SCREEN_SAVED_GAMES.H

The SAVED GAMES screen (ae_screen_saved_games.c): every profile's saved game of the campaign on this PC, with the mission it is
at, when it was kept and whether it takes the AI squad. A picks one: player 1 plays with that profile from then on, and the
campaign screen's CONTINUE is its saved game. X deletes the saved game under a dialog (the profile's finished missions and best
runs stay). With none left, the screen says so.
*/

#ifndef __AE_SCREEN_SAVED_GAMES_H
#define __AE_SCREEN_SAVED_GAMES_H

/* opens SAVED GAMES over the stack, the active profile's focused; picked, when set, is called after a pick (the opener's
screen reads the campaign again). 0 when the stack is full */
int ae_screen_saved_games_open(void (*picked)(void));
/* how many saved games this PC has (every profile's) */
int ae_screen_saved_games_count(void);
/* (DEV MODE `menu saved [empty] [delete]`) opened over the stack; empty: as when none is left; delete: the delete dialog open */
int ae_screen_saved_games_open_staged(int empty, int delete_dialog);

#endif
