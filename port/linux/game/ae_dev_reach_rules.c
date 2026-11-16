/*
AE_DEV_REACH_RULES.C

DEV MODE's can't-reach report, as pure rules (ae_dev_reach_rules.h).
*/

#include "ae_dev_reach_rules.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static float distance3(float const *a, float const *b)
{
	float dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];

	return (float)sqrt(dx * dx + dy * dy + dz * dz);
}

static float across(float const *a, float const *b)
{
	float dx = a[0] - b[0], dy = a[1] - b[1];

	return (float)sqrt(dx * dx + dy * dy);
}

void ae_reach_clear(struct ae_reach_plan *plan)
{
	memset(plan, 0, sizeof *plan);
}

int ae_reach_add_start(struct ae_reach_plan *plan, float x, float y, float z)
{
	float at[3];
	int index;
	struct ae_reach_point *start;

	at[0] = x;
	at[1] = y;
	at[2] = z;
	for (index = 0; index < plan->start_count; index++)
	{
		if (distance3(plan->starts[index].at, at) <= AE_REACH_MERGE)
			return 0;
	}
	if (plan->start_count >= AE_REACH_STARTS)
		return 0;
	start = &plan->starts[plan->start_count++];
	memcpy(start->at, at, sizeof at);
	snprintf(start->name, sizeof start->name, "spawn %d", plan->start_count);
	return 1;
}

int ae_reach_add_goal(struct ae_reach_plan *plan, float x, float y, float z, char const *name)
{
	float at[3];
	int index;
	struct ae_reach_point *goal;

	at[0] = x;
	at[1] = y;
	at[2] = z;
	for (index = 0; index < plan->goal_count; index++)
	{
		if (distance3(plan->goals[index].at, at) <= AE_REACH_SAME)
			return 0;
	}
	if (plan->goal_count >= AE_REACH_GOALS)
		return 0;
	goal = &plan->goals[plan->goal_count++];
	memcpy(goal->at, at, sizeof at);
	snprintf(goal->name, sizeof goal->name, "%s", name ? name : "goal");
	return 1;
}

int ae_reach_make_pairs(struct ae_reach_plan *plan)
{
	int goal, start;

	plan->pair_count = 0;
	for (goal = 0; goal < plan->goal_count; goal++)
	{
		for (start = 0; start < plan->start_count && plan->pair_count < AE_REACH_PAIRS; start++)
		{
			if (distance3(plan->starts[start].at, plan->goals[goal].at) <= AE_REACH_MERGE)
				continue;
			plan->pair_start[plan->pair_count] = (unsigned char)start;
			plan->pair_goal[plan->pair_count] = (unsigned char)goal;
			plan->pair_count++;
		}
	}
	return plan->pair_count;
}

long ae_reach_walk_ticks(float const *start, float const *goal)
{
	float seconds = AE_REACH_BASE_SECONDS + distance3(start, goal) / AE_REACH_PACE;

	if (seconds > AE_REACH_MOST_SECONDS)
		seconds = AE_REACH_MOST_SECONDS;
	return (long)(seconds * AE_REACH_TICKS);
}

void ae_reach_walk_begin(struct ae_reach_walk *walk, float const *start, float const *goal, long now)
{
	memset(walk, 0, sizeof *walk);
	walk->outcome = AE_REACH_GOING;
	walk->start_tick = walk->closer_tick = walk->moved_tick = now;
	walk->best = walk->mark = distance3(start, goal);
	memcpy(walk->moved_from, start, 3 * sizeof(float));
	memcpy(walk->at, start, 3 * sizeof(float));
}

static int end(struct ae_reach_walk *walk, int outcome, long now)
{
	walk->outcome = outcome;
	walk->end_tick = now;
	return outcome;
}

int ae_reach_walk_sample(struct ae_reach_walk *walk, float const *start, float const *goal, float x, float y, float z,
	int alive, long now)
{
	float distance;

	if (walk->outcome != AE_REACH_GOING)
		return walk->outcome;
	/* (died: where it was last seen living) */
	if (!alive)
		return end(walk, AE_REACH_DIED, now);
	walk->at[0] = x;
	walk->at[1] = y;
	walk->at[2] = z;
	distance = distance3(walk->at, goal);
	if (across(walk->at, goal) <= AE_REACH_AT && fabs(walk->at[2] - goal[2]) <= AE_REACH_AT_UP)
	{
		if (distance < walk->best)
			walk->best = distance;
		return end(walk, AE_REACH_REACHED, now);
	}
	if (distance < walk->best)
		walk->best = distance;
	if (distance <= walk->mark - AE_REACH_PROGRESS)
	{
		walk->mark = distance;
		walk->closer_tick = now;
	}
	if (distance3(walk->at, walk->moved_from) >= AE_REACH_MOVED)
	{
		memcpy(walk->moved_from, walk->at, sizeof walk->at);
		walk->moved_tick = now;
	}
	if (now - walk->moved_tick >= (long)(AE_REACH_STILL_SECONDS * AE_REACH_TICKS))
		return end(walk, AE_REACH_STUCK, now);
	if (now - walk->closer_tick >= (long)(AE_REACH_LOST_SECONDS * AE_REACH_TICKS))
		return end(walk, AE_REACH_LOST, now);
	if (now - walk->start_tick >= ae_reach_walk_ticks(start, goal))
		return end(walk, AE_REACH_SLOW, now);
	return AE_REACH_GOING;
}

char const *ae_reach_outcome_name(int outcome)
{
	static char const *const names[AE_REACH_OUTCOMES] = { "going", "reached", "stuck", "lost", "slow", "died" };

	return outcome >= 0 && outcome < AE_REACH_OUTCOMES ? names[outcome] : "?";
}

int ae_reach_line(char *text, int size, char const *start_name, char const *goal_name, struct ae_reach_walk const *walk)
{
	float seconds = (float)(walk->end_tick - walk->start_tick) / AE_REACH_TICKS;
	int wrote;

	if (walk->outcome == AE_REACH_REACHED)
		wrote = snprintf(text, (size_t)size, "%s -> %s: reached in %.0f s", start_name, goal_name, (double)seconds);
	else
		wrote = snprintf(text, (size_t)size, "%s -> %s: %s at (%.1f %.1f %.1f) after %.0f s, %.1f u short at best",
			start_name, goal_name, ae_reach_outcome_name(walk->outcome), (double)walk->at[0], (double)walk->at[1],
			(double)walk->at[2], (double)seconds, (double)walk->best);
	return wrote < 0 ? 0 : (wrote >= size ? size - 1 : wrote);
}
