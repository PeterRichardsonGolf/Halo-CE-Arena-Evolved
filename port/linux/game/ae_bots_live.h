/*
AE_BOTS_LIVE.H

bots.count changed during a local (split screen) game takes effect at once:
bots join the game in progress or leave it (ae_bots_live.c; the rules:
ae_bots_live_rules.c). Included by ae_bots.h.

bots.teams changed during a team game moves the bots in it to the teams it
asks for (ae_bots_live_teams_step), on the host of a local game and of a LAN
or internet game (ae_lan_bots.c's ae_lan_bots_ingame).
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

/* the most bots ae_bots_live_teams_step counts (the local game's BOTS_MAXIMUM, ae_lan_bots.c's AE_NET_BOTS_MAXIMUM) */
#define AE_BOTS_LIVE_TEAMS_BOTS 128

/* one host's bots as bots.teams last saw them in the game in progress (ae_bots_live_teams_reset between games) */
struct ae_bots_live_teams
{
	/* bots.teams as the game last saw it, NONE before its first look */
	short told;
	/* the setting changed: the bots move until their teams are as it asks */
	boolean moving;
	/* the last move's tick (NONE: none yet) */
	long last_move;
	/* each bot's move under way (made at its death, or at its next respawn in a round game): the team it goes to
	(NONE for none) and the tick it was asked */
	signed char pending[AE_BOTS_LIVE_TEAMS_BOTS];
	long pending_tick[AE_BOTS_LIVE_TEAMS_BOTS];
};

void ae_bots_live_teams_reset(struct ae_bots_live_teams *teams);

/* (the host, each frame of a game in progress while its bots are not joining or leaving) bots.teams looked at: when it
changed, the bots move to the teams it asks for, one at a time (AE_BOTS_LIVE_TEAM_MOVE_TICKS apart), as an admin's move
(team_change_move: the unit dies as nobody's kill, nothing counted, and respawns on the new team); a person never moves.
bot_slot[0 .. count - 1]: each bot's slot in the game's player list (NONE: not in the game); people_team: the first
person's team (NONE: none). The fireteams are split again once a bot's move is made. TRUE when bots.teams changed this
frame */
boolean ae_bots_live_teams_step(struct ae_bots_live_teams *teams, struct network_game *game, long count,
	long const *bot_slot, long people_team);

/* (bots.c's bots_update_actions, each bot player) TRUE for a bot that has left the game in progress (its body
goes at its quit time): its controls this tick are let go, and bots.c leaves it be (a bot of its number that joins
again is another player) */
boolean ae_bots_live_quitting(struct player_datum const *player, long absolute_index, struct player_action *actions);

#endif
