/*
AE_MARK_RULES.C

Pure rules for MARKS (ae_mark_rules.h). No engine includes.
*/

#include "ae_mark_rules.h"

#include <math.h>

#define AE_MARK_SAME_PLACE_SQUARED 0.25f	/* half a world unit */

int ae_mark_press_step(struct ae_mark_press *press, int down, int taken, long now, int *light)
{
	int mark = 0;

	if (down && !press->down)
	{
		press->since = now;
		press->taken = taken;
		press->held = 0;
	}
	else if (!down && press->down)
		mark = !press->taken && !press->held;
	press->down = down;
	if (down && !press->taken && now - press->since >= AE_MARK_HOLD_TICKS)
		press->held = 1;
	*light = !down || press->taken || press->held;
	return mark;
}

float ae_mark_alpha(long age)
{
	if (age < 0 || age >= AE_MARK_LIFE_TICKS)
		return 0.0f;
	if (age < AE_MARK_APPEAR_TICKS)
		return (float)(age + 1) / (float)AE_MARK_APPEAR_TICKS;
	if (age < AE_MARK_FADE_AT_TICKS)
		return 1.0f;
	return (float)(AE_MARK_LIFE_TICKS - age) / (float)(AE_MARK_LIFE_TICKS - AE_MARK_FADE_AT_TICKS);
}

float ae_mark_appear_scale(long age)
{
	float left;

	if (age < 0 || age >= AE_MARK_APPEAR_TICKS)
		return 1.0f;
	left = 1.0f - (float)(age + 1) / (float)AE_MARK_APPEAR_TICKS;
	return 1.0f + 0.35f * left * left;
}

int ae_mark_clamp(float *x, float *y, float radius_x, float radius_y, int off_screen)
{
	float across = *x / radius_x;
	float along = *y / radius_y;
	float reach = across * across + along * along;

	if (!off_screen && reach < 1.0f)
		return 0;
	if (reach <= 0.0f)
	{
		/* (straight behind: below the view) */
		*x = 0.0f;
		*y = radius_y;
		return 1;
	}
	reach = 1.0f / sqrtf(reach);
	*x *= reach;
	*y *= reach;
	return 1;
}

int ae_mark_same(int kind_a, long object_a, int kind_b, long object_b, float distance_squared)
{
	if (kind_a != kind_b)
		return 0;
	if (kind_a == AE_MARK_KIND_PLACE)
		return distance_squared < AE_MARK_SAME_PLACE_SQUARED;
	return object_a == object_b;
}

int ae_mark_spam_take(struct ae_mark_spam *spam, long now)
{
	int index, inside = 1;

	if (spam->blocked)
	{
		if (now < spam->blocked_until)
			return AE_MARK_SPAM_STILL_BLOCKED;
		spam->blocked = 0;
		spam->used = 0;
		spam->next = 0;
	}
	for (index = 0; index < spam->used; index++)
		inside += now - spam->times[index] < AE_MARK_SPAM_WINDOW_TICKS;
	if (inside >= AE_MARK_SPAM_LIMIT)
	{
		spam->blocked = 1;
		spam->blocked_until = now + AE_MARK_SPAM_BLOCK_TICKS;
		return AE_MARK_SPAM_NOW_BLOCKED;
	}
	spam->times[spam->next] = now;
	spam->next = (spam->next + 1) % AE_MARK_SPAM_LIMIT;
	if (spam->used < AE_MARK_SPAM_LIMIT)
		spam->used++;
	return inside == AE_MARK_SPAM_WARN_AT ? AE_MARK_SPAM_WARN : AE_MARK_SPAM_OK;
}
