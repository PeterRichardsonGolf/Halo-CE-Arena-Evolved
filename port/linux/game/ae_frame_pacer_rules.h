/*
AE_FRAME_PACER_RULES.H

LOW LATENCY (display.low_latency "on" and "boost"): with V-SYNC on, the game sleeps before it reads input so that the
frame finishes just before the next blank, not a whole refresh before it. Pure rules
(port/linux/tests/ae_frame_pacer_rules_test.c); ae_frame_pacer.c feeds them each frame's times.

- The period is the median of the last AE_PACER_GAPS gaps between swaps returning. Nothing is waited for until
  AE_PACER_LEARN gaps are known. Where the display mode's refresh is known (ae_pacer_display), a period more than
  AE_PACER_DISPLAY_PERCENT off it is not the display's (frames that miss blanks learn two refreshes or something in
  between; a variable refresh rate under its top shows each frame when it is done): it is forgotten and learnt again,
  and the AE_PACER_WRONG_PERIODS-th time the rules give up until ae_pacer_reset, and say so. Where it is not known,
  the measured period is used as it is.
- The cost is the largest of the last AE_PACER_COSTS frames' time, from the frame's start (after the wait) to the
  graphics driver having it all: the slower of the CPU's work and the GPU's (AE_PACER_GPU_UNKNOWN_MS more than the
  CPU's when the GPU's is not known).
- The wait ends at the next blank (the last swap's return and one period) less the cost and the margin. The margin
  starts at AE_PACER_MARGIN_MS, grows AE_PACER_MARGIN_GROW_MS after each late frame (up to AE_PACER_MARGIN_MAX_MS) and
  shrinks AE_PACER_MARGIN_SHRINK_MS a frame.
- No wait while cost + margin is over AE_PACER_SKIP_PERCENT of the period (no wait would help); waiting again from
  AE_PACER_RESUME_PERCENT, well under it, so that a frame near the line does not turn the wait on and off.
- No time to spare from the start: the first time AE_PACER_COSTS frames' costs and the period are known, nothing has
  been waited for yet; if the middle of those costs and the margin are over AE_PACER_SKIP_PERCENT of the period, the
  rules give up until ae_pacer_reset at once, and say so (a PC that never has time to spare does not switch the wait
  on and off for its first seconds). The largest cost, not the middle, decides every wait after that.
- No time to spare: AE_PACER_GIVE_UP_STOPS such stops, or AE_PACER_GIVE_UP_LATE late frames, within
  AE_PACER_GIVE_UP_MS give up until ae_pacer_reset, and say so. Each stop and each miss moves the delay between the
  hands and the screen by up to a refresh, which feels worse than a steady refresh more.
- A late frame is one whose swap returned more than AE_PACER_LATE_PERIODS periods after the last: the blank was
  missed. It counts, grows the margin, and the next frame is not waited for.
- A swap that itself took that long (a driver whose queue was full holds it, which a GPU-bound frame does) is not
  counted and not timed: the next frame is not waited for.
- A gap of AE_PACER_PAUSE_PERIODS periods or more (a load, a pause, a hitch) is no frame: the period and the cost
  are forgotten, nothing counts.
- A gap of AE_PACER_HITCH_MS or more before any period is known is no gap either.
- A period under AE_PACER_PERIOD_MIN_MS is not a display (the swaps do not wait for one): the rules give up until
  ae_pacer_reset, and say so. One over AE_PACER_PERIOD_MAX_MS is a game too slow to pace (under 20 frames a second):
  nothing waits, the period is learnt again, and that is said once until a period in bounds is learnt.
*/

#ifndef AE_FRAME_PACER_RULES_H
#define AE_FRAME_PACER_RULES_H

#define AE_PACER_GAPS 32
#define AE_PACER_LEARN 8
#define AE_PACER_COSTS 60
#define AE_PACER_GPU_UNKNOWN_MS 1.5f
#define AE_PACER_MARGIN_MS 1.0f
#define AE_PACER_MARGIN_GROW_MS 0.5f
#define AE_PACER_MARGIN_MAX_MS 4.0f
#define AE_PACER_MARGIN_SHRINK_MS 0.005f
#define AE_PACER_SKIP_PERCENT 85.0f
#define AE_PACER_RESUME_PERCENT 65.0f
#define AE_PACER_DISPLAY_PERCENT 3.0f
#define AE_PACER_WRONG_PERIODS 3
#define AE_PACER_GIVE_UP_STOPS 3
#define AE_PACER_GIVE_UP_LATE 10
#define AE_PACER_GIVE_UP_MS 60000.0
#define AE_PACER_LATE_PERIODS 1.5f
#define AE_PACER_PAUSE_PERIODS 20.0f
#define AE_PACER_PERIOD_MIN_MS 2.0f
#define AE_PACER_PERIOD_MAX_MS 50.0f
#define AE_PACER_HITCH_MS 250.0f

/* what happened, for the log: ae_pacer_take_event */
enum
{
	AE_PACER_EVENT_NONE,
	AE_PACER_EVENT_LATE,
	AE_PACER_EVENT_STALL,
	AE_PACER_EVENT_NOT_A_DISPLAY,
	AE_PACER_EVENT_TOO_SLOW,
	AE_PACER_EVENT_OVER_BUDGET,
	AE_PACER_EVENT_FAST_AGAIN,
	/* given up until ae_pacer_reset: the period is not the display's; too many stops; too many late frames */
	AE_PACER_EVENT_WRONG_PERIOD,
	AE_PACER_EVENT_NO_TIME_STOPS,
	AE_PACER_EVENT_NO_TIME_LATE,
	AE_PACER_EVENT_NO_TIME_START
};

struct ae_pacer_state
{
	float gaps[AE_PACER_GAPS];
	int gap_count, gap_next;
	float costs[AE_PACER_COSTS];
	int cost_count, cost_next;
	float margin_ms;
	/* when the last swap returned (ns), 0 before the first */
	unsigned long long last_return_ns;
	int skip_next, suspended, gave_up;
	/* whether "too slow" was said for the period being learnt */
	int too_slow;
	/* the display mode's refresh interval (ms, 0 not known), the periods learnt that were not it, and the last
	period that was not (for the log) */
	float display_ms;
	int wrong_periods;
	float wrong_period_ms;
	/* when the last stops and the last late frames were (ns, rings; 0 none) */
	unsigned long long stop_at[AE_PACER_GIVE_UP_STOPS];
	int stop_next;
	unsigned long long late_at[AE_PACER_GIVE_UP_LATE];
	int late_next;
	/* the start's check is done (until ae_pacer_reset), and the middle cost it found (ms, for the log) */
	int start_checked;
	float start_cost_ms;
	/* the frames that missed the blank since ae_pacer_reset */
	unsigned long late;
	int event;
};

/* everything forgotten, the late count, the giving up and the display's refresh too */
void ae_pacer_reset(struct ae_pacer_state *state);

/* the period and the cost forgotten (the margin, the late count, the giving up and its counts kept) */
void ae_pacer_forget(struct ae_pacer_state *state);

/* the display mode's refresh interval in ms (0: not known); kept until ae_pacer_reset or the next call */
void ae_pacer_display(struct ae_pacer_state *state, float display_ms);

/* a frame's cost: its CPU time and its GPU time (ms; gpu_ms below 0: not known) */
void ae_pacer_frame(struct ae_pacer_state *state, float cpu_ms, float gpu_ms);

/* a swap returned at now_ns after taking swap_ns */
void ae_pacer_swap(struct ae_pacer_state *state, unsigned long long now_ns, unsigned long long swap_ns);

/* the nanoseconds to sleep now before reading input, 0 for none */
unsigned long long ae_pacer_wait(struct ae_pacer_state *state, unsigned long long now_ns);

/* the median period in ms (0 while it is not known), the cost in ms (below 0 while none is known) */
float ae_pacer_period_ms(struct ae_pacer_state const *state);
float ae_pacer_cost_ms(struct ae_pacer_state const *state);

/* the last event, and none after it */
int ae_pacer_take_event(struct ae_pacer_state *state);

#endif
