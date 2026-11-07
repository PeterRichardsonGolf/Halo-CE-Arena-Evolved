#include <math.h>
#include <stdio.h>

#include "ae_view_sway_rules.h"

static int failures;
static int checks;
#define CHECK(c) do { checks++; if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

/* the stock spring as the game steps it once a tick (its numbers a tick's; maximum_velocity is the velocity's change a
tick, acceleration the speed's limit), written again here to hold the stepped one to it */
static void stock_step(float *position, float *velocity, float target, float maximum_velocity, float acceleration,
	float lower, float upper)
{
	float delta = target - *position, limit = fminf(maximum_velocity, acceleration), speed, step;

	if (fabsf(delta - *velocity) <= limit)
	{
		*velocity = 0.0f;
		*position = target < lower ? lower : target > upper ? upper : target;
		return;
	}
	speed = 2.0f * maximum_velocity * fabsf(delta) >= acceleration * acceleration ? acceleration :
		sqrtf(2.0f * maximum_velocity * fabsf(delta));
	if (delta < 0.0f)
		speed = -speed;
	step = speed - *velocity;
	if (fabsf(step) > maximum_velocity)
		step = step < 0.0f ? -maximum_velocity : maximum_velocity;
	*velocity += step;
	*position = step * 0.5f + *velocity + *position;
	if (*position < lower)
		*position = lower;
	else if (*position > upper)
		*position = upper;
}

/* A tick's step is the stock step, number for number, over a whole lean and its return (the velocity is a second's:
the stock one's times 30). */
static void test_tick_is_stock(void)
{
	static const float targets[] = { 1.0f, -0.4f, 0.0f, 0.75f, 0.0f };
	float a = 0.0f, av = 0.0f, b = 0.0f, bv = 0.0f, worst = 0.0f;
	int index, tick;

	for (index = 0; index < 5; index++)
		for (tick = 0; tick < 40; tick++)
		{
			stock_step(&a, &av, targets[index], AE_VIEW_SWAY_TURN_ACCELERATION, AE_VIEW_SWAY_TURN_SPEED, -1.0f, 1.0f);
			ae_view_sway_step(&b, &bv, targets[index], AE_VIEW_SWAY_TURN_ACCELERATION, AE_VIEW_SWAY_TURN_SPEED,
				1.0f / 30.0f, -1.0f, 1.0f);
			worst = fmaxf(worst, fabsf(a - b));
			worst = fmaxf(worst, fabsf(av * 30.0f - bv));
		}
	CHECK(worst < 0.0005f);
	/* (the walk spring too) */
	a = av = b = bv = 0.0f;
	worst = 0.0f;
	for (tick = 0; tick < 30; tick++)
	{
		stock_step(&a, &av, 1.0f, AE_VIEW_SWAY_WALK_ACCELERATION, AE_VIEW_SWAY_WALK_SPEED, -1.0f, 1.0f);
		ae_view_sway_step(&b, &bv, 1.0f, AE_VIEW_SWAY_WALK_ACCELERATION, AE_VIEW_SWAY_WALK_SPEED, 1.0f / 30.0f, -1.0f,
			1.0f);
		worst = fmaxf(worst, fabsf(a - b));
	}
	CHECK(worst < 0.0005f);
}

/* Four frames a tick follow the same lean as one step a tick within a little (the stock tick step leads by half its
velocity change, a step's worth), arriving within two ticks of it. */
static void test_frames_follow_the_ticks(void)
{
	float a = 0.0f, av = 0.0f, b = 0.0f, bv = 0.0f, worst = 0.0f;
	int tick, frame, a_done = -1, b_done = -1;

	for (tick = 0; tick < 60; tick++)
	{
		ae_view_sway_step(&a, &av, 1.0f, AE_VIEW_SWAY_TURN_ACCELERATION, AE_VIEW_SWAY_TURN_SPEED, 1.0f / 30.0f, -1.0f,
			1.0f);
		for (frame = 0; frame < 4; frame++)
			ae_view_sway_step(&b, &bv, 1.0f, AE_VIEW_SWAY_TURN_ACCELERATION, AE_VIEW_SWAY_TURN_SPEED, 1.0f / 120.0f,
				-1.0f, 1.0f);
		worst = fmaxf(worst, fabsf(a - b));
		if (a_done < 0 && a == 1.0f)
			a_done = tick;
		if (b_done < 0 && fabsf(b - 1.0f) < 0.001f)
			b_done = tick;
	}
	CHECK(worst < 0.15f);
	CHECK(a_done > 0 && b_done > 0 && b_done <= a_done + 2);
}

/* The target, strength and speed numbers, and odd steps. */
static void test_numbers(void)
{
	float p = 0.25f, v = 0.0f, as, ss;

	CHECK(fabsf(ae_view_sway_turn_target(0.01f, 1.0f / 30.0f) - 0.3f) < 0.0001f);
	CHECK(ae_view_sway_turn_target(1.0f, 1.0f / 30.0f) == 1.0f && ae_view_sway_turn_target(-1.0f, 0.01f) == -1.0f);
	CHECK(ae_view_sway_turn_target(0.5f, 0.0f) == 0.0f);
	CHECK(ae_view_sway_strength(100.0f) == 1.0f && ae_view_sway_strength(25.0f) == 0.25f &&
		ae_view_sway_strength(-5.0f) == 0.0f && ae_view_sway_strength(150.0f) == 1.0f && ae_view_sway_strength(NAN) == 0.0f);
	ae_view_sway_speed(2.0f, &as, &ss);
	CHECK(as == 4.0f && ss == 2.0f);
	ae_view_sway_speed(0.5f, &as, &ss);
	CHECK(as == 1.0f && ss == 1.0f);
	ae_view_sway_speed(20.0f, &as, &ss);
	CHECK(as == 64.0f && ss == 8.0f);
	/* (no time moves nothing; a long hitch moves no more than the longest step) */
	ae_view_sway_step(&p, &v, 1.0f, AE_VIEW_SWAY_TURN_ACCELERATION, AE_VIEW_SWAY_TURN_SPEED, 0.0f, -1.0f, 1.0f);
	CHECK(p == 0.25f && v == 0.0f);
	{
		float q = 0.0f, qv = 0.0f, r = 0.0f, rv = 0.0f;

		ae_view_sway_step(&q, &qv, 1.0f, AE_VIEW_SWAY_TURN_ACCELERATION, AE_VIEW_SWAY_TURN_SPEED, 5.0f, -1.0f, 1.0f);
		ae_view_sway_step(&r, &rv, 1.0f, AE_VIEW_SWAY_TURN_ACCELERATION, AE_VIEW_SWAY_TURN_SPEED,
			AE_VIEW_SWAY_LONGEST_STEP, -1.0f, 1.0f);
		CHECK(q == r && qv == rv);
	}
	/* (a stiffer spring gets there sooner) */
	{
		float soft = 0.0f, softv = 0.0f, stiff = 0.0f, stiffv = 0.0f;
		int frame;

		for (frame = 0; frame < 6; frame++)
		{
			ae_view_sway_step(&soft, &softv, 1.0f, AE_VIEW_SWAY_TURN_ACCELERATION, AE_VIEW_SWAY_TURN_SPEED, 1.0f / 60.0f,
				-1.0f, 1.0f);
			ae_view_sway_step(&stiff, &stiffv, 1.0f, AE_VIEW_SWAY_TURN_ACCELERATION * 4.0f, AE_VIEW_SWAY_TURN_SPEED * 2.0f,
				1.0f / 60.0f, -1.0f, 1.0f);
		}
		CHECK(stiff > soft);
	}
}

int main(void)
{
	test_tick_is_stock();
	test_frames_follow_the_ticks();
	test_numbers();
	printf("%d checks, %d failures\n", checks, failures);
	return failures ? 1 : 0;
}
