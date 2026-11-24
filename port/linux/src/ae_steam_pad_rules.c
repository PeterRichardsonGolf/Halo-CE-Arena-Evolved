/*
AE_STEAM_PAD_RULES.C

The Steam Controller's arithmetic (ae_steam_pad_rules.h). No SDL here.
*/

#include <math.h>
#include <stddef.h>
#include <string.h>

#include "ae_steam_pad_rules.h"

int ae_steam_pad_rules_kind(unsigned int vendor, unsigned int product)
{
	if (vendor != AE_STEAM_PAD_VENDOR)
		return AE_STEAM_PAD_NONE;
	if (product == AE_STEAM_PAD_PRODUCT_STEAM_INPUT)
		return AE_STEAM_PAD_STEAM_INPUT;
	if (product == AE_STEAM_PAD_PRODUCT_WIRED || product == AE_STEAM_PAD_PRODUCT_BLUETOOTH ||
		product == AE_STEAM_PAD_PRODUCT_RECEIVER || product == AE_STEAM_PAD_PRODUCT_RECEIVER_2)
		return AE_STEAM_PAD_CONTROLLER;
	return AE_STEAM_PAD_OTHER_VALVE;
}

const char *ae_steam_pad_rules_mode(unsigned int product)
{
	switch (product)
	{
	case AE_STEAM_PAD_PRODUCT_WIRED: return "WIRED";
	case AE_STEAM_PAD_PRODUCT_BLUETOOTH: return "BLUETOOTH";
	case AE_STEAM_PAD_PRODUCT_RECEIVER:
	case AE_STEAM_PAD_PRODUCT_RECEIVER_2: return "PUCK";
	default: return NULL;
	}
}

int ae_steam_pad_rules_world(int kind, int behind_is_steam_controller, int no_access)
{
	if (kind == AE_STEAM_PAD_CONTROLLER)
		return AE_STEAM_WORLD_DIRECT;
	if (kind == AE_STEAM_PAD_STEAM_INPUT)
		return behind_is_steam_controller ? AE_STEAM_WORLD_STEAM_INPUT : AE_STEAM_WORLD_STEAM_INPUT_OTHER;
	return no_access ? AE_STEAM_WORLD_NO_ACCESS : AE_STEAM_WORLD_NONE;
}

int ae_steam_pad_rules_with_access(int world, int present, int no_access)
{
	return world == AE_STEAM_WORLD_NONE && !present && no_access ? AE_STEAM_WORLD_NO_ACCESS : world;
}

int ae_steam_pad_rules_trackpad_shown(int world)
{
	return world == AE_STEAM_WORLD_DIRECT || world == AE_STEAM_WORLD_STEAM_INPUT;
}

int ae_steam_pad_rules_rows_live(int world)
{
	return world == AE_STEAM_WORLD_DIRECT;
}

int ae_steam_pad_rules_gyro_greyed(int world, int pad_has_gyro)
{
	return world == AE_STEAM_WORLD_STEAM_INPUT && !pad_has_gyro;
}

/* a new reading's weight in the finger's speed: the step over the step and this many seconds */
#define SPEED_SMOOTHING 0.03

static void smooth_step(struct ae_trackpad_rules *state, float *turn_x, float *turn_y, double seconds, int finger);

void ae_trackpad_rules_step(
	struct ae_trackpad_rules *state,
	int down,
	float x,
	float y,
	double seconds,
	float sensitivity,
	float *yaw,
	float *pitch)
{
	float turn_x = 0.0f, turn_y = 0.0f, rest_x = 0.0f, rest_y = 0.0f;

	*yaw = 0.0f;
	*pitch = 0.0f;
	if (!(seconds > 0.0))
		return;
	if (seconds > AE_TRACKPAD_RULES_LONGEST_STEP)
	{
		/* (a read after a pause: nothing carries over) */
		memset(state, 0, sizeof(*state));
		if (!down)
			return;
		seconds = AE_TRACKPAD_RULES_LONGEST_STEP;
	}
	if (down)
	{
		state->glide_x = 0.0f;
		state->glide_y = 0.0f;
		if (!state->down)
		{
			/* (a landing: the dead zone's centre, no turn) */
			state->down = 1;
			state->moving = 0;
			state->anchor_x = state->last_x = x;
			state->anchor_y = state->last_y = y;
			state->speed_x = 0.0f;
			state->speed_y = 0.0f;
			state->still = 0.0;
			state->pending_x = 0.0f;
			state->pending_y = 0.0f;
			return;
		}
		if (fabsf(x - state->last_x) > AE_TRACKPAD_RULES_JUMP || fabsf(y - state->last_y) > AE_TRACKPAD_RULES_JUMP)
		{
			/* (too far for one read: the finger lifted and landed again, a new landing) */
			state->moving = 0;
			state->anchor_x = state->last_x = x;
			state->anchor_y = state->last_y = y;
			state->speed_x = 0.0f;
			state->speed_y = 0.0f;
			state->still = 0.0;
			state->pending_x = 0.0f;
			state->pending_y = 0.0f;
			return;
		}
		if (!state->moving)
		{
			float dx = x - state->anchor_x, dy = y - state->anchor_y;

			state->last_x = x;
			state->last_y = y;
			if (fabsf(dx) < AE_TRACKPAD_RULES_DEAD_ZONE && fabsf(dy) < AE_TRACKPAD_RULES_DEAD_ZONE)
			{
				state->still += seconds;
				return;
			}
			/* (out of the dead zone: what is beyond it counts, per axis) */
			state->moving = 1;
			state->still = 0.0;
			turn_x = fabsf(dx) > AE_TRACKPAD_RULES_DEAD_ZONE ? dx - copysignf(AE_TRACKPAD_RULES_DEAD_ZONE, dx) : 0.0f;
			turn_y = fabsf(dy) > AE_TRACKPAD_RULES_DEAD_ZONE ? dy - copysignf(AE_TRACKPAD_RULES_DEAD_ZONE, dy) : 0.0f;
			state->speed_x = (float)(turn_x / seconds);
			state->speed_y = (float)(turn_y / seconds);
		}
		else
		{
			float dx = x - state->last_x, dy = y - state->last_y;
			float weight = (float)(seconds / (seconds + SPEED_SMOOTHING));

			turn_x = dx;
			turn_y = dy;
			state->speed_x += ((float)(dx / seconds) - state->speed_x) * weight;
			state->speed_y += ((float)(dy / seconds) - state->speed_y) * weight;
			if (dx != 0.0f || dy != 0.0f)
				state->still = 0.0;
			else
				state->still += seconds;
			state->last_x = x;
			state->last_y = y;
		}
	}
	else if (state->down)
	{
		/* (the finger left: a fast one that was moving glides on) */
		float speed = hypotf(state->speed_x, state->speed_y);

		state->down = 0;
		if (state->moving && state->still < AE_TRACKPAD_RULES_REST && speed > AE_TRACKPAD_RULES_FLICK)
		{
			state->glide_x = state->speed_x;
			state->glide_y = state->speed_y;
		}
		state->moving = 0;
		state->speed_x = 0.0f;
		state->speed_y = 0.0f;
	}
	if (down)
		smooth_step(state, &turn_x, &turn_y, seconds, 1);
	else if (state->pending_x != 0.0f || state->pending_y != 0.0f)
	{
		/* (a lifted finger's travel not turned yet: turned now, at the slow pace) */
		rest_x = 0.0f;
		rest_y = 0.0f;
		smooth_step(state, &rest_x, &rest_y, seconds, 0);
	}
	if (!down && (state->glide_x != 0.0f || state->glide_y != 0.0f))
	{
		float keep = (float)exp(-seconds / AE_TRACKPAD_RULES_GLIDE);

		/* (the distance the decaying speed covers in the step, whatever the step) */
		turn_x = state->glide_x * (float)(AE_TRACKPAD_RULES_GLIDE * (1.0 - exp(-seconds / AE_TRACKPAD_RULES_GLIDE)));
		turn_y = state->glide_y * (float)(AE_TRACKPAD_RULES_GLIDE * (1.0 - exp(-seconds / AE_TRACKPAD_RULES_GLIDE)));
		state->glide_x *= keep;
		state->glide_y *= keep;
		if (hypotf(state->glide_x, state->glide_y) < AE_TRACKPAD_RULES_GLIDE_END)
		{
			state->glide_x = 0.0f;
			state->glide_y = 0.0f;
		}
	}
	turn_x += rest_x;
	turn_y += rest_y;
	*yaw = -turn_x * AE_TRACKPAD_RULES_TURN * sensitivity;
	*pitch = -turn_y * AE_TRACKPAD_RULES_TURN * sensitivity;
}

/* the share of the finger's travel turned now: the travel waiting plus this read's, a part of it by the finger's speed
(see AE_TRACKPAD_RULES_SMOOTH_SLOW); the rest waits for the next read. A lifted finger's rest is turned at the slow pace */
static void smooth_step(struct ae_trackpad_rules *state, float *turn_x, float *turn_y, double seconds, int finger)
{
	float speed = finger ? hypotf(*turn_x, *turn_y) / (float)seconds : 0.0f;
	double smooth, share;

	state->pending_x += *turn_x;
	state->pending_y += *turn_y;
	if (speed >= AE_TRACKPAD_RULES_FAST)
		smooth = AE_TRACKPAD_RULES_SMOOTH_FAST;
	else if (speed <= AE_TRACKPAD_RULES_SLOW)
		smooth = AE_TRACKPAD_RULES_SMOOTH_SLOW;
	else
		smooth = AE_TRACKPAD_RULES_SMOOTH_SLOW + (AE_TRACKPAD_RULES_SMOOTH_FAST - AE_TRACKPAD_RULES_SMOOTH_SLOW) *
			(double)((speed - AE_TRACKPAD_RULES_SLOW) / (AE_TRACKPAD_RULES_FAST - AE_TRACKPAD_RULES_SLOW));
	share = 1.0 - exp(-seconds / smooth);
	*turn_x = (float)(state->pending_x * share);
	*turn_y = (float)(state->pending_y * share);
	state->pending_x -= *turn_x;
	state->pending_y -= *turn_y;
	/* (a rest too small to see is dropped) */
	if (fabsf(state->pending_x) < 1e-6f && fabsf(state->pending_y) < 1e-6f)
	{
		state->pending_x = 0.0f;
		state->pending_y = 0.0f;
	}
}
