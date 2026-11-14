/* The round modes' danger zone (see the header) */
#include <math.h>

#include "ae_round_zone_rules.h"

static int clamp_seconds(int seconds)
{
	if (seconds < 1)
		return 1;
	return seconds > AE_ZONE_LONGEST_PHASE_SECONDS ? AE_ZONE_LONGEST_PHASE_SECONDS : seconds;
}

/* the seconds a closing of distance takes: at least the setting's, its edge at most half a walking speed */
static int closing_seconds_for(real distance, int least)
{
	real fastest = AE_ZONE_WALK_SPEED * 0.5f;
	int needed = distance > 0.0f ? (int)ceil(distance / fastest) : 0;

	return clamp_seconds(needed > least ? needed : least);
}

void ae_zone_plan_make(struct ae_zone_plan *plan, real start_radius, int wait_seconds, int closing_seconds)
{
	if (!(start_radius > 0.0f))
		start_radius = AE_ZONE_LEAST_RADIUS;
	plan->start_radius = start_radius;
	plan->mid_radius = start_radius * AE_ZONE_MID_FRACTION;
	plan->wait_seconds = clamp_seconds(wait_seconds);
	plan->wait_2_seconds = clamp_seconds(AE_ZONE_WAIT_2_SECONDS);
	plan->closing_seconds = closing_seconds_for(start_radius - plan->mid_radius, closing_seconds);
	plan->closing_2_seconds = closing_seconds_for(plan->mid_radius, closing_seconds);
}

/* the phases' ends, in ticks from the start */
static void ends(struct ae_zone_plan const *plan, long *end)
{
	end[0] = (long)plan->wait_seconds * AE_ZONE_TICKS_PER_SECOND;
	end[1] = end[0] + (long)plan->closing_seconds * AE_ZONE_TICKS_PER_SECOND;
	end[2] = end[1] + (long)plan->wait_2_seconds * AE_ZONE_TICKS_PER_SECOND;
	end[3] = end[2] + (long)plan->closing_2_seconds * AE_ZONE_TICKS_PER_SECOND;
}

int ae_zone_phase_at(struct ae_zone_plan const *plan, long elapsed)
{
	long end[4];
	int phase;

	ends(plan, end);
	for (phase = 0; phase < 4; phase++)
	{
		if (elapsed < end[phase])
			return phase;
	}
	return AE_ZONE_PHASE_CLOSED;
}

real ae_zone_radius_at(struct ae_zone_plan const *plan, long elapsed)
{
	long end[4];

	ends(plan, end);
	if (elapsed < end[0])
		return plan->start_radius;
	if (elapsed < end[1])
		return plan->start_radius - (plan->start_radius - plan->mid_radius) * (real)(elapsed - end[0]) /
			(real)(end[1] - end[0]);
	if (elapsed < end[2])
		return plan->mid_radius;
	if (elapsed < end[3])
		return plan->mid_radius * (1.0f - (real)(elapsed - end[2]) / (real)(end[3] - end[2]));
	return 0.0f;
}

long ae_zone_phase_ticks_left(struct ae_zone_plan const *plan, long elapsed)
{
	long end[4];
	int phase = ae_zone_phase_at(plan, elapsed);

	ends(plan, end);
	return phase < AE_ZONE_PHASE_CLOSED ? end[phase] - (elapsed < 0 ? 0 : elapsed) : 0;
}

real ae_zone_next_radius(struct ae_zone_plan const *plan, long elapsed)
{
	switch (ae_zone_phase_at(plan, elapsed))
	{
	case AE_ZONE_PHASE_WAIT:
	case AE_ZONE_PHASE_CLOSING:
		return plan->mid_radius;
	default:
		return 0.0f;
	}
}

int ae_zone_inside(real centre_x, real centre_y, real radius, real x, real y)
{
	real dx = x - centre_x, dy = y - centre_y;

	return dx * dx + dy * dy <= radius * radius;
}

int ae_zone_damage(real maximum_shield, real maximum_body, real *shield, real *body)
{
	real points, shield_points;

	if (maximum_shield < 0.0f)
		maximum_shield = 0.0f;
	if (maximum_body <= 0.0f)
	{
		*body = 0.0f;
		return 1;
	}
	points = (maximum_shield + maximum_body) * AE_ZONE_DAMAGE_SHARE;
	shield_points = *shield > 0.0f ? *shield * maximum_shield : 0.0f;
	if (shield_points >= points && maximum_shield > 0.0f)
	{
		*shield -= points / maximum_shield;
		return 0;
	}
	points -= shield_points;
	*shield = 0.0f;
	*body -= points / maximum_body;
	if (*body <= 0.0005f)
	{
		*body = 0.0f;
		return 1;
	}
	return 0;
}

int ae_zone_fair_hill(real hill_x, real hill_y, real red_x, real red_y, real blue_x, real blue_y)
{
	real to_red = (real)sqrt((hill_x - red_x) * (hill_x - red_x) + (hill_y - red_y) * (hill_y - red_y));
	real to_blue = (real)sqrt((hill_x - blue_x) * (hill_x - blue_x) + (hill_y - blue_y) * (hill_y - blue_y));
	real difference = to_red > to_blue ? to_red - to_blue : to_blue - to_red;

	return difference <= AE_ZONE_FAIR_SHARE * (to_red + to_blue);
}

real ae_zone_start_radius(real centre_x, real centre_y, real const (*points)[2], int count)
{
	real farthest = 0.0f;
	int index;

	for (index = 0; index < count; index++)
	{
		real dx = points[index][0] - centre_x, dy = points[index][1] - centre_y;
		real distance = (real)sqrt(dx * dx + dy * dy);

		if (distance > farthest)
			farthest = distance;
	}
	farthest *= AE_ZONE_START_MARGIN;
	return farthest < AE_ZONE_LEAST_RADIUS ? AE_ZONE_LEAST_RADIUS : farthest;
}

void ae_zone_point_toward(real centre_x, real centre_y, real x, real y, real distance, real *out_x, real *out_y)
{
	real dx = x - centre_x, dy = y - centre_y;
	real length = (real)sqrt(dx * dx + dy * dy);

	if (length < 0.001f)
	{
		*out_x = centre_x;
		*out_y = centre_y;
		return;
	}
	*out_x = centre_x + dx / length * distance;
	*out_y = centre_y + dy / length * distance;
}

int ae_zone_leashes(int phase, real edge_distance, real radius)
{
	int closing = phase == AE_ZONE_PHASE_CLOSING || phase == AE_ZONE_PHASE_CLOSING_2;
	/* (a ring smaller than twice the margin has no ground a bot may fight on: the margin is half the radius at most) */
	real margin = radius * 0.5f < AE_ZONE_LEASH_MARGIN ? radius * 0.5f : AE_ZONE_LEASH_MARGIN;

	return edge_distance < (closing ? margin : 0.0f);
}

long ae_zone_round_seconds_left(long total_seconds, long limit_seconds, long elapsed_ticks)
{
	long length = total_seconds, remaining;

	if (limit_seconds > 0 && limit_seconds < length)
		length = limit_seconds;
	remaining = length * AE_ZONE_TICKS_PER_SECOND - elapsed_ticks;
	return remaining <= 0 ? 0 : (remaining + AE_ZONE_TICKS_PER_SECOND - 1) / AE_ZONE_TICKS_PER_SECOND;
}

real ae_zone_reveal(struct ae_zone_plan const *plan, int running, long elapsed)
{
	long closing_start = (long)plan->wait_seconds * AE_ZONE_TICKS_PER_SECOND;

	if (!running || elapsed < closing_start)
		return 0.0f;
	if (elapsed >= closing_start + AE_ZONE_REVEAL_TICKS)
		return 1.0f;
	return (real)(elapsed - closing_start) / (real)AE_ZONE_REVEAL_TICKS;
}

int ae_zone_clock(struct ae_zone_plan const *plan, long elapsed, long *seconds)
{
	int phase = ae_zone_phase_at(plan, elapsed);
	long left = ae_zone_phase_ticks_left(plan, elapsed);

	*seconds = (left + AE_ZONE_TICKS_PER_SECOND - 1) / AE_ZONE_TICKS_PER_SECOND;
	if (phase == AE_ZONE_PHASE_CLOSING)
		return AE_ZONE_CLOCK_CLOSING;
	if (phase == AE_ZONE_PHASE_CLOSING_2 || phase == AE_ZONE_PHASE_CLOSED)
		return AE_ZONE_CLOCK_FINAL;
	return AE_ZONE_CLOCK_COUNT;
}

int ae_zone_warning(struct ae_zone_plan const *plan, long elapsed)
{
	int phase = ae_zone_phase_at(plan, elapsed);
	long left = ae_zone_phase_ticks_left(plan, elapsed);

	if (phase == AE_ZONE_PHASE_CLOSING)
		return 3;
	if (phase == AE_ZONE_PHASE_CLOSING_2)
		return 4;
	if (phase != AE_ZONE_PHASE_WAIT && phase != AE_ZONE_PHASE_WAIT_2)
		return 0;
	if (left <= 10L * AE_ZONE_TICKS_PER_SECOND)
		return 2;
	if (left <= 30L * AE_ZONE_TICKS_PER_SECOND && (phase == AE_ZONE_PHASE_WAIT_2 ? plan->wait_2_seconds : plan->wait_seconds) >= 35)
		return 1;
	return 0;
}

real ae_zone_edge_distance(real centre_x, real centre_y, real radius, real x, real y)
{
	real dx = x - centre_x, dy = y - centre_y;

	return radius - (real)sqrt(dx * dx + dy * dy);
}

static real smooth(real t)
{
	t = t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t;
	return t * t * (3.0f - 2.0f * t);
}

real ae_zone_tint_alpha(real across, real down)
{
	/* the nearest frame edge's depth: 0 at the edge, 0.5 in the middle; clear from the middle's AE_ZONE_TINT_CLEAR on */
	real edge_x = across < 0.5f ? across : 1.0f - across;
	real edge_y = down < 0.5f ? down : 1.0f - down;
	real band = (1.0f - AE_ZONE_TINT_CLEAR) * 0.5f;
	real depth_x = edge_x / band, depth_y = edge_y / band;
	real depth = depth_x < depth_y ? depth_x : depth_y;

	return AE_ZONE_TINT_ALPHA * (1.0f - smooth(depth));
}

real ae_zone_pulse_alpha(long ticks_after)
{
	if (ticks_after < 0 || ticks_after >= AE_ZONE_PULSE_TICKS)
		return 0.0f;
	return AE_ZONE_PULSE_ALPHA * (1.0f - (real)ticks_after / (real)AE_ZONE_PULSE_TICKS);
}

real ae_zone_wall_height(int look)
{
	return look == 'a' ? AE_ZONE_WALL_HEIGHT : AE_ZONE_CURTAIN_HEIGHT;
}

/* a cheap smooth value noise of two numbers, 0..1 */
static real hash2(long a, long b)
{
	unsigned long h = (unsigned long)a * 374761393UL + (unsigned long)b * 668265263UL;

	h = (h ^ (h >> 13)) * 1274126177UL;
	h ^= h >> 16;
	return (real)(h & 0xFFFFUL) / 65535.0f;
}

static real noise(real x, real y)
{
	long ix = (long)floor(x), iy = (long)floor(y);
	real fx = smooth(x - (real)ix), fy = smooth(y - (real)iy);
	real a = hash2(ix, iy), b = hash2(ix + 1, iy), c = hash2(ix, iy + 1), d = hash2(ix + 1, iy + 1);

	return (a + (b - a) * fx) + ((c + (d - c) * fx) - (a + (b - a) * fx)) * fy;
}

void ae_zone_wall_sample(int look, real height, real arc, long ticks, real *alpha, real *ramp)
{
	real top = ae_zone_wall_height(look);
	real t = height / top;
	real seconds = (real)ticks / (real)AE_ZONE_TICKS_PER_SECOND;

	if (t < 0.0f) t = 0.0f;
	if (t > 1.0f) t = 1.0f;
	if (look == 'a')
	{
		/* the energy wall: a mottled band that drifts up, ragged at the top (the fringe's height varies along the
		ring), a hot foot line */
		real mottle = noise(arc * 0.35f, height * 0.5f - seconds * 0.9f) * 0.6f + noise(arc * 1.1f + 40.0f,
			height * 1.3f - seconds * 1.6f) * 0.4f;
		real ragged = 0.70f + 0.30f * noise(arc * 0.6f, seconds * 0.5f);
		real fade = t / ragged;
		real body = fade >= 1.0f ? 0.0f : 0.66f * (1.0f - smooth(fade));

		*alpha = body * (0.62f + 0.62f * mottle);
		*ramp = 0.25f + 0.45f * mottle * (1.0f - t);
		if (height < 0.35f)
		{
			*alpha = *alpha > 0.62f ? *alpha : 0.62f;
			*ramp = 1.0f;
		}
	}
	else
	{
		/* the light curtain: a light foot fading up, ribs every AE_ZONE_RIB_SPACING along the arc that drift up */
		real across = arc / AE_ZONE_RIB_SPACING;
		real into = across - (real)floor(across);
		real rib = 1.0f - smooth(fabsf(into - 0.5f) * 6.0f);
		real rise = t * top * 0.35f - seconds * 0.8f;
		real flow = 0.5f + 0.5f * (real)sin(rise * 3.0f);
		real foot = (real)pow((double)(1.0f - t), 1.4);

		*alpha = 0.60f * foot + 0.10f * (1.0f - t) + rib * (0.40f * (1.0f - t) + 0.12f * flow * (1.0f - t));
		*ramp = 0.30f + 0.55f * foot + 0.2f * rib;
		if (height < 0.12f)
		{
			*alpha = *alpha > 0.75f ? *alpha : 0.75f;
			*ramp = 1.0f;
		}
	}
	if (*alpha > 1.0f) *alpha = 1.0f;
	if (*ramp > 1.0f) *ramp = 1.0f;
}
