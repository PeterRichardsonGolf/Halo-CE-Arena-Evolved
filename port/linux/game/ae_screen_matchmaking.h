/*
AE_SCREEN_MATCHMAKING.H

Arena Evolved's MATCHMAKING lobby (menus phase 1, R2-04 / R2-05; ae_screen_matchmaking.c): MULTIPLAYER's second row.
One row a playlist (CASUAL / BTB / COMP from matchmaking.txt beside the game; owner 11-11: rows, not a value to step;
the focus starts on the last one joined, LB / RB step it too), the focused playlist's card (its label, description and
the server's players as the server browser last heard them), MY PARTY and LAST GAME on the right. A (or START, or a
click) on a playlist searches: its first
server with room for the party (ae_matchmaking_rules.h), joined as JOIN BY ADDRESS joins; a full or unreachable one is
skipped for the next; none left: "No CASUAL server has room right now." and back. B cancels a search, else goes back.

Its log: "ae matchmaking: open (N playlists, CASUAL)", "playlist BTB", "searching CASUAL", "joining CASUAL: <address>",
"<address>: <why>, next", "none with room (CASUAL)", "joined <address>", "cancelled", "closed".
*/

#ifndef __AE_SCREEN_MATCHMAKING_H
#define __AE_SCREEN_MATCHMAKING_H

int ae_screen_matchmaking_open(const char *crumb);
void ae_screen_matchmaking_close(void);

#endif
