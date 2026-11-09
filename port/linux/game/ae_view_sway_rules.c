/*
AE_VIEW_SWAY_RULES.C

The first-person weapon's sway springs stepped by any time (ae_view_sway_rules.h). No engine includes.
*/

#include <math.h>

#include "ae_view_sway_rules.h"

void ae_view_sway_step(float *position, float *velocity, float target, float acceleration, float speed, float seconds,
	float lower, float upper)
{
	/* (the stock step, in a second's units: a tick's velocity change of acceleration is a second's change of
	acceleration * 30 over a tick, the second's acceleration acceleration * 900; a tick's speed is a second's speed * 30;
	the braking speed square_root(2 * a * distance) is the same in either) */
	float const ticks = AE_VIEW_SWAY_TICKS_PER_SECOND;
	float accel = acceleration * ticks * ticks;
	float most = speed * ticks;
	float delta = target - *position;
	float step_limit, snap, wanted, change, moved;

	if (!(seconds > 0.0f))
		return;
	if (seconds > AE_VIEW_SWAY_LONGEST_STEP)
		seconds = AE_VIEW_SWAY_LONGEST_STEP;
	step_limit = accel * seconds;
	/* (close enough to stop: the stock rule, the gap less the velocity's step within the smaller of the two limits) */
	snap = fminf(step_limit * seconds, most * seconds);
	if (fabsf(delta - *velocity * seconds) <= snap)
	{
		*velocity = 0.0f;
		*position = target < lower ? lower : target > upper ? upper : target;
		return;
	}
	wanted = 2.0f * accel * fabsf(delta) >= most * most ? most : sqrtf(2.0f * accel * fabsf(delta));
	if (delta < 0.0f)
		wanted = -wanted;
	change = wanted - *velocity;
	if (change > step_limit)
		change = step_limit;
	else if (change < -step_limit)
		change = -step_limit;
	*velocity += change;
	/* (the stock position step: the new velocity and half the change, a tick's worth, scaled to the step) */
	moved = *position + (*velocity + change * 0.5f) * seconds;
	*position = moved < lower ? lower : moved > upper ? upper : moved;
}

float ae_view_sway_turn_target(float radians, float seconds)
{
	float target;

	if (!(seconds > 0.0f))
		return 0.0f;
	target = radians / seconds;
	return target < -1.0f ? -1.0f : target > 1.0f ? 1.0f : target;
}

float ae_view_sway_strength(float percent)
{
	if (!(percent >= 0.0f))
		return 0.0f;
	return percent > 100.0f ? 1.0f : percent / 100.0f;
}

void ae_view_sway_speed(float multiplier, float *acceleration_scale, float *speed_scale)
{
	if (!(multiplier >= 1.0f))
		multiplier = 1.0f;
	else if (multiplier > 8.0f)
		multiplier = 8.0f;
	*acceleration_scale = multiplier * multiplier;
	*speed_scale = multiplier;
}
