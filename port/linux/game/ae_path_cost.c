/*
AE_PATH_COST.C

A vehicle's route prefers open ground (ae_path_cost.h). While a driver's
route is searched, every edge's traversal cost grows as the edge narrows,
and a small noise, different for every edge and every driver (a hash of
the driver's seed and the two surfaces the edge joins), spreads two drivers
over different lines. The noise only ever adds (0..AE_PATH_NOISE_MOST of an
edge's cost), so no edge costs less than its straight length and the
search's distance estimate stays a lower bound (admissible); the same
seed over the same edges always gives the same route.
Armed by bots_search around path_state_find for a vehicle-width search;
a search on foot pays nothing.
*/

#include "ae_path_cost.h"
#include "ae_bots_vehicle_rules.h"

/* edges at least this long (world units) cost nothing extra; the penalty
grows linearly to AE_PATH_NARROW_MOST at zero width */
#define AE_PATH_WIDE_EDGE 4.0f
#define AE_PATH_NARROW_MOST 0.5f
/* the per-driver spread, at most this share of an edge's cost */
#define AE_PATH_NOISE_MOST 0.15f
/* a search on foot: radii at or under this arm nothing */
#define AE_PATH_FOOT_RADIUS 0.3f

static struct
{
	int active;
	/* a bots' search of any radius is running (bots_search's AE hook): the foot penalties are its, never the game's
	other searches' */
	int armed;
	/* the learnt stuck spots of this map (ae_bots_learnt.h): pieces and their penalty levels, for searches on foot */
	int stuck_count;
	long stuck_surface[AE_PATH_STUCK_PIECES];
	int stuck_levels[AE_PATH_STUCK_PIECES];
	float half_width; /* the armed search's radius */
	unsigned long seed; /* the driver's, spread; fixed for the whole search */
	/* the learnt wedges of this map: pieces and how many times; used unless switched off (the classic driving) */
	int wedges_off;
	int wedge_count;
	long wedge_surface[AE_PATH_WEDGE_PIECES];
	int wedge_times[AE_PATH_WEDGE_PIECES];
	/* the scenery obstacles of this map (circles); used unless switched off */
	int obstacles_off;
	int obstacle_count;
	float obstacle_x[AE_BOTS_OBSTACLE_MAXIMUM], obstacle_y[AE_BOTS_OBSTACLE_MAXIMUM], obstacle_radius[AE_BOTS_OBSTACLE_MAXIMUM];
	/* each circle's height span (a keepout's is every height) and its object (-1, NONE: none; an object's index is a
	salted handle, often negative as a long) */
	float obstacle_zlo[AE_BOTS_OBSTACLE_MAXIMUM], obstacle_zhi[AE_BOTS_OBSTACLE_MAXIMUM];
	long obstacle_object[AE_BOTS_OBSTACLE_MAXIMUM];
} ae_path_cost;

void ae_path_cost_stucks_clear(void)
{
	ae_path_cost.stuck_count = 0;
}

void ae_path_cost_stuck(long surface, int levels)
{
	int index;

	if (surface < 0 || levels <= 0)
		return;
	for (index = 0; index < ae_path_cost.stuck_count; index++)
	{
		if (ae_path_cost.stuck_surface[index] == surface)
		{
			ae_path_cost.stuck_levels[index] = levels;
			return;
		}
	}
	if (ae_path_cost.stuck_count >= AE_PATH_STUCK_PIECES)
		return;
	ae_path_cost.stuck_surface[ae_path_cost.stuck_count] = surface;
	ae_path_cost.stuck_levels[ae_path_cost.stuck_count] = levels;
	ae_path_cost.stuck_count++;
}

int ae_path_cost_stuck_levels(long surface)
{
	int index;

	for (index = 0; index < ae_path_cost.stuck_count; index++)
	{
		if (ae_path_cost.stuck_surface[index] == surface)
			return ae_path_cost.stuck_levels[index];
	}
	return 0;
}

void ae_path_cost_obstacles_clear(void)
{
	ae_path_cost.obstacle_count = 0;
}

void ae_path_cost_obstacle_add(float x, float y, float z, float radius)
{
	(void)z; /* (a keepout blocks every height) */
	ae_path_cost_obstacle_add_span(x, y, -1e30f, 1e30f, radius, -1);
}

void ae_path_cost_obstacle_add_span(float x, float y, float zlo, float zhi, float radius, long object_index)
{
	int index = ae_path_cost.obstacle_count;

	if (index >= AE_BOTS_OBSTACLE_MAXIMUM)
		return;
	ae_path_cost.obstacle_x[index] = x;
	ae_path_cost.obstacle_y[index] = y;
	ae_path_cost.obstacle_radius[index] = radius;
	ae_path_cost.obstacle_zlo[index] = zlo;
	ae_path_cost.obstacle_zhi[index] = zhi;
	ae_path_cost.obstacle_object[index] = object_index;
	ae_path_cost.obstacle_count++;
}

int ae_path_cost_obstacle_set_radius(int index, float radius)
{
	if (index < 0 || index >= ae_path_cost.obstacle_count || !(radius > 0.0f))
		return 0;
	ae_path_cost.obstacle_radius[index] = radius;
	return 1;
}

int ae_path_cost_obstacle_span(int index, float *zlo, float *zhi, long *object_index)
{
	if (index < 0 || index >= ae_path_cost.obstacle_count)
		return 0;
	*zlo = ae_path_cost.obstacle_zlo[index];
	*zhi = ae_path_cost.obstacle_zhi[index];
	*object_index = ae_path_cost.obstacle_object[index];
	return 1;
}

void ae_path_cost_obstacles_on(int on)
{
	ae_path_cost.obstacles_off = on == 0;
}

int ae_path_cost_obstacles_are_on(void)
{
	return !ae_path_cost.obstacles_off;
}

int ae_path_cost_obstacle_count(void)
{
	return ae_path_cost.obstacle_count;
}

int ae_path_cost_obstacle_get(int index, float *x, float *y, float *radius)
{
	if (index < 0 || index >= ae_path_cost.obstacle_count)
		return 0;
	*x = ae_path_cost.obstacle_x[index];
	*y = ae_path_cost.obstacle_y[index];
	*radius = ae_path_cost.obstacle_radius[index];
	return 1;
}

float ae_path_cost_obstacles_edge(float cost, float ax, float ay, float bx, float by)
{
	int index, foot = !ae_path_cost.active;
	/* (a person's search: its own radius and no margin, and only scenery circles) */
	float half_width = foot ? AE_PATH_FOOT_RADIUS : ae_path_cost.half_width, margin = foot ? 0.0f : AE_BOTS_OBSTACLE_MARGIN;

	if ((foot && !ae_path_cost.armed) || ae_path_cost.obstacles_off || ae_path_cost.obstacle_count == 0 || cost <= 0.0f)
		return cost;
	for (index = 0; index < ae_path_cost.obstacle_count; index++)
	{
		/* (a bounding box first: most edges are nowhere near an obstacle; the height is ignored throughout) */
		float reach = ae_path_cost.obstacle_radius[index] + half_width + margin;
		float x = ae_path_cost.obstacle_x[index], y = ae_path_cost.obstacle_y[index];

		if (foot && ae_path_cost.obstacle_object[index] == -1)
			continue;
		if ((ax < x - reach && bx < x - reach) || (ax > x + reach && bx > x + reach) ||
			(ay < y - reach && by < y - reach) || (ay > y + reach && by > y + reach))
			continue;
		if (ae_bots_obstacle_piece_blocked(ax, ay, bx, by, ae_path_cost.obstacle_x[index], ae_path_cost.obstacle_y[index],
			ae_path_cost.obstacle_radius[index], half_width, margin))
			return cost * (1.0f + (foot ? AE_PATH_FOOT_OBSTACLE_PENALTY : AE_PATH_OBSTACLE_PENALTY));
	}
	return cost;
}

void ae_path_cost_wedge(long surface)
{
	int index, least = 0;

	if (surface < 0)
		return;
	for (index = 0; index < ae_path_cost.wedge_count; index++)
	{
		if (ae_path_cost.wedge_surface[index] == surface)
		{
			if (ae_path_cost.wedge_times[index] < 1000)
				ae_path_cost.wedge_times[index]++;
			return;
		}
		if (ae_path_cost.wedge_times[index] < ae_path_cost.wedge_times[least])
			least = index;
	}
	/* (a full list: the least wedged piece gives way) */
	index = ae_path_cost.wedge_count < AE_PATH_WEDGE_PIECES ? ae_path_cost.wedge_count++ : least;
	ae_path_cost.wedge_surface[index] = surface;
	ae_path_cost.wedge_times[index] = 1;
}

void ae_path_cost_wedges_clear(void)
{
	ae_path_cost.wedge_count = 0;
}

void ae_path_cost_wedges_on(int on)
{
	ae_path_cost.wedges_off = on == 0;
}

int ae_path_cost_wedges(long surface)
{
	int index;

	for (index = 0; index < ae_path_cost.wedge_count; index++)
	{
		if (ae_path_cost.wedge_surface[index] == surface)
			return ae_path_cost.wedge_times[index];
	}
	return 0;
}

/* 0..1, the same for the same driver seed and the same two surfaces (the
edge a->b is not b->a: the hash is ordered) */
float ae_path_cost_noise(unsigned long seed, long from_surface, long to_surface)
{
	unsigned long h = (seed ^ 0x9e3779b9ul) & 0xFFFFFFFFul;

	h = (h ^ ((unsigned long)from_surface * 2654435761ul)) & 0xFFFFFFFFul;
	h = (h << 13 | h >> 19) & 0xFFFFFFFFul;
	h = (h ^ ((unsigned long)to_surface * 2246822519ul)) & 0xFFFFFFFFul;
	h = (h * 3266489917ul + 374761393ul) & 0xFFFFFFFFul;
	h ^= h >> 15;
	h = (h * 2246822519ul) & 0xFFFFFFFFul;
	h ^= h >> 13;
	return (float)((h >> 6) & 0x3FF) / 1023.0f;
}

void ae_path_cost_begin(float radius, unsigned long seed)
{
	ae_path_cost.active = radius > AE_PATH_FOOT_RADIUS;
	ae_path_cost.armed = 1;
	ae_path_cost.half_width = radius;
	/* (the same driver keeps the same taste for a route until its seed
	changes) */
	ae_path_cost.seed = (seed * 2654435761ul + 97ul) & 0xFFFFFFFFul;
}

void ae_path_cost_end(void)
{
	ae_path_cost.active = 0;
	ae_path_cost.armed = 0;
}

float ae_path_cost_edge(float cost, float edge_length_squared, long from_surface, long to_surface)
{
	float length, narrow;

	if (cost <= 0.0f)
		return cost;
	/* (a bot on foot: the learnt stuck spots' pieces cost more to walk onto) */
	if (!ae_path_cost.active)
	{
		int levels = ae_path_cost.armed && ae_path_cost.stuck_count > 0 ? ae_path_cost_stuck_levels(to_surface) : 0;

		return levels > 0 ? cost * (1.0f + AE_PATH_STUCK_PENALTY * (float)levels) : cost;
	}
	length = edge_length_squared >= AE_PATH_WIDE_EDGE * AE_PATH_WIDE_EDGE ? AE_PATH_WIDE_EDGE :
		edge_length_squared <= 0.0f ? 0.0f : __builtin_sqrtf(edge_length_squared);
	narrow = (AE_PATH_WIDE_EDGE - length) / AE_PATH_WIDE_EDGE;
	{
		int wedges = !ae_path_cost.wedges_off && ae_path_cost.wedge_count > 0 ? ae_path_cost_wedges(to_surface) : 0;
		float avoid = wedges >= AE_PATH_WEDGES_TO_AVOID ?
			AE_PATH_WEDGE_PENALTY * (float)(wedges < AE_PATH_WEDGE_MOST ? wedges : AE_PATH_WEDGE_MOST) : 0.0f;

		return cost * (1.0f + AE_PATH_NARROW_MOST * narrow + avoid +
			AE_PATH_NOISE_MOST * ae_path_cost_noise(ae_path_cost.seed, from_surface, to_surface));
	}
}
