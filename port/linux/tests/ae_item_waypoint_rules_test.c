/* ae_item_waypoint_rules_test.c: when a power item's waypoint shows (ae_item_waypoint_rules.h). */
#include <stdio.h>
#include "ae_item_waypoint_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

#define PERIOD 3600L	/* 2 minutes */
#define BEFORE 300L		/* 10 s */
#define AFTER 600L		/* 20 s, TRAINING's window */
#define NOT_TAKEN (-1L)

static int reason(long now, long taken_spawn_tick, int training)
{
	return ae_iw_reason(now, 0, PERIOD, BEFORE, AFTER, taken_spawn_tick, training);
}

int main(void)
{
	/* the item lying at the game's start shows from tick 0 */
	CHECK(reason(0, NOT_TAKEN, 0) == AE_IW_ON_THE_MAP);
	CHECK(reason(1500, NOT_TAKEN, 0) == AE_IW_ON_THE_MAP);
	/* taken from the start's spawn (tick 0): off, and the others' stay */
	CHECK(reason(1500, 0, 0) == AE_IW_NONE);
	CHECK(reason(1500, NOT_TAKEN, 0) == AE_IW_ON_THE_MAP);
	/* a spawn's item shows while it lies there, long past the old 20 s window */
	CHECK(reason(PERIOD + 10, NOT_TAKEN, 0) == AE_IW_ON_THE_MAP);
	CHECK(reason(PERIOD + 2000, NOT_TAKEN, 0) == AE_IW_ON_THE_MAP);
	/* taken from the second spawn: off until the countdown */
	CHECK(reason(PERIOD + 2000, PERIOD, 0) == AE_IW_NONE);
	/* a pickup of an earlier spawn does not hide this one */
	CHECK(reason(PERIOD + 2000, 0, 0) == AE_IW_ON_THE_MAP);
	/* the countdown: 10 s before the respawn, taken or not */
	CHECK(reason(PERIOD - BEFORE - 1, 0, 0) == AE_IW_NONE);
	CHECK(reason(PERIOD - BEFORE, 0, 0) == AE_IW_RESPAWN_SOON);
	CHECK(reason(PERIOD - 1, 0, 0) == AE_IW_RESPAWN_SOON);
	CHECK(reason(PERIOD - BEFORE, NOT_TAKEN, 0) == AE_IW_RESPAWN_SOON);
	/* at the respawn tick the countdown is over: the new item is on the map */
	CHECK(reason(PERIOD, NOT_TAKEN, 0) == AE_IW_ON_THE_MAP);
	CHECK(reason(PERIOD, 0, 0) == AE_IW_ON_THE_MAP);
	/* no period: nothing */
	CHECK(ae_iw_reason(100, 0, 0, BEFORE, AFTER, NOT_TAKEN, 0) == AE_IW_NONE);
	/* TRAINING keeps its fixed window: none at the start, 20 s after each spawn, taken or not */
	CHECK(reason(0, NOT_TAKEN, 1) == AE_IW_NONE);
	CHECK(reason(1500, NOT_TAKEN, 1) == AE_IW_NONE);
	CHECK(reason(PERIOD + 10, PERIOD, 1) == AE_IW_ON_THE_MAP);
	CHECK(reason(PERIOD + AFTER, NOT_TAKEN, 1) == AE_IW_NONE);
	CHECK(reason(PERIOD - BEFORE, NOT_TAKEN, 1) == AE_IW_RESPAWN_SOON);
	/* a dev respawn's phase: the next spawn comes `in` ticks on, then every period */
	{
		long now = 5000, in = 450, phase = ae_iw_phase(now, PERIOD, in);

		CHECK((now + in + phase) % PERIOD == 0);
		CHECK(phase >= 0 && phase < PERIOD);
		/* 15 s on, the countdown (10 s) shows after 5 s; the item taken meanwhile stays off before it */
		CHECK(ae_iw_reason(now, phase, PERIOD, BEFORE, AFTER, now - (now + phase) % PERIOD, 0) == AE_IW_NONE);
		CHECK(ae_iw_reason(now + 149, phase, PERIOD, BEFORE, AFTER, now - (now + phase) % PERIOD, 0) == AE_IW_NONE);
		CHECK(ae_iw_reason(now + 150, phase, PERIOD, BEFORE, AFTER, now - (now + phase) % PERIOD, 0) == AE_IW_RESPAWN_SOON);
		CHECK(ae_iw_reason(now + in, phase, PERIOD, BEFORE, AFTER, NOT_TAKEN, 0) == AE_IW_ON_THE_MAP);
		CHECK(ae_iw_phase(now, PERIOD, PERIOD - now % PERIOD) == 0);
	}
	/* the log's words */
	CHECK(ae_iw_reason_word(AE_IW_ON_THE_MAP)[0] == 'o');
	CHECK(ae_iw_reason_word(AE_IW_RESPAWN_SOON)[0] == 'r');

	if (failures)
		return 1;
	printf("ok\n");
	return 0;
}
