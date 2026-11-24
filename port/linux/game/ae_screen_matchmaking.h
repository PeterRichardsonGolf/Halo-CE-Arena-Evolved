/*
AE_SCREEN_MATCHMAKING.H

Arena Evolved's MATCHMAKING lobby (menus phase 1, R2-04 / R2-05; ae_screen_matchmaking.c): MULTIPLAYER's second row.
One row a playlist (CASUAL / BTB / COMP from matchmaking.txt beside the game; owner 11-11: rows, not a value to step;
the focus starts on the last one joined, LB / RB step it too), the focused playlist's card (its label, description and
the server's players as the server browser last heard them), MY PARTY and LAST GAME on the right. A (or START, or a
click) on a playlist searches: its first
server with room for the party (ae_matchmaking_rules.h), joined as JOIN BY ADDRESS joins; a full or unreachable one is
skipped for the next; none left: "No CASUAL server has room right now." and back. B cancels a search, else goes back.

(0.9.3) AE TOURNAMENT (owner 2026-11-15; its own main menu row): the same screen with six rows (the offline playlists of social.txt),
each with a RANK badge and its BOT SKILL as the value (AUTO or a fixed skill: Left / Right, kept in social.skills); A opens the
TOURNAMENT LOBBY (ae_screen_social_lobby.h), Y the CAREER (ae_screen_career.h). MATCHMAKING itself is online only again. The card: the shape, BOT SKILL, the maps this PC has, the rank and EXP.

Its log: "ae matchmaking: open (N playlists, CASUAL)", "playlist BTB", "searching CASUAL", "joining CASUAL: <address>",
"<address>: <why>, next", "none with room (CASUAL)", "joined <address>", "cancelled", "closed".
*/

#ifndef __AE_SCREEN_MATCHMAKING_H
#define __AE_SCREEN_MATCHMAKING_H

int ae_screen_matchmaking_open(const char *crumb);
/* (0.9.3) AE TOURNAMENT: the offline playlists (social.txt), a playlist focused (NULL: the one used last); crumb NULL: none.
(menus.matchmaking_tab is read by nothing now) */
int ae_screen_tournament_open(const char *crumb, char const *playlist_id);
/* the same on a filter ("casual" or "comp") when no playlist is named (DEV MODE's `menu tournament filter <f>`) */
int ae_screen_tournament_open_filter(const char *crumb, char const *playlist_id, char const *filter);
/* (rc5, DEV MODE `menu tournament ... party`) the PARTY manager over the open AE TOURNAMENT, as X opens it */
int ae_screen_tournament_party_open(void);
void ae_screen_matchmaking_close(void);

#endif
