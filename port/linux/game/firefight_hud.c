/*
FIREFIGHT_HUD.C

Halo 1 CE-styled Firefight HUD module:
- (the set, round, wave, lives, hostiles and time are the score panel's:
  campaign_score.c)
- Pre-Round Preparation Countdown Banner ("ROUND X STARTS IN XXs — RESUPPLY & BOARD VEHICLES")
- Animated Wave / Boss / Skull / Ordnance Announcement Banners
- Generator Defense Alpha / Beta / Gamma Shield & Health Bars + Lockdown status
- 3D->2D World-to-Screen Navpoints for Stragglers (<= 3 enemies), Dropships & Ordnance Drops
- Scoreboard header formatter, a multiplayer report's extras, and the
  postgame's XP screen
*/

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "cseries/cseries.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "interface/hud.h"
#include "interface/hud_definitions.h"
#include "interface/hud_messaging.h"
#include "interface/interface.h"
#include "math/real_math.h"
#include "memory/data.h"
#include "objects/objects.h"
#include "rasterizer/rasterizer.h"
#include "render/render.h"
#include "render/render_camera_projection.h"
#include "render/render_cameras.h"
#include "scenario/scenario.h"
#include "scenario/scenario_definitions.h"
#include "tag_files/tag_files.h"
#include "tag_files/tag_groups.h"
#include "text/draw_string.h"
#include "text/font_group.h"
#include "text/unicode.h"
#include "units/units.h"

#include "halo_firefight.h"
#include "ae_firefight_glue.h"
#include "cache/texture_cache.h"
#include "halo_skulls.h"
#include "bitmaps/bitmaps.h"
#include "bitmaps/bitmap_group.h"
#include "interface/ui_widget.h"
#include "rasterizer/rasterizer_swizzle.h"
#include <xtl.h>
#include "rasterizer/xbox/rasterizer_xbox.h"
#include <stdlib.h>

/* (the C library's allocator, not the game's tracked one (cseries.h): the
rank icons' file comes from config_file_read, which allocates with it; the
icons and the working pictures are kept for the whole run) */
#undef malloc
#undef free
#undef realloc

unsigned long system_milliseconds(void);
/* the platform layer's (port_config.c) */
void config_folder(char *path, size_t size);
char *config_file_read(const char *path, size_t *size);
void platform_log(char const *format, ...);

void draw_quad(rectangle2d const *bounds, pixel32 color);
struct bitmap_data *bitmap_group_try_and_get_bitmap(long bitmap_group_index, short bitmap_index);
short find_nav_point(char const *name);
short hud_get_nav_point_render_type(short local_player_index, real_point3d const *head, real_point3d const *position, long reference_object_index);
void custom_render_nav_point(short local_player_index, real_point3d const *position_pointer, short nav_index, short waypoint_type);
void render_interpolation_object_point(long object_index, real_point3d *point);

/* ---------- constants */

/* Stock Halo CE pause/objective box palette (matches pause_box_svg in tools/port_settings.py) */
#define FF_HUD_PANEL_FILL  0xBF0A2649
#define FF_HUD_PANEL_BOT   0xBF050D1C
#define FF_HUD_HEADER_FILL 0xBF000000
#define FF_HUD_PANEL_EDGE  0xFF2896FF
#define FF_HUD_WARN_FILL   0xBF481010
#define FF_HUD_WARN_BOT    0xBF220606
#define FF_HUD_WARN_EDGE   0xFFFF4444
#define FF_HUD_BAR_BG      0xBF050D1C
#define FF_HUD_SHIELD_BAR  0xFF2896FF
#define FF_HUD_HEALTH_BAR  0xFF40E070
#define FF_HUD_LOCK_BAR    0xFFFFCC33

enum
{
	_ff_text_left = 0,
	_ff_text_right = 1,
	_ff_text_center = 2
};

/* ---------- helpers */

static pixel32 ff_lerp_argb(pixel32 c0, pixel32 c1, real t)
{
	long a0 = (long)((c0 >> 24) & 0xFF);
	long r0 = (long)((c0 >> 16) & 0xFF);
	long g0 = (long)((c0 >> 8) & 0xFF);
	long b0 = (long)(c0 & 0xFF);
	long a1 = (long)((c1 >> 24) & 0xFF);
	long r1 = (long)((c1 >> 16) & 0xFF);
	long g1 = (long)((c1 >> 8) & 0xFF);
	long b1 = (long)(c1 & 0xFF);
	unsigned long a = (unsigned long)PIN((long)(a0 + (a1 - a0) * t + 0.5f), 0L, 255L);
	unsigned long r = (unsigned long)PIN((long)(r0 + (r1 - r0) * t + 0.5f), 0L, 255L);
	unsigned long g = (unsigned long)PIN((long)(g0 + (g1 - g0) * t + 0.5f), 0L, 255L);
	unsigned long b = (unsigned long)PIN((long)(b0 + (b1 - b0) * t + 0.5f), 0L, 255L);

	return (pixel32)((a << 24) | (r << 16) | (g << 8) | b);
}

static void ff_draw_bitmap_uv(
	struct bitmap_data *bitmap,
	short x0,
	short y0,
	short x1,
	short y1,
	real u0,
	real v0,
	real u1,
	real v1,
	pixel32 tint_argb)
{
	struct dynamic_screen_vertex vertices[4];
	struct rasterizer_dynamic_screen_geometry_parameters parameters;
	int v;

	if (!bitmap || !bitmap->hardware_format || x1 <= x0 || y1 <= y0 || ((tint_argb >> 24) & 0xFFUL) == 0)
		return;

	for (v = 0; v < 4; v++)
	{
		vertices[v].position.x = (v % 3) ? (real)x1 : (real)x0;
		vertices[v].position.y = (v > 1) ? (real)y1 : (real)y0;
		vertices[v].texture_coordinates.x = (v % 3) ? u1 : u0;
		vertices[v].texture_coordinates.y = (v > 1) ? v1 : v0;
		vertices[v].color = tint_argb;
	}

	csmemset(&parameters, 0, sizeof(parameters));
	parameters.meter_parameters = NULL;
	parameters.map_offset[0] = NULL;
	parameters.map_wrapped[0] = FALSE;
	parameters.map_wrapped[1] = FALSE;
	parameters.map_wrapped[2] = FALSE;
	parameters.map_scale[0].i = 1.0f;
	parameters.map_scale[0].j = 1.0f;
	parameters.map_texture_scale[0].i = 1.0f;
	parameters.map_texture_scale[0].j = 1.0f;
	parameters.point_sampled = FALSE;
	parameters.framebuffer_blend_function = 0;
	parameters.map[0] = bitmap;
	parameters.map[1] = NULL;
	parameters.map[2] = NULL;

	rasterizer_psuedo_dynamic_screen_quad_draw(&parameters, vertices);
}

static boolean ff_draw_ce_menu_bitmap_uv(
	char const *tag_name,
	short frame_idx,
	short x0,
	short y0,
	short x1,
	short y1,
	real u0,
	real v0,
	real u1,
	real v1,
	pixel32 tint_argb)
{
	long tag_idx;
	struct bitmap_data *bmp;

	if (!tag_name || !tag_name[0] || x1 <= x0 || y1 <= y0)
		return FALSE;

	tag_idx = tag_loaded(BITMAP_GROUP_TAG, tag_name);
	if (tag_idx == NONE)
		return FALSE;

	bmp = bitmap_group_get_bitmap_from_sequence(tag_idx, 0, frame_idx);
	if (!bmp && frame_idx != 0)
		bmp = bitmap_group_get_bitmap_from_sequence(tag_idx, 0, 0);
	if (!bmp || !bmp->hardware_format || bmp->width <= 0 || bmp->height <= 0)
		return FALSE;

	ff_draw_bitmap_uv(bmp, x0, y0, x1, y1, u0, v0, u1, v1, tint_argb);
	return TRUE;
}

/* Draws a 3-slice horizontally rounded Halo CE option/row pill bar using pc\bitmaps\option_bkds_sm
 * (exact same rounded bar style used in the Forge menu). */
boolean firefight_draw_ce_rounded_pill(
	short px0,
	short py0,
	short px1,
	short py1,
	short frame_idx,
	real scale,
	pixel32 tint_argb)
{
	short w = (short)(px1 - px0);
	short h = (short)(py1 - py0);
	short cap = (short)MIN(w / 2, MAX(4, (int)(12.0f * scale + 0.5f)));
	real v1 = 28.0f / 32.0f;
	real u_cap = 16.0f / 256.0f;

	if (w <= 2 || h <= 2 || ((tint_argb >> 24) & 0xFFUL) == 0)
		return FALSE;

	if (w <= cap * 2)
	{
		return ff_draw_ce_menu_bitmap_uv(
			"pc\\bitmaps\\option_bkds_sm", frame_idx,
			px0, py0, px1, py1,
			0.0f, 0.0f, 1.0f, v1, tint_argb);
	}

	if (ff_draw_ce_menu_bitmap_uv("pc\\bitmaps\\option_bkds_sm", frame_idx, px0, py0, (short)(px0 + cap), py1, 0.0f, 0.0f, u_cap, v1, tint_argb) &&
		ff_draw_ce_menu_bitmap_uv("pc\\bitmaps\\option_bkds_sm", frame_idx, (short)(px0 + cap), py0, (short)(px1 - cap), py1, u_cap, 0.0f, 1.0f - u_cap, v1, tint_argb) &&
		ff_draw_ce_menu_bitmap_uv("pc\\bitmaps\\option_bkds_sm", frame_idx, (short)(px1 - cap), py0, px1, py1, 1.0f - u_cap, 0.0f, 1.0f, v1, tint_argb))
	{
		return TRUE;
	}
	return FALSE;
}

/* Draws the authentic 9-slice Halo CE pause/objectives container box with rounded blue corners
 * (exact same rounded panel style used in the Forge menu). */
void firefight_draw_ce_rounded_box(
	short px0,
	short py0,
	short px1,
	short py1,
	short header_px,
	real scale,
	pixel32 tint_argb,
	pixel32 fallback_fill,
	pixel32 fallback_edge)
{
	short w = (short)(px1 - px0);
	short h = (short)(py1 - py0);
	boolean with_header = (header_px > 0 && header_px < h - 4);
	char const *top_left   = with_header ? "pc\\pause\\objectives_left"   : "pc\\pause\\pausebox_left";
	char const *top_center = with_header ? "pc\\pause\\objectives_center" : "pc\\pause\\pausebox_center";
	char const *top_right  = with_header ? "pc\\pause\\objectives_right"  : "pc\\pause\\pausebox_right";
	char const *obj_left   = "pc\\pause\\objectives_left";
	char const *obj_center = "pc\\pause\\objectives_center";
	char const *obj_right  = "pc\\pause\\objectives_right";
	short frame_idx = 1;
	short cap_w = (short)MIN(w / 3, MAX(6, (int)(14.0f * scale + 0.5f)));
	short top_h = with_header
		? (short)PIN(header_px, (short)8, (short)(h - 6))
		: (short)MIN(h / 2, MAX(4, (int)(11.0f * scale + 0.5f)));
	short bot_h = (short)MIN(h - top_h, MAX(4, (int)(11.0f * scale + 0.5f)));
	real v_top_end = with_header ? (30.0f / 256.0f) : (14.0f / 256.0f);
	real v_body_start = 32.0f / 256.0f;
	real v_body_end = 211.0f / 256.0f;
	real v_bot_end = 225.0f / 256.0f;

	if (w <= 6 || h <= 6 || ((tint_argb >> 24) & 0xFFUL) == 0)
		return;

	/* Compact single-row bars without headers use the 3-slice Halo CE rounded pill (option_bkds_sm) */
	if (!with_header && h <= (short)(20.0f * scale + 0.5f))
	{
		if (firefight_draw_ce_rounded_pill(px0, py0, px1, py1, 0, scale, tint_argb))
			return;
	}

	if (w > cap_w * 2 && top_h > 0 && bot_h > 0)
	{
		boolean ok =
			ff_draw_ce_menu_bitmap_uv(top_left, frame_idx, px0, py0, (short)(px0 + cap_w), (short)(py0 + top_h), 0.0f, 0.0f, 1.0f, v_top_end, tint_argb) &&
			ff_draw_ce_menu_bitmap_uv(top_center, frame_idx, (short)(px0 + cap_w), py0, (short)(px1 - cap_w), (short)(py0 + top_h), 0.0f, 0.0f, 1.0f, v_top_end, tint_argb) &&
			ff_draw_ce_menu_bitmap_uv(top_right, frame_idx, (short)(px1 - cap_w), py0, px1, (short)(py0 + top_h), 0.0f, 0.0f, 1.0f, v_top_end, tint_argb);

		if (ok && (py1 - bot_h) > (py0 + top_h))
		{
			ok =
				ff_draw_ce_menu_bitmap_uv(obj_left, frame_idx, px0, (short)(py0 + top_h), (short)(px0 + cap_w), (short)(py1 - bot_h), 0.0f, v_body_start, 1.0f, v_body_end, tint_argb) &&
				ff_draw_ce_menu_bitmap_uv(obj_center, frame_idx, (short)(px0 + cap_w), (short)(py0 + top_h), (short)(px1 - cap_w), (short)(py1 - bot_h), 0.0f, v_body_start, 1.0f, v_body_end, tint_argb) &&
				ff_draw_ce_menu_bitmap_uv(obj_right, frame_idx, (short)(px1 - cap_w), (short)(py0 + top_h), px1, (short)(py1 - bot_h), 0.0f, v_body_start, 1.0f, v_body_end, tint_argb);
		}

		if (ok)
		{
			ok =
				ff_draw_ce_menu_bitmap_uv(obj_left, frame_idx, px0, (short)(py1 - bot_h), (short)(px0 + cap_w), py1, 0.0f, v_body_end, 1.0f, v_bot_end, tint_argb) &&
				ff_draw_ce_menu_bitmap_uv(obj_center, frame_idx, (short)(px0 + cap_w), (short)(py1 - bot_h), (short)(px1 - cap_w), py1, 0.0f, v_body_end, 1.0f, v_bot_end, tint_argb) &&
				ff_draw_ce_menu_bitmap_uv(obj_right, frame_idx, (short)(px1 - cap_w), (short)(py1 - bot_h), px1, py1, 0.0f, v_body_end, 1.0f, v_bot_end, tint_argb);
		}

		if (ok)
			return;
	}

	/* Fallback if menu bitmap tags are not present */
	{
		short border_px = (short)MAX(2, (int)(2.0f * scale + 0.5f));
		short corner_in = (short)MAX(2, (int)(3.0f * scale + 0.5f));
		short header_end_y = with_header ? (short)(py0 + top_h) : py0;
		short divider_end_y = (header_end_y > py0) ? (short)MIN(py1 - border_px, header_end_y + border_px) : py0;
		short body_y0 = (divider_end_y > py0) ? divider_end_y : (short)(py0 + border_px);
		short body_y1 = (short)(py1 - border_px);
		short inner_x0 = (short)(px0 + border_px);
		short inner_x1 = (short)(px1 - border_px);
		pixel32 bot_fill = (fallback_fill == FF_HUD_WARN_FILL) ? FF_HUD_WARN_BOT : FF_HUD_PANEL_BOT;
		rectangle2d r;
		int band;

		if (inner_x1 <= inner_x0 || body_y1 <= py0 + border_px)
			return;

		r.x0 = (short)(px0 + corner_in);
		r.x1 = (short)(px1 - corner_in);
		r.y0 = py0;
		r.y1 = (short)(py0 + border_px);
		if (r.x1 > r.x0)
			draw_quad(&r, fallback_edge);

		r.y0 = (short)(py1 - border_px);
		r.y1 = py1;
		if (r.x1 > r.x0)
			draw_quad(&r, fallback_edge);

		r.x0 = px0;
		r.x1 = inner_x0;
		r.y0 = (short)(py0 + border_px);
		r.y1 = body_y1;
		draw_quad(&r, fallback_edge);

		r.x0 = inner_x1;
		r.x1 = px1;
		draw_quad(&r, fallback_edge);

		if (header_end_y > py0 + border_px)
		{
			r.x0 = inner_x0;
			r.x1 = inner_x1;
			r.y0 = (short)(py0 + border_px);
			r.y1 = header_end_y;
			draw_quad(&r, FF_HUD_HEADER_FILL);

			r.y0 = header_end_y;
			r.y1 = divider_end_y;
			draw_quad(&r, fallback_edge);
		}

		if (body_y1 > body_y0)
		{
			short body_h = (short)(body_y1 - body_y0);
			for (band = 0; band < 3; band++)
			{
				short by0 = (short)(body_y0 + (body_h * band) / 3);
				short by1 = (band == 2) ? body_y1 : (short)(body_y0 + (body_h * (band + 1)) / 3);
				if (by1 > by0)
				{
					pixel32 fill_color = ff_lerp_argb(fallback_fill, bot_fill, (real)band * 0.5f);
					r.x0 = inner_x0;
					r.x1 = inner_x1;
					r.y0 = by0;
					r.y1 = by1;
					draw_quad(&r, fill_color);
				}
			}
		}
	}
}

/* ---------- the HUD's boxes

drawn here, not from the pause menu's art, whose body has no side border where
it meets the header: a gap in the frame at the size of a banner. Nothing is
drawn twice, the fills being translucent */

static void ff_box_quad(
	short x0,
	short y0,
	short x1,
	short y1,
	pixel32 color)
{
	rectangle2d r;

	if (x1 <= x0 || y1 <= y0)
		return;
	r.x0 = x0;
	r.y0 = y0;
	r.x1 = x1;
	r.y1 = y1;
	draw_quad(&r, color);
}

/* how far in a rounded corner's edge is, `row` rows from its flat side */
static short ff_box_corner_inset(
	short row,
	short radius)
{
	real t = (real)radius - (real)row - 0.5f;

	if (row >= radius)
		return 0;
	return (short)((real)radius - (real)sqrt((double)((real)(radius * radius) - t * t)) + 0.5f);
}

/* a row of the box's inside: `side` of it at each end (the wells') in the
one colour, the middle in the other */
static void ff_box_row(
	short x0,
	short x1,
	short y0,
	short y1,
	short side_x0,
	short side_x1,
	pixel32 middle,
	pixel32 sides)
{
	short middle_x0 = MAX(x0, side_x0);
	short middle_x1 = MIN(x1, side_x1);

	ff_box_quad(x0, y0, middle_x0, y1, sides);
	ff_box_quad(middle_x0, y0, middle_x1, y1, middle);
	ff_box_quad(middle_x1, y0, x1, y1, sides);
}

/* side_px: wells at each end, for the skulls' icons, which the header and its
divider stop short of */
static void ff_draw_panel_box(
	short px0,
	short py0,
	short px1,
	short py1,
	short header_px,
	short side_px,
	real scale,
	pixel32 fill,
	pixel32 edge)
{
	short border = (short)MAX(2, (int)(1.5f * scale + 0.5f));
	short radius = (short)MAX(border + 2, (int)(6.0f * scale + 0.5f));
	pixel32 bottom = fill == FF_HUD_WARN_FILL ? FF_HUD_WARN_BOT : FF_HUD_PANEL_BOT;
	pixel32 top_middle = header_px > 0 ? FF_HUD_HEADER_FILL : fill;
	short well_x0 = (short)(px0 + side_px);
	short well_x1 = (short)(px1 - side_px);
	short body_y0 = (short)(py0 + radius);
	short body_y1 = (short)(py1 - radius);
	short row;
	short band;

	if (px1 - px0 <= radius * 2 || py1 - py0 <= radius * 2)
		return;

	/* the corners, a row at a time, top and bottom */
	for (row = 0; row < radius; row++)
	{
		short outer = ff_box_corner_inset(row, radius);
		short top = (short)(py0 + row);
		short low = (short)(py1 - row - 1);

		if (row < border)
		{
			ff_box_quad((short)(px0 + outer), top, (short)(px1 - outer), (short)(top + 1), edge);
			ff_box_quad((short)(px0 + outer), low, (short)(px1 - outer), (short)(low + 1), edge);
		}
		else
		{
			short inner = (short)(border + ff_box_corner_inset((short)(row - border), (short)(radius - border)));

			ff_box_quad((short)(px0 + outer), top, (short)(px0 + inner), (short)(top + 1), edge);
			ff_box_quad((short)(px1 - inner), top, (short)(px1 - outer), (short)(top + 1), edge);
			ff_box_quad((short)(px0 + outer), low, (short)(px0 + inner), (short)(low + 1), edge);
			ff_box_quad((short)(px1 - inner), low, (short)(px1 - outer), (short)(low + 1), edge);
			ff_box_row((short)(px0 + inner), (short)(px1 - inner), top, (short)(top + 1),
				well_x0, well_x1, top_middle, fill);
			ff_box_quad((short)(px0 + inner), low, (short)(px1 - inner), (short)(low + 1), bottom);
		}
	}

	/* the sides, unbroken */
	ff_box_quad(px0, body_y0, (short)(px0 + border), body_y1, edge);
	ff_box_quad((short)(px1 - border), body_y0, px1, body_y1, edge);

	/* the header and its divider */
	if (header_px > 0)
	{
		short header_y1 = (short)MIN(py0 + header_px, body_y1);
		short divider_y1 = (short)MIN(header_y1 + border, body_y1);

		ff_box_row((short)(px0 + border), (short)(px1 - border), body_y0, header_y1,
			well_x0, well_x1, FF_HUD_HEADER_FILL, fill);
		ff_box_row((short)(px0 + border), (short)(px1 - border), header_y1, divider_y1,
			well_x0, well_x1, edge, fill);
		body_y0 = divider_y1;
	}

	/* the body, shaded to the bottom's fill */
	for (band = 0; band < 3; band++)
	{
		short height = (short)(body_y1 - body_y0);

		ff_box_quad((short)(px0 + border), (short)(body_y0 + (height * band) / 3),
			(short)(px1 - border), band == 2 ? body_y1 : (short)(body_y0 + (height * (band + 1)) / 3),
			ff_lerp_argb(fill, bottom, (real)band * 0.5f));
	}
}

static void ff_draw_box_with_wells(
	real vx0,
	real vy0,
	real scale,
	real x0,
	real y0,
	real x1,
	real y1,
	real header_h,
	real well_w,
	pixel32 fill,
	pixel32 edge)
{
	short px0 = (short)(vx0 + x0 * scale + 0.5f);
	short py0 = (short)(vy0 + y0 * scale + 0.5f);
	short px1 = (short)MAX(vx0 + x1 * scale + 0.5f, px0 + 6);
	short py1 = (short)MAX(vy0 + y1 * scale + 0.5f, py0 + 6);
	short header_px = (header_h > 0.0f && y0 + header_h < y1) ? (short)(header_h * scale + 0.5f) : 0;

	ff_draw_panel_box(px0, py0, px1, py1, header_px, (short)(well_w * scale + 0.5f), scale, fill, edge);
}

static void ff_draw_box(
	real vx0,
	real vy0,
	real scale,
	real x0,
	real y0,
	real x1,
	real y1,
	real header_h,
	pixel32 fill,
	pixel32 edge)
{
	ff_draw_box_with_wells(vx0, vy0, scale, x0, y0, x1, y1, header_h, 0.0f, fill, edge);
}

static void ff_draw_quad(
	real vx0,
	real vy0,
	real scale,
	real x0,
	real y0,
	real x1,
	real y1,
	pixel32 col)
{
	rectangle2d r;
	r.x0 = (short)(vx0 + x0 * scale + 0.5f);
	r.y0 = (short)(vy0 + y0 * scale + 0.5f);
	r.x1 = (short)(vx0 + x1 * scale + 0.5f);
	r.y1 = (short)(vy0 + y1 * scale + 0.5f);
	if (r.x1 > r.x0 && r.y1 > r.y0)
		draw_quad(&r, col);
}

static void ff_draw_text(
	long font_index,
	short line_height,
	real vx0,
	real vy0,
	real scale,
	char const *ascii_text,
	real x0,
	real x1,
	real y,
	real text_scale,
	short justification,
	real_argb_color const *color)
{
	real avail_w = MAX(1.0f, x1 - x0);
	real eff_scale = text_scale;
	real draw_x, ox, oy, final_scale;
	wchar_t wide[128];
	rectangle2d measure_bounds, ink, cursor, bounds;
	short ink_w;
	long i;

	if (!ascii_text || !ascii_text[0] || font_index == NONE)
		return;

	/* (" & " spelled out: the HUD font's & is a stylized one, which the banners'
	scale makes a smudge of) */
	{
		long length = 0;

		for (i = 0; length < (long)NUMBEROF(wide) - 4 && ascii_text[i]; i++)
		{
			if (ascii_text[i] == '&' && i > 0 && ascii_text[i - 1] == ' ' && ascii_text[i + 1] == ' ')
			{
				wide[length++] = 'A';
				wide[length++] = 'N';
				wide[length++] = 'D';
			}
			else
			{
				wide[length++] = (wchar_t)(unsigned char)ascii_text[i];
			}
		}
		wide[length] = 0;
	}

	draw_string_set_draw_mode(font_index, NONE, _ff_text_left, 0, color);
	measure_bounds.x0 = 0;
	measure_bounds.y0 = 0;
	measure_bounds.x1 = 4096;
	measure_bounds.y1 = (short)(line_height * 2);
	draw_unicode_string_compute_bounds(&measure_bounds, wide, &ink, &cursor);

	ink_w = (ink.x1 > ink.x0) ? (short)(ink.x1 - ink.x0) : 1;
	if ((real)ink_w * eff_scale > avail_w)
		eff_scale = avail_w / (real)ink_w;

	if (justification == _ff_text_center)
		draw_x = x0 + (avail_w - (real)ink_w * eff_scale) * 0.5f;
	else if (justification == _ff_text_right)
		draw_x = x1 - (real)ink_w * eff_scale;
	else
		draw_x = x0;

	ox = vx0 + draw_x * scale;
	oy = vy0 + y * scale;
	final_scale = scale * eff_scale;

	bounds.x0 = (short)(ox - (ink.x0 > 0 ? ink.x0 : 0));
	bounds.y0 = (short)oy;
	bounds.x1 = (short)(bounds.x0 + ink_w + 64);
	bounds.y1 = (short)(oy + line_height * 2);

	rasterizer_text_set_scale(final_scale, ox, oy);
	rasterizer_draw_unicode_string(&bounds, NULL, NULL, 0, wide);
	rasterizer_text_set_scale(1.0f, 0.0f, 0.0f);
}

static void format_elapsed_time(long elapsed_ticks, char *out_buf, size_t buf_size)
{
	long total_seconds = MAX(0L, elapsed_ticks / TICKS_PER_SECOND);
	long hours = total_seconds / 3600L;
	long minutes = (total_seconds / 60L) % 60L;
	long seconds = total_seconds % 60L;

	if (hours > 0)
		snprintf(out_buf, buf_size, "%ld:%02ld:%02ld", hours, minutes, seconds);
	else
		snprintf(out_buf, buf_size, "%02ld:%02ld", minutes, seconds);
}

/* ---------- the ordnance pods' markers (port) */

/* a player's place among the game's players (those not quit, by their
index, as every machine has them); NONE for none */
static short firefight_hud_player_order(long player_index)
{
	struct data_iterator iterator;
	struct player_datum *player;
	short order = 0;

	if (!player_data || player_index == NONE)
		return NONE;
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		if (iterator.datum_index == player_index)
			return order;
		if (!player->quit_out_of_game)
			order++;
	}
	return NONE;
}

/* whether this view's player marks the pod: its own (each pod's player, by
its place among the players: firefight_spawner.c's), or everyone's; from a
host from before, its own set of ordnance_pods_per_player (a ninth player
sharing the first's), else all of them */
static boolean firefight_hud_own_pod(short pod, short pods)
{
	short per_player = ff_state.ordnance_pods_per_player;
	byte owner = ff_state.ordnance_pod_owners[pod];
	short order;

	if (render.local_player_index == NONE)
		return TRUE;
	order = firefight_hud_player_order(local_player_get_player_index(render.local_player_index));
	if (order == NONE)
		return TRUE;
	if (per_player > 0)
	{
		short sets = (short)((pods + per_player - 1) / per_player);

		return sets <= 1 || pod / per_player == order % sets;
	}
	return owner == FIREFIGHT_ORDNANCE_POD_ANYONE ||
		owner == (byte)MIN(order, FIREFIGHT_ORDNANCE_POD_ANYONE - 1);
}

/* whether this view's player has been to the pod (within 2.5 units, since
it landed: the weapon on it taken, or seen and left), its marker gone till
the next drop */
#define FF_HUD_POD_VISIT_DISTANCE 2.5f
/* (so long after its own landing: its group's, firefight_ordnance_pod_land_time) */
#define FF_HUD_POD_LANDED_TICKS (TICKS_PER_SECOND)
static boolean firefight_hud_pod_visited(short pod)
{
	static long drop_time[MAXIMUM_LOCAL_PLAYERS];
	static unsigned long visited[MAXIMUM_LOCAL_PLAYERS][(FIREFIGHT_MAX_ORDNANCE_PODS + 31) / 32];
	short local = render.local_player_index;
	struct player_datum *player;
	struct object_datum *unit;
	real_point3d const *spot = &ff_state.ordnance_pod_positions[pod];

	if (local < 0 || local >= MAXIMUM_LOCAL_PLAYERS || pod < 0 || pod >= FIREFIGHT_MAX_ORDNANCE_PODS)
		return FALSE;
	if (drop_time[local] != ff_state.ordnance_pod_drop_time)
	{
		drop_time[local] = ff_state.ordnance_pod_drop_time;
		csmemset(visited[local], 0, sizeof(visited[local]));
	}
	if (visited[local][pod >> 5] & (1UL << (pod & 31)))
		return TRUE;
	if (firefight_ordnance_pod_spent(pod))
		return TRUE;
	if (ff_state.ordnance_pod_drop_time <= 0 ||
		game_time_get() - firefight_ordnance_pod_land_time(pod) < FF_HUD_POD_LANDED_TICKS)
	{
		return FALSE;
	}
	player = (struct player_datum *)datum_try_and_get(player_data, local_player_get_player_index(local));
	unit = player ? object_try_and_get(player->unit_index) : NULL;
	if (!unit)
		return FALSE;
	if (distance2d((real_point2d const *)&unit->object.position, (real_point2d const *)spot) < FF_HUD_POD_VISIT_DISTANCE &&
		fabs(unit->object.position.z - spot->z) < 2.0f)
	{
		visited[local][pod >> 5] |= 1UL << (pod & 31);
		return TRUE;
	}
	return FALSE;
}

/* ---------- 3D -> 2D Navpoints (Stragglers & Resupply Drops) */

static void draw_world_navpoint(
	long font_index,
	short line_height,
	real scale,
	real_point3d const *world_pos,
	char const *label,
	pixel32 box_color,
	real_argb_color const *text_color,
	boolean is_friendly_marker)
{
	real_point3d view_pt;
	real_point2d screen_pt;
	real dist;
	char full_label[48];
	rectangle2d marker;
	short pill_x0, pill_y0, pill_x1, pill_y1;
	boolean drew_friendly_bitmap = FALSE;

	matrix4x3_transform_point(&render.frustum.world_to_view, world_pos, &view_pt);
	if (!render_camera_view_to_screen(&render.camera, &render.frustum, &view_pt, &screen_pt))
	{
		/* When ordnance is off-screen, render a green off-screen directional arrow */
		if (is_friendly_marker && render.local_player_index != NONE && hud_globals && hud_globals->waypoint.arrows.count > 0)
		{
			short arrow_index = find_nav_point("default");
			if (arrow_index == NONE)
				arrow_index = 0;
			if (arrow_index >= 0 && arrow_index < hud_globals->waypoint.arrows.count)
			{
				struct ff_hud_waypoint_arrow
				{
					char name[0x20];
					long unused0[2];
					unsigned long color;
					real opacity;
					real fade;
					short sequence_indices[3];
					short pad;
					long unused1[4];
					unsigned long flags;
					long unused2[6];
				};
				struct ff_hud_waypoint_arrow *arrow = TAG_BLOCK_GET_ELEMENT(
					&hud_globals->waypoint.arrows,
					arrow_index,
					struct ff_hud_waypoint_arrow);
				unsigned long saved_color = arrow->color;
				arrow->color = 0x0040E070;
				custom_render_nav_point(
					render.local_player_index,
					world_pos,
					arrow_index,
					1 /* _waypoint_off_screen */);
				arrow->color = saved_color;
			}
		}
		return;
	}

	dist = distance3d(&render.camera.position, world_pos);
	snprintf(full_label, sizeof(full_label), "%s [%dm]", label, (int)(dist * 3.0f + 0.5f));

	pill_x0 = (short)(screen_pt.x - 48.0f * scale + 0.5f);
	pill_y0 = (short)(screen_pt.y - 21.0f * scale + 0.5f);
	pill_x1 = (short)(screen_pt.x + 48.0f * scale + 0.5f);
	pill_y1 = (short)(screen_pt.y - 8.0f * scale + 0.5f);
	firefight_draw_ce_rounded_pill(pill_x0, pill_y0, pill_x1, pill_y1, 0, scale, 0xD8FFFFFFUL);

	if (is_friendly_marker)
	{
		long mp_hud_tag_index = interface_get_tag_index(_interface_bitmap_multiplayer_hud);
		if (mp_hud_tag_index != NONE)
		{
			struct bitmap_data *bitmap = bitmap_group_get_bitmap_from_sequence(
				mp_hud_tag_index,
				0,
				0);
			if (bitmap && _texture_cache_bitmap_get_hardware_format(bitmap, FALSE, TRUE))
			{
				point2d position;
				real_rectangle2d clip;
				real depth_factor;
				real fade;

				position.x = (short)fast_ftol(screen_pt.x) - render.camera.viewport_bounds.x0;
				position.y = (short)fast_ftol(screen_pt.y) - render.camera.viewport_bounds.y0;
				depth_factor = 1.0f -
					((-view_pt.z - render.frustum.z_near) * 100.0f) /
					(render.frustum.z_far - render.frustum.z_near);
				fade = PIN(depth_factor, 0.25f, 1.0f);
				clip.x0 = 0.0f;
				clip.x1 = 1.0f;
				clip.y0 = (1.0f - fade) * 0.5f;
				clip.y1 = 1.0f;
				interface_draw_bitmap(
					bitmap,
					&position,
					&clip,
					1.0f,
					0.0f,
					1.0f);
				drew_friendly_bitmap = TRUE;
			}
		}
	}

	if (!drew_friendly_bitmap)
	{
		/* Draw a downward chevron/diamond marker instead of a flat square */
		short cx = (short)(screen_pt.x + 0.5f);
		short cy = (short)(screen_pt.y + 1.0f * scale + 0.5f);
		short row;
		for (row = 0; row < 5; row++)
		{
			short half_w = (short)((4 - row) * scale + 0.5f);
			marker.x0 = cx - half_w;
			marker.x1 = cx + half_w + 1;
			marker.y0 = cy + (short)(row * scale);
			marker.y1 = marker.y0 + (short)(scale + 0.5f);
			if (marker.y1 <= marker.y0)
				marker.y1 = marker.y0 + 1;
			draw_quad(&marker, box_color);
		}
	}

	ff_draw_text(
		font_index,
		line_height,
		screen_pt.x - 46.0f * scale,
		screen_pt.y - 19.5f * scale,
		scale,
		full_label,
		0.0f,
		92.0f,
		0.0f,
		0.48f,
		_ff_text_center,
		text_color);
}

/* a hostile object's red arrow, off screen as well; its label where the HUD
has no such arrow. The arrow, or NONE */
static short hostile_nav_arrow_index(void)
{
	short arrow_index = find_nav_point("target_red");

	if (arrow_index == NONE)
		arrow_index = find_nav_point("default_red");
	if (arrow_index == NONE && hud_globals && hud_globals->waypoint.arrows.count > 0)
		arrow_index = (hud_globals->waypoint.arrows.count > 1) ? 1 : 0;
	return arrow_index;
}

static void draw_hostile_navpoint(
	long font_index,
	short line_height,
	real scale,
	long object_index,
	short arrow_index,
	char const *label,
	real_argb_color const *text_color)
{
	struct object_datum *object = object_get(object_index);
	short local_player_index = render.local_player_index;
	real_point3d nav_pos = object->object.bounding_sphere_center;

	render_interpolation_object_point(object_index, &nav_pos);
	nav_pos.z += object->object.bounding_sphere_radius * 0.65f + 0.15f;

	if (local_player_index != NONE && arrow_index != NONE)
	{
		short waypoint_type = hud_get_nav_point_render_type(
			local_player_index,
			&render.camera.position,
			&nav_pos,
			object_index);
		custom_render_nav_point(
			local_player_index,
			&nav_pos,
			arrow_index,
			waypoint_type);
	}
	else
	{
		draw_world_navpoint(
			font_index, line_height, scale,
			&nav_pos, label, FF_HUD_WARN_EDGE, text_color, FALSE);
	}
}

/* ---------- Generator Defense HUD Bars */

static void draw_generator_bars(
	long font_index,
	short line_height,
	real vx0,
	real vy0,
	real scale)
{
	static char const *const gen_names[3] = { "ALPHA GEN", "BETA GEN", "GAMMA GEN" };
	real base_x = 14.0f;
	real base_y = 106.0f;
	real card_w = 136.0f;
	real card_h = 24.0f;
	short g;

	if (ff_state.generator_count <= 0)
		return;

	for (g = 0; g < ff_state.generator_count && g < FIREFIGHT_MAX_GENERATORS; g++)
	{
		struct firefight_generator_state const *gen = &ff_state.generators[g];
		real y = base_y + g * (card_h + 4.0f);
		real_argb_color white = { 1.0f, 0.95f, 0.95f, 0.95f };
		real_argb_color red = { 1.0f, 1.0f, 0.35f, 0.35f };
		real_argb_color gold = { 1.0f, 1.0f, 0.85f, 0.25f };
		boolean destroyed = !gen->alive;
		real health_frac = (gen->max_health > 0) ? ((real)gen->health / (real)gen->max_health) : 0.0f;
		real shield_frac = (gen->max_shield > 0) ? ((real)gen->shield / (real)gen->max_shield) : 0.0f;
		char status_str[32];

		ff_draw_box(
			vx0, vy0, scale,
			base_x, y, base_x + card_w, y + card_h,
			0.0f,
			destroyed ? FF_HUD_WARN_FILL : FF_HUD_PANEL_FILL,
			destroyed ? FF_HUD_WARN_EDGE : FF_HUD_PANEL_EDGE);

		ff_draw_text(
			font_index, line_height, vx0, vy0, scale,
			gen_names[g],
			base_x + 6.0f, base_x + 76.0f, y + 3.0f,
			0.51f, _ff_text_left, &white);

		if (destroyed)
		{
			ff_draw_text(
				font_index, line_height, vx0, vy0, scale,
				"DESTROYED",
				base_x + 78.0f, base_x + card_w - 6.0f, y + 3.0f,
				0.51f, _ff_text_right, &red);
		}
		else if (gen->lockdown_ticks > 0)
		{
			snprintf(status_str, sizeof(status_str), "LOCK %ds",
				(int)((gen->lockdown_ticks + TICKS_PER_SECOND - 1) / TICKS_PER_SECOND));
			ff_draw_text(
				font_index, line_height, vx0, vy0, scale,
				status_str,
				base_x + 78.0f, base_x + card_w - 6.0f, y + 3.0f,
				0.51f, _ff_text_right, &gold);
		}
		else
		{
			snprintf(status_str, sizeof(status_str), "%d%%",
				(int)(health_frac * 100.0f + 0.5f));
			ff_draw_text(
				font_index, line_height, vx0, vy0, scale,
				status_str,
				base_x + 78.0f, base_x + card_w - 6.0f, y + 3.0f,
				0.51f, _ff_text_right, &white);
		}

		/* Shield & Health bar strips */
		if (!destroyed)
		{
			real bar_x0 = base_x + 6.0f;
			real bar_w = card_w - 12.0f;
			rectangle2d bar_rect;

			/* Background */
			bar_rect.x0 = (short)(vx0 + bar_x0 * scale);
			bar_rect.y0 = (short)(vy0 + (y + 14.0f) * scale);
			bar_rect.x1 = (short)(vx0 + (bar_x0 + bar_w) * scale);
			bar_rect.y1 = (short)(vy0 + (y + 20.0f) * scale);
			draw_quad(&bar_rect, FF_HUD_BAR_BG);

			/* Health fill */
			bar_rect.x1 = (short)(vx0 + (bar_x0 + bar_w * PIN(health_frac, 0.0f, 1.0f)) * scale);
			bar_rect.y1 = (short)(vy0 + (y + 17.0f) * scale);
			draw_quad(&bar_rect, gen->lockdown_ticks > 0 ? FF_HUD_LOCK_BAR : FF_HUD_HEALTH_BAR);

			/* Shield fill */
			bar_rect.y0 = (short)(vy0 + (y + 17.0f) * scale);
			bar_rect.y1 = (short)(vy0 + (y + 20.0f) * scale);
			bar_rect.x1 = (short)(vx0 + (bar_x0 + bar_w * PIN(shield_frac, 0.0f, 1.0f)) * scale);
			draw_quad(&bar_rect, FF_HUD_SHIELD_BAR);

			/* 3D World-to-Screen Navpoint over active generator */
			{
				real_point3d gen_nav = gen->position;
				gen_nav.z += 1.25f;
				draw_world_navpoint(
					font_index,
					line_height,
					scale,
					&gen_nav,
					gen_names[g],
					gen->under_attack_ticks > 0 ? FF_HUD_WARN_EDGE : FF_HUD_PANEL_EDGE,
					gen->under_attack_ticks > 0 ? &red : (gen->lockdown_ticks > 0 ? &gold : &white),
					FALSE);
			}
		}
	}
}

struct ff_skull_hud_def
{
	char const *name;
	char const *code;
	char const *desc;
	pixel32 badge_color;
};

static struct ff_skull_hud_def const s_ff_skull_hud_defs[NUMBER_OF_FIREFIGHT_SKULLS] =
{
	{ "IRON",           "IRN", "TEAM WIPE ENDS THE SET",                 0xFFE83A3A },
	{ "BLACK EYE",      "BLK", "SHIELDS ONLY RECHARGE ON MELEE",         0xFFB668FF },
	{ "TOUGH LUCK",     "TLK", "ENEMIES DODGE GRENADES & ROCKETS",       0xFFF09828 },
	{ "CATCH",          "CAT", "ENEMIES THROW MORE GRENADES",            0xFF38C8F0 },
	{ "CLOUD",          "FOG", "MOTION TRACKER DISABLED",                0xFF90A8C0 },
	{ "FAMINE",         "FAM", "DROPPED WEAPONS HAVE LESS AMMO",         0xFFD4A838 },
	{ "TILT",           "TLT", "ENEMY SHIELD & ARMOR RESISTANCE UP",     0xFF4288F5 },
	{ "MYTHIC",         "MYT", "ENEMIES HAVE 2X HEALTH & SHIELDS",       0xFFFF5038 },
	{ "THUNDERSTORM",   "THN", "ENEMIES PROMOTED TO HIGHER RANKS",       0xFFFFD038 },
	{ "BLIND",          "BLD", "HUD & FIRST-PERSON WEAPON HIDDEN",       0xFF808898 },
	{ "BOOM",           "BOM", "2X EXPLOSION RADIUS & DAMAGE",           0xFFFF6E28 },
	{ "COWBELL",        "CWB", "ACCELERATED EXPLOSIVE KNOCKBACK",        0xFF50E088 },
	{ "GRUNT BIRTHDAY", "GBP", "HEADSHOTS DETONATE IN CONFETTI",         0xFFF570C0 }
};

/* the skulls of a mask of enum firefight_skull_bit, by their bits */
static short ff_skull_bits_of(
	unsigned long skulls,
	short *bits,
	short maximum)
{
	short count = 0;
	short bit;

	for (bit = 0; bit < NUMBER_OF_FIREFIGHT_SKULLS && count < maximum; bit++)
	{
		if ((skulls & (1UL << bit)) != 0)
			bits[count++] = bit;
	}
	return count;
}

enum
{
	FF_BANNER_SKULL_INSET = 8,
	FF_BANNER_SKULL_GAP = 3
};

/* how wide the wells for `count` skulls of `size` are */
static real ff_banner_well_width(
	short count,
	real size)
{
	return FF_BANNER_SKULL_INSET + ((count + 1) / 2) * (size + FF_BANNER_SKULL_GAP) + 3.0f;
}

/* the skulls' own icons in the banner's wells: one on each side for one
skull, or shared out between them */
static void ff_draw_banner_skulls(
	real vx0,
	real vy0,
	real scale,
	real x0,
	real x1,
	real y,
	real size,
	short const *bits,
	short count)
{
	short left_count = (short)((count + 1) / 2);
	short right_count = (short)(count == 1 ? 1 : count - left_count);
	real gap = FF_BANNER_SKULL_GAP;
	real inset = FF_BANNER_SKULL_INSET;
	short index;

	for (index = 0; index < left_count + right_count; index++)
	{
		short bit = bits[count == 1 ? 0 : index];
		short slot = (short)(index < left_count ? index : index - left_count);
		real ix = index < left_count ?
			x0 + inset + slot * (size + gap) :
			x1 - inset - size - (right_count - 1 - slot) * (size + gap);

		skulls_draw_icon(firefight_map_reach_skull_to_40_skull((enum firefight_skull_bit)bit),
			(short)(vx0 + ix * scale + 0.5f), (short)(vy0 + y * scale + 0.5f),
			(short)(vx0 + (ix + size) * scale + 0.5f), (short)(vy0 + (y + size) * scale + 0.5f), 1.0f);
	}
}

static void format_banner_strings(
	short banner_type,
	short banner_data,
	char *title_out,
	size_t title_size,
	char *sub_out,
	size_t sub_size)
{
	title_out[0] = '\0';
	sub_out[0] = '\0';

	switch (banner_type)
	{
	case _ff_banner_set_start:
		snprintf(title_out, title_size, "SET %d START", (int)banner_data);
		snprintf(sub_out, sub_size, "PREPARE FOR COMBAT");
		break;
	case _ff_banner_round_start:
		snprintf(title_out, title_size, "ROUND %d", (int)banner_data);
		snprintf(sub_out, sub_size, "HOLD YOUR GROUND");
		break;
	case _ff_banner_wave_start:
		snprintf(title_out, title_size, "WAVE %d", (int)banner_data);
		snprintf(sub_out, sub_size, "HOSTILES INBOUND");
		break;
	case _ff_banner_boss_wave:
		snprintf(title_out, title_size, "BOSS WAVE %d", (int)banner_data);
		snprintf(sub_out, sub_size, "ELIMINATE HIGH-VALUE TARGETS");
		break;
	case _ff_banner_reinforcements:
		snprintf(title_out, title_size, "REINFORCEMENTS");
		snprintf(sub_out, sub_size, "ENEMY SQUADS DEPLOYING");
		break;
	case _ff_banner_bonus_round:
		snprintf(title_out, title_size, "BONUS ROUND");
		snprintf(sub_out, sub_size, "60s GRUNT BIRTHDAY PARTY — EXTRA LIVES!");
		break;
	case _ff_banner_set_complete:
		snprintf(title_out, title_size, "SET %d COMPLETE", (int)banner_data);
		snprintf(sub_out, sub_size, "BONUS LIVES AWARDED");
		break;
	case _ff_banner_skull_activated:
		if (banner_data >= 0 && banner_data < NUMBER_OF_FIREFIGHT_SKULLS)
		{
			snprintf(title_out, title_size, "SKULL ACTIVE: %s", s_ff_skull_hud_defs[banner_data].name);
			snprintf(sub_out, sub_size, "%s", s_ff_skull_hud_defs[banner_data].desc);
		}
		else
		{
			snprintf(title_out, title_size, "SKULL ACTIVATED");
			snprintf(sub_out, sub_size, "ENEMY MODIFIER ENGAGED");
		}
		break;
	case _ff_banner_last_man_standing:
		snprintf(title_out, title_size, "LAST MAN STANDING");
		snprintf(sub_out, sub_size, "SURVIVE TO KEEP THE TEAM ALIVE");
		break;
	case _ff_banner_generator_under_attack:
		snprintf(title_out, title_size, "GENERATOR %c UNDER ATTACK", 'A' + PIN(banner_data, 0, 2));
		snprintf(sub_out, sub_size, "DEFEND THE GENERATOR");
		break;
	case _ff_banner_generator_locked_down:
		snprintf(title_out, title_size, "GENERATOR %c LOCKED DOWN", 'A' + PIN(banner_data, 0, 2));
		snprintf(sub_out, sub_size, "BLAST SHIELDS ACTIVE FOR 30s");
		break;
	case _ff_banner_generator_destroyed:
		snprintf(title_out, title_size, "GENERATOR %c DESTROYED", 'A' + PIN(banner_data, 0, 2));
		snprintf(sub_out, sub_size, "DEFENSIVE PERIMETER BREACHED");
		break;
	case _ff_banner_ordnance_drop:
		snprintf(title_out, title_size, "WEAPON & AMMO DROP INBOUND");
		snprintf(sub_out, sub_size, "RESUPPLY CACHE DEPLOYED AT BASE");
		break;
	case _ff_banner_heavy_ordnance_drop:
		snprintf(title_out, title_size, "HEAVY ANTI-ARMOR DROP");
		snprintf(sub_out, sub_size, "ROCKETS, SNIPER & OVERSHIELD DEPLOYED");
		break;
	case _ff_banner_dropship_inbound:
		/* (its data what brings it: enum firefight_drop_carrier) */
		if (banner_data == _ff_drop_carrier_flood)
		{
			snprintf(title_out, title_size, "THE FLOOD IS EMERGING");
			snprintf(sub_out, sub_size, "A FLOOD FORM IS GROWING OUT OF THE GROUND");
		}
		else if (banner_data == _ff_drop_carrier_pelican)
		{
			snprintf(title_out, title_size, "PELICAN INBOUND");
			snprintf(sub_out, sub_size, "UNSC INSERTION DETECTED");
		}
		else
		{
			snprintf(title_out, title_size, "DROPSHIP INBOUND");
			snprintf(sub_out, sub_size, "COVENANT SPIRIT INSERTION DETECTED");
		}
		break;
	case _ff_banner_vehicle_deployed:
		snprintf(title_out, title_size, "VEHICLE DEPLOYED");
		snprintf(sub_out, sub_size, "ARMORED SUPPORT AT PLAYER BASE");
		break;
	case _ff_banner_bonus_life:
		snprintf(title_out, title_size, "+%d BONUS LIFE", (int)MAX(1, banner_data));
		snprintf(sub_out, sub_size, "TEAM REINFORCEMENT EARNED");
		break;
	case _ff_banner_victory:
		snprintf(title_out, title_size, "FIREFIGHT VICTORY");
		snprintf(sub_out, sub_size, "ALL SETS CLEARED");
		break;
	case _ff_banner_defeat:
		snprintf(title_out, title_size, "FIREFIGHT OVER");
		snprintf(sub_out, sub_size, "TEAM ELIMINATED");
		break;
	default:
		break;
	}
}

/* ---------- Objectives (firefight_objectives.c): their cards, their notice
and their markers, as the host's view has them */

/* (Purge's enemies marked within this of this view's player) */
#define FF_OBJECTIVE_UNIT_MARK_DISTANCE 30.0f

static void draw_objective_card(
	long font_index,
	short line_height,
	real vx0,
	real vy0,
	real scale,
	real y,
	char const *header,
	char const *title,
	char const *status,
	boolean warn)
{
	real base_x = 14.0f;
	real card_w = 176.0f;
	real card_h = 30.0f;
	real_argb_color white = { 1.0f, 0.95f, 0.95f, 0.95f };
	real_argb_color cyan = { 1.0f, 0.42f, 0.78f, 1.0f };
	real_argb_color gold = { 1.0f, 1.0f, 0.85f, 0.25f };

	ff_draw_box(vx0, vy0, scale, base_x, y, base_x + card_w, y + card_h, 0.0f,
		warn ? FF_HUD_WARN_FILL : FF_HUD_PANEL_FILL, warn ? FF_HUD_WARN_EDGE : FF_HUD_PANEL_EDGE);
	ff_draw_text(font_index, line_height, vx0, vy0, scale, header,
		base_x + 6.0f, base_x + 70.0f, y + 2.0f, 0.42f, _ff_text_left, &cyan);
	ff_draw_text(font_index, line_height, vx0, vy0, scale, status,
		base_x + 70.0f, base_x + card_w - 6.0f, y + 2.0f, 0.42f, _ff_text_right, &gold);
	ff_draw_text(font_index, line_height, vx0, vy0, scale, title,
		base_x + 6.0f, base_x + card_w - 6.0f, y + 14.0f, 0.5f, _ff_text_left, &white);
}

/* this view's player's absolute index, NONE for none */
static short objective_local_absolute(void)
{
	long player_index;

	if (render.local_player_index == NONE)
		return NONE;
	player_index = local_player_get_player_index(render.local_player_index);
	return player_index != NONE ? (short)DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index) : NONE;
}

/* whether this view's player is the one carrying (an absolute index) */
static boolean objective_local_carrier(short carrier)
{
	long player_index;

	if (carrier == NONE || render.local_player_index == NONE)
		return FALSE;
	player_index = local_player_get_player_index(render.local_player_index);
	return player_index != NONE && DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index) == carrier;
}

/* this view's player's unit, NONE for none */
static long objective_local_unit(void)
{
	long player_index;
	struct player_datum *player;

	if (render.local_player_index == NONE)
		return NONE;
	player_index = local_player_get_player_index(render.local_player_index);
	player = player_index != NONE ? (struct player_datum *)datum_try_and_get(player_data, player_index) : NULL;
	return player && object_try_and_get(player->unit_index) ? player->unit_index : NONE;
}

/* the carrier's unit (an absolute player index), NONE for none */
static long objective_carrier_unit(short carrier)
{
	struct data_iterator iterator;
	struct player_datum *player;

	if (carrier == NONE || !player_data)
		return NONE;
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		if ((short)DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index) == carrier)
			return object_try_and_get(player->unit_index) ? player->unit_index : NONE;
	}
	return NONE;
}

/* where a carried objective is shown: over its carrier (a rider's put away,
its own place left where they got in), else where it lies; FALSE for none */
static boolean objective_item_position(short carrier, short item, real_point3d *at)
{
	long unit = objective_carrier_unit(carrier);
	long object_index;

	if (unit != NONE)
	{
		struct object_datum *object = object_get(unit);

		object_get_origin(unit, at);
		at->z += object->object.bounding_sphere_radius + 0.6f;
		return TRUE;
	}
	object_index = item != NONE ? firefight_objectives_object(item) : NONE;
	if (object_index == NONE || !object_try_and_get(object_index))
		return FALSE;
	object_get_origin(object_index, at);
	at->z += 0.8f;
	return TRUE;
}

/* (a mark of the view's: an object lying about, its absolute index) drawn
over it, with its label */
static void draw_objective_object_mark(long font_index, short line_height, real scale, short absolute, char const *label,
	pixel32 box_color, real_argb_color const *text_color)
{
	long object_index = absolute != NONE ? firefight_objectives_object(absolute) : NONE;
	struct object_datum *object = object_index != NONE ? object_try_and_get(object_index) : NULL;
	real_point3d at;

	if (!object || object->object.parent_object_index != NONE)
		return;
	object_get_origin(object_index, &at);
	at.z += 0.6f;
	draw_world_navpoint(font_index, line_height, scale, &at, label, box_color, text_color, FALSE);
}

static void draw_objectives(
	long font_index,
	short line_height,
	real vx0,
	real vy0,
	real vw,
	real scale,
	real notice_top)
{
	struct firefight_objectives_view const *view = firefight_objectives_view_get();
	real_argb_color white = { 1.0f, 0.96f, 0.98f, 1.0f };
	real_argb_color red = { 1.0f, 1.0f, 0.35f, 0.35f };
	real_argb_color gold = { 1.0f, 1.0f, 0.85f, 0.25f };
	char title[96];
	char status[64];
	real y = 106.0f + (real)PIN(ff_state.generator_count, 0, FIREFIGHT_MAX_GENERATORS) * 28.0f;

	/* the cards, under the generators' */
	if (firefight_objectives_round_card(title, sizeof(title), status, sizeof(status)))
	{
		draw_objective_card(font_index, line_height, vx0, vy0, scale, y, "WAVE", title, status,
			view->round_state == _ff_objective_failed);
		y += 34.0f;
	}
	if (firefight_objectives_set_card(title, sizeof(title), status, sizeof(status), objective_local_absolute()))
	{
		draw_objective_card(font_index, line_height, vx0, vy0, scale, y, "ROUND", title, status,
			view->set_state == _ff_objective_failed || (view->set_flags & FF_OBJECTIVE_CONTESTED));
		/* (a hold's, a generator's, the marines' life: a bar under it) */
		if (view->set_state == _ff_objective_running && view->set_goal > 0 &&
			view->set_kind != _ff_set_objective_flag && view->set_kind != _ff_set_objective_purge &&
			view->set_kind != _ff_set_objective_target &&
			(view->set_kind != _ff_set_objective_bomb || (view->set_flags & FF_OBJECTIVE_PLANTED)))
		{
			real fraction = PIN((real)view->set_progress / (real)view->set_goal, 0.0f, 1.0f);

			ff_draw_quad(vx0, vy0, scale, 20.0f, y + 26.0f, 184.0f, y + 28.0f, FF_HUD_BAR_BG);
			ff_draw_quad(vx0, vy0, scale, 20.0f, y + 26.0f, 20.0f + 164.0f * fraction, y + 28.0f,
				view->set_kind == _ff_set_objective_generator ? FF_HUD_WARN_EDGE : FF_HUD_LOCK_BAR);
		}
	}

	/* the notice: what started, what was broken and by whom, what was won */
	if (view->notice_ticks > 0 && view->notice[0])
	{
		real bw = 300.0f;
		real bx = (vw / scale - bw) * 0.5f;
		/* (under the banners shown: a round's countdown and its skulls') */
		real by = MAX(96.0f, notice_top);

		ff_draw_box(vx0, vy0, scale, bx, by, bx + bw, by + 16.0f, 0.0f, FF_HUD_PANEL_FILL, FF_HUD_LOCK_BAR);
		ff_draw_text(font_index, line_height, vx0, vy0, scale, view->notice,
			bx + 6.0f, bx + bw - 6.0f, by + 2.0f, 0.48f, _ff_text_center, &white);
	}

	/* the markers */
	if (view->round_state == _ff_objective_running && view->round_kind == _ff_round_objective_kill_first &&
		view->round_target != NONE)
	{
		long target = firefight_objectives_object(view->round_target);

		if (target != NONE && object_try_and_get_and_verify_type(target, _object_mask_unit))
			draw_hostile_navpoint(font_index, line_height, scale, target, hostile_nav_arrow_index(), "FIRST", &red);
	}
	/* (Vehicle kills' vehicle, while this view's player is not in it) */
	if (view->round_state == _ff_objective_running && view->round_kind == _ff_round_objective_vehicle_kills &&
		view->round_target != NONE)
	{
		long vehicle = firefight_objectives_object(view->round_target);
		struct object_datum *object = vehicle != NONE ? object_try_and_get_and_verify_type(vehicle, _object_mask_vehicle) : NULL;
		long player_index = render.local_player_index != NONE ? local_player_get_player_index(render.local_player_index) : NONE;
		struct player_datum *player = player_index != NONE ? (struct player_datum *)datum_try_and_get(player_data, player_index) : NULL;
		struct object_datum *unit = player ? object_try_and_get(player->unit_index) : NULL;

		if (object && !TEST_FLAG(object->object.damage_flags, _object_dead_bit) &&
			(!unit || unit->object.parent_object_index != vehicle))
		{
			real_point3d at;

			object_get_origin(vehicle, &at);
			at.z += 1.5f;
			draw_world_navpoint(font_index, line_height, scale, &at, view->round_label, FF_HUD_LOCK_BAR, &gold, FALSE);
		}
	}
	/* (Travel's point; over a round objective's marker, should they share
	a spot, higher) */
	if (view->round_state == _ff_objective_running && view->round_kind == _ff_round_objective_travel)
	{
		real_point3d at = view->round_point;

		at.z += 1.0f;
		if (view->set_state == _ff_objective_running &&
			(distance3d(&view->round_point, &view->set_site) < 4.0f ||
				distance3d(&view->round_point, &view->set_goal_point) < 4.0f))
		{
			at.z += 1.6f;
		}
		draw_world_navpoint(font_index, line_height, scale, &at, "GO HERE", FF_HUD_LOCK_BAR, &gold, FALSE);
	}
	/* (Use X only's weapons lying about, while this view's player has none
	of it: one dropped dying, a pod's) */
	if (view->round_state == _ff_objective_running && view->round_kind == _ff_round_objective_weapon_only)
	{
		long unit = objective_local_unit();
		short index;
		long tag = NONE;

		for (index = 0; index < FF_OBJECTIVE_WEAPON_MARKS && tag == NONE; index++)
		{
			long weapon = view->round_marks[index] != NONE ? firefight_objectives_object(view->round_marks[index]) : NONE;
			struct object_datum *object = weapon != NONE ? object_try_and_get_and_verify_type(weapon, _object_mask_weapon) : NULL;

			if (object)
				tag = object->definition_index;
		}
		if (tag != NONE && (unit == NONE || !unit_has_weapon_definition_index(unit, tag)))
		{
			for (index = 0; index < FF_OBJECTIVE_WEAPON_MARKS; index++)
			{
				draw_objective_object_mark(font_index, line_height, scale, view->round_marks[index], view->round_label,
					FF_HUD_LOCK_BAR, &gold);
			}
		}
	}
	if (view->set_state != _ff_objective_running)
		return;
	switch (view->set_kind)
	{
	case _ff_set_objective_target:
	{
		long target = firefight_objectives_object(view->set_target);

		if (target != NONE && object_try_and_get_and_verify_type(target, _object_mask_unit))
			draw_hostile_navpoint(font_index, line_height, scale, target, hostile_nav_arrow_index(), "VIP", &red);
		break;
	}
	case _ff_set_objective_generator:
	{
		real_point3d at = view->set_site;

		at.z += 1.25f;
		draw_world_navpoint(font_index, line_height, scale, &at, "ENEMY GENERATOR", FF_HUD_WARN_EDGE, &red, FALSE);
		break;
	}
	case _ff_set_objective_purge:
	{
		real_point3d at = view->set_site;
		long unit = objective_local_unit();
		real_point3d from;
		short index;

		at.z += 1.0f;
		draw_world_navpoint(font_index, line_height, scale, &at, "ENEMY BASE", FF_HUD_WARN_EDGE, &red, FALSE);
		/* (its enemies, each, near enough) */
		if (unit == NONE)
			break;
		object_get_origin(unit, &from);
		for (index = 0; index < FF_OBJECTIVE_UNIT_MARKS; index++)
		{
			long enemy = view->set_units[index] != NONE ? firefight_objectives_object(view->set_units[index]) : NONE;
			struct object_datum *object = enemy != NONE ? object_try_and_get_and_verify_type(enemy, _object_mask_unit) : NULL;
			real_point3d head;

			if (!object || TEST_FLAG(object->object.damage_flags, _object_dead_bit))
				continue;
			object_get_origin(enemy, &head);
			if (distance3d(&head, &from) > FF_OBJECTIVE_UNIT_MARK_DISTANCE)
				continue;
			head.z += object->object.bounding_sphere_radius * 1.3f + 0.3f;
			draw_world_navpoint(font_index, line_height, scale, &head, "ELIMINATE", FF_HUD_WARN_EDGE, &red, FALSE);
		}
		break;
	}
	case _ff_set_objective_data_link:
	{
		real_point3d at = view->set_site;

		at.z += 1.0f;
		draw_world_navpoint(font_index, line_height, scale, &at, "DATA LINK",
			(view->set_flags & FF_OBJECTIVE_CONTESTED) ? FF_HUD_WARN_EDGE : FF_HUD_LOCK_BAR,
			(view->set_flags & FF_OBJECTIVE_CONTESTED) ? &red : &gold, FALSE);
		break;
	}
	case _ff_set_objective_flag:
	{
		real_point3d at = view->set_goal_point;

		at.z += 1.0f;
		if (objective_local_carrier(view->set_carrier))
			draw_world_navpoint(font_index, line_height, scale, &at, "CAPTURE", FF_HUD_LOCK_BAR, &gold, FALSE);
		else
		{
			real_point3d flag_at;

			if (objective_item_position(view->set_carrier, view->set_target, &flag_at))
			{
				draw_world_navpoint(font_index, line_height, scale, &flag_at,
					view->set_carrier != NONE ? "FLAG CARRIER" : "FLAG", FF_HUD_LOCK_BAR, &gold, FALSE);
			}
		}
		break;
	}
	case _ff_set_objective_collect:
	{
		real_point3d at = view->set_site;
		short index;

		at.z += 1.6f;
		draw_world_navpoint(font_index, line_height, scale, &at, "DEPOSIT", FF_HUD_LOCK_BAR, &gold, FALSE);
		/* (the skulls a player dropped, dying) */
		for (index = 0; index < FF_OBJECTIVE_DROP_MARKS; index++)
		{
			char label[32];

			snprintf(label, sizeof(label), "%d DROPPED SKULL%s", (int)view->set_drop_values[index],
				view->set_drop_values[index] == 1 ? "" : "S");
			draw_objective_object_mark(font_index, line_height, scale, view->set_drops[index], label,
				FF_HUD_LOCK_BAR, &gold);
		}
		break;
	}
	case _ff_set_objective_bomb:
	{
		real_point3d at = view->set_site;

		at.z += 1.0f;
		if (view->set_flags & FF_OBJECTIVE_PLANTED)
		{
			draw_world_navpoint(font_index, line_height, scale, &at, "BOMB",
				(view->set_flags & FF_OBJECTIVE_DISARMING) ? FF_HUD_WARN_EDGE : FF_HUD_LOCK_BAR,
				(view->set_flags & FF_OBJECTIVE_DISARMING) ? &red : &gold, FALSE);
			break;
		}
		draw_world_navpoint(font_index, line_height, scale, &at, "TARGET", FF_HUD_LOCK_BAR, &gold, FALSE);
		if (!objective_local_carrier(view->set_carrier))
		{
			real_point3d bomb_at;

			if (objective_item_position(view->set_carrier, view->set_target, &bomb_at))
			{
				draw_world_navpoint(font_index, line_height, scale, &bomb_at,
					view->set_carrier != NONE ? "BOMB CARRIER" : "BOMB", FF_HUD_LOCK_BAR, &gold, FALSE);
			}
		}
		break;
	}
	case _ff_set_objective_rescue:
		if (!(view->set_flags & FF_OBJECTIVE_RESCUED))
		{
			real_point3d at = view->set_site;

			at.z += 1.0f;
			draw_world_navpoint(font_index, line_height, scale, &at, "RESCUE", 0xE040E070, &white, FALSE);
		}
		break;
	case _ff_set_objective_oddball:
	{
		long target = firefight_objectives_object(view->set_target);

		if (target == NONE || objective_local_carrier(view->set_carrier))
			break;
		if (object_try_and_get_and_verify_type(target, _object_mask_unit))
			draw_hostile_navpoint(font_index, line_height, scale, target, hostile_nav_arrow_index(), "SKULL CARRIER", &red);
		else
		{
			real_point3d at;

			if (objective_item_position(view->set_carrier, view->set_target, &at))
			{
				draw_world_navpoint(font_index, line_height, scale, &at, view->set_carrier != NONE ? "SKULL HOLDER" : "SKULL",
					FF_HUD_LOCK_BAR, &gold, FALSE);
			}
		}
		break;
	}
	default:
		break;
	}
}

/* ---------- Main Firefight HUD Draw */

void firefight_hud_draw(void)
{
	long font_index;
	struct font_header *font;
	short line_height;
	real vx0, vy0, vw, vh, scale;
	real_argb_color white = { 1.0f, 0.96f, 0.98f, 1.0f };
	real_argb_color cyan = { 1.0f, 0.42f, 0.78f, 1.0f };
	real_argb_color gold = { 1.0f, 0.96f, 0.98f, 1.0f };
	real_argb_color red = { 1.0f, 1.0f, 0.35f, 0.35f };
	real_argb_color green = { 1.0f, 0.35f, 1.0f, 0.45f };
	boolean prep_phase;
	/* (the banners' bottom: the objectives' notice goes under it) */
	real banners_bottom = 0.0f;

	if (!firefight_active())
		return;

	font_index = hud_get_font_index();
	if (font_index == NONE)
		return;
	font = font_definition_get(font_index);
	if (!font)
		return;
	line_height = (short)(font->ascending_height + font->descending_height);
	if (line_height <= 0)
		return;

	vx0 = (real)render.camera.viewport_bounds.x0;
	vy0 = (real)render.camera.viewport_bounds.y0;
	vw = (real)(render.camera.viewport_bounds.x1 - render.camera.viewport_bounds.x0);
	vh = (real)(render.camera.viewport_bounds.y1 - render.camera.viewport_bounds.y0);
	scale = vh / 480.0f;
	if (scale <= 0.0f)
		return;

	prep_phase = (ff_state.phase_state == _ff_phase_pre_set || ff_state.phase_state == _ff_phase_pre_round);

	/* (the set, the round, the wave, the lives, the hostiles and the time are
	the score panel's: campaign_score.c) */

	/* 2. Pre-Round Preparation Countdown Banner */
	if (prep_phase && ff_state.phase_timer_ticks > 0)
	{
		real bw = 280.0f;
		real bh = 38.0f;
		real bx = (vw / scale - bw) * 0.5f;
		real by = 52.0f;
		long secs = (ff_state.phase_timer_ticks + TICKS_PER_SECOND - 1) / TICKS_PER_SECOND;
		char prep_title[64];
		short round_bits[NUMBER_OF_FIREFIGHT_SKULLS];
		short round_count = ff_skull_bits_of(ff_state.round_skulls, round_bits, NUMBER_OF_FIREFIGHT_SKULLS);

		ff_draw_box(vx0, vy0, scale, bx, by, bx + bw, by + bh, 19.0f, FF_HUD_PANEL_FILL, FF_HUD_LOCK_BAR);
		snprintf(prep_title, sizeof(prep_title), "ROUND %d STARTS IN %lds",
			(int)ff_state.current_round, MAX(1L, secs));
		ff_draw_text(font_index, line_height, vx0, vy0, scale,
			prep_title, bx + 8.0f, bx + bw - 8.0f, by + 4.0f,
			0.68f, _ff_text_center, &gold);
		ff_draw_text(font_index, line_height, vx0, vy0, scale,
			"RESUPPLY & BOARD VEHICLES", bx + 8.0f, bx + bw - 8.0f, by + 23.0f,
			0.51f, _ff_text_center, &cyan);

		if (round_count > 0)
		{
			char sk_title[96];
			char const *sk_desc = round_count == 1 ? s_ff_skull_hud_defs[round_bits[0]].desc : "ENEMY MODIFIERS ENGAGED";
			real sby = by + bh + 4.0f;
			real inset = ff_banner_well_width(round_count, 18.0f);
			short name;

			ff_draw_box_with_wells(vx0, vy0, scale, bx, sby, bx + bw, sby + 34.0f, 17.0f, inset, FF_HUD_WARN_FILL,
				s_ff_skull_hud_defs[round_bits[0]].badge_color);
			banners_bottom = sby + 34.0f + 4.0f;
			ff_draw_banner_skulls(vx0, vy0, scale, bx, bx + bw, sby + 8.0f, 18.0f, round_bits, round_count);
			snprintf(sk_title, sizeof(sk_title), "%s: ", round_count == 1 ? "SKULL ACTIVE" : "SKULLS ACTIVE");
			for (name = 0; name < round_count; name++)
			{
				if (name > 0)
					strncat(sk_title, " + ", sizeof(sk_title) - strlen(sk_title) - 1);
				strncat(sk_title, s_ff_skull_hud_defs[round_bits[name]].name, sizeof(sk_title) - strlen(sk_title) - 1);
			}
			ff_draw_text(font_index, line_height, vx0, vy0, scale,
				sk_title, bx + inset, bx + bw - inset, sby + 2.5f,
				0.58f, _ff_text_center, &gold);
			ff_draw_text(font_index, line_height, vx0, vy0, scale,
				sk_desc, bx + inset, bx + bw - inset, sby + 20.0f,
				0.48f, _ff_text_center, &white);
		}
	}
	/* 3. Wave / Boss / Skull / Ordnance Announcement Banner */
	else if (ff_state.banner_timer_ticks > 0 && ff_state.banner_type != _ff_banner_none)
	{
		char banner_title[64];
		char banner_sub[64];
		real bw = 280.0f;
		real bh = 38.0f;
		real bx = (vw / scale - bw) * 0.5f;
		real by = 52.0f;

		format_banner_strings(
			ff_state.banner_type,
			ff_state.banner_data,
			banner_title, sizeof(banner_title),
			banner_sub, sizeof(banner_sub));

		if (banner_title[0] != '\0')
		{
			boolean is_skull_b = (ff_state.banner_type == _ff_banner_skull_activated);
			pixel32 edge_c = is_skull_b && ff_state.banner_data >= 0 && ff_state.banner_data < NUMBER_OF_FIREFIGHT_SKULLS
				? s_ff_skull_hud_defs[ff_state.banner_data].badge_color
				: FF_HUD_PANEL_EDGE;

			ff_draw_box_with_wells(vx0, vy0, scale, bx, by, bx + bw, by + bh, 19.0f,
				is_skull_b ? ff_banner_well_width(1, 18.0f) : 0.0f,
				is_skull_b ? FF_HUD_WARN_FILL : FF_HUD_PANEL_FILL, edge_c);
			if (is_skull_b && ff_state.banner_data >= 0 && ff_state.banner_data < NUMBER_OF_FIREFIGHT_SKULLS)
			{
				short banner_bit = ff_state.banner_data;

				ff_draw_banner_skulls(vx0, vy0, scale, bx, bx + bw, by + 10.0f, 18.0f, &banner_bit, 1);
			}
			ff_draw_text(font_index, line_height, vx0, vy0, scale,
				banner_title, bx + 8.0f, bx + bw - 8.0f, by + 4.0f,
				0.68f, _ff_text_center, &gold);
			if (banner_sub[0] != '\0')
			{
				ff_draw_text(font_index, line_height, vx0, vy0, scale,
					banner_sub, bx + 8.0f, bx + bw - 8.0f, by + 23.0f,
					0.51f, _ff_text_center, &cyan);
			}
		}
	}

	/* 4. Generator Defense Status Bars */
	draw_generator_bars(font_index, line_height, vx0, vy0, scale);
	/* (and the objectives', under them) */
	draw_objectives(font_index, line_height, vx0, vy0, vw, scale, banners_bottom);

	/* 5. 3D->2D World-to-Screen Navpoints for Stragglers (<= 3 enemies left), Dropships & Resupply Drops */
	if (ff_state.straggler_navpoints_active && ff_state.enemies_alive > 0 && ff_state.enemies_alive <= FIREFIGHT_STRAGGLER_THRESHOLD)
	{
		short red_arrow_index = hostile_nav_arrow_index();
		short i;

		for (i = 0; i < FIREFIGHT_MAX_TRACKED_ENEMIES; i++)
		{
			long unit_index = ff_state.tracked_enemy_units[i];
			struct unit_datum *unit;

			if (unit_index == NONE)
				continue;
			unit = (struct unit_datum *)object_try_and_get_and_verify_type(unit_index, _object_mask_unit);
			if (!unit || unit->object.body_vitality <= 0.0f || TEST_FLAG(unit->object.damage_flags, _object_dead_bit))
				continue;

			draw_hostile_navpoint(font_index, line_height, scale, unit_index, red_arrow_index, "HOSTILE", &red);
		}
	}

	/* (the whole flight, in and out: it's a thing to watch) */
	if (ff_state.active_dropship.phase != 0 &&
		ff_state.active_dropship.vehicle_index != NONE &&
		object_try_and_get_and_verify_type(ff_state.active_dropship.vehicle_index, _object_mask_vehicle))
	{
		draw_hostile_navpoint(
			font_index, line_height, scale,
			ff_state.active_dropship.vehicle_index,
			hostile_nav_arrow_index(),
			/* (a client is told of the ship alone: its tag says which) */
			strstr(tag_get_name(object_get(ff_state.active_dropship.vehicle_index)->definition_index), "pelican") ?
				"PELICAN" : "DROPSHIP",
			&red);
	}
	/* (the Flood's drop, Keyes' form growing: each machine's own scenery) */
	{
		long flood_form = firefight_flood_drop_object();

		if (flood_form != NONE && object_try_and_get_and_verify_type(flood_form, _object_mask_scenery))
			draw_hostile_navpoint(font_index, line_height, scale, flood_form, hostile_nav_arrow_index(), "FLOOD", &red);
	}

	if (ff_state.ordnance_navpoint_ticks > 0)
	{
		/* (the pods of the drop, where each lands: the host's spots, which
		its clients have too, firefight_network.c; port: this player's own
		set alone, less those it has been to since they landed) */
		short pods = (short)PIN(ff_state.ordnance_pod_count, 1, FIREFIGHT_MAX_ORDNANCE_PODS);
		short pod;

		for (pod = 0; pod < pods; pod++)
		{
			real_point3d ord_pos = ff_state.ordnance_pod_positions[pod];
			char const *label;

			if (!firefight_hud_own_pod(pod, pods) || firefight_hud_pod_visited(pod))
				continue;
			switch (ff_state.ordnance_pod_kinds[pod])
			{
			case _ff_pod_kind_ammo_crate: label = "AMMO"; break;
			case _ff_pod_kind_health: label = "HEALTH"; break;
			case _ff_pod_kind_overshield: label = "OVERSHIELD"; break;
			default: label = ff_state.last_ordnance_is_heavy ? "HEAVY ORDNANCE" : "ORDNANCE"; break;
			}

			ord_pos.z += 0.4f;
			draw_world_navpoint(
				font_index, line_height, scale,
				&ord_pos,
				label,
				0xE040E070, &green, TRUE);
		}
	}
}

void firefight_hud_post_rasterize(void)
{
}

void firefight_format_scoreboard_title(
	wchar_t *title_string,
	long count,
	boolean postgame)
{
	char narrow[160];
	char time_str[24];
	long i;

	(void)postgame;
	if (!title_string || count <= 0)
		return;

	format_elapsed_time(ff_state.session_elapsed_ticks, time_str, sizeof(time_str));
	snprintf(
		narrow,
		sizeof(narrow),
		"FIREFIGHT | SET %d RND %d WAVE %d | LIVES: %d | TIME: %s | SCORE: %ld",
		(int)ff_state.current_set,
		(int)ff_state.current_round,
		(int)ff_state.current_wave,
		(int)ff_state.shared_lives_remaining,
		time_str,
		campaign_score_team_points());

	for (i = 0; i < count - 1 && narrow[i]; i++)
		title_string[i] = (wchar_t)(unsigned char)narrow[i];
	title_string[i] = 0;
}

boolean firefight_hud_format_scoreboard_title(
	wchar_t *title_string,
	long count)
{
	if (!firefight_active() || !title_string || count <= 0)
		return FALSE;

	firefight_format_scoreboard_title(title_string, count, FALSE);
	return TRUE;
}

