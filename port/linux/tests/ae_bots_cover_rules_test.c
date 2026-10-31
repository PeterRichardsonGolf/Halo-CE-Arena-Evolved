#include <stdio.h>
#include "ae_bots_cover_rules.h"
#include "ae_bots_combat_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	int level;

	/* cover: two walls or a roof; a corner, an alcove and a roof score more; open ground none */
	CHECK(ae_bots_cover_score(0u, 0) == 0);
	CHECK(ae_bots_cover_score(1u, 0) == 0);
	CHECK(ae_bots_cover_score(0u, 1) > 0);
	CHECK(ae_bots_cover_score(0x11u, 0) > 0);
	CHECK(ae_bots_cover_score(0x03u, 0) > ae_bots_cover_score(0x11u, 0));
	CHECK(ae_bots_cover_score(0x1Fu, 0) > ae_bots_cover_score(0x07u, 0));
	CHECK(ae_bots_cover_score(0x07u, 1) > ae_bots_cover_score(0x07u, 0));
	CHECK(ae_bots_cover_score(0xFFu, 1) >= ae_bots_cover_score(0x7Fu, 1));

	/* the way out: the middle of the longest open run; none when walled all round */
	CHECK(ae_bots_cover_open_side(0xFFu) == -1);
	CHECK(ae_bots_cover_open_side(0x00u) == 0);
	CHECK(ae_bots_cover_open_side(0xFEu) == 0);
	CHECK(ae_bots_cover_open_side(0x7Fu) == 7);
	/* (walls 0-2: open 3-7, its middle 5) */
	CHECK(ae_bots_cover_open_side(0x07u) == 5);
	/* (walls 3-5: open 6,7,0,1,2 round the circle, its middle 0) */
	CHECK(ae_bots_cover_open_side(0x38u) == 0);

	/* the cheap score: away from the threat, near, covered; by an enemy or through one far worse */
	for (level = AE_BOTS_LEVEL_RECRUIT; level < AE_BOTS_LEVELS; level++)
	{
		CHECK(ae_bots_cover_spot_score(level, 5.0f, 20.0f, 4, 0, 0, 0.0f) > ae_bots_cover_spot_score(level, 5.0f, 12.0f, 4, 0, 0, 0.0f));
		CHECK(ae_bots_cover_spot_score(level, 5.0f, 20.0f, 4, 0, 0, 0.0f) > ae_bots_cover_spot_score(level, 15.0f, 20.0f, 4, 0, 0, 0.0f));
		CHECK(ae_bots_cover_spot_score(level, 5.0f, 20.0f, 8, 0, 0, 0.0f) > ae_bots_cover_spot_score(level, 5.0f, 20.0f, 2, 0, 0, 0.0f));
		CHECK(ae_bots_cover_spot_score(level, 5.0f, 20.0f, 4, 2, 0, 0.0f) > ae_bots_cover_spot_score(level, 5.0f, 20.0f, 4, 0, 0, 0.0f));
		CHECK(ae_bots_cover_spot_score(level, 5.0f, 6.0f, 4, 0, 0, 1.0f) < ae_bots_cover_spot_score(level, 5.0f, 12.0f, 4, 0, 0, 0.0f));
		CHECK(ae_bots_cover_spot_score(level, 5.0f, 20.0f, 4, 0, 1, 1.0f) < ae_bots_cover_spot_score(level, 5.0f, 20.0f, 4, 0, 0, 0.0f));
		/* (none known: as far as the tracker's reach) */
		CHECK(ae_bots_cover_spot_score(level, 5.0f, -1.0f, 4, 0, 0, 0.0f) == ae_bots_cover_spot_score(level, 5.0f, 30.0f, 4, 0, 0, 0.0f));
		/* (being seen costs, more each level up) */
		CHECK(ae_bots_cover_seen_penalty(level, 0) == 0.0f);
		CHECK(ae_bots_cover_seen_penalty(level, 2) > ae_bots_cover_seen_penalty(level, 1));
		if (level > AE_BOTS_LEVEL_RECRUIT)
		{
			CHECK(ae_bots_cover_seen_penalty(level, 1) >= ae_bots_cover_seen_penalty(level - 1, 1));
			CHECK(ae_bots_cover_candidates(level) > ae_bots_cover_candidates(level - 1));
			CHECK(ae_bots_cover_hold_ticks(level) >= ae_bots_cover_hold_ticks(level - 1));
			CHECK(ae_bots_cover_watch_ticks(level) < ae_bots_cover_watch_ticks(level - 1));
		}
		CHECK(ae_bots_cover_candidates(level) <= 12);
	}
	/* (chance counts more at the bottom: RECRUIT's pick may lose to a worse spot, DEMON's does not) */
	CHECK(ae_bots_cover_spot_score(AE_BOTS_LEVEL_RECRUIT, 8.0f, 15.0f, 4, 0, 0, 1.0f) >
		ae_bots_cover_spot_score(AE_BOTS_LEVEL_RECRUIT, 5.0f, 20.0f, 4, 0, 0, 0.0f));
	CHECK(ae_bots_cover_spot_score(AE_BOTS_LEVEL_DEMON, 8.0f, 15.0f, 4, 0, 0, 1.0f) <
		ae_bots_cover_spot_score(AE_BOTS_LEVEL_DEMON, 5.0f, 20.0f, 4, 0, 0, 0.0f));
	/* (out of range levels as the nearest) */
	CHECK(ae_bots_cover_candidates(-3) == ae_bots_cover_candidates(AE_BOTS_LEVEL_RECRUIT));
	CHECK(ae_bots_cover_candidates(99) == ae_bots_cover_candidates(AE_BOTS_LEVEL_DEMON));

	/* the hold: on while hidden and alone; an enemy near, a line to it, a hurt, the hold's end each move it on */
	CHECK(ae_bots_cover_leave(AE_BOTS_LEVEL_HEROIC, -1.0f, 0, 0, 0, 30, 0) == 0);
	CHECK(ae_bots_cover_leave(AE_BOTS_LEVEL_HEROIC, 20.0f, 0, 0, 0, 30, 0) == 0);
	CHECK(ae_bots_cover_leave(AE_BOTS_LEVEL_HEROIC, 6.0f, 0, 0, 0, 30, 0) == 1);
	CHECK(ae_bots_cover_leave(AE_BOTS_LEVEL_HEROIC, 20.0f, 1, 0, 0, 30, 0) == 2);
	CHECK(ae_bots_cover_leave(AE_BOTS_LEVEL_HEROIC, 20.0f, 0, 0, 1, 30, 0) == 3);
	CHECK(ae_bots_cover_leave(AE_BOTS_LEVEL_HEROIC, 20.0f, 0, 0, 0, ae_bots_cover_hold_ticks(AE_BOTS_LEVEL_HEROIC), 0) == 4);
	/* (camouflaged: bolder, only the near ones count) */
	CHECK(ae_bots_cover_leave(AE_BOTS_LEVEL_HEROIC, 6.0f, 2, 0, 0, 30, 1) == 0);
	CHECK(ae_bots_cover_leave(AE_BOTS_LEVEL_HEROIC, 3.0f, 0, 0, 0, 30, 1) == 1);
	CHECK(ae_bots_cover_leave(AE_BOTS_LEVEL_HEROIC, 6.0f, 2, 1, 0, 30, 1) == 2);
	/* (RECRUIT holds shorter than DEMON) */
	CHECK(ae_bots_cover_leave(AE_BOTS_LEVEL_RECRUIT, -1.0f, 0, 0, 0, 20 * 30, 0) == 4);
	CHECK(ae_bots_cover_leave(AE_BOTS_LEVEL_DEMON, -1.0f, 0, 0, 0, 20 * 30, 0) == 0);

	if (failures)
		return 1;
	printf("ae_bots_cover_rules: all passed\n");
	return 0;
}
