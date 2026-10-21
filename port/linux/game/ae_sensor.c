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
#include "cseries/cseries_windows.h"
#include "cseries/errors.h"
#include "math/real_math.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "interface/hud.h"
#include "interface/hud_definitions.h"
#include "main/main.h"
#include "render/render.h"

#include "ae_hud_digits.h"
#include "ae_hud_style.h"
#include "ae_hud_text.h"
#include "ae_panel_draw.h"
#include "ae_panel_engine.h"
#include "ae_panel_layout.h"
#include "ae_panel_model.h"
#include "ae_panels.h"
#include "ae_score_strip.h"
#include "ae_sensor.h"
#include "ae_sensor_geometry.h"
#include "ae_sensor_range_rules.h"

int config_boolean(const char *name);
void platform_log(char const *format, ...);
long game_time_get(void);
long hud_unit_port_script_flags(void);

#define RIM_UNITS 42.0f                 /* CE's sweep radius, HUD units */
#define RIM_UNITS_SPLIT 32.0f
#define DESIGN_RIM 92.0f                /* the spec's 1p rim, px: blip sizes are given for it */
#define AE_HUD_PANEL_SENSOR_BLINK 5      /* hud_unit.c's _hud_panel_motion_sensor_blink_bit (hud_blink_motion_sensor) */
#define LABEL_LEFT_OF_CENTRE 1.033f     /* x r: the label's left edge (1p x 142 for a centre at 237, r 92) */
#define AE_PI 3.14159265f
#define CHEVRON_PX 14.0f   /* the player's chevron, px at scale 1 (owner 10-15: rev13's 10 read as a speck in play) */

static struct { boolean valid; real x0, y0, x1, y1; unsigned long drawn_at; } last[MAXIMUM_LOCAL_PLAYERS];

/* (the last draw's rectangle, only while the sensor is being drawn: the panels' rule (ae_panels.c), so that waypoint
labels do not fade under a sensor a gametype turned off, an old view's layout or another map's) */
#define LAST_DRAW_MILLISECONDS 250

boolean ae_sensor_active(void)
{
	return ae_hud_style_is_ae();
}

/* CE draws its own contacts, exactly as on its own sensor (its blip texture, added light, trail and sweep), over
AE's disc (owner 2026-10-15: "clone exactly how ce does the dots"; AE's own dots were harder to track) */
boolean ae_sensor_takes_blips(void)
{
	return FALSE;
}

boolean ae_sensor_take_blip(const union real_point2d *position, real fade, real size,
	const union real_rgb_color *color, boolean custom, real sensor_scale)
{
	(void)position; (void)fade; (void)size; (void)color; (void)custom; (void)sensor_scale;
	return FALSE;
}

boolean ae_sensor_rectangle(short local_player_index, real *x0, real *y0, real *x1, real *y1)
{
	if (local_player_index < 0 || local_player_index >= MAXIMUM_LOCAL_PLAYERS || !last[local_player_index].valid ||
		system_milliseconds() - last[local_player_index].drawn_at >= LAST_DRAW_MILLISECONDS)
	{
		return FALSE;
	}
	*x0 = last[local_player_index].x0;
	*y0 = last[local_player_index].y0;
	*x1 = last[local_player_index].x1;
	*y1 = last[local_player_index].y1;
	return TRUE;
}

/* the scripts' blink (hud_blink_motion_sensor) while the disc is drawn: every alpha x this */
static real sensor_fade = 1.0f;

static void color_of(short tone, real alpha, real_argb_color *color)
{
	real rgb[3];

	ae_panel_tone_rgb(tone, rgb);
	color->alpha = alpha * sensor_fade;
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
	real line = 1.6f * (k > 0.75f ? k : 0.75f);   /* the strip's line (1.6 px x scale) */
	real_argb_color color;
	char label_ascii[8];
	wchar_t label[8];
	short i;

	if (!ae_sensor_active() || local_player_index < 0 || local_player_index >= MAXIMUM_LOCAL_PLAYERS)
		return;
	/* the scripts' blink bit (hud_blink_motion_sensor; campaign review 2026-10-18 I1): the whole disc dimmed on the
	panels' beat, in its own blue (CE's sweep and contacts, drawn after, keep theirs) */
	sensor_fade = ae_panel_script_blink_fade((boolean)(TEST_FLAG(hud_unit_port_script_flags(),
		AE_HUD_PANEL_SENSOR_BLINK) || ae_panels_forced_script_blink()), game_time_get(),
		ae_panel_engine_reduced_motion());
	ae_panel_draw_set_fade(sensor_fade);
	ae_hud_backing_draw(local_player_index, cx, cy, 1.18f * r, 1.18f * r, 0.45f * sensor_fade);
	ae_panel_draw_disc(cx, cy, r, _ae_tone_blue, 0.08f, FALSE);   /* (light, as the draft: the lines' dark shows) */
	/* the view's wedge: its fill, then its two edges from the centre to the rim */
	wedge_fill(cx, cy, r, half, 0.10f);
	/* the inner ring at half the range: the strip's recipe, the line at 0.70 */
	ae_panel_draw_ring(cx, cy, inner, line, _ae_tone_blue, 0.70f, TRUE);
	/* the dividers from the inner ring to the rim: left, right, behind, in the strip's recipe at 3/4 of the line;
	left and right hidden past a 75 degree half-angle (G4) */
	for (i = 0; i < 3; i++)
	{
		real ux = i == 0 ? -1.0f : i == 1 ? 1.0f : 0.0f, uy = i == 2 ? 1.0f : 0.0f;

		if (i < 2 && !ae_sensor_side_dividers_shown(half))
			continue;
		ae_panel_draw_line(cx + ux * inner, cy + uy * inner, cx + ux * r, cy + uy * r, line * 0.75f, _ae_tone_blue,
			0.55f, TRUE);
	}
	for (i = 0; i < 2; i++)
	{
		real angle = (-90.0f + (i ? half : -half)) * AE_PI / 180.0f;

		/* (from the chevron's shoulder out: the lines never run under it) */
		real from = CHEVRON_PX * (k > 0.8f ? k : 0.8f) * 0.45f;

		ae_panel_draw_line(cx + from * (real)cos(angle), cy + from * (real)sin(angle), cx + r * (real)cos(angle),
			cy + r * (real)sin(angle), line, _ae_tone_blue, 0.85f, TRUE);
	}
	/* the rim: the outline rule (1.5 px at 0.85, keylined both sides) */
	ae_panel_draw_ring(cx, cy, r - line / 2.0f, line, _ae_tone_blue, 0.85f, TRUE);
	/* (CE's own sweep comes with its contacts, drawn after this: motion_sensor_draw_screen) */
	/* the player: a solid chevron pointing up, CHEVRON_PX tall, centred on the wedge's point and drawn over the wedge's lines
	with a strong soft dark rim, so the lines end at its shoulders (rev13's drawing; owner 10-15) */
	{
		real h = CHEVRON_PX * (k > 0.8f ? k : 0.8f);
		real chevron[4][2];

		chevron[0][0] = cx; chevron[0][1] = cy - 0.55f * h;
		chevron[1][0] = cx + 0.42f * h; chevron[1][1] = cy + 0.45f * h;
		chevron[2][0] = cx; chevron[2][1] = cy + 0.18f * h;
		chevron[3][0] = cx - 0.42f * h; chevron[3][1] = cy + 0.45f * h;
		color_of(_ae_tone_dark, 0.92f, &color);
		ae_panel_fill_polygon_soft((const real (*)[2])chevron, 4, 2.0f, &color);
		color_of(_ae_tone_blue, 1.0f, &color);
		ae_panel_fill_polygon_soft((const real (*)[2])chevron, 4, 0.0f, &color);
	}
	/* the range label: grey, its left edge where CE's sits, on the shared bottom line */
	ae_sensor_range_label(hud_globals->defaults.motion_sensor_range, label_ascii);
	for (i = 0; label_ascii[i] && i < 7; i++)
		label[i] = (wchar_t)label_ascii[i];
	label[i] = 0;
	color_of(_ae_tone_grey, 1.0f, &color);
	ae_hud_text_draw_ex(local_player_index, cx - LABEL_LEFT_OF_CENTRE * r, ae_score_strip_shared_line_y(local_player_index),
		label, ae_panel_label_px(r / DESIGN_RIM), &color, _ae_text_outline);
	ae_panel_draw_set_fade(1.0f);
	sensor_fade = 1.0f;

	last[local_player_index].valid = TRUE;
	last[local_player_index].drawn_at = system_milliseconds();
	last[local_player_index].x0 = cx - 1.18f * r;
	last[local_player_index].y0 = cy - 1.18f * r;
	last[local_player_index].x1 = cx + 1.18f * r;
	last[local_player_index].y1 = cy + 1.18f * r;
	if (ae_panel_engine_log_on() && game_time_get() / 60 != logged[local_player_index])
	{
		logged[local_player_index] = game_time_get() / 60;
		platform_log("ae sensor: tick %ld view %d centre=%.1f,%.1f r=%.1f inner=%.1f half=%.1f vfov=%.4f view=%.0fx%.0f "
			"label=%s baseline=%.1f range=%.3f scale=%.3f", logged[local_player_index] * 60, local_player_index, cx, cy, r, inner,
			half, render.camera.vertical_field_of_view, view_w, view_h, label_ascii,
			ae_score_strip_shared_line_y(local_player_index), hud_globals->defaults.motion_sensor_range,
			hud_globals->defaults.motion_sensor_scale);
	}
}

/* (game_engine.h names the two bits the rules header reads) */
typedef char ae_sensor_range_bits_check[_game_variant_sensor_range_first_bit == AE_SENSOR_RANGE_SHIFT &&
	_game_variant_sensor_range_second_bit == AE_SENSOR_RANGE_SHIFT + 1 ? 1 : -1];

/* SENSOR RANGE (ae_sensor_range_rules.h): the game's range into the map's HUD globals, as they load for the game
(hud.c's hud_initialize_for_new_map, after game.c's game_engine_initialize took the variant: the host's, on a
network client too). A gametype at STOCK, or no game (a campaign, the main menu), puts the map's own back */
static struct ae_sensor_range_memory memory;

void ae_sensor_range_apply(
	void)
{
	int range = game_engine_running() ? ae_sensor_range_get(game_engine_get_variant()->universal_variant.flags) :
		_ae_sensor_range_stock;
	real *field;

	if (!hud_globals)
		return;
	field = &hud_globals->defaults.motion_sensor_range;
	*field = ae_sensor_range_next(&memory, main_get_map_name(), field, *field, range);
	if (game_engine_running())
	{
		if (range == _ae_sensor_range_stock)
			error(_error_silent, "AE motion sensor range: stock (%.2f world units)", (double)*field);
		else
			error(_error_silent, "AE motion sensor range: %d m (%.2f world units; the map's own %.2f)",
				ae_sensor_range_metres(range), (double)*field, (double)memory.stock);
	}
}

float ae_sensor_range_map_own(
	float current)
{
	return memory.valid ? memory.stock : current;
}
