#include <stdio.h>
#include <math.h>
#include "ae_dev_hit_markers_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
#define NEAR(a, b) (fabsf((a) - (b)) < 0.001f)

int main(void)
{
	float alpha, scale;

	/* a hit: full at once, held, fading, then over */
	ae_dev_hit_marker_shape(0, 0, &alpha, &scale);
	CHECK(NEAR(alpha, 1.0f) && NEAR(scale, 1.0f));
	ae_dev_hit_marker_shape(99, 0, &alpha, &scale);
	CHECK(NEAR(alpha, 1.0f));
	ae_dev_hit_marker_shape(150, 0, &alpha, &scale);
	CHECK(NEAR(alpha, 0.5f));
	ae_dev_hit_marker_shape(200, 0, &alpha, &scale);
	CHECK(NEAR(alpha, 0.0f));
	ae_dev_hit_marker_shape(-1, 0, &alpha, &scale);
	CHECK(NEAR(alpha, 0.0f));

	/* a kill: pops in at 125 %, settles to 100 % by 80 ms, held to 280 ms, gone at 430 ms */
	ae_dev_hit_marker_shape(0, 1, &alpha, &scale);
	CHECK(NEAR(alpha, 1.0f) && NEAR(scale, 1.25f));
	ae_dev_hit_marker_shape(40, 1, &alpha, &scale);
	CHECK(scale < 1.25f && scale > 1.0f);
	ae_dev_hit_marker_shape(80, 1, &alpha, &scale);
	CHECK(NEAR(scale, 1.0f));
	ae_dev_hit_marker_shape(279, 1, &alpha, &scale);
	CHECK(NEAR(alpha, 1.0f));
	ae_dev_hit_marker_shape(355, 1, &alpha, &scale);
	CHECK(NEAR(alpha, 0.5f));
	ae_dev_hit_marker_shape(430, 1, &alpha, &scale);
	CHECK(NEAR(alpha, 0.0f));

	/* what counts: this machine's player's damage to anything else */
	CHECK(ae_dev_hit_marker_counts(1, 0, 0, 10.0f, 0, 0) == 1);
	CHECK(ae_dev_hit_marker_counts(0, 0, 0, 10.0f, 0, 0) == 0);
	CHECK(ae_dev_hit_marker_counts(1, 1, 0, 10.0f, 0, 0) == 0);
	CHECK(ae_dev_hit_marker_counts(1, 0, 1, 10.0f, 0, 0) == 0);
	CHECK(ae_dev_hit_marker_counts(1, 0, 0, 0.0f, 0, 0) == 0);
	CHECK(ae_dev_hit_marker_counts(1, 0, 0, 0.0f, 1, 0) == 1);
	/* a body already dead: only its kill */
	CHECK(ae_dev_hit_marker_counts(1, 0, 0, 10.0f, 0, 1) == 0);
	CHECK(ae_dev_hit_marker_counts(1, 0, 0, 10.0f, 1, 1) == 1);

	/* the size: the reticle's drawn half-extent plus the gap, else the default, at the HUD's scale */
	CHECK(NEAR(ae_dev_hit_marker_half_size(40.0f, 24.0f, 6.0f, 2.0f), 52.0f));
	CHECK(NEAR(ae_dev_hit_marker_half_size(0.0f, 24.0f, 6.0f, 2.0f), 60.0f));

	if (failures)
		return 1;
	printf("ae_dev_hit_markers_rules: all passed\n");
	return 0;
}
