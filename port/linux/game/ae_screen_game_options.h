/*
AE_SCREEN_GAME_OPTIONS.H

The game setup spike's GAME OPTIONS screen (ae_screen_game_options.c; notes/plans/2026-10-21-game-setup-spike.md): a
gametype's rules on one page in groups, edited as a copy (ae_glue_setup_options.h). A debug screen for now:
debug.ae_test_screen 30.
*/

#ifndef __AE_SCREEN_GAME_OPTIONS_H
#define __AE_SCREEN_GAME_OPTIONS_H

/* begins an edit of the gametype whose stored name is gametype (NULL or not found: the last used one) and opens the
screen over the stack; closes upstream's menus. 0 when there is no gametype or the stack is full */
int ae_screen_game_options_open(char const *gametype);
/* opens the screen over the stack on the edit open now (CREATE GAME's GAME OPTIONS); 0 when there is none or the
stack is full */
int ae_screen_game_options_show(void);

#endif
