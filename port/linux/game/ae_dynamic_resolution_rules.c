/*
AE_DYNAMIC_RESOLUTION_RULES.C

TARGET 60 FPS: the scale's rules (ae_dynamic_resolution_rules.h).
*/

#include <math.h>

#include "ae_dynamic_resolution_rules.h"

static int clamp_percent(int percent)
{
	return percent < AE_DYNRES_FLOOR ? AE_DYNRES_FLOOR : percent > AE_DYNRES_FULL ? AE_DYNRES_FULL : percent;
}

void ae_dynres_reset(struct ae_dynres_state *state)
{
	state->percent = AE_DYNRES_FULL;
	state->slow_frames = 0;
	state->fast_frames = 0;
	state->settle_frames = 0;
}

int ae_dynres_fitting_percent(int percent, float frame_ms)
{
	float fitting;
	int stepped;

	if (!(frame_ms > 0.0f))
		return clamp_percent(percent);
	fitting = (float)percent * sqrtf(AE_DYNRES_SLOW_MS / frame_ms);
	/* (down to a whole step) */
	stepped = ((int)fitting / AE_DYNRES_STEP) * AE_DYNRES_STEP;
	return clamp_percent(stepped);
}

int ae_dynres_frame(struct ae_dynres_state *state, int on, int views, float frame_ms)
{
	if (!on || on != state->on || views != state->views)
	{
		ae_dynres_reset(state);
		state->on = on;
		state->views = views;
		if (on)
			state->settle_frames = AE_DYNRES_START_FRAMES;
		return state->percent;
	}
	if (state->settle_frames > 0)
	{
		state->settle_frames--;
		return state->percent;
	}
	if (!(frame_ms >= 0.0f) || frame_ms >= AE_DYNRES_HITCH_MS)
	{
		state->slow_frames = 0;
		state->fast_frames = 0;
		return state->percent;
	}
	if (frame_ms > AE_DYNRES_SLOW_MS)
	{
		state->fast_frames = 0;
		if (++state->slow_frames >= AE_DYNRES_SLOW_FRAMES && state->percent > AE_DYNRES_FLOOR)
		{
			int fitting = ae_dynres_fitting_percent(state->percent, frame_ms);

			state->percent = clamp_percent(fitting < state->percent ? fitting : state->percent - AE_DYNRES_STEP);
			state->slow_frames = 0;
			state->settle_frames = AE_DYNRES_SETTLE_FRAMES;
		}
		return state->percent;
	}
	state->slow_frames = 0;
	if (frame_ms < AE_DYNRES_FAST_MS)
	{
		if (++state->fast_frames >= AE_DYNRES_FAST_FRAMES && state->percent < AE_DYNRES_FULL)
		{
			state->percent = clamp_percent(state->percent + AE_DYNRES_STEP);
			state->fast_frames = 0;
			state->settle_frames = AE_DYNRES_SETTLE_FRAMES;
		}
	}
	else
	{
		/* (between: neither, and the run of fast frames starts again) */
		state->fast_frames = 0;
	}
	return state->percent;
}

int ae_dynres_forced(long value)
{
	if (value <= 0)
		return 0;
	return value >= AE_DYNRES_FULL ? AE_DYNRES_FULL : value < AE_DYNRES_FLOOR ? AE_DYNRES_FLOOR : (int)value;
}
