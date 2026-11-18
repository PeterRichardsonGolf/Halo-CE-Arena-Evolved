/*
AE_DYNAMIC_RESOLUTION_RULES.H

TARGET 60 FPS (display.target_60_fps for one player, display.split_target_60_fps for two to four): the 3D picture of
every view is drawn at a part of its resolution each way and scaled up to the view before the HUD and menus are drawn
over it, so that a frame takes no more than AE_DYNRES_SLOW_MS (60 frames a second with room). Pure rules
(port/linux/tests/ae_dynamic_resolution_rules_test.c); ae_dynamic_resolution.c feeds them each frame's time.

- The scale is a whole percent each way, from 100 down to AE_DYNRES_FLOOR, in steps of AE_DYNRES_STEP.
- Down fast: after AE_DYNRES_SLOW_FRAMES frames in a row over AE_DYNRES_SLOW_MS, the scale drops to what the last of
  them says would fit (the pixels drawn go with the square of the scale), at least one step.
- Up slowly: after AE_DYNRES_FAST_FRAMES frames in a row under AE_DYNRES_FAST_MS, one step up.
- After a change the next AE_DYNRES_SETTLE_FRAMES frames are not counted (the GPU's timings come a few frames late).
- A frame of AE_DYNRES_HITCH_MS or more (a load, a hitch) counts neither way and starts both counts again.
- (0.9.2) With V-SYNC holding the frames (a refresh interval is given: ae_dynres_target_interval_ms) a frame shown
  one interval after the last is on target: the ms rules above are not used. A miss is a frame shown more than
  AE_DYNRES_MISS_FACTOR intervals after the last (the presented gap, swap to swap: a frame of 18 ms of work under a
  16.7 ms refresh is shown 33 ms after the last). AE_DYNRES_SLOW_FRAMES misses in the last AE_DYNRES_WINDOW_MS drop
  the scale (to what the frames' work says fits 90 % of the interval, at least a step). The drop is judged over
  AE_DYNRES_JUDGE_MS: unless the share of missed frames at least halved (against the share over as long a span before
  it, not the window alone), it is undone and no drop is tried for
  AE_DYNRES_HOLD_MS. After AE_DYNRES_QUIET_MS in which the window never held AE_DYNRES_QUIET_MISSES misses (a stray
  hitch does not count) the scale probes one step up; that many misses in the AE_DYNRES_PROBE_WATCH_MS after the probe
  take it back down, and so does a drop within AE_DYNRES_PROBE_FAIL_MS of a probe; either way no probe is tried for
  AE_DYNRES_PROBE_HOLD_MS. Every time here is ms of presented frames, so a 144 Hz display waits as long as a 60 Hz one.
  A change of mode (V-SYNC on or off, the refresh known or not) starts every count and timer again.
- The scale is the same for every view of a frame; it goes back to 100 when the setting is off or the number of views
  changes, and the next AE_DYNRES_START_FRAMES frames are not counted (a game's first frames, its players spawning,
  are slow once: on the box's GPU three of them dropped a view to 60 % for the eight seconds it took to climb back).
*/

#ifndef AE_DYNAMIC_RESOLUTION_RULES_H
#define AE_DYNAMIC_RESOLUTION_RULES_H

#define AE_DYNRES_FULL 100
#define AE_DYNRES_FLOOR 50
#define AE_DYNRES_STEP 5
#define AE_DYNRES_SLOW_MS 15.0f
#define AE_DYNRES_SLOW_FRAMES 3
#define AE_DYNRES_FAST_MS 12.0f
#define AE_DYNRES_FAST_FRAMES 60
#define AE_DYNRES_SETTLE_FRAMES 3
#define AE_DYNRES_START_FRAMES 90
#define AE_DYNRES_HITCH_MS 250.0f
/* (0.9.1) a drop is judged: the frames after it, settled, against the slow ones before it */
#define AE_DYNRES_JUDGE_FRAMES 20
#define AE_DYNRES_GAIN 0.92f
#define AE_DYNRES_HOLD_FRAMES 1800
/* (0.9.2) with a refresh interval (times in ms of presented frames) */
#define AE_DYNRES_TARGET_MS (1000.0f / 60.0f)
#define AE_DYNRES_MISS_FACTOR 1.3f
#define AE_DYNRES_FIT_SHARE 0.9f
#define AE_DYNRES_WINDOW_MS 500.0f
/* (enough for AE_DYNRES_JUDGE_MS of presented frames at 250 Hz) */
#define AE_DYNRES_RING 256
#define AE_DYNRES_JUDGE_MS 1000.0f
#define AE_DYNRES_HOLD_MS 30000.0f
#define AE_DYNRES_QUIET_MS 4000.0f
#define AE_DYNRES_QUIET_MISSES 2
#define AE_DYNRES_PROBE_WATCH_MS 2000.0f
#define AE_DYNRES_PROBE_FAIL_MS 10000.0f
#define AE_DYNRES_PROBE_HOLD_MS 30000.0f

/* why the scale last changed (ae_dynres_reason_text) */
enum
{
	AE_DYNRES_REASON_NONE,
	AE_DYNRES_REASON_RESET,
	AE_DYNRES_REASON_SLOW,
	AE_DYNRES_REASON_FAST,
	AE_DYNRES_REASON_MISSED,
	AE_DYNRES_REASON_PROBE_UP,
	AE_DYNRES_REASON_PROBE_FAILED,
	AE_DYNRES_REASON_NO_GAIN
};

struct ae_dynres_state
{
	/* the scale, percent each way */
	int percent;
	int slow_frames, fast_frames, settle_frames;
	/* (0.9.1) the slow frames' time before the last drop, the percent dropped from, the frames still to judge it and
	their time, and the frames no drop is tried for after one that did not help */
	float slow_sum_ms, before_ms, judge_sum_ms;
	int dropped_from, judge_frames, judged, hold_frames;
	/* what the scale was last set up for: whether it is on, and the number of views */
	int on, views;
	/* (0.9.2) the reason of the last change (AE_DYNRES_REASON_*), taken by the caller; whether the frames before were
	judged by a refresh interval */
	int reason, interval_mode;
	/* (0.9.2, interval mode) the recent frames' presented gaps (a ring, newest at ring_at - 1) */
	float ring[AE_DYNRES_RING];
	int ring_used, ring_at;
	/* (interval mode) a drop being judged: the share of missed frames before it, the frames and misses since, the ms
	left; the ms no drop is tried for; the ms the window stayed quiet; a probe's watch and the time since it; the hold
	on probes; the percent a probe came from */
	float miss_share_before, judge_ms, hold_ms, quiet_ms, probe_watch_ms, since_probe_ms, probe_hold_ms;
	int judge_seen, judge_missed, probe_from;
};

/* the state at 100 percent */
void ae_dynres_reset(struct ae_dynres_state *state);

/* one frame: whether the setting is on for these views now, how many views, the frame's time (ms; the slower of the
CPU's work and the GPU's, as measured). Returns the percent for the next frame.
(0.9.1) A drop is judged: AE_DYNRES_JUDGE_FRAMES frames after it settles, their mean against the mean of the slow
frames that caused it. Not under AE_DYNRES_GAIN of it (the frame is the CPU's, not the pixels'), the scale goes back up
to where it was and no drop is tried for AE_DYNRES_HOLD_FRAMES: a game the processor holds back is not drawn soft for
nothing. The GPU timer cannot tell the two apart (its time includes the GPU waiting for the CPU). */
int ae_dynres_frame(struct ae_dynres_state *state, int on, int views, float frame_ms, float interval_ms,
	float shown_ms);

/* (0.9.2) interval_ms: the interval a frame is judged by while V-SYNC holds the frames (ae_dynres_target_interval_ms;
0: the ms rules); shown_ms: the time from the last frame shown to the one before (swap to swap; 0 or below when not
known: frame_ms stands in). The reason of the last change, and no more of it until the next change; "" for none */
const char *ae_dynres_take_reason(struct ae_dynres_state *state);

/* (0.9.2) the interval for ae_dynres_frame: 0 without V-SYNC; with it the display's refresh interval (refresh_ms; 0 or
below when not known: a 60 Hz display is assumed), never under AE_DYNRES_TARGET_MS (a faster display still targets
60 frames a second) */
float ae_dynres_target_interval_ms(int vsync, float refresh_ms);

/* (0.9.2) whether the rules need the GPU's time for the next frame (interval_ms as given to ae_dynres_frame): not while
V-SYNC holds the frames at 100 % with no drop being judged, where a miss is seen from the presented gap alone and the
GPU timer's small cost each frame buys nothing; the first drop is then sized from the CPU's work (at least a step) and
the timer runs again below 100 %. Always without V-SYNC (the ms rules read the frame's time) */
int ae_dynres_needs_gpu_time(struct ae_dynres_state const *state, float interval_ms);

/* the percent that would fit a frame of frame_ms drawn at percent: percent x sqrt(AE_DYNRES_SLOW_MS / frame_ms),
down to a step and kept from AE_DYNRES_FLOOR to AE_DYNRES_FULL */
int ae_dynres_fitting_percent(int percent, float frame_ms);

/* a forced percent (debug.dynamic_resolution_force, HALO_DYNAMIC_RESOLUTION_FORCE): 0 for none, else kept from
AE_DYNRES_FLOOR to AE_DYNRES_FULL (a value of 1 to 49 is the floor) */
int ae_dynres_forced(long value);

#endif
