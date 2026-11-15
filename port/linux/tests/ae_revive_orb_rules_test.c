#include <math.h>
#include <stdio.h>
#include "ae_revive_orb_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	real red[3], blue[3], other[3];
	long t;

	/* team colours: red red, blue blue, others ice */
	ae_revive_orb_team_rgb(0, red);
	ae_revive_orb_team_rgb(1, blue);
	ae_revive_orb_team_rgb(7, other);
	CHECK(red[0] > red[2] && blue[2] > blue[0] && other[2] > 0.9f);
	/* the pulse stays 0..1 and repeats */
	for (t = -200; t < 400; t++)
	{
		real p = ae_revive_orb_pulse(t);

		CHECK(p >= 0.0f && p <= 1.0f);
		CHECK(fabs((double)p - (double)ae_revive_orb_pulse(t + AE_REVIVE_PULSE_TICKS)) < 0.001);
	}
	/* alpha: more at the rim than face on, never over 0.7, never negative */
	CHECK(ae_revive_orb_alpha(0.0f, 3, 100, 0.5f) > ae_revive_orb_alpha(1.0f, 3, 100, 0.5f));
	for (t = 0; t < 500; t += 7)
	{
		real a = ae_revive_orb_alpha(-0.3f, (int)(t % 80), t, 1.0f);

		CHECK(a > 0.0f && a <= 0.7f);
	}
	/* a body at the spot is the downed player's, one far off is not */
	CHECK(ae_revive_body_is_at_spot(0.3f, -0.3f, 0.4f));
	CHECK(!ae_revive_body_is_at_spot(2.0f, 0.0f, 0.0f) && !ae_revive_body_is_at_spot(0.0f, 0.0f, 1.5f));
	if (failures)
		return 1;
	printf("ae_revive_orb_rules_test: ok\n");
	return 0;
}
