/*
AE_CLOCK_SLEW.H

CLOCK SLEW (network.clock_slew, ae_clock_slew_rules.h): a client's clock catches up with the host's by running up to
5 percent fast. Hooks: network_distributed.c tells it each new host time; game_time.c asks for the factor and the
burst bound.
*/

#ifndef AE_CLOCK_SLEW_H
#define AE_CLOCK_SLEW_H

#include "cseries/cseries.h"

/* a new host time (a message's tick) came, with this client's round trip in ticks; kept as a sample */
void ae_clock_slew_note_host(long host_time, real round_trip_ticks);

/* the samples dropped: the clock was set (a jump, a start) or a game began */
void ae_clock_slew_reset(void);

/* the factor on a client's clock speed: 1 when off, not a client, held, short of samples or the samples old */
real ae_clock_slew_speed_factor(void);

/* the most ticks a frame may run; the stock bound for anything but a client of a network game */
long ae_clock_slew_frame_ticks(long stock_ticks);

/* (game_time.c, after a frame's ticks) the seconds a client's frame carries to the next frames: the ticks the burst
bound held back, up to the stock bound (ae_clock_slew_carry_ticks); 0 for anything but a client of a network game */
real ae_clock_slew_carry(real ticks_elapsed, long ticks_run, long stock_ticks, real ticks_per_second);

/* the median error now, in ticks (0 with no samples), for the log and the tests' reports */
real ae_clock_slew_error_ticks(void);

#endif
