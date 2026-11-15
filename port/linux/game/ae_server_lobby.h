#ifndef AE_SERVER_LOBBY_H
#define AE_SERVER_LOBBY_H

/*
AE_SERVER_LOBBY.H

A dedicated server's lobby page (0.7.7; the approved mockups-077-server/server-lobby option A), the engine side. The
rules and the wire: ae_server_lobby_rules.h; the page: ae_screen_server_lobby.c.

The server's side (halo --server): while its lobby is up (the pregame), it sends each Arena Evolved machine the public
"lobby info" message (distributed message 115) when what it says changes, and again to a machine new to the lobby: the
region, the join word, the message of the day, the people it waits for, the bots on each team, the lobby's game, the
games to come that everyone present can play and the map vote's choices (each with its gametype's name and rules), the
team rule and the chat commands it takes. Its lobby's team rule is the server's network.team_change, applied to the
pregame's team requests too (ae_server_lobby_team_refused).

The client's side: the message kept while this machine is a client of a dedicated server; the page reads it, with what
the game already knows (the server's name, map and gametype, its players, the countdown, the vote, the download).
*/

#include "ae_server_lobby_rules.h"

struct network_game;

/* ---------- the server's (each frame, from server_director.c's update; nothing outside a dedicated server) */
void ae_server_lobby_host_update(void);
/* (network_server_message_handler.c's pregame player settings, an AE hook) a client's own player asks for wanted_team in
the lobby: 1 if the server's team rule refuses it (the player stays on the team they are on) */
int ae_server_lobby_team_refused(struct network_game const *game, long player_list_index, short wanted_team);

/* ---------- the client's */

/* whether this machine is a client of a dedicated server (the game's dedicated bit, as the host sets it) */
int ae_server_lobby_dedicated(void);
/* the server's last lobby info, NULL without one (or off a dedicated server) */
struct ae_sl_wire const *ae_server_lobby_info(void);
/* each frame (ae_hooks.c): the info forgotten once this machine is no client */
void ae_server_lobby_client_tick(unsigned long now);
/* the game's map being downloaded now, as map_share.c's line has it (ae_map_share_all_render feeds it): 1 and its
level, bytes had and in all (total 0 while it asks the host), else 0 */
int ae_server_lobby_download(char *level, int size, unsigned long *done, unsigned long *total);
void ae_server_lobby_download_seen(char const *level, int asking, int transferring, unsigned long done,
	unsigned long total);

/* the netcode's (network_distributed.c's hook): whether a message is the lobby info, and its handling */
int ae_server_lobby_handles_message(unsigned short const *message, unsigned short size);
void ae_server_lobby_handle_message(long machine_index, unsigned short const *message, unsigned short size);

#endif
