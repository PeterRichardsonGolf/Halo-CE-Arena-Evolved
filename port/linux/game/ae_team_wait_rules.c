/*
AE_TEAM_WAIT_RULES.C

The wait-for-players rule (ae_team_wait_rules.h).
*/

#include "ae_team_wait_rules.h"

enum ae_team_wait_verdict ae_team_wait_rule(
	int ae_rules,
	int teams,
	int teams_present,
	long held_ticks,
	long ticks_per_second)
{
	/* not AE's gametype, or not a team game: the upstream rule */
	if (!ae_rules || !teams)
		return _ae_team_wait_end;
	/* two teams still have players: a team was knocked out by its lives, not emptied by leaving */
	if (teams_present >= 2)
		return _ae_team_wait_end;
	/* nobody at all is left: nothing to wait for */
	if (teams_present <= 0)
		return _ae_team_wait_end;
	if (ticks_per_second <= 0)
		return _ae_team_wait_end;
	if (held_ticks >= (long)AE_TEAM_WAIT_SECONDS * ticks_per_second)
		return _ae_team_wait_end;
	return _ae_team_wait_hold;
}
