/*
AE_CLOCK_SLEW_RULES.C

CLOCK SLEW's pure rules (ae_clock_slew_rules.h).
*/

#include "ae_clock_slew_rules.h"

void ae_clock_slew_samples_reset(struct ae_clock_slew_samples *samples)
{
	int index;

	samples->count = 0;
	samples->next = 0;
	for (index = 0; index < AE_CLOCK_SLEW_SAMPLES; index++)
		samples->values[index] = 0.0f;
}

void ae_clock_slew_samples_add(struct ae_clock_slew_samples *samples, float error)
{
	samples->values[samples->next] = error;
	samples->next = (samples->next + 1) % AE_CLOCK_SLEW_SAMPLES;
	if (samples->count < AE_CLOCK_SLEW_SAMPLES)
		samples->count++;
}

float ae_clock_slew_samples_median(struct ae_clock_slew_samples const *samples)
{
	float sorted[AE_CLOCK_SLEW_SAMPLES];
	int count = samples->count;
	int index;
	int other;

	if (count <= 0)
		return 0.0f;
	if (count > AE_CLOCK_SLEW_SAMPLES)
		count = AE_CLOCK_SLEW_SAMPLES;
	for (index = 0; index < count; index++)
		sorted[index] = samples->values[index];
	for (index = 1; index < count; index++)
	{
		float value = sorted[index];

		for (other = index; other > 0 && sorted[other - 1] > value; other--)
			sorted[other] = sorted[other - 1];
		sorted[other] = value;
	}
	if (count % 2)
		return sorted[count / 2];
	return (sorted[count / 2 - 1] + sorted[count / 2]) * 0.5f;
}

float ae_clock_slew_error(long host_time, float round_trip_ticks, long local_time)
{
	if (round_trip_ticks < 0.0f)
		round_trip_ticks = 0.0f;
	return (float)(host_time - local_time) + round_trip_ticks * 0.5f;
}

float ae_clock_slew_factor(float error_ticks)
{
	if (!(error_ticks >= (float)AE_CLOCK_SLEW_MINIMUM_TICKS) || error_ticks > (float)AE_CLOCK_SLEW_MAXIMUM_TICKS)
		return 1.0f;
	return 1.0f + ((float)AE_CLOCK_SLEW_MAXIMUM_PERCENT / 100.0f) * error_ticks / (float)AE_CLOCK_SLEW_MAXIMUM_TICKS;
}

long ae_clock_slew_burst(long ticks)
{
	if (ticks < 0)
		return 0;
	return ticks > AE_CLOCK_SLEW_BURST_TICKS ? AE_CLOCK_SLEW_BURST_TICKS : ticks;
}

float ae_clock_slew_carry_ticks(float ticks_elapsed, long ticks_run, long stock_ticks)
{
	float kept;

	/* (a number that is not one carries nothing) */
	if (!(ticks_elapsed > (float)ticks_run) || ticks_run < 0)
		return 0.0f;
	kept = ticks_elapsed < (float)stock_ticks ? ticks_elapsed : (float)stock_ticks;
	return kept > (float)ticks_run ? kept - (float)ticks_run : 0.0f;
}
