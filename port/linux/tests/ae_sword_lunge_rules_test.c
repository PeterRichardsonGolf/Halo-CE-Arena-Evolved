#include <math.h>
#include <stdio.h>

#include "ae_sword_lunge_rules.h"

static int failures;
static int checks;
#define CHECK(c) do { checks++; if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

#define PI 3.14159265358979323846f
#define DEGREES(x) ((x) * PI / 180.0f)

static int close_enough(float a, float b)
{
	return fabsf(a - b) < 0.0001f;
}

/* A missing, explicit carry or bad value gets the default; only strike keeps the earlier order. */
static void test_style(void)
{
	CHECK(ae_sword_lunge_style_from_text(NULL) == _ae_sword_lunge_style_carry);
	CHECK(ae_sword_lunge_style_from_text("") == _ae_sword_lunge_style_carry);
	CHECK(ae_sword_lunge_style_from_text("carry") == _ae_sword_lunge_style_carry);
	CHECK(ae_sword_lunge_style_from_text("other") == _ae_sword_lunge_style_carry);
	CHECK(ae_sword_lunge_style_from_text("strike") == _ae_sword_lunge_style_strike);
}

/* The carry style's melee action is capped at two seconds; strike keeps a travel estimate and a six-tick allowance. */
static void test_duration(void)
{
	CHECK(ae_sword_lunge_duration_ticks(_ae_sword_lunge_style_carry, 0.91f, 15.0f) == 12);
	CHECK(ae_sword_lunge_duration_ticks(_ae_sword_lunge_style_carry, 20.0f, 1.0f) == 12);
	CHECK(ae_sword_lunge_duration_ticks(_ae_sword_lunge_style_strike, 1.4f, 7.0f) == 12);
	CHECK(ae_sword_lunge_duration_ticks(_ae_sword_lunge_style_strike, 2.5f, 7.0f) == 16);
	CHECK(ae_sword_lunge_duration_ticks(_ae_sword_lunge_style_strike, 2.5f, 0.0f) == 60);
}

/* The tracked view turns by the shortest yaw/pitch path, at no more than the carry style's per-tick limit. */
static void test_turn_angle(void)
{
	CHECK(close_enough(ae_sword_lunge_turn_angle(0.0f, DEGREES(10.0f), DEGREES(18.0f)), DEGREES(10.0f)));
	CHECK(close_enough(ae_sword_lunge_turn_angle(0.0f, DEGREES(40.0f), DEGREES(18.0f)), DEGREES(18.0f)));
	CHECK(close_enough(ae_sword_lunge_turn_angle(DEGREES(170.0f), DEGREES(-170.0f), DEGREES(18.0f)),
		DEGREES(-172.0f)));
	CHECK(close_enough(ae_sword_lunge_turn_angle(DEGREES(-170.0f), DEGREES(170.0f), DEGREES(18.0f)),
		DEGREES(172.0f)));
}

/* The carry style's lunge is the attack: contact damages now and failures do not start a second swing. */
static void test_judge(void)
{
	struct ae_sword_lunge_tick tick = { 2.0f, 0.6f, 0.75f, 3.5f, 0.5f, 3.2f, 8, 0, 2 };

	CHECK(ae_sword_lunge_judge(&tick) == -1);
	tick.distance = 0.6f;
	CHECK(ae_sword_lunge_judge(&tick) == _ae_sword_lunge_end_arrived);
	/* (the owner's two misses: held up 0.61 from him, at contact: a blow now) */
	tick.distance = 0.61f, tick.stalled = 2;
	CHECK(ae_sword_lunge_judge(&tick) == _ae_sword_lunge_end_arrived);
	tick.distance = 1.5f;
	CHECK(ae_sword_lunge_judge(&tick) == _ae_sword_lunge_end_stalled);
	/* (the telejump: he got away, or the lunge used up its distance) */
	tick.stalled = 0, tick.distance = 4.1f;
	CHECK(ae_sword_lunge_judge(&tick) == _ae_sword_lunge_end_lost);
	tick.distance = 2.0f, tick.covered = 3.3f;
	CHECK(ae_sword_lunge_judge(&tick) == _ae_sword_lunge_end_lost);
	tick.covered = 1.0f, tick.ticks_left = 0;
	CHECK(ae_sword_lunge_judge(&tick) == _ae_sword_lunge_end_timeout);
}

static void test_end_action(void)
{
	CHECK(ae_sword_lunge_cooldown_blocks_melee(_ae_sword_lunge_style_carry));
	CHECK(!ae_sword_lunge_cooldown_blocks_melee(_ae_sword_lunge_style_strike));
	CHECK(ae_sword_lunge_end_action(_ae_sword_lunge_style_carry, _ae_sword_lunge_end_arrived) ==
		_ae_sword_lunge_end_action_damage);
	CHECK(ae_sword_lunge_end_action(_ae_sword_lunge_style_carry, _ae_sword_lunge_end_timeout) ==
		_ae_sword_lunge_end_action_cancel);
	CHECK(ae_sword_lunge_end_action(_ae_sword_lunge_style_carry, _ae_sword_lunge_end_stalled) ==
		_ae_sword_lunge_end_action_cancel);
	CHECK(ae_sword_lunge_end_action(_ae_sword_lunge_style_strike, _ae_sword_lunge_end_arrived) ==
		_ae_sword_lunge_end_action_strike);
	CHECK(ae_sword_lunge_end_action(_ae_sword_lunge_style_strike, _ae_sword_lunge_end_timeout) ==
		_ae_sword_lunge_end_action_strike);
	CHECK(ae_sword_lunge_end_action(_ae_sword_lunge_style_strike, _ae_sword_lunge_end_stalled) ==
		_ae_sword_lunge_end_action_strike);
}

int main(void)
{
	test_style();
	test_duration();
	test_turn_angle();
	test_judge();
	test_end_action();
	printf("%d checks, %d failures\n", checks, failures);
	return failures ? 1 : 0;
}
