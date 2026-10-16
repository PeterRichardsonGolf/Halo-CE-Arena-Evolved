/* HUD STYLE AE's motion sensor, the pure parts (port/linux/game/ae_sensor_geometry.c): spec 14.15 part 4, G3, G4 */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ae_sensor_geometry.h"

static int failures;
static int checks;
#define AE_CHECK(c) do { checks++; if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } \
	while (0)
#define AE_NEAR(a, b, d) do { double a_ = (a), b_ = (b); checks++; if (fabs(a_ - b_) > (d)) { \
	printf("FAIL %s:%d %s = %.3f, wanted %.3f\n", __FILE__, __LINE__, #a, a_, b_); failures++; } } while (0)

#define DEGREES(d) ((d) * 3.14159265 / 180.0)

int main(void)
{
	/* CE's FOV 70 is 70 degrees across a 4:3 view: 55.4 vertical; on a 16:9 view the wedge's half-angle is 43 */
	real vertical70 = (real)(2.0 * atan(tan(DEGREES(35.0)) * 3.0 / 4.0));
	real half, dx, dy, r, a;
	char label[8];

	half = ae_sensor_wedge_half_angle(vertical70, 1920, 1080);
	AE_NEAR(half, 43.0, 1.0);
	AE_CHECK(ae_sensor_wedge_half_angle(vertical70, 2560, 1080) > half + 5.0f);      /* 21:9 wider */
	AE_CHECK(ae_sensor_wedge_half_angle(vertical70 / 2.0f, 1920, 1080) < half - 15.0f);  /* a 2x scope narrower */
	AE_NEAR(ae_sensor_wedge_half_angle(0.001f, 1920, 1080), 5.0, 0.001);           /* clamped */
	AE_NEAR(ae_sensor_wedge_half_angle(3.1f, 1920, 1080), 85.0, 0.001);
	AE_NEAR(ae_sensor_wedge_half_angle(vertical70, 1920, 0), 43.0, 43.0);          /* never divides by zero */
	{
		/* FOV 110 on 16:9 */
		real vertical110 = (real)(2.0 * atan(tan(DEGREES(55.0)) * 3.0 / 4.0));
		AE_CHECK(ae_sensor_wedge_half_angle(vertical110, 1920, 1080) > 60.0f);
	}

	AE_NEAR(ae_sensor_inner_ring(92), 0.616 * 92, 0.01);
	AE_NEAR(pow(0.5, 0.7), 0.616, 0.001);                                          /* half the range */
	AE_CHECK(ae_sensor_side_dividers_shown(43.0f));
	AE_CHECK(ae_sensor_side_dividers_shown(75.0f));
	AE_CHECK(!ae_sensor_side_dividers_shown(75.5f));

	/* forward is up, left is left; the sensor's scale is the rim */
	/* (left: +x; ahead: -y; behind: +y) */
	ae_sensor_blip_offset(32, 0, 32, 92, &dx, &dy); AE_NEAR(dx, -92, 0.001); AE_NEAR(dy, 0, 0.001);
	ae_sensor_blip_offset(0, -16, 32, 92, &dx, &dy); AE_NEAR(dx, 0, 0.001); AE_NEAR(dy, -46, 0.001);
	ae_sensor_blip_offset(0, 16, 32, 92, &dx, &dy); AE_NEAR(dx, 0, 0.001); AE_NEAR(dy, 46, 0.001);
	ae_sensor_blip_offset(-16, 0, 32, 92, &dx, &dy); AE_NEAR(dx, 46, 0.001);
	ae_sensor_blip_offset(5, 5, 0, 92, &dx, &dy); AE_NEAR(dx, 0, 0.001); AE_NEAR(dy, 0, 0.001);  /* no scale */

	ae_sensor_sweep(0.0f, 92, &r, &a); AE_NEAR(r, 0, 0.01); AE_NEAR(a, 0.30, 0.001);
	ae_sensor_sweep(2.0999f, 92, &r, &a); AE_NEAR(r, 92, 0.1); AE_NEAR(a, 0.12, 0.001);
	ae_sensor_sweep(2.1f + 1.05f, 92, &r, &a); AE_NEAR(r, 46, 0.1);                 /* every 2.1 s */

	ae_sensor_range_label(20.0f, label); AE_CHECK(!strcmp(label, "15m"));          /* CE's own art: 20 units = 15m */
	ae_sensor_range_label(-3.0f, label); AE_CHECK(!strcmp(label, "0m"));
	ae_sensor_range_label(1.0e9f, label); AE_CHECK(!strcmp(label, "999m"));

	printf("%d checks, %d failures\n", checks, failures);
	return failures ? 1 : 0;
}
