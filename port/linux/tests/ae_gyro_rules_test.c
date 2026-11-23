#include <math.h>
#include <stdio.h>
#include "ae_gyro_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static int near(float a, float b)
{
	return fabsf(a - b) < 1e-6f;
}

int main(void)
{
	float yaw, pitch;

	/* the pad turned left (yaw rate up) and its front tilted up (pitch rate up), 1 rad/s for 20 ms: 0.02 rad each;
	the roll rate (z) turns nothing */
	{
		static const float rate[3] = { 1.0f, 1.0f, 5.0f };

		ae_gyro_rules_turn(rate, 0.02, &yaw, &pitch);
		CHECK(near(yaw, 0.02f) && near(pitch, 0.02f));
	}
	/* turned right, front down: both negative */
	{
		static const float rate[3] = { -2.0f, -0.5f, 0.0f };

		ae_gyro_rules_turn(rate, 0.01, &yaw, &pitch);
		CHECK(near(yaw, -0.005f) && near(pitch, -0.02f));
	}
	/* a pad at rest drifts under AE_GYRO_RULES_STILL: no turn on either axis, the other axis still turns */
	{
		static const float rate[3] = { 0.009f, -0.009f, 0.0f };
		static const float one[3] = { 0.5f, 0.009f, 0.0f };

		ae_gyro_rules_turn(rate, 0.02, &yaw, &pitch);
		CHECK(yaw == 0.0f && pitch == 0.0f);
		ae_gyro_rules_turn(one, 0.02, &yaw, &pitch);
		CHECK(yaw == 0.0f && near(pitch, 0.01f));
	}
	/* a read after a pause covers AE_GYRO_RULES_LONGEST_STEP at most; no time or time going back: nothing */
	{
		static const float rate[3] = { 0.0f, 3.0f, 0.0f };

		ae_gyro_rules_turn(rate, 2.0, &yaw, &pitch);
		CHECK(near(yaw, (float)(3.0 * AE_GYRO_RULES_LONGEST_STEP)) && pitch == 0.0f);
		ae_gyro_rules_turn(rate, 0.0, &yaw, &pitch);
		CHECK(yaw == 0.0f && pitch == 0.0f);
		ae_gyro_rules_turn(rate, -0.5, &yaw, &pitch);
		CHECK(yaw == 0.0f && pitch == 0.0f);
	}
	if (failures)
		return 1;
	printf("ae_gyro_rules_test: all passed\n");
	return 0;
}
