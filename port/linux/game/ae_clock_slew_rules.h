/*
AE_CLOCK_SLEW_RULES.H

CLOCK SLEW (network.clock_slew, a client's): each machine ticks on its own wall clock, and a client that falls a little
behind the host (a hitch, a slow frame) stayed behind for the whole game, the host's time taken only when it was more
than a second ahead. A client now runs its clock a little fast until it has caught up. These pure functions are the
rules (port/linux/tests/ae_clock_slew_rules_test.c); ae_clock_slew.c feeds them the host's times.

- The error, in ticks: the host's time when it sent a message, plus half the round trip (what the host's clock reads by
  the time the message is here), less this machine's time. Positive: this machine is behind.
- The median of the last 8 samples is the error used: one late or early message moves nothing.
- Less than 3 ticks behind: no change (a tick or two is the clock's own grain and the round trip's guess).
  3 to 30 ticks behind: a factor on the clock's speed from 1 up to 1.05, in proportion to the error (5 percent at 30).
  More than 30 ticks (a second): none here; the instant jump to the host's time (network_client_manager.c) stays.
- Never below 1 (a clock is never slowed) and the time never goes back.
- A frame runs at most 8 ticks on a client of a network game (the stock game: 30). The ticks over 8, up to the stock 30,
  are carried to the next frames, 8 a frame, so a hitch is caught up within a few frames; only time past the stock 30 is
  dropped, as it was. (Dropping all past 8 left a client up to 22 ticks behind, which the slew gave back over many
  seconds, and left one 1 or 2 ticks behind for the whole game.)

The host's speed check (network_distributed.c, CLIENT_CLOCK_FAST_RATE 1.1 and CLIENT_CLOCK_AHEAD_TICKS 15) refuses a
client whose clock runs over 1.1 times the host's and is over 15 ticks ahead. A slewing client is behind and runs at
most 1.05 times the speed, so it can meet neither.
*/

#ifndef AE_CLOCK_SLEW_RULES_H
#define AE_CLOCK_SLEW_RULES_H

enum
{
	/* the host times kept: the median is of these */
	AE_CLOCK_SLEW_SAMPLES = 8,
	/* ticks behind below which nothing changes, and above which the clock jumps instead of slewing */
	AE_CLOCK_SLEW_MINIMUM_TICKS = 3,
	AE_CLOCK_SLEW_MAXIMUM_TICKS = 30,
	/* the most the clock runs fast, percent */
	AE_CLOCK_SLEW_MAXIMUM_PERCENT = 5,
	/* the most ticks a client of a network game runs in one frame (the game's own bound is 30) */
	AE_CLOCK_SLEW_BURST_TICKS = 8
};

struct ae_clock_slew_samples
{
	float values[AE_CLOCK_SLEW_SAMPLES];
	int count;
	int next;
};

void ae_clock_slew_samples_reset(struct ae_clock_slew_samples *samples);
void ae_clock_slew_samples_add(struct ae_clock_slew_samples *samples, float error);
/* the median of the samples kept (the mean of the middle two of an even count); 0 with none */
float ae_clock_slew_samples_median(struct ae_clock_slew_samples const *samples);

/* one sample's error, in ticks: host_time + round_trip / 2 - local_time (a negative round trip counts as 0) */
float ae_clock_slew_error(long host_time, float round_trip_ticks, long local_time);

/* the clock's speed factor for an error in ticks: 1 under 3 or over 30, else 1 + 0.05 * error / 30 */
float ae_clock_slew_factor(float error_ticks);

/* the ticks a frame of a network client may run: no more than the burst bound, none below 0 */
long ae_clock_slew_burst(long ticks);

/* the ticks a frame that ran ticks_run of ticks_elapsed (whole and part) carries to the next frames: those past the
ones it ran, up to stock_ticks (the stock game's bound a frame); 0 when it ran them all */
float ae_clock_slew_carry_ticks(float ticks_elapsed, long ticks_run, long stock_ticks);

#endif
