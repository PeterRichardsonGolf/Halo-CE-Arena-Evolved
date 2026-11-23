/* The revive orb's pure rules (ae_revive_orb_rules.h) */
#include "ae_revive_orb_rules.h"

#include <math.h>

void ae_revive_orb_team_rgb(int team, real rgb[3])
{
	if (team == 0)
	{
		rgb[0] = 0.92f; rgb[1] = 0.24f; rgb[2] = 0.20f;
	}
	else if (team == 1)
	{
		rgb[0] = 0.24f; rgb[1] = 0.48f; rgb[2] = 0.95f;
	}
	else
	{
		rgb[0] = 0.62f; rgb[1] = 0.86f; rgb[2] = 1.0f;
	}
}

real ae_revive_orb_pulse(long ticks)
{
	long t = ticks % AE_REVIVE_PULSE_TICKS;

	if (t < 0)
		t += AE_REVIVE_PULSE_TICKS;
	return 0.5f + 0.5f * (real)sin((double)t * 6.28318530718 / (double)AE_REVIVE_PULSE_TICKS);
}

real ae_revive_orb_alpha(real facing, int facet, long ticks, real pulse)
{
	real rim, shimmer, alpha;

	if (facing < 0.0f) facing = -facing;
	if (facing > 1.0f) facing = 1.0f;
	rim = 1.0f - facing;
	shimmer = 0.85f + 0.15f * (real)sin((double)(ticks + facet * 13) * 0.07);
	alpha = (0.12f + 0.38f * rim * rim + 0.05f * pulse) * shimmer;
	return alpha > 0.7f ? 0.7f : alpha;
}

int ae_revive_body_is_at_spot(real dx, real dy, real dz)
{
	return dx * dx + dy * dy <= AE_REVIVE_BODY_REACH * AE_REVIVE_BODY_REACH &&
		(dz < 0 ? -dz : dz) <= AE_REVIVE_BODY_REACH_UP;
}
