/*
AE_DEV_HIT_MARKERS.C

DEV MODE's hit markers (ae_dev_hit_markers.h; the rules: ae_dev_hit_markers_rules.c).
*/

#include <math.h>

#include "cseries.h"
#include "cseries/cseries_windows.h"
#include "game/players.h"
#include "objects/objects.h"
#include "render/render.h"
#include "units/units.h"

#include "ae_dev_mode.h"
#include "ae_dev_hit_markers_rules.h"
#include "ae_hud_digits.h"
#include "ae_panel_draw.h"
#include "ae_score_strip.h"
#include "ae_test_hooks.h"
#include "ae_dev_hit_markers.h"

void platform_log(char const *format, ...);

/* ---------- constants */

/* (pixels at 1080p, times the HUD's scale) the brackets' half-size with no reticle drawn, their gap outside the
reticle, a corner's arm, the stroke; a kill's brackets this much further out and its stroke this much wider */
#define AE_DEV_HIT_MARKER_DEFAULT_HALF 24.0f
#define AE_DEV_HIT_MARKER_GAP 6.0f
#define AE_DEV_HIT_MARKER_ARM 10.0f
#define AE_DEV_HIT_MARKER_STROKE 3.0f
#define AE_DEV_HIT_MARKER_KILL_OUT 4.0f
#define AE_DEV_HIT_MARKER_KILL_STROKE 1.0f
/* the HUD's keyline: 1 px each side, dark at 0.90 (ae_panel_draw.h) */
#define AE_DEV_HIT_MARKER_KEYLINE 2.0f
#define AE_DEV_HIT_MARKER_KEYLINE_ALPHA 0.90f
/* a centred piece bigger than this share of the view's height is a scope's overlay, not the reticle */
#define AE_DEV_HIT_MARKER_RETICLE_MOST 0.25f

/* ---------- globals */

static struct
{
	/* when the marker started (system_milliseconds), and whether it is a kill's; never started: active FALSE */
	unsigned long start_ms;
	boolean active;
	boolean kill;
	/* drawn once (for the log) */
	boolean drawn;
	/* this frame's reticle half-extent (HUD units), 0 until a piece is drawn */
	real reticle_half;
} markers[MAXIMUM_LOCAL_PLAYERS];

/* ---------- public code */

void ae_dev_hit_markers_note(long owner_player_index, long object_index, float damage, int kill)
{
	struct player_datum *player;
	struct object_datum *object;
	struct unit_datum *unit;
	short local_player_index;
	float alpha, scale;

	if (!ae_dev_mode_on() || owner_player_index == NONE || object_index == NONE)
		return;
	player = player_get(owner_player_index);
	local_player_index = player->local_player_index;
	object = object_get(object_index);
	unit = player->unit_index == NONE ? NULL : unit_try_and_get(player->unit_index);
	if (!ae_dev_hit_marker_counts(local_player_index >= 0 && local_player_index < MAXIMUM_LOCAL_PLAYERS,
			object_index == player->unit_index, unit && unit->object.parent_object_index == object_index, damage, kill,
			TEST_FLAG(object->object.damage_flags, _object_dead_bit) && !kill))
		return;

	/* (a hit never cuts a kill's marker short) */
	if (markers[local_player_index].active && markers[local_player_index].kill && !kill)
	{
		ae_dev_hit_marker_shape((long)(system_milliseconds() - markers[local_player_index].start_ms), TRUE, &alpha,
			&scale);
		if (alpha > 0.0f)
			return;
	}
	markers[local_player_index].start_ms = system_milliseconds();
	markers[local_player_index].active = TRUE;
	markers[local_player_index].kill = kill != 0;
	markers[local_player_index].drawn = FALSE;
	platform_log("dev hit markers: %s by local player %d on object %08lx (%.1f damage)", kill ? "kill" : "hit",
		local_player_index, object_index, damage);
	/* (debug.capture_on_event's hit_marker and hit_marker_kill: a shot of it while it shows) */
	ae_test_capture(kill ? "hit_marker_kill" : "hit_marker", 40);
}

void ae_dev_hit_markers_reticle(short x, short y, const real_rectangle2d *bounds, const real_vector2d *xy_scale,
	float reticle_scale)
{
	short local_player_index = render.local_player_index;
	real centre_x, centre_y, view_height, half, extent;

	if (local_player_index < 0 || local_player_index >= MAXIMUM_LOCAL_PLAYERS || !ae_dev_mode_on())
		return;
	/* (as hud_draw_bitmap_internal places a piece: the view's centre in HUD units) */
	centre_x = (render.camera.window_bounds.x1 + render.camera.window_bounds.x0) / 2 - render.camera.viewport_bounds.x0;
	centre_y = (render.camera.window_bounds.y1 + render.camera.window_bounds.y0) / 2 - render.camera.viewport_bounds.y0;
	view_height = (real)(render.camera.viewport_bounds.y1 - render.camera.viewport_bounds.y0);
	half = 0.0f;
	extent = (real)fabs(x + bounds->x0 * xy_scale->i - centre_x);
	half = MAX(half, extent);
	extent = (real)fabs(x + bounds->x1 * xy_scale->i - centre_x);
	half = MAX(half, extent);
	extent = (real)fabs(y + bounds->y0 * xy_scale->j - centre_y);
	half = MAX(half, extent);
	extent = (real)fabs(y + bounds->y1 * xy_scale->j - centre_y);
	half = MAX(half, extent);
	half *= reticle_scale;
	if (half < view_height * AE_DEV_HIT_MARKER_RETICLE_MOST)
		markers[local_player_index].reticle_half = MAX(markers[local_player_index].reticle_half, half);
}

void ae_dev_hit_markers_draw(short local_player_index)
{
	real ppu, s, centre_x, centre_y, half, arm, stroke, ends;
	float alpha, scale;
	short pass, corner, tone;
	real reticle_half;

	if (local_player_index < 0 || local_player_index >= MAXIMUM_LOCAL_PLAYERS)
		return;
	reticle_half = markers[local_player_index].reticle_half;
	markers[local_player_index].reticle_half = 0.0f;
	if (!markers[local_player_index].active || !ae_dev_mode_on())
		return;
	ae_dev_hit_marker_shape((long)(system_milliseconds() - markers[local_player_index].start_ms),
		markers[local_player_index].kill, &alpha, &scale);
	if (alpha <= 0.0f)
	{
		markers[local_player_index].active = FALSE;
		return;
	}

	ppu = ae_hud_pixels_per_unit();
	s = ae_score_strip_view_scale();
	if (ppu <= 0.0f)
		return;
	centre_x = ((render.camera.window_bounds.x1 + render.camera.window_bounds.x0) / 2 -
		render.camera.viewport_bounds.x0) * ppu;
	centre_y = ((render.camera.window_bounds.y1 + render.camera.window_bounds.y0) / 2 -
		render.camera.viewport_bounds.y0) * ppu;
	half = ae_dev_hit_marker_half_size(reticle_half * ppu, AE_DEV_HIT_MARKER_DEFAULT_HALF, AE_DEV_HIT_MARKER_GAP, s);
	stroke = AE_DEV_HIT_MARKER_STROKE * s;
	if (markers[local_player_index].kill)
	{
		half += AE_DEV_HIT_MARKER_KILL_OUT * s;
		stroke += AE_DEV_HIT_MARKER_KILL_STROKE * s;
	}
	half *= scale;
	arm = AE_DEV_HIT_MARKER_ARM * s * scale;
	tone = markers[local_player_index].kill ? _ae_tone_red : _ae_tone_white;

	/* the keylines first, then the strokes, so no corner's keyline crosses another's stroke; each arm runs half a
	stroke past its corner, so the corner closes */
	ae_panel_draw_set_fade(alpha);
	for (pass = 0; pass < 2; pass++)
	{
		real width = pass ? stroke : stroke + AE_DEV_HIT_MARKER_KEYLINE * s;

		ends = width / 2.0f;
		for (corner = 0; corner < 4; corner++)
		{
			real sx = (corner & 1) ? 1.0f : -1.0f, sy = (corner & 2) ? 1.0f : -1.0f;
			real x = centre_x + sx * half, y = centre_y + sy * half;

			ae_panel_draw_line(x, y - sy * arm, x, y + sy * ends, width, pass ? tone : _ae_tone_dark,
				pass ? 1.0f : AE_DEV_HIT_MARKER_KEYLINE_ALPHA, FALSE);
			ae_panel_draw_line(x - sx * arm, y, x + sx * ends, y, width, pass ? tone : _ae_tone_dark,
				pass ? 1.0f : AE_DEV_HIT_MARKER_KEYLINE_ALPHA, FALSE);
		}
	}
	ae_panel_draw_set_fade(1.0f);
	if (!markers[local_player_index].drawn)
	{
		markers[local_player_index].drawn = TRUE;
		platform_log("dev hit markers: %s drawn in view %d, %.0f px round a reticle of %.0f", markers[local_player_index].kill ?
			"kill" : "hit", local_player_index, half, reticle_half * ppu);
	}
}
