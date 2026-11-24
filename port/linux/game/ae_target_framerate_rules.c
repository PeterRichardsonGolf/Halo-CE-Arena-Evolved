/*
AE_TARGET_FRAMERATE_RULES.C

TARGET FRAMERATE: the choices, the earlier boolean's carry-over and the rate held (ae_target_framerate_rules.h).
*/

#include <ctype.h>
#include <math.h>
#include <stdlib.h>

#include "ae_target_framerate_rules.h"

static const char *const words[] = { AE_TARGET_FRAMERATE_WORDS };

static int same_word(const char *a, const char *b)
{
	for (; *a && *b; a++, b++)
	{
		if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
			return 0;
	}
	return !*a && !*b;
}

int ae_target_framerate_choice(const char *word)
{
	int index;

	if (!word || !*word)
		return 0;
	for (index = 0; index < AE_TARGET_FRAMERATE_COUNT; index++)
	{
		if (!same_word(word, words[index]))
			continue;
		if (same_word(word, "display"))
			return AE_TARGET_FRAMERATE_DISPLAY;
		return atoi(words[index]);
	}
	return 0;
}

const char *ae_target_framerate_carry_over(int earlier_on)
{
	return AE_TARGET_FRAMERATE_CARRY_OVER(earlier_on);
}

float ae_target_framerate_hz(int choice, int vsync, float refresh_hz, long max_fps)
{
	float refresh = refresh_hz > 0.0f ? refresh_hz : AE_TARGET_FRAMERATE_UNKNOWN_HZ;
	float hz, reach;

	if (choice == AE_TARGET_FRAMERATE_DISPLAY)
		hz = refresh;
	else if (choice > 0)
		hz = (float)choice;
	else
		return 0.0f;
	/* (what the frames can reach: V-SYNC shows no more than the refresh; without it FRAME RATE LIMIT's cap) */
	if (vsync)
		reach = refresh;
	else if (max_fps > 0)
		reach = (float)max_fps;
	else if (max_fps == 0)
		reach = 2.0f * refresh;
	else
		reach = 0.0f;
	return reach > 0.0f && hz > reach ? reach : hz;
}

const char *ae_target_framerate_word(int choice)
{
	int index;

	if (choice == AE_TARGET_FRAMERATE_DISPLAY)
		return "display";
	if (choice <= 0)
		return "off";
	for (index = 1; index < AE_TARGET_FRAMERATE_COUNT; index++)
	{
		if (atoi(words[index]) == choice)
			return words[index];
	}
	return "off";
}

/* a target's frames a second, not clamped (0: off) */
static float wanted_hz(int choice, float refresh)
{
	return choice == AE_TARGET_FRAMERATE_DISPLAY ? refresh : choice > 0 ? (float)choice : 0.0f;
}

/* the frames a second a limit lets through (NONE: HUGE_VAL) */
static float limit_hz(long max_fps, float refresh)
{
	return max_fps < 0 ? (float)HUGE_VAL : max_fps == 0 ? 2.0f * refresh : (float)max_fps;
}

/* a target stepped down to its highest value at or under a limit (under the lowest: off) */
static int stepped_down(int choice, float refresh, float limit)
{
	int index, best = 0;

	if (wanted_hz(choice, refresh) <= limit)
		return choice;
	for (index = 1; index < AE_TARGET_FRAMERATE_COUNT; index++)
	{
		int value = atoi(words[index]);

		if (value > 0 && (float)value <= limit && value > best)
			best = value;
	}
	return best;
}

static int moved(struct ae_target_limit const *a, struct ae_target_limit const *b)
{
	return a->target != b->target || a->split_target != b->split_target || a->max_fps != b->max_fps || a->own != b->own;
}

int ae_target_limit_follow(enum ae_target_limit_change change, int vsync, float refresh_hz,
	struct ae_target_limit *state)
{
	float refresh = refresh_hz > 0.0f ? refresh_hz : AE_TARGET_FRAMERATE_UNKNOWN_HZ;
	struct ae_target_limit before = *state;
	float needed, limit;

	/* (a limit the player sets is their own, V-SYNC or not) */
	if (change == AE_TARGET_LIMIT_LIMIT)
		state->own = AE_TARGET_LIMIT_NO_OWN;
	if (vsync)
		return moved(&before, state);
	limit = limit_hz(state->max_fps, refresh);
	if (change == AE_TARGET_LIMIT_LIMIT)
	{
		state->target = stepped_down(state->target, refresh, limit);
		state->split_target = stepped_down(state->split_target, refresh, limit);
	}
	else
	{
		needed = wanted_hz(state->target, refresh);
		if (wanted_hz(state->split_target, refresh) > needed)
			needed = wanted_hz(state->split_target, refresh);
		if (needed > limit)
		{
			/* (raised: the player's own limit kept, once) */
			if (state->own == AE_TARGET_LIMIT_NO_OWN)
				state->own = state->max_fps;
			state->max_fps = (long)ceilf(needed);
		}
		else if (state->own != AE_TARGET_LIMIT_NO_OWN && change != AE_TARGET_LIMIT_VSYNC)
		{
			/* (lowered: back toward the player's own, no lower than the targets need) */
			if (limit_hz(state->own, refresh) >= needed)
			{
				state->max_fps = state->own;
				state->own = AE_TARGET_LIMIT_NO_OWN;
			}
			else
				state->max_fps = (long)ceilf(needed);
		}
	}
	return moved(&before, state);
}
