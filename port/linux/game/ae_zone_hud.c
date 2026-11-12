/* The round's danger zone on the HUD (ae_zone_hud.h) */
#include "cseries.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "cutscene/cinematics.h"
#include "interface/hud.h"
#include "interface/hud_messaging.h"
#include "objects/objects.h"
#include "render/render.h"
#include "units/units.h"

#include "ae_hud_style.h"
#include "ae_hud_text.h"
#include "ae_lobby_countdown.h"
#include "ae_round_zone_rules.h"
#include "ae_score_strip.h"
#include "ae_sound.h"
#include "ae_zone_hud.h"
#include "ae_zone_view.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

/* the HUD's pixels for a HUD unit (ae_hud_style.c's), and the shapes of ae_hud_text.c */
real ae_hud_pixels_per_unit(void);
void platform_log(char const *format, ...);

#define METRES_PER_WORLD_UNIT 3.048f
#define GRID_COLUMNS 16
#define GRID_ROWS 9
/* the tint's ramp-in on entering the zone's outside, in ticks */
#define TINT_RAMP_TICKS 8
/* inside, the edge line shows within this many world units of the edge */
#define EDGE_LINE_RANGE 10.0f

struct view_state
{
	long boundary;          /* the last damage tick seen (elapsed / 30) */
	long pulse_elapsed;     /* the elapsed tick it started at, -1 none */
	boolean outside_before;
	long outside_since;     /* tick the view went outside, -1 not outside */
	int warning;            /* the last warning said */
	int phase;
	long zone_start_mark;   /* a restarted zone resets the state */
};
static struct view_state states[MAXIMUM_NUMBER_OF_LOCAL_PLAYERS];

static boolean alive(short local_player_index, real_point3d *at)
{
	long player_index = local_player_get_player_index(local_player_index);
	struct player_datum *player = player_index == NONE ? NULL : player_try_and_get(player_index);
	struct object_datum *unit;

	if (!player || player->unit_index == NONE)
		return FALSE;
	unit = object_try_and_get_and_verify_type(player->unit_index, _object_mask_unit);
	if (!unit || TEST_FLAG(unit->object.damage_flags, _object_dead_bit))
		return FALSE;
	object_get_origin(player->unit_index, at);
	return TRUE;
}

/* the countdown line baseline, a part of the view height down: just above the strip (bottom centre) */
#define ZONE_LINE_DOWN 0.855f

/* the view-wide shade: the tint's shape at the given strength, per grid corner */
static void shade(real width, real height, real red, real green, real blue, real peak)
{
	int column, row;

	ae_hud_quads_begin();
	for (row = 0; row < GRID_ROWS; row++)
	{
		for (column = 0; column < GRID_COLUMNS; column++)
		{
			real points[4][2];
			real_argb_color colors[4];
			int corner;
			boolean any = FALSE;

			for (corner = 0; corner < 4; corner++)
			{
				int c = column + (corner == 1 || corner == 2);
				int r = row + (corner >= 2);
				real alpha = ae_zone_tint_alpha((real)c / GRID_COLUMNS, (real)r / GRID_ROWS) / AE_ZONE_TINT_ALPHA;

				points[corner][0] = width * (real)c / GRID_COLUMNS;
				points[corner][1] = height * (real)r / GRID_ROWS;
				colors[corner].alpha = alpha * peak;
				colors[corner].red = red;
				colors[corner].green = green;
				colors[corner].blue = blue;
				any = (boolean)(any || colors[corner].alpha > 0.002f);
			}
			if (any)
				ae_hud_fill_quad(points, colors);
		}
	}
	ae_hud_quads_end();
}

/* an arrow pointing the way (angle: 0 up the view, positive clockwise), size px, centred on (x, y) */
static void arrow(real x, real y, real size, real angle, real_argb_color const *color)
{
	real points[4][2];
	real tip[2] = { 0.0f, -1.0f }, left[2] = { -0.7f, 0.8f }, notch[2] = { 0.0f, 0.4f }, right[2] = { 0.7f, 0.8f };
	real *shape[4] = { tip, left, notch, right };
	real s = (real)sin(angle), c = (real)cos(angle);
	int i;

	for (i = 0; i < 4; i++)
	{
		points[i][0] = x + (shape[i][0] * c - shape[i][1] * s) * size * 0.5f;
		points[i][1] = y + (shape[i][0] * s + shape[i][1] * c) * size * 0.5f;
	}
	ae_hud_fill_quad_color(points, color);
}

/* the arrow's angle from the view's heading to the point */
static real way_to(real from_x, real from_y, real to_x, real to_y)
{
	real heading = (real)atan2(render.camera.forward.j, render.camera.forward.i);
	real bearing = (real)atan2(to_y - from_y, to_x - from_x);
	real angle = heading - bearing;

	/* (the world's counter-clockwise bearings, the screen's clockwise) */
	while (angle > _pi) angle -= 2.0f * _pi;
	while (angle < -_pi) angle += 2.0f * _pi;
	return angle;
}

static void line_draw(short view, real centre_x, real baseline, char const *text, real px, real_argb_color const *color,
	boolean with_arrow, real angle)
{
	wchar_t wide[64];
	real width, x, arrow_size = px * 1.35f, gap = px * 0.45f;
	int at;

	for (at = 0; text[at] && at < 63; at++)
		wide[at] = (wchar_t)(unsigned char)text[at];
	wide[at] = 0;
	width = ae_hud_text_measure(wide, px);
	x = centre_x - (width + (with_arrow ? arrow_size + gap : 0.0f)) * 0.5f;
	if (with_arrow)
	{
		arrow(x + arrow_size * 0.5f, baseline - px * 0.35f, arrow_size, angle, color);
		x += arrow_size + gap;
	}
	ae_hud_text_draw_ex(view, x, baseline, wide, px, color, _ae_text_outline);
}

void ae_zone_hud_draw(short local_player_index)
{
	struct ae_zone_view zone;
	struct view_state *state;
	real_point3d at;
	real ppu = ae_hud_pixels_per_unit(), width, height, distance, reference, edge, k;
	real_argb_color white, soft;
	long now = game_time_get();
	boolean outside, near_line;
	int warning;

	if (local_player_index < 0 || local_player_index >= MAXIMUM_NUMBER_OF_LOCAL_PLAYERS || !(ppu > 0.0f) ||
		!game_engine_running() || cinematic_in_progress() || !ae_zone_view_get(&zone))
	{
		return;
	}
	white.alpha = soft.alpha = 1.0f;
	white.red = white.green = white.blue = 1.0f;
	soft.red = 0.80f; soft.green = 0.90f; soft.blue = 1.0f;
	state = &states[local_player_index];
	if (!zone.running)
	{
		memset(state, 0, sizeof(*state));
		state->pulse_elapsed = -1;
		state->outside_since = -1;
		return;
	}
	if (zone.elapsed / AE_ZONE_TICKS_PER_SECOND < state->boundary)
	{
		/* (a new round: the zone started again) */
		memset(state, 0, sizeof(*state));
		state->pulse_elapsed = -1;
		state->outside_since = -1;
	}
	/* the warnings: 30 s and 10 s before a closing, and the closing (one line each; a tick at 10 s on the first view) */
	warning = ae_zone_warning(&zone.plan, zone.elapsed);
	if (warning != state->warning || zone.phase != state->phase)
	{
		if (warning > 0 && (warning > state->warning || zone.phase != state->phase) && !(warning == 1 && state->warning > 1))
		{
			/* (no message of its own: the line over the strip carries the warning, flashing at 30 s and 10 s) */
			platform_log("zone hud: view %d warning %d (zone tick %ld)", (int)local_player_index, warning, zone.elapsed);
			if (warning == 2 && local_player_index == 0)
			{
				ae_glue_sound_countdown(AE_BEEP_TICK, 10);
				platform_log("zone hud: tick sound at 10 s");
			}
		}
		state->warning = warning;
		state->phase = zone.phase;
	}
	/* the zone's countdown, a line centred just above the strip (the strip's clock cell is the round's clock) */
	{
		real line_width = (real)(render.camera.viewport_bounds.x1 - render.camera.viewport_bounds.x0) * ppu;
		real line_height = (real)(render.camera.viewport_bounds.y1 - render.camera.viewport_bounds.y0) * ppu;
		real line_k = line_height / 1080.0f;
		char text[48];
		long seconds = (ae_zone_phase_ticks_left(&zone.plan, zone.elapsed) + AE_ZONE_TICKS_PER_SECOND - 1) / AE_ZONE_TICKS_PER_SECOND;

		real_argb_color const *line_colour = &soft;
		long left_ticks = ae_zone_phase_ticks_left(&zone.plan, zone.elapsed);

		if (line_k < 0.55f)
			line_k = 0.55f;
		/* the 30 s and 10 s marks: the line flashes white and a little larger (the first 3 s of the 30 s mark, all of the last 10 s) */
		if ((warning == 2 || (warning == 1 && left_ticks > 27L * AE_ZONE_TICKS_PER_SECOND)) && (now / 8) % 2 == 0)
		{
			line_colour = &white;
			line_k *= 1.15f;
		}
		if (zone.phase == AE_ZONE_PHASE_WAIT || zone.phase == AE_ZONE_PHASE_WAIT_2)
			snprintf(text, sizeof(text), "DANGER ZONE CLOSING IN %ld", seconds);
		else if (zone.phase == AE_ZONE_PHASE_CLOSING)
			snprintf(text, sizeof(text), "DANGER ZONE CLOSING");
		else
			snprintf(text, sizeof(text), "FINAL ZONE");
		if (line_width > 0.0f && line_height > 0.0f)
			line_draw(local_player_index, line_width * 0.5f, line_height * ZONE_LINE_DOWN, text, 17.0f * line_k, line_colour, FALSE, 0.0f);
	}
	if (!alive(local_player_index, &at))
	{
		state->outside_before = FALSE;
		state->outside_since = -1;
		return;
	}
	width = (real)(render.camera.viewport_bounds.x1 - render.camera.viewport_bounds.x0) * ppu;
	height = (real)(render.camera.viewport_bounds.y1 - render.camera.viewport_bounds.y0) * ppu;
	if (!(width > 0.0f && height > 0.0f))
		return;
	k = height / 1080.0f;
	if (k < 0.55f)
		k = 0.55f;
	edge = ae_zone_edge_distance(zone.centre_x, zone.centre_y, zone.radius, at.x, at.y);
	outside = (boolean)(edge < 0.0f);

	/* the damage tick: the host hurts everyone outside once a second; the pulse is on this view's tick if it was out */
	{
		long boundary = zone.elapsed / AE_ZONE_TICKS_PER_SECOND;

		if (boundary > state->boundary)
		{
			state->boundary = boundary;
			if (boundary > 0 && (outside || state->outside_before))
			{
				state->pulse_elapsed = boundary * AE_ZONE_TICKS_PER_SECOND;
				platform_log("zone hud: view %d damage tick (outside, %.1f wu past the edge)", (int)local_player_index, -edge);
			}
		}
	}
	state->outside_before = outside;
	if (outside && state->outside_since < 0)
		state->outside_since = now;
	if (!outside)
		state->outside_since = -1;

	if (outside)
	{
		real ramp = (real)(now - state->outside_since) / (real)TINT_RAMP_TICKS;
		real away = -edge * METRES_PER_WORLD_UNIT;
		char text[48];

		if (ramp > 1.0f) ramp = 1.0f;
		shade(width, height, 175.0f / 255.0f, 90.0f / 255.0f, 1.0f, AE_ZONE_TINT_ALPHA * ramp);
		if (state->pulse_elapsed >= 0)
		{
			real pulse = ae_zone_pulse_alpha(zone.elapsed - state->pulse_elapsed);

			if (pulse > 0.0f)
				shade(width, height, 0.42f, 0.0f, 0.02f, pulse);
		}
		line_draw(local_player_index, width * 0.5f, height * 0.17f, "RETURN TO THE ZONE", 30.0f * k, &white, FALSE, 0.0f);
		snprintf(text, sizeof(text), "%.0fM", away < 1.0f ? 1.0f : away);
		line_draw(local_player_index, width * 0.5f, height * 0.17f + 30.0f * k, text, 20.0f * k, &soft, TRUE,
			way_to(at.x, at.y, zone.centre_x, zone.centre_y));
		return;
	}
	/* inside: a closing's edge, or the next ring's while the wait is nearly over, near it */
	reference = zone.radius;
	near_line = FALSE;
	if (zone.phase == AE_ZONE_PHASE_CLOSING || zone.phase == AE_ZONE_PHASE_CLOSING_2)
		near_line = TRUE;
	else if ((zone.phase == AE_ZONE_PHASE_WAIT || zone.phase == AE_ZONE_PHASE_WAIT_2) &&
		ae_zone_phase_ticks_left(&zone.plan, zone.elapsed) <= 10L * AE_ZONE_TICKS_PER_SECOND)
	{
		reference = zone.next_radius;
		near_line = TRUE;
	}
	if (near_line)
	{
		real to_edge = ae_zone_edge_distance(zone.centre_x, zone.centre_y, reference, at.x, at.y);

		if (to_edge < EDGE_LINE_RANGE)
		{
			char text[48];
			boolean beyond = (boolean)(to_edge < 0.0f);
			real metres = (beyond ? -to_edge : to_edge) * METRES_PER_WORLD_UNIT;

			snprintf(text, sizeof(text), "%s %.0fM", reference == zone.radius ? "ZONE EDGE" : beyond ? "OUT OF THE NEXT ZONE" :
				"NEXT ZONE EDGE", metres < 1.0f ? 1.0f : metres);
			line_draw(local_player_index, width * 0.5f, height * 0.745f, text, 20.0f * k, &white, TRUE,
				way_to(at.x, at.y, zone.centre_x, zone.centre_y));
		}
	}
}
