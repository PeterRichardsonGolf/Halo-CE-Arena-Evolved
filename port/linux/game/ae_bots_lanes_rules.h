/*
AE_BOTS_LANES_RULES.H

Arena Evolved: side lanes for the bots' mini squads (ae_bots_lanes.c reads the map, this file is the pure part;
port/linux/tests/ae_bots_lanes_rules_test.c). From halo-ce-vita (GPL-3.0) via Halo Content Evolved
(Watanoge/halo-content-evolved, AGPL-3.0-or-later): the walkable graph's searches and the lane choice are theirs,
adapted here.

The map's walkable surfaces are a graph (a node a surface, a link across each shared edge). A cheapest-way search
from the first base gives the direct route to the second; then a search for each side lane, in which a node costs
more the nearer it is to the direct route and more again on the wrong side of it, finds the way round the side. A lane
that keeps near the direct route, or is far longer, is none, and so is a second lane by the first's waypoints. A lane's
waypoints are its nodes a third and two thirds of the way, those well off the direct route. Lane 0 is the lane on the
left-hand side (route_side +1) of the way from the first base to the second, lane 1 the other side.

The builder works a bounded step at a time (a search's pops, a count of nodes) so the engine side can spread it over
the first ticks of a game; the same inputs always give the same lanes.
*/

#ifndef __AE_BOTS_LANES_RULES_H
#define __AE_BOTS_LANES_RULES_H

#define AE_LANES_SIDE_LANES 2
#define AE_LANES_WAYPOINTS 2
#define AE_LANES_ROUTE_POINTS 17

/* a goal nearer than this is gone to directly; how near a waypoint counts as reached; how long one has (seconds) */
#define AE_LANES_LEAST_DISTANCE 25.0f
#define AE_LANES_WAYPOINT_REACHED 3.0f
#define AE_LANES_WAYPOINT_HEIGHT 2.5f
#define AE_LANES_WAYPOINT_SECONDS 40

struct ae_lanes_node
{
	float x, y, z;
	/* from the first base: its cost (a very large number: not reached) */
	float cost;
	/* across the direct route: how far along (0 at the first base, 1 at the second), how far from it, which side (+1, -1) */
	float route_along, route_distance, route_side;
};

struct ae_lanes_route
{
	int count;
	float x[AE_LANES_ROUTE_POINTS], y[AE_LANES_ROUTE_POINTS], z[AE_LANES_ROUTE_POINTS];
	float length;
	float segment[AE_LANES_ROUTE_POINTS], from_start[AE_LANES_ROUTE_POINTS];
};

struct ae_lanes_waypoint
{
	float x, y, z;
	long node;
	float along;
};

struct ae_lanes_lane
{
	int valid;
	int waypoint_count;
	struct ae_lanes_waypoint waypoints[AE_LANES_WAYPOINTS];
	float length, offset;
	char const *why;
};

enum { AE_LANES_RUNNING, AE_LANES_DONE, AE_LANES_NO_WAY, AE_LANES_NO_MEMORY };

struct ae_lanes_builder
{
	struct ae_lanes_node *nodes;
	long node_count;
	long const *link_first;
	long const *links;
	long base[2];
	int phase;
	/* the search: costs, the way back, a binary heap and each node's place in it (-1: not in it, -2: done) */
	float *cost;
	long *previous;
	long *heap;
	long *heap_position;
	long heap_count;
	long target;
	float lane_side;
	float penalty_radius;
	long cursor;
	int lane_index;
	struct ae_lanes_route route;
	struct ae_lanes_lane lanes[AE_LANES_SIDE_LANES];
	int lane_count;
};

/* begin over a graph the caller keeps until ae_lanes_end: nodes (their route fields are set here), the links (link_first
has node_count + 1 entries) and the two bases' nodes. Returns AE_LANES_RUNNING or AE_LANES_NO_MEMORY */
int ae_lanes_begin(struct ae_lanes_builder *builder, struct ae_lanes_node *nodes, long node_count, long const *link_first,
	long const *links, long base_a, long base_b);
/* a step: up to `pops` nodes of a search taken, up to `placed` nodes placed on the route. AE_LANES_RUNNING until done;
AE_LANES_DONE when the lanes (none, perhaps) are made; AE_LANES_NO_WAY when the bases are not joined (the nodes' costs
are set: what is reached from the first base) */
int ae_lanes_step(struct ae_lanes_builder *builder, long pops, long placed);
void ae_lanes_end(struct ae_lanes_builder *builder);

/* the direct route's polyline: how far along (0 to 1) and how far off, signed by side, a point is */
void ae_lanes_route_place(struct ae_lanes_route const *route, float x, float y, float *along, float *offset);

/* a bot's waypoint on a lane: the first one not passed on the way from start_along to goal_along (the way it goes),
strictly between them. -1 for none (the goal is not far along the route: go direct) */
int ae_lanes_next_waypoint(struct ae_lanes_lane const *lane, int const *passed, float start_along, float goal_along);
/* the lane a squad's side takes: side +1 or -1 as bots.c's mini squads read them (the cross product of the way to the
goal and the way to the point), travelling forward (the first base to the second) or back */
int ae_lanes_lane_for_side(int side, int forward);

/* a waypoint walk's step: AE_LANES_WALK_KEEP, AE_LANES_WALK_REACHED (near it, and not far above or below) or
AE_LANES_WALK_GIVE_UP (it has been walked to for too long, in ticks) */
enum { AE_LANES_WALK_KEEP, AE_LANES_WALK_REACHED, AE_LANES_WALK_GIVE_UP };
int ae_lanes_walk_step(float distance_2d, float height_difference, long walked_ticks, long ticks_per_second);

#endif
