#ifndef AE_TEAM_WAIT_RULES_H
#define AE_TEAM_WAIT_RULES_H

/*
AE_TEAM_WAIT_RULES.H

The rule behind ae_team_wait.c, kept apart so a unit test can drive it (port/linux/tests/ae_team_wait_rules_test.c).

A team game ends when fewer than two teams have a player who can still play (game_engine.c's multiple_teams_alive).
In AE's gametypes, when that happens because players LEFT (fewer than two teams still have a player in the game at
all), the game waits for players instead: it goes on for AE_TEAM_WAIT_SECONDS, and ends only if nobody is back on a
second team by then. A team knocked out by its lives (round games) still ends the round at once, and every other
gametype keeps the upstream rule.
*/

#define AE_TEAM_WAIT_SECONDS 60

enum ae_team_wait_verdict
{
	_ae_team_wait_end = 0,  /* end the game now, as upstream does */
	_ae_team_wait_hold,     /* keep the game going: waiting for players */
};

/* ae_rules: the game's gametype is one of AE's; teams: a team game; teams_present: teams with a player still in the
game (alive, dead or waiting to respawn); held_ticks: how long the game has waited so far; ticks_per_second: the
game's tick rate */
enum ae_team_wait_verdict ae_team_wait_rule(
	int ae_rules,
	int teams,
	int teams_present,
	long held_ticks,
	long ticks_per_second);

#endif
