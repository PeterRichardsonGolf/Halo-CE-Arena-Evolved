/*
AE_SCREEN_CREATE_GAME.H

CREATE GAME, one screen with its lobby (ae_screen_create_game.c; 0.3.9, mockups-038/one-lobby): opening it hosts at
once (the lobby live, invite only), the game set up on the left from a gametype's copy (GAME OPTIONS), a map (CHANGE
MAP) and the quick options, the lobby on the right (LOBBY SETUP: network, PUBLIC, MAX PLAYERS; the invite; the roster;
the bots), started, and back to it after the game. Opened from AE's MULTIPLAYER (CREATE GAME) or debug.ae_test_screen
32; a joiner's LOBBY is the same screen, read only.
*/

#ifndef __AE_SCREEN_CREATE_GAME_H
#define __AE_SCREEN_CREATE_GAME_H

/* opens CREATE GAME over the stack on the last used gametype (or $HALO_AE_TEST_GAMETYPE's) and hosts its lobby, the
game's menus closed (they come back when it closes); 0 when there is no gametype or the stack is full */
int ae_screen_create_game_open(void);
/* opened from AE's MULTIPLAYER (the copy open stays, else the last used gametype's), its lobby hosted at once. B
closes the lobby and goes back to MULTIPLAYER */
int ae_screen_create_game_from_menu(void);
/* the LOBBY of a game this machine has joined, player 1 in (SERVER BROWSER / LAN GAMES, ae_screen_browser.c; JOIN
WITH INVITE, ae_screen_join_invite.c); how:
its crumb ("JOINED ON YOUR NETWORK"). Leaving it goes back to the screen under it; 0 when the stack is full */
int ae_screen_create_game_lobby_joined(const char *how);

#endif
