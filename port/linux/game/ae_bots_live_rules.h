/*
AE_BOTS_LIVE_RULES.H

The bots of a local game in progress (ae_bots_live.c) as pure rules: no
engine or config calls (port/linux/tests/ae_bots_live_rules_test.c runs
them).

bots.count changed while a local (split screen) game is played takes effect
at once, one bot a frame, as the lobby has it (network_server_manager.c's
network_game_server_port_bots):

- Too many bots: one leaves. In a team game with bots.teams "even", the last
  bot of the larger team (the teams stay as even as they were), else the
  last bot.
- Too few: the first bot not in the game joins, while the game has room; in
  a team game on the team bots.teams asks for: "against", the team the
  people are not on; "even", the smaller team (red when even).
*/

#ifndef __AE_BOTS_LIVE_RULES_H
#define __AE_BOTS_LIVE_RULES_H

enum
{
	AE_BOTS_LIVE_NONE = 0,
	AE_BOTS_LIVE_ADD,
	AE_BOTS_LIVE_REMOVE,
};

/* a frame's change: none, bot number `bot` (0 up) joins on `team`, or leaves */
struct ae_bots_live_change
{
	int kind;
	int bot;
	int team;
};

/* the change for this frame. bot_team[0 .. maximum - 1]: each bot's team in the game (0 red, 1 blue; any value from
0 in a game without teams), -1 for a bot not in it. wanted: bots.count's (0 for a gametype bots do not play). teams:
a team game; against: bots.teams "against"; people_team: the first person's team (-1 for none: no bots then);
red_count, blue_count: the game's players on each team, bots too; room: a player can be added */
struct ae_bots_live_change ae_bots_live_decide(int wanted, int maximum, int const *bot_team, int teams, int against,
	int people_team, int red_count, int blue_count, int room);

/* HALO_BOTS_LIVE_TEST's text ("<second>:<count>,...", a test's bots.count changes in a game): step number `step`
(0 up) into *second and *count; 0 when the text has no such step or it is not of that form */
int ae_bots_live_test_step(char const *text, int step, double *second, int *count);

#endif
