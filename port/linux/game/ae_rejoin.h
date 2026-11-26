/*
AE_REJOIN.H

REJOIN AFTER A DROP (rc6, network.rejoin_after_drop; the rules: ae_rejoin_rules.h).

The client: every join this machine starts names its target (ae_rejoin_note_join, from ae_glue_lobby.c's joins); once
this machine is a guest in a multiplayer game the join is armed (a join with no named target, as a LAN game's, takes
its host's address from the connection). A lost connection with no reason from the host (ae_errors_take asks
ae_rejoin_lost) is not shown: once the menus are up the same join is made again, at most AE_REJOIN_TRIES times within
AE_REJOIN_WINDOW_MS, with AE's dialog saying so (its one choice stops it); back in the game, one chat notice. When every
try failed or the player stopped it, the lost-connection error shows as it did before.

The host (a dedicated server or a player's): a join by the same machine as a machine of the game silent for
AE_REJOIN_STALE_MS or more replaces it (ae_rejoin_server_replace, from the join request's handler), so the rejoin table
gives the new one the old one's team and statistics.

A test drop (debug.network_test_drop, "<seconds>:reset|silent"): seconds after this machine's game began, its client
closes its connection (reset: the host sees the stream close) or lets it go while its stream socket stays open
(silent: the host hears nothing, as a machine whose network went away); once for each value of the setting.

Every step is logged ("rejoin: ...").
*/

#ifndef AE_REJOIN_H
#define AE_REJOIN_H

#include <stddef.h>

struct network_connection;
struct network_game_server;
struct network_game_server_client_machine;

enum
{
	AE_REJOIN_TARGET_NONE,
	/* a host's name or address and port (JOIN BY ADDRESS, the server browser, matchmaking) */
	AE_REJOIN_TARGET_HOST,
	/* an invite's text */
	AE_REJOIN_TARGET_INVITE,
	/* a join with no target of its own (a LAN game): the host's address from the connection, once in the game */
	AE_REJOIN_TARGET_CONNECTION,
};

/* (ae_glue_lobby.c) a join started: its target (host and port, or the invite's text) */
void ae_rejoin_note_join(int kind, char const *host, unsigned short port, char const *invite);
/* (ae_glue_errors.c) the join is armed: a lost connection now may be rejoined */
int ae_rejoin_armed(void);
/* (ae_glue_errors.c) a lost connection (lost: the error is a lost connection or the host gone) whose host gave a reason
or not: 1 when it is rejoined (the error held), 0 to show it */
int ae_rejoin_lost(int lost, int host_gave_reason);
/* (ae_glue_errors.c) a rejoin goes on: the network errors of its own tries are held */
int ae_rejoin_running(void);
/* (ae_glue_lobby.c ae_lobby_update) once a frame, in the menus and in the game */
void ae_rejoin_tick(unsigned long now);

/* (network_server_message_handler.c, a join request) the joining machine's name (cleaned, length characters): 1 when
it replaced a machine of the game */
int ae_rejoin_server_replace(struct network_game_server *server, struct network_game_server_client_machine *joining,
	wchar_t const *name, int length);

/* (network_client_manager.c, the client in the game, each frame) the test drop when its time comes */
void ae_rejoin_test_drop(struct network_connection *connection);
/* (transport_endpoint_winsock.c disconnect_endpoint) a socket about to be closed: a silent test drop keeps its stream
open */
void ae_rejoin_test_socket_closing(long socket);

#endif
