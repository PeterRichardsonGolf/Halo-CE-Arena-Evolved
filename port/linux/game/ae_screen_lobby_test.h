/*
AE_SCREEN_LOBBY_TEST.H

The lobby drive (ae_screen_lobby_test.c): a debug screen that drives AE's
lobby glue end to end for the harness tests (tools/test_ae_lobby.py), and is
AE's pregame screen for those sessions (the post-game take-over shows it).
Removed in M8 with the other test screens.
*/

#ifndef __AE_SCREEN_LOBBY_TEST_H
#define __AE_SCREEN_LOBBY_TEST_H

/* debug.ae_test_screen 90: host LAN, add controller 1's player, wait for 4 players (or 30 s), Blood Gulch slayer,
start, end it 8 s in, back to the lobby 4 s into the post-game; 0 for another value */
int ae_screen_lobby_test_open(int value);
/* once a frame (ae_hooks, in the menus and in the game): the drive's next step */
void ae_screen_lobby_test_tick(unsigned long now_ms);

#endif
