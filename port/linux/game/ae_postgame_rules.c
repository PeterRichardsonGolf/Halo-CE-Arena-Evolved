/*
AE_POSTGAME_RULES.C

Arena Evolved: the carnage report's confirm, its decisions (ae_postgame.h),
pure, so tools/harness/tests/test_ae_postgame.py takes them as they are.

The stock code reads held buttons each frame (game_engine.c's
test_any_gamepad_button): a press is a run of frames down. So the confirm
waits for both buttons up after each press it takes: the B that opened it
does not close it the next frame, and the A or B that answered it does not
reach the stock code (A the host's pick game, B leaving at once) or open it
again.
*/

#include "ae_postgame.h"

enum ae_postgame_action ae_postgame_confirm_step(
	struct ae_postgame_confirm *state,
	boolean accept_down,
	boolean back_down)
{
	if (state->wait_release)
	{
		if (accept_down || back_down)
			return AE_POSTGAME_HELD;
		state->wait_release = FALSE;
	}
	if (!state->open)
	{
		/* (A first, as the stock code reads them: pick game; B asks first) */
		if (accept_down || !back_down)
			return AE_POSTGAME_STOCK;
		state->open = TRUE;
		state->wait_release = TRUE;
		return AE_POSTGAME_HELD;
	}
	if (accept_down)
	{
		state->open = FALSE;
		state->wait_release = TRUE;
		return AE_POSTGAME_LEAVE;
	}
	if (back_down)
	{
		state->open = FALSE;
		state->wait_release = TRUE;
	}
	return AE_POSTGAME_HELD;
}

short ae_postgame_number_shift(short header_width, short number_width)
{
	return header_width > number_width ? (short)(header_width - number_width) : 0;
}
