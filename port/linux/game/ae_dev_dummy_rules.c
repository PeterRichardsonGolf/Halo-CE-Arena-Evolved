/*
AE_DEV_DUMMY_RULES.C

DEV MODE's target dummies: spots, timers and bot choice (ae_dev_dummy_rules.h). No engine includes.
*/

#include <math.h>

#include "ae_dev_dummy_rules.h"

struct ae_dummy_spot ae_dummy_spot(int index, int count, float ox, float oy, float oz, float fx, float fy,
	float distance, int here)
{
	struct ae_dummy_spot spot;
	float length = sqrtf(fx * fx + fy * fy), side;

	if (length < 0.01f)
	{
		fx = 1.0f;
		fy = 0.0f;
		length = 1.0f;
	}
	fx /= length;
	fy /= length;
	side = ((float)index - (float)(count - 1) * 0.5f) * AE_DUMMY_SPACING;
	if (here)
		distance = 0.0f;
	/* (the left of the line of sight is (-fy, fx)) */
	spot.x = ox + fx * distance - fy * side;
	spot.y = oy + fy * distance + fx * side;
	spot.z = oz + 0.5f;
	/* (back at the player; at the player's own spot, the way the player does not look) */
	spot.yaw = atan2f(-fy, -fx);
	return spot;
}

int ae_dummy_heal_due(long hurt_tick, long now)
{
	return now - hurt_tick >= AE_DUMMY_HEAL_TICKS;
}

long ae_dummy_respawn_timer(long dead_tick, long now, long current)
{
	long left = AE_DUMMY_RESPAWN_TICKS - (now - dead_tick);

	if (left < 1)
		left = 1;
	return current < left ? current : left;
}

int ae_dummy_given_up(int present, int seen, long join_tick, long now)
{
	if (present)
		return 0;
	return seen || now - join_tick >= AE_DUMMY_JOIN_TICKS;
}

int ae_dummy_pick(unsigned char const *present, int maximum, int claimed, int count, int *out)
{
	int bot, found = 0;

	for (bot = 0; bot < maximum && found < count; bot++)
	{
		if (present[bot])
			continue;
		if (claimed > 0)
		{
			claimed--;
			continue;
		}
		out[found++] = bot;
	}
	return found;
}

int ae_dummy_leaver(unsigned char const *flag, unsigned char const *present, int maximum, int fallback)
{
	int bot;

	for (bot = 0; bot < maximum; bot++)
	{
		if (flag[bot] && present[bot])
			return bot;
	}
	return fallback;
}
