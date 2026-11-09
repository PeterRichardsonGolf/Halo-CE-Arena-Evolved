#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include "ae_bots_lanes_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

#define W 61
#define H 31

/* a grid of nodes a unit apart (8 links), the nodes on `wall` left out; the two bases mid-height at each end */
struct grid
{
	struct ae_lanes_node nodes[W * H];
	long index[W * H];
	long first[W * H + 1];
	long links[W * H * 8];
	long count;
	long base[2];
};

static int walled(int x, int y, int wall)
{
	/* wall 1: a block across the middle of the direct route */
	if (wall == 2)
		return x == 30;
	return wall == 1 && x >= 26 && x <= 34 && y >= 10 && y <= 20;
}

static void build(struct grid *g, int wall)
{
	int x, y, dx, dy;
	long link = 0;

	g->count = 0;
	for (y = 0; y < H; y++)
		for (x = 0; x < W; x++)
			g->index[y * W + x] = -1;
	for (y = 0; y < H; y++)
		for (x = 0; x < W; x++)
			if (!walled(x, y, wall))
			{
				g->index[y * W + x] = g->count;
				g->nodes[g->count].x = (float)x;
				g->nodes[g->count].y = (float)y;
				g->nodes[g->count].z = 0.0f;
				g->count++;
			}
	for (y = 0, g->count = 0; y < H; y++)
		for (x = 0; x < W; x++)
		{
			if (g->index[y * W + x] < 0)
				continue;
			g->first[g->count++] = link;
			for (dy = -1; dy <= 1; dy++)
				for (dx = -1; dx <= 1; dx++)
				{
					int ox = x + dx, oy = y + dy;

					if ((dx || dy) && ox >= 0 && oy >= 0 && ox < W && oy < H && g->index[oy * W + ox] >= 0)
						g->links[link++] = g->index[oy * W + ox];
				}
		}
	g->first[g->count] = link;
	g->base[0] = g->index[15 * W + 2];
	g->base[1] = g->index[15 * W + 58];
}

static int run(struct grid *g, struct ae_lanes_builder *b)
{
	int result, steps = 0;

	if (ae_lanes_begin(b, g->nodes, g->count, g->first, g->links, g->base[0], g->base[1]) != AE_LANES_RUNNING)
		return AE_LANES_NO_MEMORY;
	while ((result = ae_lanes_step(b, 80, 200)) == AE_LANES_RUNNING && steps++ < 100000)
		;
	return result;
}

int main(void)
{
	static struct grid g;
	struct ae_lanes_builder b;
	int result, passed[AE_LANES_WAYPOINTS] = { 0, 0 };

	/* an open field: the direct route straight across, a side lane each side */
	build(&g, 0);
	result = run(&g, &b);
	CHECK(result == AE_LANES_DONE);
	CHECK(b.route.count == AE_LANES_ROUTE_POINTS);
	CHECK(b.route.length > 55.0f && b.route.length < 60.0f);
	CHECK(b.lane_count == 2);
	CHECK(b.lanes[0].valid && b.lanes[1].valid);
	if (b.lanes[0].valid && b.lanes[1].valid)
	{
		/* lane 0 is on the +1 side, lane 1 on the other; waypoints go in order along the route and stand well off it */
		float along, offset;
		int w;

		for (w = 0; w < b.lanes[0].waypoint_count; w++)
		{
			ae_lanes_route_place(&b.route, b.lanes[0].waypoints[w].x, b.lanes[0].waypoints[w].y, &along, &offset);
			CHECK(offset > 2.0f);
			CHECK(along > 0.1f && along < 0.9f);
		}
		for (w = 0; w < b.lanes[1].waypoint_count; w++)
		{
			ae_lanes_route_place(&b.route, b.lanes[1].waypoints[w].x, b.lanes[1].waypoints[w].y, &along, &offset);
			CHECK(offset < -2.0f);
		}
		CHECK(b.lanes[0].offset >= 5.0f && b.lanes[1].offset >= 5.0f);
		CHECK(b.lanes[0].length < b.route.length * 2.2f);
		CHECK(b.lanes[0].waypoint_count >= 1 && b.lanes[1].waypoint_count >= 1);
	}
	/* the nodes reached from the first base: all of them */
	CHECK(g.nodes[g.base[1]].cost < FLT_MAX && g.nodes[g.base[1]].route_along > 0.95f);
	ae_lanes_end(&b);

	/* the same field again: the same lanes (no randomness) */
	{
		struct ae_lanes_lane first = b.lanes[0];

		build(&g, 0);
		run(&g, &b);
		CHECK(b.lanes[0].waypoint_count == first.waypoint_count);
		CHECK(b.lanes[0].waypoints[0].node == first.waypoints[0].node);
		CHECK(b.lanes[0].length == first.length);
		ae_lanes_end(&b);
	}

	/* a block across the way: the direct route goes round it; lanes are found or given a reason, never a crash */
	build(&g, 1);
	result = run(&g, &b);
	CHECK(result == AE_LANES_DONE);
	CHECK(b.route.length > 58.0f);
	CHECK(b.lane_count >= 1);
	ae_lanes_end(&b);

	/* the bases not joined (a wall across the whole field): no way, no lanes */
	build(&g, 2);
	result = run(&g, &b);
	CHECK(result == AE_LANES_NO_WAY);
	CHECK(b.lane_count == 0);
	CHECK(g.nodes[g.base[1]].cost >= FLT_MAX);
	ae_lanes_end(&b);

	/* a lane's waypoints on the way: the first not passed, strictly between the start and the goal, either way */
	{
		struct ae_lanes_lane lane;

		lane.valid = 1;
		lane.waypoint_count = 2;
		lane.waypoints[0].along = 0.33f;
		lane.waypoints[1].along = 0.67f;
		CHECK(ae_lanes_next_waypoint(&lane, passed, 0.0f, 1.0f) == 0);
		passed[0] = 1;
		CHECK(ae_lanes_next_waypoint(&lane, passed, 0.0f, 1.0f) == 1);
		passed[1] = 1;
		CHECK(ae_lanes_next_waypoint(&lane, passed, 0.0f, 1.0f) == -1);
		passed[0] = passed[1] = 0;
		/* back the other way: the last waypoint first */
		CHECK(ae_lanes_next_waypoint(&lane, passed, 1.0f, 0.0f) == 1);
		/* a goal on the way to the first waypoint's short side, or not far along the route: none */
		CHECK(ae_lanes_next_waypoint(&lane, passed, 0.0f, 0.4f) == 0);
		CHECK(ae_lanes_next_waypoint(&lane, passed, 0.0f, 0.3f) == -1);
		CHECK(ae_lanes_next_waypoint(&lane, passed, 0.5f, 0.6f) == -1);
		/* start past the first waypoint: the second */
		CHECK(ae_lanes_next_waypoint(&lane, passed, 0.4f, 1.0f) == 1);
		lane.valid = 0;
		CHECK(ae_lanes_next_waypoint(&lane, passed, 0.0f, 1.0f) == -1);
	}

	/* a squad's side takes the lane on that side of the way it goes */
	CHECK(ae_lanes_lane_for_side(1, 1) == 0);
	CHECK(ae_lanes_lane_for_side(-1, 1) == 1);
	CHECK(ae_lanes_lane_for_side(1, 0) == 1);
	CHECK(ae_lanes_lane_for_side(-1, 0) == 0);

	/* the walk: reached near it and level, kept on the way, given up after 40 seconds */
	CHECK(ae_lanes_walk_step(2.0f, 0.5f, 10, 30) == AE_LANES_WALK_REACHED);
	CHECK(ae_lanes_walk_step(2.0f, 4.0f, 10, 30) == AE_LANES_WALK_KEEP);
	CHECK(ae_lanes_walk_step(10.0f, 0.0f, 30 * 40, 30) == AE_LANES_WALK_KEEP);
	CHECK(ae_lanes_walk_step(10.0f, 0.0f, 30 * 40 + 1, 30) == AE_LANES_WALK_GIVE_UP);
	CHECK(ae_lanes_walk_step(1.0f, 0.0f, 30 * 90, 30) == AE_LANES_WALK_REACHED);

	return failures ? 1 : (printf("ae_bots_lanes_rules: all passed\n"), 0);
}
