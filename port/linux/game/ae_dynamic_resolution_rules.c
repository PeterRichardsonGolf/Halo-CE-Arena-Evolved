/*
AE_DYNAMIC_RESOLUTION_RULES.C

TARGET 60 FPS: the scale's rules (ae_dynamic_resolution_rules.h).
*/

#include <math.h>

#include "ae_dynamic_resolution_rules.h"

static int clamp_percent(int percent)
{
	return percent < AE_DYNRES_FLOOR ? AE_DYNRES_FLOOR : percent > AE_DYNRES_FULL ? AE_DYNRES_FULL : percent;
}

/* (interval mode) every count and timer of its rules started again; no probe was made */
static void interval_clear(struct ae_dynres_state *state)
{
	state->ring_used = state->ring_at = 0;
	state->miss_share_before = state->judge_ms = state->hold_ms = state->quiet_ms = 0.0f;
	state->probe_watch_ms = state->probe_hold_ms = 0.0f;
	state->since_probe_ms = AE_DYNRES_PROBE_FAIL_MS;
	state->judge_seen = state->judge_missed = state->probe_from = 0;
}

void ae_dynres_reset(struct ae_dynres_state *state)
{
	state->percent = AE_DYNRES_FULL;
	state->slow_frames = 0;
	state->fast_frames = 0;
	state->settle_frames = 0;
	state->slow_sum_ms = state->before_ms = state->judge_sum_ms = 0.0f;
	state->dropped_from = state->judge_frames = state->judged = state->hold_frames = 0;
	state->reason = AE_DYNRES_REASON_NONE;
	state->interval_mode = 0;
	interval_clear(state);
}

const char *ae_dynres_take_reason(struct ae_dynres_state *state)
{
	int reason = state->reason;

	state->reason = AE_DYNRES_REASON_NONE;
	switch (reason)
	{
	case AE_DYNRES_REASON_RESET: return "(setting or views changed)";
	case AE_DYNRES_REASON_SLOW: return "(slow frames)";
	case AE_DYNRES_REASON_FAST: return "(fast frames)";
	case AE_DYNRES_REASON_MISSED: return "(missed refreshes)";
	case AE_DYNRES_REASON_PROBE_UP: return "(probe up)";
	case AE_DYNRES_REASON_PROBE_FAILED: return "(probe missed: back down, held 30 s)";
	case AE_DYNRES_REASON_NO_GAIN: return "(no gain: undone, held 30 s)";
	default: return "";
	}
}

static int fitting_for(int percent, float frame_ms, float target_ms)
{
	float fitting;
	int stepped;

	if (!(frame_ms > 0.0f))
		return clamp_percent(percent);
	fitting = (float)percent * sqrtf(target_ms / frame_ms);
	/* (down to a whole step) */
	stepped = ((int)fitting / AE_DYNRES_STEP) * AE_DYNRES_STEP;
	return clamp_percent(stepped);
}

int ae_dynres_fitting_percent(int percent, float frame_ms)
{
	return fitting_for(percent, frame_ms, AE_DYNRES_SLOW_MS);
}

float ae_dynres_target_interval_ms(int vsync, float refresh_ms)
{
	if (!vsync)
		return 0.0f;
	/* (V-SYNC on and the refresh not known: a 60 Hz display, not the ms rules, which call a held 60 slow) */
	if (!(refresh_ms > 0.0f) || refresh_ms < AE_DYNRES_TARGET_MS)
		return AE_DYNRES_TARGET_MS;
	return refresh_ms;
}

int ae_dynres_needs_gpu_time(struct ae_dynres_state const *state, float interval_ms)
{
	return !(interval_ms > 0.0f && state->percent >= AE_DYNRES_FULL && !(state->judge_ms > 0.0f));
}

/* (interval mode: the recent presented gaps; add one) */
static void ring_add(struct ae_dynres_state *state, float shown_ms)
{
	state->ring[state->ring_at] = shown_ms;
	state->ring_at = (state->ring_at + 1) % AE_DYNRES_RING;
	if (state->ring_used < AE_DYNRES_RING)
		state->ring_used++;
}

/* (the misses in the window: the newest frames back to AE_DYNRES_WINDOW_MS of presented time, the newest always in
it; *frames: how many frames it holds) */
static int window_misses(struct ae_dynres_state const *state, float limit_ms, int *frames)
{
	float sum = 0.0f;
	int i, counted = 0, misses = 0;

	for (i = 0; i < state->ring_used; i++)
	{
		float shown = state->ring[(state->ring_at - 1 - i + AE_DYNRES_RING) % AE_DYNRES_RING];

		if (counted && sum + shown > AE_DYNRES_WINDOW_MS)
			break;
		sum += shown;
		counted++;
		if (shown > limit_ms)
			misses++;
	}
	if (frames)
		*frames = counted;
	return misses;
}

static float count_down(float left_ms, float shown_ms)
{
	return left_ms > shown_ms ? left_ms - shown_ms : 0.0f;
}

/* (0.9.1, the ms rules) a drop being judged: the frames after it, settled, against the slow ones before it */
static void judge_frame(struct ae_dynres_state *state, float frame_ms)
{
	state->judge_sum_ms += frame_ms;
	state->judged++;
	if (--state->judge_frames == 0)
	{
		float after_ms = state->judge_sum_ms / (float)state->judged;

		if (after_ms > state->before_ms * AE_DYNRES_GAIN)
		{
			/* (no gain: the pixels were not what held the frame back) */
			state->percent = clamp_percent(state->dropped_from);
			state->hold_frames = AE_DYNRES_HOLD_FRAMES;
			state->settle_frames = AE_DYNRES_SETTLE_FRAMES;
			state->reason = AE_DYNRES_REASON_NO_GAIN;
		}
		state->slow_frames = state->fast_frames = 0;
	}
}

/* (0.9.2) one frame with V-SYNC holding the frames: frame_ms is its work, shown_ms the gap it was shown after */
static void interval_frame(struct ae_dynres_state *state, float frame_ms, float shown_ms, float interval_ms)
{
	float limit_ms = interval_ms * AE_DYNRES_MISS_FACTOR;
	int missed = shown_ms > limit_ms, misses, frames;

	state->hold_ms = count_down(state->hold_ms, shown_ms);
	state->probe_hold_ms = count_down(state->probe_hold_ms, shown_ms);
	state->probe_watch_ms = count_down(state->probe_watch_ms, shown_ms);
	if (state->since_probe_ms < AE_DYNRES_PROBE_FAIL_MS)
		state->since_probe_ms += shown_ms;
	ring_add(state, shown_ms);

	/* (a drop being judged: the share of missed frames after it against the share before) */
	if (state->judge_ms > 0.0f)
	{
		state->judge_seen++;
		state->judge_missed += missed;
		state->judge_ms = count_down(state->judge_ms, shown_ms);
		if (state->judge_ms <= 0.0f)
		{
			float share_after = (float)state->judge_missed / (float)state->judge_seen;

			if (share_after > state->miss_share_before * 0.5f)
			{
				/* (no gain: the pixels were not what made the frames miss) */
				state->percent = clamp_percent(state->dropped_from);
				state->hold_ms = AE_DYNRES_HOLD_MS;
				state->settle_frames = AE_DYNRES_SETTLE_FRAMES;
				state->reason = AE_DYNRES_REASON_NO_GAIN;
				state->ring_used = state->ring_at = 0;
			}
			state->quiet_ms = 0.0f;
		}
		return;
	}

	misses = window_misses(state, limit_ms, &frames);
	if (state->probe_watch_ms > 0.0f && misses >= AE_DYNRES_QUIET_MISSES)
	{
		/* (the probe's step is not held: back down, and no probe for a while) */
		state->percent = clamp_percent(state->probe_from);
		state->probe_watch_ms = 0.0f;
		state->probe_hold_ms = AE_DYNRES_PROBE_HOLD_MS;
		state->quiet_ms = 0.0f;
		state->settle_frames = AE_DYNRES_SETTLE_FRAMES;
		state->reason = AE_DYNRES_REASON_PROBE_FAILED;
		state->ring_used = state->ring_at = 0;
		return;
	}
	/* (quiet: the window never holds AE_DYNRES_QUIET_MISSES misses; a stray hitch does not start it again) */
	if (misses >= AE_DYNRES_QUIET_MISSES)
		state->quiet_ms = 0.0f;
	else
		state->quiet_ms += shown_ms;
	if (misses >= AE_DYNRES_SLOW_FRAMES && state->percent > AE_DYNRES_FLOOR && state->hold_ms <= 0.0f)
	{
		int fitting = fitting_for(state->percent, frame_ms, interval_ms * AE_DYNRES_FIT_SHARE);

		state->miss_share_before = (float)misses / (float)frames;
		state->dropped_from = state->percent;
		state->percent = clamp_percent(fitting < state->percent ? fitting : state->percent - AE_DYNRES_STEP);
		state->settle_frames = AE_DYNRES_SETTLE_FRAMES;
		state->judge_ms = AE_DYNRES_JUDGE_MS;
		state->judge_seen = state->judge_missed = 0;
		state->quiet_ms = 0.0f;
		state->probe_watch_ms = 0.0f;
		/* (a drop soon after a probe: the probe's level does not hold here; no probe for a while) */
		if (state->since_probe_ms < AE_DYNRES_PROBE_FAIL_MS)
		{
			state->probe_hold_ms = AE_DYNRES_PROBE_HOLD_MS;
			state->since_probe_ms = AE_DYNRES_PROBE_FAIL_MS;
		}
		state->reason = AE_DYNRES_REASON_MISSED;
		state->ring_used = state->ring_at = 0;
		return;
	}
	if (state->quiet_ms >= AE_DYNRES_QUIET_MS && state->percent < AE_DYNRES_FULL && state->hold_ms <= 0.0f &&
		state->probe_hold_ms <= 0.0f)
	{
		state->probe_from = state->percent;
		state->percent = clamp_percent(state->percent + AE_DYNRES_STEP);
		state->probe_watch_ms = AE_DYNRES_PROBE_WATCH_MS;
		state->since_probe_ms = 0.0f;
		state->quiet_ms = 0.0f;
		state->settle_frames = AE_DYNRES_SETTLE_FRAMES;
		state->reason = AE_DYNRES_REASON_PROBE_UP;
		state->ring_used = state->ring_at = 0;
	}
}

int ae_dynres_frame(struct ae_dynres_state *state, int on, int views, float frame_ms, float interval_ms,
	float shown_ms)
{
	int interval_mode = interval_ms > 0.0f;

	if (!on || on != state->on || views != state->views)
	{
		int was = state->percent;

		ae_dynres_reset(state);
		if (was != AE_DYNRES_FULL)
			state->reason = AE_DYNRES_REASON_RESET;
		state->on = on;
		state->views = views;
		if (on)
			state->settle_frames = AE_DYNRES_START_FRAMES;
		return state->percent;
	}
	if (interval_mode != state->interval_mode)
	{
		/* (V-SYNC went on or off, or the refresh became known: every count, timer and judgement of the other rules
		means nothing here) */
		state->interval_mode = interval_mode;
		state->slow_frames = state->fast_frames = 0;
		state->judge_frames = state->judged = state->hold_frames = 0;
		state->judge_sum_ms = state->before_ms = state->slow_sum_ms = 0.0f;
		interval_clear(state);
	}
	if (state->settle_frames > 0)
	{
		state->settle_frames--;
		return state->percent;
	}
	if (!(shown_ms > 0.0f))
		shown_ms = frame_ms;
	if (!(frame_ms >= 0.0f) || frame_ms >= AE_DYNRES_HITCH_MS || shown_ms >= AE_DYNRES_HITCH_MS)
	{
		/* (a load or a hitch: no frame of either rules; the counts start again) */
		state->slow_frames = 0;
		state->fast_frames = 0;
		state->quiet_ms = 0.0f;
		state->ring_used = state->ring_at = 0;
		return state->percent;
	}
	if (interval_mode)
	{
		interval_frame(state, frame_ms, shown_ms, interval_ms);
		return state->percent;
	}
	/* (a drop being judged: these frames are measured, nothing else moves the scale) */
	if (state->judge_frames > 0)
	{
		judge_frame(state, frame_ms);
		return state->percent;
	}
	if (state->hold_frames > 0)
		state->hold_frames--;
	if (frame_ms > AE_DYNRES_SLOW_MS)
	{
		state->fast_frames = 0;
		state->slow_sum_ms = state->slow_frames ? state->slow_sum_ms + frame_ms : frame_ms;
		if (++state->slow_frames >= AE_DYNRES_SLOW_FRAMES && state->percent > AE_DYNRES_FLOOR &&
			!state->hold_frames)
		{
			int fitting = ae_dynres_fitting_percent(state->percent, frame_ms);

			state->before_ms = state->slow_sum_ms / (float)state->slow_frames;
			state->dropped_from = state->percent;
			state->percent = clamp_percent(fitting < state->percent ? fitting : state->percent - AE_DYNRES_STEP);
			state->slow_frames = 0;
			state->settle_frames = AE_DYNRES_SETTLE_FRAMES;
			state->judge_frames = AE_DYNRES_JUDGE_FRAMES;
			state->judge_sum_ms = 0.0f;
			state->judged = 0;
			state->reason = AE_DYNRES_REASON_SLOW;
		}
		return state->percent;
	}
	state->slow_frames = 0;
	if (frame_ms < AE_DYNRES_FAST_MS)
	{
		if (++state->fast_frames >= AE_DYNRES_FAST_FRAMES && state->percent < AE_DYNRES_FULL)
		{
			state->percent = clamp_percent(state->percent + AE_DYNRES_STEP);
			state->fast_frames = 0;
			state->settle_frames = AE_DYNRES_SETTLE_FRAMES;
			state->reason = AE_DYNRES_REASON_FAST;
		}
	}
	else
	{
		/* (between: neither, and the run of fast frames starts again) */
		state->fast_frames = 0;
	}
	return state->percent;
}

int ae_dynres_forced(long value)
{
	if (value <= 0)
		return 0;
	return value >= AE_DYNRES_FULL ? AE_DYNRES_FULL : value < AE_DYNRES_FLOOR ? AE_DYNRES_FLOOR : (int)value;
}
