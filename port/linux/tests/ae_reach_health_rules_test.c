#include <stdio.h>
#include "ae_reach_health_rules.h"

static int failures;
static int checks;
#define CHECK(c) do { checks++; if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

#define TICK (1.0f / 30.0f)

/* ticks of refill from `health` until it stops (at most a minute) */
static int ticks_to_stop(float health, float *stopped)
{
	int ticks = 0;
	float next;

	while (ticks < 30 * 60 && (next = ae_reach_health_step(health, TICK)) != health)
	{
		health = next;
		ticks++;
	}
	*stopped = health;
	return ticks;
}

/* the stops: the top of the third it is in, 0.333 and 0.666 */
static void test_cap(void)
{
	CHECK(ae_reach_health_cap(0.01f) == AE_REACH_HEALTH_STOP_LOW);
	CHECK(ae_reach_health_cap(0.333f) == AE_REACH_HEALTH_STOP_LOW);
	CHECK(ae_reach_health_cap(0.3331f) == AE_REACH_HEALTH_STOP_HIGH);
	/* (exactly a third is above 0.333: it goes on to 0.666) */
	CHECK(ae_reach_health_cap(1.0f / 3.0f) == AE_REACH_HEALTH_STOP_HIGH);
	CHECK(ae_reach_health_cap(0.666f) == AE_REACH_HEALTH_STOP_HIGH);
	CHECK(ae_reach_health_cap(0.6661f) == 1.0f);
	CHECK(ae_reach_health_cap(0.99f) == 1.0f);
}

/* no move: dead, full, on a stop, no time */
static void test_still(void)
{
	CHECK(ae_reach_health_step(0.0f, TICK) == 0.0f);
	CHECK(ae_reach_health_step(-0.1f, TICK) == -0.1f);
	CHECK(ae_reach_health_step(1.0f, TICK) == 1.0f);
	CHECK(ae_reach_health_step(AE_REACH_HEALTH_STOP_LOW, TICK) == AE_REACH_HEALTH_STOP_LOW);
	CHECK(ae_reach_health_step(AE_REACH_HEALTH_STOP_HIGH, TICK) == AE_REACH_HEALTH_STOP_HIGH);
	CHECK(ae_reach_health_step(0.5f, 0.0f) == 0.5f);
}

/* the rate: 1/9 a second times 3 near empty, times 1 near full */
static void test_rate(void)
{
	/* (a second near empty would pass the stop: a short step) */
	float low = ae_reach_health_step(0.0001f, 0.01f) - 0.0001f;
	float high = ae_reach_health_step(0.9999f, 0.00001f) - 0.9999f;

	CHECK(low > 0.01f * 3.0f / 9.0f * 0.999f && low < 0.01f * 3.0f / 9.0f * 1.001f);
	CHECK(high > 0.00001f / 9.0f * 0.9f && high < 0.00001f / 9.0f * 1.1f);
	/* (one tick at half health: 1/9 x 2 x 1/30) */
	{
		float half = ae_reach_health_step(0.5f, TICK) - 0.5f;
		float want = (3.0f - 6.0f * 0.25f + 4.0f * 0.125f) / 9.0f * TICK;

		CHECK(half > want * 0.999f && half < want * 1.001f);
	}
}

/* each third from its bottom: about 1.1 s, 1.5 s and 2.6 s at 30 ticks a second, stopping on its top */
static void test_thirds(void)
{
	float stopped;
	int ticks;

	ticks = ticks_to_stop(0.0001f, &stopped);
	CHECK(ticks >= 30 && ticks <= 34);
	CHECK(stopped == AE_REACH_HEALTH_STOP_LOW);
	ticks = ticks_to_stop(0.3331f, &stopped);
	CHECK(ticks >= 44 && ticks <= 48);
	CHECK(stopped == AE_REACH_HEALTH_STOP_HIGH);
	ticks = ticks_to_stop(0.6661f, &stopped);
	CHECK(ticks >= 76 && ticks <= 80);
	CHECK(stopped == 1.0f);
	/* (from the middle of a third, only to its top) */
	ticks = ticks_to_stop(0.5f, &stopped);
	CHECK(ticks > 0 && ticks < 46);
	CHECK(stopped == AE_REACH_HEALTH_STOP_HIGH);
}

int main(void)
{
	test_cap();
	test_still();
	test_rate();
	test_thirds();
	printf("%d checks, %d failures\n", checks, failures);
	return failures ? 1 : 0;
}
