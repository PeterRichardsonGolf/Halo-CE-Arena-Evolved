/*
AE_SCREEN_SOCIAL_LOBBY.H

The TOURNAMENT LOBBY (0.9.3, ae_screen_social_lobby.c): the offline tournament's "finding a game" (bots on this PC). Opened from AE TOURNAMENT
(A on a playlist) or DEV MODE's `social start`. SEARCHING (the range strip widens, the player's strength in it),
MATCH FOUND (the bots join one at a time, nearest tier first; the map's card shows), STARTING IN 5 (the teams are set); A starts
the game now, B leaves (nothing is recorded). The clock and the draws are the pure rules (ae_social_lobby_rules.h,
ae_social_match_rules.h); the game itself is ae_glue_social_game.h's.

Log: "ae social lobby: open <id> (seed n, map m)", "stage FOUND", "stage STARTING", "go", "left".
*/

#ifndef __AE_SCREEN_SOCIAL_LOBBY_H
#define __AE_SCREEN_SOCIAL_LOBBY_H

/* the lobby of a playlist: seed 0 from the clock; map NULL: the playlist's pick; now: the game loads at once (DEV MODE). 1 when open */
int ae_screen_social_lobby_open(char const *playlist, unsigned long seed, char const *map, int now);
void ae_screen_social_lobby_close(void);
/* (rc5) the lobby after a game: the REMATCH (the NEXT GAME stage), the finished game's roster kept (leavers and fill-ins by
ae_rematch_rules.h), its LAST GAME card, the map vote at once with SAME AGAIN first, the countdown on. 0 when no social game was
played (the caller opens the search lobby instead) */
int ae_screen_social_lobby_open_rematch(void);
/* DEV MODE: the next lobby that opens runs this many seconds and is held there (its clock stops, no game loads); -1: not held */
/* the hold that runs the lobby to its STARTING stage (DEV MODE "hold starting") */
#define AE_SOCIAL_LOBBY_DEV_HOLD_STARTING (-2)
void ae_screen_social_lobby_dev_hold(int seconds);
/* the next staged post-game lobby's LAST GAME card: 1 win, 2 tie, 3 loss, 4 quit (0: the default made-up win) */
void ae_screen_social_lobby_dev_result(int kind);

#endif
