/*
AE_MARKER_DRAW.C

The markers drawn (ae_marker_draw.h). Everything is the view's pixels (x from its left, y from its top), as the wheel's: the
numbers are the design's at 1080 lines, kept with the view's scale s (the strip's: 1, 0.85, 0.75, times the screen's lines over
1080) for the gaps and the screen's lines over 1080 (hs) for the glyph and the text, which keep a floor of 16 px.
*/

#include <math.h>
#include <string.h>
#include <wchar.h>

#include "cseries/cseries.h"
#include "cseries/cseries_windows.h"
#include "math/real_math.h"
#include "bitmaps/bitmap_color_conversion.h"
#include "bitmaps/bitmap_group.h"
#include "bitmaps/bitmaps.h"
#include "game/game.h"
#include "game/players.h"
#include "interface/hud.h"
#include "interface/hud_definitions.h"
#include "render/render.h"
#include "text/unicode.h"

#include "ae_hud_digits.h"
#include "ae_hud_messages.h"
#include "ae_hud_style.h"
#include "ae_hud_text.h"
#include "ae_key_prompts.h"
#include "ae_mark_prompt.h"
#include "ae_mark_prompt_rules.h"
#include "ae_mark_targets.h"
#include "ae_marker_draw.h"
#include "ae_marker_rules.h"
#include "ae_panel_draw.h"
#include "ae_score_strip.h"
#include "ae_waypoint_icons.h"
#include "ae_waypoint_ink.h"

real hud_item_timers_ae_waypoint_scale(real world_units);
struct bitmap_data const *hud_item_timers_ae_glyph(short glyph);
short hud_item_timers_ae_waypoint_style(void);

#define MARKER_PLAYERS 16
#define MARKER_VIEWS 4
/* a HUD unit is this many pixels at 1080 lines */
#define MARKER_UNIT 2.25f
/* a place's pin stands on its tip, 15/32 of the glyph under its middle */
#define MARKER_PIN_TIP 0.46875f

/* the rows: the text's sizes at 1080 lines (the fact's smaller in split), their gaps (all x s) */
#define MARKER_TEXT 16.0f
#define MARKER_FACT 18.0f
#define MARKER_FACT_SPLIT 16.0f
#define MARKER_TEXT_FLOOR 16.0f
#define MARKER_ROW_GAP 8.0f
#define MARKER_OVER_GLYPH 3.0f
#define MARKER_SPRITE_WIDE 120.0f
#define MARKER_SPRITE_GAP 10.0f
#define MARKER_RIFLE_WIDE 64.0f
#define MARKER_RIFLE_OF 36.0f
/* the OK row: its pill, its gap to the names, and how far under the glyph's middle it stands (the flash ring's radius and 10) */
#define MARKER_OK_PILL_WIDE 46.0f
#define MARKER_OK_PILL_HIGH 22.0f
#define MARKER_OK_GAP 8.0f
#define MARKER_OK_UNDER 48.0f
#define MARKER_LEAN 0.46f
/* the flash's ring: its line and the dark under it */
#define MARKER_RING_LINE 3.0f
#define MARKER_RING_KEYLINE 6.0f
#define MARKER_RING_SEGMENTS 32
/* the lead line's width, and its dark keyline's, with their floors */
#define MARKER_LINE 2.0f
#define MARKER_KEYLINE 4.0f
#define MARKER_LINE_FLOOR 1.5f
#define MARKER_KEYLINE_FLOOR 3.0f
#define MARKER_LINE_ALPHA 0.82f
#define MARKER_KEYLINE_ALPHA 0.60f
#define MARKER_OUTLINE_ALPHA 0.85f
#define MARKER_SPRITE_KEYLINE 1.5f
#define MARKER_TEXT_ROWS_KEPT 8.0f

/* the inks of this draw (ae_marker_ink_get: HUD STYLE AE's, or CE's own in any other style), and whether it is AE's */
static struct ae_marker_ink ink;
static boolean ink_ae;

/* ---------- what is kept of each mark */

static struct marker_state
{
	long born;
	boolean used;
	unsigned short seen;			/* the answers there were when last looked at */
	unsigned long flash_ms;			/* when the last new answer came; 0 for none */
	real eased[MARKER_VIEWS];		/* each view's glyph size (1080 lines' pixels), on its way to its target */
	unsigned long eased_ms[MARKER_VIEWS];
} states[MARKER_PLAYERS];

static struct marker_state *state_get(struct ae_marker const *marker, unsigned long now)
{
	struct marker_state *state;

	if (marker->slot < 0 || marker->slot >= MARKER_PLAYERS)
		return NULL;
	state = &states[marker->slot];
	if (!state->used || state->born != marker->born)
	{
		memset(state, 0, sizeof(*state));
		state->used = TRUE;
		state->born = marker->born;
		/* (the answers a mark has when first seen came before: no flash for them) */
		state->seen = marker->responders;
	}
	if ((unsigned short)(marker->responders & ~state->seen))
		state->flash_ms = now ? now : 1;
	state->seen = marker->responders;
	return state;
}

/* ---------- drawing */

static void make_color(real const rgb[3], real alpha, real_argb_color *color)
{
	color->alpha = alpha < 0.0f ? 0.0f : alpha > 1.0f ? 1.0f : alpha;
	color->red = rgb[0];
	color->green = rgb[1];
	color->blue = rgb[2];
}

static pixel32 premultiplied(real const rgb[3], real alpha)
{
	real_argb_color color;

	color.alpha = alpha < 0.0f ? 0.0f : alpha > 1.0f ? 1.0f : alpha;
	color.red = rgb[0] * color.alpha;
	color.green = rgb[1] * color.alpha;
	color.blue = rgb[2] * color.alpha;
	return real_argb_color_to_pixel32(&color);
}

/* a text starting at x, its capitals' middle on middle, on whole pixels */
static void text_left_edge(short view, wchar_t const *text, real x, real middle, real px, real const rgb[3], real alpha,
	enum ae_hud_text_edge edge)
{
	real_argb_color color;

	make_color(rgb, alpha, &color);
	ae_hud_text_draw_ex(view, (real)floor(x + 0.5f), (real)floor(middle + AE_HUD_TEXT_CAP * px / 2.0f + 0.5f), text, px,
		&color, edge);
}

static void text_left(short view, wchar_t const *text, real x, real middle, real px, real const rgb[3], real alpha)
{
	text_left_edge(view, text, x, middle, px, rgb, alpha, _ae_text_outline);
}

static real text_width(wchar_t const *text, real px)
{
	return text[0] ? ae_hud_text_measure(text, px) + 2.0f : 0.0f;
}

/* a glyph (the marks' 64 grid, its dark outline the next one) in a square about (x, y) */
static void glyph_draw(short glyph, real x, real y, real size, real const rgb[3], real alpha, boolean outlined)
{
	struct bitmap_data const *fill = hud_item_timers_ae_glyph(glyph);
	struct bitmap_data const *outline = outlined ? hud_item_timers_ae_glyph((short)(glyph + 1)) : NULL;
	real_rectangle2d whole;
	real left = (real)floor(x - size / 2.0f + 0.5f), top = (real)floor(y - size / 2.0f + 0.5f);

	if (!fill)
		return;
	whole.x0 = 0.0f;
	whole.x1 = 1.0f;
	whole.y0 = 0.0f;
	whole.y1 = 1.0f;
	if (outline)
	{
		ae_waypoint_sprite_draw_pixels(outline, &whole, left, top, left + size, top + size,
			premultiplied(ink.dark, MARKER_OUTLINE_ALPHA * alpha));
	}
	ae_waypoint_sprite_draw_pixels(fill, &whole, left, top, left + size, top + size, premultiplied(rgb, alpha));
}

/* a map's sprite (a weapon's picture) of this width about (x, y), in its own proportions, with a dark keyline round it; the
ink drawn over itself to thicken it, as the wheel's rifle is */
static void sprite_draw(struct bitmap_data const *bitmap, real_rectangle2d const *clip, real x, real y, real width,
	real const rgb[3], real alpha)
{
	static real const around[8][2] = { { 1.0f, 0.0f }, { -1.0f, 0.0f }, { 0.0f, 1.0f }, { 0.0f, -1.0f },
		{ 0.7071f, 0.7071f }, { -0.7071f, 0.7071f }, { 0.7071f, -0.7071f }, { -0.7071f, -0.7071f } };
	real texels_wide = (clip->x1 - clip->x0) * (real)bitmap->width;
	real texels_high = (clip->y1 - clip->y0) * (real)bitmap->height;
	real height, left, top;
	short pass;

	if (texels_wide <= 0.0f || texels_high <= 0.0f)
		return;
	height = width * texels_high / texels_wide;
	left = (real)floor(x - width / 2.0f + 0.5f);
	top = (real)floor(y - height / 2.0f + 0.5f);
	for (pass = 0; pass < 8; pass++)
	{
		ae_waypoint_sprite_draw_pixels(bitmap, clip, left + around[pass][0] * MARKER_SPRITE_KEYLINE,
			top + around[pass][1] * MARKER_SPRITE_KEYLINE, left + width + around[pass][0] * MARKER_SPRITE_KEYLINE,
			top + height + around[pass][1] * MARKER_SPRITE_KEYLINE, premultiplied(ink.dark, 0.375f * alpha));
	}
	for (pass = 0; pass < 2; pass++)
		ae_waypoint_sprite_draw_pixels(bitmap, clip, left, top, left + width, top + height, premultiplied(rgb, 0.7f * alpha));
}

/* the sprite's height at that width (0 for a sprite that cannot be drawn) */
static real sprite_height(struct bitmap_data const *bitmap, real_rectangle2d const *clip, real width)
{
	real texels_wide = (clip->x1 - clip->x0) * (real)bitmap->width;
	real texels_high = (clip->y1 - clip->y0) * (real)bitmap->height;

	return texels_wide > 0.0f && texels_high > 0.0f ? width * texels_high / texels_wide : 0.0f;
}

/* a line of this width, over its dark keyline */
static void lead_line(real x0, real y0, real x1, real y1, real s, real alpha)
{
	real_argb_color dark, line;
	real keyline = MARKER_KEYLINE * s, width = MARKER_LINE * s;

	make_color(ink.dark, MARKER_KEYLINE_ALPHA * alpha, &dark);
	make_color(ink.line, MARKER_LINE_ALPHA * alpha, &line);
	ae_hud_line(x0, y0, x1, y1, keyline < MARKER_KEYLINE_FLOOR ? MARKER_KEYLINE_FLOOR : keyline, &dark);
	ae_hud_line(x0, y0, x1, y1, width < MARKER_LINE_FLOOR ? MARKER_LINE_FLOOR : width, &line);
}

/* the flash's ring: in the flash colour over a wider dark one */
static void ring_draw(real x, real y, real radius, real s, real alpha)
{
	real_argb_color dark, line;
	short i;

	make_color(ink.dark, 0.55f * alpha, &dark);
	make_color(ink.flash, alpha, &line);
	for (i = 0; i < MARKER_RING_SEGMENTS; i++)
	{
		real a0 = 6.2831853f * (real)i / (real)MARKER_RING_SEGMENTS, a1 = 6.2831853f * (real)(i + 1) / (real)MARKER_RING_SEGMENTS;

		ae_hud_line(x + radius * (real)cos(a0), y + radius * (real)sin(a0), x + radius * (real)cos(a1), y + radius * (real)sin(a1),
			MARKER_RING_KEYLINE * s, &dark);
	}
	for (i = 0; i < MARKER_RING_SEGMENTS; i++)
	{
		real a0 = 6.2831853f * (real)i / (real)MARKER_RING_SEGMENTS, a1 = 6.2831853f * (real)(i + 1) / (real)MARKER_RING_SEGMENTS;

		ae_hud_line(x + radius * (real)cos(a0), y + radius * (real)sin(a0), x + radius * (real)cos(a1), y + radius * (real)sin(a1),
			MARKER_RING_LINE * s, &line);
	}
}

/* ---------- the rows */

/* the metrics of one view's drawing */
struct view
{
	real ppu, hs, s, width, height;
	boolean split;
	real text_px, fact_px;
	short style;			/* display.waypoint_style: TEXT, ICONS or BOTH */
	struct ae_prompt_metrics prompt;
};

static real floored(real px)
{
	return px < MARKER_TEXT_FLOOR ? MARKER_TEXT_FLOOR : px;
}

static boolean view_get(struct view *view)
{
	view->ppu = ae_hud_pixels_per_unit();
	if (view->ppu <= 0.0f)
		return FALSE;
	view->hs = view->ppu / MARKER_UNIT;
	view->s = ae_score_strip_view_scale();
	view->width = (real)(render.camera.viewport_bounds.x1 - render.camera.viewport_bounds.x0) * view->ppu;
	view->height = (real)(render.camera.viewport_bounds.y1 - render.camera.viewport_bounds.y0) * view->ppu;
	view->split = (boolean)(local_player_count() > 1);
	if (!(view->s > 0.0f) || view->width <= 0.0f || view->height <= 0.0f)
		return FALSE;
	/* (HUD STYLE AE's inks; any other style draws the same forms in CE's own: its message colour for the line and the label) */
	{
		float ce[3];

		ink_ae = ae_hud_style_is_ae();
		if (hud_globals)
		{
			ce[0] = hud_globals->messaging.text_color.red;
			ce[1] = hud_globals->messaging.text_color.green;
			ce[2] = hud_globals->messaging.text_color.blue;
		}
		ae_marker_ink_get(ink_ae, hud_globals ? ce : NULL, &ink);
	}
	view->style = hud_item_timers_ae_waypoint_style();
	view->text_px = floored(MARKER_TEXT * view->hs);
	view->fact_px = floored((view->split ? MARKER_FACT_SPLIT : MARKER_FACT) * view->hs);
	ae_prompt_metrics_get(view->split, view->hs, &view->prompt);
	return TRUE;
}

/* "HERE  30.5m": the name, two spaces, the distance (in metres: a world unit being ten feet; one decimal) */
static void distance_text(real distance, wchar_t *text, int size)
{
	long tenths = (long)(distance * 30.48f + 0.5f);

	if (tenths < 0)
		tenths = 0;
	if (tenths > 99999)
		tenths = 99999;
	usnprintf(text, (unsigned long)size, L"%d.%dm", (int)(tenths / 10), (int)(tenths % 10));
}

/* the name and its distance as one row: what is in each, and the width of the whole */
struct name_row
{
	wchar_t name[AE_MARKER_NAME_SIZE + 2];
	wchar_t distance[12];
	real name_width, whole_width;
};

static void name_row_make(struct ae_marker const *marker, real px, struct name_row *row)
{
	unsigned long length;

	/* (a fact that names it leaves the name out: just how far) */
	ustrncpy_terminated(row->name, marker->name, NUMBEROF(row->name) - 2);
	length = ustrlen(row->name);
	if (length > 0)
	{
		row->name[length] = L' ';
		row->name[length + 1] = L' ';
		row->name[length + 2] = 0;
	}
	distance_text(marker->distance, row->distance, NUMBEROF(row->distance));
	row->name_width = row->name[0] ? ae_hud_text_measure(row->name, px) : 0.0f;
	row->whole_width = row->name_width + ae_hud_text_measure(row->distance, px) + 2.0f;
}

static void name_row_draw(short local_player_index, struct ae_marker const *marker, struct name_row const *row, real left,
	real middle, real px, real alpha)
{
	/* (the classic marks' own names had no outline but an enemy's) */
	if (row->name[0])
		text_left_edge(local_player_index, row->name, left, middle, px, marker->enemy ? ink.red : ink.white, alpha,
			ink_ae || marker->enemy ? _ae_text_outline : _ae_text_plain);
	text_left(local_player_index, row->distance, left + row->name_width, middle, px, ink.white, alpha);
}

/* where a row is placed: the edge it aligns to and the way it grows from there (+1: starting at the edge, -1: ending at it,
0: centred on it) */
static real row_left(real edge, real width, int dir)
{
	return dir > 0 ? edge : dir < 0 ? edge - width : edge - width / 2.0f;
}

/* the aim prompt's row (its chip, then the word): its width; and drawn when asked */
static boolean action_row(short local_player_index, struct view const *view, wchar_t const *word, real *width, real *chip_width)
{
	real chip = 0.0f;

	if (!word[0] || !ae_key_prompts_chip_width(local_player_index, AE_CHIP_MARK, view->prompt.chip_height, &chip) || chip <= 0.0f)
		return FALSE;
	*chip_width = chip;
	*width = chip + view->prompt.chip_gap + ae_hud_text_measure(word, view->prompt.word_px) + 2.0f;
	return TRUE;
}

/* ---------- the OK row */

/* the pill: a pale leaning cell with a dark OK */
static void ok_pill_draw(short local_player_index, real left, real top, real width, real height, real px, real s, real alpha)
{
	real points[4][2];
	real_argb_color fill, dark, line;
	real lean = MARKER_LEAN * height;

	/* (a parallelogram leaning right: the bottom left, bottom right less the lean, top right, top left plus the lean) */
	points[0][0] = left + lean; points[0][1] = top;
	points[1][0] = left + width; points[1][1] = top;
	points[2][0] = left + width - lean; points[2][1] = top + height;
	points[3][0] = left; points[3][1] = top + height;
	make_color(ink.line, 0.92f * alpha, &fill);
	make_color(ink.dark, MARKER_KEYLINE_ALPHA * alpha, &dark);
	make_color(ink.line, MARKER_LINE_ALPHA * alpha, &line);
	ae_panel_fill_polygon_soft((real const (*)[2])points, 4, 0.0f, &fill);
	ae_panel_stroke_polygon_soft((real const (*)[2])points, 4, 0.0f, MARKER_KEYLINE * s < MARKER_KEYLINE_FLOOR ?
		MARKER_KEYLINE_FLOOR : MARKER_KEYLINE * s, &dark);
	ae_panel_stroke_polygon_soft((real const (*)[2])points, 4, 0.0f, MARKER_LINE * s < MARKER_LINE_FLOOR ?
		MARKER_LINE_FLOOR : MARKER_LINE * s, &line);
	{
		real_argb_color text;
		real ok_width = ae_hud_text_measure(L"OK", px);

		make_color(ink.dark, alpha, &text);
		ae_hud_text_draw_ex(local_player_index, (real)floor(left + width / 2.0f - ok_width / 2.0f + 0.5f),
			(real)floor(top + height / 2.0f + AE_HUD_TEXT_CAP * px / 2.0f + 0.5f), L"OK", px, &text, _ae_text_plain);
	}
}

/* the names after the pill: up to three, then "+2" */
static void ok_names_text(struct ae_marker const *marker, wchar_t *text, int size)
{
	int shown, more, index, length = 0;

	ae_ok_names_split(marker->ok_count, &shown, &more);
	text[0] = 0;
	for (index = 0; index < shown && index < AE_MARKER_OK_NAMES; index++)
	{
		long left = (long)size - 1 - length;
		unsigned long name_length = ustrlen(marker->ok_names[index]);

		if (index > 0 && left > 2)
		{
			text[length++] = L',';
			text[length++] = L' ';
			text[length] = 0;
			left -= 2;
		}
		if ((long)name_length > left)
			name_length = (unsigned long)(left > 0 ? left : 0);
		memcpy(text + length, marker->ok_names[index], name_length * sizeof(wchar_t));
		length += (int)name_length;
		text[length] = 0;
	}
	if (more > 0 && length < size - 6)
	{
		text[length++] = L' ';
		text[length++] = L'+';
		text[length++] = (wchar_t)(L'0' + (more > 9 ? 9 : more));
		text[length] = 0;
	}
}

/* ---------- the whole */

/* whether a box comes within the kept gap of a row of the game's text */
static boolean touches_text_rows(short local_player_index, real const box[4], real gap)
{
	real rows[AE_HUD_MESSAGE_ROWS_MOST][4];
	short count = ae_hud_message_rows(local_player_index, rows, AE_HUD_MESSAGE_ROWS_MOST), index;

	for (index = 0; index < count; index++)
	{
		if (box[0] < rows[index][2] + gap && rows[index][0] - gap < box[2] && box[1] < rows[index][3] + gap &&
			rows[index][1] - gap < box[3])
		{
			return TRUE;
		}
	}
	return FALSE;
}

void ae_marker_draw(short local_player_index, struct ae_marker const *marker)
{
	struct view view;
	struct marker_state *state;
	struct name_row row;
	struct ae_marker_lead_in lead_in;
	struct ae_marker_lead_out lead;
	real_rectangle2d rifle_clip, sprite_clip;
	struct bitmap_data const *rifle = NULL;
	real const *rgb = marker->enemy ? ink.red : ink.white;
	real centre_x, centre_y, size, target, e, waypoint_scale, alpha, lift = 1.0f, ring = 0.0f, ring_alpha = 0.0f;
	real action_width = 0.0f, chip_width = 0.0f, fact_width, owner_width, sprite_width = 0.0f, sprite_high = 0.0f;
	real block_width, block_height, glyph_half;
	real box[4];
	unsigned long now = system_milliseconds();
	boolean has_action, has_fact, has_owner, show_name, use_lead, rows_hidden;

	if (local_player_index < 0 || local_player_index >= MARKER_VIEWS || !marker || !view_get(&view))
		return;
	state = state_get(marker, now);
	if (!state)
		return;
	alpha = marker->alpha * marker->fade;
	if (alpha <= 0.0f)
		return;

	/* the size: the table's by where it sits against the view's middle, eased */
	centre_x = marker->x * view.ppu;
	centre_y = marker->y * view.ppu;
	e = ae_marker_e(centre_x - view.width / 2.0f, centre_y - view.height / 2.0f, view.width, view.height);
	waypoint_scale = hud_item_timers_ae_waypoint_scale(marker->distance);
	target = ae_marker_size(waypoint_scale * MARKER_UNIT, e);
	if (state->eased_ms[local_player_index] == 0 || now - state->eased_ms[local_player_index] > 500)
		state->eased[local_player_index] = target;
	else
		state->eased[local_player_index] = ae_marker_ease(state->eased[local_player_index], target,
			(long)(now - state->eased_ms[local_player_index]));
	state->eased_ms[local_player_index] = now ? now : 1;
	if (!state->flash_ms || !ae_marker_flash((long)(now - state->flash_ms), &ring, &ring_alpha, &lift))
	{
		/* (no flash, or its 240 ms are over) */
		state->flash_ms = 0;
		ring_alpha = 0.0f;
		lift = 1.0f;
	}
	size = state->eased[local_player_index] * view.hs * marker->appear * ae_marker_repeat_scale(marker->repeats) * lift;
	if (size < 2.0f)
		return;
	/* (a place's pin stands on its tip) */
	if (marker->pin && !marker->clamped)
		centre_y -= size * MARKER_PIN_TIP;
	glyph_half = size / 2.0f;

	/* the glyph, or the rifle's picture (no call sets it today; the wheel's REGROUP wedge keeps the map's rifle until its icon is redrawn) */
	if (marker->rifle)
		rifle = ae_mark_rifle_sprite(&rifle_clip);
	if (ring_alpha > 0.0f)
		ring_draw(centre_x, centre_y, ring * view.s, view.s, ring_alpha * alpha);
	if (rifle)
		sprite_draw(rifle, &rifle_clip, centre_x, centre_y, MARKER_RIFLE_WIDE / MARKER_RIFLE_OF * size, ink.white, alpha);
	else
		glyph_draw(marker->glyph, centre_x, centre_y, size, rgb, alpha, (boolean)(ink_ae || marker->enemy));

	/* the rows */
	has_action = FALSE;
	if (marker->action[0])
		has_action = action_row(local_player_index, &view, marker->action, &action_width, &chip_width);
	has_fact = (boolean)(marker->fact[0] != 0);
	has_owner = (boolean)(marker->owner[0] != 0);
	/* (WAYPOINT STYLE: the pictures alone have no name and distance row; the rows that answer the call stay in every style) */
	show_name = (boolean)(ae_marker_name_row_shown(view.style) != 0);
	use_lead = (boolean)((has_action || has_fact || has_owner || show_name) && (has_action || has_fact || e <= 0.30f));
	name_row_make(marker, view.text_px, &row);
	fact_width = text_width(marker->fact, view.fact_px);
	owner_width = text_width(marker->owner, view.text_px);
	if (has_fact && marker->fact_sprite)
	{
		real_rectangle2d clip;

		clip.x0 = marker->fact_clip[0];
		clip.y0 = marker->fact_clip[1];
		clip.x1 = marker->fact_clip[2];
		clip.y1 = marker->fact_clip[3];
		sprite_clip = clip;
		sprite_width = MARKER_SPRITE_WIDE * view.s;
		sprite_high = sprite_height(marker->fact_sprite, &sprite_clip, sprite_width);
	}

	rows_hidden = FALSE;
	if (use_lead)
	{
		real row_gap = MARKER_ROW_GAP * view.s;
		real top, rows_top;
		real edge;
		boolean placed;
		int dir;

		/* (the block: the prompt's row, the fact (with its picture), the name and distance, the owner; a gap between each) */
		int rows = 0;

		block_width = 0.0f;
		block_height = 0.0f;
		if (has_action)
		{
			block_width = MAX(block_width, action_width);
			block_height += view.prompt.chip_height;
			rows++;
		}
		if (has_fact)
		{
			block_width = MAX(block_width, fact_width);
			block_height += view.fact_px;
			if (sprite_high > 0.0f)
			{
				block_width = MAX(block_width, sprite_width);
				block_height += sprite_high + MARKER_SPRITE_GAP * view.s;
			}
			rows++;
		}
		if (show_name)
		{
			block_width = MAX(block_width, row.whole_width);
			block_height += view.text_px;
			rows++;
		}
		if (has_owner)
		{
			block_width = MAX(block_width, owner_width);
			block_height += view.text_px;
			rows++;
		}
		block_height += row_gap * (real)(rows > 0 ? rows - 1 : 0);
		memset(&lead_in, 0, sizeof(lead_in));
		lead_in.view_width = view.width;
		lead_in.view_height = view.height;
		lead_in.s = view.s;
		lead_in.glyph_x = centre_x;
		lead_in.glyph_y = centre_y;
		lead_in.glyph_size = size;
		lead_in.block_width = block_width;
		lead_in.block_height = block_height;
		ae_marker_lead(&lead_in, &lead);
		box[0] = lead.box[0];
		box[1] = lead.box[1];
		box[2] = lead.box[2];
		box[3] = lead.box[3];
		rows_hidden = touches_text_rows(local_player_index, box, MARKER_TEXT_ROWS_KEPT * view.s);
		if (!rows_hidden)
		{
			dir = lead.dir;
			edge = lead.elbow_x;
			lead_line(lead.start_x, lead.start_y, lead.elbow_x, lead.elbow_y, view.s, alpha);
			lead_line(lead.elbow_x, lead.elbow_y, lead.shelf_end_x, lead.elbow_y, view.s, alpha);
			/* (the rows from the shelf up: the last of them first, a gap before each one over another) */
			top = lead.rows_bottom;
			placed = FALSE;
			if (has_owner)
			{
				top -= view.text_px;
				text_left(local_player_index, marker->owner, row_left(edge, owner_width, dir), top + view.text_px / 2.0f, view.text_px,
					ink.blue, alpha);
				placed = TRUE;
			}
			if (show_name)
			{
				top -= (placed ? row_gap : 0.0f) + view.text_px;
				name_row_draw(local_player_index, marker, &row, row_left(edge, row.whole_width, dir), top + view.text_px / 2.0f,
					view.text_px, alpha);
				placed = TRUE;
			}
			if (has_fact)
			{
				top -= (placed ? row_gap : 0.0f) + view.fact_px;
				text_left(local_player_index, marker->fact, row_left(edge, fact_width, dir), top + view.fact_px / 2.0f,
					view.fact_px, ink.white, alpha);
				if (sprite_high > 0.0f)
				{
					rows_top = top - MARKER_SPRITE_GAP * view.s - sprite_high;
					sprite_draw(marker->fact_sprite, &sprite_clip, row_left(edge, sprite_width, dir) + sprite_width / 2.0f,
						rows_top + sprite_high / 2.0f, sprite_width, ink.line, alpha);
					top = rows_top;
				}
				placed = TRUE;
			}
			if (has_action)
			{
				real left, middle, drawn = 0.0f;

				top -= (placed ? row_gap : 0.0f);
				left = row_left(edge, action_width, dir);
				middle = top - view.prompt.chip_height / 2.0f;
				if (ae_key_prompts_draw_chip(local_player_index, AE_CHIP_MARK, (real)floor(left + 0.5f), (real)floor(middle + 0.5f),
					view.prompt.chip_height, alpha, &drawn))
				{
					text_left(local_player_index, marker->action, left + drawn + view.prompt.chip_gap, middle, view.prompt.word_px,
						ink.white, alpha);
					/* (this frame's block has the prompt's row: the prompt draws none of its own) */
					ae_mark_prompt_block_takes(local_player_index, TRUE);
				}
			}
		}
	}
	else
	{
		/* (one row over the glyph: the name and the distance) */
		real left = row_left(centre_x, row.whole_width, 0), bottom = centre_y - glyph_half - MARKER_OVER_GLYPH * view.s;

		box[0] = left;
		box[1] = bottom - view.text_px;
		box[2] = left + row.whole_width;
		box[3] = bottom;
		rows_hidden = touches_text_rows(local_player_index, box, MARKER_TEXT_ROWS_KEPT * view.s);
		if (!rows_hidden && show_name)
			name_row_draw(local_player_index, marker, &row, left, bottom - view.text_px / 2.0f, view.text_px, alpha);
	}

	/* the OK row: the pill and the names of who said OK, under the glyph for the mark's life */
	if (marker->ok_count > 0 && !marker->clamped)
	{
		wchar_t names[AE_MARKER_OK_NAMES * (AE_MARKER_NAME_SIZE + 2) + 8];
		real pill_wide = MARKER_OK_PILL_WIDE * view.s, pill_high = MAX(MARKER_OK_PILL_HIGH * view.s, 22.0f * view.text_px / 16.0f);
		real names_wide, whole, left, top;

		ok_names_text(marker, names, NUMBEROF(names));
		names_wide = text_width(names, view.text_px);
		whole = pill_wide + (names_wide > 0.0f ? MARKER_OK_GAP * view.s + names_wide : 0.0f);
		left = centre_x - whole / 2.0f;
		top = centre_y + MARKER_OK_UNDER * view.s;
		box[0] = left;
		box[1] = top;
		box[2] = left + whole;
		box[3] = top + pill_high;
		if (!touches_text_rows(local_player_index, box, MARKER_TEXT_ROWS_KEPT * view.s))
		{
			ok_pill_draw(local_player_index, (real)floor(left + 0.5f), (real)floor(top + 0.5f), pill_wide, pill_high,
				view.text_px, view.s, alpha);
			if (names_wide > 0.0f)
			{
				text_left(local_player_index, names, left + pill_wide + MARKER_OK_GAP * view.s, top + pill_high / 2.0f,
					view.text_px, ink.white, alpha);
			}
		}
	}
}
