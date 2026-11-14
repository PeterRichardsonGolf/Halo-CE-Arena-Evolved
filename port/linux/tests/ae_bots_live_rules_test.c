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

/* bots.teams changed in a game: AGAINST YOU moves the bots on the people's team over, last first */
static void test_team_move_against(void)
{
	int bots[MAXIMUM];
	int team;

	none(bots);
	/* (the person red; EVEN had dealt them blue, red, blue, red) */
	bots[0] = 1;
	bots[1] = 0;
	bots[2] = 1;
	bots[3] = 0;
	CHECK(ae_bots_live_team_move(MAXIMUM, bots, 1, 1, 0, 3, 2, &team) == 3 && team == 1);
	bots[3] = 1;
	CHECK(ae_bots_live_team_move(MAXIMUM, bots, 1, 1, 0, 2, 3, &team) == 1 && team == 1);
	bots[1] = 1;
	CHECK(ae_bots_live_team_move(MAXIMUM, bots, 1, 1, 0, 1, 4, &team) == -1 && team == -1);
	/* (the people blue: the bots on blue go red) */
	none(bots);
	bots[0] = 0;
	bots[5] = 1;
	CHECK(ae_bots_live_team_move(MAXIMUM, bots, 1, 1, 1, 1, 2, &team) == 5 && team == 0);
	/* no person in the game, no team game, no bots: no move */
	CHECK(ae_bots_live_team_move(MAXIMUM, bots, 1, 1, -1, 1, 2, &team) == -1);
	CHECK(ae_bots_live_team_move(MAXIMUM, bots, 0, 1, 1, 1, 2, &team) == -1);
	none(bots);
	CHECK(ae_bots_live_team_move(MAXIMUM, bots, 1, 1, 0, 2, 0, &team) == -1);
	CHECK(ae_bots_live_team_move(0, bots, 1, 1, 0, 2, 0, &team) == -1);
}

/* bots.teams changed in a game: EVEN moves the larger team's last bot while the teams are 2 or more apart */
static void test_team_move_even(void)
{
	int bots[MAXIMUM];
	int team;
	int red = 1, blue = 5;

	none(bots);
	/* (the person red, AGAINST YOU had all five bots blue: two moves, then 3 v 3) */
	bots[0] = 1;
	bots[1] = 1;
	bots[2] = 1;
	bots[3] = 1;
	bots[4] = 1;
	CHECK(ae_bots_live_team_move(MAXIMUM, bots, 1, 0, 0, red, blue, &team) == 4 && team == 0);
	bots[4] = 0;
	red++;
	blue--;
	CHECK(ae_bots_live_team_move(MAXIMUM, bots, 1, 0, 0, red, blue, &team) == 3 && team == 0);
	bots[3] = 0;
	red++;
	blue--;
	CHECK(red == 3 && blue == 3);
	CHECK(ae_bots_live_team_move(MAXIMUM, bots, 1, 0, 0, red, blue, &team) == -1 && team == -1);
	/* (one person, four bots: 1 v 4 takes one move, 2 v 3) */
	none(bots);
	bots[0] = 1;
	bots[1] = 1;
	bots[2] = 1;
	bots[3] = 1;
	CHECK(ae_bots_live_team_move(MAXIMUM, bots, 1, 0, 0, 1, 4, &team) == 3 && team == 0);
	CHECK(ae_bots_live_team_move(MAXIMUM, bots, 1, 0, 0, 2, 3, &team) == -1);
	/* (1 apart, or even: no move) */
	CHECK(ae_bots_live_team_move(MAXIMUM, bots, 1, 0, 0, 2, 3, &team) == -1);
	CHECK(ae_bots_live_team_move(MAXIMUM, bots, 1, 0, 0, 3, 3, &team) == -1);
	/* the larger team red (two people there): its last bot goes blue */
	none(bots);
	bots[0] = 0;
	bots[1] = 1;
	bots[4] = 0;
	CHECK(ae_bots_live_team_move(MAXIMUM, bots, 1, 0, 0, 4, 1, &team) == 4 && team == 1);
	/* the larger team has no bot (people only): no move, a person never moves */
	none(bots);
	bots[0] = 1;
	CHECK(ae_bots_live_team_move(MAXIMUM, bots, 1, 0, 0, 3, 1, &team) == -1);
	/* EVEN needs no person (a dedicated server's bots) */
	none(bots);
	bots[0] = 0;
	bots[1] = 0;
	CHECK(ae_bots_live_team_move(MAXIMUM, bots, 1, 0, -1, 2, 0, &team) == 1 && team == 1);
	/* a game without teams: no move */
	CHECK(ae_bots_live_team_move(MAXIMUM, bots, 0, 0, -1, 2, 0, &team) == -1);
}

int main(void)
{
	test_steady();
	test_free_for_all();
	test_limits();
	test_even();
	test_against();
	test_steps();
	test_team_move_against();
	test_team_move_even();
	printf("%d checks, %d failures\n", checks, failures);
	return failures ? 1 : 0;
}
