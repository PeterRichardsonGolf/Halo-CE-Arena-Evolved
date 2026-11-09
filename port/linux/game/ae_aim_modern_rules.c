/*
AE_AIM_MODERN_RULES.C

AIM CONTROL: MODERN's pure rules (ae_aim_modern_rules.h).
*/

#include <math.h>

#include "ae_aim_modern_rules.h"

enum { STICK_MAX = 32767, STICK_MIN_MAGNITUDE = 32768 };

static short whole(double value)
{
	if (value > (double)STICK_MAX)
		return (short)STICK_MAX;
	if (value < -(double)STICK_MIN_MAGNITUDE)
		return (short)-STICK_MIN_MAGNITUDE;
	return (short)value;
}

short ae_aim_dead_zone_axial(short value, float axial)
{
	double range = (double)axial * STICK_MAX;

	if (range < 0.0)
		range = 0.0;
	if (range > (double)STICK_MAX)
		range = (double)STICK_MAX;
	if ((double)value > range)
		return whole(((double)value - range) * STICK_MAX / (STICK_MAX - range));
	if ((double)value < -range)
		return whole(((double)value + range) * STICK_MIN_MAGNITUDE / (STICK_MIN_MAGNITUDE - range));
	return 0;
}

void ae_aim_dead_zone_radial(short *x, short *y, float radial)
{
	double range = (double)radial * STICK_MAX;
	double length;
	double inside;

	if (range <= 0.0)
		return;
	if (range >= (double)STICK_MAX)
	{
		*x = 0;
		*y = 0;
		return;
	}
	length = sqrt((double)*x * (double)*x + (double)*y * (double)*y);
	if (length <= range)
	{
		*x = 0;
		*y = 0;
		return;
	}
	inside = length > (double)STICK_MAX ? (double)STICK_MAX : length;
	inside = (inside - range) * STICK_MAX / (STICK_MAX - range);
	*x = whole((double)*x / length * inside);
	*y = whole((double)*y / length * inside);
}

void ae_aim_dead_zone_stick(short *x, short *y, float axial, float radial)
{
	*x = ae_aim_dead_zone_axial(*x, axial);
	*y = ae_aim_dead_zone_axial(*y, axial);
	ae_aim_dead_zone_radial(x, y, radial);
}

void ae_aim_modern_dead_zone(short *x, short *y, float axial, long radial_percent)
{
	float radial = axial;

	if (radial_percent >= 0)
	{
		if (radial_percent > 20)
			radial_percent = 20;
		radial = (float)radial_percent / 100.f;
	}
	ae_aim_dead_zone_stick(x, y, axial, radial);
}

static float zone_of(long percent, float default_zone)
{
	if (percent <= 0)
		return default_zone;
	if (percent > 90)
		percent = 90;
	return (float)percent / 100.f;
}

void ae_aim_modern_dead_zone_sticks(short *left_x, short *left_y, short *right_x, short *right_y, long move_percent,
	long look_percent, long look_radial_percent, int southpaw)
{
	float move = zone_of(move_percent, AE_AIM_DEFAULT_MOVE_ZONE);
	float look = zone_of(look_percent, AE_AIM_DEFAULT_LOOK_ZONE);

	if (southpaw)
	{
		ae_aim_modern_dead_zone(left_x, left_y, look, look_radial_percent);
		ae_aim_modern_dead_zone(right_x, right_y, move, -1);
	}
	else
	{
		ae_aim_modern_dead_zone(left_x, left_y, move, -1);
		ae_aim_modern_dead_zone(right_x, right_y, look, look_radial_percent);
	}
}

float ae_aim_modern_curve(float r)
{
	if (r <= 0.5f)
		return ((1.54475856f * r - 0.623594284f) * r + 0.250862837f) * r;
	return ((-1.9517467f * r + 5.70230103f) * r - 3.46320319f) * r + 0.712648511f;
}

void ae_aim_modern_stick(float yaw, float pitch, float *curve_yaw, float *curve_pitch)
{
	float length = sqrtf(yaw * yaw + pitch * pitch);

	if (length <= 0.f)
	{
		*curve_yaw = 0.f;
		*curve_pitch = 0.f;
		return;
	}
	{
		float scale = ae_aim_modern_curve(length) / length;

		*curve_yaw = yaw * scale;
		*curve_pitch = pitch * scale;
	}
}

float ae_aim_modern_k(long menu)
{
	if (menu == 0)
		return 0.f;
	if (menu < 1)
		menu = 1;
	if (menu > 10)
		menu = 10;
	return 0.700f + 0.075f * (float)(menu - 1);
}

float ae_aim_modern_ramp(float *timer, float scaled_abs, float scale_max, float k, float dt)
{
	if (k <= 0.f)
	{
		*timer = 0.f;
		return 1.f;
	}
	if (scaled_abs >= AE_AIM_FULL_THRESHOLD)
	{
		float fraction = *timer / AE_AIM_RAMP_SECONDS;

		if (fraction < 0.f)
			fraction = 0.f;
		if (fraction > 1.f)
			fraction = 1.f;
		*timer += k * dt;
		return (scale_max - 1.f) * fraction + 1.f;
	}
	*timer = 0.f;
	return 1.f;
}

float ae_aim_modern_pitch_ratio(long menu, float current_rate)
{
	if (menu == 0 || current_rate <= 0.f)
		return 1.f;
	if (menu < 1)
		menu = 1;
	if (menu > 10)
		menu = 10;
	return (30.f + 10.f * (float)menu) / current_rate;
}
