#include <stdio.h>
#include <string.h>
#include "ae_bots_vantage_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static struct ae_vantage_spot spot_at(float x, float y, float z, float first, unsigned long sees, short open, float height)
{
	struct ae_vantage_spot spot;

	memset(&spot, 0, sizeof(spot));
	spot.x = x;
	spot.y = y;
	spot.z = z;
	spot.first_score = first;
	spot.sees = sees;
	spot.open = open;
	spot.height = height;
	return spot;
}

int main(void)
{
	struct ae_vantage_place places[3] = { { 0.0f, 0.0f, 0.0f, 3.0f }, { 40.0f, 0.0f, 0.0f, 2.0f }, { 200.0f, 0.0f, 0.0f, 1.0f } };
	struct ae_vantage_spot candidates[AE_VANTAGE_CANDIDATES], chosen[AE_VANTAGE_MAXIMUM + 2], spot;
	struct ae_vantage_grid grid;
	int count = 0, index;

	/* the distance band: a little very near, rising to 12, full to 50, falling to 90, none beyond */
	CHECK(ae_vantage_range(0.0f) == 0.2f);
	CHECK(ae_vantage_range(4.9f) == 0.2f);
	CHECK(ae_vantage_range(12.0f) == 1.0f);
	CHECK(ae_vantage_range(30.0f) == 1.0f);
	CHECK(ae_vantage_range(49.9f) == 1.0f);
	CHECK(ae_vantage_range(70.0f) > 0.6f && ae_vantage_range(70.0f) < 0.7f);
	CHECK(ae_vantage_range(89.9f) < 0.32f);
	CHECK(ae_vantage_range(90.0f) == 0.0f);
	CHECK(ae_vantage_range(8.5f) > 0.2f && ae_vantage_range(8.5f) < 1.0f);

	/* the first score: height pinned from -2 to 8, a quarter of the places' weight by the band */
	CHECK(ae_vantage_first_score(20.0f, places, 0, 0.0f, 0.0f, 0.0f) == 8.0f);
	CHECK(ae_vantage_first_score(-9.0f, places, 0, 0.0f, 0.0f, 0.0f) == -2.0f);
	CHECK(ae_vantage_first_score(0.0f, places, 3, 20.0f, 0.0f, 0.0f) == 0.25f * (3.0f + 2.0f));
	CHECK(ae_vantage_first_score(0.0f, places, 3, 20.0f, 0.0f, 0.0f) > ae_vantage_first_score(0.0f, places, 3, 300.0f, 0.0f, 0.0f));

	/* candidates: 6 units apart, the better of two near each other, 30 at most and the best of them */
	spot = spot_at(10.0f, 0.0f, 0.0f, 1.0f, 0, 0, 0.0f);
	ae_vantage_consider(candidates, &count, &spot);
	spot = spot_at(14.0f, 0.0f, 0.0f, 2.0f, 0, 0, 0.0f);
	ae_vantage_consider(candidates, &count, &spot);
	CHECK(count == 1 && candidates[0].first_score == 2.0f);
	spot = spot_at(15.0f, 0.0f, 0.0f, 1.5f, 0, 0, 0.0f);
	ae_vantage_consider(candidates, &count, &spot);
	CHECK(count == 1 && candidates[0].first_score == 2.0f);
	spot = spot_at(21.0f, 0.0f, 0.0f, 3.0f, 0, 0, 0.0f);
	ae_vantage_consider(candidates, &count, &spot);
	CHECK(count == 2);
	for (index = 0; index < 60; index++)
	{
		spot = spot_at(100.0f + 10.0f * (float)index, 0.0f, 0.0f, (float)index, 0, 0, 0.0f);
		ae_vantage_consider(candidates, &count, &spot);
	}
	CHECK(count == AE_VANTAGE_CANDIDATES);
	{
		float lowest = 1.0e9f;

		for (index = 0; index < count; index++)
			if (candidates[index].first_score < lowest)
				lowest = candidates[index].first_score;
		/* (the 30 best of the 62 offered are kept: nothing under 30) */
		CHECK(lowest >= 30.0f);
	}

	/* the final score: the places seen by weight and distance, height pinned 0 to 6, openness of eight */
	spot = spot_at(20.0f, 0.0f, 0.0f, 0.0f, 3UL, 8, 10.0f);
	ae_vantage_score(&spot, places, 3);
	CHECK(spot.sees_count == 2);
	CHECK(spot.score > 0.8f * 6.0f + 2.5f + 3.0f * 1.0f + 2.0f * 1.0f - 0.001f);
	CHECK(spot.score < 0.8f * 6.0f + 2.5f + 3.0f * 1.0f + 2.0f * 1.0f + 0.001f);
	spot = spot_at(20.0f, 0.0f, 0.0f, 0.0f, 0UL, 4, -3.0f);
	ae_vantage_score(&spot, places, 3);
	CHECK(spot.sees_count == 0 && spot.score > 1.24f && spot.score < 1.26f);

	/* the choice: two places seen at least, 12 units apart, 6 at most, best first */
	{
		struct ae_vantage_spot pool[10];
		int chosen_count;

		for (index = 0; index < 10; index++)
		{
			pool[index] = spot_at(100.0f * (float)index, 0.0f, 0.0f, 0.0f, 3UL, 8, 0.0f);
			pool[index].sees_count = 2;
			pool[index].score = 10.0f + (float)index;
		}
		/* a spot that sees one place only is never chosen, whatever its score */
		pool[9].sees_count = 1;
		pool[9].score = 99.0f;
		/* two near each other (5 apart): the better of them only */
		pool[8].x = pool[7].x + 5.0f;
		pool[8].score = 12.5f;
		chosen_count = ae_vantage_choose(pool, 10, chosen);
		CHECK(chosen_count == AE_VANTAGE_MAXIMUM);
		CHECK(chosen[0].score == 17.0f);
		CHECK(chosen[0].score >= chosen[1].score && chosen[1].score >= chosen[2].score);
		for (index = 0; index < chosen_count; index++)
		{
			CHECK(chosen[index].sees_count >= 2);
			CHECK(chosen[index].score != 12.5f);
		}
		/* none seeing two places: none */
		for (index = 0; index < 10; index++)
			pool[index].sees_count = 1;
		CHECK(ae_vantage_choose(pool, 10, chosen) == 0);
	}

	/* the height grid: a spot on a hill is high over its surroundings, one in a valley is low; outside the grid, 0 */
	CHECK(ae_vantage_grid_begin(&grid, 0.0f, 0.0f, 100.0f, 100.0f, 8.0f, 128));
	for (index = 0; index < 100; index++)
	{
		int other;

		for (other = 0; other < 100; other++)
			ae_vantage_grid_add(&grid, (float)index, (float)other, index > 40 && index < 50 ? 6.0f : 0.0f, 1.0f);
	}
	CHECK(ae_vantage_grid_height(&grid, 45.0f, 50.0f, 6.0f) > 2.0f);
	CHECK(ae_vantage_grid_height(&grid, 90.0f, 50.0f, 0.0f) == 0.0f);
	CHECK(ae_vantage_grid_height(&grid, 500.0f, 500.0f, 3.0f) == 0.0f);
	ae_vantage_grid_end(&grid);

	/* the weapon for the holds: a rocket launcher heavy, a sniper rifle long, another precision rifle a rifle's (a third cap), any other none */
	CHECK(ae_vantage_weapon_class(0, 0, 0) == AE_VANTAGE_WEAPON_NONE);
	CHECK(ae_vantage_weapon_class(1, 0, 0) == AE_VANTAGE_WEAPON_LONG);
	CHECK(ae_vantage_weapon_class(0, 1, 0) == AE_VANTAGE_WEAPON_RIFLE);
	CHECK(ae_vantage_weapon_class(1, 1, 0) == AE_VANTAGE_WEAPON_LONG);
	CHECK(ae_vantage_cap(9, AE_VANTAGE_WEAPON_RIFLE) == 3);
	CHECK(ae_vantage_weapon_class(0, 0, 1) == AE_VANTAGE_WEAPON_HEAVY);

	/* the caps: a third of the team's bots on spots, a half with long weapons, one at least; none for a bot with neither weapon */
	CHECK(ae_vantage_cap(9, AE_VANTAGE_WEAPON_HEAVY) == 3);
	CHECK(ae_vantage_cap(9, AE_VANTAGE_WEAPON_LONG) == 4);
	CHECK(ae_vantage_cap(2, AE_VANTAGE_WEAPON_HEAVY) == 1);
	CHECK(ae_vantage_cap(1, AE_VANTAGE_WEAPON_LONG) == 1);
	CHECK(ae_vantage_may_take(0, 8, AE_VANTAGE_WEAPON_LONG));
	CHECK(ae_vantage_may_take(3, 8, AE_VANTAGE_WEAPON_LONG));
	CHECK(!ae_vantage_may_take(4, 8, AE_VANTAGE_WEAPON_LONG));
	CHECK(ae_vantage_may_take(1, 8, AE_VANTAGE_WEAPON_HEAVY));
	CHECK(!ae_vantage_may_take(2, 8, AE_VANTAGE_WEAPON_HEAVY));
	CHECK(!ae_vantage_may_take(0, 8, AE_VANTAGE_WEAPON_NONE));
	CHECK(!ae_vantage_may_take(0, 0, AE_VANTAGE_WEAPON_LONG));

	/* HIGH GROUND WHEN FREE: any other weapon holds as ANY (bots.high_ground on), a third of the team, 10 to 20 s; off, or no weapon: none */
	CHECK(ae_vantage_free_class(AE_VANTAGE_WEAPON_NONE, 1, 1) == AE_VANTAGE_WEAPON_ANY);
	CHECK(ae_vantage_free_class(AE_VANTAGE_WEAPON_NONE, 1, 0) == AE_VANTAGE_WEAPON_NONE);
	CHECK(ae_vantage_free_class(AE_VANTAGE_WEAPON_NONE, 0, 1) == AE_VANTAGE_WEAPON_NONE);
	CHECK(ae_vantage_free_class(AE_VANTAGE_WEAPON_LONG, 1, 1) == AE_VANTAGE_WEAPON_LONG);
	CHECK(ae_vantage_free_class(AE_VANTAGE_WEAPON_HEAVY, 1, 1) == AE_VANTAGE_WEAPON_HEAVY);
	CHECK(ae_vantage_cap(9, AE_VANTAGE_WEAPON_ANY) == 3);
	CHECK(ae_vantage_may_take(2, 9, AE_VANTAGE_WEAPON_ANY) && !ae_vantage_may_take(3, 9, AE_VANTAGE_WEAPON_ANY));
	CHECK(ae_vantage_hold_ticks_for(AE_VANTAGE_WEAPON_ANY, 0.0f, 30) == 300);
	CHECK(ae_vantage_hold_ticks_for(AE_VANTAGE_WEAPON_ANY, 1.0f, 30) == 600);
	CHECK(ae_vantage_hold_ticks_for(AE_VANTAGE_WEAPON_LONG, 0.0f, 30) == ae_vantage_hold_ticks(0.0f, 30));

	/* the pick: the best by score and nearness, never a spot taken, the last, one failed, or too far */
	{
		float scores[4] = { 20.0f, 18.0f, 16.0f, 10.0f }, near[4] = { 10.0f, 10.0f, 10.0f, 10.0f }, noise[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

		CHECK(ae_vantage_pick(4, scores, near, 0UL, -1, 0UL, 70.0f, noise) == 0);
		CHECK(ae_vantage_pick(4, scores, near, 1UL, -1, 0UL, 70.0f, noise) == 1);
		CHECK(ae_vantage_pick(4, scores, near, 0UL, 0, 0UL, 70.0f, noise) == 1);
		CHECK(ae_vantage_pick(4, scores, near, 0UL, -1, 3UL, 70.0f, noise) == 2);
		CHECK(ae_vantage_pick(4, scores, near, 7UL, -1, 8UL, 70.0f, noise) == -1);
		/* nearness: a spot 60 units further costs six points */
		near[0] = 70.0f;
		CHECK(ae_vantage_pick(4, scores, near, 0UL, -1, 0UL, 70.0f, noise) == 1);
		near[0] = 71.0f;
		CHECK(ae_vantage_pick(4, scores, near, 0UL, -1, 0UL, 70.0f, noise) == 1);
		/* the only spot of the map may be the last one held */
		CHECK(ae_vantage_pick(1, scores, near, 0UL, 0, 0UL, 100.0f, noise) == 0);
		/* its own noise breaks near ties, deterministically */
		near[0] = 10.0f;
		noise[1] = 2.5f;
		CHECK(ae_vantage_pick(4, scores, near, 0UL, -1, 0UL, 70.0f, noise) == 1);
	}

	/* a hold lasts 30 to 60 seconds */
	CHECK(ae_vantage_hold_ticks(0.0f, 30) == 900);
	CHECK(ae_vantage_hold_ticks(1.0f, 30) == 1800);
	CHECK(ae_vantage_hold_ticks(-4.0f, 30) == 900);
	CHECK(ae_vantage_hold_ticks(9.0f, 30) == 1800);

	/* leaving: its weapon changed, flanked, an enemy within 8 units, hurt below half, low on ammunition, time up; else it stays */
	CHECK(ae_vantage_leave(AE_VANTAGE_WEAPON_LONG, AE_VANTAGE_WEAPON_LONG, 0, 40.0f, 2.0f, 0, 100, 900) == AE_VANTAGE_STAY);
	CHECK(ae_vantage_leave(AE_VANTAGE_WEAPON_NONE, AE_VANTAGE_WEAPON_LONG, 0, 40.0f, 2.0f, 0, 100, 900) == AE_VANTAGE_LEFT_WEAPON);
	CHECK(ae_vantage_leave(AE_VANTAGE_WEAPON_LONG, AE_VANTAGE_WEAPON_LONG, 1, 40.0f, 2.0f, 0, 100, 900) == AE_VANTAGE_LEFT_FLANKED);
	CHECK(ae_vantage_leave(AE_VANTAGE_WEAPON_LONG, AE_VANTAGE_WEAPON_LONG, 0, 7.9f, 2.0f, 0, 100, 900) == AE_VANTAGE_LEFT_ENEMY);
	CHECK(ae_vantage_leave(AE_VANTAGE_WEAPON_LONG, AE_VANTAGE_WEAPON_LONG, 0, 8.0f, 2.0f, 0, 100, 900) == AE_VANTAGE_STAY);
	CHECK(ae_vantage_leave(AE_VANTAGE_WEAPON_LONG, AE_VANTAGE_WEAPON_LONG, 0, -1.0f, 2.0f, 0, 100, 900) == AE_VANTAGE_STAY);
	CHECK(ae_vantage_leave(AE_VANTAGE_WEAPON_LONG, AE_VANTAGE_WEAPON_LONG, 0, 40.0f, 0.99f, 0, 100, 900) == AE_VANTAGE_LEFT_HURT);
	CHECK(ae_vantage_leave(AE_VANTAGE_WEAPON_LONG, AE_VANTAGE_WEAPON_LONG, 0, 40.0f, 2.0f, 1, 100, 900) == AE_VANTAGE_LEFT_AMMO);
	CHECK(ae_vantage_leave(AE_VANTAGE_WEAPON_LONG, AE_VANTAGE_WEAPON_LONG, 0, 40.0f, 2.0f, 0, 899, 900) == AE_VANTAGE_STAY);
	CHECK(ae_vantage_leave(AE_VANTAGE_WEAPON_LONG, AE_VANTAGE_WEAPON_LONG, 0, 40.0f, 2.0f, 0, 900, 900) == AE_VANTAGE_LEFT_TIME);
	/* (the weapon and the flank come before the rest: a bot hurt and flanked leaves flanked) */
	CHECK(ae_vantage_leave(AE_VANTAGE_WEAPON_LONG, AE_VANTAGE_WEAPON_LONG, 1, 3.0f, 0.2f, 1, 999, 900) == AE_VANTAGE_LEFT_FLANKED);
	CHECK(ae_vantage_ammunition_low(3, 24) == 1);
	CHECK(ae_vantage_ammunition_low(6, 24) == 0);
	CHECK(ae_vantage_ammunition_low(0, 0) == 0);
	CHECK(!strcmp(ae_vantage_leave_name(AE_VANTAGE_LEFT_FLANKED), "flanked"));
	CHECK(!strcmp(ae_vantage_leave_name(99), "?"));
	/* (0.9.0: a power item took the goal: a short rest, then back to a spot; any other reason the full rest) */
	CHECK(!strcmp(ae_vantage_leave_name(AE_VANTAGE_LEFT_POWER), "power"));
	CHECK(ae_vantage_rest_ticks(AE_VANTAGE_LEFT_POWER, 30) == 3 * 30);
	CHECK(ae_vantage_rest_ticks(AE_VANTAGE_LEFT_OBJECTIVE, 30) == AE_VANTAGE_REST_SECONDS * 30);
	CHECK(ae_vantage_rest_ticks(AE_VANTAGE_LEFT_TIME, 30) == AE_VANTAGE_REST_SECONDS * 30);
	CHECK(!strcmp(ae_vantage_leave_name(AE_VANTAGE_LEFT_DIED), "died"));
	/* (a bot with any weapon takes only the high ground near it) */
	CHECK(ae_vantage_max_distance(AE_VANTAGE_WEAPON_ANY) == 25.0f);
	/* (an enemy near: a sniper leaves, a bot with any weapon fights from the spot; hurt, it leaves either way) */
	CHECK(ae_vantage_leave(AE_VANTAGE_WEAPON_ANY, AE_VANTAGE_WEAPON_ANY, 0, 3.0f, 2.0f, 0, 100, 900) == AE_VANTAGE_STAY);
	CHECK(ae_vantage_leave(AE_VANTAGE_WEAPON_ANY, AE_VANTAGE_WEAPON_ANY, 0, 3.0f, 0.9f, 0, 100, 900) == AE_VANTAGE_LEFT_HURT);
	CHECK(ae_vantage_leave(AE_VANTAGE_WEAPON_ANY, AE_VANTAGE_WEAPON_ANY, 1, 3.0f, 2.0f, 0, 100, 900) == AE_VANTAGE_LEFT_FLANKED);
	CHECK(ae_vantage_max_distance(AE_VANTAGE_WEAPON_LONG) == AE_VANTAGE_MAX_DISTANCE);
	CHECK(ae_vantage_max_distance(AE_VANTAGE_WEAPON_HEAVY) == AE_VANTAGE_MAX_DISTANCE);

	return failures ? 1 : (printf("ae_bots_vantage_rules: all passed\n"), 0);
}
