/*
AE_SCREEN_POSTGAME.H

AE's post-game (0.4.0; the approved design with the answers of
2026-10-25: R-team, R-team-counting, R-ffa, R-2p, A-carnage). With AE's menus on, the end of a multiplayer game shows
it in place of the game's carnage report (ae_postgame.c's hook in game_engine_post_rasterize_post_game); classic menus
keep the game's.

Two steps over a dark backdrop (the game draws no world in its post-game):

1. GAME RESULT: the result in the winning team's colour (RED TEAM WINS; FFA the player's place spelled out,
   FIRST PLACE, in their own armour colour; a tie TIED 2ND PLACE), the gametype on the map, the player (emblem, name, team), and the player's rows coming in one by
   one, their numbers counting up (the timings: the first at 600 ms, then every 1100 ms, each number 700 ms):
   RESULT (WIN / LOSS / TIE, FFA the place), KILLS (deaths under it), ASSISTS, BEST SPREE (kills in one life; left out
   when the build does not record it); the FINAL SCORE card on the right (team scores, FFA the leaders; TIME PLAYED).
   Split screen: each player's own result in their own view, no card. A skips the counting, then goes on (anyone's).
2. CARNAGE: the AE scoreboard (ae_scoreboard.c, drawn in AE's layer) under the result banner (RED TEAM WINS 50 - 38)
   and the gametype, map and time played; every player of this machine in the "you" style. Player 1's A (the host's):
   CONTINUE, back to the lobby (the stock A: network_game_server_reset_to_pregame), taken once the game's scores are
   up; a guest waits for its host ("Waiting for the host"). Player 1's B: "Leave the game?" (STAY / LEAVE: the stock
   B, network_game_abort).

Its log: "ae postgame: open (<n> players, <teams|ffa>)", "ae postgame: carnage", "ae postgame: counting skipped",
"ae postgame: continue asked", "ae postgame: leave asked", "ae postgame: closed".
*/

#ifndef __AE_SCREEN_POSTGAME_H
#define __AE_SCREEN_POSTGAME_H

/* once a frame (ae_ui_process, AE's menus on): opens as the post-game is due (ae_glue_postgame_due), closes once it
is not; a host's CONTINUE asked before the scores were up is taken when they are */
void ae_screen_postgame_tick(void);
/* whether it is on AE's stack (the game's carnage report buttons are then its own) */
int ae_screen_postgame_up(void);

#endif
