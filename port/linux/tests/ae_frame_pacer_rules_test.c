#include <stdio.h>

#include "ae_frame_pacer_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

#define MS(x) ((unsigned long long)((x) * 1.0e6))

/* n swaps one period apart from t (ns), each taking swap_ms; the time of the last */
static unsigned long long swaps(struct ae_pacer_state *state, unsigned long long t, float period_ms, float swap_ms,
	int n)
{
	while (n-- > 0)
	{
		t += MS(period_ms);
		ae_pacer_swap(state, t, MS(swap_ms));
	}
	return t;
}

static void costs(struct ae_pacer_state *state, float cpu_ms, float gpu_ms, int n)
{
	while (n-- > 0)
		ae_pacer_frame(state, cpu_ms, gpu_ms);
}

/* reset, with the start's check taken as passed (the cases below are about the waits after it) */
static void ready(struct ae_pacer_state *state)
{
	ae_pacer_reset(state);
	state->start_checked = 1;
}

int main(void)
{
	struct ae_pacer_state state;
	unsigned long long t, wait;
	int i;

	/* nothing is known: no wait */
	ready(&state);
	CHECK(ae_pacer_period_ms(&state) == 0.0f);
	CHECK(ae_pacer_cost_ms(&state) < 0.0f);
	CHECK(ae_pacer_wait(&state, MS(5.0)) == 0);

	/* the period is learnt from AE_PACER_LEARN gaps (the first swap has no gap) */
	t = swaps(&state, MS(1000.0), 16.667f, 0.2f, AE_PACER_LEARN);
	CHECK(ae_pacer_period_ms(&state) == 0.0f);
	t = swaps(&state, t, 16.667f, 0.2f, 1);
	CHECK(ae_pacer_period_ms(&state) > 16.6f && ae_pacer_period_ms(&state) < 16.7f);

	/* no cost yet: no wait; with one, the wait ends at the next blank less the cost and the margin */
	CHECK(ae_pacer_wait(&state, t + MS(0.1)) == 0);
	costs(&state, 4.0f, 5.0f, 3);
	CHECK(ae_pacer_cost_ms(&state) == 5.0f);
	wait = ae_pacer_wait(&state, t + MS(0.1));
	/* 16.667 - 5 - 1 = 10.667 after the swap, 0.1 gone */
	CHECK(wait > MS(10.5) && wait < MS(10.6));
	/* a frame that is already late for its start waits for nothing */
	CHECK(ae_pacer_wait(&state, t + MS(11.0)) == 0);
	/* the cost is the largest of the last frames, and a frame without the GPU's time costs the CPU's and a little more */
	costs(&state, 6.0f, 2.0f, 1);
	CHECK(ae_pacer_cost_ms(&state) == 6.0f);
	ae_pacer_forget(&state);
	costs(&state, 3.0f, -1.0f, 1);
	CHECK(ae_pacer_cost_ms(&state) > 4.4f && ae_pacer_cost_ms(&state) < 4.6f);
	/* the cost ages out after AE_PACER_COSTS frames */
	costs(&state, 9.0f, 9.0f, 1);
	costs(&state, 2.0f, 2.0f, AE_PACER_COSTS);
	CHECK(ae_pacer_cost_ms(&state) == 2.0f);

	/* a frame that needs more than 85 percent of the period is not waited for, and the rules say so once; waiting
	starts again under 65 percent */
	ready(&state);
	t = swaps(&state, MS(1000.0), 16.667f, 0.2f, AE_PACER_LEARN + 1);
	ae_pacer_take_event(&state);
	costs(&state, 14.0f, 14.0f, 1);
	CHECK(ae_pacer_wait(&state, t + MS(0.1)) == 0);
	CHECK(ae_pacer_take_event(&state) == AE_PACER_EVENT_OVER_BUDGET);
	CHECK(ae_pacer_wait(&state, t + MS(0.1)) == 0);
	CHECK(ae_pacer_take_event(&state) == AE_PACER_EVENT_NONE);
	/* between the two (13.6 of 16.667: 82 percent) it stays off once off */
	state.cost_count = state.cost_next = 0;
	costs(&state, 12.6f, 12.6f, 1);
	CHECK(ae_pacer_wait(&state, t + MS(0.1)) == 0);
	CHECK(ae_pacer_take_event(&state) == AE_PACER_EVENT_NONE);
	/* and at 72 percent (12 of 16.667) too */
	state.cost_count = state.cost_next = 0;
	costs(&state, 11.0f, 11.0f, 1);
	CHECK(ae_pacer_wait(&state, t + MS(0.1)) == 0);
	CHECK(ae_pacer_take_event(&state) == AE_PACER_EVENT_NONE);
	/* under 65 percent (10 of 16.667) it waits again, and the rules say so */
	state.cost_count = state.cost_next = 0;
	costs(&state, 9.0f, 9.0f, 1);
	CHECK(ae_pacer_wait(&state, t + MS(0.1)) > 0);
	CHECK(ae_pacer_take_event(&state) == AE_PACER_EVENT_FAST_AGAIN);
	/* and between the two it stays on once on */
	state.cost_count = state.cost_next = 0;
	costs(&state, 12.6f, 12.6f, 1);
	CHECK(ae_pacer_wait(&state, t + MS(0.1)) > 0);

	/* a late frame: counted, the margin grows, the next frame is not waited for, the period is kept */
	ready(&state);
	t = swaps(&state, MS(1000.0), 16.667f, 0.2f, AE_PACER_LEARN + 1);
	costs(&state, 4.0f, 4.0f, 2);
	t += MS(16.667 * 2);
	ae_pacer_swap(&state, t, MS(0.3));
	CHECK(state.late == 1);
	CHECK(ae_pacer_take_event(&state) == AE_PACER_EVENT_LATE);
	CHECK(state.margin_ms > 1.4f && state.margin_ms < 1.6f);
	CHECK(ae_pacer_period_ms(&state) > 16.6f && ae_pacer_period_ms(&state) < 16.7f);
	CHECK(ae_pacer_wait(&state, t + MS(0.1)) == 0);
	CHECK(ae_pacer_wait(&state, t + MS(0.1)) > 0);
	/* the margin shrinks slowly, never under its start, and grows to a limit */
	t = swaps(&state, t, 16.667f, 0.2f, 100);
	CHECK(state.margin_ms > 1.0f && state.margin_ms < 1.5f);
	t = swaps(&state, t, 16.667f, 0.2f, 400);
	CHECK(state.margin_ms == AE_PACER_MARGIN_MS);
	for (i = 0; i < 8; i++)
	{
		t += MS(16.667 * 2);
		ae_pacer_swap(&state, t, MS(0.3));
	}
	CHECK(state.margin_ms == AE_PACER_MARGIN_MAX_MS);
	CHECK(state.late == 9);
	CHECK(!state.gave_up);

	/* a swap that took 1.5 periods itself (a full driver queue): no late frame, the period kept, no wait next */
	ready(&state);
	t = swaps(&state, MS(1000.0), 16.667f, 0.2f, AE_PACER_LEARN + 1);
	costs(&state, 4.0f, 4.0f, 2);
	t += MS(30.0);
	ae_pacer_swap(&state, t, MS(26.0));
	CHECK(ae_pacer_take_event(&state) == AE_PACER_EVENT_STALL);
	CHECK(state.late == 0);
	CHECK(ae_pacer_period_ms(&state) > 16.6f && ae_pacer_period_ms(&state) < 16.7f);
	CHECK(ae_pacer_wait(&state, t + MS(0.1)) == 0);

	/* no time to spare: AE_PACER_GIVE_UP_LATE late frames within a minute give up until a reset, and say so */
	ready(&state);
	t = swaps(&state, MS(1000.0), 16.667f, 0.2f, AE_PACER_LEARN + 1);
	costs(&state, 4.0f, 4.0f, 2);
	for (i = 0; i < AE_PACER_GIVE_UP_LATE - 1; i++)
	{
		t += MS(16.667 * 2);
		ae_pacer_swap(&state, t, MS(0.3));
		t = swaps(&state, t, 16.667f, 0.2f, 60);
	}
	CHECK(!state.gave_up);
	t += MS(16.667 * 2);
	ae_pacer_swap(&state, t, MS(0.3));
	CHECK(state.gave_up);
	CHECK(ae_pacer_take_event(&state) == AE_PACER_EVENT_NO_TIME_LATE);
	CHECK(ae_pacer_wait(&state, t + MS(0.1)) == 0);
	t = swaps(&state, t, 16.667f, 0.2f, 100);
	CHECK(ae_pacer_wait(&state, t + MS(0.1)) == 0);
	CHECK(ae_pacer_take_event(&state) == AE_PACER_EVENT_NONE);
	/* the same late frames spread over more than a minute: still waiting */
	ready(&state);
	t = swaps(&state, MS(1000.0), 16.667f, 0.2f, AE_PACER_LEARN + 1);
	costs(&state, 4.0f, 4.0f, 2);
	for (i = 0; i < AE_PACER_GIVE_UP_LATE * 2; i++)
	{
		t += MS(16.667 * 2);
		ae_pacer_swap(&state, t, MS(0.3));
		/* (7 s of frames between misses) */
		t = swaps(&state, t, 16.667f, 0.2f, 420);
	}
	CHECK(!state.gave_up);
	CHECK(state.late == AE_PACER_GIVE_UP_LATE * 2);

	/* no time to spare: AE_PACER_GIVE_UP_STOPS stops within a minute give up, and say so; spread out they do not */
	ready(&state);
	t = swaps(&state, MS(1000.0), 16.667f, 0.2f, AE_PACER_LEARN + 1);
	ae_pacer_take_event(&state);
	for (i = 0; i < AE_PACER_GIVE_UP_STOPS; i++)
	{
		state.cost_count = state.cost_next = 0;
		costs(&state, 15.0f, 15.0f, 1);
		CHECK(ae_pacer_wait(&state, t + MS(0.1)) == 0);
		CHECK(ae_pacer_take_event(&state) ==
			(i < AE_PACER_GIVE_UP_STOPS - 1 ? AE_PACER_EVENT_OVER_BUDGET : AE_PACER_EVENT_NO_TIME_STOPS));
		state.cost_count = state.cost_next = 0;
		costs(&state, 4.0f, 4.0f, 1);
		t = swaps(&state, t, 16.667f, 0.2f, 600);
		ae_pacer_wait(&state, t + MS(0.1));
		ae_pacer_take_event(&state);
	}
	CHECK(state.gave_up);
	CHECK(ae_pacer_wait(&state, t + MS(0.1)) == 0);
	ready(&state);
	t = swaps(&state, MS(1000.0), 16.667f, 0.2f, AE_PACER_LEARN + 1);
	for (i = 0; i < AE_PACER_GIVE_UP_STOPS * 2; i++)
	{
		state.cost_count = state.cost_next = 0;
		costs(&state, 15.0f, 15.0f, 1);
		CHECK(ae_pacer_wait(&state, t + MS(0.1)) == 0);
		state.cost_count = state.cost_next = 0;
		costs(&state, 4.0f, 4.0f, 1);
		/* (35 s of frames between stops) */
		t = swaps(&state, t, 16.667f, 0.2f, 2100);
		CHECK(ae_pacer_wait(&state, t + MS(0.1)) > 0);
	}
	CHECK(!state.gave_up);

	/* the display mode known: a period on its refresh is kept */
	ready(&state);
	ae_pacer_display(&state, 16.667f);
	t = swaps(&state, MS(1000.0), 16.6f, 0.2f, AE_PACER_LEARN + 1);
	CHECK(ae_pacer_period_ms(&state) > 16.5f && ae_pacer_period_ms(&state) < 16.7f);
	CHECK(ae_pacer_take_event(&state) == AE_PACER_EVENT_NONE);
	/* one off it (frames missing blanks in split screen: 23 ms at 60 Hz) is learnt again, and the third time the rules
	give up and say so */
	ready(&state);
	ae_pacer_display(&state, 16.667f);
	t = swaps(&state, MS(1000.0), 23.07f, 0.2f, AE_PACER_LEARN + 1);
	CHECK(ae_pacer_period_ms(&state) == 0.0f);
	CHECK(!state.gave_up);
	for (i = 1; i < AE_PACER_WRONG_PERIODS; i++)
		t = swaps(&state, t, 23.07f, 0.2f, AE_PACER_LEARN);
	CHECK(state.gave_up);
	CHECK(ae_pacer_take_event(&state) == AE_PACER_EVENT_WRONG_PERIOD);
	CHECK(state.wrong_period_ms > 23.0f && state.wrong_period_ms < 23.1f);
	costs(&state, 4.0f, 4.0f, 2);
	CHECK(ae_pacer_wait(&state, t + MS(0.1)) == 0);
	/* a period a little fast of it (15.1 ms early in a game at 60 Hz) is not the display's either */
	ready(&state);
	ae_pacer_display(&state, 16.667f);
	t = swaps(&state, MS(1000.0), 15.13f, 0.2f, AE_PACER_LEARN + 1);
	CHECK(ae_pacer_period_ms(&state) == 0.0f);
	CHECK(state.wrong_periods == 1);
	/* a wrong one at the game's start then the right one: kept, waiting */
	t = swaps(&state, t, 16.667f, 0.2f, AE_PACER_LEARN);
	CHECK(ae_pacer_period_ms(&state) > 16.6f && ae_pacer_period_ms(&state) < 16.7f);
	CHECK(!state.gave_up);
	costs(&state, 4.0f, 4.0f, 2);
	CHECK(ae_pacer_wait(&state, t + MS(0.1)) > 0);
	/* a reset forgets the display too */
	ready(&state);
	CHECK(state.display_ms == 0.0f && state.wrong_periods == 0);

	/* a load or a pause is no frame */
	ready(&state);
	t = swaps(&state, MS(1000.0), 16.667f, 0.2f, AE_PACER_LEARN + 1);
	t += MS(3000.0);
	ae_pacer_swap(&state, t, MS(0.2));
	CHECK(state.late == 0);
	CHECK(ae_pacer_take_event(&state) == AE_PACER_EVENT_NONE);
	CHECK(ae_pacer_period_ms(&state) == 0.0f);

	/* a load before the period is known is no gap */
	ready(&state);
	ae_pacer_swap(&state, MS(1000.0), MS(0.2));
	ae_pacer_swap(&state, MS(4000.0), MS(0.2));
	CHECK(state.gap_count == 0);
	t = swaps(&state, MS(4000.0), 16.667f, 0.2f, AE_PACER_LEARN);
	CHECK(ae_pacer_period_ms(&state) > 16.6f && ae_pacer_period_ms(&state) < 16.7f);

	/* a variable refresh rate or 144 Hz: the measured period, not a rate asked of the display */
	ready(&state);
	t = swaps(&state, MS(1000.0), 6.944f, 0.1f, AE_PACER_LEARN + 1);
	CHECK(ae_pacer_period_ms(&state) > 6.9f && ae_pacer_period_ms(&state) < 7.0f);
	costs(&state, 2.0f, 2.0f, 1);
	wait = ae_pacer_wait(&state, t + MS(0.1));
	CHECK(wait > MS(3.7) && wait < MS(3.9));
	/* one odd gap among many does not move the period */
	t = swaps(&state, t, 9.0f, 0.1f, 1);
	CHECK(ae_pacer_period_ms(&state) > 6.9f && ae_pacer_period_ms(&state) < 7.0f);

	/* swaps that do not wait for a display (nothing to sleep for): give up, once, and stay given up */
	ready(&state);
	t = swaps(&state, MS(1000.0), 0.8f, 0.0f, AE_PACER_LEARN + 1);
	CHECK(state.gave_up);
	CHECK(ae_pacer_take_event(&state) == AE_PACER_EVENT_NOT_A_DISPLAY);
	costs(&state, 0.5f, 0.5f, 2);
	CHECK(ae_pacer_wait(&state, t) == 0);
	t = swaps(&state, t, 16.667f, 0.2f, 40);
	CHECK(ae_pacer_wait(&state, t) == 0);
	CHECK(ae_pacer_take_event(&state) == AE_PACER_EVENT_NONE);
	ready(&state);
	CHECK(!state.gave_up);
	/* a game too slow to pace (80 ms frames): no wait and said once, not given up; paced again once faster */
	t = swaps(&state, MS(1000.0), 80.0f, 0.2f, AE_PACER_LEARN + 1);
	CHECK(!state.gave_up);
	CHECK(ae_pacer_take_event(&state) == AE_PACER_EVENT_TOO_SLOW);
	costs(&state, 4.0f, 4.0f, 2);
	CHECK(ae_pacer_wait(&state, t + MS(0.1)) == 0);
	t = swaps(&state, t, 80.0f, 0.2f, AE_PACER_LEARN * 3);
	CHECK(ae_pacer_take_event(&state) == AE_PACER_EVENT_NONE);
	t = swaps(&state, t, 16.667f, 0.2f, AE_PACER_LEARN + 1);
	CHECK(ae_pacer_period_ms(&state) > 16.6f && ae_pacer_period_ms(&state) < 16.7f);
	CHECK(!state.too_slow);

	/* the start: nothing is waited for until a full ring of costs is known */
	ae_pacer_reset(&state);
	t = swaps(&state, MS(1000.0), 16.667f, 0.2f, AE_PACER_LEARN + 1);
	costs(&state, 4.0f, 4.0f, AE_PACER_COSTS - 1);
	CHECK(ae_pacer_wait(&state, t + MS(0.1)) == 0);
	CHECK(!state.start_checked);
	/* then a PC with time to spare waits, though a spawn's slow frames are among those costs */
	costs(&state, 4.0f, 4.0f, 1);
	state.costs[0] = 40.0f;
	state.costs[1] = 25.0f;
	ae_pacer_take_event(&state);
	ae_pacer_wait(&state, t + MS(0.1));
	CHECK(state.start_checked && !state.gave_up);
	state.cost_count = state.cost_next = 0;
	costs(&state, 4.0f, 4.0f, 1);
	CHECK(ae_pacer_wait(&state, t + MS(0.1)) > 0);
	/* a PC whose frames take most of the refresh from the start (a PC's 15 to 25 ms at 60 Hz): off for the game
	at once, said once */
	ae_pacer_reset(&state);
	t = swaps(&state, MS(1000.0), 16.667f, 0.2f, AE_PACER_LEARN + 1);
	for (i = 0; i < AE_PACER_COSTS; i++)
		costs(&state, i % 3 ? 15.0f : 22.0f, -1.0f, 1);
	CHECK(ae_pacer_wait(&state, t + MS(0.1)) == 0);
	CHECK(state.gave_up);
	CHECK(ae_pacer_take_event(&state) == AE_PACER_EVENT_NO_TIME_START);
	CHECK(state.start_cost_ms > 16.0f && state.start_cost_ms < 17.0f);
	t = swaps(&state, t, 16.667f, 0.2f, 100);
	state.cost_count = state.cost_next = 0;
	costs(&state, 4.0f, 4.0f, AE_PACER_COSTS);
	CHECK(ae_pacer_wait(&state, t + MS(0.1)) == 0);
	CHECK(ae_pacer_take_event(&state) == AE_PACER_EVENT_NONE);
	/* the check is once a game: a reset makes it again */
	ae_pacer_reset(&state);
	CHECK(!state.start_checked && !state.gave_up);

	if (failures)
		return 1;
	printf("ae_frame_pacer_rules: ok\n");
	return 0;
}
