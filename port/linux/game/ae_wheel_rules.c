/*
AE_WHEEL_RULES.C

Pure rules for the CALLOUT WHEEL (ae_wheel_rules.h). No engine includes.
*/

#include "ae_wheel_rules.h"
#include "ae_mark_rules.h"

#include <math.h>
#include <stdlib.h>

#define WHEEL_ENTRY_DEGREES (360.0f / AE_WHEEL_COUNT)

int ae_wheel_press_step(struct ae_wheel_press *press, int down, int interrupted, long now, int focus)
{
	int result = AE_PRESS_NOTHING;

	if (interrupted)
	{
		if (press->open)
			result = AE_PRESS_CANCEL;
		press->open = 0;
		press->pending = 0;
		press->second = 0;
		/* (pause and the menus zero the input: a press held into them is still held, until a tick after them says not) */
		press->ignore = press->ignore || press->down || down;
		press->down = down;
		return result;
	}
	if (press->ignore)
	{
		press->ignore = down;
		press->down = down;
		return AE_PRESS_NOTHING;
	}
	if (down)
	{
		if (!press->down)
		{
			press->since = now;
			press->open = 0;
			press->second = press->pending && now - press->released <= AE_WHEEL_DOUBLE_TICKS;
		}
		press->down = 1;
		if (!press->open && now - press->since >= AE_MARK_HOLD_TICKS)
		{
			press->open = 1;
			press->pending = 0;
			result = AE_PRESS_OPEN;
		}
		return result;
	}
	if (!press->down)
		return AE_PRESS_NOTHING;
	press->down = 0;
	if (press->open)
	{
		press->open = 0;
		return focus != AE_WHEEL_NONE ? AE_PRESS_SEND : AE_PRESS_CANCEL;
	}
	if (press->second)
	{
		press->second = 0;
		press->pending = 0;
		return AE_PRESS_DOUBLE;
	}
	press->pending = 1;
	press->released = now;
	return AE_PRESS_TAP;
}

/* the entry at (x, y) from the middle, y up, when it is at least travel out */
static int wheel_sector(int current, float x, float y, float travel)
{
	float degrees, off;

	if (!(x * x + y * y >= travel * travel))
		return AE_WHEEL_NONE;
	degrees = atan2f(x, y) * (180.0f / 3.14159265f);
	if (degrees < 0.0f)
		degrees += 360.0f;
	if (current >= 0 && current < AE_WHEEL_COUNT)
	{
		off = fabsf(degrees - WHEEL_ENTRY_DEGREES * (float)current);
		if (off > 180.0f)
			off = 360.0f - off;
		if (off <= WHEEL_ENTRY_DEGREES * 0.5f + AE_WHEEL_STICKY_DEGREES)
			return current;
	}
	return (int)((degrees + WHEEL_ENTRY_DEGREES * 0.5f) / WHEEL_ENTRY_DEGREES) % AE_WHEEL_COUNT;
}

int ae_wheel_pick(int current, float x, float y)
{
	return wheel_sector(current, x, y, AE_WHEEL_STICK_TRAVEL);
}

void ae_wheel_mouse_add(float *x, float *y, float dx, float dy)
{
	float distance;

	*x += dx;
	*y += dy;
	distance = sqrtf(*x * *x + *y * *y);
	if (distance > AE_WHEEL_MOUSE_RADIUS)
	{
		*x *= AE_WHEEL_MOUSE_RADIUS / distance;
		*y *= AE_WHEEL_MOUSE_RADIUS / distance;
	}
}

int ae_wheel_pick_mouse(int current, float x, float y)
{
	return wheel_sector(current, x, y, AE_WHEEL_MOUSE_FOCUS);
}

int ae_wheel_focus_held(struct ae_wheel_focus *state, int picked, int from_stick, long now)
{
	if (picked != AE_WHEEL_NONE)
	{
		state->held = 1;
		state->entry = picked;
		state->lost = 0;
		return picked;
	}
	if (!state->held)
		return AE_WHEEL_NONE;
	if (from_stick)
	{
		if (!state->lost)
		{
			state->lost = 1;
			state->lost_at = now;
		}
		if (now - state->lost_at < AE_WHEEL_GRACE_TICKS)
			return state->entry;
	}
	state->held = 0;
	state->lost = 0;
	return AE_WHEEL_NONE;
}

int ae_wheel_entry_shown(int entry, int dead)
{
	if (entry < 0 || entry >= AE_WHEEL_COUNT)
		return 0;
	return !dead || entry == AE_WHEEL_SMART || entry == AE_WHEEL_ENEMY || entry == AE_WHEEL_GOING;
}

int ae_wheel_legend_shown(int calls_sent, int split, int hints_on)
{
	return hints_on && !split && calls_sent < AE_WHEEL_LEGEND_CALLS;
}

int ae_wheel_calls_after(int calls_sent)
{
	if (calls_sent < 0)
		calls_sent = 0;
	return calls_sent + 1 > AE_WHEEL_LEGEND_CALLS ? AE_WHEEL_LEGEND_CALLS : calls_sent + 1;
}

int ae_wheel_calls_from_text(char const *text)
{
	char *end;
	long value;

	if (!text || !text[0])
		return 0;
	value = strtol(text, &end, 10);
	if (end == text)
		return 0;
	return value < 0 ? 0 : value > AE_WHEEL_LEGEND_CALLS ? AE_WHEEL_LEGEND_CALLS : (int)value;
}
