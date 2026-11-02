#include <stdio.h>
#include "ae_bots_live_rules.h"

static int failures;
static int checks;
#define CHECK(c) do { checks++; if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

#define MAXIMUM 15

static void none(int *bot_team)
{
	int index;

	for (index = 0; index < MAXIMUM; index++)
		bot_team[index] = -1;
}

/* as many as asked: nothing changes */
static void test_steady(void)
{
	int bots[MAXIMUM];
	struct ae_bots_live_change change;

	none(bots);
	change = ae_bots_live_decide(0, MAXIMUM, bots, 0, 0, 0, 1, 0, 1);
	CHECK(change.kind == AE_BOTS_LIVE_NONE);
	bots[0] = 0;
	bots[1] = 0;
	change = ae_bots_live_decide(2, MAXIMUM, bots, 0, 0, 0, 3, 0, 1);
	CHECK(change.kind == AE_BOTS_LIVE_NONE);
}

/* free for all: the first missing bot joins, the last leaves */
static void test_free_for_all(void)
{
	int bots[MAXIMUM];
	struct ae_bots_live_change change;

	none(bots);
	bots[0] = 0;
	change = ae_bots_live_decide(3, MAXIMUM, bots, 0, 0, 0, 2, 0, 1);
	CHECK(change.kind == AE_BOTS_LIVE_ADD && change.bot == 1 && change.team == 0);
	/* (a gap left by an earlier leave is filled first) */
	bots[2] = 0;
	change = ae_bots_live_decide(3, MAXIMUM, bots, 0, 0, 0, 3, 0, 1);
	CHECK(change.kind == AE_BOTS_LIVE_ADD && change.bot == 1);
	/* no room: none joins */
	change = ae_bots_live_decide(3, MAXIMUM, bots, 0, 0, 0, 3, 0, 0);
	CHECK(change.kind == AE_BOTS_LIVE_NONE);
	/* fewer: the last leaves, one a frame */
	change = ae_bots_live_decide(0, MAXIMUM, bots, 0, 0, 0, 3, 0, 1);
	CHECK(change.kind == AE_BOTS_LIVE_REMOVE && change.bot == 2);
	bots[2] = -1;
	change = ae_bots_live_decide(0, MAXIMUM, bots, 0, 0, 0, 2, 0, 1);
	CHECK(change.kind == AE_BOTS_LIVE_REMOVE && change.bot == 0);
}

/* the setting past the maximum, below 0, or no person in the game */
static void test_limits(void)
{
	int bots[MAXIMUM];
	int index;
	struct ae_bots_live_change change;

	for (index = 0; index < MAXIMUM; index++)
		bots[index] = 0;
	change = ae_bots_live_decide(40, MAXIMUM, bots, 0, 0, 0, 16, 0, 1);
	CHECK(change.kind == AE_BOTS_LIVE_NONE);
	change = ae_bots_live_decide(-3, MAXIMUM, bots, 0, 0, 0, 16, 0, 1);
	CHECK(change.kind == AE_BOTS_LIVE_REMOVE && change.bot == MAXIMUM - 1);
	change = ae_bots_live_decide(15, MAXIMUM, bots, 0, 0, -1, 15, 0, 1);
	CHECK(change.kind == AE_BOTS_LIVE_REMOVE && change.bot == MAXIMUM - 1);
}

/* even: a bot joins the smaller team (red when even), the larger team's last bot leaves */
static void test_even(void)
{
	int bots[MAXIMUM];
	struct ae_bots_live_change change;

	none(bots);
	/* one person on red: blue, then red */
	change = ae_bots_live_decide(1, MAXIMUM, bots, 1, 0, 0, 1, 0, 1);
	CHECK(change.kind == AE_BOTS_LIVE_ADD && change.bot == 0 && change.team == 1);
	bots[0] = 1;
	change = ae_bots_live_decide(2, MAXIMUM, bots, 1, 0, 0, 1, 1, 1);
	CHECK(change.kind == AE_BOTS_LIVE_ADD && change.bot == 1 && change.team == 0);
	bots[1] = 0;
	bots[2] = 1;
	/* red 2 (person, bot 1), blue 2 (bots 0, 2): even, the last bot leaves */
	change = ae_bots_live_decide(2, MAXIMUM, bots, 1, 0, 0, 2, 2, 1);
	CHECK(change.kind == AE_BOTS_LIVE_REMOVE && change.bot == 2 && change.team == 1);
	/* red 2, blue 1 (bot 0): red's bot 1 leaves, not bot 0 */
	bots[2] = -1;
	change = ae_bots_live_decide(1, MAXIMUM, bots, 1, 0, 0, 2, 1, 1);
	CHECK(change.kind == AE_BOTS_LIVE_REMOVE && change.bot == 1 && change.team == 0);
	/* the larger team has only people: the last bot leaves */
	bots[1] = -1;
	change = ae_bots_live_decide(0, MAXIMUM, bots, 1, 0, 0, 3, 1, 1);
	CHECK(change.kind == AE_BOTS_LIVE_REMOVE && change.bot == 0);
}

/* against: every bot on the people's other team; the last leaves */
static void test_against(void)
{
	int bots[MAXIMUM];
	struct ae_bots_live_change change;

	none(bots);
	change = ae_bots_live_decide(2, MAXIMUM, bots, 1, 1, 0, 1, 0, 1);
	CHECK(change.kind == AE_BOTS_LIVE_ADD && change.team == 1);
	change = ae_bots_live_decide(2, MAXIMUM, bots, 1, 1, 1, 0, 1, 1);
	CHECK(change.kind == AE_BOTS_LIVE_ADD && change.team == 0);
	bots[0] = 1;
	bots[1] = 1;
	bots[2] = 1;
	change = ae_bots_live_decide(1, MAXIMUM, bots, 1, 1, 0, 1, 3, 1);
	CHECK(change.kind == AE_BOTS_LIVE_REMOVE && change.bot == 2);
}

static void test_steps(void)
{
	double second = 0.0;
	int count = 0;

	CHECK(ae_bots_live_test_step("20:3,40:1,60:0", 0, &second, &count) && second == 20.0 && count == 3);
	CHECK(ae_bots_live_test_step("20:3,40:1,60:0", 1, &second, &count) && second == 40.0 && count == 1);
	CHECK(ae_bots_live_test_step("20:3,40:1,60:0", 2, &second, &count) && second == 60.0 && count == 0);
	CHECK(!ae_bots_live_test_step("20:3,40:1,60:0", 3, &second, &count));
	CHECK(ae_bots_live_test_step("2.5:4", 0, &second, &count) && second == 2.5 && count == 4);
	CHECK(!ae_bots_live_test_step("", 0, &second, &count));
	CHECK(!ae_bots_live_test_step(NULL, 0, &second, &count));
	CHECK(!ae_bots_live_test_step("20", 0, &second, &count));
	CHECK(!ae_bots_live_test_step("20:x", 0, &second, &count));
	CHECK(!ae_bots_live_test_step("20:3;40:1", 0, &second, &count));
}

int main(void)
{
	test_steady();
	test_free_for_all();
	test_limits();
	test_even();
	test_against();
	test_steps();
	printf("%d checks, %d failures\n", checks, failures);
	return failures ? 1 : 0;
}
