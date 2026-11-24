/*
AE_SCREEN_COOP_LOBBY.H

The CO-OP LOBBY (0.9.4, ae_screen_coop_lobby.c): a campaign level hosted for others, from AE's CAMPAIGN (X on a level), and the
lobby a player sees on joining such a game (the SERVER BROWSER, JOIN WITH INVITE, JOIN BY ADDRESS). The host's has
the level, DIFFICULTY (left / right), PUBLIC and INVITE (online) and START GAME; a joiner's reads the same, waiting for the host.
The engine's own co-op game is what is hosted (ae_lobby_set_coop_level: the Map screen's campaign levels, no game engine in
the gametype), up to 16 players; this PC's seated players of CAMPAIGN's SPLIT SCREEN play in it too, and a pad's START adds more
here (up to four on one PC).

Its log: "ae coop lobby: host level <n> (<network>)", "hosted, player 1 in", "open failed: <why>", "joined", "start",
"cancelled", "leave", "left".
*/

#ifndef __AE_SCREEN_COOP_LOBBY_H
#define __AE_SCREEN_COOP_LOBBY_H

struct ae_flow_screen;

/* hosts the level (0-9) at the difficulty (0-3) as a co-op game on the network the settings allow (online, following
network.coop_public, when internet play is on, else the unlisted LAN), this PC's pads (the mask of controllers 1-3) joining it once the lobby is live, and opens
the lobby over the screen. Returns 0 when it opens, else an AE_COOP_HOST_ERR_* (ae_coop_rules.h) for the caller to show */
int ae_screen_coop_lobby_host(short level, short difficulty, unsigned pads, struct ae_flow_screen const *flow);
/* the LOBBY of a co-op game this machine has joined, player 1 in (the browser, an invite, an address); how: its crumb. 0 when
the stack is full */
int ae_screen_coop_lobby_joined(const char *how);
/* (DEV MODE `menu coop [players <1-4>]`) the first level hosted on this PC as a LAN game on NORMAL, that many players of this PC
in it (guests on controllers 2 up); 1 when the lobby opened */
int ae_screen_coop_lobby_open_staged(int players);

#endif
