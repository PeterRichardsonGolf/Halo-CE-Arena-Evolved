/*
RENDER_SPAWN_MARKERS.C

port: the gametype's TRAINING's spawn markers, as Halo 1: NHE's Training
mode's green floor markers (its spawn_marker scenery, created by its map
scripts), on any map: one at each player starting location the game type
uses (scenario players matching game_engine_matches_game_type, as
game_engine_get_starting_location_rating takes them), GREEN for every spawn
as NHE's; or with SPAWN HEAT (display.spawn_heat, spawn_heat.c) each
coloured by how likely it is to be picked next for the view's player (or
an enemy): by its chance over the likeliest's, t, from cold blue-grey
(t 0) through green and yellow to hot orange-red (t 1), alpha 0.35 + 0.5 t,
the one likeliest pulsing slowly (near uniform chances, as free for all's
mostly are: all the scale's middle, none pulsing); fading as the camera
nears one (one underfoot stays faint); dark red with a cross through the chevron
(alpha 0.3) for one that can't be picked now (an enemy within 2 world
units, a teammate on it, a vehicle on it), the other team's CTF spawns
faint in their team's colour (alpha 0.15). A player's real spawn flashes:
a bright ring growing out from its spawn's for 1.5 seconds (the team's
colour, white without teams).

Each is a flat ring with a chevron in it pointing the way the spawn faces,
lying on the floor (a short collision ray down from the spawn point finds
it and its slope: the map's structure, or scenery or a machine the spawn
stands on, such as a crate, a platform or a bridge) SPAWN_MARKER_HEIGHT
over it, drawn in the world: depth tested against what the view has drawn,
so walls, floors and objects hide it as they would NHE's scenery, and the
first person weapon and the HUD (drawn after it) stay over it. Within
SPAWN_MARKER_RANGE world units of the view's camera, fading out over the
last SPAWN_MARKER_FADE, and not behind it. Every marker a view shows is
built into one array (each fading through its vertices' alpha) and drawn
with one call through the debug geometry path's non-opaque triangles
(rasterizer_debug_draw_triangles_now): one dynamic vertex buffer a view,
alpha blended, no texture, so no tag is needed.

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
#include "game/spawn_heat.h"
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
/* SPAWN HEAT's cross through a spawn that can't be picked: two bars */
#define SPAWN_MARKER_CROSS_TRIANGLES 4
#define SPAWN_MARKER_CROSS_HALF_LENGTH 0.26f
#define SPAWN_MARKER_CROSS_HALF_WIDTH 0.035f
/* a real spawn's flash: a ring (as the marker's) growing from the marker's
size to this, as wide as this, fading out */
#define SPAWN_MARKER_FLASH_TRIANGLES (2 * SPAWN_MARKER_SEGMENTS)
#define SPAWN_MARKER_FLASH_RADIUS 1.6f
#define SPAWN_MARKER_FLASH_WIDTH 0.12f
#define SPAWN_MARKER_MAXIMUM_FLASHES 4

/* the most vertices a view draws (its one draw's: 48 plain markers of 102):
within a window's share of the frame's debug vertices in a four-way split
screen (24576 / 4, rasterizer_xbox_draw_primitives.c), with room left for
the debug geometry's own; more spawns than fit within SPAWN_MARKER_RANGE
are left out (the latest; SPAWN HEAT's flashes go first) */
#define SPAWN_MARKER_MAXIMUM_DRAWN 48
#define SPAWN_MARKER_MAXIMUM_VERTICES (SPAWN_MARKER_MAXIMUM_DRAWN * SPAWN_MARKER_VERTICES)

/* SPAWN HEAT's colours: the scale's stops (t, red, green, blue), dark red
for a spawn that can't be picked, the teams' (red 0, blue 1) */
static real const spawn_heat_scale[][4] =
{
	{ 0.0f, 0.55f, 0.66f, 0.85f },	/* cold blue-grey */
	{ 0.4f, 0.15f, 0.95f, 0.30f },	/* green */
	{ 0.7f, 1.00f, 0.88f, 0.10f },	/* yellow */
	{ 1.0f, 1.00f, 0.30f, 0.05f },	/* hot orange-red */
};
#define SPAWN_HEAT_ALPHA_COLD 0.35f
#define SPAWN_HEAT_ALPHA_HOT_EXTRA 0.5f
#define SPAWN_HEAT_ZERO_ALPHA 0.3f
#define SPAWN_HEAT_OTHER_TEAM_ALPHA 0.15f
/* the likeliest spawn's pulse: its alpha from all of it to this part and
back, once in this many ticks (1.6 s) */
#define SPAWN_HEAT_PULSE_LOW 0.55f
#define SPAWN_HEAT_PULSE_TICKS 48
/* SPAWN HEAT's near fade: a marker (and a flash) the view's camera is near
fades, from all of its alpha this far from its spawn point to this part
of it this near (a standing player's eyes are about 0.6 over the spawn
they stand on): one underfoot stays faintly there, not over the view */
#define SPAWN_HEAT_NEAR_FULL 4.0f
#define SPAWN_HEAT_NEAR_FAINT 1.5f
#define SPAWN_HEAT_NEAR_FAINT_PART 0.2f
/* near uniform chances (spawn_heat_near_uniform: free for all, mostly):
every spawn that can be picked is drawn as this point of the scale, none
pulsing */
#define SPAWN_HEAT_UNIFORM_T 0.5f

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
static long spawn_marker_build(struct player_starting_location const *location, boolean cross, real_point3d *points);
static long spawn_marker_flash_build(struct player_starting_location const *location, real age, real_point3d *points);
static void spawn_marker_heat_color(struct spawn_heat_view const *heat, long index, real_argb_color *color,
	boolean *cross);
static void spawn_marker_team_color(short team_index, real_argb_color *color);
static real spawn_marker_near_fade(real distance);

/* ---------- globals */

/* whether each view's markers are on (the rules for them hold), for the
log; off at a new map */
static boolean spawn_markers_shown[MAXIMUM_NUMBER_OF_LOCAL_PLAYERS];

/* the ring's segments' cos and sin, from angle 0 round to 2 pi */
static real spawn_marker_ring[SPAWN_MARKER_SEGMENTS + 1][2];
static boolean spawn_marker_ring_ready = FALSE;

/* one view's markers, drawn in one call */
static real_point3d spawn_marker_points[SPAWN_MARKER_MAXIMUM_VERTICES];
static pixel32 spawn_marker_colors[SPAWN_MARKER_MAXIMUM_VERTICES];

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
	struct spawn_heat_view const *heat;
	struct spawn_heat_flash flashes[SPAWN_HEAT_MAXIMUM_FLASHES];
	short flash_count;
	short flash_index;
	short flashes_drawn = 0;
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
		heat = spawn_heat_get_view(local_player_index);

		/* SPAWN HEAT's flashes of real spawns, first (those in range, at
		most SPAWN_MARKER_MAXIMUM_FLASHES) */
		flash_count = spawn_heat_get_flashes(flashes, SPAWN_HEAT_MAXIMUM_FLASHES);
		for (flash_index = 0; flash_index < flash_count; flash_index++)
		{
			struct player_starting_location const *location;
			real_argb_color color;
			real distance;
			pixel32 pixel;
			long built;
			long vertex_index;

			if (flashes[flash_index].spawn_index >= scenario->players.count)
				continue;
			location = TAG_BLOCK_GET_ELEMENT(&scenario->players, flashes[flash_index].spawn_index,
				struct player_starting_location);
			distance = distance3d(&location->position, &render.camera.position);
			if (distance >= SPAWN_MARKER_RANGE || flashes_drawn >= SPAWN_MARKER_MAXIMUM_FLASHES)
				continue;
			if (vertex_count + SPAWN_MARKER_FLASH_TRIANGLES * NUMBER_OF_VERTICES_PER_TRIANGLE >
				(long)NUMBEROF(spawn_marker_points))
			{
				continue;
			}
			built = spawn_marker_flash_build(location, flashes[flash_index].age, &spawn_marker_points[vertex_count]);
			spawn_marker_team_color(flashes[flash_index].team_index, &color);
			color.alpha = (1.0f - flashes[flash_index].age) * spawn_marker_near_fade(distance);
			if (distance > SPAWN_MARKER_RANGE - SPAWN_MARKER_FADE)
				color.alpha *= (SPAWN_MARKER_RANGE - distance) / SPAWN_MARKER_FADE;
			pixel = real_argb_color_to_pixel32(&color);
			for (vertex_index = 0; vertex_index < built; vertex_index++)
				spawn_marker_colors[vertex_count + vertex_index] = pixel;
			vertex_count += built;
			flashes_drawn++;
		}

		for (index = 0; index < scenario->players.count; index++)
		{
			struct player_starting_location const *location =
				TAG_BLOCK_GET_ELEMENT(&scenario->players, index, struct player_starting_location);
			real_argb_color color;
			real_vector3d delta;
			real distance;
			pixel32 pixel;
			boolean cross = FALSE;
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

			/* GREEN, or SPAWN HEAT's colour */
			color.alpha = SPAWN_MARKER_ALPHA;
			color.red = 0.15f;
			color.green = 1.0f;
			color.blue = 0.25f;
			if (heat && index < heat->count)
				spawn_marker_heat_color(heat, index, &color, &cross);
			if (vertex_count + SPAWN_MARKER_VERTICES +
				(cross ? SPAWN_MARKER_CROSS_TRIANGLES * NUMBER_OF_VERTICES_PER_TRIANGLE : 0) >
				(long)NUMBEROF(spawn_marker_points))
			{
				continue;
			}

			built = spawn_marker_build(location, cross, &spawn_marker_points[vertex_count]);
			if (!built)
				continue;

			/* fading out toward the range's limit; with SPAWN HEAT, and
			near the camera */
			if (distance > SPAWN_MARKER_RANGE - SPAWN_MARKER_FADE)
				color.alpha *= (SPAWN_MARKER_RANGE - distance) / SPAWN_MARKER_FADE;
			if (heat)
				color.alpha *= spawn_marker_near_fade(distance);
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

/* a spawn's marker's triangles into points (SPAWN_MARKER_VERTICES of them,
with a cross SPAWN_MARKER_CROSS_TRIANGLES' more): returns how many points,
0 for none (a spawn facing straight into its floor) */
static long spawn_marker_build(
	struct player_starting_location const *location,
	boolean cross,
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

	/* SPAWN HEAT's cross through the chevron: two bars, corner to corner */
	if (cross)
	{
		short bar;

		for (bar = 0; bar < 2; bar++)
		{
			real side = bar ? -1.0f : 1.0f;
			real_point3d corners[4];
			real along = SPAWN_MARKER_CROSS_HALF_LENGTH * 0.70710678f;
			real across = SPAWN_MARKER_CROSS_HALF_WIDTH * 0.70710678f;

			/* (the bar from (-along, -along side) to (along, along side),
			widened across it) */
			spawn_marker_point(&center, &forward, &left, -along - across, (-along + across) * side, &corners[0]);
			spawn_marker_point(&center, &forward, &left, along - across, (along + across) * side, &corners[1]);
			spawn_marker_point(&center, &forward, &left, along + across, (along - across) * side, &corners[2]);
			spawn_marker_point(&center, &forward, &left, -along + across, (-along - across) * side, &corners[3]);
			points[point_index++] = corners[0];
			points[point_index++] = corners[1];
			points[point_index++] = corners[2];
			points[point_index++] = corners[0];
			points[point_index++] = corners[2];
			points[point_index++] = corners[3];
		}
	}

	return point_index;
}

/* a real spawn's flash's ring into points (SPAWN_MARKER_FLASH_TRIANGLES *
3), age 0 to 1 growing it from the marker's ring to
SPAWN_MARKER_FLASH_RADIUS; how many points */
static long spawn_marker_flash_build(
	struct player_starting_location const *location,
	real age,
	real_point3d *points)
{
	real_point3d center;
	real_vector3d normal;
	real_vector3d forward;
	real_vector3d left;
	real outer = SPAWN_MARKER_OUTER_RADIUS + (SPAWN_MARKER_FLASH_RADIUS - SPAWN_MARKER_OUTER_RADIUS) * age;
	real inner = outer - SPAWN_MARKER_FLASH_WIDTH;
	short segment;
	long point_index = 0;

	spawn_marker_floor(&location->position, &center, &normal);
	center.x += normal.i * SPAWN_MARKER_HEIGHT;
	center.y += normal.j * SPAWN_MARKER_HEIGHT;
	center.z += normal.k * SPAWN_MARKER_HEIGHT;

	/* (any two directions along the floor: a ring has no facing) */
	forward.i = 1.0f;
	forward.j = 0.0f;
	forward.k = 0.0f;
	if (normal.i > 0.9f || normal.i < -0.9f)
	{
		forward.i = 0.0f;
		forward.j = 1.0f;
	}
	{
		real length = dot_product3d(&forward, &normal);

		forward.i -= normal.i * length;
		forward.j -= normal.j * length;
		forward.k -= normal.k * length;
	}
	normalize3d(&forward);
	cross_product3d(&normal, &forward, &left);

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

		spawn_marker_point(&center, &forward, &left, cos0 * outer, sin0 * outer, &outer0);
		spawn_marker_point(&center, &forward, &left, cos1 * outer, sin1 * outer, &outer1);
		spawn_marker_point(&center, &forward, &left, cos0 * inner, sin0 * inner, &inner0);
		spawn_marker_point(&center, &forward, &left, cos1 * inner, sin1 * inner, &inner1);
		points[point_index++] = outer0;
		points[point_index++] = outer1;
		points[point_index++] = inner1;
		points[point_index++] = outer0;
		points[point_index++] = inner1;
		points[point_index++] = inner0;
	}

	return point_index;
}

/* SPAWN HEAT's colour of the view's spawn index (color's alpha the
marker's before its fade), and whether it gets a cross */
static void spawn_marker_heat_color(
	struct spawn_heat_view const *heat,
	long index,
	real_argb_color *color,
	boolean *cross)
{
	struct spawn_heat_spawn const *spawn = &heat->spawns[index];

	*cross = FALSE;
	switch (spawn->zero_reason)
	{
	case _spawn_rating_rated:
	{
		real t = heat->probability_maximum > 0.0f ? spawn->probability / heat->probability_maximum : 0.0f;

		/* (near uniform: all the middle colour) */
		if (heat->near_uniform)
			t = SPAWN_HEAT_UNIFORM_T;
		short stop;

		t = PIN(t, 0.0f, 1.0f);
		for (stop = 1; stop < (short)NUMBEROF(spawn_heat_scale) - 1 && t > spawn_heat_scale[stop][0]; stop++)
			;
		{
			real const *low = spawn_heat_scale[stop - 1];
			real const *high = spawn_heat_scale[stop];
			real part = (t - low[0]) / (high[0] - low[0]);

			part = PIN(part, 0.0f, 1.0f);
			color->red = low[1] + (high[1] - low[1]) * part;
			color->green = low[2] + (high[2] - low[2]) * part;
			color->blue = low[3] + (high[3] - low[3]) * part;
		}
		color->alpha = SPAWN_HEAT_ALPHA_COLD + SPAWN_HEAT_ALPHA_HOT_EXTRA * t;
		/* (the one likeliest pulses) */
		if (index == heat->hottest)
		{
			real phase = (real)(game_time_get() % SPAWN_HEAT_PULSE_TICKS) / (real)SPAWN_HEAT_PULSE_TICKS;
			real wave = 0.5f + 0.5f * (real)cos(2.0f * _pi * phase);

			color->alpha *= SPAWN_HEAT_PULSE_LOW + (1.0f - SPAWN_HEAT_PULSE_LOW) * wave;
		}
		break;
	}

	case _spawn_rating_zero_team:
	{
		/* (CTF: the other team's, faint in its colour; neither's grey) */
		struct player_starting_location const *location =
			TAG_BLOCK_GET_ELEMENT(&global_scenario_get()->players, index, struct player_starting_location);

		spawn_marker_team_color(location->team_index == NONE ? (short)2 : location->team_index, color);
		color->alpha = SPAWN_HEAT_OTHER_TEAM_ALPHA;
		break;
	}

	default:
		/* (can't be picked now: dark red, crossed) */
		color->red = 0.5f;
		color->green = 0.03f;
		color->blue = 0.03f;
		color->alpha = SPAWN_HEAT_ZERO_ALPHA;
		*cross = TRUE;
		break;
	}

	return;
}

/* SPAWN HEAT's near fade at a distance from the camera: 1 from
SPAWN_HEAT_NEAR_FULL out, SPAWN_HEAT_NEAR_FAINT_PART from
SPAWN_HEAT_NEAR_FAINT in, straight between */
static real spawn_marker_near_fade(
	real distance)
{
	real part;

	if (distance >= SPAWN_HEAT_NEAR_FULL)
		return 1.0f;
	if (distance <= SPAWN_HEAT_NEAR_FAINT)
		return SPAWN_HEAT_NEAR_FAINT_PART;
	part = (distance - SPAWN_HEAT_NEAR_FAINT) / (SPAWN_HEAT_NEAR_FULL - SPAWN_HEAT_NEAR_FAINT);

	return SPAWN_HEAT_NEAR_FAINT_PART + (1.0f - SPAWN_HEAT_NEAR_FAINT_PART) * part;
}

/* a team's colour: red 0, blue 1, white for none (a game without teams),
grey for another (a CTF spawn neither team uses) */
static void spawn_marker_team_color(
	short team_index,
	real_argb_color *color)
{
	color->alpha = 1.0f;
	switch (team_index)
	{
	case 0:
		color->red = 1.0f;
		color->green = 0.22f;
		color->blue = 0.18f;
		break;
	case 1:
		color->red = 0.25f;
		color->green = 0.5f;
		color->blue = 1.0f;
		break;
	case NONE:
		color->red = 1.0f;
		color->green = 1.0f;
		color->blue = 1.0f;
		break;
	default:
		color->red = 0.6f;
		color->green = 0.6f;
		color->blue = 0.6f;
		break;
	}

	return;
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
	/* (the map's structure and the objects a spawn can stand on: scenery,
	such as crates, and machines, such as platforms and bridges; not
	bipeds, vehicles, items or projectiles, which come and go) */
	if (collision_test_vector(FLAG(_collision_test_front_facing_surfaces_bit) | FLAG(_collision_test_structure_bit) |
			FLAG(_collision_test_objects_bit) | FLAG(_collision_test_objects_scenery_bit) |
			FLAG(_collision_test_objects_machines_bit),
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
