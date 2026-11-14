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
	unsigned long seed; /* the driver's, spread; fixed for the whole search */
	/* the learnt wedges of this map: pieces and how many times; used unless switched off (the classic driving) */
	int wedges_off;
	int wedge_count;
	long wedge_surface[AE_PATH_WEDGE_PIECES];
	int wedge_times[AE_PATH_WEDGE_PIECES];
} ae_path_cost;

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
	/* (the same driver keeps the same taste for a route until its seed
	changes) */
	ae_path_cost.seed = (seed * 2654435761ul + 97ul) & 0xFFFFFFFFul;
}

void ae_path_cost_end(void)
{
	ae_path_cost.active = 0;
}

float ae_path_cost_edge(float cost, float edge_length_squared, long from_surface, long to_surface)
{
	float length, narrow;

	if (!ae_path_cost.active || cost <= 0.0f)
		return cost;
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
