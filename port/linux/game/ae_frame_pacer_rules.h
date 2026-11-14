/*
AE_FRAME_PACER_RULES.H

LOW LATENCY (display.low_latency "on" and "boost"): with V-SYNC on, the game sleeps before it reads input so that the
frame finishes just before the next blank, not a whole refresh before it. Pure rules
(port/linux/tests/ae_frame_pacer_rules_test.c); ae_frame_pacer.c feeds them each frame's times.

- The period is the median of the last AE_PACER_GAPS gaps between swaps returning (measured: a variable refresh
  rate, 144 Hz and a display mode the game did not ask for are all the same to it). Nothing is waited for until
  AE_PACER_LEARN gaps are known.
- The cost is the largest of the last AE_PACER_COSTS frames' time, from the frame's start (after the wait) to the
  graphics driver having it all: the slower of the CPU's work and the GPU's (AE_PACER_GPU_UNKNOWN_MS more than the
  CPU's when the GPU's is not known).
- The wait ends at the next blank (the last swap's return and one period) less the cost and the margin. The margin
  starts at AE_PACER_MARGIN_MS, grows AE_PACER_MARGIN_GROW_MS after each late frame (up to AE_PACER_MARGIN_MAX_MS) and
  shrinks AE_PACER_MARGIN_SHRINK_MS a frame.
- No wait while cost + margin is over AE_PACER_SKIP_PERCENT of the period (no wait would help); waiting again from
  AE_PACER_RESUME_PERCENT.
- A late frame is one whose swap returned more than AE_PACER_LATE_PERIODS periods after the last: the blank was
  missed. It counts, grows the margin, and the next frame is not waited for. AE_PACER_RELEARN_LATE late frames in a
  row (the refresh is slower now: another display, a lower rate) forget the period: it is learnt again.
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
#define AE_PACER_RESUME_PERCENT 80.0f
#define AE_PACER_LATE_PERIODS 1.5f
#define AE_PACER_PAUSE_PERIODS 20.0f
#define AE_PACER_PERIOD_MIN_MS 2.0f
#define AE_PACER_PERIOD_MAX_MS 50.0f
#define AE_PACER_HITCH_MS 250.0f
#define AE_PACER_RELEARN_LATE 30

/* what happened, for the log: ae_pacer_take_event */
enum
{
	AE_PACER_EVENT_NONE,
	AE_PACER_EVENT_LATE,
	AE_PACER_EVENT_STALL,
	AE_PACER_EVENT_NOT_A_DISPLAY,
	AE_PACER_EVENT_TOO_SLOW,
	AE_PACER_EVENT_OVER_BUDGET,
	AE_PACER_EVENT_FAST_AGAIN
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
	/* the late frames in a row, and whether "too slow" was said for the period being learnt */
	int late_run, too_slow;
	/* the frames that missed the blank since ae_pacer_reset */
	unsigned long late;
	int event;
};

/* everything forgotten, the late count and the giving up too */
void ae_pacer_reset(struct ae_pacer_state *state);

/* the period and the cost forgotten (the margin, the late count and the giving up kept) */
void ae_pacer_forget(struct ae_pacer_state *state);

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
