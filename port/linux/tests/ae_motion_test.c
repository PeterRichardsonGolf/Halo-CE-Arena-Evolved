#include <math.h>
#include <stdio.h>
#include "ae_motion.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static int near(float a, float b)
{
	return fabsf(a - b) <= 1e-4f;
}

int main(void)
{
	struct ae_motion m = { 0, 0, 0.0f, 0.0f };
	float old_x, old_alpha, new_x, new_alpha, scrim, scale, alpha;

	/* cubic ease-out, t clamped */
	CHECK(ae_ease_out(0) == 0 && ae_ease_out(1) == 1 && near(ae_ease_out(.5f), .875f));
	CHECK(ae_ease_out(-1) == 0 && ae_ease_out(2) == 1);
	/* the spec 7 timings */
	CHECK(AE_MOTION_SELECTION_MS == 100 && AE_MOTION_SCROLL_MS == 100 && AE_MOTION_SCREEN_MS == 200 &&
		AE_MOTION_DIALOG_OPEN_MS == 150 && AE_MOTION_DIALOG_CLOSE_MS == 100 && AE_MOTION_VALUE_TICK_MS == 120 &&
		AE_MOTION_CARET_MS == 530 && AE_MOTION_SCREEN_SHIFT_U == 120 && AE_MOTION_VALUE_SHIFT_U == 12);
	/* a 100 ms motion from 0 to 10 at +50 ms is 8.75 */
	ae_motion_set_reduced(0);
	ae_motion_set_now(1000);
	CHECK(ae_motion_now() == 1000 && !ae_motion_reduced());
	CHECK(!ae_motion_running(&m) && ae_motion_value(&m) == 0.0f);
	ae_motion_start(&m, 0, 10, 100);
	CHECK(ae_motion_running(&m) && ae_motion_value(&m) == 0.0f && ae_motion_progress(&m) == 0.0f);
	ae_motion_set_now(1050);
	CHECK(near(ae_motion_value(&m), 8.75f) && near(ae_motion_progress(&m), .5f) && ae_motion_running(&m));
	/* restarting it mid-way to 20 starts from 8.75 (no jump) */
	ae_motion_start(&m, 0, 20, 100);
	CHECK(near(ae_motion_value(&m), 8.75f) && ae_motion_running(&m));
	ae_motion_set_now(1150);
	CHECK(ae_motion_value(&m) == 20.0f && !ae_motion_running(&m) && ae_motion_progress(&m) == 1.0f);
	/* not running: a start begins at from */
	ae_motion_start(&m, 5, 15, 100);
	CHECK(ae_motion_value(&m) == 5.0f);
	/* finish jumps to the end */
	ae_motion_finish(&m);
	CHECK(ae_motion_value(&m) == 15.0f && !ae_motion_running(&m));
	/* the clock wraps */
	ae_motion_set_now((unsigned long)-20);
	ae_motion_start(&m, 0, 10, 100);
	ae_motion_set_now(30);
	CHECK(near(ae_motion_value(&m), 8.75f));
	/* reduced: the value is `to` at once and nothing runs; a motion already running ends too */
	ae_motion_set_now(2000);
	ae_motion_start(&m, 0, 10, 100);
	ae_motion_set_reduced(1);
	CHECK(ae_motion_reduced() && ae_motion_value(&m) == 10.0f && !ae_motion_running(&m));
	ae_motion_start(&m, 0, 30, 100);
	CHECK(ae_motion_value(&m) == 30.0f && !ae_motion_running(&m) && ae_motion_progress(&m) == 1.0f);
	ae_motion_set_reduced(0);
	/* the caret: 530 ms on, 530 off; reduced always on */
	ae_motion_set_now(5000);
	CHECK(ae_caret_visible(5000) == 1);
	ae_motion_set_now(5529); CHECK(ae_caret_visible(5000) == 1);
	ae_motion_set_now(5530); CHECK(ae_caret_visible(5000) == 0);
	ae_motion_set_now(6059); CHECK(ae_caret_visible(5000) == 0);
	ae_motion_set_now(6060); CHECK(ae_caret_visible(5000) == 1);
	ae_motion_set_reduced(1);
	ae_motion_set_now(5530); CHECK(ae_caret_visible(5000) == 1);
	ae_motion_set_reduced(0);
	/* screen forward / back */
	ae_motion_screen(0, 1, &old_x, &old_alpha, &new_x, &new_alpha);
	CHECK(old_x == 0 && old_alpha == 1 && near(new_x, 120) && new_alpha == 0);
	ae_motion_screen(.6f, 1, &old_x, &old_alpha, &new_x, &new_alpha);
	CHECK(old_alpha == 0 && near(old_x, -120 * ae_ease_out(.6f)) && near(new_alpha, .75f));
	ae_motion_screen(.3f, 1, &old_x, &old_alpha, &new_x, &new_alpha);
	CHECK(near(old_alpha, .5f) && near(new_x, 120 * (1 - ae_ease_out(.3f))));
	ae_motion_screen(.8f, 1, &old_x, &old_alpha, &new_x, &new_alpha);
	CHECK(new_alpha == 1);
	ae_motion_screen(1, 1, &old_x, &old_alpha, &new_x, &new_alpha);
	CHECK(new_x == 0 && old_x == -120 && new_alpha == 1 && old_alpha == 0);
	ae_motion_screen(1, -1, &old_x, &old_alpha, &new_x, &new_alpha);
	CHECK(new_x == 0 && old_x == 120);
	ae_motion_screen(0, -1, &old_x, &old_alpha, &new_x, &new_alpha);
	CHECK(near(new_x, -120));
	/* dialog open / close */
	ae_motion_dialog(0, 1, &scrim, &scale, &alpha);
	CHECK(scrim == 0 && near(scale, .96f) && alpha == 0);
	ae_motion_dialog(1, 1, &scrim, &scale, &alpha);
	CHECK(scrim == 1 && scale == 1 && alpha == 1);
	ae_motion_dialog(.5f, 1, &scrim, &scale, &alpha);
	CHECK(near(scrim, .875f) && near(scale, .96f + .04f * .875f) && near(alpha, .875f));
	ae_motion_dialog(0, 0, &scrim, &scale, &alpha);
	CHECK(scrim == 1 && scale == 1 && alpha == 1);
	ae_motion_dialog(1, 0, &scrim, &scale, &alpha);
	CHECK(scrim == 0 && near(scale, .96f) && alpha == 0);
	/* value tick: the pressed side */
	ae_motion_value_tick(0, 1, &old_x, &old_alpha, &new_x, &new_alpha);
	CHECK(old_x == 0 && old_alpha == 1 && near(new_x, 12) && new_alpha == 0);
	ae_motion_value_tick(1, 1, &old_x, &old_alpha, &new_x, &new_alpha);
	CHECK(near(old_x, -12) && old_alpha == 0 && new_x == 0 && new_alpha == 1);
	ae_motion_value_tick(0, -1, &old_x, &old_alpha, &new_x, &new_alpha);
	CHECK(near(new_x, -12));
	if (failures)
		printf("%d failures\n", failures);
	return failures ? 1 : 0;
}
