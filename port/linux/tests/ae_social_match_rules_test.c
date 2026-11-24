#include <stdio.h>
#include <string.h>
#include "ae_social_match_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
#define EQ(a, b) do { long x_ = (long)(a), y_ = (long)(b); if (x_ != y_) { printf("FAIL %s:%d %s = %ld, want %ld\n", __FILE__, __LINE__, #a, x_, y_); failures++; } } while (0)

static void test_ladder(void)
{
	/* golden: shooting, movement, strength and level of every tier */
	static int const golden[AE_SOCIAL_TIERS][4] = {
		{1, 1, 3, 4}, {1, 2, 4, 6}, {1, 3, 5, 8}, {2, 2, 6, 10}, {2, 3, 7, 12}, {3, 2, 8, 15}, {3, 3, 9, 17}, {3, 4, 10, 19},
		{4, 3, 11, 22}, {4, 4, 12, 24}, {4, 5, 13, 27}, {5, 4, 14, 29}, {5, 5, 15, 32}, {5, 6, 16, 35}, {6, 5, 17, 38},
		{6, 6, 18, 41}, {6, 7, 19, 44}, {7, 6, 20, 47}, {7, 7, 21, 49}};
	int tier, step;

	for (tier = 1; tier <= AE_SOCIAL_TIERS; tier++)
	{
		struct ae_social_tier const *t = ae_social_tier(tier);

		CHECK(t != NULL);
		EQ(t->shooting, golden[tier - 1][0]);
		EQ(t->movement, golden[tier - 1][1]);
		EQ(t->strength, golden[tier - 1][2]);
		EQ(t->level, golden[tier - 1][3]);
		/* strength = 2 x shooting + movement, one tier a value from 3, movement 1 under to 2 over shooting */
		EQ(t->strength, 2 * t->shooting + t->movement);
		EQ(t->strength, tier + 2);
		CHECK(t->movement >= t->shooting - 1 && t->movement <= t->shooting + 2);
		if (tier > 1)
			CHECK(t->level > ae_social_tier(tier - 1)->level);
	}
	CHECK(ae_social_tier(0) == NULL && ae_social_tier(20) == NULL);
	/* FIXED: the same-skill tiers */
	EQ(ae_social_tier_fixed(1), 1); EQ(ae_social_tier_fixed(2), 4); EQ(ae_social_tier_fixed(3), 7);
	EQ(ae_social_tier_fixed(4), 10); EQ(ae_social_tier_fixed(5), 13); EQ(ae_social_tier_fixed(6), 16);
	EQ(ae_social_tier_fixed(7), 19); EQ(ae_social_tier_fixed(0), 0); EQ(ae_social_tier_fixed(8), 0);
	for (step = 1; step <= 7; step++)
		CHECK(ae_social_skill_name(step)[0] != 0);
	CHECK(!strcmp(ae_social_skill_name(3), "NORMAL") && !strcmp(ae_social_skill_name(7), "DEMON"));
	CHECK(ae_social_skill_name(0)[0] == 0);
	/* the tier of a level: nearest, a tie to the lower (16 is between 15 and 17: 15's tier 6) */
	EQ(ae_social_tier_of_level(17), 7); EQ(ae_social_tier_of_level(16), 6); EQ(ae_social_tier_of_level(1), 1);
	EQ(ae_social_tier_of_level(50), 19); EQ(ae_social_tier_of_level(22), 9);
	/* the start: 1600 points = shown 17 = tier 7 */
	EQ(ae_social_match_level(AE_SOCIAL_MATCH_START), 16);
	EQ(ae_social_match_tier(AE_SOCIAL_MATCH_START), 7);
	EQ(ae_social_match_points_of_tier(7), 1600);
	EQ(ae_social_match_points_of_tier(9), 2100);
	EQ(ae_social_match_tier(ae_social_match_points_of_tier(9)), 9);
	for (tier = 1; tier <= AE_SOCIAL_TIERS; tier++)
		EQ(ae_social_match_tier(ae_social_match_points_of_tier(tier)), tier);
	EQ(ae_social_match_level(99999), 49); EQ(ae_social_match_level(-5), 0);
}

static void test_window_holds_neighbours(void)
{
	int tier;

	for (tier = 1; tier <= AE_SOCIAL_TIERS; tier++)
	{
		int low, high, other;

		ae_social_match_window(ae_social_tier_raw_level(tier), &low, &high);
		for (other = tier - 2; other <= tier + 2; other++)
		{
			if (other < 1 || other > AE_SOCIAL_TIERS)
				continue;
			CHECK(ae_social_tier(other)->level >= low && ae_social_tier(other)->level <= high);
		}
	}
}

static void test_change(void)
{
	int levels[4], results[4];

	/* a win against an equal bot +100, a loss -100 (no visible-rank weights at low levels) */
	EQ(ae_social_match_pair_change(10, 10, 1), 100);
	EQ(ae_social_match_pair_change(10, 10, -1), -100);
	EQ(ae_social_match_pair_change(10, 10, 0), 0);
	/* a win against one 8+ levels above +180, 7 below +30; the loss mirrored */
	EQ(ae_social_match_pair_change(10, 18, 1), 180);
	EQ(ae_social_match_pair_change(10, 3, 1), 30);
	EQ(ae_social_match_pair_change(10, 18, -1), -20);
	EQ(ae_social_match_pair_change(10, 3, -1), -170);
	levels[0] = levels[1] = levels[2] = levels[3] = 16;
	results[0] = results[1] = results[2] = results[3] = 1;
	/* the placement games count double: +200 for a win against equals; later +100 */
	EQ(ae_social_match_change(1600, 0, levels, results, 4), 200);
	EQ(ae_social_match_change(1600, 4, levels, results, 4), 200);
	EQ(ae_social_match_change(1600, 5, levels, results, 4), 100);
	results[0] = results[1] = results[2] = results[3] = -1;
	EQ(ae_social_match_change(1600, 5, levels, results, 4), -100);
	/* averaged: two wins two losses against equals */
	results[0] = results[1] = 1;
	EQ(ae_social_match_change(1600, 5, levels, results, 4), 0);
	/* clamped to +-200 before the doubling, and the points kept in range */
	levels[0] = 49; results[0] = -1;
	EQ(ae_social_match_change(4900, 9, levels, results, 1), -100);
	EQ(ae_social_match_change(50, 9, levels, results, 1), -20);
	levels[0] = 0; results[0] = -1;
	EQ(ae_social_match_change(50, 9, levels, results, 1), -50);
	EQ(ae_social_match_change(1600, 9, levels, results, 0), 0);
	EQ(ae_social_match_apply(4990, 200), AE_SOCIAL_MATCH_POINTS_MAX);
	EQ(ae_social_match_apply(10, -200), 0);
}

static unsigned long sim_state = 12345;
static int sim_below(int bound)
{
	sim_state = sim_state * 1103515245u + 12345u;
	return (int)((sim_state >> 16) % (unsigned)bound);
}

/* a player of a true strength (shown level) plays many 2 x 4 games at AUTO: his hidden level settles near it */
static int simulate(int true_level, int games)
{
	long points = AE_SOCIAL_MATCH_START, sum = 0;
	int game, samples = 0;

	for (game = 0; game < games; game++)
	{
		struct ae_social_shape shape = {2, 4};
		struct ae_social_roster roster;
		int levels[8], results[8], count = 0, seat, win, p;
		long total = 0;

		if (!ae_social_roster_draw(&shape, 1, points, 0, (unsigned long)game * 7919 + 1, &roster))
			return -1;
		for (seat = 0; seat < roster.count; seat++)
			if (roster.seat[seat].team != roster.seat[0].team)
			{
				levels[count++] = roster.seat[seat].level - 1;
				total += roster.seat[seat].level;
			}
		p = 500 + (true_level - (int)(total / count)) * 40;
		if (p < 20) p = 20;
		if (p > 980) p = 980;
		win = sim_below(1000) < p;
		for (seat = 0; seat < count; seat++)
			results[seat] = win ? 1 : -1;
		points += ae_social_match_change(points, game, levels, results, count);
		if (game >= games - 300)
		{
			sum += ae_social_match_level(points) + 1;
			samples++;
		}
	}
	return (int)(sum / samples);
}

static void test_convergence(void)
{
	int level;

	/* a 50 % player stays, a weak one falls, a strong one rises: settles within 4 levels of his true strength */
	level = simulate(17, 1500);
	CHECK(level >= 13 && level <= 21);
	level = simulate(8, 1500);
	CHECK(level >= 4 && level <= 12);
	level = simulate(30, 1500);
	CHECK(level >= 26 && level <= 34);
	level = simulate(44, 3000);
	CHECK(level >= 40 && level <= 48);
}

static void test_roster(void)
{
	struct ae_social_shape shape = {2, 4}, ffa = {0, 8}, huge = {2, 9}, none = {0, 0};
	struct ae_social_roster a, b, fixed;
	int seat, team, order[AE_SOCIAL_SEATS], count, previous = 0, counts[2];

	/* a shape's seats */
	EQ(ae_social_shape_seats(&shape), 8); EQ(ae_social_shape_seats(&ffa), 8); EQ(ae_social_shape_seats(&huge), 0);
	EQ(ae_social_shape_seats(&none), 0);
	CHECK(!ae_social_roster_draw(&huge, 1, 1600, 0, 1, &a));
	CHECK(!ae_social_roster_draw(&shape, 0, 1600, 0, 1, &a));
	CHECK(!ae_social_roster_draw(&shape, 5, 1600, 0, 1, &a));
	CHECK(!ae_social_roster_draw(&shape, 1, 1600, 8, 1, &a));
	/* seeded repeat */
	CHECK(ae_social_roster_draw(&shape, 1, 1700, 0, 7, &a));
	CHECK(ae_social_roster_draw(&shape, 1, 1700, 0, 7, &b));
	CHECK(!memcmp(&a, &b, sizeof(a)));
	EQ(a.count, 8); EQ(a.humans, 1); EQ(a.teams, 2);
	CHECK(a.seat[0].human && !a.seat[1].human);
	EQ(a.player_tier, 7);
	/* every bot inside the window and within 2 tiers; the teams even, the human's team of 4 */
	counts[0] = counts[1] = 0;
	for (seat = 0; seat < a.count; seat++)
	{
		counts[a.seat[seat].team]++;
		if (!a.seat[seat].human)
		{
			int low, high;

			ae_social_match_window(ae_social_match_level(1700), &low, &high);
			CHECK(a.seat[seat].level >= low && a.seat[seat].level <= high);
			CHECK(a.seat[seat].tier >= a.player_tier - 2 && a.seat[seat].tier <= a.player_tier + 2);
			CHECK(a.seat[seat].shooting == ae_social_tier(a.seat[seat].tier)->shooting);
		}
	}
	EQ(counts[0], 4); EQ(counts[1], 4);
	/* join order: nearest search step first; the steps sit inside 0 .. span + 1; the closest tier joins at step 0 */
	count = ae_social_roster_join_order(&a, order);
	EQ(count, 7);
	for (seat = 0; seat < count; seat++)
	{
		CHECK(a.seat[order[seat]].join_step >= previous);
		previous = a.seat[order[seat]].join_step;
		CHECK(previous <= a.span + 1);
	}
	for (seat = 1; seat < a.count; seat++)
	{
		int distance = ae_social_tier_raw_level(a.seat[seat].tier) - ae_social_match_level(1700);

		if (distance < 0)
			distance = -distance;
		/* (a bot is in the range from the step that is its distance from the player, the window's edge aside) */
		CHECK(a.seat[seat].join_step <= distance + 1);
	}
	/* the spread over many seeds: mostly the player's tier, never past 2 */
	{
		int seed, same = 0, total = 0;

		for (seed = 1; seed <= 400; seed++)
		{
			CHECK(ae_social_roster_draw(&shape, 1, 1600, 0, (unsigned long)seed, &a));
			for (seat = 1; seat < a.count; seat++)
			{
				total++;
				same += a.seat[seat].tier == 7;
				CHECK(a.seat[seat].tier >= 5 && a.seat[seat].tier <= 9);
			}
		}
		CHECK(same * 100 / total >= 52 && same * 100 / total <= 68);
	}
	/* a free for all: each seat its own team; a party of 2 in a 2 x 2 stays together */
	CHECK(ae_social_roster_draw(&ffa, 1, 1600, 0, 3, &a));
	for (seat = 0; seat < a.count; seat++)
		EQ(a.seat[seat].team, seat);
	{
		struct ae_social_shape doubles = {2, 2};

		for (team = 1; team <= 50; team++)
		{
			CHECK(ae_social_roster_draw(&doubles, 2, 1600, 0, (unsigned long)team, &a));
			EQ(a.seat[0].team, a.seat[1].team);
			counts[0] = counts[1] = 0;
			for (seat = 0; seat < a.count; seat++)
				counts[a.seat[seat].team]++;
			EQ(counts[0], 2); EQ(counts[1], 2);
		}
	}
	/* FIXED: every bot at that skill's tier, no spread, all join at step 0 */
	CHECK(ae_social_roster_draw(&shape, 1, 1600, 4, 9, &fixed));
	EQ(fixed.fixed, 4);
	for (seat = 1; seat < fixed.count; seat++)
	{
		EQ(fixed.seat[seat].tier, 10); EQ(fixed.seat[seat].shooting, 4); EQ(fixed.seat[seat].movement, 4);
		EQ(fixed.seat[seat].join_step, 0);
	}
	/* the ends of the ladder: the window clips the draw */
	for (team = 1; team <= 200; team++)
	{
		CHECK(ae_social_roster_draw(&shape, 1, 0, 0, (unsigned long)team, &a));
		for (seat = 1; seat < a.count; seat++)
			CHECK(a.seat[seat].tier >= 1 && a.seat[seat].tier <= 3);
		CHECK(ae_social_roster_draw(&shape, 1, 4999, 0, (unsigned long)team, &a));
		for (seat = 1; seat < a.count; seat++)
			CHECK(a.seat[seat].tier >= 17 && a.seat[seat].tier <= 19);
	}
}

static void test_text(void)
{
	struct ae_social_shape shape = {2, 4}, ffa = {0, 4};
	struct ae_social_roster roster;
	char text[200];
	int length;

	CHECK(ae_social_roster_draw(&shape, 1, 1600, 3, 7, &roster));
	length = ae_social_roster_text(&roster, "team-slayer", 7, text, sizeof(text));
	CHECK(!strcmp(text, "roster team-slayer seed 7: you L17 T7; red T7 T7 T7; blue T7 T7 T7 T7"));
	EQ(length, (long)strlen(text));
	CHECK(ae_social_roster_draw(&ffa, 1, 1600, 4, 7, &roster));
	ae_social_roster_text(&roster, "free-for-all", 7, text, sizeof(text));
	CHECK(!strcmp(text, "roster free-for-all seed 7: you L17 T7; players T10 T10 T10"));
	/* cut to a small buffer, still a string */
	length = ae_social_roster_text(&roster, "free-for-all", 7, text, 12);
	EQ(length, 11); EQ(strlen(text), 11);
	EQ(ae_social_roster_text(&roster, "x", 1, text, 0), 0);
}

static void test_calibration(void)
{
	/* the three answers start at EASY, NORMAL (the old start) and HEROIC's same-skill tiers */
	EQ(ae_social_calibration_start(AE_SOCIAL_EXPERIENCE_NEW), ae_social_match_points_of_tier(4));
	EQ(ae_social_calibration_start(AE_SOCIAL_EXPERIENCE_SOME), AE_SOCIAL_MATCH_START);
	EQ(ae_social_calibration_start(AE_SOCIAL_EXPERIENCE_A_LOT), ae_social_match_points_of_tier(10));
	EQ(ae_social_calibration_start(AE_SOCIAL_EXPERIENCE_UNSET), AE_SOCIAL_MATCH_START);
	EQ(ae_social_calibration_start(99), AE_SOCIAL_MATCH_START);
	EQ(ae_social_match_tier(ae_social_calibration_start(AE_SOCIAL_EXPERIENCE_NEW)), 4);
	EQ(ae_social_match_tier(ae_social_calibration_start(AE_SOCIAL_EXPERIENCE_A_LOT)), 10);
	CHECK(!strcmp(ae_social_experience_name(AE_SOCIAL_EXPERIENCE_A_LOT), "A LOT"));
	CHECK(!strcmp(ae_social_experience_skill(AE_SOCIAL_EXPERIENCE_NEW), "EASY"));
	CHECK(!strcmp(ae_social_experience_skill(AE_SOCIAL_EXPERIENCE_UNSET), "NORMAL"));
	/* placement: kills minus deaths count, capped; after placement nothing is added */
	EQ(ae_social_placement_change(-100, 0, 2, 15), -100 - 13 * AE_SOCIAL_PLACEMENT_KD_POINTS);
	EQ(ae_social_placement_change(100, 4, 40, 0), 100 + AE_SOCIAL_PLACEMENT_KD_MAX);
	EQ(ae_social_placement_change(-100, 2, 0, 40), -100 - AE_SOCIAL_PLACEMENT_KD_MAX);
	EQ(ae_social_placement_change(50, AE_SOCIAL_PLACEMENT_GAMES, 30, 0), 50);
	EQ(ae_social_placement_change(0, 0, -3, -1), 0);
	/* a party: the average of its players */
	{
		long party[3] = {900, 1600, 2300};

		EQ(ae_social_party_points(party, 3), 1600);
		EQ(ae_social_party_points(party, 2), 1250);
		EQ(ae_social_party_points(party, 0), AE_SOCIAL_MATCH_START);
		EQ(ae_social_party_points(NULL, 2), AE_SOCIAL_MATCH_START);
	}
}

static void test_new_player_settles_low(void)
{
	/* a NEW player who loses every placement game 3-12 against tier 4 bots ends at a lower tier than started; a strong one
	   at SOME who wins 20-4 climbs at least two tiers in placement */
	long points = ae_social_calibration_start(AE_SOCIAL_EXPERIENCE_NEW), games;
	int level, result;

	for (games = 0; games < AE_SOCIAL_PLACEMENT_GAMES; games++)
	{
		long change;

		level = ae_social_tier_raw_level(ae_social_match_tier(points));
		result = -1;
		change = ae_social_match_change(points, games, &level, &result, 1);
		points = ae_social_match_apply(points, ae_social_placement_change(change, games, 3, 12));
	}
	CHECK(ae_social_match_tier(points) < 4);
	points = ae_social_calibration_start(AE_SOCIAL_EXPERIENCE_SOME);
	for (games = 0; games < AE_SOCIAL_PLACEMENT_GAMES; games++)
	{
		long change;

		level = ae_social_tier_raw_level(ae_social_match_tier(points));
		result = 1;
		change = ae_social_match_change(points, games, &level, &result, 1);
		points = ae_social_match_apply(points, ae_social_placement_change(change, games, 20, 4));
	}
	CHECK(ae_social_match_tier(points) >= 9);
}

int main(void)
{
	test_calibration();
	test_new_player_settles_low();
	test_ladder();
	test_window_holds_neighbours();
	test_change();
	test_convergence();
	test_roster();
	test_text();
	if (failures)
		return 1;
	printf("ae_social_match_rules_test: ok\n");
	return 0;
}
