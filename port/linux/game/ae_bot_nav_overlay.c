/* The bots' vehicle navigation in the world (ae_bot_nav_overlay.h) */
#include "cseries/cseries.h"

#include "bitmaps/bitmap_color_conversion.h"
#include "cutscene/cinematics.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "math/real_math.h"
#include "physics/collisions.h"
#include "physics/collision_usage.h"
#include "rasterizer/rasterizer.h"
#include "render/render.h"

#include "ae_bot_nav.h"
#include "ae_bot_nav_overlay.h"
#include "ae_bot_nav_overlay_rules.h"
#include "ae_bots_learnt.h"
#include "ae_bots_overlay.h"
#include "ae_bots_vehicle_rules.h"
#include "ae_path_cost.h"

#include <math.h>
#include <string.h>

/* port_config.c's */
long config_integer(char const *name);
void platform_log(char const *format, ...);
/* the bots' translation unit's (ae_bots_vehicle.inc) */
int ae_bots_drive_spot_count(void);
struct ae_bots_drive_spot const *ae_bots_drive_spot_get(int index);

enum
{
	/* the triangles a frame holds */
	MAXIMUM_TRIANGLES = 6000,
	MAXIMUM_VERTICES = MAXIMUM_TRIANGLES * 3,
	/* the closed surfaces, and drivers, a frame draws */
	MAXIMUM_CLOSED = 48,
	MAXIMUM_DRIVERS = 16,
	/* the ground rays a frame fires, and the obstacles whose ground is kept */
	RAYS_PER_FRAME = 24,
	MAXIMUM_KEPT = 2048,
	GROUND_STALE_TICKS = 4 * 30,
	RING_SEGMENTS = 24
};
#define VIEW_RANGE 40.0f
#define ROUTE_LIFT 0.15f
#define LINE_WIDTH 0.08f
#define THIN_WIDTH 0.04f
#define THICK_WIDTH 0.16f
#define KEEPOUT_NONE (-1.0e29f)

struct ground_entry
{
	real z;
	long stamp;
};

static real_point3d points[MAXIMUM_VERTICES];
static unsigned char point_colours[MAXIMUM_VERTICES];
static pixel32 colours[MAXIMUM_VERTICES];
static struct ae_nav_mesh mesh;
static boolean mesh_ready;
static struct ground_entry kept[MAXIMUM_KEPT];
static real_point3d closed_corners[MAXIMUM_CLOSED * AE_BOTS_OVERLAY_CORNERS];
static long closed_counts[MAXIMUM_CLOSED];
static struct ae_bots_overlay_driver drivers[MAXIMUM_DRIVERS];
static int logged_mode;
static char logged_map[64];
static long most_dropped;

/* the ground under (x, y) near the camera: a ray down from above the camera, the last answer kept for a while */
static real ground_at(int key, real x, real y, long now, int *rays)
{
	struct ground_entry *entry = key >= 0 && key < MAXIMUM_KEPT ? &kept[key] : NULL;
	real fallback = render.camera.position.z - 0.65f;

	/* (a stamp later than now is from before a map change or restart: stale) */
	if (entry && entry->stamp != 0 && (now < entry->stamp || now - entry->stamp > GROUND_STALE_TICKS))
		entry->stamp = 0;
	if (entry && entry->stamp != 0)
		return entry->z;
	if (*rays > 0)
	{
		struct collision_result result;
		real_point3d start;
		real_vector3d vector;
		boolean pushed = FALSE;
		real found = entry && entry->stamp != 0 ? entry->z : fallback;

		(*rays)--;
		start.x = x;
		start.y = y;
		start.z = render.camera.position.z + 20.0f;
		vector.i = 0.0f;
		vector.j = 0.0f;
		vector.k = -80.0f;
		if (global_current_collision_user_depth < MAXIMUM_COLLISION_USER_STACK_DEPTH)
		{
			global_current_collision_users[global_current_collision_user_depth++] = 20;
			pushed = TRUE;
		}
		if (collision_test_vector(FLAG(_collision_test_front_facing_surfaces_bit) | FLAG(_collision_test_structure_bit), &start,
				&vector, NONE, &result) && result.plane.n.k > 0.3f)
		{
			found = result.point.z;
		}
		if (pushed)
			--global_current_collision_user_depth;
		if (entry)
		{
			entry->z = found;
			entry->stamp = now > 0 ? now : 1;
		}
		return found;
	}
	return entry && entry->stamp != 0 ? entry->z : fallback;
}

static void point_of(real_point3d const *p, float *out, real lift)
{
	out[0] = p->x;
	out[1] = p->y;
	out[2] = p->z + lift;
}

/* the nav file's routes */
static void routes_build(real cam_x, real cam_y)
{
	struct ae_bot_nav_map const *map = ae_bot_nav_map();
	int route, index;

	if (!map)
		return;
	for (route = 0; route < map->route_count; route++)
	{
		struct ae_bot_nav_route const *line = &map->routes[route];
		int colour = ae_nav_route_colour(line->kinds), last = line->first + line->count;

		for (index = line->first; index < last && index < map->point_count; index++)
		{
			struct ae_bot_nav_point const *point = &map->points[index];
			float a[3], b[3], centre[3];
			int next = index + 1 < last ? index + 1 : line->loop && line->count > 2 ? line->first : -1;

			a[0] = point->x; a[1] = point->y; a[2] = point->z + ROUTE_LIFT;
			if (ae_nav_point_near(a, cam_x, cam_y, VIEW_RANGE))
			{
				centre[0] = a[0]; centre[1] = a[1]; centre[2] = a[2] + 0.02f;
				ae_nav_square(&mesh, centre, 0.3f, colour);
			}
			if (next < 0 || next >= map->point_count)
				continue;
			b[0] = map->points[next].x; b[1] = map->points[next].y; b[2] = map->points[next].z + ROUTE_LIFT;
			if (!ae_nav_segment_near(a, b, cam_x, cam_y, VIEW_RANGE))
				continue;
			ae_nav_line(&mesh, a, b, LINE_WIDTH, colour);
			ae_nav_chevron(&mesh, a, b, 0.9f, LINE_WIDTH, colour);
		}
	}
}

/* the obstacle circles: the nav file's keepouts dashed amber on the ground, the scenery's red at its height, learnt ones
thicker */
static void obstacles_build(real cam_x, real cam_y, long now)
{
	int index, count = ae_path_cost_obstacle_count(), rays = RAYS_PER_FRAME;

	for (index = 0; index < count; index++)
	{
		float x, y, radius, zlo, zhi, centre[3], z;
		long object_index;

		if (!ae_path_cost_obstacle_get(index, &x, &y, &radius) || !ae_path_cost_obstacle_span(index, &zlo, &zhi, &object_index))
			continue;
		centre[0] = x;
		centre[1] = y;
		centre[2] = 0.0f;
		/* (the ring's nearest point is within range) */
		if (!ae_nav_point_near(centre, cam_x, cam_y, VIEW_RANGE + radius))
			continue;
		if (zlo < KEEPOUT_NONE)
		{
			z = ground_at(index, x, y, now, &rays) + 0.1f;
			ae_nav_ring(&mesh, x, y, z, radius, RING_SEGMENTS, LINE_WIDTH, TRUE, AE_NAV_COLOUR_AMBER);
		}
		else
		{
			ae_nav_ring(&mesh, x, y, zlo + 0.4f, radius, RING_SEGMENTS, object_index == NONE ? THICK_WIDTH : LINE_WIDTH, FALSE,
				AE_NAV_COLOUR_RED);
		}
	}
}

static void closed_build(real cam_x, real cam_y)
{
	real_point3d centre;
	long count, index;

	centre.x = cam_x;
	centre.y = cam_y;
	centre.z = 0.0f;
	count = ae_bots_overlay_closed_near(&centre, VIEW_RANGE, closed_corners, closed_counts, MAXIMUM_CLOSED);
	for (index = 0; index < count; index++)
	{
		float outline[AE_BOTS_OVERLAY_CORNERS][3];
		long corner;

		for (corner = 0; corner < closed_counts[index]; corner++)
			point_of(&closed_corners[index * AE_BOTS_OVERLAY_CORNERS + corner], outline[corner], 0.06f);
		ae_nav_outline(&mesh, (float const (*)[3])outline, (int)closed_counts[index], THIN_WIDTH, AE_NAV_COLOUR_RED);
	}
}

static void spots_build(real cam_x, real cam_y)
{
	int index, count = ae_bots_drive_spot_count();
	long wedges = ae_bots_learnt_wedge_count();

	for (index = 0; index < count; index++)
	{
		struct ae_bots_drive_spot const *spot = ae_bots_drive_spot_get(index);
		float centre[3];
		int level = 0;

		if (!spot)
			continue;
		centre[0] = spot->x;
		centre[1] = spot->y;
		if (!ae_nav_point_near(centre, cam_x, cam_y, VIEW_RANGE))
			continue;
		/* (a spot with several kinds of trouble stacks one diamond for each) */
		if (spot->crashes > 0)
		{
			centre[2] = spot->z + 0.4f + 0.7f * (float)level++;
			ae_nav_diamond(&mesh, centre, 0.35f, AE_NAV_COLOUR_RED);
		}
		if (spot->wedges > 0)
		{
			centre[2] = spot->z + 0.4f + 0.7f * (float)level++;
			ae_nav_diamond(&mesh, centre, 0.35f, AE_NAV_COLOUR_ORANGE);
		}
		if (spot->stucks > 0)
		{
			centre[2] = spot->z + 0.4f + 0.7f * (float)level++;
			ae_nav_diamond(&mesh, centre, 0.35f, AE_NAV_COLOUR_PURPLE);
		}
	}
	for (index = 0; index < wedges; index++)
	{
		real_point3d place;
		long hits;
		float centre[3];

		if (!ae_bots_learnt_wedge_get(index, &place, &hits))
			continue;
		point_of(&place, centre, 0.4f);
		if (ae_nav_point_near(centre, cam_x, cam_y, VIEW_RANGE))
			ae_nav_diamond_outline(&mesh, centre, 0.5f, LINE_WIDTH, AE_NAV_COLOUR_RED);
	}
}

static void drivers_build(real cam_x, real cam_y)
{
	long count = ae_bots_overlay_drivers(drivers, MAXIMUM_DRIVERS), index;

	for (index = 0; index < count; index++)
	{
		struct ae_bots_overlay_driver const *driver = &drivers[index];
		float from[3], to[3];
		int step;

		point_of(&driver->position, from, 0.5f);
		if (!ae_nav_point_near(from, cam_x, cam_y, VIEW_RANGE))
			continue;
		if (driver->have_next)
		{
			point_of(&driver->next, to, 0.5f);
			ae_nav_line(&mesh, from, to, LINE_WIDTH, AE_NAV_COLOUR_WHITE);
			memcpy(from, to, sizeof from);
		}
		for (step = 0; step < driver->step_count; step++)
		{
			point_of(&driver->steps[step], to, 0.5f);
			/* (the next waypoint is the line already drawn) */
			if (fabsf(to[0] - from[0]) + fabsf(to[1] - from[1]) > 0.05f)
				ae_nav_line(&mesh, from, to, THIN_WIDTH, AE_NAV_COLOUR_ICE_THIN);
			memcpy(from, to, sizeof from);
		}
	}
}

/* the counts of the log line */
static void log_state(int mode, char const *map)
{
	struct ae_bot_nav_map const *nav = ae_bot_nav_map();
	int index, scenery = 0, keepouts = 0, learnt = 0, count = ae_path_cost_obstacle_count();

	for (index = 0; index < count; index++)
	{
		float zlo, zhi;
		long object_index;

		if (!ae_path_cost_obstacle_span(index, &zlo, &zhi, &object_index))
			continue;
		if (zlo < KEEPOUT_NONE)
			keepouts++;
		else if (object_index == NONE)
			learnt++;
		else
			scenery++;
	}
	platform_log("bot nav overlay: %s: %d routes (%d points), %d obstacles, %d keepouts, %d learnt circles, %ld closed surfaces, "
		"%d drive spots (%s)", map, nav ? nav->route_count : 0, nav ? nav->point_count : 0, scenery, keepouts, learnt,
		ae_bots_overlay_closed_count(), ae_bots_drive_spot_count(), mode >= 2 ? "all" : "routes");
}

void ae_bot_nav_overlay_draw(short local_player_index)
{
	long mode = config_integer("debug.bot_nav_show"), now, index;
	real cam_x, cam_y;
	char const *map;
	real_argb_color color;
	pixel32 palette[AE_NAV_COLOURS];

	if (local_player_index < 0 || local_player_index >= MAXIMUM_NUMBER_OF_LOCAL_PLAYERS)
		return;
	if (mode <= 0 || !game_engine_running() || cinematic_in_progress())
	{
		logged_mode = 0;
		return;
	}
	if (mode > 2)
		mode = 2;
	map = ae_bot_nav_map_file();
	if (!map[0])
		map = "(no map)";
	if ((int)mode != logged_mode || strncmp(map, logged_map, sizeof logged_map))
	{
		logged_mode = (int)mode;
		most_dropped = 0;
		strncpy(logged_map, map, sizeof logged_map - 1);
		logged_map[sizeof logged_map - 1] = 0;
		log_state((int)mode, map);
	}
	if (!mesh_ready)
	{
		ae_nav_mesh_init(&mesh, (float (*)[3])points, point_colours, MAXIMUM_VERTICES);
		mesh_ready = TRUE;
	}
	ae_nav_mesh_clear(&mesh);
	now = game_time_get();
	cam_x = render.camera.position.x;
	cam_y = render.camera.position.y;
	/* (the drivers, routes and trouble spots first: a crowd of rings fills the buffer last) */
	if (mode >= 2)
		drivers_build(cam_x, cam_y);
	routes_build(cam_x, cam_y);
	if (mode >= 2)
	{
		spots_build(cam_x, cam_y);
		obstacles_build(cam_x, cam_y, now);
		closed_build(cam_x, cam_y);
	}
	if (mesh.dropped > most_dropped)
	{
		most_dropped = mesh.dropped;
		platform_log("bot nav overlay: view %d: %ld triangles dropped (the buffer holds %d)", (int)local_player_index,
			mesh.dropped, (int)MAXIMUM_TRIANGLES);
	}
	if (mesh.count <= 0)
		return;
	for (index = 0; index < AE_NAV_COLOURS; index++)
	{
		struct ae_nav_colour const *colour = &ae_bot_nav_overlay_palette[index];

		color.alpha = colour->alpha;
		color.red = (real)colour->red / 255.0f;
		color.green = (real)colour->green / 255.0f;
		color.blue = (real)colour->blue / 255.0f;
		palette[index] = real_argb_color_to_pixel32(&color);
	}
	for (index = 0; index < mesh.count; index++)
		colours[index] = palette[point_colours[index]];
	rasterizer_debug_draw_triangles_now(points, colours, mesh.count / NUMBER_OF_VERTICES_PER_TRIANGLE);
}
