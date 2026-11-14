#include <stdio.h>

#include "ae_clock_slew_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
#define NEAR(a, b) ((a) - (b) < 0.0001f && (b) - (a) < 0.0001f)

int main(void)
{
	struct ae_clock_slew_samples samples;
	float error;
	int index;

	/* the error: host time + half the round trip - local time */
	CHECK(NEAR(ae_clock_slew_error(100, 6.0f, 90), 13.0f));
	CHECK(NEAR(ae_clock_slew_error(100, 0.0f, 103), -3.0f));
	CHECK(NEAR(ae_clock_slew_error(100, -4.0f, 100), 0.0f));

	/* the factor: 1 under 3 and over 30; in proportion between; never below 1 */
	CHECK(NEAR(ae_clock_slew_factor(-50.0f), 1.0f));
	CHECK(NEAR(ae_clock_slew_factor(0.0f), 1.0f));
	CHECK(NEAR(ae_clock_slew_factor(2.99f), 1.0f));
	CHECK(NEAR(ae_clock_slew_factor(3.0f), 1.005f));
	CHECK(NEAR(ae_clock_slew_factor(15.0f), 1.025f));
	CHECK(NEAR(ae_clock_slew_factor(30.0f), 1.05f));
	CHECK(NEAR(ae_clock_slew_factor(30.01f), 1.0f));
	CHECK(NEAR(ae_clock_slew_factor(1000.0f), 1.0f));
	{
		float nan = 0.0f;

		nan = nan / nan;
		CHECK(NEAR(ae_clock_slew_factor(nan), 1.0f));
	}
	for (index = -100; index <= 200; index++)
	{
		float factor = ae_clock_slew_factor((float)index * 0.5f);

		CHECK(factor >= 1.0f && factor <= 1.0501f);
		/* the host's check is at 1.1 */
		CHECK(factor < 1.1f);
	}

	/* the median: none, one, odd, even, the oldest replaced after 8, a wild one ignored */
	ae_clock_slew_samples_reset(&samples);
	CHECK(NEAR(ae_clock_slew_samples_median(&samples), 0.0f));
	ae_clock_slew_samples_add(&samples, 7.0f);
	CHECK(NEAR(ae_clock_slew_samples_median(&samples), 7.0f));
	ae_clock_slew_samples_add(&samples, 1.0f);
	CHECK(NEAR(ae_clock_slew_samples_median(&samples), 4.0f));
	ae_clock_slew_samples_add(&samples, 5.0f);
	CHECK(NEAR(ae_clock_slew_samples_median(&samples), 5.0f));
	for (index = 0; index < 20; index++)
		ae_clock_slew_samples_add(&samples, 10.0f);
	CHECK(samples.count == AE_CLOCK_SLEW_SAMPLES);
	CHECK(NEAR(ae_clock_slew_samples_median(&samples), 10.0f));
	ae_clock_slew_samples_add(&samples, 500.0f);
	ae_clock_slew_samples_add(&samples, -500.0f);
	ae_clock_slew_samples_add(&samples, 500.0f);
	error = ae_clock_slew_samples_median(&samples);
	CHECK(NEAR(error, 10.0f));
	/* 4 wild high of 8 (and one low): the middle two decide */
	ae_clock_slew_samples_add(&samples, 500.0f);
	CHECK(NEAR(ae_clock_slew_samples_median(&samples), 10.0f));
	ae_clock_slew_samples_add(&samples, 500.0f);
	CHECK(NEAR(ae_clock_slew_samples_median(&samples), 255.0f));
	ae_clock_slew_samples_reset(&samples);
	CHECK(samples.count == 0 && NEAR(ae_clock_slew_samples_median(&samples), 0.0f));

	/* the burst bound */
	CHECK(ae_clock_slew_burst(-5) == 0);
	CHECK(ae_clock_slew_burst(0) == 0);
	CHECK(ae_clock_slew_burst(3) == 3);
	CHECK(ae_clock_slew_burst(8) == 8);
	CHECK(ae_clock_slew_burst(9) == 8);
	CHECK(ae_clock_slew_burst(30) == 8);
	/* the carry: a frame that ran 8 of 20 ticks keeps 12; past the stock 30 only up to 30; none when all ran */
	CHECK(ae_clock_slew_carry_ticks(20.0f, 8, 30) == 12.0f);
	CHECK(ae_clock_slew_carry_ticks(45.0f, 8, 30) == 22.0f);
	CHECK(ae_clock_slew_carry_ticks(6.0f, 6, 30) == 0.0f);
	CHECK(ae_clock_slew_carry_ticks(3.0f, 8, 30) == 0.0f);
	CHECK(ae_clock_slew_carry_ticks(0.0f / 0.0f, 8, 30) == 0.0f);

	if (failures)
		return 1;
	printf("ae_clock_slew_rules_test: ok\n");
	return 0;
}
