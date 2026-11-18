/*
AE_BACK_PRESSES.C

Mouse button 4's presses for AE's menus (ae_platform.h): counted by
sdl_platform.c's pointer branch (an AE hook line), only while the game's side
has armed the count, that is while an AE screen is open. Upstream's own menus
then never leave a count behind for the next AE screen, and with
display.arena_menus off the count is never armed. Pure C (no SDL), so the unit
tests build it (port/linux/tests/ae_input_rules_test.c). Both sides run on the
game's thread: sdl_platform.c's events are pumped from XInputGetState and
Present.
*/

#include "ae_platform.h"

int ae_platform_tab_press_kind(int key_repeat, int shift, int alt, int gui)
{
	/* (the key's own repeats are AE's repeat's business; Alt+Tab and Super+Tab are the desktop's) */
	if (key_repeat)
		return AE_TAB_PRESS_NONE;
	if (alt || gui)
		return AE_TAB_PRESS_IGNORED;
	return shift ? AE_TAB_PRESS_BACKWARD : AE_TAB_PRESS_FORWARD;
}

static int back_presses_armed;
static int back_presses;

void ae_platform_count_back_press(void)
{
	if (back_presses_armed)
		back_presses++;
}

void ae_platform_arm_back_presses(int armed)
{
	back_presses_armed = armed != 0;
	if (!back_presses_armed)
		back_presses = 0;
}

int ae_platform_take_back_presses(void)
{
	int presses = back_presses;

	back_presses = 0;
	return presses;
}

int ae_input_pointer_motion_counts(struct ae_input_pointer_motion *state, float x, float y, unsigned long now_ms)
{
	float dx, dy;

	if (!state->started)
	{
		state->started = 1;
		state->anchor_x = state->last_x = x;
		state->anchor_y = state->last_y = y;
		state->last_ms = now_ms;
		state->engaged = 0;
		return 0;
	}
	if (state->engaged && now_ms - state->last_ms > AE_INPUT_POINTER_REST_MS)
		state->engaged = 0;
	if (!state->engaged && now_ms - state->last_ms > AE_INPUT_POINTER_REST_MS)
	{
		/* (rested: the dead zone is round where it rested) */
		state->anchor_x = state->last_x;
		state->anchor_y = state->last_y;
	}
	state->last_x = x;
	state->last_y = y;
	state->last_ms = now_ms;
	if (state->engaged)
		return 1;
	dx = x - state->anchor_x;
	dy = y - state->anchor_y;
	if (dx * dx + dy * dy >= (float)(AE_INPUT_POINTER_DEAD_ZONE_PIXELS * AE_INPUT_POINTER_DEAD_ZONE_PIXELS))
	{
		state->engaged = 1;
		return 1;
	}
	return 0;
}
