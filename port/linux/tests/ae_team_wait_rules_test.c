/* The wait-for-players rule (port/linux/game/ae_team_wait_rules.c). */

#include <stdio.h>
#include <stdlib.h>

#include "../game/ae_team_wait_rules.h"

static int failures = 0;

static void expect(int got, int want, char const *what)
{
	if (got != want)
	{
		printf("FAIL %s: got %d, want %d\n", what, got, want);
		failures++;
	}
}

int main(void)
{
	/* the upstream rule outside AE's gametypes and outside team games */
	expect(ae_team_wait_rule(0, 1, 1, 0, 30), _ae_team_wait_end, "stock gametype ends");
	expect(ae_team_wait_rule(1, 0, 1, 0, 30), _ae_team_wait_end, "free for all ends");
	/* a team emptied by leaving: wait 60 s */
	expect(ae_team_wait_rule(1, 1, 1, 0, 30), _ae_team_wait_hold, "one team left: waits");
	expect(ae_team_wait_rule(1, 1, 1, 60 * 30 - 1, 30), _ae_team_wait_hold, "waits up to the last tick");
	expect(ae_team_wait_rule(1, 1, 1, 60 * 30, 30), _ae_team_wait_end, "ends after 60 s");
	/* knocked out by lives: two teams still have players, the round ends at once */
	expect(ae_team_wait_rule(1, 1, 2, 0, 30), _ae_team_wait_end, "eliminated team ends the round");
	/* nobody left at all */
	expect(ae_team_wait_rule(1, 1, 0, 0, 30), _ae_team_wait_end, "empty game ends");
	expect(ae_team_wait_rule(1, 1, 1, 0, 0), _ae_team_wait_end, "no tick rate: ends");
	if (failures)
		return EXIT_FAILURE;
	printf("ae_team_wait_rules: ok\n");
	return EXIT_SUCCESS;
}
