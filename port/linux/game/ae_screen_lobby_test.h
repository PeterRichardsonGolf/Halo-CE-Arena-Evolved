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
start, end it 8 s in, back to the lobby 4 s into the post-game; 92: the same for 2 players, then the roster logged as
it changes; 91: join the first LAN game, wait for 2 players, leave once back from the host's game; 93: host LOCAL;
95: host ONLINE (both: the roster logged as it changes); 94: the profile drive (a profile made, player 1's, its
controls and colour saved and read back from its file; player 2 a guest, nothing saved); 0 for another value */
int ae_screen_lobby_test_open(int value);
/* once a frame (ae_hooks, in the menus and in the game): the drive's next step */
void ae_screen_lobby_test_tick(unsigned long now_ms);

#endif
