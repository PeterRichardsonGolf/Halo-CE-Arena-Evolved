#include <math.h>
#include <stdio.h>
#include "ae_mouse.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
#define NEAR(a, b, tolerance) (fabs((double)(a) - (double)(b)) <= (tolerance))

#define PI 3.14159265358979323846

int main(void)
{
	float yaw, pitch, x, y;

	/* the style text */
	CHECK(ae_mouse_style_from_text("mcc") == AE_MOUSE_MCC);
	CHECK(ae_mouse_style_from_text("MCC") == AE_MOUSE_MCC);
	CHECK(ae_mouse_style_from_text("classic") == AE_MOUSE_CLASSIC);
	CHECK(ae_mouse_style_from_text("") == AE_MOUSE_CLASSIC);
	CHECK(ae_mouse_style_from_text("mccx") == AE_MOUSE_CLASSIC);
	CHECK(ae_mouse_style_from_text("mc") == AE_MOUSE_CLASSIC);
	CHECK(ae_mouse_style_from_text(NULL) == AE_MOUSE_CLASSIC);

	/* classic 1.0 is 0.0022 rad per count exactly as xinput_sdl.c had it */
	CHECK(ae_mouse_classic_radians(1.0f) == 0.0022f);
	CHECK(ae_mouse_classic_radians(2.5f) == 0.0022f * 2.5f);
	/* MCC 1.0 is pi / 8100 rad per count (1/45 degree) */
	CHECK(NEAR(ae_mouse_mcc_radians(1.0f), PI / 8100.0, 1e-9));
	CHECK(NEAR(ae_mouse_mcc_radians(1.4f) * 180.0 / PI, 1.4 / 45.0, 1e-6));
	/* MCC 1.0 is 1.01% above Source's 0.022 degrees per count (1/45 = 0.02222) */
	CHECK(NEAR(ae_mouse_mcc_radians(1.0f) * 180.0 / PI / 0.022, 1.0, 0.0102));
	/* MCC 5.67 turns as classic 1.0, within 1% */
	CHECK(NEAR(ae_mouse_mcc_radians(5.67f) / ae_mouse_classic_radians(1.0f), 1.0, 0.01));

	/* seeding: classic x 5.6723, one decimal, 0.1 to 10 */
	CHECK(ae_mouse_seed(1.0f) == 5.7f);
	CHECK(ae_mouse_seed(0.1f) == 0.6f);
	CHECK(ae_mouse_seed(4.0f) == 10.0f); /* 22.7 before the clamp */
	CHECK(ae_mouse_seed(0.5f) == 2.8f);
	CHECK(ae_mouse_seed(0.0f) == 5.7f);
	CHECK(ae_mouse_seed(-3.0f) == 5.7f);

	/* the clamps */
	CHECK(ae_mouse_clamp_sensitivity(0.0f) == 0.1f);
	CHECK(ae_mouse_clamp_sensitivity(99.0f) == 10.0f);
	CHECK(ae_mouse_clamp_sensitivity(1.6f) == 1.6f);
	CHECK(ae_mouse_clamp_scale(0.0f) == 0.1f);
	CHECK(ae_mouse_clamp_scale(5.0f) == 2.0f);
	CHECK(ae_mouse_clamp_scale(0.7f) == 0.7f);

	/* NaN and infinity are pinned, not passed on */
	CHECK(ae_mouse_clamp_sensitivity(NAN) == 0.1f && ae_mouse_clamp_sensitivity(INFINITY) == 10.0f);
	CHECK(ae_mouse_clamp_scale(NAN) == 0.1f && ae_mouse_clamp_scale(INFINITY) == 2.0f);

	/* the zoom and vehicle factors are 1 in classic style whatever the keys say */
	CHECK(ae_mouse_zoom_factor(AE_MOUSE_CLASSIC, 0.5f, 1) == 1.0f);
	CHECK(ae_mouse_zoom_factor(AE_MOUSE_CLASSIC, 2.0f, 1) == 1.0f);
	CHECK(ae_mouse_vehicle_factor(AE_MOUSE_CLASSIC, 0.5f, 1) == 1.0f);
	CHECK(ae_mouse_vehicle_factor(AE_MOUSE_CLASSIC, 2.0f, 1) == 1.0f);
	/* in MCC style they scale only when zoomed or in a vehicle */
	CHECK(ae_mouse_zoom_factor(AE_MOUSE_MCC, 0.5f, 1) == 0.5f);
	CHECK(ae_mouse_zoom_factor(AE_MOUSE_MCC, 0.5f, 0) == 1.0f);
	CHECK(ae_mouse_vehicle_factor(AE_MOUSE_MCC, 1.5f, 1) == 1.5f);
	CHECK(ae_mouse_vehicle_factor(AE_MOUSE_MCC, 1.5f, 0) == 1.0f);
	CHECK(ae_mouse_zoom_factor(AE_MOUSE_MCC, 9.0f, 1) == 2.0f);

	/* MCC yaw equals pitch for the same counts (the vertical key is not an input), the signs as classic's */
	x = 100.0f;
	y = 100.0f;
	ae_mouse_mcc_look(1.4f, x, y, 0, &yaw, &pitch);
	CHECK(yaw == pitch);
	CHECK(yaw < 0.0f);
	CHECK(NEAR(-yaw * 180.0 / PI, 100.0 * 1.4 / 45.0, 1e-4));
	ae_mouse_mcc_look(1.4f, x, y, 1, &yaw, &pitch);
	CHECK(yaw < 0.0f && pitch > 0.0f && NEAR(-yaw, pitch, 1e-9));
	ae_mouse_mcc_look(1.4f, 0.0f, 0.0f, 0, &yaw, &pitch);
	CHECK(yaw == 0.0f && pitch == 0.0f);
	/* a value out of range is pinned */
	ae_mouse_mcc_look(0.0f, 10.0f, 0.0f, 0, &yaw, &pitch);
	CHECK(NEAR(-yaw, 10.0 * ae_mouse_mcc_radians(0.1f), 1e-9));

	/* the test verb */
	CHECK(ae_mouse_test_parse("mouse:100,0", &x, &y) && x == 100.0f && y == 0.0f);
	CHECK(ae_mouse_test_parse("mouse:-40,12.5", &x, &y) && x == -40.0f && y == 12.5f);
	CHECK(!ae_mouse_test_parse("mouse:100", &x, &y));
	CHECK(!ae_mouse_test_parse("mouse:1,2x", &x, &y));
	CHECK(!ae_mouse_test_parse("menu:a", &x, &y));
	CHECK(!ae_mouse_test_parse("", &x, &y));
	CHECK(!ae_mouse_test_parse(NULL, &x, &y));

	if (failures)
		return 1;
	printf("ae_mouse_test: all passed\n");
	return 0;
}
