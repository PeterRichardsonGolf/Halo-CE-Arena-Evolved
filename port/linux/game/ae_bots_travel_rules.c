/*
AE_BOTS_TRAVEL_RULES.C

The bots' getting around as pure rules (ae_bots_travel_rules.h): no engine includes.
*/

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ae_bots_travel_rules.h"

real ae_bots_travel_walk(boolean found, real way_length, boolean reached, real remainder_straight, real straight)
{
	if (!found)
		return straight * AE_BOTS_TRAVEL_NO_WAY_STRETCH + AE_BOTS_TRAVEL_NO_WAY_EXTRA;
	if (reached)
		return way_length;
	/* (as near as the search came: the rest is no walk it knows, a ledge or a floor the walkable surfaces miss) */
	return way_length + remainder_straight * AE_BOTS_TRAVEL_NO_WAY_STRETCH + AE_BOTS_TRAVEL_NO_WAY_EXTRA * 0.5f;
}

real ae_bots_travel_hop_cost(boolean jump, real hop_length)
{
	if (!jump)
		return AE_BOTS_TRAVEL_TELEPORT_COST;
	return hop_length * AE_BOTS_TRAVEL_JUMP_STRETCH + AE_BOTS_TRAVEL_JUMP_COST;
}

real ae_bots_travel_link_estimate(real to_entry_straight, real hop_cost, real exit_to_goal_straight)
{
	return to_entry_straight * AE_BOTS_TRAVEL_WALK_STRETCH + hop_cost + exit_to_goal_straight * AE_BOTS_TRAVEL_WALK_STRETCH;
}

boolean ae_bots_travel_link_worth(real estimate, real walk)
{
	return (boolean)(estimate < walk * AE_BOTS_TRAVEL_TRY_SHARE - AE_BOTS_TRAVEL_TRY_MARGIN);
}

boolean ae_bots_travel_link_taken(real entry_walk, real hop_cost, real exit_to_goal_straight, real walk)
{
	real cost = entry_walk + hop_cost + exit_to_goal_straight * AE_BOTS_TRAVEL_WALK_STRETCH;

	return (boolean)(cost < walk * AE_BOTS_TRAVEL_TAKE_SHARE - AE_BOTS_TRAVEL_TAKE_MARGIN);
}

boolean ae_bots_travel_link_allowed(real entry_x, real entry_y, real entry_z, boolean arrived, real arrival_x,
	real arrival_y, real arrival_z, long ticks_since_arrival, boolean same_link)
{
	real dx = entry_x - arrival_x, dy = entry_y - arrival_y, dz = entry_z - arrival_z;

	if (!arrived || ticks_since_arrival < 0)
		return TRUE;
	if (same_link && ticks_since_arrival < AE_BOTS_TRAVEL_AGAIN_TICKS)
		return FALSE;
	if (ticks_since_arrival < AE_BOTS_TRAVEL_RETURN_TICKS &&
		dx * dx + dy * dy + dz * dz <= AE_BOTS_TRAVEL_RETURN_RADIUS * AE_BOTS_TRAVEL_RETURN_RADIUS)
	{
		return FALSE;
	}
	return TRUE;
}

real ae_bots_travel_pack_need(int health_style, real health)
{
	real top;

	if (health <= 0.0f)
		return 0.0f;
	if (health > 1.0f)
		health = 1.0f;
	switch (health_style)
	{
	case AE_BOTS_TRAVEL_HEALTH_CLASSIC:
		return 1.0f - health;
	case AE_BOTS_TRAVEL_HEALTH_REACH:
		/* (the top of the third it is in, as damage.c's REACH comes back to; on a third's top, that third) */
		top = (real)((long)(health * 3.0f - 0.0001f) + 1) / 3.0f;
		if (top > 1.0f)
			top = 1.0f;
		return 1.0f - top;
	default:
		return 0.0f;
	}
}

real ae_bots_travel_pack_score(real need, real distance, real height, boolean chasing)
{
	real reach, effective;

	if (need < (chasing ? AE_BOTS_TRAVEL_PACK_LEAST_NEED_CHASING : AE_BOTS_TRAVEL_PACK_LEAST_NEED))
		return 0.0f;
	reach = AE_BOTS_TRAVEL_PACK_REACH_BASE + AE_BOTS_TRAVEL_PACK_REACH_PER_NEED * need;
	effective = (real)fabs(distance) + (real)fabs(height) * AE_BOTS_TRAVEL_PACK_HEIGHT_STRETCH;
	if (effective >= reach)
		return 0.0f;
	return need * (1.0f - effective / reach);
}

boolean ae_bots_travel_jump_aligned(real dx, real dy, real hop_dx, real hop_dy)
{
	real length = (real)sqrt(dx * dx + dy * dy), hop = (real)sqrt(hop_dx * hop_dx + hop_dy * hop_dy);

	if (length < 1.0e-4f || hop < 1.0e-4f)
		return FALSE;
	return (boolean)((dx * hop_dx + dy * hop_dy) / (length * hop) >=
		(real)cos(AE_BOTS_TRAVEL_JUMP_FACING_DEGREES * 3.14159265f / 180.0f));
}

void ae_bots_travel_stuck_add(struct ae_bots_travel_stuck_spots *spots, real x, real y, real z)
{
	int index, nearest = -1;
	real nearest_distance = AE_BOTS_TRAVEL_STUCK_SPOT_RADIUS * AE_BOTS_TRAVEL_STUCK_SPOT_RADIUS;

	for (index = 0; index < spots->count; index++)
	{
		struct ae_bots_travel_stuck_spot const *spot = &spots->spots[index];
		real dx = spot->x - x, dy = spot->y - y, dz = spot->z - z, distance = dx * dx + dy * dy + dz * dz;

		if (distance <= nearest_distance)
		{
			nearest_distance = distance;
			nearest = index;
		}
	}
	if (nearest >= 0)
	{
		struct ae_bots_travel_stuck_spot *spot = &spots->spots[nearest];
		real weight = 1.0f / (real)(spot->total + 1);

		spot->x += (x - spot->x) * weight;
		spot->y += (y - spot->y) * weight;
		spot->z += (z - spot->z) * weight;
		spot->total++;
		spot->minute++;
		return;
	}
	if (spots->count >= AE_BOTS_TRAVEL_STUCK_SPOTS)
	{
		spots->dropped++;
		return;
	}
	spots->spots[spots->count].x = x;
	spots->spots[spots->count].y = y;
	spots->spots[spots->count].z = z;
	spots->spots[spots->count].total = 1;
	spots->spots[spots->count].minute = 1;
	spots->count++;
}

int ae_bots_travel_stuck_minute(struct ae_bots_travel_stuck_spots *spots, int maximum, char *text, int size)
{
	int listed = 0, length = 0, index;
	boolean used[AE_BOTS_TRAVEL_STUCK_SPOTS];

	if (text && size > 0)
		text[0] = 0;
	memset(used, 0, sizeof(used));
	while (listed < maximum)
	{
		int best = -1;

		for (index = 0; index < spots->count; index++)
		{
			if (used[index] || spots->spots[index].minute <= 0)
				continue;
			/* (most this minute first; the earlier spot of a tie) */
			if (best < 0 || spots->spots[index].minute > spots->spots[best].minute)
				best = index;
		}
		if (best < 0)
			break;
		used[best] = TRUE;
		if (text && size > 0 && length < size)
		{
			int written = snprintf(text + length, (size_t)(size - length), " %.1f %.1f %.1f (%ld of %ld)",
				(double)spots->spots[best].x, (double)spots->spots[best].y, (double)spots->spots[best].z,
				spots->spots[best].minute, spots->spots[best].total);

			if (written > 0)
				length += written;
			if (length > size)
				length = size;
		}
		listed++;
	}
	for (index = 0; index < spots->count; index++)
		spots->spots[index].minute = 0;
	return listed;
}

boolean ae_bots_travel_walk_ends(real across, real rise)
{
	if (across < AE_BOTS_TRAVEL_WALK_SHORTEST || across > AE_BOTS_TRAVEL_WALK_LONGEST)
		return FALSE;
	return (boolean)(rise <= across * AE_BOTS_TRAVEL_WALK_SLOPE + 0.3f && -rise <= across * AE_BOTS_TRAVEL_WALK_SLOPE + 0.3f);
}

boolean ae_bots_travel_walk_step(real rise)
{
	return (boolean)(rise <= AE_BOTS_TRAVEL_WALK_RISE && rise >= -AE_BOTS_TRAVEL_WALK_FALL);
}

int ae_bots_travel_route(int link_count, long const *from, long const *to, unsigned char const *usable, long start,
	long goal)
{
	long nodes[AE_BOTS_TRAVEL_ROUTE_NODES];
	int hops[AE_BOTS_TRAVEL_ROUTE_NODES];
	int node_count = 0, index, changed, best = -1, best_hops = 0x7FFF;

	if (start == -1 || goal == -1 || start == goal)
		return -1;
	nodes[node_count] = goal;
	hops[node_count++] = 0;
	/* (hops to the goal, each link's exit to entry, until none changes) */
	do
	{
		changed = 0;
		for (index = 0; index < link_count; index++)
		{
			int exit_node = -1, entry_node = -1, node;

			for (node = 0; node < node_count; node++)
			{
				if (nodes[node] == to[index])
					exit_node = node;
				if (nodes[node] == from[index])
					entry_node = node;
			}
			if (exit_node < 0 || from[index] == -1 || to[index] == -1)
				continue;
			if (entry_node < 0)
			{
				if (node_count >= AE_BOTS_TRAVEL_ROUTE_NODES)
					continue;
				entry_node = node_count++;
				nodes[entry_node] = from[index];
				hops[entry_node] = 0x7FFF;
			}
			if (hops[exit_node] + 1 < hops[entry_node])
			{
				hops[entry_node] = hops[exit_node] + 1;
				changed = 1;
			}
		}
	} while (changed);
	for (index = 0; index < link_count; index++)
	{
		int node, through = 0x7FFF;

		if (from[index] != start || !usable[index] || to[index] == -1)
			continue;
		for (node = 0; node < node_count; node++)
		{
			if (nodes[node] == to[index])
				through = hops[node];
		}
		if (through < best_hops)
		{
			best_hops = through;
			best = index;
		}
	}
	return best;
}

boolean ae_bots_travel_partial_gains(real start_walk, real end_walk)
{
	if (end_walk < 0.0f)
		return FALSE;
	if (start_walk < 0.0f)
		return TRUE;
	return (boolean)(end_walk <= start_walk - AE_BOTS_TRAVEL_GAIN_LEAST);
}

int ae_bots_travel_way_via(real const *walked, int count, real reach)
{
	int index, via = count > 0 ? 0 : -1;

	for (index = 1; index < count; index++)
	{
		if (walked[index] > reach)
			break;
		via = index;
	}
	return via;
}

boolean ae_bots_travel_climb_drops(boolean ground, real here_z, real ground_z, real goal_z)
{
	/* (no ground at all: the void; ground far below, the goal not on it: a pit to jump into) */
	if (!ground)
		return TRUE;
	return ground_z < here_z - AE_BOTS_TRAVEL_CLIMB_DROP && goal_z > ground_z + AE_BOTS_TRAVEL_CLIMB_DROP;
}

real ae_bots_travel_device_block_radius(real bounding_radius)
{
	if (bounding_radius < AE_BOTS_TRAVEL_DEVICE_BLOCK_MIN)
		return AE_BOTS_TRAVEL_DEVICE_BLOCK_MIN;
	return bounding_radius > AE_BOTS_TRAVEL_DEVICE_BLOCK_MAX ? AE_BOTS_TRAVEL_DEVICE_BLOCK_MAX : bounding_radius;
}

int ae_bots_travel_goal_stuck_next(int count, long last_tick, long now)
{
	if (count <= 0 || now - last_tick > AE_BOTS_TRAVEL_GOAL_STUCK_FORGET)
		return 1;
	return count + 1;
}

boolean ae_bots_travel_goal_stuck_gives_up(int count)
{
	return (boolean)(count >= AE_BOTS_TRAVEL_GOAL_STUCK_GIVE_UP);
}
