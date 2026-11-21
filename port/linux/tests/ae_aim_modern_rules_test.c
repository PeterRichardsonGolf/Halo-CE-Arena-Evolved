#include <math.h>
#include <stdio.h>
#include "ae_aim_modern_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
#define NEAR(a, b, e) CHECK(fabs((double)(a) - (double)(b)) <= (e))

#define PI 3.14159265358979

/* a raw stick pushed all the way, at an angle off the horizontal axis, through the dead zones and the curve:
yaw and pitch in degrees a second (sensitivity 3: yaw 120, pitch 60) */
static void push(double percent, double degrees, double *yaw, double *pitch)
{
	short x = (short)(percent / 100.0 * 32767.0 * cos(degrees * PI / 180.0));
	short y = (short)(percent / 100.0 * 32767.0 * sin(degrees * PI / 180.0));
	float cy;
	float cp;

	ae_aim_dead_zone_stick(&x, &y, 0.12f, 0.12f);
	ae_aim_modern_stick((float)x / 32767.f, (float)y / 32767.f, &cy, &cp);
	*yaw = cy * 120.0;
	*pitch = cp * 60.0;
}

int main(void)
{
	static const double r[] = { 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 0.95, 1.0 };
	static const double f[] = { 0.020, 0.038, 0.061, 0.099, 0.163, 0.266, 0.413, 0.592, 0.792, 0.896, 1.000 };
	static const double angles[] = { 0.0, 15.0, 45.0 };
	static const double yaws[] = { 120.0, 111.1, 73.5 };
	static const double pitches[] = { 0.0, 9.1, 36.7 };
	int index;
	short x, y;
	double yaw, pitch;
	float timer;

	/* the curve: the table, the pieces meet, 0 at 0 and 1 at 1, no clamp on a diagonal */
	for (index = 0; index < 11; index++)
		NEAR(ae_aim_modern_curve((float)r[index]), f[index], 0.0015);
	NEAR(ae_aim_modern_curve(0.f), 0.0, 1e-6);
	NEAR(ae_aim_modern_curve(1.f), 1.0, 1e-4);
	NEAR(ae_aim_modern_curve(0.5f), 0.1626, 0.0005);
	NEAR(ae_aim_modern_curve(1.41421356f), 1.70, 0.01);
	{
		float cy, cp;

		ae_aim_modern_stick(0.f, 0.f, &cy, &cp);
		CHECK(cy == 0.f && cp == 0.f);
		ae_aim_modern_stick(-1.f, 0.f, &cy, &cp);
		NEAR(cy, -1.0, 1e-4);
		ae_aim_modern_stick(0.7071f, 0.7071f, &cy, &cp);
		NEAR(cy, cp, 1e-6);
		NEAR(sqrt((double)cy * cy + (double)cp * cp), ae_aim_modern_curve(0.99999f), 1e-3);
	}

	/* the dead zones at 12 and 12: nothing to 22.56 %, then it starts; whole shorts, the length at most 32767 */
	x = (short)(0.2255 * 32767.0); y = 0;
	ae_aim_dead_zone_stick(&x, &y, 0.12f, 0.12f);
	CHECK(x == 0 && y == 0);
	x = (short)(0.2300 * 32767.0); y = 0;
	ae_aim_dead_zone_stick(&x, &y, 0.12f, 0.12f);
	NEAR(x / 32767.0, 0.006, 0.0015);
	x = -(short)(0.2300 * 32767.0); y = 0;
	ae_aim_dead_zone_stick(&x, &y, 0.12f, 0.12f);
	NEAR(x / 32767.0, -0.006, 0.0015);
	x = 32767; y = 0;
	ae_aim_dead_zone_stick(&x, &y, 0.12f, 0.12f);
	CHECK(x == 32767 && y == 0);
	x = -32768; y = 0;
	ae_aim_dead_zone_stick(&x, &y, 0.12f, 0.12f);
	CHECK(x == -32767 || x == -32768);
	x = 32767; y = 32767;
	ae_aim_dead_zone_stick(&x, &y, 0.12f, 0.12f);
	CHECK(sqrt((double)x * x + (double)y * y) <= 32767.5);
	/* the axial step alone: the game's fix_dead_zone's numbers (9000 of 32767) */
	CHECK(ae_aim_dead_zone_axial(9000, 9000.f / 32767.f) == 0);
	CHECK(ae_aim_dead_zone_axial(-9000, 9000.f / 32767.f) == 0);
	CHECK(ae_aim_dead_zone_axial(32767, 9000.f / 32767.f) == 32767);
	CHECK(ae_aim_dead_zone_axial(-32768, 9000.f / 32767.f) == -32768);
	CHECK(ae_aim_dead_zone_axial(20000, 0.f) == 20000);
	/* radial 0: nothing, a diagonal can pass 1 (the curve has no clamp) */
	x = 32767; y = 32767;
	ae_aim_dead_zone_stick(&x, &y, 0.f, 0.f);
	CHECK(x == 32767 && y == 32767);
	/* the look's start on an axis: d + d (1 - d) */
	x = (short)(0.0784 * 32767.0 + 40); y = 0;
	ae_aim_dead_zone_stick(&x, &y, 0.04f, 0.04f);
	CHECK(x > 0);
	x = (short)(0.0784 * 32767.0 - 40); y = 0;
	ae_aim_dead_zone_stick(&x, &y, 0.04f, 0.04f);
	CHECK(x == 0);

	/* a full push off the axis (sensitivity 3): the study's table */
	for (index = 0; index < 3; index++)
	{
		push(100.0, angles[index], &yaw, &pitch);
		NEAR(yaw, yaws[index], 0.4);
		NEAR(pitch, pitches[index], 0.4);
	}
	/* a push along the axis, the table's rows */
	push(50.0, 0.0, &yaw, &pitch);
	NEAR(yaw, 9.5, 0.3);
	push(75.0, 0.0, &yaw, &pitch);
	NEAR(yaw, 45.1, 0.5);
	push(90.0, 0.0, &yaw, &pitch);
	NEAR(yaw, 87.9, 0.7);

	/* the ramp: k 1 gives 224.3 at 0.5 s and 360 at 1.15 s; k 0.7 gives 193.0 at 0.5 s and 360 at 1.643 s */
	{
		timer = 0.f;
		NEAR(ae_aim_modern_ramp(&timer, 1.f, 3.f, 1.f, 0.5f), 1.0, 1e-6);
		NEAR(120.0 * ae_aim_modern_ramp(&timer, 1.f, 3.f, 1.f, 0.5f), 224.3, 0.1);
		timer = 1.15f;
		NEAR(120.0 * ae_aim_modern_ramp(&timer, 1.f, 3.f, 1.f, 0.1f), 360.0, 0.01);
		timer = 5.f;
		NEAR(120.0 * ae_aim_modern_ramp(&timer, 1.f, 3.f, 1.f, 0.1f), 360.0, 0.01);
		/* (the timer counts k * seconds: 0.5 s at k 0.7 is 0.35, 1.643 s is 1.15) */
		timer = 0.35f;
		NEAR(120.0 * ae_aim_modern_ramp(&timer, 1.f, 3.f, 0.7f, 0.1f), 193.0, 0.1);
		timer = 1.15f;
		NEAR(120.0 * ae_aim_modern_ramp(&timer, 1.f, 3.f, 0.7f, 0.1f), 360.0, 0.1);
		/* stepped: 1.643 s of ticks at k 0.7 reaches the top */
		{
			int step;
			float mult = 1.f;

			timer = 0.f;
			for (step = 0; step < 60; step++)
				mult = ae_aim_modern_ramp(&timer, 1.f, 3.f, 0.7f, 1.f / 30.f);
			NEAR(120.0 * mult, 360.0, 0.01);
		}
		/* pitch tops at 2 times: 60 to 120 */
		timer = 1.15f;
		NEAR(60.0 * ae_aim_modern_ramp(&timer, 1.f, AE_AIM_RAMP_PITCH, 1.f, 0.1f), 120.0, 0.01);
		timer = 0.5f;
		NEAR(60.0 * ae_aim_modern_ramp(&timer, 1.f, AE_AIM_RAMP_PITCH, 1.f, 0.1f), 86.1, 0.1);
		/* the timer after a tick gains k * dt */
		timer = 0.f;
		ae_aim_modern_ramp(&timer, 0.96f, 3.f, 0.7f, 0.1f);
		NEAR(timer, 0.07, 1e-5);
		/* below 0.95 it resets, the multiplier 1 */
		NEAR(ae_aim_modern_ramp(&timer, 0.9499f, 3.f, 0.7f, 0.1f), 1.0, 1e-6);
		CHECK(timer == 0.f);
		timer = 0.4f;
		NEAR(ae_aim_modern_ramp(&timer, 0.f, 3.f, 1.f, 0.1f), 1.0, 1e-6);
		CHECK(timer == 0.f);
	}
	/* LOOK ACCELERATION's k */
	timer = 0.8f;
	NEAR(ae_aim_modern_ramp(&timer, 1.f, AE_AIM_RAMP_YAW, ae_aim_modern_k(0), 10.f), 1.0, 1e-6);
	CHECK(timer == 0.f);
	NEAR(ae_aim_modern_k(1), 0.700, 1e-6);
	NEAR(ae_aim_modern_k(5), 1.000, 1e-6);
	NEAR(ae_aim_modern_k(10), 1.375, 1e-6);
	NEAR(ae_aim_modern_k(0), 0.0, 1e-6);
	NEAR(ae_aim_modern_k(99), 1.375, 1e-6);
	NEAR(1.15 / ae_aim_modern_k(1), 1.643, 0.001);

	/* a radial override changes only the radial step: the look starts at axial + radial * (1 - axial) */
	x = (short)(0.1201 * 32767.0); y = 0;
	ae_aim_modern_dead_zone(&x, &y, 0.12f, 0);
	CHECK(x > 0);
	x = (short)(0.2959 * 32767.0); y = 0;
	ae_aim_modern_dead_zone(&x, &y, 0.12f, 20);
	CHECK(x == 0);
	x = (short)(0.2961 * 32767.0); y = 0;
	ae_aim_modern_dead_zone(&x, &y, 0.12f, 20);
	CHECK(x > 0);
	/* SOUTHPAW puts the look settings, including its radial override, on the left stick */
	x = (short)(0.25 * 32767.0); y = 0;
	{
		short rx = x, ry = 0;

		ae_aim_modern_dead_zone_sticks(&x, &y, &rx, &ry, 0, 0, 20, 1);
		CHECK(x == 0 && rx > 0);
	}

	/* VERTICAL LOOK SENSITIVITY uses the chosen pitch-table rate over LOOK SENSITIVITY's */
	NEAR(ae_aim_modern_pitch_ratio(0, 60.f), 1.0, 1e-6);
	NEAR(ae_aim_modern_pitch_ratio(7, 60.f), 100.0 / 60.0, 1e-6);

	if (failures)
		printf("%d failures\n", failures);
	else
		printf("ae_aim_modern_rules_test: ok\n");
	return failures ? 1 : 0;
}
