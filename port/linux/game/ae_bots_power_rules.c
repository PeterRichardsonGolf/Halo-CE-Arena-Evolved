/*
AE_BOTS_POWER_RULES.C

Arena Evolved: the bots' power items (ae_bots_power_rules.h).
*/

#include <math.h>
#include "ae_bots_power_rules.h"
#include "ae_bots_combat_rules.h"

float ae_bots_power_reach(
	int level)
{
	static const float reach[AE_BOTS_LEVELS] = { 8.0f, 12.0f, 25.0f, 45.0f, 57.5f, 70.0f, 1.0e9f };

	return level >= 0 && level < AE_BOTS_LEVELS ? reach[level] : 0.0f;
}

float ae_bots_power_lead_seconds(
	int level)
{
	static const float lead[AE_BOTS_LEVELS] = { 0.0f, 0.0f, 0.0f, 5.0f, 7.5f, 10.0f, 15.0f };

	return level >= 0 && level < AE_BOTS_LEVELS ? lead[level] : 0.0f;
}

float ae_bots_power_value(
	int kind)
{
	static const float value[AE_BOTS_POWER_KINDS] = { 10.0f, 9.0f, 6.0f, 10.0f, 8.0f, 8.0f, 5.0f };

	return kind >= 0 && kind < AE_BOTS_POWER_KINDS ? value[kind] : 0.0f;
}

float ae_bots_power_want(
	struct ae_bots_power_view const *view)
{
	float reach, travel;

	if (!view || view->known_taken || view->holds_it || view->teammate_going || ae_bots_power_value(view->kind) <= 0.0f)
		return 0.0f;
	reach = ae_bots_power_reach(view->level);
	if (view->fighting && reach > AE_BOTS_POWER_FIGHTING_REACH)
		reach = AE_BOTS_POWER_FIGHTING_REACH;
	if (view->distance > reach)
		return 0.0f;
	if (!view->up)
	{
		float lead = ae_bots_power_lead_seconds(view->level);

		travel = view->distance / AE_BOTS_POWER_RUN_SPEED;
		/* (not up: only a level that times spawns, and not before it would arrive its lead early) */
		if (lead <= 0.0f || view->seconds_left > travel + lead)
			return 0.0f;
	}
	return ae_bots_power_value(view->kind) / (1.0f + view->distance / 20.0f);
}

int ae_bots_power_mount_applies(
	float rise,
	float across)
{
	return rise >= AE_BOTS_POWER_MOUNT_RISE_MIN && rise <= AE_BOTS_POWER_MOUNT_RISE_MAX &&
		across <= AE_BOTS_POWER_MOUNT_RANGE;
}

void ae_bots_power_mount_point(
	float item_x,
	float item_y,
	float from_x,
	float from_y,
	int shift,
	float *x,
	float *y)
{
	static const float axis_x[4] = { 1.0f, 0.0f, -1.0f, 0.0f };
	static const float axis_y[4] = { 0.0f, 1.0f, 0.0f, -1.0f };
	float dx = from_x - item_x, dy = from_y - item_y;
	int axis;

	/* (the axis the bot is nearest to: east, north, west or south; a bot on a diagonal takes east or west) */
	if (dx * dx >= dy * dy)
		axis = dx >= 0.0f ? 0 : 2;
	else
		axis = dy >= 0.0f ? 1 : 3;
	axis = (axis + (shift % 4 + 4) % 4) % 4;
	*x = item_x + axis_x[axis] * AE_BOTS_POWER_MOUNT_STAGING;
	*y = item_y + axis_y[axis] * AE_BOTS_POWER_MOUNT_STAGING;
}

int ae_bots_power_mount_in_lane(
	float x,
	float y,
	float axis_x,
	float axis_y)
{
	float along = x * axis_x + y * axis_y;
	float lateral = x * axis_y - y * axis_x;

	if (lateral < 0.0f)
		lateral = -lateral;
	return along >= 1.0f && lateral <= AE_BOTS_POWER_MOUNT_LANE;
}

int ae_bots_power_mount_jump(
	float across,
	float velocity_x,
	float velocity_y,
	float to_item_x,
	float to_item_y)
{
	float speed = (float)sqrt(velocity_x * velocity_x + velocity_y * velocity_y);
	float length = (float)sqrt(to_item_x * to_item_x + to_item_y * to_item_y);

	if (across > AE_BOTS_POWER_MOUNT_JUMP_ACROSS || across < AE_BOTS_POWER_MOUNT_JUMP_NEAREST || speed < 0.02f ||
		length <= 0.0f)
	{
		return 0;
	}
	return (velocity_x * to_item_x + velocity_y * to_item_y) / (speed * length) >= 0.95f;
}

float ae_bots_power_shoot_reach(
	int level,
	float weapon_range)
{
	float reach = weapon_range * AE_BOTS_POWER_SHOOT_REACH_SCALE;
	float level_reach = ae_bots_power_reach(level);

	if (reach < AE_BOTS_POWER_SHOOT_REACH_LEAST)
		reach = AE_BOTS_POWER_SHOOT_REACH_LEAST;
	return reach < level_reach ? reach : level_reach;
}

int ae_bots_power_shoot_wanted(
	int level,
	float distance,
	float weapon_range,
	float calm_seconds,
	float seconds_trying)
{
	return calm_seconds >= AE_BOTS_POWER_SHOOT_CALM_SECONDS && seconds_trying < AE_BOTS_POWER_SHOOT_GIVE_UP_SECONDS &&
		distance <= ae_bots_power_shoot_reach(level, weapon_range);
}
