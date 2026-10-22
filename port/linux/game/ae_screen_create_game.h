/*
AE_SCREEN_CREATE_GAME.H

The game setup spike's CREATE GAME and LOBBY screens (ae_screen_create_game.c; notes/plans/2026-10-21-game-setup-spike.md,
steps 3 and 4): a game set up from a gametype's copy (GAME OPTIONS), a map (CHANGE MAP) and the network, hosted
through AE's lobby glue, its lobby with the quick options, started, and back to the lobby after it. Opened from the
game's main menu (ae_screen_create_game_entry: F3 or a pad's BACK) or debug.ae_test_screen 32.
*/

#ifndef __AE_SCREEN_CREATE_GAME_H
#define __AE_SCREEN_CREATE_GAME_H

/* opens CREATE GAME over the stack on the last used gametype (or $HALO_AE_TEST_GAMETYPE's), the game's menus closed
(they come back when it closes); 0 when there is no gametype or the stack is full */
int ae_screen_create_game_open(void);
/* opened from AE's MULTIPLAYER (the copy open stays, else the last used gametype's); join: its JOIN WITH INVITE at
once. B goes back to MULTIPLAYER */
int ae_screen_create_game_from_menu(int join);
/* (ae_hooks.c, each frame AE's menus are on) on the game's main menu with no AE screen up: F3 (pressed: keys are
AE_KEY_* held now) or a pad's BACK opens CREATE GAME */
void ae_screen_create_game_entry(int keys);

#endif
