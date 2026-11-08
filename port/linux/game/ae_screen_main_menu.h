/*
AE_SCREEN_MAIN_MENU.H

AE's MAIN MENU and its MULTIPLAYER (ae_screen_main_menu.c; notes/plans/2026-10-21-game-setup-spike.md, the 0.3.7
menus): the order MULTIPLAYER (CREATE GAME, JOIN WITH INVITE, SERVER BROWSER, LAN GAMES),
CAMPAIGN, SETTINGS, PROFILES, QUIT, in AE's look. While display.arena_menus is on it takes the game's main menu's place
(ae_hooks.c); an item whose AE screen is not built yet is shown disabled with why, never the game's old screen.
*/

#ifndef __AE_SCREEN_MAIN_MENU_H
#define __AE_SCREEN_MAIN_MENU_H

/* opens AE's main menu over an empty stack, the game's menus closed; 0 when a screen is up or the stack is full */
int ae_screen_main_menu_open(void);
/* closes it (and its MULTIPLAYER) wherever it is on the stack: the game has left its main menu */
void ae_screen_main_menu_close(void);
/* a split-screen player signed in here but not yet in the lobby; cancelling its profile pick signs it out */
short ae_screen_main_menu_signed_in_waiting(void);
/* (split screen) a player 2-4 signed in here with the profile they picked */
int ae_screen_main_menu_signed_in(short local_player);
void ae_screen_main_menu_signed_in_cancel(short local_player);
void ae_screen_main_menu_signed_in_joined(short local_player);

#endif
