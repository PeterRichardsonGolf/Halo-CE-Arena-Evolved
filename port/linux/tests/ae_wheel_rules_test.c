#include <math.h>
#include <stdio.h>
#include "ae_mark_rules.h"
#include "ae_wheel_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

#define DEGREES_TO_RADIANS 0.017453292f

/* a stick pushed this far, in degrees clockwise from up */
static float stick_x(float degrees, float push)
{
	return push * sinf(degrees * DEGREES_TO_RADIANS);
}

static float stick_y(float degrees, float push)
{
	return push * cosf(degrees * DEGREES_TO_RADIANS);
}

static int pick_at(int current, float degrees, float push)
{
	return ae_wheel_pick(current, stick_x(degrees, push), stick_y(degrees, push));
}

static void press_timing_test(void)
{
	struct ae_wheel_press press = { 0 };
	long now;

	/* idle: nothing */
	for (now = 0; now < 10; now++)
		CHECK(ae_wheel_press_step(&press, 0, 0, now, AE_WHEEL_NONE) == AE_PRESS_NOTHING);
	/* a release before the hold time is a tap, at once (no wait for a double) */
	CHECK(ae_wheel_press_step(&press, 1, 0, 100, AE_WHEEL_NONE) == AE_PRESS_NOTHING);
	CHECK(ae_wheel_press_step(&press, 1, 0, 100 + AE_MARK_HOLD_TICKS - 2, AE_WHEEL_NONE) == AE_PRESS_NOTHING);
	CHECK(ae_wheel_press_step(&press, 0, 0, 100 + AE_MARK_HOLD_TICKS - 1, AE_WHEEL_NONE) == AE_PRESS_TAP);
	CHECK(ae_wheel_press_step(&press, 0, 0, 100 + AE_MARK_HOLD_TICKS, AE_WHEEL_NONE) == AE_PRESS_NOTHING);

	/* down for the hold time opens the wheel, once, with no tap after */
	CHECK(ae_wheel_press_step(&press, 1, 0, 200, AE_WHEEL_NONE) == AE_PRESS_NOTHING);
	CHECK(ae_wheel_press_step(&press, 1, 0, 200 + AE_MARK_HOLD_TICKS - 1, AE_WHEEL_NONE) == AE_PRESS_NOTHING);
	CHECK(ae_wheel_press_step(&press, 1, 0, 200 + AE_MARK_HOLD_TICKS, AE_WHEEL_NONE) == AE_PRESS_OPEN);
	CHECK(ae_wheel_press_step(&press, 1, 0, 200 + AE_MARK_HOLD_TICKS + 1, AE_WHEEL_NONE) == AE_PRESS_NOTHING);
	CHECK(ae_wheel_press_step(&press, 1, 0, 200 + AE_MARK_HOLD_TICKS + 20, AE_WHEEL_NONE) == AE_PRESS_NOTHING);
	/* the release with an entry focused sends it */
	CHECK(ae_wheel_press_step(&press, 0, 0, 200 + AE_MARK_HOLD_TICKS + 21, AE_WHEEL_GOING) == AE_PRESS_SEND);
	CHECK(ae_wheel_press_step(&press, 0, 0, 200 + AE_MARK_HOLD_TICKS + 22, AE_WHEEL_NONE) == AE_PRESS_NOTHING);

	/* the release with none focused cancels */
	CHECK(ae_wheel_press_step(&press, 1, 0, 300, AE_WHEEL_NONE) == AE_PRESS_NOTHING);
	CHECK(ae_wheel_press_step(&press, 1, 0, 300 + AE_MARK_HOLD_TICKS, AE_WHEEL_NONE) == AE_PRESS_OPEN);
	CHECK(ae_wheel_press_step(&press, 0, 0, 300 + AE_MARK_HOLD_TICKS + 3, AE_WHEEL_NONE) == AE_PRESS_CANCEL);
	/* the first entry is an entry too (zero) */
	CHECK(ae_wheel_press_step(&press, 1, 0, 400, AE_WHEEL_NONE) == AE_PRESS_NOTHING);
	CHECK(ae_wheel_press_step(&press, 1, 0, 400 + AE_MARK_HOLD_TICKS, AE_WHEEL_NONE) == AE_PRESS_OPEN);
	CHECK(ae_wheel_press_step(&press, 0, 0, 400 + AE_MARK_HOLD_TICKS + 3, AE_WHEEL_SMART) == AE_PRESS_SEND);
}

static void press_double_test(void)
{
	struct ae_wheel_press press = { 0 };

	/* a second press 12 ticks after the first's release: its release is the double */
	CHECK(ae_wheel_press_step(&press, 1, 0, 100, AE_WHEEL_NONE) == AE_PRESS_NOTHING);
	CHECK(ae_wheel_press_step(&press, 0, 0, 102, AE_WHEEL_NONE) == AE_PRESS_TAP);
	CHECK(ae_wheel_press_step(&press, 1, 0, 114, AE_WHEEL_NONE) == AE_PRESS_NOTHING);
	CHECK(ae_wheel_press_step(&press, 0, 0, 116, AE_WHEEL_NONE) == AE_PRESS_DOUBLE);
	/* a third is a first again: a tap */
	CHECK(ae_wheel_press_step(&press, 1, 0, 118, AE_WHEEL_NONE) == AE_PRESS_NOTHING);
	CHECK(ae_wheel_press_step(&press, 0, 0, 120, AE_WHEEL_NONE) == AE_PRESS_TAP);

	/* at 13 it is a tap */
	CHECK(ae_wheel_press_step(&press, 1, 0, 200, AE_WHEEL_NONE) == AE_PRESS_NOTHING);
	CHECK(ae_wheel_press_step(&press, 0, 0, 202, AE_WHEEL_NONE) == AE_PRESS_TAP);
	CHECK(ae_wheel_press_step(&press, 1, 0, 215, AE_WHEEL_NONE) == AE_PRESS_NOTHING);
	CHECK(ae_wheel_press_step(&press, 0, 0, 217, AE_WHEEL_NONE) == AE_PRESS_TAP);
	/* and that tap is the first of the next double */
	CHECK(ae_wheel_press_step(&press, 1, 0, 220, AE_WHEEL_NONE) == AE_PRESS_NOTHING);
	CHECK(ae_wheel_press_step(&press, 0, 0, 222, AE_WHEEL_NONE) == AE_PRESS_DOUBLE);

	/* a second press that is held opens the wheel; it is no double, and the open press leaves no first behind */
	CHECK(ae_wheel_press_step(&press, 1, 0, 300, AE_WHEEL_NONE) == AE_PRESS_NOTHING);
	CHECK(ae_wheel_press_step(&press, 0, 0, 302, AE_WHEEL_NONE) == AE_PRESS_TAP);
	CHECK(ae_wheel_press_step(&press, 1, 0, 305, AE_WHEEL_NONE) == AE_PRESS_NOTHING);
	CHECK(ae_wheel_press_step(&press, 1, 0, 305 + AE_MARK_HOLD_TICKS, AE_WHEEL_NONE) == AE_PRESS_OPEN);
	CHECK(ae_wheel_press_step(&press, 0, 0, 305 + AE_MARK_HOLD_TICKS + 2, AE_WHEEL_ENEMY) == AE_PRESS_SEND);
	CHECK(ae_wheel_press_step(&press, 1, 0, 305 + AE_MARK_HOLD_TICKS + 4, AE_WHEEL_NONE) == AE_PRESS_NOTHING);
	CHECK(ae_wheel_press_step(&press, 0, 0, 305 + AE_MARK_HOLD_TICKS + 6, AE_WHEEL_NONE) == AE_PRESS_TAP);
}

static void wheel_interrupted_test(void)
{
	struct ae_wheel_press press = { 0 };

	/* open, then interrupted: cancel at once; the release after is nothing */
	CHECK(ae_wheel_press_step(&press, 1, 0, 100, AE_WHEEL_NONE) == AE_PRESS_NOTHING);
	CHECK(ae_wheel_press_step(&press, 1, 0, 100 + AE_MARK_HOLD_TICKS, AE_WHEEL_NONE) == AE_PRESS_OPEN);
	CHECK(ae_wheel_press_step(&press, 1, 1, 100 + AE_MARK_HOLD_TICKS + 2, AE_WHEEL_ENEMY) == AE_PRESS_CANCEL);
	CHECK(ae_wheel_press_step(&press, 1, 1, 100 + AE_MARK_HOLD_TICKS + 3, AE_WHEEL_ENEMY) == AE_PRESS_NOTHING);
	CHECK(ae_wheel_press_step(&press, 1, 0, 100 + AE_MARK_HOLD_TICKS + 4, AE_WHEEL_ENEMY) == AE_PRESS_NOTHING);
	CHECK(ae_wheel_press_step(&press, 0, 0, 100 + AE_MARK_HOLD_TICKS + 5, AE_WHEEL_ENEMY) == AE_PRESS_NOTHING);
	/* the next press is whole again */
	CHECK(ae_wheel_press_step(&press, 1, 0, 200, AE_WHEEL_NONE) == AE_PRESS_NOTHING);
	CHECK(ae_wheel_press_step(&press, 0, 0, 202, AE_WHEEL_NONE) == AE_PRESS_TAP);

	/* interrupted while held but not open: the press is cleared, so no hold opens it and the release is nothing */
	CHECK(ae_wheel_press_step(&press, 1, 0, 300, AE_WHEEL_NONE) == AE_PRESS_NOTHING);
	CHECK(ae_wheel_press_step(&press, 1, 1, 302, AE_WHEEL_NONE) == AE_PRESS_NOTHING);
	CHECK(ae_wheel_press_step(&press, 1, 0, 300 + AE_MARK_HOLD_TICKS + 2, AE_WHEEL_NONE) == AE_PRESS_NOTHING);
	CHECK(ae_wheel_press_step(&press, 0, 0, 300 + AE_MARK_HOLD_TICKS + 3, AE_WHEEL_NONE) == AE_PRESS_NOTHING);
	/* a press that starts while interrupted is no press either */
	CHECK(ae_wheel_press_step(&press, 1, 1, 400, AE_WHEEL_NONE) == AE_PRESS_NOTHING);
	CHECK(ae_wheel_press_step(&press, 1, 0, 401, AE_WHEEL_NONE) == AE_PRESS_NOTHING);
	CHECK(ae_wheel_press_step(&press, 0, 0, 402, AE_WHEEL_NONE) == AE_PRESS_NOTHING);
	/* interrupted with the input zeroed, the button still held: down reads 0 till it ends. Held after: no press, and no tap */
	CHECK(ae_wheel_press_step(&press, 1, 0, 450, AE_WHEEL_NONE) == AE_PRESS_NOTHING);
	CHECK(ae_wheel_press_step(&press, 0, 1, 451, AE_WHEEL_NONE) == AE_PRESS_NOTHING);
	CHECK(ae_wheel_press_step(&press, 0, 1, 452, AE_WHEEL_NONE) == AE_PRESS_NOTHING);
	CHECK(ae_wheel_press_step(&press, 1, 0, 453, AE_WHEEL_NONE) == AE_PRESS_NOTHING);
	CHECK(ae_wheel_press_step(&press, 1, 0, 453 + AE_MARK_HOLD_TICKS, AE_WHEEL_NONE) == AE_PRESS_NOTHING);
	CHECK(ae_wheel_press_step(&press, 0, 0, 454 + AE_MARK_HOLD_TICKS, AE_WHEEL_NONE) == AE_PRESS_NOTHING);
	/* let go during it: the next press is whole */
	CHECK(ae_wheel_press_step(&press, 1, 0, 480, AE_WHEEL_NONE) == AE_PRESS_NOTHING);
	CHECK(ae_wheel_press_step(&press, 0, 1, 481, AE_WHEEL_NONE) == AE_PRESS_NOTHING);
	CHECK(ae_wheel_press_step(&press, 0, 0, 482, AE_WHEEL_NONE) == AE_PRESS_NOTHING);
	CHECK(ae_wheel_press_step(&press, 1, 0, 483, AE_WHEEL_NONE) == AE_PRESS_NOTHING);
	CHECK(ae_wheel_press_step(&press, 0, 0, 485, AE_WHEEL_NONE) == AE_PRESS_TAP);
	CHECK(ae_wheel_press_step(&press, 0, 0, 485 + AE_WHEEL_DOUBLE_TICKS + 1, AE_WHEEL_NONE) == AE_PRESS_NOTHING);
	/* an interruption between the taps of a double ends the first */
	CHECK(ae_wheel_press_step(&press, 1, 0, 500, AE_WHEEL_NONE) == AE_PRESS_NOTHING);
	CHECK(ae_wheel_press_step(&press, 0, 0, 502, AE_WHEEL_NONE) == AE_PRESS_TAP);
	CHECK(ae_wheel_press_step(&press, 0, 1, 504, AE_WHEEL_NONE) == AE_PRESS_NOTHING);
	CHECK(ae_wheel_press_step(&press, 1, 0, 506, AE_WHEEL_NONE) == AE_PRESS_NOTHING);
	CHECK(ae_wheel_press_step(&press, 0, 0, 508, AE_WHEEL_NONE) == AE_PRESS_TAP);
}

static void wheel_pick_test(void)
{
	int entry;

	/* under the travel: none; at it: an entry */
	CHECK(ae_wheel_pick(AE_WHEEL_NONE, 0.0f, 0.0f) == AE_WHEEL_NONE);
	CHECK(ae_wheel_pick(AE_WHEEL_NONE, 0.0f, 0.34f) == AE_WHEEL_NONE);
	CHECK(ae_wheel_pick(AE_WHEEL_NONE, 0.24f, 0.24f) == AE_WHEEL_NONE);
	CHECK(ae_wheel_pick(AE_WHEEL_SMART, 0.0f, 0.34f) == AE_WHEEL_NONE);
	CHECK(ae_wheel_pick(AE_WHEEL_NONE, 0.0f, 0.36f) == AE_WHEEL_SMART);
	CHECK(ae_wheel_pick(AE_WHEEL_NONE, 0.0f, 1.0f) == AE_WHEEL_SMART);
	/* the eight directions, clockwise from up */
	for (entry = 0; entry < AE_WHEEL_COUNT; entry++)
	{
		CHECK(pick_at(AE_WHEEL_NONE, 45.0f * (float)entry, 1.0f) == entry);
		CHECK(pick_at(AE_WHEEL_NONE, 45.0f * (float)entry, 0.4f) == entry);
		/* (and either side of its middle, nothing sticky yet) */
		CHECK(pick_at(AE_WHEEL_NONE, 45.0f * (float)entry + 20.0f, 1.0f) == entry);
		CHECK(pick_at(AE_WHEEL_NONE, 45.0f * (float)entry - 20.0f, 1.0f) == entry);
	}
	CHECK(ae_wheel_pick(AE_WHEEL_NONE, 1.0f, 0.0f) == AE_WHEEL_ENEMY);
	CHECK(ae_wheel_pick(AE_WHEEL_NONE, 0.0f, -1.0f) == AE_WHEEL_DEFEND);
	CHECK(ae_wheel_pick(AE_WHEEL_NONE, -1.0f, 0.0f) == AE_WHEEL_GOING);
	/* up and a little left is still the first entry, past 337.5 it is the last */
	CHECK(pick_at(AE_WHEEL_NONE, 350.0f, 1.0f) == AE_WHEEL_SMART);
	CHECK(pick_at(AE_WHEEL_NONE, 320.0f, 1.0f) == AE_WHEEL_WATCH);

	/* stickiness: the border between the first and second is at 22.5 degrees. Past it by 3 the current keeps; by 5 it changes */
	CHECK(pick_at(AE_WHEEL_NONE, 25.5f, 1.0f) == AE_WHEEL_ATTACK);
	CHECK(pick_at(AE_WHEEL_SMART, 25.5f, 1.0f) == AE_WHEEL_SMART);
	CHECK(pick_at(AE_WHEEL_SMART, 27.5f, 1.0f) == AE_WHEEL_ATTACK);
	/* and back the other way */
	CHECK(pick_at(AE_WHEEL_ATTACK, 19.5f, 1.0f) == AE_WHEEL_ATTACK);
	CHECK(pick_at(AE_WHEEL_ATTACK, 17.5f, 1.0f) == AE_WHEEL_SMART);
	/* across the top, where the angle wraps */
	CHECK(pick_at(AE_WHEEL_SMART, 360.0f - 25.5f, 1.0f) == AE_WHEEL_SMART);
	CHECK(pick_at(AE_WHEEL_SMART, 360.0f - 27.5f, 1.0f) == AE_WHEEL_WATCH);
	CHECK(pick_at(AE_WHEEL_WATCH, 360.0f - 27.5f, 1.0f) == AE_WHEEL_WATCH);
	CHECK(pick_at(AE_WHEEL_WATCH, 360.0f - 17.5f, 1.0f) == AE_WHEEL_SMART);
	CHECK(pick_at(AE_WHEEL_WATCH, 360.0f - 19.5f, 1.0f) == AE_WHEEL_WATCH);
	/* a current entry far away does not stick */
	CHECK(pick_at(AE_WHEEL_RIDE, 25.5f, 1.0f) == AE_WHEEL_ATTACK);
}

static void wheel_mouse_test(void)
{
	float x = 0.0f, y = 0.0f;

	/* the travel adds up, then clamps to a circle of 120 */
	ae_wheel_mouse_add(&x, &y, 30.0f, 40.0f);
	CHECK(x == 30.0f && y == 40.0f);
	ae_wheel_mouse_add(&x, &y, 0.0f, 20.0f);
	CHECK(x == 30.0f && y == 60.0f);
	ae_wheel_mouse_add(&x, &y, 300.0f, 0.0f);
	CHECK(fabsf(sqrtf(x * x + y * y) - 120.0f) < 0.01f);
	CHECK(x > 0.0f && y > 0.0f && x > y);
	x = 0.0f;
	y = 0.0f;
	ae_wheel_mouse_add(&x, &y, -500.0f, 0.0f);
	CHECK(fabsf(x + 120.0f) < 0.001f && y == 0.0f);
	/* the pull back works from the edge */
	ae_wheel_mouse_add(&x, &y, 10.0f, 0.0f);
	CHECK(fabsf(x + 110.0f) < 0.001f);

	/* focus from 40 pixels out: 39 is none, 40 is an entry */
	CHECK(ae_wheel_pick_mouse(AE_WHEEL_NONE, 0.0f, 39.0f) == AE_WHEEL_NONE);
	CHECK(ae_wheel_pick_mouse(AE_WHEEL_NONE, 0.0f, 40.0f) == AE_WHEEL_SMART);
	CHECK(ae_wheel_pick_mouse(AE_WHEEL_NONE, 40.0f, 0.0f) == AE_WHEEL_ENEMY);
	CHECK(ae_wheel_pick_mouse(AE_WHEEL_NONE, 0.0f, -40.0f) == AE_WHEEL_DEFEND);
	CHECK(ae_wheel_pick_mouse(AE_WHEEL_NONE, 0.0f, 120.0f) == AE_WHEEL_SMART);
	CHECK(ae_wheel_pick_mouse(AE_WHEEL_NONE, 27.0f, 28.0f) == AE_WHEEL_NONE);
	CHECK(ae_wheel_pick_mouse(AE_WHEEL_NONE, 29.0f, 29.0f) == AE_WHEEL_ATTACK);
	/* the stickiness is the same: 25.5 degrees keeps the first entry, 27.5 does not */
	CHECK(ae_wheel_pick_mouse(AE_WHEEL_SMART, 60.0f * sinf(25.5f * DEGREES_TO_RADIANS),
	                          60.0f * cosf(25.5f * DEGREES_TO_RADIANS)) == AE_WHEEL_SMART);
	CHECK(ae_wheel_pick_mouse(AE_WHEEL_SMART, 60.0f * sinf(27.5f * DEGREES_TO_RADIANS),
	                          60.0f * cosf(27.5f * DEGREES_TO_RADIANS)) == AE_WHEEL_ATTACK);
}

static void focus_grace_test(void)
{
	struct ae_wheel_focus focus = { 0 };
	long now;

	/* idle: none */
	CHECK(ae_wheel_focus_held(&focus, AE_WHEEL_NONE, 1, 0) == AE_WHEEL_NONE);
	/* a pick is the focus */
	CHECK(ae_wheel_focus_held(&focus, AE_WHEEL_GOING, 1, 10) == AE_WHEEL_GOING);
	CHECK(ae_wheel_focus_held(&focus, AE_WHEEL_SMART, 1, 11) == AE_WHEEL_SMART);
	/* the stick recentred: the focus stays 6 ticks, then clears */
	for (now = 20; now < 26; now++)
		CHECK(ae_wheel_focus_held(&focus, AE_WHEEL_NONE, 1, now) == AE_WHEEL_SMART);
	CHECK(ae_wheel_focus_held(&focus, AE_WHEEL_NONE, 1, 26) == AE_WHEEL_NONE);
	CHECK(ae_wheel_focus_held(&focus, AE_WHEEL_NONE, 1, 27) == AE_WHEEL_NONE);
	/* a pick within the grace takes over and the grace starts again after */
	CHECK(ae_wheel_focus_held(&focus, AE_WHEEL_ENEMY, 1, 30) == AE_WHEEL_ENEMY);
	CHECK(ae_wheel_focus_held(&focus, AE_WHEEL_NONE, 1, 31) == AE_WHEEL_ENEMY);
	CHECK(ae_wheel_focus_held(&focus, AE_WHEEL_NONE, 1, 34) == AE_WHEEL_ENEMY);
	CHECK(ae_wheel_focus_held(&focus, AE_WHEEL_WATCH, 1, 35) == AE_WHEEL_WATCH);
	for (now = 36; now < 42; now++)
		CHECK(ae_wheel_focus_held(&focus, AE_WHEEL_NONE, 1, now) == AE_WHEEL_WATCH);
	CHECK(ae_wheel_focus_held(&focus, AE_WHEEL_NONE, 1, 42) == AE_WHEEL_NONE);

	/* the mouse has none */
	CHECK(ae_wheel_focus_held(&focus, AE_WHEEL_RIDE, 0, 50) == AE_WHEEL_RIDE);
	CHECK(ae_wheel_focus_held(&focus, AE_WHEEL_NONE, 0, 51) == AE_WHEEL_NONE);
	CHECK(ae_wheel_focus_held(&focus, AE_WHEEL_NONE, 0, 52) == AE_WHEEL_NONE);
	/* and the stick after the mouse has nothing left to hold */
	CHECK(ae_wheel_focus_held(&focus, AE_WHEEL_NONE, 1, 53) == AE_WHEEL_NONE);
}

static void dead_entries_test(void)
{
	int entry;

	for (entry = 0; entry < AE_WHEEL_COUNT; entry++)
		CHECK(ae_wheel_entry_shown(entry, 0) == 1);
	CHECK(ae_wheel_entry_shown(AE_WHEEL_SMART, 1) == 1);
	CHECK(ae_wheel_entry_shown(AE_WHEEL_ENEMY, 1) == 1);
	CHECK(ae_wheel_entry_shown(AE_WHEEL_GOING, 1) == 1);
	CHECK(ae_wheel_entry_shown(AE_WHEEL_ATTACK, 1) == 0);
	CHECK(ae_wheel_entry_shown(AE_WHEEL_REGROUP, 1) == 0);
	CHECK(ae_wheel_entry_shown(AE_WHEEL_DEFEND, 1) == 0);
	CHECK(ae_wheel_entry_shown(AE_WHEEL_RIDE, 1) == 0);
	CHECK(ae_wheel_entry_shown(AE_WHEEL_WATCH, 1) == 0);
	/* none and out of range are not entries */
	CHECK(ae_wheel_entry_shown(AE_WHEEL_NONE, 0) == 0);
	CHECK(ae_wheel_entry_shown(AE_WHEEL_COUNT, 0) == 0);
}

static void legend_life_test(void)
{
	CHECK(ae_wheel_legend_shown(0, 0, 1) == 1);
	CHECK(ae_wheel_legend_shown(4, 0, 1) == 1);
	CHECK(ae_wheel_legend_shown(5, 0, 1) == 0);
	CHECK(ae_wheel_legend_shown(40, 0, 1) == 0);
	CHECK(ae_wheel_legend_shown(0, 1, 1) == 0);
	CHECK(ae_wheel_legend_shown(0, 0, 0) == 0);
}

/* the count a profile keeps: a call adds one, to the legend's own limit and no more; a file's text is read within that */
static void calls_kept_test(void)
{
	CHECK(ae_wheel_calls_after(0) == 1);
	CHECK(ae_wheel_calls_after(3) == 4);
	CHECK(ae_wheel_calls_after(4) == 5);
	CHECK(ae_wheel_calls_after(5) == 5);
	CHECK(ae_wheel_calls_after(99) == 5);
	CHECK(ae_wheel_calls_after(-3) == 1);
	CHECK(ae_wheel_calls_from_text("0") == 0);
	CHECK(ae_wheel_calls_from_text("3") == 3);
	CHECK(ae_wheel_calls_from_text("5") == 5);
	CHECK(ae_wheel_calls_from_text("40") == 5);
	CHECK(ae_wheel_calls_from_text("-2") == 0);
	CHECK(ae_wheel_calls_from_text("") == 0);
	CHECK(ae_wheel_calls_from_text("two") == 0);
	CHECK(ae_wheel_calls_from_text(NULL) == 0);
	CHECK(ae_wheel_calls_from_text("99999999999999999999") == 5);
	/* (the legend is shown until the fifth call, however the count came) */
	CHECK(ae_wheel_legend_shown(ae_wheel_calls_from_text("4"), 0, 1) == 1);
	CHECK(ae_wheel_legend_shown(ae_wheel_calls_from_text("7"), 0, 1) == 0);
}

int main(void)
{
	calls_kept_test();
	press_timing_test();
	press_double_test();
	wheel_interrupted_test();
	wheel_pick_test();
	wheel_mouse_test();
	focus_grace_test();
	dead_entries_test();
	legend_life_test();
	if (!failures)
		printf("ae_wheel_rules_test: ok\n");
	return failures != 0;
}
