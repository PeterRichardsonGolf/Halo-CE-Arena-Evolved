/* The round's danger zone in the world (ae_zone_world.h) */
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

#include "ae_round_zone_rules.h"
#include "ae_zone_view.h"
#include "ae_zone_world.h"

#include <math.h>
#include <string.h>

/* port_config.c's */
char const *config_string(char const *name);
void platform_log(char const *format, ...);

enum
{
	/* the ring's angular grid: a floor height is kept for each of its points, whatever the ring's radius */
	GRID = 512,
	/* the most floor rays a view fires a frame */
	RAYS_PER_FRAME = 48,
	/* a kept floor height goes stale after this many ticks, or when the ring has moved this far since */
	FLOOR_STALE_TICKS = 3 * 30,
	MAXIMUM_VERTICES = 7200,
};
#define FLOOR_STALE_MOVE 1.0f
/* the wall's foot sinks this far under the ground so a slope never opens a gap under it */
#define WALL_SINK 0.5f
#define VIEW_RANGE 160.0f
#define VIEW_FADE 40.0f
/* the wall's tint ramp: deep violet to hot white-violet */
static real const ramp_low[3] = { 110.0f / 255.0f, 40.0f / 255.0f, 200.0f / 255.0f };
static real const ramp_high[3] = { 245.0f / 255.0f, 225.0f / 255.0f, 255.0f / 255.0f };
/* the next ring's colour: the strip's ice */
static real const ice[3] = { 0.62f, 0.86f, 1.0f };

struct floor_entry
{
	real z, radius;
	long stamp;
};

static real point_z[GRID];
static boolean point_known[GRID];
static struct floor_entry floors[MAXIMUM_NUMBER_OF_LOCAL_PLAYERS][GRID];
static int override_look = 0;
static real_point3d points[MAXIMUM_VERTICES];
static pixel32 colors[MAXIMUM_VERTICES];
static boolean shown[MAXIMUM_NUMBER_OF_LOCAL_PLAYERS];
static long most_drawn;
static real reveal_now = 1.0f;

void ae_zone_world_set_look(int look)
{
	override_look = look == 'a' || look == 'b' ? look : 0;
}

int ae_zone_world_look(void)
{
	char const *text;

	if (override_look)
		return override_look;
	text = config_string("game.zone_look");
	return text && (text[0] == 'a' || text[0] == 'A') ? 'a' : 'b';
}

/* the floor under a ring point: a kept height, a new ray when it is stale and the frame has rays left */
static real floor_at(short view, int grid, real x, real y, real fallback, real radius, long now, int *rays)
{
	struct floor_entry *entry = &floors[view][grid];
	boolean have = entry->stamp != 0;

	if (have && now - entry->stamp <= FLOOR_STALE_TICKS && fabsf(entry->radius - radius) <= FLOOR_STALE_MOVE)
		return entry->z;
	if (*rays > 0)
	{
		struct collision_result result;
		real_point3d start;
		real_vector3d vector;
		boolean pushed = FALSE;

		(*rays)--;
		start.x = x;
		start.y = y;
		start.z = (fallback > render.camera.position.z ? fallback : render.camera.position.z) + 30.0f;
		vector.i = 0.0f;
		vector.j = 0.0f;
		vector.k = -110.0f;
		if (global_current_collision_user_depth < MAXIMUM_COLLISION_USER_STACK_DEPTH)
		{
			global_current_collision_users[global_current_collision_user_depth++] = 20;
			pushed = TRUE;
		}
		entry->z = have ? entry->z : fallback;
		if (collision_test_vector(FLAG(_collision_test_front_facing_surfaces_bit) | FLAG(_collision_test_structure_bit) |
				FLAG(_collision_test_objects_bit) | FLAG(_collision_test_objects_scenery_bit) |
				FLAG(_collision_test_objects_machines_bit), &start, &vector, NONE, &result) &&
			result.plane.n.k > 0.3f)
		{
			entry->z = result.point.z;
		}
		if (pushed)
			--global_current_collision_user_depth;
		entry->radius = radius;
		entry->stamp = now > 0 ? now : 1;
		return entry->z;
	}
	return have ? entry->z : fallback;
}

static void colour(real ramp, real alpha, pixel32 *out)
{
	real_argb_color color;

	color.alpha = alpha < 0.0f ? 0.0f : alpha > 1.0f ? 1.0f : alpha;
	color.red = ramp_low[0] + (ramp_high[0] - ramp_low[0]) * ramp;
	color.green = ramp_low[1] + (ramp_high[1] - ramp_low[1]) * ramp;
	color.blue = ramp_low[2] + (ramp_high[2] - ramp_low[2]) * ramp;
	*out = real_argb_color_to_pixel32(&color);
}

static void quad(long *count, real_point3d const *p0, real_point3d const *p1, real_point3d const *p2,
	real_point3d const *p3, pixel32 c0, pixel32 c1, pixel32 c2, pixel32 c3)
{
	/* (corners: 0 and 1 the bottom, 2 over 0 and 3 over 1) */
	long at = *count;

	points[at] = *p0; colors[at++] = c0;
	points[at] = *p1; colors[at++] = c1;
	points[at] = *p3; colors[at++] = c3;
	points[at] = *p0; colors[at++] = c0;
	points[at] = *p3; colors[at++] = c3;
	points[at] = *p2; colors[at++] = c2;
	*count = at;
}

static long vertex_cap(void)
{
	short views = local_player_count();

	return views <= 1 ? MAXIMUM_VERTICES : views == 2 ? 3300 : 1700;
}

/* the wall of either look at one radius: returns the vertices built */
static long wall_build(short view, struct ae_zone_view const *zone, real radius, int look, long now, long budget)
{
	static real const levels_a[] = { 0.0f, 0.35f, 1.0f, 2.5f, 4.5f, 7.0f, 10.0f };
	static real const levels_b[] = { 0.0f, 0.12f, 1.2f, 5.0f };
	real const *levels = look == 'a' ? levels_a : levels_b;
	int rows = look == 'a' ? (int)NUMBEROF(levels_a) : (int)NUMBEROF(levels_b);
	real circumference = 2.0f * _pi * radius;
	real step = look == 'a' ? 2.4f : 2.0f;
	int stride = 1;
	int segments;
	int rays = RAYS_PER_FRAME;
	long count = 0;
	real cam_x = render.camera.position.x, cam_y = render.camera.position.y;
	real camera_angle = (real)atan2(cam_y - zone->centre_y, cam_x - zone->centre_x);
	int nearest, offset;
	real fallback = render.camera.position.z - 0.65f;

	while (stride < 32 && (real)(GRID / (stride * 2)) >= circumference / step)
		stride *= 2;
	segments = GRID / stride;
	nearest = (int)floor((camera_angle / (2.0f * _pi) + 1.0f) * (real)segments + 0.5f) % segments;
	/* the ground under every ring point in view, nearest first (each point's height is kept once, so two segments that meet
	share it: one unbroken strip), a point whose ray finds nothing taking its nearer neighbour's height */
	{
		real chain = fallback;

		memset(point_known, 0, sizeof(point_known));
		for (offset = 0; offset < segments; offset++)
		{
			int index = (nearest + (offset & 1 ? (offset + 1) / 2 : segments - offset / 2)) % segments;
			real angle = 2.0f * _pi * (real)index / (real)segments;
			real px = zone->centre_x + radius * (real)cos(angle), py = zone->centre_y + radius * (real)sin(angle);
			real distance = (real)sqrt((px - cam_x) * (px - cam_x) + (py - cam_y) * (py - cam_y));

			if (distance >= VIEW_RANGE + 8.0f ||
				(px - cam_x) * render.camera.forward.i + (py - cam_y) * render.camera.forward.j < -16.0f)
			{
				continue;
			}
			point_z[index] = floor_at(view, index * stride, px, py, chain, radius, now, &rays);
			point_known[index] = TRUE;
			chain = point_z[index];
		}
	}
	for (offset = 0; offset < segments; offset++)
	{
		int index = (nearest + (offset & 1 ? (offset + 1) / 2 : segments - offset / 2)) % segments;
		real angle0 = 2.0f * _pi * (real)index / (real)segments;
		real angle1 = 2.0f * _pi * (real)(index + 1) / (real)segments;
		real_point3d base[2];
		real x[2], y[2], arc[2];
		real mid_x, mid_y, distance, fade, near_fade = 1.0f;
		int side, row;

		x[0] = zone->centre_x + radius * (real)cos(angle0);
		y[0] = zone->centre_y + radius * (real)sin(angle0);
		x[1] = zone->centre_x + radius * (real)cos(angle1);
		y[1] = zone->centre_y + radius * (real)sin(angle1);
		mid_x = (x[0] + x[1]) * 0.5f;
		mid_y = (y[0] + y[1]) * 0.5f;
		distance = (real)sqrt((mid_x - cam_x) * (mid_x - cam_x) + (mid_y - cam_y) * (mid_y - cam_y));
		if (distance >= VIEW_RANGE || !point_known[index] || !point_known[(index + 1) % segments])
			continue;
		/* (none well behind the camera) */
		if ((mid_x - cam_x) * render.camera.forward.i + (mid_y - cam_y) * render.camera.forward.j < -8.0f)
			continue;
		if (count + (look == 'a' ? 6 * (rows - 1) : 6 * (rows - 1) + 6) > budget)
			break;
		fade = distance > VIEW_RANGE - VIEW_FADE ? (VIEW_RANGE - distance) / VIEW_FADE : 1.0f;
		fade *= reveal_now;
		if (look == 'a' && distance < 3.0f)
			near_fade = 0.3f + 0.7f * distance / 3.0f;
		for (side = 0; side < 2; side++)
		{
			base[side].x = x[side];
			base[side].y = y[side];
			base[side].z = point_z[(index + side) % segments] + 0.04f;
			arc[side] = radius * (side ? angle1 : angle0);
		}
		for (row = 0; row + 1 < rows; row++)
		{
			real_point3d p[2][2];
			pixel32 c[2][2];
			int level;

			for (side = 0; side < 2; side++)
			{
				for (level = 0; level < 2; level++)
				{
					real height = levels[row + level], alpha, ramp;

					p[side][level] = base[side];
					p[side][level].z += height - (row == 0 && level == 0 ? WALL_SINK : 0.0f);
					/* (the arc's valley for the curtain's base: its ribs are separate) */
					ae_zone_wall_sample(look, height, look == 'a' ? arc[side] : 0.0f, now, &alpha, &ramp);
					colour(ramp, alpha * fade * near_fade, &c[side][level]);
				}
			}
			quad(&count, &p[0][0], &p[1][0], &p[0][1], &p[1][1], c[0][0], c[1][0], c[0][1], c[1][1]);
		}
		/* the curtain's ribs: one for each AE_ZONE_RIB_SPACING of arc that starts in this segment, a column that tapers up */
		if (look == 'b')
		{
			long first = (long)ceil((double)(arc[0] / AE_ZONE_RIB_SPACING)), rib;

			for (rib = first; (real)rib * AE_ZONE_RIB_SPACING < arc[1] && count + 6 <= budget; rib++)
			{
				real rib_arc = ((real)rib + 0.0f) * AE_ZONE_RIB_SPACING;
				real t = (rib_arc - arc[0]) / (arc[1] - arc[0]);
				real rib_angle = angle0 + (angle1 - angle0) * t;
				real_point3d foot, top;
				real_point3d left_foot, right_foot, left_top, right_top;
				real foot_alpha, foot_ramp, top_alpha, top_ramp, tangent_x, tangent_y;
				pixel32 c_foot, c_top;
				real foot_z = base[0].z + (base[1].z - base[0].z) * t;

				tangent_x = -(real)sin(rib_angle);
				tangent_y = (real)cos(rib_angle);
				foot.x = zone->centre_x + radius * (real)cos(rib_angle);
				foot.y = zone->centre_y + radius * (real)sin(rib_angle);
				foot.z = foot_z;
				top = foot;
				top.z = foot_z + AE_ZONE_CURTAIN_HEIGHT;
				ae_zone_wall_sample(look, 0.4f, rib_arc + 0.5f * AE_ZONE_RIB_SPACING, now, &foot_alpha, &foot_ramp);
				ae_zone_wall_sample(look, 4.6f, rib_arc + 0.5f * AE_ZONE_RIB_SPACING, now, &top_alpha, &top_ramp);
				colour(foot_ramp, foot_alpha * fade, &c_foot);
				colour(top_ramp, top_alpha * fade, &c_top);
				left_foot = right_foot = foot;
				left_foot.x -= tangent_x * 0.22f; left_foot.y -= tangent_y * 0.22f;
				right_foot.x += tangent_x * 0.22f; right_foot.y += tangent_y * 0.22f;
				left_top = right_top = top;
				left_top.x -= tangent_x * 0.03f; left_top.y -= tangent_y * 0.03f;
				right_top.x += tangent_x * 0.03f; right_top.y += tangent_y * 0.03f;
				quad(&count, &left_foot, &right_foot, &left_top, &right_top, c_foot, c_foot, c_top, c_top);
			}
		}
	}
	return count;
}

/* the next ring: a low ice-coloured dashed band, every dash 1.2 wu with a gap of 1.3 wu */
static long preview_build(short view, struct ae_zone_view const *zone, real radius, long now, long budget)
{
	real circumference = 2.0f * _pi * radius;
	long dashes = (long)(circumference / 2.5f);
	real cam_x = render.camera.position.x, cam_y = render.camera.position.y;
	real fallback = render.camera.position.z - 0.65f;
	int rays = RAYS_PER_FRAME / 2;
	long count = 0, index;

	if (dashes < 6)
		dashes = 6;
	if (dashes > 160)
		dashes = 160;
	for (index = 0; index < dashes && count + 6 <= budget; index++)
	{
		real angle0 = 2.0f * _pi * (real)index / (real)dashes;
		real angle1 = angle0 + 1.2f / radius;
		real x0 = zone->centre_x + radius * (real)cos(angle0), y0 = zone->centre_y + radius * (real)sin(angle0);
		real x1 = zone->centre_x + radius * (real)cos(angle1), y1 = zone->centre_y + radius * (real)sin(angle1);
		real mid_x = (x0 + x1) * 0.5f, mid_y = (y0 + y1) * 0.5f;
		real distance = (real)sqrt((mid_x - cam_x) * (mid_x - cam_x) + (mid_y - cam_y) * (mid_y - cam_y));
		real z, fade;
		int grid = ((int)floor(angle0 / (2.0f * _pi) * GRID + 0.5f)) % GRID;
		real_point3d a, b, a_top, b_top;
		real_argb_color color;
		pixel32 low, high;

		if (distance >= VIEW_RANGE ||
			(mid_x - cam_x) * render.camera.forward.i + (mid_y - cam_y) * render.camera.forward.j < -8.0f)
		{
			continue;
		}
		fade = distance > VIEW_RANGE - VIEW_FADE ? (VIEW_RANGE - distance) / VIEW_FADE : 1.0f;
		fade *= reveal_now;
		z = floor_at(view, grid, x0, y0, fallback, radius, now, &rays) + 0.05f;
		a.x = x0; a.y = y0; a.z = z;
		b.x = x1; b.y = y1; b.z = z;
		a_top = a; b_top = b;
		a_top.z += 1.4f; b_top.z += 1.4f;
		color.alpha = 0.55f * fade; color.red = ice[0]; color.green = ice[1]; color.blue = ice[2];
		low = real_argb_color_to_pixel32(&color);
		color.alpha = 0.04f * fade;
		high = real_argb_color_to_pixel32(&color);
		quad(&count, &a, &b, &a_top, &b_top, low, low, high, high);
	}
	return count;
}

void ae_zone_world_draw(short local_player_index)
{
	struct ae_zone_view zone;
	long now = game_time_get(), count, budget;
	int look;
	boolean on;
	real reveal;

	if (local_player_index < 0 || local_player_index >= MAXIMUM_NUMBER_OF_LOCAL_PLAYERS)
		return;
	on = (boolean)(game_engine_running() && !cinematic_in_progress() && ae_zone_view_get(&zone));
	if (on != shown[local_player_index])
	{
		shown[local_player_index] = on;
		platform_log("zone world: view %d %s at tick %ld", (int)local_player_index, on ? "on" : "off", now);
	}
	if (!on || zone.radius < 0.2f)
		return;
	/* (the zone is not seen until it comes in: nothing through the first wait, then it eases in) */
	reveal = ae_zone_reveal(&zone.plan, zone.running, zone.elapsed);
	if (reveal <= 0.0f)
		return;
	reveal_now = reveal;
	look = ae_zone_world_look();
	budget = vertex_cap();
	count = wall_build(local_player_index, &zone, zone.radius, look, now, budget);
	if (zone.next_radius > 0.5f && zone.next_radius < zone.radius - 0.5f && budget - count > 100)
		count += preview_build(local_player_index, &zone, zone.next_radius, now, budget - count);
	if (count > most_drawn + most_drawn / 8 + 30)
	{
		most_drawn = count;
		platform_log("zone world: look %c, %ld vertices in view %d (most so far; at most %ld)", (char)look, count,
			(int)local_player_index, budget);
	}
	if (count > 0)
		rasterizer_debug_draw_triangles_now(points, colors, count / NUMBER_OF_VERTICES_PER_TRIANGLE);
}
