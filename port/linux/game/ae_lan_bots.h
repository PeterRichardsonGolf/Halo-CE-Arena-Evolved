/*
AE_LAN_BOTS.H

Bots in LAN games ("virtual pads"; the rules: ae_lan_bots_rules.h). What the
AE hook lines in upstream files call (port/linux/game/ae_hooks.txt lists
them). Included after cseries.h (boolean).

A LAN game's bots are network players of the HOST's machine, on controller
slots its people leave free, as a second pad's player would be: every other
machine sees them as the host's players, so no message of the network
protocol changes, and a stock OpenCE or ChupathingyCE machine that joins sees
them too. On the host they are not local players: no view, no HUD, no sound
listener, no player_control. Their input is the bot brain's (bots.c, OpenCE
PR #367), written into the host's tick where a remote player's input would be
(update_server_next_update), so the host's simulation and the input it relays
to its clients are the same; the host deals their damage as its own.
*/

#ifndef __AE_LAN_BOTS_H
#define __AE_LAN_BOTS_H

struct network_game_server;
struct network_game;
struct network_player;
struct player_action;

/* (network_server_manager.c's network_game_server_idle, beside the local games' bots) the LAN lobby's bots joined,
moved and let go as the settings and the lobby ask (setting_up: pregame, the start not sent); TRUE when the game's
players changed (the host then sends its clients the lobby) */
boolean ae_lan_bots_lobby(struct network_game_server *server, struct network_game *game, boolean setting_up);

/* SPIKE (debug.network_bots): (network_server_manager.c's network_game_server_start_network_game, before the
settings go out with the start) the lobby's last look: in a team game whose players are odd, one more bot takes the
free slot, so the teams are even */
void ae_lan_bots_start(struct network_game_server *server, struct network_game *game);

/* SPIKE (debug.network_bots): whether a machine slot is one of the host's virtual machines (its bots past the
host's own controllers, with no connection): network_game_server_add_new_client gives a new connection another */
boolean ae_lan_bots_machine_held(long machine_index);

/* (network_server_manager.c, an AE hook line) whether a machine slot is free for a virtual machine: no connection
holds it and the game has no machine there */
boolean ae_lan_bots_server_slot_free(struct network_game_server *server, long machine_index);

/* (network_server_manager.c's network_game_server_add_player_to_game, once the request is known to be from the
player's own machine) while the game is set up, a bot on the controller slot a person of the host's machine joins on
gives way; under way, the join is refused as before (the slot is taken) */
void ae_lan_bots_give_way(struct network_game *game, struct network_player const *joining, boolean setting_up);

/* whether the network player is a LAN bot of this machine (the host's): it is no local player here
(network_game_spawn_player, the client's start, the PC lobby's local players) */
boolean ae_lan_bots_network_player(struct network_player const *player);

/* (player_queues_new.c's update_server_next_update) the host's tick's input of the player at this absolute index:
a LAN bot's from the bot brain, in place of the idle input its queue holds */
void ae_lan_bots_action(long absolute_player_index, struct player_action *action);

/* whether the game now has LAN bots (this machine hosts it): Delta Stats records no such game (ae_bots.c) */
boolean ae_lan_bots_in_game(void);

/* bots.c's brain for them (ae_bots_lan.inc, included at bots.c's end): the tick's shared work (the map's places
and walkable surfaces, the skill, halo.log's minute line), then each bot's controls */
void bots_ae_lan_tick(void);
void bots_ae_lan_drive(long bot_index, long player_index, struct player_action *action);

#endif
