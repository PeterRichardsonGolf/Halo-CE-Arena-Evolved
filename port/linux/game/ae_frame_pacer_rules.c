/*
AE_FRAME_PACER_RULES.C

LOW LATENCY's rules (ae_frame_pacer_rules.h).
*/

#include <string.h>

#include "ae_frame_pacer_rules.h"

#define NS_PER_MS 1.0e6

void ae_pacer_forget(struct ae_pacer_state *state)
{
	state->gap_count = state->gap_next = 0;
	state->cost_count = state->cost_next = 0;
	state->skip_next = 0;
	state->suspended = 0;
	state->late_run = 0;
}

void ae_pacer_reset(struct ae_pacer_state *state)
{
	memset(state, 0, sizeof(*state));
	state->margin_ms = AE_PACER_MARGIN_MS;
}

/* the median of a ring's values (the middle one; the lower of the two middle ones for an even count) */
static float median(float const *values, int count)
{
	float sorted[AE_PACER_GAPS];
	int i, j;

	for (i = 0; i < count; i++)
	{
		float value = values[i];

		for (j = i; j > 0 && sorted[j - 1] > value; j--)
			sorted[j] = sorted[j - 1];
		sorted[j] = value;
	}
	return sorted[(count - 1) / 2];
}

float ae_pacer_period_ms(struct ae_pacer_state const *state)
{
	return state->gap_count >= AE_PACER_LEARN ? median(state->gaps, state->gap_count) : 0.0f;
}

float ae_pacer_cost_ms(struct ae_pacer_state const *state)
{
	float most = -1.0f;
	int i;

	for (i = 0; i < state->cost_count; i++)
		if (state->costs[i] > most)
			most = state->costs[i];
	return most;
}

void ae_pacer_frame(struct ae_pacer_state *state, float cpu_ms, float gpu_ms)
{
	float cost;

	if (!(cpu_ms >= 0.0f))
		return;
	cost = gpu_ms >= 0.0f ? (gpu_ms > cpu_ms ? gpu_ms : cpu_ms) : cpu_ms + AE_PACER_GPU_UNKNOWN_MS;
	state->costs[state->cost_next] = cost;
	state->cost_next = (state->cost_next + 1) % AE_PACER_COSTS;
	if (state->cost_count < AE_PACER_COSTS)
		state->cost_count++;
}

void ae_pacer_swap(struct ae_pacer_state *state, unsigned long long now_ns, unsigned long long swap_ns)
{
	float gap, period, swap_ms;

	if (state->gave_up)
		return;
	if (!state->last_return_ns || now_ns <= state->last_return_ns)
	{
		state->last_return_ns = now_ns;
		return;
	}
	gap = (float)((double)(now_ns - state->last_return_ns) / NS_PER_MS);
	swap_ms = (float)((double)swap_ns / NS_PER_MS);
	state->last_return_ns = now_ns;
	period = ae_pacer_period_ms(state);
	/* (a hitch before any period is known, a load: not a gap between frames) */
	if (period <= 0.0f && gap > AE_PACER_HITCH_MS)
		return;
	if (period > 0.0f)
	{
		if (swap_ms > AE_PACER_LATE_PERIODS * period)
		{
			/* (a full driver queue holds a GPU-bound frame's swap: no late frame, no gap, and no wait next) */
			state->skip_next = 1;
			state->event = AE_PACER_EVENT_STALL;
			return;
		}
		if (gap >= AE_PACER_PAUSE_PERIODS * period)
		{
			ae_pacer_forget(state);
			return;
		}
		if (gap > AE_PACER_LATE_PERIODS * period)
		{
			state->late++;
			if (++state->late_run >= AE_PACER_RELEARN_LATE)
			{
				/* (the refresh is slower than the period learnt: learn it again) */
				ae_pacer_forget(state);
				state->event = AE_PACER_EVENT_LATE;
				return;
			}
			state->skip_next = 1;
			state->margin_ms += AE_PACER_MARGIN_GROW_MS;
			if (state->margin_ms > AE_PACER_MARGIN_MAX_MS)
				state->margin_ms = AE_PACER_MARGIN_MAX_MS;
			state->event = AE_PACER_EVENT_LATE;
			return;
		}
	}
	state->late_run = 0;
	state->gaps[state->gap_next] = gap;
	state->gap_next = (state->gap_next + 1) % AE_PACER_GAPS;
	if (state->gap_count < AE_PACER_GAPS)
		state->gap_count++;
	state->margin_ms -= AE_PACER_MARGIN_SHRINK_MS;
	if (state->margin_ms < AE_PACER_MARGIN_MS)
		state->margin_ms = AE_PACER_MARGIN_MS;
	period = ae_pacer_period_ms(state);
	if (period > 0.0f && period < AE_PACER_PERIOD_MIN_MS)
	{
		state->gave_up = 1;
		state->event = AE_PACER_EVENT_NOT_A_DISPLAY;
	}
	else if (period > AE_PACER_PERIOD_MAX_MS)
	{
		/* (a game too slow to pace: nothing waits, the period is learnt again; said once) */
		if (!state->too_slow)
			state->event = AE_PACER_EVENT_TOO_SLOW;
		state->too_slow = 1;
		ae_pacer_forget(state);
	}
	else if (period > 0.0f)
		state->too_slow = 0;
}

unsigned long long ae_pacer_wait(struct ae_pacer_state *state, unsigned long long now_ns)
{
	float period = ae_pacer_period_ms(state);
	float cost = ae_pacer_cost_ms(state);
	float need;
	unsigned long long next_blank, target;

	if (state->gave_up || period <= 0.0f || cost < 0.0f || !state->last_return_ns)
		return 0;
	need = cost + state->margin_ms;
	if (need > period * AE_PACER_SKIP_PERCENT / 100.0f)
	{
		if (!state->suspended)
			state->event = AE_PACER_EVENT_OVER_BUDGET;
		state->suspended = 1;
		state->skip_next = 0;
		return 0;
	}
	if (state->suspended)
	{
		if (need > period * AE_PACER_RESUME_PERCENT / 100.0f)
			return 0;
		state->suspended = 0;
		state->event = AE_PACER_EVENT_FAST_AGAIN;
	}
	if (state->skip_next)
	{
		state->skip_next = 0;
		return 0;
	}
	next_blank = state->last_return_ns + (unsigned long long)((double)period * NS_PER_MS);
	target = next_blank - (unsigned long long)((double)need * NS_PER_MS);
	if (now_ns >= target || target - now_ns > (unsigned long long)((double)period * NS_PER_MS))
		return 0;
	return target - now_ns;
}

int ae_pacer_take_event(struct ae_pacer_state *state)
{
	int event = state->event;

	state->event = AE_PACER_EVENT_NONE;
	return event;
}
