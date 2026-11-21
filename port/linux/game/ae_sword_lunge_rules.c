/* ae_sword_lunge_rules.c: the energy sword lunge's pure carry/strike choices. */

#include <math.h>
#include <string.h>

#include "ae_sword_lunge_rules.h"

#define AE_SWORD_LUNGE_PI 3.14159265358979323846f

enum ae_sword_lunge_style ae_sword_lunge_style_from_text(char const *text)
{
	return text && !strcmp(text, "strike") ? _ae_sword_lunge_style_strike : _ae_sword_lunge_style_carry;
}

short ae_sword_lunge_duration_ticks(enum ae_sword_lunge_style style, float distance, float speed)
{
	/* (the carry: at most 12 ticks, 0.4 s, as the latest-gen lunge's short dash; the strike style keeps its own) */
	if (style == _ae_sword_lunge_style_carry)
		return 12;
	if (speed <= 0.0f)
		return 60;
	return (short)((distance / speed) * 30.0f) + 6;
}

float ae_sword_lunge_turn_angle(float current, float target, float limit)
{
	float difference = target - current;

	while (difference > AE_SWORD_LUNGE_PI)
		difference -= 2.0f * AE_SWORD_LUNGE_PI;
	while (difference < -AE_SWORD_LUNGE_PI)
		difference += 2.0f * AE_SWORD_LUNGE_PI;
	if (difference > limit)
		difference = limit;
	else if (difference < -limit)
		difference = -limit;
	current += difference;
	while (current > AE_SWORD_LUNGE_PI)
		current -= 2.0f * AE_SWORD_LUNGE_PI;
	while (current < -AE_SWORD_LUNGE_PI)
		current += 2.0f * AE_SWORD_LUNGE_PI;
	return current;
}

int ae_sword_lunge_cooldown_blocks_melee(enum ae_sword_lunge_style style)
{
	/* (0.7.6, owner play: a swing that did nothing for 1.1 s after a lunge read as a dead button. Neither style
	blocks it now: a swing in the cooldown is a swing where the player stands) */
	(void)style;
	return 0;
}

int ae_sword_lunge_swings_in_place(int melee, float distance, float minimum, long now, long next_time)
{
	if (melee)
		return 0;
	return distance <= minimum || now < next_time;
}

float ae_sword_lunge_cone(float angle, float distance, float body_radius)
{
	/* (the cone reaches the edge of his body, not only its centre: up close his centre sits well off the aim, below
	the eyes, and a 25 degree cone to it missed enemies at arm's length) */
	if (distance <= 0.0f)
		return AE_SWORD_LUNGE_PI;
	return angle + atanf(body_radius / distance);
}

enum ae_sword_lunge_end_action ae_sword_lunge_end_action(enum ae_sword_lunge_style style,
	enum ae_sword_lunge_end end)
{
	if (style == _ae_sword_lunge_style_strike)
		return _ae_sword_lunge_end_action_strike;
	return end == _ae_sword_lunge_end_arrived ? _ae_sword_lunge_end_action_damage :
		_ae_sword_lunge_end_action_cancel;
}

int ae_sword_lunge_judge(struct ae_sword_lunge_tick const *tick)
{
	if (tick->distance <= tick->strike)
		return _ae_sword_lunge_end_arrived;
	/* (held up against him, at contact: the blow lands as an arrival's; a hold-up short of him is a miss) */
	if (tick->stalled >= tick->stall_ticks)
		return tick->distance <= tick->contact ? _ae_sword_lunge_end_arrived : _ae_sword_lunge_end_stalled;
	if (tick->distance > tick->reach + 0.5f || tick->covered > tick->budget)
		return _ae_sword_lunge_end_lost;
	if (tick->ticks_left <= 0)
		return _ae_sword_lunge_end_timeout;
	return -1;
}
