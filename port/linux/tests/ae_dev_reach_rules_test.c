#include <stdio.h>
#include <string.h>

#include "ae_dev_reach_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static struct ae_reach_plan plan;

/* starts merge within 8, goals within 1.5; each goal from every start not beside it, goal by goal */
static void plan_tests(void)
{
	int index;

	ae_reach_clear(&plan);
	CHECK(ae_reach_add_start(&plan, 0.0f, 0.0f, 0.0f) && ae_reach_add_start(&plan, 50.0f, 0.0f, 0.0f));
	CHECK(!ae_reach_add_start(&plan, 5.0f, 3.0f, 0.0f) && plan.start_count == 2 && !strcmp(plan.starts[1].name, "spawn 2"));
	CHECK(ae_reach_add_goal(&plan, 25.0f, 0.0f, 1.0f, "red flag") && ae_reach_add_goal(&plan, 3.0f, 0.0f, 0.0f, "sniper"));
	CHECK(!ae_reach_add_goal(&plan, 25.5f, 0.5f, 1.0f, "again") && plan.goal_count == 2);
	/* (the sniper is beside spawn 1: walked only from spawn 2) */
	CHECK(ae_reach_make_pairs(&plan) == 3);
	CHECK(plan.pair_goal[0] == 0 && plan.pair_start[0] == 0 && plan.pair_goal[1] == 0 && plan.pair_start[1] == 1);
	CHECK(plan.pair_goal[2] == 1 && plan.pair_start[2] == 1);
	/* (caps) */
	ae_reach_clear(&plan);
	for (index = 0; index < AE_REACH_STARTS + 3; index++)
		ae_reach_add_start(&plan, (float)index * 20.0f, 0.0f, 0.0f);
	for (index = 0; index < AE_REACH_GOALS + 3; index++)
		ae_reach_add_goal(&plan, (float)index * 20.0f + 10.0f, 100.0f, 0.0f, "item");
	CHECK(plan.start_count == AE_REACH_STARTS && plan.goal_count == AE_REACH_GOALS);
	CHECK(ae_reach_make_pairs(&plan) == AE_REACH_PAIRS);
	/* (the time: 20 s and 1 s a unit, 120 s at most) */
	{
		float a[3] = { 0.0f, 0.0f, 0.0f }, b[3] = { 30.0f, 40.0f, 0.0f }, far_away[3] = { 500.0f, 0.0f, 0.0f };

		CHECK(ae_reach_walk_ticks(a, b) == 70 * AE_REACH_TICKS);
		CHECK(ae_reach_walk_ticks(a, far_away) == 120 * AE_REACH_TICKS);
	}
}

/* a walk's outcomes from its samples */
static void walk_tests(void)
{
	float start[3] = { 0.0f, 0.0f, 0.0f }, goal[3] = { 20.0f, 0.0f, 0.0f };
	struct ae_reach_walk walk;
	char line[160];
	long tick;
	int outcome = AE_REACH_GOING;

	/* (walks straight there at 2 u/s: reached in 9.5 s; a spot above it, under 2 up, is the goal's) */
	ae_reach_walk_begin(&walk, start, goal, 100);
	for (tick = 101; outcome == AE_REACH_GOING && tick < 2000; tick++)
		outcome = ae_reach_walk_sample(&walk, start, goal, (float)(tick - 100) * (2.0f / AE_REACH_TICKS), 0.0f, 1.5f, 1, tick);
	CHECK(outcome == AE_REACH_REACHED && walk.end_tick - walk.start_tick == 278);
	CHECK(ae_reach_line(line, (int)sizeof line, "spawn 1", "red flag", &walk) > 0 &&
		!strcmp(line, "spawn 1 -> red flag: reached in 9 s"));
	CHECK(ae_reach_walk_sample(&walk, start, goal, 0.0f, 0.0f, 0.0f, 0, tick) == AE_REACH_REACHED);
	/* (stands at 5 u: stuck after 10 s still) */
	ae_reach_walk_begin(&walk, start, goal, 0);
	outcome = AE_REACH_GOING;
	for (tick = 1; outcome == AE_REACH_GOING && tick < 2000; tick++)
		outcome = ae_reach_walk_sample(&walk, start, goal, tick < 60 ? (float)tick * 0.1f : 5.9f, 0.0f, 0.0f, 1, tick);
	CHECK(outcome == AE_REACH_STUCK && walk.end_tick == 45 + 300 && walk.at[0] == 5.9f);
	CHECK(ae_reach_line(line, (int)sizeof line, "spawn 2", "hill 1", &walk) > 0 &&
		!strcmp(line, "spawn 2 -> hill 1: stuck at (5.9 0.0 0.0) after 12 s, 14.1 u short at best"));
	/* (walks a circle that never comes closer: lost after 30 s) */
	ae_reach_walk_begin(&walk, start, goal, 0);
	outcome = AE_REACH_GOING;
	for (tick = 1; outcome == AE_REACH_GOING && tick < 4000; tick++)
	{
		float x = (tick / 60) % 2 ? -3.0f : -6.0f;

		outcome = ae_reach_walk_sample(&walk, start, goal, x, 0.0f, 0.0f, 1, tick);
	}
	CHECK(outcome == AE_REACH_LOST && walk.end_tick == 900);
	/* (comes closer slowly, past its time: slow) */
	ae_reach_walk_begin(&walk, start, goal, 0);
	outcome = AE_REACH_GOING;
	for (tick = 1; outcome == AE_REACH_GOING && tick < 4000; tick++)
		outcome = ae_reach_walk_sample(&walk, start, goal, (float)tick * 0.012f, (tick / 60) % 2 ? 1.0f : -1.0f, 0.0f, 1,
			tick);
	CHECK(outcome == AE_REACH_SLOW && walk.end_tick == ae_reach_walk_ticks(start, goal));
	/* (died) */
	ae_reach_walk_begin(&walk, start, goal, 0);
	CHECK(ae_reach_walk_sample(&walk, start, goal, 3.0f, 0.0f, 0.0f, 1, 10) == AE_REACH_GOING);
	CHECK(ae_reach_walk_sample(&walk, start, goal, 0.0f, 0.0f, 0.0f, 0, 11) == AE_REACH_DIED && walk.at[0] == 3.0f);
	CHECK(!strcmp(ae_reach_outcome_name(AE_REACH_DIED), "died") && !strcmp(ae_reach_outcome_name(99), "?"));
}

int main(void)
{
	plan_tests();
	walk_tests();
	if (failures)
		printf("%d failures\n", failures);
	else
		printf("ae_dev_reach_rules: ok\n");
	return failures != 0;
}
