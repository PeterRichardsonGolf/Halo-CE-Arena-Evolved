#include <stdio.h>
#include <stdlib.h>
#include "ae_bots_fear_rules.h"

#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); exit(1); } } while (0)

int main(void)
{
	/* no cool-down, path or no stuck: may start */
	CHECK(ae_bots_fear_flank_may_start(1000, 0, 1, 0));
	CHECK(ae_bots_fear_flank_may_start(1000, 0, 0, 0));
	CHECK(ae_bots_fear_flank_may_start(1000, 0, 1, 1));
	/* inside the cool-down: no; at its end: yes */
	CHECK(!ae_bots_fear_flank_may_start(1000, 1100, 1, 0));
	CHECK(!ae_bots_fear_flank_may_start(1000, 1001, 1, 0));
	CHECK(ae_bots_fear_flank_may_start(1100, 1100, 1, 0));
	CHECK(ae_bots_fear_flank_may_start(1200, 1100, 1, 0));
	/* a stale far-future value (clock restarted) does not block */
	CHECK(ae_bots_fear_flank_may_start(10, 100000, 1, 0));
	/* no path and stuck: no */
	CHECK(!ae_bots_fear_flank_may_start(1000, 0, 0, 1));
	/* the cool-down's end by why */
	CHECK(ae_bots_fear_flank_cooldown_end(1000, "unseen") == 1000 + AE_FEAR_FLANK_COOLDOWN_TICKS);
	CHECK(ae_bots_fear_flank_cooldown_end(1000, "target") == 1150);
	CHECK(ae_bots_fear_flank_cooldown_end(1000, "time") == 1150);
	CHECK(AE_FEAR_FLANK_COOLDOWN_TICKS == 150);
	CHECK(ae_bots_fear_flank_cooldown_end(1000, "reached") == 0);
	CHECK(ae_bots_fear_flank_cooldown_end(1000, "gone") == 0);
	CHECK(ae_bots_fear_flank_cooldown_end(1000, "edge") == 0);
	CHECK(ae_bots_fear_flank_cooldown_end(1000, NULL) == 0);
	/* a flank that ended unseen then starts again 1 s later (the owner's loop): blocked */
	CHECK(!ae_bots_fear_flank_may_start(1030, ae_bots_fear_flank_cooldown_end(1000, "unseen"), 1, 0));
	printf("ae_bots_fear_rules: all passed\n");
	return 0;
}
