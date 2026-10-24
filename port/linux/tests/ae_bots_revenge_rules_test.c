#include <stdio.h>
#include "ae_bots_revenge_rules.h"
#include "ae_bots_combat_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	struct ae_bots_revenge state;
	int tps = 30;
	int handle = (int)0xec750005u;

	/* three deaths in a row to one player: the third makes them the nemesis for 60 s */
	ae_bots_revenge_clear(&state);
	CHECK(ae_bots_revenge_nemesis(&state, 0) == -1);
	CHECK(ae_bots_revenge_death(&state, 7, 100, tps) == 0);
	CHECK(ae_bots_revenge_death(&state, 7, 200, tps) == 0);
	CHECK(ae_bots_revenge_nemesis(&state, 250) == -1);
	CHECK(ae_bots_revenge_death(&state, 7, 300, tps) == 1);
	CHECK(ae_bots_revenge_nemesis(&state, 300) == 7);
	CHECK(ae_bots_revenge_nemesis(&state, 300 + 60 * tps - 1) == 7);
	/* time up: let go */
	CHECK(ae_bots_revenge_nemesis(&state, 300 + 60 * tps) == -1);

	/* a death to someone else starts a new count; suicides and teammates (-1) count nothing and break nothing */
	ae_bots_revenge_clear(&state);
	ae_bots_revenge_death(&state, 7, 100, tps);
	ae_bots_revenge_death(&state, 7, 110, tps);
	CHECK(ae_bots_revenge_death(&state, -1, 120, tps) == 0);
	CHECK(ae_bots_revenge_death(&state, 7, 130, tps) == 1);
	ae_bots_revenge_clear(&state);
	ae_bots_revenge_death(&state, 7, 100, tps);
	ae_bots_revenge_death(&state, 7, 110, tps);
	CHECK(ae_bots_revenge_death(&state, 4, 120, tps) == 0);
	CHECK(ae_bots_revenge_death(&state, 7, 130, tps) == 0);
	CHECK(ae_bots_revenge_nemesis(&state, 130) == -1);

	/* killing the killer back resets the count */
	ae_bots_revenge_clear(&state);
	ae_bots_revenge_death(&state, 7, 100, tps);
	ae_bots_revenge_death(&state, 7, 110, tps);
	CHECK(ae_bots_revenge_kill(&state, 7) == 0);
	CHECK(ae_bots_revenge_death(&state, 7, 130, tps) == 0);
	CHECK(ae_bots_revenge_nemesis(&state, 130) == -1);

	/* killing the nemesis ends the hunt (1); killing someone else does not */
	ae_bots_revenge_clear(&state);
	ae_bots_revenge_death(&state, 7, 100, tps);
	ae_bots_revenge_death(&state, 7, 110, tps);
	ae_bots_revenge_death(&state, 7, 120, tps);
	CHECK(ae_bots_revenge_kill(&state, 3) == 0);
	CHECK(ae_bots_revenge_nemesis(&state, 130) == 7);
	CHECK(ae_bots_revenge_kill(&state, 7) == 1);
	CHECK(ae_bots_revenge_nemesis(&state, 130) == -1);
	CHECK(ae_bots_revenge_kill(&state, -1) == 0);

	/* killed three more times while hunting the same one: the clock restarts, not a new hunt (0) */
	ae_bots_revenge_clear(&state);
	ae_bots_revenge_death(&state, 7, 100, tps);
	ae_bots_revenge_death(&state, 7, 110, tps);
	ae_bots_revenge_death(&state, 7, 120, tps);
	ae_bots_revenge_death(&state, 7, 1000, tps);
	ae_bots_revenge_death(&state, 7, 1100, tps);
	CHECK(ae_bots_revenge_death(&state, 7, 1200, tps) == 0);
	CHECK(ae_bots_revenge_nemesis(&state, 1200 + 60 * tps - 1) == 7);
	/* three to another while hunting: that one is the nemesis now */
	ae_bots_revenge_death(&state, 5, 1300, tps);
	ae_bots_revenge_death(&state, 5, 1310, tps);
	CHECK(ae_bots_revenge_death(&state, 5, 1320, tps) == 1);
	CHECK(ae_bots_revenge_nemesis(&state, 1320) == 5);

	/* a real player handle is negative as an int: still a killer */
	ae_bots_revenge_clear(&state);
	ae_bots_revenge_death(&state, handle, 100, tps);
	ae_bots_revenge_death(&state, handle, 110, tps);
	CHECK(ae_bots_revenge_death(&state, handle, 120, tps) == 1);
	CHECK(ae_bots_revenge_nemesis(&state, 130) == handle);
	CHECK(ae_bots_revenge_kill(&state, handle) == 1);

	/* by level: RECRUIT..NORMAL reckless, HEROIC direct, LEGENDARY and DEMON smart; the tilt shrinks with level */
	CHECK(ae_bots_revenge_style(AE_BOTS_LEVEL_RECRUIT) == AE_BOTS_REVENGE_RECKLESS);
	CHECK(ae_bots_revenge_style(AE_BOTS_LEVEL_EASY) == AE_BOTS_REVENGE_RECKLESS);
	CHECK(ae_bots_revenge_style(AE_BOTS_LEVEL_NORMAL) == AE_BOTS_REVENGE_RECKLESS);
	CHECK(ae_bots_revenge_style(AE_BOTS_LEVEL_HEROIC) == AE_BOTS_REVENGE_DIRECT);
	CHECK(ae_bots_revenge_style(AE_BOTS_LEVEL_LEGENDARY) == AE_BOTS_REVENGE_SMART);
	CHECK(ae_bots_revenge_style(AE_BOTS_LEVEL_DEMON) == AE_BOTS_REVENGE_SMART);
	CHECK(ae_bots_revenge_confidence(AE_BOTS_LEVEL_NORMAL) == 0.3f);
	CHECK(ae_bots_revenge_confidence(AE_BOTS_LEVEL_HEROIC) == 0.1f);
	CHECK(ae_bots_revenge_confidence(AE_BOTS_LEVEL_DEMON) == 0.0f);
	/* (a reckless bot at even strength stays in the fight longer: past the retreat's threshold by the tilt) */
	CHECK(ae_bots_revenge_confidence(AE_BOTS_LEVEL_NORMAL) > ae_bots_revenge_confidence(AE_BOTS_LEVEL_HEROIC));

	if (failures)
		return 1;
	printf("ok\n");
	return 0;
}
