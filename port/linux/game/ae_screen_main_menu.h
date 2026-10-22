/*
AE_SCREEN_MAIN_MENU.H

AE's MAIN MENU and its MULTIPLAYER (ae_screen_main_menu.c; notes/plans/2026-10-21-game-setup-spike.md, the 0.3.7
menus): Halo 2's and Gravemind's order, MULTIPLAYER (CREATE GAME, JOIN WITH INVITE, SERVER BROWSER, LAN GAMES),
CAMPAIGN, SETTINGS, PROFILES, QUIT, in AE's look. While display.arena_menus is on it takes the game's main menu's place
(ae_hooks.c); an item whose AE screen is not built yet is shown disabled with why, never the game's old screen.
*/

#ifndef __AE_SCREEN_MAIN_MENU_H
#define __AE_SCREEN_MAIN_MENU_H

/* opens AE's main menu over an empty stack, the game's menus closed; 0 when a screen is up or the stack is full */
int ae_screen_main_menu_open(void);

#endif
