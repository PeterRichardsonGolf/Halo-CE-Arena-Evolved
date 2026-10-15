/*
AE_SENSOR.C

HUD STYLE AE's motion sensor (ae_sensor.h; spec 14.15 part 4, G3, G4, G8).
The rim is CE's sweep's radius (rasterizer_xbox_motion_sensor.c's
_rasterizer_hud_motion_sensor_blip_end: 42 HUD units, 32 in split screen);
the centre is the HUD's blip placement (hud_unit.c's hud_calculate_point).
*/

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "cseries/cseries.h"
#include "math/real_math.h"
#include "game/players.h"
#include "interface/hud.h"
#include "interface/hud_definitions.h"
#include "render/render.h"

#include "ae_hud_digits.h"
#include "ae_hud_style.h"
#include "ae_hud_text.h"
#include "ae_panel_draw.h"
#include "ae_panel_layout.h"
#include "ae_score_strip.h"
#include "ae_sensor.h"
#include "ae_sensor_geometry.h"

int config_boolean(const char *name);
void platform_log(char const *format, ...);
long game_time_get(void);

#define RIM_UNITS 42.0f                 /* CE's sweep radius, HUD units */
#define RIM_UNITS_SPLIT 32.0f
#define DESIGN_RIM 92.0f                /* the spec's 1p rim, px: blip sizes are given for it */
#define NEWEST_BLIP_PX 3.4f             /* a medium contact's newest sample's radius at the design rim */
#define LABEL_LEFT_OF_CENTRE 1.033f     /* x r: the label's left edge (1p x 142 for a centre at 237, r 92) */
#define MAXIMUM_TAKEN_BLIPS 512
#define AE_PI 3.14159265f

struct taken_blip { real x, y, fade, size; real rgb[3]; boolean custom; };
static struct { struct taken_blip blips[MAXIMUM_TAKEN_BLIPS]; long count; real sensor_scale; } taken;
static struct { boolean valid; real x0, y0, x1, y1; } last[MAXIMUM_LOCAL_PLAYERS];

boolean ae_sensor_active(void)
{
	return ae_hud_style_is_ae();
}

boolean ae_sensor_takes_blips(void)
{
	/* (debug.ae_sensor_with_ce: CE's own sensor drawn too, to lay AE's blips on CE's) */
	return (boolean)(ae_sensor_active() && !config_boolean("debug.ae_sensor_with_ce"));
}

boolean ae_sensor_take_blip(const union real_point2d *position, real fade, real size,
	const union real_rgb_color *color, boolean custom, real sensor_scale)
{
	if (!ae_sensor_active())
		return FALSE;
	if (taken.count < MAXIMUM_TAKEN_BLIPS)
	{
		struct taken_blip *blip = &taken.blips[taken.count++];

		blip->x = position->x;
		blip->y = position->y;
		blip->fade = fade;
		blip->size = size;
		blip->rgb[0] = color->red;
		blip->rgb[1] = color->green;
		blip->rgb[2] = color->blue;
		blip->custom = custom;
	}
	taken.sensor_scale = sensor_scale;
	return ae_sensor_takes_blips();
}

boolean ae_sensor_rectangle(short local_player_index, real *x0, real *y0, real *x1, real *y1)
{
	if (local_player_index < 0 || local_player_index >= MAXIMUM_LOCAL_PLAYERS || !last[local_player_index].valid)
		return FALSE;
	*x0 = last[local_player_index].x0;
	*y0 = last[local_player_index].y0;
	*x1 = last[local_player_index].x1;
	*y1 = last[local_player_index].y1;
	return TRUE;
}

static void color_of(short tone, real alpha, real_argb_color *color)
{
	real rgb[3];

	ae_panel_tone_rgb(tone, rgb);
	color->alpha = alpha;
	color->red = rgb[0];
	color->green = rgb[1];
	color->blue = rgb[2];
}

/* the wedge's fill: a sector from the centre to the rim, up the screen, of half-angle half */
static void wedge_fill(real cx, real cy, real r, real half, real alpha)
{
	real_argb_color color;
	short steps = 16, i;
	real a0 = (-90.0f - half) * AE_PI / 180.0f, a1 = (-90.0f + half) * AE_PI / 180.0f;

	color_of(_ae_tone_blue, alpha, &color);
	for (i = 0; i < steps; i++)
	{
		real points[4][2];
		real t0 = a0 + (a1 - a0) * (real)i / (real)steps, t1 = a0 + (a1 - a0) * (real)(i + 1) / (real)steps;

		points[0][0] = cx; points[0][1] = cy;
		points[1][0] = cx + r * (real)cos(t0); points[1][1] = cy + r * (real)sin(t0);
		points[2][0] = cx + r * (real)cos(t1); points[2][1] = cy + r * (real)sin(t1);
		points[3][0] = points[2][0]; points[3][1] = points[2][1];
		ae_hud_fill_quad_color(points, &color);
	}
}

/* the chevron (tip, right foot, notch, left foot) as two triangles from its tip: it is not convex, so it is never
handed to the quad path whole */
static void chevron_fill(const real points[4][2], const union real_argb_color *color)
{
	real triangle[4][2];
	short side, j;

	for (side = 0; side < 2; side++)
	{
		short corner[3] = { 0, side ? 2 : 1, side ? 3 : 2 };

		for (j = 0; j < 3; j++)
		{
			triangle[j][0] = points[corner[j]][0];
			triangle[j][1] = points[corner[j]][1];
		}
		triangle[3][0] = triangle[2][0];
		triangle[3][1] = triangle[2][1];
		ae_hud_fill_quad_color(triangle, color);
	}
}

static void blips(real cx, real cy, real r, real k)
{
	long i;
	real_argb_color dark;
	short pass;

	/* history first (larger and fainter), then the newest samples keylined, on top of everything */
	for (pass = 0; pass < 2; pass++)
	{
		for (i = 0; i < taken.count; i++)
		{
			struct taken_blip *b = &taken.blips[i];
			boolean newest = (boolean)(b->fade >= 0.999f);
			real dx, dy, radius = b->size * NEWEST_BLIP_PX * k;
			real_argb_color color;

			if (newest != (pass == 1) || b->fade <= 0.01f)
				continue;
			ae_sensor_blip_offset(b->x, b->y, taken.sensor_scale, r, &dx, &dy);
			if (radius < 1.2f)
				radius = 1.2f;
			color.alpha = newest ? 1.0f : b->fade;
			color.red = b->rgb[0];
			color.green = b->rgb[1];
			color.blue = b->rgb[2];
			if (newest)
			{
				color_of(_ae_tone_dark, 0.90f, &dark);
				ae_panel_draw_disc(cx + dx, cy + dy, radius + 1.0f, _ae_tone_dark, 0.90f, FALSE);
			}
			/* (colour by its own rgb: the tokens hold no blip colours) */
			ae_panel_fill_disc(cx + dx, cy + dy, radius, &color);
		}
	}
}

void ae_sensor_draw(short local_player_index, const union point2d *centre, boolean split)
{
	static long logged[MAXIMUM_LOCAL_PLAYERS] = { -1, -1, -1, -1 };
	real ppu = ae_hud_pixels_per_unit();
	real r = (split ? RIM_UNITS_SPLIT : RIM_UNITS) * ppu;
	real cx = (real)centre->x * ppu, cy = (real)centre->y * ppu;
	real k = r / DESIGN_RIM;
	real view_w = (real)(render.camera.viewport_bounds.x1 - render.camera.viewport_bounds.x0) * ppu;
	real view_h = (real)(render.camera.viewport_bounds.y1 - render.camera.viewport_bounds.y0) * ppu;
	real half = ae_sensor_wedge_half_angle(render.camera.vertical_field_of_view, view_w, view_h);
	real inner = ae_sensor_inner_ring(r);
	real line = 1.5f * (k > 0.67f ? k : 0.67f);
	real sweep_r, sweep_a, seconds = (real)game_time_get() / 30.0f;
	real_argb_color color;
	char label_ascii[8];
	wchar_t label[8];
	short i;

	if (!ae_sensor_active() || local_player_index < 0 || local_player_index >= MAXIMUM_LOCAL_PLAYERS)
	{
		taken.count = 0;
		return;
	}
	ae_hud_backing_draw(local_player_index, cx, cy, 1.18f * r, 1.18f * r, 0.45f);
	ae_panel_draw_disc(cx, cy, r, _ae_tone_blue, 0.08f, FALSE);
	/* the view's wedge: its fill, then its two edges from the centre to the rim */
	wedge_fill(cx, cy, r, half, 0.10f);
	/* the inner ring at half the range (G3: 1.5 px at 0.60, keylined) */
	ae_panel_draw_ring(cx, cy, inner, line, _ae_tone_blue, 0.60f, TRUE);
	/* the dividers from the inner ring to the rim: left, right, behind; blue at 0.50 and a 1 px DARK line at 0.60 on
	the lower / right side (G3); left and right hidden past a 75 degree half-angle (G4) */
	for (i = 0; i < 3; i++)
	{
		real ux = i == 0 ? -1.0f : i == 1 ? 1.0f : 0.0f, uy = i == 2 ? 1.0f : 0.0f;
		real ox = i == 2 ? 1.0f : 0.0f, oy = i == 2 ? 0.0f : 1.0f;

		if (i < 2 && !ae_sensor_side_dividers_shown(half))
			continue;
		ae_panel_draw_line(cx + ux * inner + ox, cy + uy * inner + oy, cx + ux * r + ox, cy + uy * r + oy, 1.0f,
			_ae_tone_dark, 0.60f, FALSE);
		ae_panel_draw_line(cx + ux * inner, cy + uy * inner, cx + ux * r, cy + uy * r, 1.0f, _ae_tone_blue, 0.50f,
			FALSE);
	}
	for (i = 0; i < 2; i++)
	{
		real angle = (-90.0f + (i ? half : -half)) * AE_PI / 180.0f;

		ae_panel_draw_line(cx, cy, cx + r * (real)cos(angle), cy + r * (real)sin(angle), line, _ae_tone_blue, 0.70f,
			TRUE);
	}
	/* the rim: the outline rule (1.5 px at 0.85, keylined both sides) */
	ae_panel_draw_ring(cx, cy, r - line / 2.0f, line, _ae_tone_blue, 0.85f, TRUE);
	/* CE's sweep, drawn as a 1 px ring */
	ae_sensor_sweep(seconds, r, &sweep_r, &sweep_a);
	if (sweep_r > 1.0f)
		ae_panel_draw_ring(cx, cy, sweep_r, 1.0f, _ae_tone_blue, sweep_a, FALSE);
	/* the player: a solid chevron pointing up, 10 tall, keylined, its TIP on the wedge's point so the wedge's lines
	end there and never cross it (rev13; owner 10-15: it read as an "x") */
	{
		real h = 10.0f * k;
		real mid_y = cy + h * 0.6f;   /* the shape's middle, for the keyline's growth */
		real chevron[4][2];
		real keyed[4][2];
		short j;

		chevron[0][0] = cx; chevron[0][1] = cy;
		chevron[1][0] = cx + 0.45f * h; chevron[1][1] = cy + h;
		chevron[2][0] = cx; chevron[2][1] = cy + 0.7f * h;
		chevron[3][0] = cx - 0.45f * h; chevron[3][1] = cy + h;
		for (j = 0; j < 4; j++)
		{
			keyed[j][0] = cx + (chevron[j][0] - cx) * (1.0f + 2.0f / h);
			keyed[j][1] = mid_y + (chevron[j][1] - mid_y) * (1.0f + 2.0f / h);
		}
		color_of(_ae_tone_dark, 0.90f, &color);
		chevron_fill(keyed, &color);
		color_of(_ae_tone_blue, 1.0f, &color);
		chevron_fill(chevron, &color);
	}
	/* CE's contacts, on top of everything (incl. the chevron) */
	blips(cx, cy, r, k);
	/* the range label: grey, its left edge where CE's sits, on the shared bottom line */
	ae_sensor_range_label(hud_globals->defaults.motion_sensor_range, label_ascii);
	for (i = 0; label_ascii[i] && i < 7; i++)
		label[i] = (wchar_t)label_ascii[i];
	label[i] = 0;
	color_of(_ae_tone_grey, 1.0f, &color);
	ae_hud_text_draw_ex(local_player_index, cx - LABEL_LEFT_OF_CENTRE * r, ae_score_strip_shared_line_y(local_player_index),
		label, ae_panel_label_px(r / DESIGN_RIM), &color, _ae_text_outline);

	last[local_player_index].valid = TRUE;
	last[local_player_index].x0 = cx - 1.18f * r;
	last[local_player_index].y0 = cy - 1.18f * r;
	last[local_player_index].x1 = cx + 1.18f * r;
	last[local_player_index].y1 = cy + 1.18f * r;
	if (config_boolean("debug.ae_panel_log") && game_time_get() / 60 != logged[local_player_index])
	{
		logged[local_player_index] = game_time_get() / 60;
		platform_log("ae sensor: tick %ld view %d centre=%.1f,%.1f r=%.1f inner=%.1f half=%.1f vfov=%.4f view=%.0fx%.0f "
			"blips=%ld label=%s baseline=%.1f range=%.3f scale=%.3f", logged[local_player_index] * 60, local_player_index, cx, cy, r, inner,
			half, render.camera.vertical_field_of_view, view_w, view_h, taken.count, label_ascii,
			ae_score_strip_shared_line_y(local_player_index), hud_globals->defaults.motion_sensor_range,
			taken.sensor_scale);
	}
	taken.count = 0;
}
