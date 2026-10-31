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

/* (debug.network_bots) (network_server_manager.c's network_game_server_start_network_game, before the
settings go out with the start) the lobby's last look: in a team game whose players are odd, one more bot takes the
free slot, so the teams are even */
void ae_lan_bots_start(struct network_game_server *server, struct network_game *game);

/* (debug.network_bots; network_server_manager.c's network_game_server_idle, the game in progress) bots join it up to
bots.count (one more for even teams) as far as it has room, on new virtual machines too (a game started before its
lobby had them all fills up in game), a bot comes back once a person leaves, and bots leave when fewer are wanted:
up to four at a time once the game's players have been the same for two seconds and no machine is still to add its
player, the next four of the same way a tenth of a second on; the machines are told as for a player added or
quitting in game */
void ae_lan_bots_ingame(struct network_game_server *server, struct network_game *game);

/* (debug.network_bots; network_game_server_accepts_late_joins) whether the game in progress has a bot that gives
way: a full game with bots takes a machine joining it */
boolean ae_lan_bots_room_ingame(struct network_game *game);

/* (debug.network_bots; network_game_server_update_ticks, before a player joins the game in progress) a full game's
bot leaves for the person: the larger team's last bot, so the person joins the team it leaves */
void ae_lan_bots_make_room(struct network_game_server *server, struct network_game *game,
	struct network_player const *joining);

/* (debug.network_bots) whether a machine slot is one of the host's virtual machines (its bots past the
host's own controllers, with no connection): network_game_server_add_new_client gives a new connection another */
boolean ae_lan_bots_machine_held(long machine_index);

/* (network_server_manager.c, an AE hook line) the client machines (connected or joined, not the host's own) with no
player in the game yet: a machine that joined adds its player a moment later (the countdown does not wait for it) */
long ae_lan_bots_server_playerless_machines(struct network_game_server *server);

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

/* (the PC lobby's team arrows on a bot) the host puts the LAN bot on this machine and controller on the other team and
holds it there: the lobby no longer deals it (ae_lan_bots_lobby), and the other bots in the lobby hold the teams they are
on now. FALSE when no bot is there or the game has no teams */
boolean ae_lan_bots_flip_team(short machine_index, short controller_index);
/* the bots' chosen teams let go: the lobby deals them again (SHUFFLE TEAMS; bots.teams changed) */
void ae_lan_bots_clear_teams(void);

/* (player_queues_new.c's update_server_next_update) the host's tick's input of the player at this absolute index:
a LAN bot's from the bot brain, in place of the idle input its queue holds */
void ae_lan_bots_action(long absolute_player_index, struct player_action *action);

/* (game_engine.c's multiple_teams_alive and team_has_players) whether the player (a datum index) is a bot that
left the game in progress on this host: it still holds its team, so its leaving never ends the game */
boolean ae_lan_bots_left_player(long player_index);

/* whether the game now has LAN bots (this machine hosts it): Delta Stats records no such game (ae_bots.c) */
boolean ae_lan_bots_in_game(void);

/* bots.c's brain for them (ae_bots_lan.inc, included at bots.c's end): the tick's shared work (the map's places
and walkable surfaces, the skill, halo.log's minute line), then each bot's controls */
void bots_ae_lan_tick(void);
void bots_ae_lan_drive(long bot_index, long player_index, struct player_action *action);

#endif
