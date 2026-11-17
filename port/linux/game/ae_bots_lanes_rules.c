/*
AE_BOTS_LANES_RULES.C

Arena Evolved: side lanes for the bots' mini squads (ae_bots_lanes_rules.h). From halo-ce-vita (GPL-3.0) via Halo
Content Evolved (Watanoge/halo-content-evolved, AGPL-3.0-or-later).
*/

#include "ae_bots_lanes_rules.h"

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

enum
{
	PHASE_SEARCH_ALL,
	PHASE_PLACE,
	PHASE_LANE,
	PHASE_DONE,
};

static float distance2d(float ax, float ay, float bx, float by)
{
	float dx = ax - bx, dy = ay - by;

	return (float)sqrt(dx * dx + dy * dy);
}

static float distance3d(struct ae_lanes_node const *a, struct ae_lanes_node const *b)
{
	float dx = a->x - b->x, dy = a->y - b->y, dz = a->z - b->z;

	return (float)sqrt(dx * dx + dy * dy + dz * dz);
}

static float pin(float value, float low, float high)
{
	return value < low ? low : value > high ? high : value;
}

/* ---------- the searches (a cheapest-way search with an indexed binary heap) */

/* what entering a node costs a lane's search: the nearer the direct route the dearer, and the other side of it dearer still;
neither near the bases (the first and last sixth of the way, full from a third on), which every way leaves and comes to */
static float weight(struct ae_lanes_builder const *builder, long index)
{
	struct ae_lanes_node const *node = &builder->nodes[index];
	float result = 1.0f, middle;

	if (builder->lane_side == 0.0f)
		return 1.0f;
	middle = node->route_along < 1.0f - node->route_along ? node->route_along : 1.0f - node->route_along;
	middle = pin((middle - 0.15f) / 0.15f, 0.0f, 1.0f);
	if (middle <= 0.0f)
		return 1.0f;
	if (node->route_distance < builder->penalty_radius)
		result += 4.0f * middle * (1.0f - node->route_distance / builder->penalty_radius);
	if (node->route_side * builder->lane_side < 0.0f && node->route_distance > 2.0f)
		result += 10.0f * middle;
	return result;
}

static int heap_less(struct ae_lanes_builder const *builder, long a, long b)
{
	return builder->cost[builder->heap[a]] < builder->cost[builder->heap[b]];
}

static void heap_swap(struct ae_lanes_builder *builder, long a, long b)
{
	long node = builder->heap[a];

	builder->heap[a] = builder->heap[b];
	builder->heap[b] = node;
	builder->heap_position[builder->heap[a]] = a;
	builder->heap_position[builder->heap[b]] = b;
}

static void heap_up(struct ae_lanes_builder *builder, long position)
{
	while (position > 0 && heap_less(builder, position, (position - 1) / 2))
	{
		heap_swap(builder, position, (position - 1) / 2);
		position = (position - 1) / 2;
	}
}

static void heap_down(struct ae_lanes_builder *builder, long position)
{
	for (;;)
	{
		long child = position * 2 + 1, best = position;

		if (child < builder->heap_count && heap_less(builder, child, best))
			best = child;
		if (child + 1 < builder->heap_count && heap_less(builder, child + 1, best))
			best = child + 1;
		if (best == position)
			return;
		heap_swap(builder, position, best);
		position = best;
	}
}

static void search_start(struct ae_lanes_builder *builder, long origin, long target, float lane_side)
{
	long index;

	for (index = 0; index < builder->node_count; index++)
	{
		builder->cost[index] = FLT_MAX;
		builder->previous[index] = -1;
		builder->heap_position[index] = -1;
	}
	builder->target = target;
	builder->lane_side = lane_side;
	builder->cost[origin] = 0.0f;
	builder->heap[0] = origin;
	builder->heap_position[origin] = 0;
	builder->heap_count = 1;
}

/* TRUE when the search is done (every reachable node taken, or the target) */
static int search_step(struct ae_lanes_builder *builder, long pops)
{
	long taken;

	for (taken = 0; taken < pops && builder->heap_count > 0; taken++)
	{
		long node_index = builder->heap[0], link;
		float node_weight = weight(builder, node_index);

		builder->heap_count--;
		if (builder->heap_count > 0)
		{
			builder->heap[0] = builder->heap[builder->heap_count];
			builder->heap_position[builder->heap[0]] = 0;
			heap_down(builder, 0);
		}
		builder->heap_position[node_index] = -2;
		if (node_index == builder->target)
			return 1;
		for (link = builder->link_first[node_index]; link < builder->link_first[node_index + 1]; link++)
		{
			long other = builder->links[link];
			float cost;

			if (builder->heap_position[other] == -2)
				continue;
			cost = builder->cost[node_index] + distance3d(&builder->nodes[node_index], &builder->nodes[other]) * 0.5f *
				(node_weight + weight(builder, other));
			if (cost >= builder->cost[other])
				continue;
			builder->cost[other] = cost;
			builder->previous[other] = node_index;
			if (builder->heap_position[other] == -1)
			{
				builder->heap[builder->heap_count] = other;
				builder->heap_position[other] = builder->heap_count;
				builder->heap_count++;
			}
			heap_up(builder, builder->heap_position[other]);
		}
	}
	return builder->heap_count <= 0;
}

/* ---------- the direct route */

void ae_lanes_route_place(struct ae_lanes_route const *route, float x, float y, float *along, float *offset)
{
	int index, best = 0;
	float best_distance = FLT_MAX, best_u = 0.0f, best_side = 1.0f;

	*along = 0.0f;
	*offset = 0.0f;
	if (route->count < 2 || route->length <= 0.0f)
		return;
	for (index = 0; index + 1 < route->count; index++)
	{
		float dx = route->x[index + 1] - route->x[index], dy = route->y[index + 1] - route->y[index];
		float px = x - route->x[index], py = y - route->y[index];
		float length2 = dx * dx + dy * dy;
		float u = length2 > 0.0001f ? (px * dx + py * dy) / length2 : 0.0f;
		float cx, cy, distance;

		u = pin(u, 0.0f, 1.0f);
		cx = route->x[index] + dx * u - x;
		cy = route->y[index] + dy * u - y;
		distance = (float)sqrt(cx * cx + cy * cy);
		if (distance < best_distance)
		{
			best_distance = distance;
			best = index;
			best_u = u;
			best_side = dx * py - dy * px >= 0.0f ? 1.0f : -1.0f;
		}
	}
	*along = (route->from_start[best] + route->segment[best] * best_u) / route->length;
	*offset = best_distance * best_side;
}

/* the node chain from the first base to the search's target; NULL when none (or no memory); *count is its length */
static long *chain(struct ae_lanes_builder const *builder, long *count)
{
	long node_index, length = 0, point;
	long *nodes;

	if (builder->cost[builder->target] >= FLT_MAX)
		return NULL;
	for (node_index = builder->target; node_index != -1; node_index = builder->previous[node_index])
		length++;
	nodes = (long *)malloc(sizeof(long) * (size_t)length);
	if (!nodes)
		return NULL;
	point = length;
	for (node_index = builder->target; node_index != -1; node_index = builder->previous[node_index])
		nodes[--point] = node_index;
	*count = length;
	return nodes;
}

/* the direct route from the way back from the second base: its points every sixteenth of its length (2D) */
static int make_direct(struct ae_lanes_builder *builder)
{
	struct ae_lanes_route *route = &builder->route;
	long count, point;
	float total = 0.0f, walked = 0.0f;
	long *nodes = chain(builder, &count);

	memset(route, 0, sizeof(*route));
	if (!nodes)
		return 0;
	for (point = 0; point + 1 < count; point++)
		total += distance2d(builder->nodes[nodes[point]].x, builder->nodes[nodes[point]].y, builder->nodes[nodes[point + 1]].x,
			builder->nodes[nodes[point + 1]].y);
	if (total < 1.0f || count < 2)
	{
		free(nodes);
		return 0;
	}
	route->x[0] = builder->nodes[nodes[0]].x;
	route->y[0] = builder->nodes[nodes[0]].y;
	route->z[0] = builder->nodes[nodes[0]].z;
	route->count = 1;
	for (point = 0; point + 1 < count && route->count < AE_LANES_ROUTE_POINTS; point++)
	{
		struct ae_lanes_node const *a = &builder->nodes[nodes[point]], *b = &builder->nodes[nodes[point + 1]];
		float step = distance2d(a->x, a->y, b->x, b->y);

		while (step > 0.0f && route->count < AE_LANES_ROUTE_POINTS - 1 &&
			walked + step >= total * (float)route->count / (float)(AE_LANES_ROUTE_POINTS - 1))
		{
			float u = (total * (float)route->count / (float)(AE_LANES_ROUTE_POINTS - 1) - walked) / step;

			route->x[route->count] = a->x + (b->x - a->x) * u;
			route->y[route->count] = a->y + (b->y - a->y) * u;
			route->z[route->count] = a->z + (b->z - a->z) * u;
			route->count++;
		}
		walked += step;
	}
	route->x[route->count] = builder->nodes[nodes[count - 1]].x;
	route->y[route->count] = builder->nodes[nodes[count - 1]].y;
	route->z[route->count] = builder->nodes[nodes[count - 1]].z;
	route->count++;
	free(nodes);
	route->length = 0.0f;
	for (point = 0; point < route->count; point++)
	{
		route->from_start[point] = route->length;
		route->segment[point] = point + 1 < route->count ?
			distance2d(route->x[point], route->y[point], route->x[point + 1], route->y[point + 1]) : 0.0f;
		route->length += route->segment[point];
	}
	return route->length > 1.0f;
}

/* ---------- the side lanes */

static void make_lane(struct ae_lanes_builder *builder, int lane_index)
{
	struct ae_lanes_lane *lane = &builder->lanes[lane_index];
	long count = 0, point, waypoint, peak = -1;
	float length = 0.0f, offset = 0.0f, walked;
	long *nodes;
	char const *why = NULL;

	memset(lane, 0, sizeof(*lane));
	nodes = chain(builder, &count);
	if (!nodes)
	{
		lane->why = "the second base not reached";
		return;
	}
	for (point = 0; point < count; point++)
	{
		struct ae_lanes_node const *node = &builder->nodes[nodes[point]];

		if (point > 0)
			length += distance3d(&builder->nodes[nodes[point - 1]], node);
		if (node->route_along > 0.15f && node->route_along < 0.85f && node->route_distance > offset)
		{
			offset = node->route_distance;
			peak = point;
		}
	}
	lane->length = length;
	lane->offset = offset;
	if (peak == -1 || offset < (builder->route.length * 0.08f > 5.0f ? builder->route.length * 0.08f : 5.0f))
		why = "it keeps near the direct route";
	else if (length > builder->route.length * 2.2f)
		why = "too long";
	if (!why)
	{
		static float const fractions[AE_LANES_WAYPOINTS] = { 0.33f, 0.67f };

		for (waypoint = 0; waypoint < AE_LANES_WAYPOINTS; waypoint++)
		{
			long best = -1;
			float best_difference = FLT_MAX;

			walked = 0.0f;
			for (point = 0; point < count; point++)
			{
				struct ae_lanes_node const *node = &builder->nodes[nodes[point]];

				if (point > 0)
					walked += distance3d(&builder->nodes[nodes[point - 1]], node);
				if (node->route_distance >= offset * 0.5f && node->route_along > 0.1f && node->route_along < 0.9f &&
					(float)fabs(walked / length - fractions[waypoint]) < best_difference)
				{
					best_difference = (float)fabs(walked / length - fractions[waypoint]);
					best = nodes[point];
				}
			}
			if (best == -1 || (lane->waypoint_count > 0 && distance2d(lane->waypoints[lane->waypoint_count - 1].x,
				lane->waypoints[lane->waypoint_count - 1].y, builder->nodes[best].x, builder->nodes[best].y) < 8.0f))
			{
				continue;
			}
			lane->waypoints[lane->waypoint_count].x = builder->nodes[best].x;
			lane->waypoints[lane->waypoint_count].y = builder->nodes[best].y;
			lane->waypoints[lane->waypoint_count].z = builder->nodes[best].z;
			lane->waypoints[lane->waypoint_count].node = best;
			lane->waypoints[lane->waypoint_count].along = builder->nodes[best].route_along;
			lane->waypoint_count++;
		}
		if (!lane->waypoint_count)
		{
			struct ae_lanes_node const *node = &builder->nodes[nodes[peak]];

			lane->waypoints[0].x = node->x;
			lane->waypoints[0].y = node->y;
			lane->waypoints[0].z = node->z;
			lane->waypoints[0].node = nodes[peak];
			lane->waypoints[0].along = node->route_along;
			lane->waypoint_count = 1;
		}
		/* (in order along the direct route) */
		if (lane->waypoint_count == 2 && lane->waypoints[0].along > lane->waypoints[1].along)
		{
			struct ae_lanes_waypoint first = lane->waypoints[0];

			lane->waypoints[0] = lane->waypoints[1];
			lane->waypoints[1] = first;
		}
	}
	/* (the second lane by the first's waypoints: the same lane) */
	if (!why && lane_index == 1 && builder->lanes[0].valid)
	{
		int a, b;

		for (a = 0; a < builder->lanes[0].waypoint_count; a++)
			for (b = 0; b < lane->waypoint_count; b++)
				if (distance2d(builder->lanes[0].waypoints[a].x, builder->lanes[0].waypoints[a].y, lane->waypoints[b].x,
					lane->waypoints[b].y) < 10.0f)
					why = "the first lane's";
	}
	free(nodes);
	if (why)
	{
		lane->waypoint_count = 0;
		lane->why = why;
		return;
	}
	lane->valid = 1;
	builder->lane_count++;
}

/* ---------- making them */

int ae_lanes_begin(struct ae_lanes_builder *builder, struct ae_lanes_node *nodes, long node_count, long const *link_first,
	long const *links, long base_a, long base_b)
{
	memset(builder, 0, sizeof(*builder));
	builder->nodes = nodes;
	builder->node_count = node_count;
	builder->link_first = link_first;
	builder->links = links;
	builder->base[0] = base_a;
	builder->base[1] = base_b;
	builder->cost = (float *)malloc(sizeof(float) * (size_t)node_count);
	builder->previous = (long *)malloc(sizeof(long) * (size_t)node_count);
	builder->heap = (long *)malloc(sizeof(long) * (size_t)node_count);
	builder->heap_position = (long *)malloc(sizeof(long) * (size_t)node_count);
	if (!builder->cost || !builder->previous || !builder->heap || !builder->heap_position)
	{
		ae_lanes_end(builder);
		return AE_LANES_NO_MEMORY;
	}
	for (base_a = 0; base_a < node_count; base_a++)
	{
		nodes[base_a].cost = FLT_MAX;
		nodes[base_a].route_along = nodes[base_a].route_distance = 0.0f;
		nodes[base_a].route_side = 1.0f;
	}
	search_start(builder, builder->base[0], -1, 0.0f);
	builder->phase = PHASE_SEARCH_ALL;
	return AE_LANES_RUNNING;
}

int ae_lanes_step(struct ae_lanes_builder *builder, long pops, long placed)
{
	switch (builder->phase)
	{
	case PHASE_SEARCH_ALL:
		if (search_step(builder, pops))
		{
			long index;

			for (index = 0; index < builder->node_count; index++)
				builder->nodes[index].cost = builder->cost[index];
			builder->target = builder->base[1];
			if (builder->cost[builder->target] >= FLT_MAX)
				return AE_LANES_NO_WAY;
			if (!make_direct(builder))
				return AE_LANES_NO_WAY;
			builder->cursor = 0;
			builder->phase = PHASE_PLACE;
		}
		break;
	case PHASE_PLACE:
	{
		long last = builder->cursor + placed < builder->node_count ? builder->cursor + placed : builder->node_count;

		for (; builder->cursor < last; builder->cursor++)
		{
			struct ae_lanes_node *node = &builder->nodes[builder->cursor];
			float offset;

			ae_lanes_route_place(&builder->route, node->x, node->y, &node->route_along, &offset);
			node->route_distance = (float)fabs(offset);
			node->route_side = offset >= 0.0f ? 1.0f : -1.0f;
		}
		if (builder->cursor >= builder->node_count)
		{
			builder->penalty_radius = pin(builder->route.length * 0.15f, 6.0f, 18.0f);
			builder->lane_index = 0;
			search_start(builder, builder->base[0], builder->base[1], 1.0f);
			builder->phase = PHASE_LANE;
		}
		break;
	}
	case PHASE_LANE:
		if (search_step(builder, pops))
		{
			make_lane(builder, builder->lane_index);
			if (++builder->lane_index < AE_LANES_SIDE_LANES)
				search_start(builder, builder->base[0], builder->base[1], -1.0f);
			else
				builder->phase = PHASE_DONE;
		}
		break;
	case PHASE_DONE:
		return AE_LANES_DONE;
	}
	return builder->phase == PHASE_DONE ? AE_LANES_DONE : AE_LANES_RUNNING;
}

void ae_lanes_end(struct ae_lanes_builder *builder)
{
	free(builder->cost);
	free(builder->previous);
	free(builder->heap);
	free(builder->heap_position);
	builder->cost = NULL;
	builder->previous = NULL;
	builder->heap = NULL;
	builder->heap_position = NULL;
}

/* ---------- a bot's lane */

int ae_lanes_next_waypoint(struct ae_lanes_lane const *lane, int const *passed, float start_along, float goal_along)
{
	float travel = goal_along - start_along;
	int index;

	if (!lane->valid || (float)fabs(travel) < 0.2f)
		return -1;
	for (index = 0; index < lane->waypoint_count; index++)
	{
		int waypoint = travel > 0.0f ? index : lane->waypoint_count - 1 - index;
		float along = lane->waypoints[waypoint].along;

		if (passed[waypoint])
			continue;
		if (travel > 0.0f ? along > start_along + 0.03f && along < goal_along - 0.03f :
			along < start_along - 0.03f && along > goal_along + 0.03f)
			return waypoint;
	}
	return -1;
}

int ae_lanes_lane_for_side(int side, int forward)
{
	return (side > 0) == (forward != 0) ? 0 : 1;
}

int ae_lanes_walk_step(float distance_2d, float height_difference, long walked_ticks, long ticks_per_second)
{
	if (distance_2d < AE_LANES_WAYPOINT_REACHED && (float)fabs(height_difference) < AE_LANES_WAYPOINT_HEIGHT)
		return AE_LANES_WALK_REACHED;
	if (walked_ticks > ticks_per_second * AE_LANES_WAYPOINT_SECONDS)
		return AE_LANES_WALK_GIVE_UP;
	return AE_LANES_WALK_KEEP;
}
