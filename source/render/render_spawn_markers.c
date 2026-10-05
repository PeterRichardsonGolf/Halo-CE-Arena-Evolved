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
view's camera, fading out over the last SPAWN_MARKER_FADE. Drawn through
the debug geometry path's non-opaque triangles
(rasterizer_debug_draw_triangles_now): one colour, alpha blended, no
texture, so no tag is needed.

Drawn per local player's view from render_window (render.c), whether the
player is alive or not (as scenery is), while item_timers_training_shown:
a TRAINING game, not on NHE's maps (their scripts place their own), not
during the PRE-GAME COUNTDOWN, not once the game is over; and not in a
cinematic. Display only; nothing is networked.
*/

/* ---------- headers */

#include "cseries/cseries.h"

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

/* the floor is looked for from this far over the spawn point to this far
under it */
#define SPAWN_MARKER_FLOOR_ABOVE 0.5f
#define SPAWN_MARKER_FLOOR_BELOW 1.0f

/* ---------- prototypes */

void platform_log(char const *format, ...);

static void spawn_marker_floor(real_point3d const *position, real_point3d *floor, real_vector3d *normal);
static void spawn_marker_point(real_point3d const *center, real_vector3d const *forward,
	real_vector3d const *left, real x, real y, real_point3d *point);

/* ---------- globals */

/* whether each view drew any markers last, for the log */
static boolean spawn_markers_shown[MAXIMUM_NUMBER_OF_LOCAL_PLAYERS];

/* ---------- public code */

void render_spawn_markers(
	short local_player_index)
{
	struct scenario *scenario;
	short drawn = 0;
	long index;

	if (local_player_index < 0 || local_player_index >= MAXIMUM_NUMBER_OF_LOCAL_PLAYERS)
		return;

	if (game_engine_running() && !cinematic_in_progress() && item_timers_training_shown())
	{
		scenario = global_scenario_get();
		for (index = 0; index < scenario->players.count; index++)
		{
			struct player_starting_location const *location =
				TAG_BLOCK_GET_ELEMENT(&scenario->players, index, struct player_starting_location);
			real_point3d points[SPAWN_MARKER_TRIANGLES * NUMBER_OF_VERTICES_PER_TRIANGLE];
			real_argb_color color;
			real_point3d center;
			real_vector3d normal;
			real_vector3d forward;
			real_vector3d left;
			real_vector3d delta;
			real distance;
			real length;
			short segment;
			long point_index = 0;

			if (!game_engine_matches_game_type(location->game_types))
				continue;
			delta.i = location->position.x - render.camera.position.x;
			delta.j = location->position.y - render.camera.position.y;
			delta.k = location->position.z - render.camera.position.z;
			distance = magnitude3d(&delta);
			if (distance >= SPAWN_MARKER_RANGE)
				continue;

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
				continue;
			cross_product3d(&normal, &forward, &left);

			/* the ring */
			for (segment = 0; segment < SPAWN_MARKER_SEGMENTS; segment++)
			{
				real angle0 = 2.0f * _pi * (real)segment / (real)SPAWN_MARKER_SEGMENTS;
				real angle1 = 2.0f * _pi * (real)(segment + 1) / (real)SPAWN_MARKER_SEGMENTS;
				real cos0 = (real)cos(angle0);
				real sin0 = (real)sin(angle0);
				real cos1 = (real)cos(angle1);
				real sin1 = (real)sin(angle1);
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

			/* GREEN, fading out toward the range's limit */
			color.alpha = SPAWN_MARKER_ALPHA;
			if (distance > SPAWN_MARKER_RANGE - SPAWN_MARKER_FADE)
				color.alpha *= (SPAWN_MARKER_RANGE - distance) / SPAWN_MARKER_FADE;
			color.red = 0.15f;
			color.green = 1.0f;
			color.blue = 0.25f;
			rasterizer_debug_draw_triangles_now(points, SPAWN_MARKER_TRIANGLES, &color);
			drawn++;
		}
	}

	/* (the log has each view's markers going on, with how many are in
	range then, and off) */
	if ((drawn > 0) != spawn_markers_shown[local_player_index])
	{
		spawn_markers_shown[local_player_index] = drawn > 0;
		if (drawn > 0)
			platform_log("spawn markers: view %d on at tick %ld (%d in range)", local_player_index, game_time_get(), drawn);
		else
			platform_log("spawn markers: view %d off at tick %ld", local_player_index, game_time_get());
	}

	return;
}

/* ---------- private code */

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
