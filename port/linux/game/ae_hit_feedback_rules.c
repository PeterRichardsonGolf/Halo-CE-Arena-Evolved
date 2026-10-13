/*
AE_HIT_FEEDBACK_RULES.C

HIT FEEDBACK's rules (ae_hit_feedback_rules.h): pure, no engine includes.
The numbers are the ones approved in play (2026-10-12).
*/

#include <math.h>
#include <stddef.h>

#include "ae_hit_feedback_rules.h"

static float pin(float value, float low, float high)
{
	return value < low ? low : value > high ? high : value;
}

int ae_hit_feedback_value(unsigned char byte)
{
	return (byte & AE_HIT_FEEDBACK_BYTE_MASK) ? AE_HIT_FEEDBACK_ARENA : AE_HIT_FEEDBACK_CLASSIC;
}

unsigned char ae_hit_feedback_byte_set(unsigned char byte, int value)
{
	return (unsigned char)((byte & ~AE_HIT_FEEDBACK_BYTE_MASK) | (value == AE_HIT_FEEDBACK_ARENA ? 1u : 0u));
}

char const *ae_hit_feedback_name(int value)
{
	return value == AE_HIT_FEEDBACK_ARENA ? "arena evolved" : "halo ce";
}

int ae_hit_feedback_pop_due(float shield_points_left, float finisher_points)
{
	/* (as the game reckons a hit on a shield: what is left over after the shield is what reaches the body) */
	return finisher_points > 0.f && finisher_points - shield_points_left > 0.f;
}

int ae_hit_feedback_pop_shape(float t, float *strength, float *top)
{
	if (!(t >= 0.f && t < 1.f))
		return 0;
	if (strength)
		*strength = 1.f;
	/* (white all over, then the white comes off from the head down) */
	if (top)
		*top = t < AE_HIT_FEEDBACK_POP_FLASH ? 1.f : 1.f - (t - AE_HIT_FEEDBACK_POP_FLASH) / (1.f - AE_HIT_FEEDBACK_POP_FLASH);
	return 1;
}

void ae_hit_feedback_band(float shield_level, float *front, float *tail)
{
	float level = pin(shield_level, 0.f, 1.f);

	if (front)
		*front = level;
	if (tail)
		*tail = pin(level - AE_HIT_FEEDBACK_BAND_WIDTH, 0.f, 1.f);
}

void ae_hit_feedback_band_color(float shield_level, float *rgb)
{
	float level = pin(shield_level, 0.f, 1.f);

	if (!rgb)
		return;
	/* (white at the feet to a pale yellow at the head) */
	rgb[0] = 1.f;
	rgb[1] = 1.f - 0.05f * level;
	rgb[2] = 1.f - 0.45f * level;
}

float ae_hit_feedback_full_flash(float seconds_since_full)
{
	float u = seconds_since_full / AE_HIT_FEEDBACK_FULL_FLASH_SECONDS;

	return u >= 0.f && u < 1.f ? 1.f - u : 0.f;
}

float ae_hit_feedback_arms_swell(float shield_level)
{
	/* (the arms sit a little over half way up the body) */
	float swell = 1.f - (float)fabs((double)(pin(shield_level, 0.f, 1.f) - 0.55f)) / 0.28f;

	return swell > 0.f ? swell : 0.f;
}

float ae_hit_feedback_hit_pulse(float game_value)
{
	float value = pin(game_value, 0.f, 1.f);

	return value * value * value;
}

int ae_hit_feedback_flicker(unsigned long unit, float seconds, float *height, float *strength)
{
	/* (14 moments a second, a band in a little over a fifth of them: about 3 a second, each unit's its own) */
	unsigned long moment = seconds > 0.f ? (unsigned long)(seconds * 14.f) : 0;
	unsigned long hash = ((unit & 0xFFFFFFFFul) * 2654435761ul) ^ (moment * 40503ul);

	hash &= 0xFFFFFFFFul;
	hash ^= hash >> 13;
	hash = (hash * 0x5bd1e995ul) & 0xFFFFFFFFul;
	hash ^= hash >> 15;
	if (hash % 100ul >= 22ul)
		return 0;
	if (height)
		*height = (float)((hash >> 8) % 100ul) / 100.f;
	if (strength)
		*strength = 0.8f * (0.45f + 0.55f * (float)((hash >> 16) % 100ul) / 100.f);
	return 1;
}
