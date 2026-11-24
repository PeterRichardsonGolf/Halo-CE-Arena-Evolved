/*
AE_GYRO_RULES.C

Gyro aiming's arithmetic (ae_gyro_rules.h). No SDL here
(port/linux/tests/ae_gyro_rules_test.c).
*/

#include "ae_gyro_rules.h"

static float axis_turn(
	float rate,
	double seconds)
{
	if (rate > -AE_GYRO_RULES_STILL && rate < AE_GYRO_RULES_STILL)
		return 0.0f;
	return (float)(rate * seconds);
}

void ae_gyro_rules_turn(
	float const rate[3],
	double seconds,
	float *yaw,
	float *pitch)
{
	*yaw = 0.0f;
	*pitch = 0.0f;
	if (!(seconds > 0.0))
		return;
	if (seconds > AE_GYRO_RULES_LONGEST_STEP)
		seconds = AE_GYRO_RULES_LONGEST_STEP;
	*yaw = axis_turn(rate[1], seconds);
	*pitch = axis_turn(rate[0], seconds);
}
