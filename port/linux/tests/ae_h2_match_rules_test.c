#include <stdio.h>
#include "ae_h2_match_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	long tied[3] = { 4, 4, 1 }, led[3] = { 1, 7, 3 }, one[1] = { 0 };

	/* no rounds, or one: the game ends with its first */
	CHECK(!ae_h2_rounds_go_on(0, 0, 0, 1, 1, 0));
	CHECK(!ae_h2_rounds_go_on(1, 0, 1, 1, 1, 0));
	/* four rounds: played through, a tie in wins plays on only with RESOLVE TIES */
	CHECK(ae_h2_rounds_go_on(4, 0, 0, 1, 1, 0));
	CHECK(ae_h2_rounds_go_on(4, 0, 0, 3, 3, 0));
	CHECK(!ae_h2_rounds_go_on(4, 0, 0, 4, 2, 2));
	CHECK(ae_h2_rounds_go_on(4, 0, 1, 4, 2, 2));
	CHECK(!ae_h2_rounds_go_on(4, 0, 1, 4, 3, 1));
	/* best of 3: a majority ends it early */
	CHECK(ae_h2_rounds_go_on(3, 2, 0, 1, 1, 0));
	CHECK(!ae_h2_rounds_go_on(3, 2, 0, 2, 2, 0));
	CHECK(ae_h2_rounds_go_on(3, 2, 0, 2, 1, 1));
	CHECK(!ae_h2_rounds_go_on(3, 2, 0, 3, 2, 1));
	CHECK(ae_h2_rounds_go_on(5, 3, 0, 4, 2, 2));
	/* leaders */
	CHECK(ae_h2_unique_leader(led, 3) == 1);
	CHECK(ae_h2_unique_leader(tied, 3) == -1);
	CHECK(ae_h2_unique_leader(one, 1) == 0);
	CHECK(ae_h2_unique_leader(one, 0) == -1);
	/* bonus points */
	CHECK(ae_h2_bonus_points(0, 1) == 0);
	CHECK(ae_h2_bonus_points(1, 1) == 1);
	CHECK(ae_h2_bonus_points(0, 5) == 1);
	CHECK(ae_h2_bonus_points(1, 10) == 2);
	CHECK(ae_h2_bonus_points(0, 0) == 0);
	/* max active players */
	CHECK(ae_h2_may_spawn(0, 9, 3));
	CHECK(ae_h2_may_spawn(2, 1, 0));
	CHECK(!ae_h2_may_spawn(2, 2, 0));
	CHECK(!ae_h2_may_spawn(2, 1, 1));
	/* an explicit end (END GAME, an admin, a vote) is the game's, rounds or not; the engines' own ends are rounds' */
	CHECK(ae_h2_end_is_round(0, 3));
	CHECK(!ae_h2_end_is_round(1, 3));
	CHECK(!ae_h2_end_is_round(0, 1));
	CHECK(!ae_h2_end_is_round(0, 0));
	/* slots: players 0 and 32 (and 127 and 255) never share one */
	CHECK(ae_h2_slot_of(0, 128) == 0);
	CHECK(ae_h2_slot_of(32, 128) == 32);
	CHECK(ae_h2_slot_of(127, 128) == 127);
	CHECK(ae_h2_slot_of(128, 128) == -1);
	CHECK(ae_h2_slot_of(-1, 128) == -1);
	if (!failures)
		printf("ok\n");
	return failures ? 1 : 0;
}
