#include <stdio.h>
#include <string.h>

#include "ae_dynamic_resolution_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

#define I60 (1000.0f / 60.0f)

/* (0.9.2) n frames with V-SYNC at interval_ms: work_ms of work each, shown shown_ms after the last; the percent after
the last */
static int vframes(struct ae_dynres_state *state, float interval_ms, float work_ms, float shown_ms, int n)
{
	int percent = state->percent;

	while (n-- > 0)
		percent = ae_dynres_frame(state, 60, 1, work_ms, interval_ms, shown_ms);
	return percent;
}

/* frames at 60 Hz, all shown on time */
static int good(struct ae_dynres_state *state, int n)
{
	return vframes(state, I60, 10.0f, I60, n);
}

/* a game at 100 with one view, set up */
static void start(struct ae_dynres_state *state)
{
	ae_dynres_reset(state);
	state->on = 60;
	state->views = 1;
}

/* down to a lower percent by misses, the drop kept: the percent */
static int dropped(struct ae_dynres_state *state)
{
	vframes(state, I60, 18.0f, 33.3f, 3);
	good(state, AE_DYNRES_SETTLE_FRAMES + 70);
	ae_dynres_take_reason(state);
	return state->percent;
}

/* n frames of frame_ms; the percent after the last */
static int frames(struct ae_dynres_state *state, int on, int views, float frame_ms, int n)
{
	int percent = state->percent;

	while (n-- > 0)
		percent = ae_dynres_frame(state, on, views, frame_ms, 0.0f, 0.0f);
	return percent;
}

int main(void)
{
	struct ae_dynres_state state;
	int percent, i;

	ae_dynres_reset(&state);
	state.on = 0;
	state.views = 0;
	CHECK(state.percent == 100);
	/* (the first frame with the setting on sets it up, at 100) */
	CHECK(ae_dynres_frame(&state, 60, 2, 10.0f, 0.0f, 0.0f) == 100);
	/* a game's first frames are not counted, however slow */
	CHECK(frames(&state, 60, 2, 40.0f, AE_DYNRES_START_FRAMES) == 100);

	/* the fitting percent: the pixels go with the square, down to a step; floor and ceiling */
	CHECK(ae_dynres_fitting_percent(100, 15.0f) == 100);
	CHECK(ae_dynres_fitting_percent(100, 20.0f) == 85);   /* 100 x sqrt(0.75) = 86.6 */
	CHECK(ae_dynres_fitting_percent(100, 30.0f) == 70);   /* 70.7 */
	CHECK(ae_dynres_fitting_percent(100, 60.0f) == 50);
	CHECK(ae_dynres_fitting_percent(100, 200.0f) == 50);  /* never under the floor */
	CHECK(ae_dynres_fitting_percent(80, 10.0f) == 95);    /* 97.9: up too, if asked */
	CHECK(ae_dynres_fitting_percent(100, 0.0f) == 100);

	/* down fast: two slow frames do nothing, the third drops to what fits */
	CHECK(frames(&state, 60, 2, 20.0f, 2) == 100);
	CHECK(ae_dynres_frame(&state, 60, 2, 20.0f, 0.0f, 0.0f) == 85);
	/* settling: the next three frames are not counted, however slow */
	CHECK(frames(&state, 60, 2, 40.0f, 3) == 85);
	/* (0.9.1) the drop judged: frames faster than the slow ones (16 against 20 ms) keep it */
	CHECK(frames(&state, 60, 2, 16.0f, AE_DYNRES_JUDGE_FRAMES) == 85);
	/* a slightly slow run: at least one step (kept: 13 against 15.5 ms) */
	CHECK(frames(&state, 60, 2, 15.5f, 3) == 80);
	frames(&state, 60, 2, 10.0f, AE_DYNRES_SETTLE_FRAMES);
	CHECK(frames(&state, 60, 2, 13.0f, AE_DYNRES_JUDGE_FRAMES) == 80);
	/* a very slow run goes to the floor at once, and no lower */
	frames(&state, 60, 2, 10.0f, 3);
	CHECK(frames(&state, 60, 2, 100.0f, 3) == 50);
	frames(&state, 60, 2, 10.0f, AE_DYNRES_SETTLE_FRAMES);
	CHECK(frames(&state, 60, 2, 40.0f, AE_DYNRES_JUDGE_FRAMES) == 50);
	frames(&state, 60, 2, 10.0f, 3);
	CHECK(frames(&state, 60, 2, 100.0f, 10) == 50);

	/* up slowly: 59 fast frames do nothing, the 60th one step (at the floor no change was made, so nothing settles) */
	CHECK(frames(&state, 60, 2, 10.0f, 59) == 50);
	CHECK(ae_dynres_frame(&state, 60, 2, 10.0f, 0.0f, 0.0f) == 55);
	/* a frame between 12 and 15 ms starts the run again */
	frames(&state, 60, 2, 10.0f, 3);
	frames(&state, 60, 2, 10.0f, 50);
	CHECK(ae_dynres_frame(&state, 60, 2, 13.0f, 0.0f, 0.0f) == 55);
	CHECK(frames(&state, 60, 2, 10.0f, 59) == 55);
	CHECK(ae_dynres_frame(&state, 60, 2, 10.0f, 0.0f, 0.0f) == 60);
	/* all the way back to 100 and no further */
	for (i = 0; i < 20; i++)
	{
		frames(&state, 60, 2, 10.0f, 3);
		frames(&state, 60, 2, 10.0f, 60);
	}
	CHECK(state.percent == 100);
	/* a slow frame between fast ones never drops it: three in a row do */
	CHECK(frames(&state, 60, 2, 20.0f, 2) == 100);
	CHECK(ae_dynres_frame(&state, 60, 2, 10.0f, 0.0f, 0.0f) == 100);
	CHECK(frames(&state, 60, 2, 20.0f, 2) == 100);

	/* (0.9.1) a frame the CPU holds back: the drop does not help, so the scale goes back up and holds there */
	ae_dynres_reset(&state);
	state.on = 60;
	state.views = 1;
	CHECK(frames(&state, 60, 1, 20.0f, 3) == 85);
	frames(&state, 60, 1, 20.0f, AE_DYNRES_SETTLE_FRAMES);
	CHECK(frames(&state, 60, 1, 19.5f, AE_DYNRES_JUDGE_FRAMES) == 100);
	CHECK(state.hold_frames == AE_DYNRES_HOLD_FRAMES);
	frames(&state, 60, 1, 20.0f, AE_DYNRES_SETTLE_FRAMES);
	CHECK(frames(&state, 60, 1, 20.0f, AE_DYNRES_HOLD_FRAMES - 10) == 100);
	/* the hold over, a drop is tried again */
	CHECK(frames(&state, 60, 1, 20.0f, 20) == 85);

	/* a hitch (a load) counts neither way */
	ae_dynres_reset(&state);
	state.on = 60;
	state.views = 1;
	CHECK(frames(&state, 60, 1, 20.0f, 2) == 100);
	CHECK(ae_dynres_frame(&state, 60, 1, 900.0f, 0.0f, 0.0f) == 100);
	CHECK(frames(&state, 60, 1, 20.0f, 2) == 100);
	percent = ae_dynres_frame(&state, 60, 1, 20.0f, 0.0f, 0.0f);
	CHECK(percent == 85);

	/* the views change, or the setting goes off: back to 100 */
	CHECK(ae_dynres_frame(&state, 60, 2, 20.0f, 0.0f, 0.0f) == 100);
	CHECK(frames(&state, 60, 2, 30.0f, AE_DYNRES_START_FRAMES) == 100);
	frames(&state, 60, 2, 30.0f, 3);
	CHECK(state.percent == 70);
	CHECK(ae_dynres_frame(&state, 0, 2, 30.0f, 0.0f, 0.0f) == 100);
	CHECK(frames(&state, 0, 2, 30.0f, 10) == 100);

	/* the forced percent */
	CHECK(ae_dynres_forced(0) == 0);
	CHECK(ae_dynres_forced(-3) == 0);
	CHECK(ae_dynres_forced(50) == 50);
	CHECK(ae_dynres_forced(10) == 50);
	CHECK(ae_dynres_forced(73) == 73);
	CHECK(ae_dynres_forced(150) == 100);

	/* (0.9.2) the interval: none without V-SYNC; 60 Hz when V-SYNC is on and the refresh unknown; at a target of 60 never
	under 60 Hz's */
	CHECK(ae_dynres_target_interval_ms(0, I60, 60.0f) == 0.0f);
	CHECK(ae_dynres_target_interval_ms(1, 0.0f, 60.0f) == AE_DYNRES_TARGET_MS);
	CHECK(ae_dynres_target_interval_ms(1, -1.0f, 60.0f) == AE_DYNRES_TARGET_MS);
	CHECK(ae_dynres_target_interval_ms(1, 1000.0f / 144.0f, 60.0f) == AE_DYNRES_TARGET_MS);
	CHECK(ae_dynres_target_interval_ms(1, 1000.0f / 30.0f, 60.0f) == 1000.0f / 30.0f);
	/* (TARGET FRAMERATE) another target: a display faster than it is judged by the target, a slower one by its refresh;
	no target (0) is 60 */
	CHECK(ae_dynres_target_interval_ms(1, 1000.0f / 144.0f, 120.0f) == 1000.0f / 120.0f);
	CHECK(ae_dynres_target_interval_ms(1, 1000.0f / 144.0f, 144.0f) == 1000.0f / 144.0f);
	CHECK(ae_dynres_target_interval_ms(1, 1000.0f / 144.0f, 90.0f) == 1000.0f / 90.0f);
	CHECK(ae_dynres_target_interval_ms(1, I60, 120.0f) == I60);
	CHECK(ae_dynres_target_interval_ms(1, 1000.0f / 144.0f, 0.0f) == AE_DYNRES_TARGET_MS);
	CHECK(ae_dynres_target_interval_ms(0, 1000.0f / 144.0f, 144.0f) == 0.0f);
	/* (TARGET FRAMERATE) the ms rules' times: 15 and 12 at 60, scaled to another target */
	CHECK(ae_dynres_slow_ms(60.0f) == AE_DYNRES_SLOW_MS && ae_dynres_fast_ms(60.0f) == AE_DYNRES_FAST_MS);
	CHECK(ae_dynres_slow_ms(0.0f) == AE_DYNRES_SLOW_MS && ae_dynres_fast_ms(0.0f) == AE_DYNRES_FAST_MS);
	CHECK(ae_dynres_slow_ms(120.0f) > 7.49f && ae_dynres_slow_ms(120.0f) < 7.51f);
	CHECK(ae_dynres_fast_ms(120.0f) > 5.99f && ae_dynres_fast_ms(120.0f) < 6.01f);
	CHECK(ae_dynres_slow_ms(144.0f) > 6.24f && ae_dynres_slow_ms(144.0f) < 6.26f);
	/* (a frame of 10 ms: fast at 60, slow at 120, where three drop the scale; a change of target starts again) */
	ae_dynres_reset(&state);
	state.on = 0;
	frames(&state, 60, 1, 10.0f, AE_DYNRES_START_FRAMES + 1);
	CHECK(frames(&state, 60, 1, 10.0f, 3) == 100);
	CHECK(ae_dynres_frame(&state, 120, 1, 10.0f, 0.0f, 0.0f) == 100 && state.on == 120);
	frames(&state, 120, 1, 10.0f, AE_DYNRES_START_FRAMES);
	CHECK(frames(&state, 120, 1, 10.0f, 3) < 100);
	CHECK(ae_dynres_frame(&state, 0, 1, 10.0f, 0.0f, 0.0f) == 100);

	/* the GPU's time: not needed under V-SYNC at 100 %; needed below 100 %, while a drop is judged, and without V-SYNC */
	start(&state);
	CHECK(!ae_dynres_needs_gpu_time(&state, I60));
	CHECK(ae_dynres_needs_gpu_time(&state, 0.0f));
	vframes(&state, I60, 18.0f, 33.3f, 3);
	CHECK(state.percent < 100 && ae_dynres_needs_gpu_time(&state, I60));
	state.percent = 100;
	CHECK(state.judge_ms > 0.0f && ae_dynres_needs_gpu_time(&state, I60));
	/* and the first drop without it, from the CPU's work alone (10 ms: fits), is one step */
	start(&state);
	CHECK(vframes(&state, I60, 10.0f, 33.3f, 3) == 95);

	/* frames held to V-SYNC are on target: 10 s at the interval, with work up to it and over, no drop */
	start(&state);
	CHECK(vframes(&state, I60, 16.67f, I60, 600) == 100);
	CHECK(vframes(&state, I60, 17.5f, I60, 600) == 100);
	CHECK(state.percent == 100 && ae_dynres_take_reason(&state)[0] == 0);

	/* (review 1) a miss is the presented gap, not the work: 18 ms of work shown every 33 ms misses; three drop it to
	what the work says fits 90 % of the interval (100 x sqrt(15 / 18) = 91: 90) */
	start(&state);
	CHECK(vframes(&state, I60, 18.0f, 33.3f, 2) == 100);
	CHECK(vframes(&state, I60, 18.0f, 33.3f, 1) == 90);
	CHECK(!strcmp(ae_dynres_take_reason(&state), "(missed refreshes)"));
	CHECK(ae_dynres_take_reason(&state)[0] == 0);
	/* a hitch (a load) counts neither way and empties the window: two misses, a hitch, one miss: no drop */
	start(&state);
	vframes(&state, I60, 18.0f, 33.3f, 2);
	CHECK(vframes(&state, I60, 18.0f, 300.0f, 1) == 100);
	CHECK(vframes(&state, I60, 18.0f, 33.3f, 2) == 100);
	CHECK(vframes(&state, I60, 18.0f, 33.3f, 1) == 90);

	/* (review 6) the window's edge: 500 ms of presented time back from the newest frame. Two misses (22 ms), 26 good
	frames and a miss: 22 + 26 x 16.67 + 44 = 499.4 ms holds all three: a drop. With 27 good frames the first miss is
	out of it (516 ms): none */
	start(&state);
	vframes(&state, I60, 10.0f, 22.0f, 2);
	good(&state, 26);
	CHECK(vframes(&state, I60, 10.0f, 22.0f, 1) == 95);
	start(&state);
	vframes(&state, I60, 10.0f, 22.0f, 2);
	good(&state, 27);
	CHECK(vframes(&state, I60, 10.0f, 22.0f, 1) == 100);

	/* the drop helps (frames on time after it): kept */
	start(&state);
	CHECK(dropped(&state) == 90);
	CHECK(state.hold_ms == 0.0f);

	/* (review 2) a drop that does not help: stray CPU spikes, 1 frame in 10 shown at 25 ms, before and after the drop:
	the share of missed frames did not halve, so it is undone and held 30 s */
	start(&state);
	for (i = 0; i < 200 && state.percent == 100; i++)
		vframes(&state, I60, 10.0f, i % 10 == 9 ? 25.0f : I60, 1);
	CHECK(state.percent == 95);
	ae_dynres_take_reason(&state);
	for (i = 0; i < 200 && state.percent == 95; i++)
		vframes(&state, I60, 10.0f, i % 10 == 9 ? 25.0f : I60, 1);
	CHECK(state.percent == 100);
	CHECK(!strcmp(ae_dynres_take_reason(&state), "(no gain: undone, held 30 s)"));
	CHECK(state.hold_ms == AE_DYNRES_HOLD_MS);
	/* held: 29 s more of the same spikes, no drop */
	for (i = 0; i < 28 * 60; i++)
		vframes(&state, I60, 10.0f, i % 10 == 9 ? 25.0f : I60, 1);
	CHECK(state.percent == 100);
	/* the hold over: a drop is tried again */
	for (i = 0; i < 200 && state.percent == 100; i++)
		vframes(&state, I60, 10.0f, i % 10 == 9 ? 25.0f : I60, 1);
	CHECK(state.percent == 95);

	/* (0.9.3) the share before a drop is taken over AE_DYNRES_JUDGE_MS, not the window that just filled with misses: a
	burst of 3 after a clean second, then 1 frame in 30 missing after the drop (0.033). Against the window's share (3 of
	27 frames: 0.11) that looks halved; against the second's (3 of 57: 0.053) it is not, so the drop is undone */
	start(&state);
	good(&state, 60);
	CHECK(vframes(&state, I60, 18.0f, 33.3f, 3) == 90);
	CHECK(state.miss_share_before > 0.05f && state.miss_share_before < 0.056f);
	ae_dynres_take_reason(&state);
	for (i = 0; i < 200 && state.percent == 90; i++)
		vframes(&state, I60, 10.0f, i % 30 == 29 ? 25.0f : I60, 1);
	CHECK(state.percent == 100);
	CHECK(!strcmp(ae_dynres_take_reason(&state), "(no gain: undone, held 30 s)"));
	/* the same burst after a second of misses as bad: the share before is the second's, and the drop that ends them is
	kept */
	start(&state);
	for (i = 0; i < 40; i++)
		vframes(&state, I60, 10.0f, i % 14 == 13 ? 25.0f : I60, 1);
	CHECK(state.percent == 100);
	CHECK(vframes(&state, I60, 18.0f, 33.3f, 3) == 90);
	good(&state, AE_DYNRES_SETTLE_FRAMES + 70);
	CHECK(state.percent == 90);

	/* (0.9.3, kept as it is) a kept drop with a steady 2 misses in each 500 ms after it (1 frame in 15) stays: the window
	never goes quiet, so no probe, and 2 is not the 3 that drop again. The judge found the pixels were what made the
	frames miss, so a step up would miss more */
	start(&state);
	percent = dropped(&state);
	for (i = 0; i < 20 * 60; i++)
		vframes(&state, I60, 10.0f, i % 15 == 14 ? 25.0f : I60, 1);
	CHECK(state.percent == percent);
	CHECK(state.quiet_ms < AE_DYNRES_QUIET_MS);

	/* after the misses stop, a probe up once the window stayed quiet AE_DYNRES_QUIET_MS (4 s: 240 frames at 60 Hz) after
	the drop's judgement (AE_DYNRES_JUDGE_MS, 60 frames, which dropped() is 10 frames past), and on to 100 */
	start(&state);
	percent = dropped(&state);
	good(&state, 228);
	CHECK(state.percent == percent);
	good(&state, 3);
	CHECK(state.percent == percent + AE_DYNRES_STEP);
	CHECK(!strcmp(ae_dynres_take_reason(&state), "(probe up)"));
	for (i = 0; i < 10; i++)
		good(&state, AE_DYNRES_SETTLE_FRAMES + 250);
	CHECK(state.percent == 100);

	/* (review 3) a stray single miss every 3 s does not keep it low: the window never holds two, so it probes */
	start(&state);
	percent = dropped(&state);
	for (i = 0; i < 6 * 60; i++)
		vframes(&state, I60, 10.0f, i % 180 == 179 ? 33.3f : I60, 1);
	CHECK(state.percent > percent);

	/* a probe that misses (two misses in its watch) goes back down and no probe is tried for 30 s */
	start(&state);
	percent = dropped(&state);
	good(&state, 250);
	CHECK(state.percent == percent + AE_DYNRES_STEP);
	good(&state, AE_DYNRES_SETTLE_FRAMES + 30);
	vframes(&state, I60, 10.0f, 33.3f, 1);
	CHECK(state.percent == percent + AE_DYNRES_STEP);
	CHECK(vframes(&state, I60, 10.0f, 33.3f, 1) == percent);
	CHECK(!strcmp(ae_dynres_take_reason(&state), "(probe missed: back down, held 30 s)"));
	CHECK(state.probe_hold_ms == AE_DYNRES_PROBE_HOLD_MS);
	CHECK(good(&state, 29 * 60) == percent);
	CHECK(good(&state, AE_DYNRES_SETTLE_FRAMES + 120) == percent + AE_DYNRES_STEP);

	/* (review 4) a probe's level that misses only after its watch (about 3 s on): the drop holds probes for 30 s, so two
	levels do not flip every few seconds */
	start(&state);
	percent = dropped(&state);
	good(&state, 250);
	CHECK(state.percent == percent + AE_DYNRES_STEP);
	good(&state, AE_DYNRES_SETTLE_FRAMES + 150);
	CHECK(vframes(&state, I60, 18.0f, 33.3f, 3) < percent + AE_DYNRES_STEP);
	CHECK(!strcmp(ae_dynres_take_reason(&state), "(missed refreshes)"));
	CHECK(state.probe_hold_ms == AE_DYNRES_PROBE_HOLD_MS);
	good(&state, AE_DYNRES_SETTLE_FRAMES + 10 * 60);
	CHECK(state.percent < percent + AE_DYNRES_STEP);

	/* (review 5) the times are ms of presented frames: at 144 Hz (frames 6.94 ms apart) the quiet before a probe is
	still 4 s (576 frames), not 240 frames */
	start(&state);
	vframes(&state, AE_DYNRES_TARGET_MS, 18.0f, 33.3f, 3);
	vframes(&state, AE_DYNRES_TARGET_MS, 5.0f, 1000.0f / 144.0f, AE_DYNRES_SETTLE_FRAMES + 150);
	percent = state.percent;
	CHECK(percent < 100);
	CHECK(vframes(&state, AE_DYNRES_TARGET_MS, 5.0f, 1000.0f / 144.0f, 400) == percent);
	CHECK(vframes(&state, AE_DYNRES_TARGET_MS, 5.0f, 1000.0f / 144.0f, 200) == percent + AE_DYNRES_STEP);
	/* and a 144 Hz frame shown on time, or a refresh late (13.9 ms), is no miss: no drop */
	start(&state);
	CHECK(vframes(&state, AE_DYNRES_TARGET_MS, 12.0f, 2000.0f / 144.0f, 1200) == 100);

	/* (review 7) a mode change (V-SYNC off, the refresh no longer known) clears the probe's watch and every timer: back
	in V-SYNC, two misses are no "probe missed" */
	start(&state);
	percent = dropped(&state);
	good(&state, 250);
	CHECK(state.percent == percent + AE_DYNRES_STEP);
	ae_dynres_take_reason(&state);
	ae_dynres_frame(&state, 60, 1, 10.0f, 0.0f, 0.0f);
	CHECK(state.probe_watch_ms == 0.0f && state.probe_hold_ms == 0.0f && state.hold_ms == 0.0f && state.judge_ms == 0.0f);
	good(&state, AE_DYNRES_SETTLE_FRAMES);
	CHECK(vframes(&state, I60, 10.0f, 33.3f, 2) == percent + AE_DYNRES_STEP);
	CHECK(ae_dynres_take_reason(&state)[0] == 0);
	/* and the ms rules' judgement and hold do not carry into V-SYNC */
	start(&state);
	frames(&state, 60, 1, 20.0f, 3);
	CHECK(state.judge_frames > 0);
	CHECK(good(&state, 1) == state.percent && state.judge_frames == 0 && state.hold_frames == 0);

	/* (review 6) the views change in V-SYNC: back to 100, every timer cleared, the reason said */
	start(&state);
	percent = dropped(&state);
	CHECK(percent < 100);
	CHECK(ae_dynres_frame(&state, 60, 2, 10.0f, I60, I60) == 100);
	CHECK(!strcmp(ae_dynres_take_reason(&state), "(setting or views changed)"));
	CHECK(state.hold_ms == 0.0f && state.quiet_ms == 0.0f && state.ring_used == 0);

	/* the ms rules' reasons */
	ae_dynres_reset(&state);
	state.on = 60;
	state.views = 1;
	frames(&state, 60, 1, 10.0f, AE_DYNRES_START_FRAMES + 1);
	frames(&state, 60, 1, 20.0f, 3);
	CHECK(!strcmp(ae_dynres_take_reason(&state), "(slow frames)"));

	if (failures)
		return 1;
	printf("ae_dynamic_resolution_rules: ok\n");
	return 0;
}
