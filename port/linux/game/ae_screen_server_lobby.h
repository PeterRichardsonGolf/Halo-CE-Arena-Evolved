/*
AE_SCREEN_SERVER_LOBBY.H

A dedicated server's lobby page (0.7.7; the approved mockups-077-server/server-lobby option A, slides 01-04 and 13-19):
what an Arena Evolved player sees in the lobby of a server (halo --server) they joined, in place of CREATE GAME's
joiner LOBBY and of upstream's GAME LOBBY. ae_screen_server_lobby.c.

The title is the server's name (its digits and # in the numerals' face: the title face draws a 1 as an I), the join
word and region on the right. On the left the game card (the map's picture or its name plate, the map, the gametype,
its rules, then what happens next: WAITING FOR PLAYERS, VOTING with the seconds left, WON THE VOTE, STARTS IN, GETTING
THE MAP), then the games to come (AFTER THAT, FROM THE PLAYLIST) or the map vote's choices (VOTE FOR THE NEXT GAME: A
votes, one vote for this PC, the card shows the focused choice; the winner framed green once it is over), then YOU:
CHANGE TEAM (left / right, or Y anywhere; THE SERVER BALANCES TEAMS where it does) and, for a player the server gave a
role, RUN THE SERVER (UP NEXT, SAY TO EVERYONE, RESTART). On the right THIS SERVER: the people and bots, the message of
the day, the chat commands it takes, each team's bar, its people, and its bots as one line ("+ 3 BOTS, LEAVING AS
PEOPLE JOIN"). X (or A on a person) opens a player's card; LB / RB move between the columns; B leaves (asked first).
A pad's START joins its player (split screen: CHANGE TEAM names the controller, the vote is THIS PC'S VOTE).

Its log: "ae server lobby: open (<how>)", "state <WAITING|VOTE|RESULT|COUNTDOWN|DOWNLOAD>", "voted for <map>:<gametype>",
"team change asked (controller N)", "team change refused: <why>", "column panel|left", "card of '<name>'", "server page",
"leave", "closed (<why>)".
*/

#ifndef __AE_SCREEN_SERVER_LOBBY_H
#define __AE_SCREEN_SERVER_LOBBY_H

/* opens the page for the dedicated server this machine joined (how: the join's words, logged); 1 when it is up */
int ae_screen_server_lobby_show(const char *how);
/* whether it is on the stack */
int ae_screen_server_lobby_open(void);
/* (ae_lobby_take_pregame_ui) back from a game on a dedicated server: the page pushed; 1 when it is up */
int ae_screen_server_lobby_take_pregame(void);
/* once a frame (ae_hooks.c): the page opened in a dedicated server's lobby whatever screen the join left up (CREATE
GAME's joiner LOBBY, upstream's GAME LOBBY), closed once the machine is no longer in its lobby */
void ae_screen_server_lobby_tick(unsigned long now);

/* (ae_join_helper_rejected) the server refused this machine while the page was up: a kick or a ban (the blacklisted
code) is the page's to tell ("REMOVED FROM THE SERVER" as it closes): 1 when taken, the game's own error box left out */
int ae_screen_server_lobby_rejected(unsigned short code);
/* (network_chat.c's hook, the menus' chat) while the page is up, the chat's lines go in THIS SERVER's room under its
teams, not over the left column: its left, right and bottom (window pixels) and the rows that fit, from line_height
(the lines' height as drawn); 0 (nothing changed) when the page is not up or has no room */
int ae_screen_server_lobby_chat_area(short line_height, short *left, short *right, short *edge, short *rows);
/* (chat.c's hook, the Delta chat's lines in the menus) the same room for its lines' box: its left, right, top and bottom
(window pixels, before the chat's own scale); 0 (nothing changed) when the page is not up */
int ae_screen_server_lobby_chat_box(short line_height, short *left, short *right, short *top, short *bottom);

#endif
