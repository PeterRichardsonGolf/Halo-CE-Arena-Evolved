/*
AE_BOTS_LIVE.H

bots.count changed during a local (split screen) game takes effect at once:
bots join the game in progress or leave it (ae_bots_live.c; the rules:
ae_bots_live_rules.c). Included by ae_bots.h.
*/

#ifndef __AE_BOTS_LIVE_H
#define __AE_BOTS_LIVE_H

struct network_game_server;
struct network_game;
struct player_datum;
struct player_action;

/* (network_server_manager.c's network_game_server_port_bots, every frame of the host's server, before its lobby
work) in a local game being played (local_ingame), a bot joins or leaves when bots.count asks for another number:
one a frame, as the lobby has them; nothing in a LAN or internet game, the lobby or the postgame */
void ae_bots_live(struct network_game_server *server, struct network_game *game, boolean local_ingame);

/* (bots.c's bots_update_actions, each bot player) TRUE for a bot that has left the game in progress (its body
goes at its quit time): its controls this tick are let go, and bots.c leaves it be (a bot of its number that joins
again is another player) */
boolean ae_bots_live_quitting(struct player_datum const *player, long absolute_index, struct player_action *actions);

#endif
