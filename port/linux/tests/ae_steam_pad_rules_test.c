#include <math.h>
#include <stdio.h>
#include <string.h>
#include "ae_steam_pad_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

#define STEP 0.016

static struct ae_trackpad_rules pad;
static float yaw, pitch;

static void landed(float x, float y)
{
	memset(&pad, 0, sizeof(pad));
	ae_trackpad_rules_step(&pad, 1, x, y, STEP, 1.0f, &yaw, &pitch);
}

/* a finger sliding right from x by delta each read, for count reads */
static float slide(float x, float delta, int count)
{
	int step;

	for (step = 1; step <= count; step++)
		ae_trackpad_rules_step(&pad, 1, x + delta * (float)step, 0.5f, STEP, 1.0f, &yaw, &pitch);
	return x + delta * (float)count;
}

/* a finger held still at x for count reads: the smoothing's travel not turned yet comes out; returns the yaw it gave */
static float rest(float x, int count, float sensitivity)
{
	float total = 0.0f;
	int step;

	for (step = 0; step < count; step++)
	{
		ae_trackpad_rules_step(&pad, 1, x, 0.5f, STEP, sensitivity, &yaw, &pitch);
		total += yaw;
	}
	return total;
}

int main(void)
{
	float total_yaw, total_pitch;
	int step;

	/* the 2026 pad: Valve's vendor id and its product ids; the 2015 pad (0x1102 wired, 0x1142 wireless) has the same
	name in SDL and is not it */
	CHECK(ae_steam_pad_rules_kind(0x28de, 0x1302) == AE_STEAM_PAD_CONTROLLER);
	CHECK(ae_steam_pad_rules_kind(0x28de, 0x1304) == AE_STEAM_PAD_CONTROLLER);
	CHECK(ae_steam_pad_rules_kind(0x28de, 0x1305) == AE_STEAM_PAD_CONTROLLER);
	CHECK(ae_steam_pad_rules_kind(0x28de, 0x1102) == AE_STEAM_PAD_OTHER_VALVE);
	CHECK(ae_steam_pad_rules_kind(0x28de, 0x1142) == AE_STEAM_PAD_OTHER_VALVE);
	CHECK(ae_steam_pad_rules_kind(0x28de, 0x1205) == AE_STEAM_PAD_OTHER_VALVE);
	CHECK(ae_steam_pad_rules_kind(0x28de, 0) == AE_STEAM_PAD_OTHER_VALVE);
	CHECK(ae_steam_pad_rules_kind(0x054c, 0x1304) == AE_STEAM_PAD_NONE);
	CHECK(ae_steam_pad_rules_kind(0, 0) == AE_STEAM_PAD_NONE);

	/* a finger that lands and rests turns nothing, even drifting inside the dead zone */
	landed(0.5f, 0.5f);
	total_yaw = total_pitch = 0.0f;
	for (step = 0; step < 100; step++)
	{
		ae_trackpad_rules_step(&pad, 1, 0.5f + 0.02f * sinf((float)step), 0.5f + 0.02f * cosf((float)step), STEP, 1.0f,
			&yaw, &pitch);
		total_yaw += fabsf(yaw);
		total_pitch += fabsf(pitch);
	}
	CHECK(total_yaw == 0.0f && total_pitch == 0.0f);

	/* a finger moving right turns the view right (yaw negative), up looks up (pitch positive); the whole way from the
	dead zone's edge counts: 0.4 across, 0.04 of it dead, 0.36 turned */
	landed(0.2f, 0.5f);
	total_yaw = 0.0f;
	for (step = 1; step <= 20; step++)
	{
		ae_trackpad_rules_step(&pad, 1, 0.2f + 0.02f * (float)step, 0.5f, STEP, 1.0f, &yaw, &pitch);
		total_yaw += yaw;
		CHECK(pitch == 0.0f);
	}
	/* (the smoothing turns the last of the travel over the next reads) */
	total_yaw += rest(0.6f, 40, 1.0f);
	CHECK(fabsf(total_yaw + 0.36f * AE_TRACKPAD_RULES_TURN) < 1e-4f);
	landed(0.5f, 0.5f);
	ae_trackpad_rules_step(&pad, 1, 0.5f, 0.4f, STEP, 1.0f, &yaw, &pitch);
	ae_trackpad_rules_step(&pad, 1, 0.5f, 0.3f, STEP, 1.0f, &yaw, &pitch);
	CHECK(pitch > 0.0f && yaw == 0.0f);

	/* a turn is the finger's distance times the pad's turn and the sensitivity */
	{
		float one, four;

		landed(0.1f, 0.5f);
		ae_trackpad_rules_step(&pad, 1, 0.3f, 0.5f, STEP, 1.0f, &yaw, &pitch);
		one = yaw;
		ae_trackpad_rules_step(&pad, 1, 0.4f, 0.5f, STEP, 1.0f, &yaw, &pitch);
		one += yaw;
		one += rest(0.4f, 40, 1.0f);
		CHECK(fabsf(one + 0.26f * AE_TRACKPAD_RULES_TURN) < 1e-4f);
		memset(&pad, 0, sizeof(pad));
		ae_trackpad_rules_step(&pad, 1, 0.1f, 0.5f, STEP, 4.0f, &yaw, &pitch);
		ae_trackpad_rules_step(&pad, 1, 0.3f, 0.5f, STEP, 4.0f, &yaw, &pitch);
		four = yaw;
		ae_trackpad_rules_step(&pad, 1, 0.4f, 0.5f, STEP, 4.0f, &yaw, &pitch);
		four += yaw;
		four += rest(0.4f, 40, 4.0f);
		CHECK(fabsf(four - 4.0f * one) < 1e-4f);
	}

	/* a slow finger with sensor noise (reads that step back and forth while it drifts right) turns one way only: the
	smoothing takes out the noise, where the raw reads would turn the view back and forth */
	{
		float at;
		int backwards = 0;

		landed(0.1f, 0.5f);
		ae_trackpad_rules_step(&pad, 1, 0.2f, 0.5f, STEP, 1.0f, &yaw, &pitch);
		rest(0.2f, 40, 1.0f);
		at = 0.2f;
		for (step = 0; step < 20; step++)
		{
			at += (step & 1) ? -0.002f : 0.004f;
			ae_trackpad_rules_step(&pad, 1, at, 0.5f, STEP, 1.0f, &yaw, &pitch);
			if (yaw > 0.0f)
				backwards++;
		}
		CHECK(backwards == 0);
	}

	/* a flick (fast, lifted at once) glides on, getting smaller each step, and stops */
	landed(0.1f, 0.5f);
	slide(0.1f, 0.08f, 9);
	{
		float previous = 1e9f;
		int steps = 0;

		ae_trackpad_rules_step(&pad, 0, 0.0f, 0.0f, STEP, 1.0f, &yaw, &pitch);
		CHECK(yaw < 0.0f);
		while (yaw != 0.0f && steps < 2000)
		{
			CHECK(fabsf(yaw) <= previous + 1e-9f);
			previous = fabsf(yaw);
			steps++;
			ae_trackpad_rules_step(&pad, 0, 0.0f, 0.0f, STEP, 1.0f, &yaw, &pitch);
		}
		CHECK(steps > 5 && steps < 60);
		CHECK(pad.glide_x == 0.0f);
	}

	/* the glide covers the same distance whatever the read's rate (one 10 ms read, or ten of 1 ms) */
	{
		float coarse = 0.0f, fine = 0.0f, part;
		struct ae_trackpad_rules start;

		landed(0.1f, 0.5f);
		slide(0.1f, 0.08f, 9);
		ae_trackpad_rules_step(&pad, 0, 0.0f, 0.0f, STEP, 1.0f, &yaw, &pitch);
		start = pad;
		for (step = 0; step < 10; step++)
		{
			ae_trackpad_rules_step(&pad, 0, 0.0f, 0.0f, 0.01, 1.0f, &part, &pitch);
			coarse += part;
		}
		pad = start;
		for (step = 0; step < 100; step++)
		{
			ae_trackpad_rules_step(&pad, 0, 0.0f, 0.0f, 0.001, 1.0f, &part, &pitch);
			fine += part;
		}
		CHECK(coarse < 0.0f && fabsf(coarse - fine) < 0.02f * fabsf(coarse));
	}

	/* a slow slide lifted gives no glide; nor does a fast one that rested before it lifted */
	landed(0.1f, 0.5f);
	slide(0.1f, 0.01f, 20);
	ae_trackpad_rules_step(&pad, 0, 0.0f, 0.0f, STEP, 1.0f, &yaw, &pitch);
	CHECK(pad.glide_x == 0.0f && pad.glide_y == 0.0f && pitch == 0.0f);
	landed(0.1f, 0.5f);
	{
		float at = slide(0.1f, 0.08f, 5);

		for (step = 0; step < 6; step++)
			ae_trackpad_rules_step(&pad, 1, at, 0.5f, STEP, 1.0f, &yaw, &pitch);
	}
	ae_trackpad_rules_step(&pad, 0, 0.0f, 0.0f, STEP, 1.0f, &yaw, &pitch);
	CHECK(pad.glide_x == 0.0f && pad.glide_y == 0.0f);

	/* a tap (landing and lifting inside the dead zone) gives no glide */
	landed(0.5f, 0.5f);
	ae_trackpad_rules_step(&pad, 1, 0.51f, 0.5f, STEP, 1.0f, &yaw, &pitch);
	ae_trackpad_rules_step(&pad, 0, 0.0f, 0.0f, STEP, 1.0f, &yaw, &pitch);
	CHECK(yaw == 0.0f && pitch == 0.0f);

	/* a finger that lands in a glide stops it */
	landed(0.1f, 0.5f);
	slide(0.1f, 0.08f, 9);
	ae_trackpad_rules_step(&pad, 0, 0.0f, 0.0f, STEP, 1.0f, &yaw, &pitch);
	CHECK(yaw < 0.0f);
	ae_trackpad_rules_step(&pad, 1, 0.8f, 0.5f, STEP, 1.0f, &yaw, &pitch);
	CHECK(yaw == 0.0f && pad.glide_x == 0.0f);

	/* a jump of more than a quarter of the pad in one read is a new landing: no turn, and the dead zone again */
	landed(0.1f, 0.5f);
	slide(0.1f, 0.02f, 5);
	ae_trackpad_rules_step(&pad, 1, 0.9f, 0.5f, STEP, 1.0f, &yaw, &pitch);
	CHECK(yaw == 0.0f && pitch == 0.0f && !pad.moving);
	ae_trackpad_rules_step(&pad, 1, 0.91f, 0.5f, STEP, 1.0f, &yaw, &pitch);
	CHECK(yaw == 0.0f);

	/* a read after a long pause starts afresh: no jump, and a glide in progress is dropped */
	landed(0.1f, 0.5f);
	slide(0.1f, 0.08f, 9);
	ae_trackpad_rules_step(&pad, 0, 0.0f, 0.0f, 2.0, 1.0f, &yaw, &pitch);
	CHECK(yaw == 0.0f && pitch == 0.0f && pad.glide_x == 0.0f);
	ae_trackpad_rules_step(&pad, 1, 0.9f, 0.9f, 2.0, 1.0f, &yaw, &pitch);
	CHECK(yaw == 0.0f && pitch == 0.0f);

	/* no time passed: nothing */
	ae_trackpad_rules_step(&pad, 1, 0.1f, 0.1f, 0.0, 1.0f, &yaw, &pitch);
	CHECK(yaw == 0.0f && pitch == 0.0f);

	if (failures)
		return 1;
	printf("ae_steam_pad_rules_test: all passed\n");
	return 0;
}
