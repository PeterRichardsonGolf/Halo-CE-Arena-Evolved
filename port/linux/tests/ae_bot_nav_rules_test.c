#include <stdio.h>
#include <string.h>
#include "ae_bot_nav_rules.h"

static int failures, checks;
#define CHECK(c) do { checks++; if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static struct ae_bot_nav_map map;

static int has_error(int line)
{
	int index;

	for (index = 0; index < map.error_count && index < AE_BOT_NAV_ERRORS; index++)
	{
		if (map.errors[index].line == line)
			return 1;
	}
	return 0;
}

int main(void)
{
	int plan[64], count, next;
	float start[3], goal[3], at[3], off;

	/* a whole file: two roads joined at a junction, a keepout, a foot link, an avoid */
	CHECK(ae_bot_nav_parse(
		"# a test map\n"
		"nav 1 testmap\n"
		"made 1a2b3c4d 0000ffff\n"
		"route west warthog,ghost\n"
		"point 0 0 0\n"
		"point 20 0 0 speed 0.6 turn 4\n"
		"point 40 0 0\n"
		"end\n"
		"route north any\n"
		"point 41 2 0\n"
		"point 41 30 0 wait 2\n"
		"point 41 60 0\n"
		"end\n"
		"keepout 30 10 0 radius 3\n"
		"link jump 1 2 3 to 4 5 6 oneway\n"
		"link walk 1 2 3 to 4 5 6\n"
		"avoid 5 5 5 radius 2\n", &map) == 0);
	CHECK(map.version == 1 && !strcmp(map.file, "testmap") && map.made_count == 2 && map.made[0] == 0x1a2b3c4du);
	CHECK(map.route_count == 2 && map.point_count == 6);
	CHECK(map.routes[0].kinds == (AE_BOT_NAV_WARTHOG | AE_BOT_NAV_GHOST) && map.routes[1].kinds == AE_BOT_NAV_ANY);
	CHECK(map.points[1].speed > 0.59f && map.points[1].speed < 0.61f && map.points[1].turn == 4.0f);
	CHECK(map.points[0].speed == 1.0f && map.points[4].wait == 2.0f);
	CHECK(map.keepout_count == 1 && map.keepouts[0].radius == 3.0f && map.keepouts[0].line == 14);
	CHECK(map.link_count == 2 && map.links[0].kind == AE_BOT_NAV_LINK_JUMP && map.links[0].oneway && !map.links[1].oneway);
	CHECK(map.avoid_count == 1);
	/* the junction: the west road's end (40 0 0) and the north road's start (41 2 0), 2.2 apart */
	CHECK(map.points[2].join_count == 1 && map.points[2].joins[0] == 3);
	CHECK(map.points[3].join_count == 1 && map.points[3].joins[0] == 2);
	CHECK(map.points[0].join_count == 0);

	/* the fit: its file, and its hashes when known */
	CHECK(ae_bot_nav_fits(&map, "testmap", 1, 0x1a2b3c4du));
	CHECK(!ae_bot_nav_fits(&map, "testmap", 1, 0x12345678u));
	CHECK(ae_bot_nav_fits(&map, "testmap", 0, 0));
	CHECK(!ae_bot_nav_fits(&map, "othermap", 0, 0));

	/* a plan: from near the west road's start to near the north road's end, across the junction */
	start[0] = 2.0f; start[1] = 3.0f; start[2] = 0.0f;
	goal[0] = 43.0f; goal[1] = 62.0f; goal[2] = 0.0f;
	count = ae_bot_nav_plan(&map, AE_BOT_NAV_WARTHOG, start, goal, plan, 64);
	CHECK(count == 6 && plan[0] == 0 && plan[2] == 2 && plan[3] == 3 && plan[5] == 5);
	/* a scorpion: the west road is not its own; only the north road, too far from its start (40 units) */
	CHECK(ae_bot_nav_plan(&map, AE_BOT_NAV_SCORPION, start, goal, plan, 64) == 0);
	/* the other way along two-way roads */
	count = ae_bot_nav_plan(&map, AE_BOT_NAV_GHOST, goal, start, plan, 64);
	CHECK(count == 6 && plan[0] == 5 && plan[5] == 0);
	/* a goal close by: no plan (not worth it) */
	goal[0] = 8.0f; goal[1] = 0.0f;
	CHECK(ae_bot_nav_plan(&map, AE_BOT_NAV_WARTHOG, start, goal, plan, 64) == 0);
	/* a start far from every route: none */
	start[0] = 200.0f; goal[0] = 43.0f; goal[1] = 62.0f;
	CHECK(ae_bot_nav_plan(&map, AE_BOT_NAV_WARTHOG, start, goal, plan, 64) == 0);
	/* a plan longer than its room: its first points */
	start[0] = 2.0f;
	CHECK(ae_bot_nav_plan(&map, AE_BOT_NAV_WARTHOG, start, goal, plan, 3) == 3 && plan[0] == 0 && plan[2] == 2);

	/* following: within reach a point is passed; past a point along its next segment, on to the next */
	count = ae_bot_nav_plan(&map, AE_BOT_NAV_WARTHOG, start, goal, plan, 64);
	at[0] = 1.0f; at[1] = 0.5f; at[2] = 0.0f;
	next = ae_bot_nav_follow(&map, plan, count, 0, at, 3.0f, &off);
	CHECK(next == 1);
	at[0] = 25.0f; at[1] = 1.0f;
	next = ae_bot_nav_follow(&map, plan, count, 0, at, 3.0f, &off);
	CHECK(next == 2 && off > 0.9f && off < 1.1f);
	at[0] = 41.0f; at[1] = 61.0f;
	CHECK(ae_bot_nav_follow(&map, plan, count, 4, at, 3.0f, &off) == count);

	/* oneway and loop */
	CHECK(ae_bot_nav_parse("nav 1 m\nroute ring any loop oneway\npoint 0 0 0\npoint 30 0 0\npoint 30 30 0\npoint 0 30 0\nend\n",
		&map) == 0);
	start[0] = 1.0f; start[1] = 29.0f; start[2] = 0.0f;
	goal[0] = 29.0f; goal[1] = 31.0f; goal[2] = 0.0f;
	/* (from the last point the loop goes on to the first, then round: one way, the long way) */
	count = ae_bot_nav_plan(&map, AE_BOT_NAV_GHOST, start, goal, plan, 64);
	CHECK(count == 0 || plan[0] == 3);

	/* refused lines, by number; the rest still loads */
	CHECK(ae_bot_nav_parse(
		"nav 1 bad\n"
		"route Bad-Name warthog\n"	/* 2: upper case */
		"point 0 0 0\n"
		"end\n"
		"route ok tank\n"	/* 5: no such kind */
		"end\n"
		"route short any\n"	/* 7: one point only */
		"point 0 0 0\n"
		"end\n"
		"keepout 1 2 radius 3\n"	/* 10 */
		"keepout 1 2 3 radius 99\n"	/* 11: over 50 */
		"link fly 0 0 0 to 1 1 1\n"	/* 12 */
		"point 1 1 1\n"	/* 13: outside a route */
		"route r any\n"
		"point 0 0 0 speed 2\n"	/* 15: speed over 1 */
		"point 0 0 0\n"
		"point 9 0 0\n"
		"end\n"
		"made 123\n"	/* 19 */
		"what is this\n"	/* 20 */
		"route open any\n"	/* 21: never ended */
		"point 0 0 0\n"
		"point 5 5 5\n", &map) == 11);
	CHECK(map.error_count == 11);
	CHECK(has_error(2) && has_error(5) && has_error(7) && has_error(10) && has_error(11) && has_error(12));
	CHECK(map.route_count == 1 && !strcmp(map.routes[0].name, "r") && map.routes[0].count == 2 && map.point_count == 2);
	CHECK(map.keepout_count == 0 && map.link_count == 0);
	/* no header: nothing */
	CHECK(ae_bot_nav_parse("route r any\n", &map) == 1 && map.version == 0);
	CHECK(ae_bot_nav_parse("nav 2 m\n", &map) == 1 && map.version == 0);
	CHECK(ae_bot_nav_parse("", &map) == 0 && map.version == 0);
	CHECK(ae_bot_nav_parse(NULL, &map) == 0);
	/* limits: 32 routes */
	{
		static char text[20000];
		int index;
		size_t used = (size_t)snprintf(text, sizeof text, "nav 1 m\n");

		for (index = 0; index < 33; index++)
			used += (size_t)snprintf(text + used, sizeof text - used, "route r%d any\npoint %d 0 0\npoint %d 9 0\nend\n",
				index, index * 100, index * 100);
		CHECK(ae_bot_nav_parse(text, &map) == 1 && map.route_count == 32);
	}
	CHECK(ae_bot_nav_kind_of_name("scorpion") == AE_BOT_NAV_SCORPION && ae_bot_nav_kind_of_name("tank") == 0 &&
		ae_bot_nav_kind_of_name(NULL) == 0);
	/* the long way round: two halls 24 apart joined only by a road 250 round; no plan by the detour rule, a plan when the
	driver's own search found no way */
	CHECK(ae_bot_nav_parse("nav 1 halls\nroute round warthog\npoint -12 0 0\npoint -12 100 0\npoint 12 100 0\n"
		"point 12 0 0\nend\n", &map) == 0);
	start[0] = -12.0f; start[1] = 1.0f; start[2] = 0.0f;
	goal[0] = 12.0f; goal[1] = 1.0f; goal[2] = 0.0f;
	CHECK(ae_bot_nav_plan(&map, AE_BOT_NAV_WARTHOG, start, goal, plan, 64) == 0);
	CHECK(ae_bot_nav_plan_any(&map, AE_BOT_NAV_WARTHOG, start, goal, plan, 64, 0) == 0);
	count = ae_bot_nav_plan_any(&map, AE_BOT_NAV_WARTHOG, start, goal, plan, 64, 1);
	CHECK(count == 4 && plan[0] == 0 && plan[3] == 3);
	/* (still none when the route brings it no nearer) */
	goal[0] = -12.0f; goal[1] = 3.0f;
	CHECK(ae_bot_nav_plan_any(&map, AE_BOT_NAV_WARTHOG, start, goal, plan, 64, 1) == 0);

	printf("ae_bot_nav_rules: %d checks, %d failures\n", checks, failures);
	return failures ? 1 : 0;
}
