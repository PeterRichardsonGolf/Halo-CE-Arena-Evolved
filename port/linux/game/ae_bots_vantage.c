/*
AE_BOTS_VANTAGE.C

The engine's side of the bots' vantage spots (ae_bots_vantage.h). From halo-ce-vita (GPL-3.0) via Halo Content Evolved
(Watanoge/halo-content-evolved, AGPL-3.0-or-later): the places and the passes are theirs, adapted here.

The places that matter: the CTF flag stands, King of the Hill's hills, the oddball's spawns (in those games), the direct route a
quarter, half and three quarters of the way between the bases, the side lanes' waypoints, and as many of the weapon and item
spawns as there is room for (sixteen places in all; each the furthest from those taken: where the players go). The spots: the
map's walkable surfaces (ae_bots_lanes.c's graph) reached from the bases, flat and a body's size at least, each scored first by
how high it stands above the walkable surfaces about it and how many places lie within a rifle's reach, the best thirty kept six
units apart; then the AI's sight test (the level, scenery and machines) from each one's eyes to each place within ninety units,
and eight rays about it for how open it is; the best six that see two places at least, twelve units apart, are the map's. All
of it a few hundred surfaces or two rays a tick, nothing at the map's load. Indoor maps have few or none.
*/

#include "cseries.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "scenario/scenario.h"
#include "scenario/scenario_definitions.h"
#include "physics/collisions.h"
#include "tag_files/tag_groups.h"

#include "ae_bots_lanes.h"
#include "ae_bots_vantage.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

void platform_log(char const *format, ...);

/* the work of a tick: surfaces scored, rays cast */
#define NODES_A_TICK 200
#define RAYS_A_TICK 2
/* a bot's eyes above its feet, a place's middle above its ground */
#define EYE_HEIGHT 0.62f
#define PLACE_HEIGHT 0.55f
/* the openness rays' length; the height grid's cells (world units) and the most along a side */
#define OPEN_LENGTH 15.0f
#define GRID_CELL 8.0f
#define GRID_MAXIMUM 128

enum { PHASE_IDLE, PHASE_BOUNDS, PHASE_GRID, PHASE_CANDIDATES, PHASE_RAYS, PHASE_DONE };

static struct
{
	int phase;
	boolean ready;
	long cursor;
	long update_tick;
	long place_count;
	struct ae_vantage_place places[AE_VANTAGE_PLACES];
	char const *place_names[AE_VANTAGE_PLACES];
	float bounds_x0, bounds_y0, bounds_x1, bounds_y1;
	struct ae_vantage_grid grid;
	int candidate_count;
	struct ae_vantage_spot candidates[AE_VANTAGE_CANDIDATES];
	long rays;
	long count;
	struct ae_vantage_spot points[AE_VANTAGE_MAXIMUM];
} vantage;

static real vantage_distance(real_point3d const *a, real_point3d const *b)
{
	real dx = a->x - b->x, dy = a->y - b->y, dz = a->z - b->z;

	return (real)sqrt(dx * dx + dy * dy + dz * dz);
}

void ae_bots_vantage_reset(void)
{
	ae_vantage_grid_end(&vantage.grid);
	memset(&vantage, 0, sizeof(vantage));
	vantage.phase = PHASE_IDLE;
	vantage.update_tick = NONE;
}

boolean ae_bots_vantage_ready(void)
{
	return vantage.ready;
}

long ae_bots_vantage_count(void)
{
	return vantage.count;
}

struct ae_vantage_spot const *ae_bots_vantage_spot(long index)
{
	return index >= 0 && index < vantage.count ? &vantage.points[index] : NULL;
}

long ae_bots_vantage_place_count(void)
{
	return vantage.place_count;
}

boolean ae_bots_vantage_place(long index, struct ae_vantage_place *place, char const **name)
{
	if (index < 0 || index >= vantage.place_count)
		return FALSE;
	*place = vantage.places[index];
	if (name)
		*name = vantage.place_names[index];
	return TRUE;
}

boolean ae_bots_vantage_named_place(char const *name, long *index)
{
	long place;

	for (place = 0; place < vantage.place_count; place++)
		if (!strcmp(vantage.place_names[place], name))
		{
			*index = place;
			return TRUE;
		}
	return FALSE;
}

/* whether a ray from `from` to `to` meets nothing (the bots' line of sight but for vehicles, which move) */
static boolean ray_clear(real_point3d const *from, real_point3d const *to)
{
	struct collision_result collision;
	real_vector3d vector;

	vector.i = to->x - from->x;
	vector.j = to->y - from->y;
	vector.k = to->z - from->z;
	vantage.rays++;
	return !collision_test_vector(_collision_test_for_line_of_sight_flags & ~FLAG(_collision_test_objects_vehicles_bit),
		from, &vector, NONE, &collision);
}

static void add_place(real_point3d const *point, real weight, char const *name)
{
	real_point3d ground, at = *point;
	struct ae_vantage_place *place;

	if (vantage.place_count >= AE_VANTAGE_PLACES)
		return;
	if (ae_bots_lanes_ground(point, &ground) != NONE)
		at = ground;
	place = &vantage.places[vantage.place_count];
	place->x = at.x;
	place->y = at.y;
	place->z = at.z + PLACE_HEIGHT;
	place->weight = weight;
	vantage.place_names[vantage.place_count] = name;
	vantage.place_count++;
}

/* the places that matter in this game */
static void find_places(void)
{
	struct scenario *scenario = global_scenario_get();
	long game = game_engine_running() && game_engine_get_variant() ? game_engine_get_variant()->game_engine_index : game_engine_none;
	struct ae_lanes_route const *route = ae_bots_lanes_route();
	long index, lane, waypoint;

	vantage.place_count = 0;
	if (!scenario)
		return;
	for (index = 0; index < scenario->netgame_flags.count; index++)
	{
		struct scenario_netgame_flag *flag = TAG_BLOCK_GET_ELEMENT(&scenario->netgame_flags, index, struct scenario_netgame_flag);

		if (flag->type == _netgame_flag_ctf_flag && (flag->team_index == 0 || flag->team_index == 1))
			add_place(&flag->position, game == game_engine_ctf ? 3.0f : 1.5f, flag->team_index ? "stand 1" : "stand 0");
	}
	if (game == game_engine_king)
	{
		/* (a hill's markers share its team index: their middle) */
		for (index = 0; index < 8; index++)
		{
			real_point3d sum = { 0.0f, 0.0f, 0.0f };
			long marker, count = 0;

			for (marker = 0; marker < scenario->netgame_flags.count; marker++)
			{
				struct scenario_netgame_flag *flag =
					TAG_BLOCK_GET_ELEMENT(&scenario->netgame_flags, marker, struct scenario_netgame_flag);

				if (flag->type == _netgame_flag_hill && flag->team_index == index)
				{
					sum.x += flag->position.x;
					sum.y += flag->position.y;
					sum.z += flag->position.z;
					count++;
				}
			}
			if (count)
			{
				sum.x /= (real)count;
				sum.y /= (real)count;
				sum.z /= (real)count;
				add_place(&sum, 3.0f, "hill");
			}
		}
	}
	if (game == game_engine_oddball)
	{
		long count = 0;

		for (index = 0; index < scenario->netgame_flags.count && count < 4; index++)
		{
			struct scenario_netgame_flag *flag =
				TAG_BLOCK_GET_ELEMENT(&scenario->netgame_flags, index, struct scenario_netgame_flag);

			if (flag->type == _netgame_flag_oddball_ball_spawn)
			{
				add_place(&flag->position, 2.0f, "ball spawn");
				count++;
			}
		}
	}
	if (route && route->count == AE_LANES_ROUTE_POINTS)
	{
		for (index = 4; index <= 12; index += 4)
		{
			real_point3d point;

			point.x = route->x[index];
			point.y = route->y[index];
			point.z = route->z[index];
			add_place(&point, 2.0f, "route");
		}
	}
	for (lane = 0; lane < AE_LANES_SIDE_LANES; lane++)
	{
		struct ae_lanes_lane const *found = ae_bots_lanes_lane((int)lane);

		for (waypoint = 0; found && waypoint < found->waypoint_count; waypoint++)
		{
			real_point3d point;

			point.x = found->waypoints[waypoint].x;
			point.y = found->waypoints[waypoint].y;
			point.z = found->waypoints[waypoint].z;
			add_place(&point, 1.0f, "lane");
		}
	}
	/* the weapon and item spawns, where the players go, as many as there is room for, each the furthest from those taken */
	{
		long first = vantage.place_count, count;

		for (count = 0; vantage.place_count < AE_VANTAGE_PLACES && count < 8; count++)
		{
			long best = NONE;
			real best_distance = -1.0f;

			for (index = 0; index < scenario->netgame_equipment.count; index++)
			{
				struct scenario_netgame_equipment *equipment =
					TAG_BLOCK_GET_ELEMENT(&scenario->netgame_equipment, index, struct scenario_netgame_equipment);
				real nearest = REAL_MAX;
				long place;

				for (place = 0; place < vantage.place_count; place++)
				{
					real_point3d at = { vantage.places[place].x, vantage.places[place].y, vantage.places[place].z };
					real distance = vantage_distance(&at, &equipment->position);

					if (place >= first || distance < 2.0f)
						nearest = MIN(nearest, distance);
				}
				if (nearest > best_distance && nearest > 6.0f)
				{
					best_distance = nearest;
					best = index;
				}
			}
			if (best == NONE)
				break;
			add_place(&TAG_BLOCK_GET_ELEMENT(&scenario->netgame_equipment, best, struct scenario_netgame_equipment)->position,
				1.0f, "item");
		}
	}
}

static void choose_points(void)
{
	long index, place;

	for (index = 0; index < vantage.candidate_count; index++)
		ae_vantage_score(&vantage.candidates[index], vantage.places, (int)vantage.place_count);
	vantage.count = ae_vantage_choose(vantage.candidates, vantage.candidate_count, vantage.points);
	for (index = 0; index < vantage.count; index++)
	{
		struct ae_vantage_spot const *spot = &vantage.points[index];
		char seen[200];
		int length = 0;

		seen[0] = 0;
		for (place = 0; place < vantage.place_count && length < (int)sizeof(seen) - 24; place++)
			if (spot->sees & (1UL << place))
			{
				real_point3d a = { spot->x, spot->y, spot->z }, b = { vantage.places[place].x, vantage.places[place].y, vantage.places[place].z };

				length += snprintf(seen + length, sizeof(seen) - (size_t)length, "%s%s %.0f", length ? ", " : "",
					vantage.place_names[place], (double)vantage_distance(&a, &b));
			}
		platform_log("bots: vantage spot %ld at (%.1f %.1f %.1f): %.1f above its surroundings, open %d/%d, sees %d of %ld places (%s); score %.1f",
			index + 1, (double)spot->x, (double)spot->y, (double)spot->z, (double)spot->height, (int)spot->open, AE_VANTAGE_OPEN_RAYS,
			(int)spot->sees_count, vantage.place_count, seen, (double)spot->score);
	}
	/* (the map's plan, one line: the places, the spots and the lanes found) */
	platform_log("bots: map plan: %ld vantage spots of %d candidates (%ld places, %ld rays), %ld side lanes", vantage.count,
		vantage.candidate_count, vantage.place_count, vantage.rays, ae_bots_lanes_count());
}

static void finish(char const *why)
{
	if (why)
		platform_log("bots: map plan: no vantage spots (%s), %ld side lanes", why, ae_bots_lanes_count());
	ae_vantage_grid_end(&vantage.grid);
	vantage.phase = PHASE_DONE;
	vantage.ready = TRUE;
	/* (the walkable graph is let go: nothing reads it after the spots) */
	ae_bots_lanes_release_graph();
}

/* a step of the map's vantage spots; TRUE once made (or none) */
boolean ae_bots_vantage_update(void)
{
	long now = game_time_get();
	struct ae_bots_graph const *graph;

	if (now == vantage.update_tick)
		return vantage.ready;
	vantage.update_tick = now;
	if (vantage.ready || !ae_bots_vantage_setting() || !ae_bots_lanes_ready())
		return vantage.ready;
	graph = ae_bots_lanes_graph();
	switch (vantage.phase)
	{
	case PHASE_IDLE:
		if (!graph)
		{
			finish(ae_bots_lanes_has_bases() ? "no walkable graph" : "no bases, no graph reached from them");
			break;
		}
		find_places();
		if (vantage.place_count < 2)
		{
			finish("fewer than two places");
			break;
		}
		vantage.bounds_x0 = vantage.bounds_y0 = REAL_MAX;
		vantage.bounds_x1 = vantage.bounds_y1 = -REAL_MAX;
		vantage.cursor = 0;
		vantage.phase = PHASE_BOUNDS;
		break;
	case PHASE_BOUNDS:
	{
		long last = MIN(vantage.cursor + NODES_A_TICK * 4, graph->node_count);

		for (; vantage.cursor < last; vantage.cursor++)
		{
			struct ae_lanes_node const *node = &graph->nodes[vantage.cursor];

			vantage.bounds_x0 = MIN(vantage.bounds_x0, node->x);
			vantage.bounds_y0 = MIN(vantage.bounds_y0, node->y);
			vantage.bounds_x1 = MAX(vantage.bounds_x1, node->x);
			vantage.bounds_y1 = MAX(vantage.bounds_y1, node->y);
		}
		if (vantage.cursor >= graph->node_count)
		{
			if (!ae_vantage_grid_begin(&vantage.grid, vantage.bounds_x0, vantage.bounds_y0, vantage.bounds_x1, vantage.bounds_y1,
				GRID_CELL, GRID_MAXIMUM))
			{
				finish("no memory for the height grid");
				break;
			}
			vantage.cursor = 0;
			vantage.phase = PHASE_GRID;
		}
		break;
	}
	case PHASE_GRID:
	{
		long last = MIN(vantage.cursor + NODES_A_TICK * 4, graph->node_count);

		for (; vantage.cursor < last; vantage.cursor++)
		{
			struct ae_lanes_node const *node = &graph->nodes[vantage.cursor];

			if (node->cost >= REAL_MAX)
				continue;
			ae_vantage_grid_add(&vantage.grid, node->x, node->y, node->z, graph->area[vantage.cursor]);
		}
		if (vantage.cursor >= graph->node_count)
		{
			vantage.cursor = 0;
			vantage.candidate_count = 0;
			vantage.phase = PHASE_CANDIDATES;
		}
		break;
	}
	case PHASE_CANDIDATES:
	{
		long last = MIN(vantage.cursor + NODES_A_TICK, graph->node_count);

		for (; vantage.cursor < last; vantage.cursor++)
		{
			struct ae_lanes_node const *node = &graph->nodes[vantage.cursor];
			struct ae_vantage_spot spot;

			/* (reached from the bases, flat, room to stand) */
			if (node->cost >= REAL_MAX || graph->normal_k[vantage.cursor] < 0.8f || graph->area[vantage.cursor] < 0.25f)
				continue;
			memset(&spot, 0, sizeof(spot));
			spot.x = node->x;
			spot.y = node->y;
			spot.z = node->z;
			spot.surface = graph->surface_index[vantage.cursor];
			spot.height = ae_vantage_grid_height(&vantage.grid, node->x, node->y, node->z);
			spot.first_score = ae_vantage_first_score(spot.height, vantage.places, (int)vantage.place_count, node->x, node->y, node->z);
			ae_vantage_consider(vantage.candidates, &vantage.candidate_count, &spot);
		}
		if (vantage.cursor >= graph->node_count)
		{
			ae_vantage_grid_end(&vantage.grid);
			vantage.cursor = 0;
			vantage.phase = PHASE_RAYS;
		}
		break;
	}
	case PHASE_RAYS:
	{
		long rays_a_spot = vantage.place_count + AE_VANTAGE_OPEN_RAYS;
		long total = rays_a_spot * vantage.candidate_count;
		long rays;

		for (rays = 0; rays < RAYS_A_TICK && vantage.cursor < total; vantage.cursor++)
		{
			struct ae_vantage_spot *spot = &vantage.candidates[vantage.cursor / rays_a_spot];
			long ray = vantage.cursor % rays_a_spot;
			real_point3d eye, to;

			eye.x = spot->x;
			eye.y = spot->y;
			eye.z = spot->z + EYE_HEIGHT;
			if (ray < vantage.place_count)
			{
				to.x = vantage.places[ray].x;
				to.y = vantage.places[ray].y;
				to.z = vantage.places[ray].z;
				if (vantage_distance(&eye, &to) > AE_VANTAGE_SIGHT || vantage_distance(&eye, &to) < 1.0f)
					continue;
				rays++;
				if (ray_clear(&eye, &to))
					spot->sees |= 1UL << ray;
			}
			else
			{
				real angle = (real)(ray - vantage.place_count) * 2.0f * 3.14159265f / (real)AE_VANTAGE_OPEN_RAYS;

				to = eye;
				to.x += (real)cos(angle) * OPEN_LENGTH;
				to.y += (real)sin(angle) * OPEN_LENGTH;
				rays++;
				if (ray_clear(&eye, &to))
					spot->open++;
			}
		}
		if (vantage.cursor >= total)
		{
			choose_points();
			finish(NULL);
		}
		break;
	}
	case PHASE_DONE:
		break;
	}
	return vantage.ready;
}
