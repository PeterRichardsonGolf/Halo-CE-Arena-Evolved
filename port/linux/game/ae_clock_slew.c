/*
AE_CLOCK_SLEW.C

CLOCK SLEW (network.clock_slew, default on; ae_clock_slew_rules.h). Each new host time a client's netcode has is a
sample of how far this machine's clock is behind the host's: network_distributed.c's header time, with the round trip
it already measures from the echo of the client's own predictions (distributed_own_round_trip_ticks). Nothing is added
to the wire. game_time_update multiplies the client's clock speed by the factor the median gives.

No sample older than a second counts: a host gone silent leaves the clock alone.
*/

#include "cseries/cseries.h"
#include "cseries/cseries_windows.h"
#include "cseries/errors.h"
#include "game/game.h"

#include "ae_clock_slew_rules.h"
#include "ae_clock_slew.h"

int config_boolean(char const *name);
unsigned long config_changes(void);

/* the samples older than this are not used */
#define AE_CLOCK_SLEW_STALE_MILLISECONDS 1000UL

static struct ae_clock_slew_samples ae_clock_slew_samples;
static unsigned long ae_clock_slew_last_sample;
static boolean ae_clock_slew_have_sample;
/* (the log, which the TEST RUN's expect lines read: behind since when, and whether the last frame carried ticks) */
static boolean ae_clock_slew_behind;
static unsigned long ae_clock_slew_behind_since;
static boolean ae_clock_slew_carrying;

/* network.clock_slew, read again when a setting changes */
static boolean ae_clock_slew_enabled(void)
{
	static unsigned long read_at = (unsigned long)-1;
	static boolean enabled;

	if (read_at != config_changes())
	{
		read_at = config_changes();
		enabled = config_boolean("network.clock_slew") != 0;
	}
	return enabled;
}

void ae_clock_slew_reset(void)
{
	ae_clock_slew_samples_reset(&ae_clock_slew_samples);
	ae_clock_slew_have_sample = FALSE;
	ae_clock_slew_behind = FALSE;
	ae_clock_slew_carrying = FALSE;
}

void ae_clock_slew_note_host(long host_time, real round_trip_ticks)
{
	if (game_connection() != _game_connection_network_client || game_time_held())
		return;
	ae_clock_slew_samples_add(&ae_clock_slew_samples,
		ae_clock_slew_error(host_time, (float)round_trip_ticks, game_time_get()));
	ae_clock_slew_last_sample = system_milliseconds();
	ae_clock_slew_have_sample = TRUE;
	/* (a client's time against the host's, said when it falls behind and when it has caught up) */
	if (ae_clock_slew_samples.count >= AE_CLOCK_SLEW_SAMPLES)
	{
		float median = ae_clock_slew_samples_median(&ae_clock_slew_samples);

		if (!ae_clock_slew_behind && median >= (float)AE_CLOCK_SLEW_MINIMUM_TICKS)
		{
			ae_clock_slew_behind = TRUE;
			ae_clock_slew_behind_since = ae_clock_slew_last_sample;
			error(_error_silent, "clock slew: %.0f ticks behind the host", (double)median);
		}
		else if (ae_clock_slew_behind && median < (float)AE_CLOCK_SLEW_MINIMUM_TICKS)
		{
			ae_clock_slew_behind = FALSE;
			error(_error_silent, "clock slew: caught up in %lu ms",
				ae_clock_slew_last_sample - ae_clock_slew_behind_since);
		}
	}
}

real ae_clock_slew_error_ticks(void)
{
	return (real)ae_clock_slew_samples_median(&ae_clock_slew_samples);
}

real ae_clock_slew_speed_factor(void)
{
	if (!ae_clock_slew_enabled() || game_connection() != _game_connection_network_client || game_time_held())
		return 1.0f;
	if (!ae_clock_slew_have_sample || ae_clock_slew_samples.count < AE_CLOCK_SLEW_SAMPLES ||
		system_milliseconds() - ae_clock_slew_last_sample > AE_CLOCK_SLEW_STALE_MILLISECONDS)
	{
		return 1.0f;
	}
	return (real)ae_clock_slew_factor(ae_clock_slew_samples_median(&ae_clock_slew_samples));
}

long ae_clock_slew_frame_ticks(long stock_ticks)
{
	if (game_connection() != _game_connection_network_client)
		return stock_ticks;
	return MIN(ae_clock_slew_burst(stock_ticks), stock_ticks);
}

real ae_clock_slew_carry(real ticks_elapsed, long ticks_run, long stock_ticks, real ticks_per_second)
{
	float carried;

	if (game_connection() != _game_connection_network_client || !(ticks_per_second > 0.0f))
		return 0.0f;
	carried = ae_clock_slew_carry_ticks(ticks_elapsed, ticks_run, stock_ticks);
	/* (said once a hitch: the first frame that carries) */
	if (carried > 0.0f && !ae_clock_slew_carrying)
		error(_error_silent, "clock slew: a frame ran %ld of %.0f ticks, %.0f carried to the next frames", ticks_run,
			(double)ticks_elapsed, (double)carried);
	ae_clock_slew_carrying = carried > 0.0f;
	return carried / ticks_per_second;
}
