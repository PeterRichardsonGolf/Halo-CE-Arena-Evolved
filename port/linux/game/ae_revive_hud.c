/*
AE_REVIVE_HUD.C

ELIMINATION's revive on screen (ae_revive_hud.h). Everything is the view's pixels (x from its left, y from its top) at 1080 lines' sizes
times the view's scale (ae_score_strip_view_scale), as the markers' (ae_marker_draw.c). The state is the round state's (ae_round_view:
the host's own, or a client's from message 106), so every machine draws the same; the host's rules (ae_round.c) are not read here.
*/

#include <math.h>
#include <string.h>
#include <wchar.h>

#include "cseries/cseries.h"
#include "math/real_math.h"
#include "bitmaps/bitmap_color_conversion.h"
#include "bitmaps/bitmaps.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "interface/hud.h"
#include "memory/data.h"
#include "objects/objects.h"
#include "render/render.h"
#include "render/render_camera_projection.h"
#include "text/unicode.h"
#include "units/units.h"

#include "ae_hud_digits.h"
#include "ae_hud_style.h"
#include "ae_hud_text.h"
#include "ae_item_prompt.h"
#include "ae_killfeed.h"
#include "ae_revive_hud.h"
#include "ae_revive_hud_rules.h"
#include "ae_round_internal.h"
#include "ae_score_strip.h"
#include "ae_variant_ext.h"
#include "ae_waypoint_icons.h"
#include "ae_waypoint_ink.h"

struct bitmap_data const *hud_item_timers_ae_glyph(short glyph);
boolean game_engine_scoreboard_held(short local_player_index);
real game_engine_scoreboard_shown(short local_player_index);

#define PI_F 3.14159265f
/* the marker's sizes at 1080 lines (M1: the open plus, its ring, the rows over it) */
#define MARKER_PLUS 11.0f
#define MARKER_ARM 3.0f
#define MARKER_HOLE 3.0f
#define MARKER_RING 13.0f
#define MARKER_RING_LINE 2.6f
#define MARKER_NAME_PX 15.0f
#define MARKER_DISTANCE_PX 13.0f
#define MARKER_ROW_GAP 6.0f
#define MARKER_TEXT_FLOOR 11.0f
/* the marker floats this far over the spot (world units) */
#define MARKER_LIFT 0.55f
/* the downed helmet lockup: the helmet's glyph square, its distance from the top, the texts */
#define HELMET_SQUARE 81.0f
#define HELMET_RING_OF_SQUARE 0.593f
#define HELMET_TOP 30.0f
#define TITLE_PX 30.0f
#define LINE_PX 17.0f
#define LOCKUP_GAP 16.0f

static real const white_rgb[3] = { 229.0f / 255.0f, 240.0f / 255.0f, 255.0f / 255.0f };
static real const dark_rgb[3] = { 6.0f / 255.0f, 10.0f / 255.0f, 18.0f / 255.0f };

/* ---------- the view */

struct view
{
	real ppu, s, width, height;
};

static boolean view_get(struct view *view)
{
	view->ppu = ae_hud_pixels_per_unit();
	view->s = ae_score_strip_view_scale();
	view->width = (real)(render.camera.viewport_bounds.x1 - render.camera.viewport_bounds.x0) * view->ppu;
	view->height = (real)(render.camera.viewport_bounds.y1 - render.camera.viewport_bounds.y0) * view->ppu;
	return (boolean)(view->ppu > 0.0f && view->s > 0.0f && view->width > 0.0f && view->height > 0.0f);
}

static real_argb_color color_of(real const rgb[3], real alpha)
{
	real_argb_color color;

	color.alpha = alpha < 0.0f ? 0.0f : alpha > 1.0f ? 1.0f : alpha;
	color.red = rgb[0];
	color.green = rgb[1];
	color.blue = rgb[2];
	return color;
}

static pixel32 premultiplied(real const rgb[3], real alpha)
{
	real_argb_color color = color_of(rgb, alpha);

	color.red *= color.alpha;
	color.green *= color.alpha;
	color.blue *= color.alpha;
	return real_argb_color_to_pixel32(&color);
}

/* ---------- shapes */

/* a filled axis rectangle */
static void rectangle_fill(real x0, real y0, real x1, real y1, real_argb_color const *color)
{
	real points[4][2];

	points[0][0] = x0; points[0][1] = y0;
	points[1][0] = x1; points[1][1] = y0;
	points[2][0] = x1; points[2][1] = y1;
	points[3][0] = x0; points[3][1] = y1;
	ae_hud_fill_quad_color(points, color);
}

/* an arc of the ring (degrees clockwise from the top) of this width about (cx, cy), in steps of at most 4 degrees */
static void arc_band(real cx, real cy, real radius, real width, real from, real to, real_argb_color const *color)
{
	short steps = (short)ceil((to - from) / 4.0f);
	short step;
	real inner = radius - width / 2.0f, outer = radius + width / 2.0f;

	if (steps < 1)
		steps = 1;
	for (step = 0; step < steps; step++)
	{
		real a0 = (from + (to - from) * (real)step / (real)steps) * PI_F / 180.0f;
		real a1 = (from + (to - from) * (real)(step + 1) / (real)steps) * PI_F / 180.0f;
		real points[4][2];

		points[0][0] = cx + inner * (real)sin(a0); points[0][1] = cy - inner * (real)cos(a0);
		points[1][0] = cx + outer * (real)sin(a0); points[1][1] = cy - outer * (real)cos(a0);
		points[2][0] = cx + outer * (real)sin(a1); points[2][1] = cy - outer * (real)cos(a1);
		points[3][0] = cx + inner * (real)sin(a1); points[3][1] = cy - inner * (real)cos(a1);
		ae_hud_fill_quad_color(points, color);
	}
}

enum ring_look
{
	RING_REVIVE,	/* the lit segments in the team's colour, the filled ones white */
	RING_BLEED,		/* the lit segments in the team's colour, the others dark */
	RING_BUTTON		/* all white, the unfilled dim */
};

/* the six segments about (cx, cy): the first white ones white, the rest by the look; each over a dark keyline */
static void segment_ring(real cx, real cy, real radius, real width, int white, int lit, real const colour[3], real alpha,
	enum ring_look look)
{
	real_argb_color dark = color_of(dark_rgb, 0.85f * alpha);
	int index;

	ae_hud_quads_begin();
	for (index = 0; index < AE_REVIVE_SEGMENTS; index++)
	{
		float from, to;

		ae_revive_segment_span(index, &from, &to);
		arc_band(cx, cy, radius, width + 2.0f, from - 0.8f, to + 0.8f, &dark);
	}
	for (index = 0; index < AE_REVIVE_SEGMENTS; index++)
	{
		float from, to;
		real_argb_color paint;

		ae_revive_segment_span(index, &from, &to);
		if (index < white)
			paint = color_of(white_rgb, alpha);
		else if (look == RING_BUTTON)
			paint = color_of(white_rgb, 0.45f * alpha);
		else if (look == RING_BLEED && !ae_revive_segment_is_lit(index, lit))
			paint = color_of(dark_rgb, 0.55f * alpha);
		else
			paint = color_of(colour, 0.85f * alpha);
		arc_band(cx, cy, radius, width, from, to, &paint);
	}
	ae_hud_quads_end();
}

/* the open plus (arms of the width from the hole outwards), dark under it */
static void open_plus(real cx, real cy, real half, real arm, real hole, real const colour[3], real alpha)
{
	real_argb_color dark = color_of(dark_rgb, 0.85f * alpha), paint = color_of(colour, alpha);
	real a = arm / 2.0f, h = hole / 2.0f;
	int pass;

	ae_hud_quads_begin();
	for (pass = 0; pass < 2; pass++)
	{
		real grow = pass == 0 ? 1.0f : 0.0f;
		real_argb_color const *fill = pass == 0 ? &dark : &paint;

		rectangle_fill(cx - half - grow, cy - a - grow, cx - h + grow, cy + a + grow, fill);
		rectangle_fill(cx + h - grow, cy - a - grow, cx + half + grow, cy + a + grow, fill);
		rectangle_fill(cx - a - grow, cy - half - grow, cx + a + grow, cy - h + grow, fill);
		rectangle_fill(cx - a - grow, cy + h - grow, cx + a + grow, cy + half + grow, fill);
	}
	ae_hud_quads_end();
}

/* a text centred on x with its baseline, outlined */
static real text_centred(short view, wchar_t const *text, real x, real baseline, real px, real const colour[3], real alpha)
{
	real_argb_color color = color_of(colour, alpha);
	real width = ae_hud_text_measure(text, px);

	ae_hud_text_draw_ex(view, (real)floor(x - width / 2.0f + 0.5f), (real)floor(baseline + 0.5f), text, px, &color, _ae_text_outline);
	return width;
}

/* ---------- what the round says */

static struct player_datum *player_of_slot(long slot)
{
	struct data_iterator iterator;
	struct player_datum *player;

	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		if (DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index) == slot)
			return player;
	}
	return NULL;
}

static void name_of(struct player_datum const *player, wchar_t *name, int size)
{
	int index;

	for (index = 0; index < size - 1 && index < (int)NUMBEROF(player->name) && player->name[index]; index++)
		name[index] = player->name[index];
	name[index] = 0;
}

static struct ae_round_wire_player const *entry_of(struct ae_round_wire const *view, long slot)
{
	int index;

	for (index = 0; index < view->count; index++)
	{
		if (view->players[index].player == slot)
			return &view->players[index];
	}
	return NULL;
}

/* the round state's, when a round mode runs its LIVE phase and the HUD is AE's */
static struct ae_round_wire const *live_view(short local_player_index)
{
	struct ae_round_wire const *view;

	if (!game_engine_running() || !ae_round_on() || !ae_hud_style_is_ae() || !hud_scripted_globals || !hud_scripted_globals->show_hud)
		return NULL;
	view = ae_round_view();
	if (!view || view->phase != AE_ROUND_PHASE_LIVE || !(view->flags & AE_ROUND_WIRE_TEAMS))
		return NULL;
	if (game_engine_scoreboard_shown(local_player_index) > 0.0f || game_engine_scoreboard_held(local_player_index))
		return NULL;
	return view;
}

boolean ae_revive_hud_owns_marker(long player_index)
{
	struct ae_round_wire const *view;
	struct ae_round_wire_player const *entry;

	if (player_index == NONE || !ae_hud_style_is_ae() || !ae_round_on() || !(view = ae_round_view()) ||
		view->phase != AE_ROUND_PHASE_LIVE)
	{
		return FALSE;
	}
	entry = entry_of(view, DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index));
	return (boolean)(entry && entry->state == AE_ROUND_PLAYER_DOWN);
}

/* ---------- the marker over a downed teammate */

static void marker_draw(short local_player_index, struct view const *view, struct player_datum const *downed,
	struct ae_round_wire_player const *entry, struct player_datum const *me)
{
	real_point3d spot, view_position;
	real_point2d screen;
	real_argb_color name_color;
	real colour[3];
	real k = view->s, x, y, distance, ring_outer, baseline;
	wchar_t name[NUMBEROF(downed->name) + 1], text[16];
	real name_px = MARKER_NAME_PX * k < MARKER_TEXT_FLOOR ? MARKER_TEXT_FLOOR : MARKER_NAME_PX * k;
	real distance_px = MARKER_DISTANCE_PX * k < MARKER_TEXT_FLOOR ? MARKER_TEXT_FLOOR : MARKER_DISTANCE_PX * k;
	real_point3d camera;
	real_vector3d away;

	(void)me;
	spot.x = ae_round_wire_real(entry->spot[0]);
	spot.y = ae_round_wire_real(entry->spot[1]);
	spot.z = ae_round_wire_real(entry->spot[2]) + MARKER_LIFT;
	matrix4x3_transform_point(&render.frustum.world_to_view, &spot, &view_position);
	if (!render_camera_view_to_screen(&render.camera, &render.frustum, &view_position, &screen))
		return;
	x = (screen.x - (real)render.camera.viewport_bounds.x0) * view->ppu;
	y = (screen.y - (real)render.camera.viewport_bounds.y0) * view->ppu;
	camera = render.camera.position;
	away.i = spot.x - camera.x;
	away.j = spot.y - camera.y;
	away.k = spot.z - camera.z;
	distance = (real)sqrt(away.i * away.i + away.j * away.j + away.k * away.k);
	hud_player_name_color(TRUE, &name_color);
	colour[0] = name_color.red;
	colour[1] = name_color.green;
	colour[2] = name_color.blue;
	segment_ring(x, y, MARKER_RING * k, MARKER_RING_LINE * k, ae_revive_white_segments(entry->progress),
		AE_REVIVE_SEGMENTS, colour, 0.95f, RING_REVIVE);
	open_plus(x, y, MARKER_PLUS * k, MARKER_ARM * k, MARKER_HOLE * k, colour, 0.95f);
	name_of(downed, name, (int)NUMBEROF(name));
	ring_outer = (MARKER_RING + MARKER_RING_LINE / 2.0f) * k;
	baseline = y - ring_outer - MARKER_ROW_GAP * k;
	{
		wchar_t label[NUMBEROF(name) + 8];

		wcscpy(label, L"REVIVE ");
		wcscat(label, name);
		text_centred(local_player_index, label, x, baseline, name_px, colour, 1.0f);
	}
	ae_revive_distance_text(ae_revive_metres(distance), text, (int)NUMBEROF(text));
	text_centred(local_player_index, text, x, baseline - name_px * 0.95f - 3.0f * k, distance_px, white_rgb, 0.95f);
}

/* ---------- the button's ring */

void ae_revive_hud_button_ring(real cx, real cy, real radius, int white_segments, real alpha)
{
	struct view view;
	real width;

	if (!view_get(&view))
		return;
	width = radius * 0.17f < 1.6f ? 1.6f : radius * 0.17f;
	segment_ring(cx, cy, radius, width, white_segments, AE_REVIVE_SEGMENTS, white_rgb, alpha, RING_BUTTON);
}

/* ---------- the prompt for a downed teammate in reach */

static void prompt_update(short local_player_index, struct ae_round_wire const *round, long viewer_slot, struct player_datum const *me)
{
	struct unit_datum const *unit = me->unit_index != NONE ? unit_try_and_get(me->unit_index) : NULL;
	struct ae_round_wire_player const *mine = entry_of(round, viewer_slot);
	struct ae_round_wire_player const *best = NULL;
	real best_distance = 0.0f;
	real_point3d at;
	int index;

	if (!unit || !mine || mine->state != AE_ROUND_PLAYER_ALIVE)
	{
		ae_item_prompt_revive_clear(local_player_index);
		return;
	}
	object_get_origin(me->unit_index, &at);
	for (index = 0; index < round->count; index++)
	{
		struct ae_round_wire_player const *entry = &round->players[index];
		struct player_datum const *downed;
		real dx, dy, dz, distance;

		if (entry->state != AE_ROUND_PLAYER_DOWN || entry->player == viewer_slot || !(downed = player_of_slot(entry->player)) ||
			downed->team_index != me->team_index)
		{
			continue;
		}
		dx = at.x - ae_round_wire_real(entry->spot[0]);
		dy = at.y - ae_round_wire_real(entry->spot[1]);
		dz = at.z - ae_round_wire_real(entry->spot[2]);
		if (!ae_revive_in_reach(dx, dy, dz))
			continue;
		distance = dx * dx + dy * dy;
		if (!best || distance < best_distance)
		{
			best = entry;
			best_distance = distance;
		}
	}
	if (best)
	{
		wchar_t name[NUMBEROF(me->name) + 1];
		struct player_datum const *downed = player_of_slot(best->player);

		name_of(downed, name, (int)NUMBEROF(name));
		/* (the ring fills with the revive only while this player is the one holding) */
		if (!ae_item_prompt_revive(local_player_index, name,
			best->reviver == viewer_slot ? ae_revive_white_segments(best->progress) : 0))
		{
			ae_item_prompt_revive_clear(local_player_index);
		}
	}
	else
		ae_item_prompt_revive_clear(local_player_index);
}

/* ---------- the downed player's own view */

static void own_view_draw(short local_player_index, struct view const *view, struct ae_round_wire const *round,
	struct ae_round_wire_player const *mine, struct player_datum const *me)
{
	struct bitmap_data const *helmet = hud_item_timers_ae_glyph(AE_WAYPOINT_GLYPH_HELMET_DOWNED);
	real k = view->s, square = HELMET_SQUARE * k, ring_radius = square * HELMET_RING_OF_SQUARE;
	real title_px = TITLE_PX * k, line_px = LINE_PX * k;
	real colour[3];
	real_argb_color name_color;
	wchar_t title[] = L"DOWNED";
	wchar_t line1[48], line2[48];
	int mode = ae_revive_self_mode(mine->reviver < AE_ROUND_WIRE_PLAYERS ? mine->reviver : -1);
	int total = ae_round_bleedout_seconds(ae_variant_ext_game());
	int white = 0, lit = AE_REVIVE_SEGMENTS;
	boolean bleeding = (boolean)(mine->bleed > 0 && total > 0);
	real_rectangle2d whole;
	real title_width, text_width, left, cx, cy, text_x, baseline;
	wchar_t reviver_name[NUMBEROF(me->name) + 1];
	real_argb_color painter;
	short pass;
	static real const around[8][2] = { { 1.0f, 0.0f }, { -1.0f, 0.0f }, { 0.0f, 1.0f }, { 0.0f, -1.0f },
		{ 0.7071f, 0.7071f }, { -0.7071f, 0.7071f }, { 0.7071f, -0.7071f }, { -0.7071f, -0.7071f } };

	(void)round;
	hud_player_name_color(TRUE, &name_color);
	colour[0] = name_color.red;
	colour[1] = name_color.green;
	colour[2] = name_color.blue;
	line1[0] = line2[0] = 0;
	reviver_name[0] = 0;
	if (mode == AE_REVIVE_SELF_REVIVING)
	{
		struct player_datum const *reviver = player_of_slot(mine->reviver);

		if (reviver)
			name_of(reviver, reviver_name, (int)NUMBEROF(reviver_name));
		white = ae_revive_white_segments(mine->progress);
	}
	if (bleeding)
		lit = ae_revive_bleed_lit(mine->bleed, total);
	if (mode == AE_REVIVE_SELF_REVIVING && reviver_name[0])
		wcscpy(line2, L" is reviving you");
	else
	{
		wcscpy(line1, L"WAITING FOR REVIVE");
		if (bleeding)
			ae_revive_bleed_text(mine->bleed, line2, (int)NUMBEROF(line2));
		else
		{
			/* (the nearest teammate standing, in metres from where this player lies) */
			real nearest = -1.0f, dx, dy, dz;
			int index;
			real_point3d spot;

			spot.x = ae_round_wire_real(mine->spot[0]);
			spot.y = ae_round_wire_real(mine->spot[1]);
			spot.z = ae_round_wire_real(mine->spot[2]);
			for (index = 0; index < round->count; index++)
			{
				struct ae_round_wire_player const *entry = &round->players[index];
				struct player_datum const *mate = entry->state == AE_ROUND_PLAYER_ALIVE ? player_of_slot(entry->player) : NULL;
				real_point3d at;
				real distance;

				if (!mate || entry->player == mine->player || mate->team_index != me->team_index || mate->unit_index == NONE ||
					!unit_try_and_get(mate->unit_index))
				{
					continue;
				}
				object_get_origin(mate->unit_index, &at);
				dx = at.x - spot.x;
				dy = at.y - spot.y;
				dz = at.z - spot.z;
				distance = (real)sqrt(dx * dx + dy * dy + dz * dz);
				if (nearest < 0.0f || distance < nearest)
					nearest = distance;
			}
			if (nearest >= 0.0f)
				ae_revive_nearest_text(ae_revive_metres(nearest), line2, (int)NUMBEROF(line2));
		}
	}
	/* (the lockup centred: the helmet's ring, a gap, then the words; the widest of the title and the lines) */
	title_width = ae_hud_text_measure(title, title_px);
	text_width = ae_hud_text_measure(reviver_name, line_px) + ae_hud_text_measure(line1[0] ? line1 : line2, line_px);
	if (ae_hud_text_measure(line2, line_px) > text_width)
		text_width = ae_hud_text_measure(line2, line_px);
	if (title_width > text_width)
		text_width = title_width;
	left = (view->width - (ring_radius * 2.0f + LOCKUP_GAP * k + text_width)) / 2.0f;
	cx = left + ring_radius;
	cy = HELMET_TOP * k + ring_radius;
	if (helmet)
	{
		whole.x0 = 0.0f;
		whole.x1 = 1.0f;
		whole.y0 = 0.0f;
		whole.y1 = 1.0f;
		for (pass = 0; pass < 8; pass++)
		{
			ae_waypoint_sprite_draw_pixels(helmet, &whole, cx - square / 2.0f + around[pass][0] * 1.5f * k,
				cy - square / 2.0f + around[pass][1] * 1.5f * k, cx + square / 2.0f + around[pass][0] * 1.5f * k,
				cy + square / 2.0f + around[pass][1] * 1.5f * k, premultiplied(dark_rgb, 0.7f));
		}
		ae_waypoint_sprite_draw_pixels(helmet, &whole, cx - square / 2.0f, cy - square / 2.0f, cx + square / 2.0f, cy + square / 2.0f,
			premultiplied(white_rgb, 1.0f));
	}
	segment_ring(cx, cy, ring_radius, ring_radius * 0.125f, white, lit, colour, 0.95f,
		bleeding ? RING_BLEED : RING_REVIVE);
	text_x = cx + ring_radius + LOCKUP_GAP * k;
	baseline = cy - ring_radius * 0.12f;
	painter = color_of(white_rgb, 1.0f);
	ae_hud_text_draw_ex(local_player_index, (real)floor(text_x + 0.5f), (real)floor(baseline + 0.5f), title, title_px, &painter,
		_ae_text_outline);
	baseline += line_px * 1.35f;
	if (mode == AE_REVIVE_SELF_REVIVING && reviver_name[0])
	{
		real_argb_color teamed = color_of(colour, 1.0f);
		real width = ae_hud_text_measure(reviver_name, line_px);

		ae_hud_text_draw_ex(local_player_index, (real)floor(text_x + 0.5f), (real)floor(baseline + 0.5f), reviver_name, line_px, &teamed,
			_ae_text_outline);
		ae_hud_text_draw_ex(local_player_index, (real)floor(text_x + width + 0.5f), (real)floor(baseline + 0.5f), line2, line_px, &painter,
			_ae_text_outline);
	}
	else
	{
		ae_hud_text_draw_ex(local_player_index, (real)floor(text_x + 0.5f), (real)floor(baseline + 0.5f), line1, line_px, &painter,
			_ae_text_outline);
		if (line2[0])
		{
			baseline += line_px * 1.25f;
			ae_hud_text_draw_ex(local_player_index, (real)floor(text_x + 0.5f), (real)floor(baseline + 0.5f), line2, line_px * 0.9f,
				&painter, _ae_text_outline);
		}
	}
}

/* ---------- the feed line: who revived whom, seen from the round state on every machine */

#define AE_ROUND_WIRE_PLAYERS_MAX_SLOTS 256
static unsigned char watched_state[AE_ROUND_WIRE_PLAYERS_MAX_SLOTS];
static unsigned char watched_reviver[AE_ROUND_WIRE_PLAYERS_MAX_SLOTS];
static long watched_tick = -1;
static int watched_round = -1;

static void feed_watch(struct ae_round_wire const *round)
{
	long now = game_time_get();
	int index;
	boolean fresh;

	if (now == watched_tick)
		return;
	/* a new round, a round not live (PRE: everyone stands up again), or the game clock gone back (a new game): what was
	watched is forgotten and the states taken as they are, with no line */
	fresh = (boolean)(round->round != watched_round || round->phase != AE_ROUND_PHASE_LIVE || now < watched_tick);
	watched_round = round->round;
	watched_tick = now;
	if (fresh)
	{
		memset(watched_state, 0, sizeof(watched_state));
		memset(watched_reviver, AE_ROUND_WIRE_NO_PLAYER, sizeof(watched_reviver));
	}
	for (index = 0; index < round->count; index++)
	{
		struct ae_round_wire_player const *entry = &round->players[index];
		unsigned slot = entry->player;

		if (slot >= AE_ROUND_WIRE_PLAYERS_MAX_SLOTS)
			continue;
		if (!fresh && watched_state[slot] == AE_ROUND_PLAYER_DOWN &&
			(entry->state == AE_ROUND_PLAYER_REVIVED || entry->state == AE_ROUND_PLAYER_ALIVE) &&
			watched_reviver[slot] != AE_ROUND_WIRE_NO_PLAYER)
		{
			ae_killfeed_revive(watched_reviver[slot], (long)slot);
		}
		if (entry->state == AE_ROUND_PLAYER_DOWN && entry->reviver != AE_ROUND_WIRE_NO_PLAYER)
			watched_reviver[slot] = entry->reviver;
		else if (entry->state != AE_ROUND_PLAYER_DOWN)
			watched_reviver[slot] = AE_ROUND_WIRE_NO_PLAYER;
		watched_state[slot] = entry->state;
	}
}

/* ---------- the entry */

void ae_revive_hud_draw(short local_player_index)
{
	struct ae_round_wire const *round;
	struct view view;
	long player_index, slot;
	struct player_datum const *me;
	struct ae_round_wire_player const *mine;
	int index;

	if (local_player_index == NONE || !(round = live_view(local_player_index)) || !(round->flags & AE_ROUND_WIRE_REVIVE))
	{
		ae_item_prompt_revive_clear(local_player_index);
		return;
	}
	if (local_player_index == 0)
		feed_watch(round);
	player_index = local_player_get_player_index(local_player_index);
	if (player_index == NONE || !view_get(&view))
		return;
	me = player_get(player_index);
	slot = DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index);
	mine = entry_of(round, slot);
	prompt_update(local_player_index, round, slot, me);
	for (index = 0; index < round->count; index++)
	{
		struct ae_round_wire_player const *entry = &round->players[index];
		struct player_datum const *downed;

		if (entry->state != AE_ROUND_PLAYER_DOWN || entry->player == slot || !(downed = player_of_slot(entry->player)) ||
			downed->team_index != me->team_index)
		{
			continue;
		}
		marker_draw(local_player_index, &view, downed, entry, me);
	}
	if (mine && mine->state == AE_ROUND_PLAYER_DOWN && me->unit_index == NONE)
		own_view_draw(local_player_index, &view, round, mine, me);
}

/* ---------- the state line */

boolean ae_revive_hud_state_message(long player_index, wchar_t *message, long count, boolean *result)
{
	struct ae_round_wire const *round;
	struct ae_round_wire_player const *entry;

	if (player_index == NONE || !game_engine_running() || !ae_round_on() || !(round = ae_round_view()) ||
		round->phase != AE_ROUND_PHASE_LIVE)
	{
		return FALSE;
	}
	entry = entry_of(round, DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index));
	if (!entry || entry->state != AE_ROUND_PLAYER_DOWN)
		return FALSE;
	if (ae_hud_style_is_ae())
	{
		/* (the own view says it: an empty line, not "You are out of lives") */
		*result = FALSE;
		return TRUE;
	}
	if (message && count > 0)
		usnprintf(message, (unsigned long)count, L"DOWNED: WAITING FOR REVIVE");
	*result = TRUE;
	return TRUE;
}
