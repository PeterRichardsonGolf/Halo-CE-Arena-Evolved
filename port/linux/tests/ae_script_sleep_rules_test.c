/* Which scenario scripts the host stopped (port/linux/game/ae_script_sleep_rules.c). */

#include <stdio.h>
#include <string.h>

#include "../game/ae_script_sleep_rules.h"

static int failures = 0;

static void expect(long got, long want, char const *what)
{
	if (got != want)
	{
		printf("FAIL %s: got %ld, want %ld\n", what, got, want);
		failures++;
	}
}

int main(void)
{
	struct ae_script_sleeps host, applied;

	ae_sleeps_clear(&host);
	ae_sleeps_clear(&applied);
	expect(ae_sleeps_any(&host), 0, "empty set has none");
	expect(ae_sleeps_next_pending(&host, &applied, 0), -1, "nothing pending when empty");
	expect(ae_sleeps_mark(&host, 7), 1, "first mark is new");
	expect(ae_sleeps_mark(&host, 7), 0, "second mark is not new");
	expect(ae_sleeps_mark(&host, 0), 1, "script 0");
	expect(ae_sleeps_mark(&host, AE_SLEEP_SCRIPTS - 1), 1, "last carried script");
	expect(ae_sleeps_mark(&host, AE_SLEEP_SCRIPTS), 0, "script past the end is not carried");
	expect(ae_sleeps_mark(&host, -1), 0, "negative script is not carried");
	expect(ae_sleeps_test(&host, 7), 1, "marked script tests set");
	expect(ae_sleeps_test(&host, 8), 0, "unmarked script tests clear");
	expect(ae_sleeps_any(&host), 1, "set has some");
	expect(ae_sleeps_next_pending(&host, &applied, 0), 0, "first pending");
	ae_sleeps_mark(&applied, 0);
	expect(ae_sleeps_next_pending(&host, &applied, 0), 7, "applied script is skipped");
	expect(ae_sleeps_next_pending(&host, &applied, 8), AE_SLEEP_SCRIPTS - 1, "scan continues from a script");
	ae_sleeps_mark(&applied, 7);
	ae_sleeps_mark(&applied, AE_SLEEP_SCRIPTS - 1);
	expect(ae_sleeps_next_pending(&host, &applied, 0), -1, "all applied");
	expect(ae_sleeps_entry_valid(40, 40), 1, "same map");
	expect(ae_sleeps_entry_valid(40, 41), 0, "another map's script count");
	expect(ae_sleeps_entry_valid(0, 0), 0, "no scripts");
	if (!failures)
		printf("ae_script_sleep_rules: ok\n");
	return failures ? 1 : 0;
}
