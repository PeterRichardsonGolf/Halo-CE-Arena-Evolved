/* ae_test_run_hide_rules.c: TEST RUN card hide presses (ae_test_run_hide_rules.h). */

#include "ae_test_run_hide_rules.h"

int ae_test_run_hide_update(struct ae_test_run_hide *state, int held, unsigned long now, int reveal)
{
	held = held != 0;
	if (held && !state->held)
		state->pressed_at = now;
	else if (!held && state->held)
	{
		if (!state->swallow && now - state->pressed_at < AE_TEST_RUN_HIDE_TOGGLE_MS)
			state->toggled = !state->toggled;
		state->swallow = 0;
	}
	state->held = held;
	if (reveal)
	{
		state->toggled = 0;
		state->swallow = held;
		return 0;
	}
	return state->held || state->toggled;
}
