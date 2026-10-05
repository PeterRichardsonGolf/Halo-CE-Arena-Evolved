/*
RENDER_SPAWN_MARKERS.C

port: the gametype's TRAINING's spawn markers, as Halo 1: NHE's Training
mode's green floor markers (its spawn_marker scenery, created by its map
scripts), on any map: one at each player starting location the game type
uses (scenario players matching game_engine_matches_game_type, as
game_engine_get_starting_location_rating takes them), GREEN for every spawn
as NHE's (team colours are for later).

Each is a flat ring with a chevron in it pointing the way the spawn faces,
lying on the floor (a short collision ray down from the spawn point finds
it and its slope) SPAWN_MARKER_HEIGHT over it, drawn in the world: depth
tested against what the view has drawn, so walls, floors and objects hide
it as they would NHE's scenery, and the first person weapon and the HUD
(drawn after it) stay over it. Within SPAWN_MARKER_RANGE world units of the
view's camera, fading out over the last SPAWN_MARKER_FADE, and not behind
it. Every marker a view shows is built into one array (each fading through
its vertices' alpha) and drawn with one call through the debug geometry
path's non-opaque triangles (rasterizer_debug_draw_triangles_now): one
dynamic vertex buffer a view, alpha blended, no texture, so no tag is
needed.

Drawn per local player's view from render_window (render.c), whether the
player is alive or not (as scenery is), while item_timers_training_shown:
a TRAINING game, not on NHE's maps (their scripts place their own), not
during the PRE-GAME COUNTDOWN, not once the game is over; and not in a
cinematic. The log has each view's markers going on (with how many are in
range then) and off as those rules change. Display only; nothing is
networked.
*/

/* ---------- headers */

#include "cseries/cseries.h"

#include "bitmaps/bitmap_color_conversion.h"
#include "cutscene/cinematics.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/item_timers.h"
#include "math/real_math.h"
#include "networking/network_connection.h"
#include "physics/collisions.h"
#include "physics/collision_usage.h"
#include "rasterizer/rasterizer.h"
#include "render/render.h"
#include "render/render_spawn_markers.h"
#include "scenario/scenario.h"
#include "scenario/scenario_definitions.h"

/* ---------- constants */

#define SPAWN_MARKER_ALPHA 0.75f
#define SPAWN_MARKER_OUTER_RADIUS 0.4f
#define SPAWN_MARKER_INNER_RADIUS 0.28f
#define SPAWN_MARKER_SEGMENTS 16
#define SPAWN_MARKER_CHEVRON_TRIANGLES 2
#define SPAWN_MARKER_TRIANGLES (2 * SPAWN_MARKER_SEGMENTS + SPAWN_MARKER_CHEVRON_TRIANGLES)
#define SPAWN_MARKER_VERTICES (SPAWN_MARKER_TRIANGLES * NUMBER_OF_VERTICES_PER_TRIANGLE)

/* the most markers a view draws (its one draw's 48 * 102 vertices): within
a window's share of the frame's debug vertices in a four-way split screen
(24576 / 4, rasterizer_xbox_draw_primitives.c), with room left for the
debug geometry's own; more spawns than that within SPAWN_MARKER_RANGE are
left out (the latest) */
#define SPAWN_MARKER_MAXIMUM_DRAWN 48

/* the floor is looked for from this far over the spawn point to this far
under it */
#define SPAWN_MARKER_FLOOR_ABOVE 0.5f
#define SPAWN_MARKER_FLOOR_BELOW 1.0f

/* how far a marker reaches from its spawn point, at most: the floor under
it, the marker's height over that and the ring's radius (for the test of
spawns behind the camera) */
#define SPAWN_MARKER_EXTENT (SPAWN_MARKER_FLOOR_BELOW + SPAWN_MARKER_HEIGHT + SPAWN_MARKER_OUTER_RADIUS)

/* ---------- prototypes */

void platform_log(char const *format, ...);

static void spawn_marker_floor(real_point3d const *position, real_point3d *floor, real_vector3d *normal);
static void spawn_marker_point(real_point3d const *center, real_vector3d const *forward,
	real_vector3d const *left, real x, real y, real_point3d *point);
static long spawn_marker_build(struct player_starting_location const *location, real_point3d *points);

/* ---------- globals */

/* whether each view's markers are on (the rules for them hold), for the
log; off at a new map */
static boolean spawn_markers_shown[MAXIMUM_NUMBER_OF_LOCAL_PLAYERS];

/* the ring's segments' cos and sin, from angle 0 round to 2 pi */
static real spawn_marker_ring[SPAWN_MARKER_SEGMENTS + 1][2];
static boolean spawn_marker_ring_ready = FALSE;

/* one view's markers, drawn in one call */
static real_point3d spawn_marker_points[SPAWN_MARKER_MAXIMUM_DRAWN * SPAWN_MARKER_VERTICES];
static pixel32 spawn_marker_colors[SPAWN_MARKER_MAXIMUM_DRAWN * SPAWN_MARKER_VERTICES];

/* ---------- public code */

void render_spawn_markers_initialize_for_new_map(
	void)
{
	csmemset(spawn_markers_shown, 0, sizeof(spawn_markers_shown));

	return;
}

void render_spawn_markers(
	short local_player_index)
{
	struct scenario *scenario;
	boolean shown;
	short in_range = 0;
	long vertex_count = 0;
	long index;

	if (local_player_index < 0 || local_player_index >= MAXIMUM_NUMBER_OF_LOCAL_PLAYERS)
		return;

	shown = game_engine_running() && !cinematic_in_progress() && item_timers_training_shown();
	if (shown)
	{
		if (!spawn_marker_ring_ready)
		{
			short segment;

			for (segment = 0; segment <= SPAWN_MARKER_SEGMENTS; segment++)
			{
				real angle = 2.0f * _pi * (real)segment / (real)SPAWN_MARKER_SEGMENTS;

				spawn_marker_ring[segment][0] = (real)cos(angle);
				spawn_marker_ring[segment][1] = (real)sin(angle);
			}
			spawn_marker_ring_ready = TRUE;
		}

		scenario = global_scenario_get();
		for (index = 0; index < scenario->players.count; index++)
		{
			struct player_starting_location const *location =
				TAG_BLOCK_GET_ELEMENT(&scenario->players, index, struct player_starting_location);
			real_argb_color color;
			real_vector3d delta;
			real distance;
			pixel32 pixel;
			long built;
			long vertex_index;

			if (!game_engine_matches_game_type(location->game_types))
				continue;
			delta.i = location->position.x - render.camera.position.x;
			delta.j = location->position.y - render.camera.position.y;
			delta.k = location->position.z - render.camera.position.z;
			distance = magnitude3d(&delta);
			if (distance >= SPAWN_MARKER_RANGE)
				continue;
			in_range++;
			/* (none behind the camera: no ray, no triangles) */
			if (dot_product3d(&render.camera.forward, &delta) < -SPAWN_MARKER_EXTENT)
				continue;
			if (vertex_count + SPAWN_MARKER_VERTICES > (long)NUMBEROF(spawn_marker_points))
				continue;

			built = spawn_marker_build(location, &spawn_marker_points[vertex_count]);
			if (!built)
				continue;

			/* GREEN, fading out toward the range's limit */
			color.alpha = SPAWN_MARKER_ALPHA;
			if (distance > SPAWN_MARKER_RANGE - SPAWN_MARKER_FADE)
				color.alpha *= (SPAWN_MARKER_RANGE - distance) / SPAWN_MARKER_FADE;
			color.red = 0.15f;
			color.green = 1.0f;
			color.blue = 0.25f;
			pixel = real_argb_color_to_pixel32(&color);
			for (vertex_index = 0; vertex_index < built; vertex_index++)
				spawn_marker_colors[vertex_count + vertex_index] = pixel;
			vertex_count += built;
		}

		if (vertex_count > 0)
		{
			rasterizer_debug_draw_triangles_now(spawn_marker_points, spawn_marker_colors,
				vertex_count / NUMBER_OF_VERTICES_PER_TRIANGLE);
		}
	}

	/* (the log has each view's markers going on, with how many are in
	range then, and off, as the rules for them change) */
	if (shown != spawn_markers_shown[local_player_index])
	{
		spawn_markers_shown[local_player_index] = shown;
		if (shown)
			platform_log("spawn markers: view %d on at tick %ld (%d in range)", local_player_index, game_time_get(), in_range);
		else
			platform_log("spawn markers: view %d off at tick %ld", local_player_index, game_time_get());
	}

	return;
}

/* ---------- private code */

/* a spawn's marker's triangles into points (SPAWN_MARKER_VERTICES of them):
returns how many points, 0 for none (a spawn facing straight into its
floor) */
static long spawn_marker_build(
	struct player_starting_location const *location,
	real_point3d *points)
{
	real_point3d center;
	real_vector3d normal;
	real_vector3d forward;
	real_vector3d left;
	real length;
	short segment;
	long point_index = 0;

	spawn_marker_floor(&location->position, &center, &normal);
	center.x += normal.i * SPAWN_MARKER_HEIGHT;
	center.y += normal.j * SPAWN_MARKER_HEIGHT;
	center.z += normal.k * SPAWN_MARKER_HEIGHT;

	/* the spawn's facing, along the floor */
	vector3d_from_angle(&forward, location->facing);
	length = dot_product3d(&forward, &normal);
	forward.i -= normal.i * length;
	forward.j -= normal.j * length;
	forward.k -= normal.k * length;
	if (normalize3d(&forward) == 0.0f)
		return 0;
	cross_product3d(&normal, &forward, &left);

	/* the ring */
	for (segment = 0; segment < SPAWN_MARKER_SEGMENTS; segment++)
	{
		real cos0 = spawn_marker_ring[segment][0];
		real sin0 = spawn_marker_ring[segment][1];
		real cos1 = spawn_marker_ring[segment + 1][0];
		real sin1 = spawn_marker_ring[segment + 1][1];
		real_point3d outer0;
		real_point3d outer1;
		real_point3d inner0;
		real_point3d inner1;

		spawn_marker_point(&center, &forward, &left,
			cos0 * SPAWN_MARKER_OUTER_RADIUS, sin0 * SPAWN_MARKER_OUTER_RADIUS, &outer0);
		spawn_marker_point(&center, &forward, &left,
			cos1 * SPAWN_MARKER_OUTER_RADIUS, sin1 * SPAWN_MARKER_OUTER_RADIUS, &outer1);
		spawn_marker_point(&center, &forward, &left,
			cos0 * SPAWN_MARKER_INNER_RADIUS, sin0 * SPAWN_MARKER_INNER_RADIUS, &inner0);
		spawn_marker_point(&center, &forward, &left,
			cos1 * SPAWN_MARKER_INNER_RADIUS, sin1 * SPAWN_MARKER_INNER_RADIUS, &inner1);
		points[point_index++] = outer0;
		points[point_index++] = outer1;
		points[point_index++] = inner1;
		points[point_index++] = outer0;
		points[point_index++] = inner1;
		points[point_index++] = inner0;
	}

	/* the chevron: tip, wings and the notch between them */
	{
		real_point3d tip;
		real_point3d left_wing;
		real_point3d right_wing;
		real_point3d notch;

		spawn_marker_point(&center, &forward, &left, 0.22f, 0.0f, &tip);
		spawn_marker_point(&center, &forward, &left, -0.12f, 0.16f, &left_wing);
		spawn_marker_point(&center, &forward, &left, -0.12f, -0.16f, &right_wing);
		spawn_marker_point(&center, &forward, &left, -0.02f, 0.0f, &notch);
		points[point_index++] = tip;
		points[point_index++] = left_wing;
		points[point_index++] = notch;
		points[point_index++] = tip;
		points[point_index++] = notch;
		points[point_index++] = right_wing;
	}

	return point_index;
}

/* the floor under (or a little over) a spawn point, and its normal; the
point itself and up where there is none */
static void spawn_marker_floor(
	real_point3d const *position,
	real_point3d *floor,
	real_vector3d *normal)
{
	struct collision_result result;
	real_point3d start;
	real_vector3d vector;
	boolean pushed = FALSE;

	start = *position;
	start.z += SPAWN_MARKER_FLOOR_ABOVE;
	vector.i = 0.0f;
	vector.j = 0.0f;
	vector.k = -(SPAWN_MARKER_FLOOR_ABOVE + SPAWN_MARKER_FLOOR_BELOW);

	/* (counted as the nav points' line of sight tests are) */
	if (global_current_collision_user_depth < MAXIMUM_COLLISION_USER_STACK_DEPTH)
	{
		global_current_collision_users[global_current_collision_user_depth++] = 20;
		pushed = TRUE;
	}
	if (collision_test_vector(FLAG(_collision_test_front_facing_surfaces_bit) | FLAG(_collision_test_structure_bit),
			&start, &vector, NONE, &result) &&
		result.plane.n.k > 0.5f)
	{
		*floor = result.point;
		*normal = result.plane.n;
	}
	else
	{
		*floor = *position;
		normal->i = 0.0f;
		normal->j = 0.0f;
		normal->k = 1.0f;
	}
	if (pushed)
		--global_current_collision_user_depth;

	return;
}

/* a point on the marker's plane, x along the spawn's facing and y to its left */
static void spawn_marker_point(
	real_point3d const *center,
	real_vector3d const *forward,
	real_vector3d const *left,
	real x,
	real y,
	real_point3d *point)
{
	point->x = center->x + forward->i * x + left->i * y;
	point->y = center->y + forward->j * x + left->j * y;
	point->z = center->z + forward->k * x + left->k * y;

	return;
}
