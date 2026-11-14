/*
AE_FOLLOW_CAMERA_RULES.C

The death and follow cameras' rules (ae_follow_camera_rules.h).
*/

#include <math.h>

#include "ae_follow_camera_rules.h"

#define AE_FOLLOW_PI 3.14159265f

float ae_follow_blend(float half_life, float seconds)
{
	if (seconds <= 0.0f)
		return 0.0f;
	if (half_life <= 0.0f)
		return 1.0f;
	return 1.0f - (float)pow(0.5, (double)(seconds / half_life));
}

float ae_follow_smooth(float current, float target, float half_life, float seconds)
{
	return current + (target - current) * ae_follow_blend(half_life, seconds);
}

float ae_follow_smooth_angle(float current, float target, float half_life, float seconds)
{
	float turn = target - current;

	while (turn > AE_FOLLOW_PI)
		turn -= 2.0f * AE_FOLLOW_PI;
	while (turn < -AE_FOLLOW_PI)
		turn += 2.0f * AE_FOLLOW_PI;
	/* (from the target, so the angle never winds up past a turn) */
	return target - turn + turn * ae_follow_blend(half_life, seconds);
}

float ae_follow_glide(float fraction)
{
	if (fraction <= 0.0f)
		return 0.0f;
	if (fraction >= 1.0f)
		return 1.0f;
	return fraction * fraction * fraction * (10.0f + fraction * (-15.0f + 6.0f * fraction));
}

float ae_follow_pitch(float aim_pitch)
{
	float pitch = aim_pitch * AE_FOLLOW_PITCH_SHARE - AE_FOLLOW_PITCH_DOWN;

	return pitch < AE_FOLLOW_PITCH_LOWEST ? AE_FOLLOW_PITCH_LOWEST :
		pitch > AE_FOLLOW_PITCH_HIGHEST ? AE_FOLLOW_PITCH_HIGHEST : pitch;
}

int ae_follow_leave_body(int setting, float dead_seconds, int pressed, int long_wait, int no_respawn)
{
	if (setting == AE_FOLLOW_SETTING_OFF)
		return 0;
	if (no_respawn && dead_seconds >= AE_FOLLOW_HOLD_SECONDS)
		return 1;
	if (setting == AE_FOLLOW_SETTING_LONG_WAITS && !long_wait)
		return 0;
	return pressed && dead_seconds >= AE_FOLLOW_PRESS_AFTER_SECONDS;
}

float ae_follow_ease_in_out(float fraction)
{
	if (fraction <= 0.0f)
		return 0.0f;
	if (fraction >= 1.0f)
		return 1.0f;
	return fraction < 0.5f ? 2.0f * fraction * fraction : 1.0f - 2.0f * (1.0f - fraction) * (1.0f - fraction);
}

int ae_follow_switch_glides(float distance, int in_sight)
{
	return in_sight && distance <= AE_FOLLOW_GLIDE_REACH;
}

float ae_death_pitch(float rise, float across)
{
	float pitch = AE_DEATH_PITCH + 0.5f * (float)atan2(rise, across > 0.1f ? across : 0.1f);

	return pitch < AE_DEATH_PITCH_LOWEST ? AE_DEATH_PITCH_LOWEST : pitch > AE_DEATH_PITCH_HIGHEST ? AE_DEATH_PITCH_HIGHEST : pitch;
}

int ae_death_killer_is_this_death(long killed_at, long started_at, long ticks_per_second)
{
	return killed_at >= 0 && killed_at >= started_at - ticks_per_second;
}

long ae_death_killer_to_watch(long killer, long dead)
{
	return killer < 0 || killer == dead ? -1 : killer;
}
