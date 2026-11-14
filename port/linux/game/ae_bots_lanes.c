/*
AE_BOTS_LANES.C

The engine's side of the bots' side lanes (ae_bots_lanes.h; the rules are ae_bots_lanes_rules.c's). From halo-ce-vita
(GPL-3.0) via Halo Content Evolved (Watanoge/halo-content-evolved, AGPL-3.0-or-later): the walkable graph and the bases
are theirs, adapted here.

The map's walkable graph: a node each walkable surface of the structure (the surfaces the bots' own searches walk),
joined across the edges they share with other walkable surfaces, but those under a machine or a door (Sidewinder's blast
door between the bases, scenery that never opens) left out. Its two bases: the CTF flag stands, else (or when there is no
way between them) the teams' starting locations, else the two starting locations furthest apart; a map with none of
these has no lanes (the squads' LEFT and RIGHT keep their offsets). It is made a few hundred surfaces a tick as the bots'
first ticks of a game run, never all at the map's load, and the same map always gives the same graph and lanes.
*/

#include "cseries.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "objects/objects.h"
#include "scenario/scenario.h"
#include "scenario/scenario_definitions.h"
#include "structures/structure_bsp_definitions.h"
#include "physics/collision_bsp_definitions.h"
#include "physics/collisions.h"
#include "tag_files/tag_groups.h"
#include "tag_files/tag_files.h"

#include "ae_bots_lanes.h"

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void platform_log(char const *format, ...);
char const *config_string(char const *name);
int config_boolean(char const *name);

/* a pathfinding surface's flag: walkable */
#define WALKABLE_BIT 6
/* the most walkable surfaces a map's graph takes (Blood Gulch has 2807); more, and the map has no lanes */
#define GRAPH_MAXIMUM_NODES 32768
/* the corners of a surface looked at (its ring of edges) */
#define CORNERS 16
/* the work of a tick: surfaces looked at, a search's nodes taken, nodes placed across the direct route */
#define SURFACES_A_TICK 300
#define POPS_A_TICK 80
#define NODES_A_TICK 200
/* the machines (doors, lifts) whose ground the graph leaves out */
#define MACHINES 32
/* the base sets tried: the flag stands, the starting locations of the teams, the two furthest apart */
#define BASE_SETS 3

enum { PHASE_IDLE, PHASE_MACHINES, PHASE_NODES, PHASE_LINKS, PHASE_BASES, PHASE_BUILD, PHASE_DONE };

static struct
{
	int phase;
	boolean ready;
	boolean has_bases;
	boolean routes_auto, vantage_on, high_ground_on;
	long settings_tick, update_tick;
	struct structure_bsp const *structure;
	long surface_count;
	long *node_of_surface;
	long cursor, link_write, link_capacity, link_count;
	struct ae_bots_graph graph;
	long *link_first;
	long *links;
	long machine_count, machine_surfaces;
	real_point3d machine_centers[MACHINES];
	real machine_radii[MACHINES];
	int base_set;
	real_point3d bases[2];
	long base_nodes[2];
	boolean costs_made;
	struct ae_lanes_builder builder;
	boolean builder_live;
	struct ae_lanes_route route;
	struct ae_lanes_lane lanes[AE_LANES_SIDE_LANES];
	long lane_surfaces[AE_LANES_SIDE_LANES][AE_LANES_WAYPOINTS];
	long lane_count;
} lanes;

static real lane_distance2d(real_point3d const *a, real_point3d const *b)
{
	real dx = a->x - b->x, dy = a->y - b->y;

	return (real)sqrt(dx * dx + dy * dy);
}

static real lane_distance3d(real_point3d const *a, real_point3d const *b)
{
	real dx = a->x - b->x, dy = a->y - b->y, dz = a->z - b->z;

	return (real)sqrt(dx * dx + dy * dy + dz * dz);
}

long ae_bots_lanes_ground(real_point3d const *point, real_point3d *ground)
{
	struct collision_result collision;
	real_point3d start = *point;
	real_vector3d down = { 0.0f, 0.0f, -6.0f };

	start.z += 0.4f;
	if (collision_test_vector(FLAG(_collision_test_structure_bit) | FLAG(_collision_test_front_facing_surfaces_bit),
		&start, &down, NONE, &collision) && collision.type == _collision_result_structure)
	{
		if (ground)
			*ground = collision.point;
		return collision.surface_index;
	}
	return NONE;
}

static struct collision_bsp const *structure_bsp(void)
{
	struct structure_bsp const *structure = lanes.structure;

	if (!structure || structure->collision_bsp.count <= 0)
		return NULL;
	return TAG_BLOCK_GET_ELEMENT(&structure->collision_bsp, 0, struct collision_bsp);
}

static boolean walkable(long surface_index)
{
	byte const *flags = (byte const *)xbox_pointer(lanes.structure->pathfinding_surfaces.address);

	return surface_index >= 0 && surface_index < lanes.surface_count && TEST_FLAG(flags[surface_index], WALKABLE_BIT);
}

/* the surface's corners (at most CORNERS) from its ring of edges, and the surface across each edge; 0 for one not the bsp's */
static long corners_of(struct collision_bsp const *bsp, long surface_index, real_point3d *corners, long *neighbours)
{
	struct collision_surface const *surface;
	long edge_index, count = 0;

	if (surface_index < 0 || surface_index >= bsp->surfaces.count)
		return 0;
	surface = TAG_BLOCK_GET_ELEMENT(&bsp->surfaces, surface_index, struct collision_surface);
	edge_index = surface->first_edge_index;
	do
	{
		struct collision_edge const *edge;
		boolean right;

		if (edge_index < 0 || edge_index >= bsp->edges.count)
			return 0;
		edge = TAG_BLOCK_GET_ELEMENT(&bsp->edges, edge_index, struct collision_edge);
		right = edge->surface_indices[1] == surface_index;
		if (!VALID_INDEX(edge->vertex_indices[right], bsp->vertices.count))
			return 0;
		corners[count] = TAG_BLOCK_GET_ELEMENT(&bsp->vertices, edge->vertex_indices[right], struct collision_vertex)->point;
		neighbours[count] = edge->surface_indices[!right];
		count++;
		edge_index = edge->edge_indices[right];
	} while (edge_index != surface->first_edge_index && count < CORNERS);
	return count;
}

/* ---------- the graph's memory */

static void free_graph(void)
{
	/* (the game's free takes no NULL) */
	if (lanes.node_of_surface)
		free(lanes.node_of_surface);
	if (lanes.graph.nodes)
		free(lanes.graph.nodes);
	if (lanes.graph.area)
		free(lanes.graph.area);
	if (lanes.graph.normal_k)
		free(lanes.graph.normal_k);
	if (lanes.graph.surface_index)
		free(lanes.graph.surface_index);
	if (lanes.link_first)
		free(lanes.link_first);
	if (lanes.links)
		free(lanes.links);
	lanes.node_of_surface = NULL;
	memset(&lanes.graph, 0, sizeof(lanes.graph));
	lanes.link_first = NULL;
	lanes.links = NULL;
	lanes.link_capacity = 0;
	lanes.link_count = 0;
	if (lanes.builder_live)
		ae_lanes_end(&lanes.builder);
	lanes.builder_live = FALSE;
}

void ae_bots_lanes_release_graph(void)
{
	free_graph();
}

void ae_bots_lanes_reset(void)
{
	free_graph();
	memset(&lanes, 0, sizeof(lanes));
	lanes.phase = PHASE_IDLE;
	lanes.routes_auto = TRUE;
	lanes.vantage_on = TRUE;
	lanes.high_ground_on = TRUE;
	lanes.settings_tick = NONE;
	lanes.update_tick = NONE;
}

boolean ae_bots_lanes_ready(void)
{
	return lanes.ready;
}

boolean ae_bots_lanes_auto(void)
{
	return lanes.routes_auto;
}

boolean ae_bots_vantage_setting(void)
{
	return lanes.vantage_on;
}

boolean ae_bots_high_ground_setting(void)
{
	return lanes.high_ground_on;
}

long ae_bots_lanes_count(void)
{
	return lanes.lane_count;
}

struct ae_lanes_lane const *ae_bots_lanes_lane(int lane)
{
	return lane >= 0 && lane < AE_LANES_SIDE_LANES && lanes.lanes[lane].valid ? &lanes.lanes[lane] : NULL;
}

long ae_bots_lanes_waypoint_surface(int lane, int waypoint)
{
	return lane >= 0 && lane < AE_LANES_SIDE_LANES && waypoint >= 0 && waypoint < AE_LANES_WAYPOINTS ?
		lanes.lane_surfaces[lane][waypoint] : NONE;
}

struct ae_lanes_route const *ae_bots_lanes_route(void)
{
	return lanes.has_bases && lanes.route.count >= 2 ? &lanes.route : NULL;
}

boolean ae_bots_lanes_has_bases(void)
{
	return lanes.has_bases;
}

void ae_bots_lanes_bases(real_point3d *first, real_point3d *second)
{
	*first = lanes.bases[0];
	*second = lanes.bases[1];
}

struct ae_bots_graph const *ae_bots_lanes_graph(void)
{
	return lanes.costs_made && lanes.graph.nodes ? &lanes.graph : NULL;
}

/* ---------- the settings (read once a second: a menu or the config changes them at any time) */

static void read_settings(void)
{
	/* (the settings' rows carry their environment variables: HALO_BOT_ROUTES, HALO_BOT_VANTAGE, HALO_BOT_HIGH_GROUND) */
	lanes.routes_auto = strcmp(config_string("bots.routes"), "offset") != 0;
	lanes.vantage_on = config_boolean("bots.vantage") != 0;
	lanes.high_ground_on = config_boolean("bots.high_ground") != 0;
}

/* ---------- the walkable graph */

/* the graph begun: the walkable surfaces counted, the memory for them; FALSE for a map without (or with too many) */
static boolean begin_graph(void)
{
	struct structure_bsp const *structure = global_structure_bsp_get();
	struct collision_bsp const *bsp;
	byte const *flags;
	long index, count = 0;

	if (!structure || structure->collision_bsp.count <= 0 || structure->pathfinding_surfaces.count <= 0)
	{
		platform_log("bots: lanes: the level has no walkable surfaces: no lanes, no vantage spots");
		return FALSE;
	}
	lanes.structure = structure;
	bsp = structure_bsp();
	lanes.surface_count = MIN((long)bsp->surfaces.count, (long)structure->pathfinding_surfaces.count);
	flags = (byte const *)xbox_pointer(structure->pathfinding_surfaces.address);
	for (index = 0; index < lanes.surface_count; index++)
		if (TEST_FLAG(flags[index], WALKABLE_BIT))
			count++;
	if (count < 8 || count > GRAPH_MAXIMUM_NODES)
	{
		platform_log("bots: lanes: %ld walkable surfaces: no lanes, no vantage spots", count);
		return FALSE;
	}
	lanes.node_of_surface = (long *)malloc(sizeof(long) * (size_t)lanes.surface_count);
	lanes.graph.nodes = (struct ae_lanes_node *)malloc(sizeof(struct ae_lanes_node) * (size_t)count);
	lanes.graph.area = (float *)malloc(sizeof(float) * (size_t)count);
	lanes.graph.normal_k = (float *)malloc(sizeof(float) * (size_t)count);
	lanes.graph.surface_index = (long *)malloc(sizeof(long) * (size_t)count);
	lanes.link_first = (long *)malloc(sizeof(long) * (size_t)(count + 1));
	lanes.link_capacity = count * 6;
	lanes.links = (long *)malloc(sizeof(long) * (size_t)lanes.link_capacity);
	if (!lanes.node_of_surface || !lanes.graph.nodes || !lanes.graph.area || !lanes.graph.normal_k ||
		!lanes.graph.surface_index || !lanes.link_first || !lanes.links)
	{
		platform_log("bots: lanes: no memory for %ld walkable surfaces", count);
		free_graph();
		return FALSE;
	}
	for (index = 0; index < lanes.surface_count; index++)
		lanes.node_of_surface[index] = NONE;
	lanes.machine_surfaces = 0;
	lanes.graph.node_count = 0;
	lanes.link_count = 0;
	lanes.cursor = 0;
	return TRUE;
}

/* the machines, and the scenery that is a door, whose ground the graph leaves out */
static void find_machines(void)
{
	struct object_iterator iterator;

	lanes.machine_count = 0;
	object_iterator_new(&iterator, _object_mask_machine | _object_mask_scenery, 0);
	while (object_iterator_next(&iterator) && lanes.machine_count < MACHINES)
	{
		struct object_datum *object = object_get(iterator.index);
		char const *name = tag_get_name(object->definition_index);

		if (!object_try_and_get_and_verify_type(iterator.index, _object_mask_machine) && (!name || !strstr(name, "door")))
			continue;
		lanes.machine_centers[lanes.machine_count] = object->object.bounding_sphere_center;
		lanes.machine_radii[lanes.machine_count] = object->object.bounding_sphere_radius;
		lanes.machine_count++;
	}
}

/* a step of the nodes' middles, areas and slopes; TRUE when all are made */
static boolean nodes_step(void)
{
	struct collision_bsp const *bsp = structure_bsp();
	long last = MIN(lanes.cursor + SURFACES_A_TICK, lanes.surface_count);

	for (; lanes.cursor < last; lanes.cursor++)
	{
		long surface_index = lanes.cursor, count, corner;
		real_point3d corners[CORNERS];
		long neighbours[CORNERS];
		struct ae_lanes_node *node;
		real_vector3d normal = { 0.0f, 0.0f, 0.0f };
		real reach, area;
		real_point3d middle = { 0.0f, 0.0f, 0.0f };
		long index = lanes.graph.node_count;

		if (!walkable(surface_index))
			continue;
		count = corners_of(bsp, surface_index, corners, neighbours);
		if (count < 3)
			continue;
		for (corner = 0; corner < count; corner++)
		{
			middle.x += corners[corner].x / (real)count;
			middle.y += corners[corner].y / (real)count;
			middle.z += corners[corner].z / (real)count;
		}
		for (corner = 1; corner + 1 < count; corner++)
		{
			real_vector3d a, b, c;

			vector_from_points3d(&corners[0], &corners[corner], &a);
			vector_from_points3d(&corners[0], &corners[corner + 1], &b);
			cross_product3d(&a, &b, &c);
			normal.i += c.i;
			normal.j += c.j;
			normal.k += c.k;
		}
		/* (not under a machine: the surface reaching into its sphere) */
		reach = 0.0f;
		for (corner = 0; corner < count; corner++)
			reach = MAX(reach, lane_distance2d(&middle, &corners[corner]));
		for (corner = 0; corner < lanes.machine_count; corner++)
		{
			real_point3d const *center = &lanes.machine_centers[corner];
			real radius = lanes.machine_radii[corner];

			if (lane_distance2d(&middle, center) < radius * 0.8f + reach && middle.z > center->z - radius - 1.0f &&
				middle.z < center->z + radius)
				break;
		}
		if (corner < lanes.machine_count)
		{
			lanes.machine_surfaces++;
			continue;
		}
		area = 0.5f * (real)sqrt(normal.i * normal.i + normal.j * normal.j + normal.k * normal.k);
		node = &lanes.graph.nodes[index];
		memset(node, 0, sizeof(*node));
		node->x = middle.x;
		node->y = middle.y;
		node->z = middle.z;
		node->cost = FLT_MAX;
		lanes.graph.area[index] = area;
		lanes.graph.normal_k[index] = area > 0.0f ? (real)fabs(normal.k) * 0.5f / area : 0.0f;
		lanes.graph.surface_index[index] = surface_index;
		/* (its walkable neighbours' surfaces, made nodes' indices once all are made) */
		lanes.link_first[index] = lanes.link_count;
		for (corner = 0; corner < count; corner++)
		{
			if (!walkable(neighbours[corner]))
				continue;
			if (lanes.link_count >= lanes.link_capacity)
			{
				long capacity = lanes.link_capacity * 2;
				long *grown = (long *)realloc(lanes.links, sizeof(long) * (size_t)capacity);

				if (!grown)
					break;
				lanes.links = grown;
				lanes.link_capacity = capacity;
			}
			lanes.links[lanes.link_count++] = neighbours[corner];
		}
		lanes.node_of_surface[surface_index] = index;
		lanes.graph.node_count++;
	}
	if (lanes.cursor < lanes.surface_count)
		return FALSE;
	lanes.link_first[lanes.graph.node_count] = lanes.link_count;
	return TRUE;
}

/* a step of the nodes' neighbours' surfaces made nodes' indices (those left out dropped, the links packed down in place) */
static boolean links_step(void)
{
	long last = MIN(lanes.cursor + SURFACES_A_TICK * 4, lanes.graph.node_count);

	for (; lanes.cursor < last; lanes.cursor++)
	{
		long node_index = lanes.cursor;
		long first = lanes.link_first[node_index], end = lanes.link_first[node_index + 1], link;

		lanes.link_first[node_index] = lanes.link_write;
		for (link = first; link < end; link++)
		{
			long other = lanes.node_of_surface[lanes.links[link]];

			if (other != NONE)
				lanes.links[lanes.link_write++] = other;
		}
	}
	if (lanes.cursor < lanes.graph.node_count)
		return FALSE;
	lanes.link_first[lanes.graph.node_count] = lanes.link_write;
	lanes.link_count = lanes.link_write;
	return TRUE;
}

/* the node of the walkable surface under a point, else the nearest node's */
static long node_at(real_point3d const *point)
{
	long surface_index = ae_bots_lanes_ground(point, NULL);
	long index, best = NONE;
	real best_distance = REAL_MAX;

	if (surface_index >= 0 && surface_index < lanes.surface_count && lanes.node_of_surface[surface_index] != NONE)
		return lanes.node_of_surface[surface_index];
	for (index = 0; index < lanes.graph.node_count; index++)
	{
		real_point3d middle = { lanes.graph.nodes[index].x, lanes.graph.nodes[index].y, lanes.graph.nodes[index].z };
		real distance = lane_distance3d(&middle, point);

		if (distance < best_distance)
		{
			best_distance = distance;
			best = index;
		}
	}
	return best;
}

/* the map's two bases, by set: 0 the CTF flag stands (every stock map has them, in every gametype), 1 the teams' starting
locations' middles, 2 the two starting locations furthest apart; FALSE without. A set that is too near or short is given
up for the next */
static boolean find_bases(int set, real_point3d *bases)
{
	struct scenario *scenario = global_scenario_get();
	boolean found[2] = { FALSE, FALSE };
	real_point3d sums[2];
	long counts[2] = { 0, 0 };
	long index, other;
	real best = 0.0f;

	if (!scenario)
		return FALSE;
	if (set == 0)
	{
		for (index = 0; index < scenario->netgame_flags.count; index++)
		{
			struct scenario_netgame_flag *flag =
				TAG_BLOCK_GET_ELEMENT(&scenario->netgame_flags, index, struct scenario_netgame_flag);

			if (flag->type == _netgame_flag_ctf_flag && (flag->team_index == 0 || flag->team_index == 1) &&
				!found[flag->team_index])
			{
				bases[flag->team_index] = flag->position;
				found[flag->team_index] = TRUE;
			}
		}
		return found[0] && found[1] && lane_distance2d(&bases[0], &bases[1]) > 10.0f;
	}
	if (set == 1)
	{
		memset(sums, 0, sizeof(sums));
		for (index = 0; index < scenario->players.count; index++)
		{
			struct player_starting_location *location =
				TAG_BLOCK_GET_ELEMENT(&scenario->players, index, struct player_starting_location);

			if (location->team_index == 0 || location->team_index == 1)
			{
				sums[location->team_index].x += location->position.x;
				sums[location->team_index].y += location->position.y;
				sums[location->team_index].z += location->position.z;
				counts[location->team_index]++;
			}
		}
		if (counts[0] <= 0 || counts[1] <= 0)
			return FALSE;
		for (index = 0; index < 2; index++)
		{
			bases[index].x = sums[index].x / (real)counts[index];
			bases[index].y = sums[index].y / (real)counts[index];
			bases[index].z = sums[index].z / (real)counts[index];
		}
		return lane_distance2d(&bases[0], &bases[1]) > 10.0f;
	}
	for (index = 0; index < scenario->players.count; index++)
	{
		struct player_starting_location *a = TAG_BLOCK_GET_ELEMENT(&scenario->players, index, struct player_starting_location);

		for (other = index + 1; other < scenario->players.count; other++)
		{
			struct player_starting_location *b = TAG_BLOCK_GET_ELEMENT(&scenario->players, other, struct player_starting_location);
			real distance = lane_distance2d(&a->position, &b->position);

			if (distance > best)
			{
				best = distance;
				bases[0] = a->position;
				bases[1] = b->position;
			}
		}
	}
	return best > 10.0f;
}

/* ---------- making them: a step a tick */

static void log_route(void)
{
	char line[600];
	int length = 0, point;

	line[0] = 0;
	for (point = 0; point < lanes.route.count; point++)
	{
		int wrote = snprintf(line + length, sizeof(line) - (size_t)length, " (%.1f %.1f)", (double)lanes.route.x[point],
			(double)lanes.route.y[point]);

		/* (0.7.8: a point that did not fit ends the line; snprintf's count is what it wanted, not what it wrote) */
		if (wrote < 0 || wrote >= (int)sizeof(line) - length)
		{
			line[length] = 0;
			break;
		}
		length += wrote;
	}
	platform_log("bots: lanes: walkable graph of %ld surfaces (%ld links; %ld under %ld machines left out), bases (%.1f %.1f %.1f) "
		"and (%.1f %.1f %.1f); the direct route %.0f units:%s", lanes.graph.node_count, lanes.link_count, lanes.machine_surfaces,
		lanes.machine_count, (double)lanes.bases[0].x, (double)lanes.bases[0].y, (double)lanes.bases[0].z,
		(double)lanes.bases[1].x, (double)lanes.bases[1].y, (double)lanes.bases[1].z, (double)lanes.route.length, line);
}

static void log_lanes(void)
{
	int lane, waypoint;
	char line[400];
	int length;

	for (lane = 0; lane < AE_LANES_SIDE_LANES; lane++)
	{
		struct ae_lanes_lane const *found = &lanes.lanes[lane];

		if (!found->valid)
		{
			platform_log("bots: lanes: lane %d (%s side): none (%s; %.0f units, %.0f off the direct route at most)", lane + 1,
				lane == 0 ? "left" : "right", found->why ? found->why : "no way", (double)found->length, (double)found->offset);
			continue;
		}
		length = snprintf(line, sizeof(line), "bots: lanes: lane %d (%s side) by", lane + 1, lane == 0 ? "left" : "right");
		for (waypoint = 0; waypoint < found->waypoint_count; waypoint++)
			length += snprintf(line + length, sizeof(line) - (size_t)length, " (%.1f %.1f %.1f)", (double)found->waypoints[waypoint].x,
				(double)found->waypoints[waypoint].y, (double)found->waypoints[waypoint].z);
		platform_log("%s: %.0f units (the direct route %.0f), %.0f off it at most", line, (double)found->length,
			(double)lanes.route.length, (double)found->offset);
	}
}

/* the builder begun for the base set; FALSE when the set has no bases on walkable surfaces */
static boolean start_set(void)
{
	long a, b;

	while (lanes.base_set < BASE_SETS)
	{
		if (find_bases(lanes.base_set, lanes.bases))
		{
			a = node_at(&lanes.bases[0]);
			b = node_at(&lanes.bases[1]);
			if (a != NONE && b != NONE && a != b)
			{
				lanes.base_nodes[0] = a;
				lanes.base_nodes[1] = b;
				if (lanes.builder_live)
					ae_lanes_end(&lanes.builder);
				lanes.builder_live = ae_lanes_begin(&lanes.builder, lanes.graph.nodes, lanes.graph.node_count, lanes.link_first,
					lanes.links, a, b) == AE_LANES_RUNNING;
				if (lanes.builder_live)
					return TRUE;
				platform_log("bots: lanes: no memory for the lane search");
				return FALSE;
			}
		}
		lanes.base_set++;
	}
	return FALSE;
}

static void finish(char const *why)
{
	if (why)
		platform_log("bots: lanes: none (%s)", why);
	lanes.phase = PHASE_DONE;
	lanes.ready = TRUE;
}

/* a step of the map's lanes; TRUE once they are made (or the map has none) */
boolean ae_bots_lanes_update(void)
{
	long now = game_time_get();

	/* (once a tick, whoever asks) */
	if (now == lanes.update_tick)
		return lanes.phase == PHASE_DONE;
	lanes.update_tick = now;
	if (lanes.settings_tick == NONE || now < lanes.settings_tick || now - lanes.settings_tick >= TICKS_PER_SECOND)
	{
		lanes.settings_tick = now;
		read_settings();
	}
	switch (lanes.phase)
	{
	case PHASE_IDLE:
		lanes.phase = begin_graph() ? PHASE_MACHINES : PHASE_DONE;
		if (lanes.phase == PHASE_DONE)
			lanes.ready = TRUE;
		break;
	case PHASE_MACHINES:
		find_machines();
		lanes.phase = PHASE_NODES;
		break;
	case PHASE_NODES:
		if (nodes_step())
		{
			lanes.cursor = 0;
			lanes.link_write = 0;
			lanes.phase = PHASE_LINKS;
		}
		break;
	case PHASE_LINKS:
		if (links_step())
			lanes.phase = PHASE_BASES;
		break;
	case PHASE_BASES:
		if (lanes.graph.node_count < 8)
		{
			finish("too few walkable surfaces");
			break;
		}
		lanes.base_set = 0;
		if (!start_set())
		{
			finish("no bases found on the map: the squads' LEFT and RIGHT keep their offsets");
			break;
		}
		lanes.phase = PHASE_BUILD;
		break;
	case PHASE_BUILD:
	{
		int result = ae_lanes_step(&lanes.builder, POPS_A_TICK, NODES_A_TICK);

		if (result == AE_LANES_NO_WAY)
		{
			platform_log("bots: lanes: no way between the bases (%.1f %.1f %.1f) and (%.1f %.1f %.1f): others",
				(double)lanes.bases[0].x, (double)lanes.bases[0].y, (double)lanes.bases[0].z, (double)lanes.bases[1].x,
				(double)lanes.bases[1].y, (double)lanes.bases[1].z);
			lanes.base_set++;
			if (!start_set())
			{
				/* (what the last search reached from its first base is what the vantage spots are read from) */
				lanes.costs_made = TRUE;
				finish("no way between any bases found: the squads' LEFT and RIGHT keep their offsets");
			}
		}
		else if (result == AE_LANES_DONE)
		{
			int lane;

			memcpy(&lanes.route, &lanes.builder.route, sizeof(lanes.route));
			memcpy(lanes.lanes, lanes.builder.lanes, sizeof(lanes.lanes));
			lanes.lane_count = lanes.builder.lane_count;
			for (lane = 0; lane < AE_LANES_SIDE_LANES; lane++)
			{
				int waypoint;

				for (waypoint = 0; waypoint < AE_LANES_WAYPOINTS; waypoint++)
					lanes.lane_surfaces[lane][waypoint] = waypoint < lanes.lanes[lane].waypoint_count ?
						lanes.graph.surface_index[lanes.lanes[lane].waypoints[waypoint].node] : NONE;
			}
			lanes.has_bases = TRUE;
			lanes.costs_made = TRUE;
			log_route();
			log_lanes();
			ae_lanes_end(&lanes.builder);
			lanes.builder_live = FALSE;
			finish(NULL);
		}
		else if (result == AE_LANES_NO_MEMORY)
			finish("no memory");
		break;
	}
	case PHASE_DONE:
		return TRUE;
	}
	return lanes.phase == PHASE_DONE;
}
