#include <math.h>
#include <stdio.h>
#include "ae_mark_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static void press_test(void)
{
	struct ae_mark_press press = { 0 };
	int light = 0;
	long now;

	/* a tap marks when it ends, and the flashlight never sees it */
	CHECK(ae_mark_press_step(&press, 1, 0, 100, &light) == 0 && light == 0);
	CHECK(ae_mark_press_step(&press, 1, 0, 103, &light) == 0 && light == 0);
	CHECK(ae_mark_press_step(&press, 0, 0, 104, &light) == 1 && light == 1);
	/* a hold is the flashlight's from its hold time, and does not mark */
	CHECK(ae_mark_press_step(&press, 1, 0, 200, &light) == 0 && light == 0);
	CHECK(ae_mark_press_step(&press, 1, 0, 200 + AE_MARK_HOLD_TICKS - 1, &light) == 0 && light == 0);
	CHECK(ae_mark_press_step(&press, 1, 0, 200 + AE_MARK_HOLD_TICKS, &light) == 0 && light == 1);
	CHECK(ae_mark_press_step(&press, 0, 0, 200 + AE_MARK_HOLD_TICKS + 1, &light) == 0 && light == 1);
	/* a press something else takes passes through and does not mark, even when short */
	CHECK(ae_mark_press_step(&press, 1, 1, 300, &light) == 0 && light == 1);
	CHECK(ae_mark_press_step(&press, 0, 0, 301, &light) == 0 && light == 1);
	/* taken is read at the press's start only */
	CHECK(ae_mark_press_step(&press, 1, 0, 400, &light) == 0);
	CHECK(ae_mark_press_step(&press, 1, 1, 401, &light) == 0 && light == 0);
	CHECK(ae_mark_press_step(&press, 0, 0, 402, &light) == 1);
	/* idle: nothing, and the flashlight sees its (up) button */
	for (now = 500; now < 510; now++)
		CHECK(ae_mark_press_step(&press, 0, 0, now, &light) == 0 && light == 1);
}

static void life_test(void)
{
	CHECK(ae_mark_alpha(-1) == 0.0f);
	CHECK(ae_mark_alpha(0) > 0.0f && ae_mark_alpha(0) < 1.0f);
	CHECK(ae_mark_alpha(AE_MARK_APPEAR_TICKS) == 1.0f);
	CHECK(ae_mark_alpha(AE_MARK_FADE_AT_TICKS - 1) == 1.0f);
	CHECK(ae_mark_alpha(AE_MARK_LIFE_TICKS - 1) > 0.0f && ae_mark_alpha(AE_MARK_LIFE_TICKS - 1) < 0.1f);
	CHECK(ae_mark_alpha(AE_MARK_LIFE_TICKS) == 0.0f);
	CHECK(ae_mark_appear_scale(0) > 1.1f && ae_mark_appear_scale(0) <= 1.35f);
	CHECK(ae_mark_appear_scale(AE_MARK_APPEAR_TICKS - 1) == 1.0f);
	CHECK(ae_mark_appear_scale(AE_MARK_APPEAR_TICKS) == 1.0f);
}

static void clamp_test(void)
{
	float x = 10.0f, y = -5.0f;

	/* inside and on screen: stays */
	CHECK(ae_mark_clamp(&x, &y, 100.0f, 50.0f, 0) == 0 && x == 10.0f && y == -5.0f);
	/* outside: onto the ellipse, the same direction */
	x = 300.0f;
	y = 0.0f;
	CHECK(ae_mark_clamp(&x, &y, 100.0f, 50.0f, 0) == 1 && fabsf(x - 100.0f) < 0.01f && y == 0.0f);
	/* off screen, however near the middle: onto the ellipse */
	x = 0.0f;
	y = -2.0f;
	CHECK(ae_mark_clamp(&x, &y, 100.0f, 50.0f, 1) == 1 && x == 0.0f && fabsf(y + 50.0f) < 0.01f);
	/* dead behind */
	x = 0.0f;
	y = 0.0f;
	CHECK(ae_mark_clamp(&x, &y, 100.0f, 50.0f, 1) == 1 && y == 50.0f);
}

static void same_test(void)
{
	CHECK(ae_mark_same(AE_MARK_KIND_PLACE, -1, AE_MARK_KIND_PLACE, -1, 0.1f) == 1);
	CHECK(ae_mark_same(AE_MARK_KIND_PLACE, -1, AE_MARK_KIND_PLACE, -1, 1.0f) == 0);
	CHECK(ae_mark_same(AE_MARK_KIND_ENEMY, 5, AE_MARK_KIND_ENEMY, 5, 99.0f) == 1);
	CHECK(ae_mark_same(AE_MARK_KIND_ENEMY, 5, AE_MARK_KIND_ENEMY, 6, 0.0f) == 0);
	CHECK(ae_mark_same(AE_MARK_KIND_ITEM, 5, AE_MARK_KIND_ENEMY, 5, 0.0f) == 0);
}

static void spam_test(void)
{
	struct ae_mark_spam spam = { 0 };
	long now = 1000;
	int index;

	for (index = 1; index < AE_MARK_SPAM_LIMIT; index++)
		CHECK(ae_mark_spam_take(&spam, now++) == (index == AE_MARK_SPAM_WARN_AT ? AE_MARK_SPAM_WARN : AE_MARK_SPAM_OK));
	CHECK(ae_mark_spam_take(&spam, now) == AE_MARK_SPAM_NOW_BLOCKED);
	CHECK(ae_mark_spam_take(&spam, now + 1) == AE_MARK_SPAM_STILL_BLOCKED);
	CHECK(ae_mark_spam_take(&spam, now + AE_MARK_SPAM_BLOCK_TICKS - 1) == AE_MARK_SPAM_STILL_BLOCKED);
	CHECK(ae_mark_spam_take(&spam, now + AE_MARK_SPAM_BLOCK_TICKS) == AE_MARK_SPAM_OK);
	/* marks out of the window do not count: a steady pace never blocks */
	{
		struct ae_mark_spam steady = { 0 };

		for (index = 0; index < 200; index++)
			CHECK(ae_mark_spam_take(&steady, 5000 + index * 60) == AE_MARK_SPAM_OK);
	}
}

int main(void)
{
	spam_test();
	press_test();
	life_test();
	clamp_test();
	same_test();
	if (!failures)
		printf("ae_mark_rules_test: ok\n");
	return failures != 0;
}
