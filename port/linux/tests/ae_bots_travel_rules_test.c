/* The bots' getting around (port/linux/game/ae_bots_travel_rules.c): links' worth, no return, health packs, jump
facing, stuck spots */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ae_bots_travel_rules.h"

static int failures;
#define AE_CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
#define NEAR(a, b) (fabs((double)(a) - (double)(b)) < 1.0e-4)

static void test_walk(void)
{
	/* a whole way: its length */
	AE_CHECK(NEAR(ae_bots_travel_walk(TRUE, 42.0f, TRUE, 0.0f, 30.0f), 42.0f));
	/* short of the goal: the rest counts long */
	AE_CHECK(ae_bots_travel_walk(TRUE, 20.0f, FALSE, 10.0f, 30.0f) > 20.0f + 10.0f * AE_BOTS_TRAVEL_WALK_STRETCH);
	/* no way at all: longer than the straight line by far */
	AE_CHECK(ae_bots_travel_walk(FALSE, 0.0f, FALSE, 0.0f, 30.0f) >= 30.0f * 3.0f);
	AE_CHECK(ae_bots_travel_walk(FALSE, 0.0f, FALSE, 0.0f, 0.0f) > 0.0f);
}

static void test_links(void)
{
	real teleport = ae_bots_travel_hop_cost(FALSE, 50.0f), jump = ae_bots_travel_hop_cost(TRUE, 4.0f);
	real estimate;

	/* a teleporter's hop costs the same however far it sends */
	AE_CHECK(NEAR(teleport, ae_bots_travel_hop_cost(FALSE, 1.0f)) && teleport > 0.0f);
	AE_CHECK(jump > 4.0f && ae_bots_travel_hop_cost(TRUE, 8.0f) > jump);
	/* the pad 5 away, its receiver 6 from the goal; walking there 80: worth a try, and taken */
	estimate = ae_bots_travel_link_estimate(5.0f, teleport, 6.0f);
	AE_CHECK(NEAR(estimate, 5.0f * AE_BOTS_TRAVEL_WALK_STRETCH + teleport + 6.0f * AE_BOTS_TRAVEL_WALK_STRETCH));
	AE_CHECK(ae_bots_travel_link_worth(estimate, 80.0f));
	AE_CHECK(ae_bots_travel_link_taken(7.0f, teleport, 6.0f, 80.0f));
	/* the walk to the pad turned out long (round a wall): not taken */
	AE_CHECK(!ae_bots_travel_link_taken(70.0f, teleport, 6.0f, 80.0f));
	/* walking about as far: not worth it (the margin) */
	AE_CHECK(!ae_bots_travel_link_worth(estimate, estimate));
	AE_CHECK(!ae_bots_travel_link_worth(estimate, (estimate + AE_BOTS_TRAVEL_TRY_MARGIN) / AE_BOTS_TRAVEL_TRY_SHARE));
	AE_CHECK(ae_bots_travel_link_worth(estimate, (estimate + AE_BOTS_TRAVEL_TRY_MARGIN) / AE_BOTS_TRAVEL_TRY_SHARE + 0.1f));
	/* a short walk never takes a link */
	AE_CHECK(!ae_bots_travel_link_worth(0.0f, 8.0f) && !ae_bots_travel_link_taken(0.0f, 0.0f, 0.0f, 5.0f));
	/* a goal the walk cannot reach (no way: the top floor the walkable surfaces miss): a jump up there is taken */
	AE_CHECK(ae_bots_travel_link_taken(10.0f, jump, 2.0f, ae_bots_travel_walk(FALSE, 0.0f, FALSE, 0.0f, 14.0f)));
	AE_CHECK(ae_bots_travel_link_taken(10.0f, jump, 2.0f, ae_bots_travel_walk(TRUE, 9.0f, FALSE, 6.0f, 14.0f)));
}

static void test_no_return(void)
{
	long a_second = AE_BOTS_TRAVEL_TICKS_PER_SECOND;

	/* never came out of one: any link */
	AE_CHECK(ae_bots_travel_link_allowed(0, 0, 0, FALSE, 0, 0, 0, 0, TRUE));
	/* the return pad beside where it came out: not for a while */
	AE_CHECK(!ae_bots_travel_link_allowed(1, 1, 0, TRUE, 0, 0, 0, a_second, FALSE));
	AE_CHECK(!ae_bots_travel_link_allowed(1, 1, 0, TRUE, 0, 0, 0, AE_BOTS_TRAVEL_RETURN_TICKS - 1, FALSE));
	AE_CHECK(ae_bots_travel_link_allowed(1, 1, 0, TRUE, 0, 0, 0, AE_BOTS_TRAVEL_RETURN_TICKS, FALSE));
	/* another pad far off: at once */
	AE_CHECK(ae_bots_travel_link_allowed(30, 0, 0, TRUE, 0, 0, 0, 1, FALSE));
	/* nor the same link again at once, wherever it starts */
	AE_CHECK(!ae_bots_travel_link_allowed(30, 0, 0, TRUE, 0, 0, 0, 1, TRUE));
	AE_CHECK(ae_bots_travel_link_allowed(30, 0, 0, TRUE, 0, 0, 0, AE_BOTS_TRAVEL_AGAIN_TICKS, TRUE));
	/* (the radius in three dimensions: a pad a floor below is another) */
	AE_CHECK(ae_bots_travel_link_allowed(0, 0, 5, TRUE, 0, 0, 0, a_second, FALSE));
}

static void test_packs(void)
{
	/* HALO CE: all that is missing */
	AE_CHECK(NEAR(ae_bots_travel_pack_need(AE_BOTS_TRAVEL_HEALTH_CLASSIC, 0.25f), 0.75f));
	AE_CHECK(NEAR(ae_bots_travel_pack_need(AE_BOTS_TRAVEL_HEALTH_CLASSIC, 1.0f), 0.0f));
	AE_CHECK(NEAR(ae_bots_travel_pack_need(AE_BOTS_TRAVEL_HEALTH_CLASSIC, 1.5f), 0.0f));
	/* REACH: what does not come back (above the third it is in) */
	AE_CHECK(NEAR(ae_bots_travel_pack_need(AE_BOTS_TRAVEL_HEALTH_REACH, 0.2f), 2.0f / 3.0f));
	AE_CHECK(NEAR(ae_bots_travel_pack_need(AE_BOTS_TRAVEL_HEALTH_REACH, 0.5f), 1.0f / 3.0f));
	AE_CHECK(NEAR(ae_bots_travel_pack_need(AE_BOTS_TRAVEL_HEALTH_REACH, 1.0f / 3.0f), 2.0f / 3.0f));
	AE_CHECK(NEAR(ae_bots_travel_pack_need(AE_BOTS_TRAVEL_HEALTH_REACH, 0.9f), 0.0f));
	/* HALO 2 / HALO 3: all of it comes back, no pack wanted */
	AE_CHECK(NEAR(ae_bots_travel_pack_need(AE_BOTS_TRAVEL_HEALTH_FULL, 0.1f), 0.0f));
	/* dead: nothing */
	AE_CHECK(NEAR(ae_bots_travel_pack_need(AE_BOTS_TRAVEL_HEALTH_CLASSIC, 0.0f), 0.0f));
	/* the score: none for a little, more the more is missing and the nearer */
	AE_CHECK(ae_bots_travel_pack_score(0.1f, 2.0f, 0.0f, FALSE) <= 0.0f);
	AE_CHECK(ae_bots_travel_pack_score(0.3f, 2.0f, 0.0f, FALSE) > 0.0f);
	AE_CHECK(ae_bots_travel_pack_score(0.3f, 2.0f, 0.0f, TRUE) <= 0.0f);
	AE_CHECK(ae_bots_travel_pack_score(0.6f, 2.0f, 0.0f, TRUE) > 0.0f);
	AE_CHECK(ae_bots_travel_pack_score(0.8f, 10.0f, 0.0f, FALSE) > ae_bots_travel_pack_score(0.4f, 10.0f, 0.0f, FALSE));
	AE_CHECK(ae_bots_travel_pack_score(0.8f, 5.0f, 0.0f, FALSE) > ae_bots_travel_pack_score(0.8f, 20.0f, 0.0f, FALSE));
	/* reach: 6 + 34 x need units */
	AE_CHECK(ae_bots_travel_pack_score(1.0f, 39.0f, 0.0f, FALSE) > 0.0f);
	AE_CHECK(ae_bots_travel_pack_score(1.0f, 40.5f, 0.0f, FALSE) <= 0.0f);
	AE_CHECK(ae_bots_travel_pack_score(0.5f, 22.0f, 0.0f, FALSE) > 0.0f);
	AE_CHECK(ae_bots_travel_pack_score(0.5f, 24.0f, 0.0f, FALSE) <= 0.0f);
	/* a floor up counts double */
	AE_CHECK(ae_bots_travel_pack_score(0.5f, 10.0f, 4.0f, FALSE) < ae_bots_travel_pack_score(0.5f, 14.0f, 0.0f, FALSE) + 1e-6f);
	AE_CHECK(ae_bots_travel_pack_score(0.5f, 10.0f, 7.0f, FALSE) <= 0.0f);
}

static void test_jump_facing(void)
{
	AE_CHECK(ae_bots_travel_jump_aligned(1, 0, 1, 0));
	AE_CHECK(ae_bots_travel_jump_aligned(1, 1.6f, 1, 0));   /* 58 degrees */
	AE_CHECK(!ae_bots_travel_jump_aligned(1, 1.8f, 1, 0));  /* 61 degrees */
	AE_CHECK(!ae_bots_travel_jump_aligned(-1, 0, 1, 0));
	AE_CHECK(!ae_bots_travel_jump_aligned(0, 0, 1, 0));     /* not moving */
	AE_CHECK(!ae_bots_travel_jump_aligned(1, 0, 0, 0));
}

static void test_stuck_spots(void)
{
	static struct ae_bots_travel_stuck_spots spots;
	char text[400];
	int index;

	memset(&spots, 0, sizeof(spots));
	AE_CHECK(ae_bots_travel_stuck_minute(&spots, 8, text, sizeof(text)) == 0 && !text[0]);
	ae_bots_travel_stuck_add(&spots, 10, 10, 0);
	ae_bots_travel_stuck_add(&spots, 11, 10, 0);   /* the same spot: the mean */
	ae_bots_travel_stuck_add(&spots, 10, 11, 0.5f);
	ae_bots_travel_stuck_add(&spots, -40, 3, 2);   /* another */
	AE_CHECK(spots.count == 2 && spots.spots[0].total == 3 && spots.spots[1].total == 1);
	AE_CHECK(NEAR(spots.spots[0].x, 31.0f / 3.0f) && NEAR(spots.spots[0].y, 31.0f / 3.0f));
	AE_CHECK(ae_bots_travel_stuck_minute(&spots, 8, text, sizeof(text)) == 2);
	AE_CHECK(strstr(text, "10.3 10.3 0.2 (3 of 3)") == text + 1 && strstr(text, "-40.0 3.0 2.0 (1 of 1)"));
	/* the next minute: only where bots stood stuck in it */
	AE_CHECK(ae_bots_travel_stuck_minute(&spots, 8, text, sizeof(text)) == 0 && !text[0]);
	ae_bots_travel_stuck_add(&spots, -40, 3, 2);
	AE_CHECK(ae_bots_travel_stuck_minute(&spots, 8, text, sizeof(text)) == 1 && strstr(text, "(1 of 2)"));
	/* most first, at most maximum */
	for (index = 0; index < 5; index++)
		ae_bots_travel_stuck_add(&spots, -40, 3, 2);
	ae_bots_travel_stuck_add(&spots, 10, 10, 0);
	ae_bots_travel_stuck_add(&spots, 100, 0, 0);
	AE_CHECK(ae_bots_travel_stuck_minute(&spots, 2, text, sizeof(text)) == 2 && strstr(text, " -40.0") == text);
	/* a full table: new spots are counted dropped, the old ones still count */
	for (index = 0; index < AE_BOTS_TRAVEL_STUCK_SPOTS + 5; index++)
		ae_bots_travel_stuck_add(&spots, 200.0f + 10.0f * (real)index, 0, 0);
	AE_CHECK(spots.count == AE_BOTS_TRAVEL_STUCK_SPOTS && spots.dropped == 8);
	ae_bots_travel_stuck_add(&spots, 10, 10, 0);
	AE_CHECK(spots.spots[0].total == 5);
	/* a small text: cut, never past its end */
	AE_CHECK(ae_bots_travel_stuck_minute(&spots, 40, text, 20) > 0 && strlen(text) < 20);
	AE_CHECK(ae_bots_travel_stuck_minute(&spots, 8, NULL, 0) == 0);
}

static void test_walk_links(void)
{
	long from[5] = { 1, 2, 1, 3, 1 }, to[5] = { 2, 3, 3, 1, 1 };
	unsigned char usable[5] = { 1, 1, 1, 1, 1 };

	/* ends: stairs-steep at most, not touching, not far */
	AE_CHECK(ae_bots_travel_walk_ends(2.0f, 1.5f));
	AE_CHECK(ae_bots_travel_walk_ends(2.0f, -1.5f));
	AE_CHECK(!ae_bots_travel_walk_ends(1.0f, 2.4f));
	AE_CHECK(!ae_bots_travel_walk_ends(0.3f, 0.0f));
	AE_CHECK(!ae_bots_travel_walk_ends(7.0f, 0.0f));
	/* steps: a stair's rise up, a little more down; not a ledge */
	AE_CHECK(ae_bots_travel_walk_step(0.2f));
	AE_CHECK(ae_bots_travel_walk_step(-0.5f));
	AE_CHECK(!ae_bots_travel_walk_step(0.6f));
	AE_CHECK(!ae_bots_travel_walk_step(-1.2f));
	/* the route: straight there when a link does it, else through the fewest; none from or to nowhere */
	AE_CHECK(ae_bots_travel_route(5, from, to, usable, 1, 3) == 2);
	AE_CHECK(ae_bots_travel_route(5, from, to, usable, 1, 2) == 0);
	usable[2] = 0;
	AE_CHECK(ae_bots_travel_route(5, from, to, usable, 1, 3) == 0);
	AE_CHECK(ae_bots_travel_route(5, from, to, usable, 3, 2) == 3);
	AE_CHECK(ae_bots_travel_route(5, from, to, usable, 2, 1) == 1);
	AE_CHECK(ae_bots_travel_route(5, from, to, usable, 1, 1) == -1);
	AE_CHECK(ae_bots_travel_route(5, from, to, usable, 1, 9) == -1);
	AE_CHECK(ae_bots_travel_route(5, from, to, usable, 9, 1) == -1);
	AE_CHECK(ae_bots_travel_route(0, from, to, usable, 1, 3) == -1);
}

static void test_long_ways(void)
{
	real walked[] = { 3.0f, 9.0f, 17.0f, 24.0f, 31.0f, 60.0f };

	/* at the wall's foot, 150 to go by the walk either way: no gain (a via); 60 along a long way: kept */
	AE_CHECK(!ae_bots_travel_partial_gains(151.0f, 150.0f));
	AE_CHECK(!ae_bots_travel_partial_gains(151.0f, 151.0f + AE_BOTS_TRAVEL_GAIN_LEAST));
	AE_CHECK(ae_bots_travel_partial_gains(151.0f, 91.0f));
	AE_CHECK(ae_bots_travel_partial_gains(151.0f, 151.0f - AE_BOTS_TRAVEL_GAIN_LEAST));
	/* ended where no walk goes on to the goal: no gain; the bot's own place unknown: whatever the end, a gain */
	AE_CHECK(!ae_bots_travel_partial_gains(40.0f, -1.0f));
	AE_CHECK(ae_bots_travel_partial_gains(-1.0f, 40.0f));
	/* the via: the last point within the reach; the first when even it is beyond; none on no way */
	AE_CHECK(ae_bots_travel_way_via(walked, 6, AE_BOTS_TRAVEL_VIA_REACH) == 3);
	AE_CHECK(ae_bots_travel_way_via(walked, 6, 2.0f) == 0);
	AE_CHECK(ae_bots_travel_way_via(walked, 6, 100.0f) == 5);
	AE_CHECK(ae_bots_travel_way_via(walked, 1, AE_BOTS_TRAVEL_VIA_REACH) == 0);
	AE_CHECK(ae_bots_travel_way_via(walked, 0, AE_BOTS_TRAVEL_VIA_REACH) == -1);
}

/* climb mode over a drop (Madhouse's top floor railing): a pit ahead ends it, ground at the bot's height or a goal down
there does not */
static void test_climb_drop(void)
{
	AE_CHECK(ae_bots_travel_climb_drops(0, 5.0f, 0.0f, 9.0f));
	AE_CHECK(ae_bots_travel_climb_drops(1, 5.0f, 0.0f, 5.0f));
	AE_CHECK(!ae_bots_travel_climb_drops(1, 5.0f, 5.0f, 9.0f));
	AE_CHECK(!ae_bots_travel_climb_drops(1, 5.0f, 4.5f, 9.0f));
	AE_CHECK(!ae_bots_travel_climb_drops(1, 5.0f, 0.0f, 0.5f));
	AE_CHECK(!ae_bots_travel_climb_drops(1, 5.0f, 0.0f, -3.0f));
}

static void test_device_and_goal_stuck(void)
{
	AE_CHECK(ae_bots_travel_device_block_radius(0.1f) == AE_BOTS_TRAVEL_DEVICE_BLOCK_MIN);
	AE_CHECK(ae_bots_travel_device_block_radius(1.2f) == 1.2f);
	AE_CHECK(ae_bots_travel_device_block_radius(9.0f) == AE_BOTS_TRAVEL_DEVICE_BLOCK_MAX);
	/* reports 10 seconds apart add up (a move between them does not clear them); a long quiet starts again */
	AE_CHECK(ae_bots_travel_goal_stuck_next(0, 0, 1000) == 1);
	AE_CHECK(ae_bots_travel_goal_stuck_next(1, 1000, 1300) == 2);
	AE_CHECK(ae_bots_travel_goal_stuck_next(3, 1300, 1600) == 4);
	AE_CHECK(ae_bots_travel_goal_stuck_next(3, 1000, 1000 + AE_BOTS_TRAVEL_GOAL_STUCK_FORGET + 1) == 1);
	AE_CHECK(!ae_bots_travel_goal_stuck_gives_up(3));
	AE_CHECK(ae_bots_travel_goal_stuck_gives_up(4));
}

int main(void)
{
	test_device_and_goal_stuck();
	test_long_ways();
	test_climb_drop();
	test_walk_links();
	test_walk();
	test_links();
	test_no_return();
	test_packs();
	test_jump_facing();
	test_stuck_spots();
	printf("%s\n", failures ? "FAILED" : "ok");
	return failures != 0;
}
