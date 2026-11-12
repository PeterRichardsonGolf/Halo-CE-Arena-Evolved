/*
AE_TEAM_OVERRIDE.H

(0.7.4) The host puts any player on the other team: in the lobby (a person's row, as it already does a bot's) and in a
game (the pause menu's PLAYERS page). The mechanics are the game's own: the lobby's move is the shuffle's (the host's
server sets the player's team and sends the lobby's data again), a game's is team_change_move (team_change.c, forced
like the dedicated server's sv_team: the unit dies as nobody's kill, the score stays theirs, the player respawns on
the new team; a round game waits for their next death). Players changing themselves keep the host's rule.
*/

#ifndef AE_TEAM_OVERRIDE_H
#define AE_TEAM_OVERRIDE_H

/* the lobby: the person at machine, controller on the other team, sent to everyone; 0 when this is not the host of a
team game's open lobby, or the player is not there. A bot's team is ae_lobby_change_bot_team's */
int ae_team_override_lobby(short machine, short controller);

/* a game: the player at machine, controller to team (0 red, 1 blue) by team_change_move; the result is its
enum delta_team_result (0 changed, 1 at their next death), text what happened, and refused ones (anything else) why; -1 when
the player is not found. A move made: name's chat line from host ("<host> moved <name> to the Blue Team") at once, or when
the death makes it (ae_team_override_tick), and, for a player of this PC, a line on their own screen */
int ae_team_override_game(short machine, short controller, short team, char const *name, char const *host,
	char *text, int size);

/* whether the player at machine, controller is the last on their team while the other team has players (the move the
engine refuses: team_change_last_of_team) */
int ae_team_override_last_of_team(short machine, short controller);

/* one chat line to everyone: "<host> moved <name> to the Blue Team" (nothing without text chat) */
void ae_team_override_say(char const *host_name, char const *name, short team);

/* each frame in the menus' tick (the host's): a move that waited for the player's death, now made, says so */
void ae_team_override_tick(void);

#endif
