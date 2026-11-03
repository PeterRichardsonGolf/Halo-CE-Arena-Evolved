/*
AE_WHEEL_DRAW.C

The callout wheel drawn (ae_wheel_draw.h), in the view's screen pixels (x from the view's left, y from its top) as the
HUD's other pieces are: the sectors and the wedge as soft polygons with their keylines (ae_panel_draw.c), the captions
and words in the HUD text (ae_hud_text.c), the icons the marks' glyphs, the rifle the map's own sprite, the legend's
chips AE's own (ae_key_prompts.c). Where everything sits is ae_wheel_layout.c's, tested alone.
*/

#include <math.h>
#include <string.h>
#include <wchar.h>

#include "cseries/cseries.h"
#include "math/real_math.h"
#include "bitmaps/bitmap_color_conversion.h"
#include "bitmaps/bitmap_group.h"
#include "bitmaps/bitmaps.h"
#include "game/players.h"
#include "interface/hud.h"
#include "interface/hud_definitions.h"
#include "render/render.h"

#include "ae_hud_digits.h"
#include "ae_hud_style.h"
#include "ae_hud_text.h"
#include "ae_key_prompts.h"
#include "ae_mark_calls.h"
#include "ae_mark_prompt.h"
#include "ae_mark_rules.h"
#include "ae_mark_targets.h"
#include "ae_panel_draw.h"
#include "ae_waypoint_icons.h"
#include "ae_waypoint_ink.h"
#include "ae_wheel.h"
#include "ae_wheel_draw.h"
#include "ae_wheel_layout.h"
#include "ae_wheel_rules.h"

void platform_video_drawable_size(int *width, int *height);
struct bitmap_data const *hud_item_timers_ae_glyph(short glyph);

/* the sectors' fills and lines (the HUD style's tokens: the radar's and the ammo panel's lines, the shield meter's fill) */
#define SECTOR_FILL_ALPHA 0.40f
#define FOCUS_FILL_ALPHA 0.92f
#define LINE_ALPHA 0.82f
#define KEYLINE_ALPHA 0.60f
#define WORD_FILL_ALPHA 0.55f
#define ICON_KEYLINE_ALPHA 0.80f
#define SPRITE_KEYLINE_ALPHA 0.75f
#define BACKING_ALPHA 0.45f
#define BACKING_ACROSS 0.62f
#define BACKING_UP 34.0f
#define DIED_BACKING_ACROSS 170.0f
#define DIED_BACKING_UP 30.0f
/* a world marker behind a sector fades to this */
#define MARKER_FADE 0.3f
/* the sprite's dark outline: the ring of passes round it, this far out */
#define SPRITE_KEYLINE_REACH 1.5f

static real const flash_rgb[3] = { 245.0f / 255.0f, 250.0f / 255.0f, 1.0f };

/* ---------- the view and the state */

struct wheel_view
{
	real width, height;		/* the view's, screen pixels */
	real x, y;				/* the ring's centre */
	struct ae_wheel_layout layout;
};

static boolean view_get(struct wheel_view *view)
{
	real ppu = ae_hud_pixels_per_unit();
	real height;
	int split = local_player_count() > 1;
	int drawable_width = 0, drawable_height = 0;
	float centre_x, centre_y;

	if (ppu <= 0.0f)
		return FALSE;
	view->width = (real)(render.camera.viewport_bounds.x1 - render.camera.viewport_bounds.x0) * ppu;
	view->height = (real)(render.camera.viewport_bounds.y1 - render.camera.viewport_bounds.y0) * ppu;
	if (view->width <= 0.0f || view->height <= 0.0f)
		return FALSE;
	/* (a split view is 540 lines at 1080: a view as tall as the screen, side by side, takes the half) */
	height = view->height;
	platform_video_drawable_size(&drawable_width, &drawable_height);
	if (split && drawable_height > 0 && height > (real)drawable_height / 2.0f)
		height = (real)drawable_height / 2.0f;
	ae_wheel_layout_get(split, height, &view->layout);
	ae_wheel_centre(view->width, view->height, &centre_x, &centre_y);
	view->x = centre_x;
	view->y = centre_y;
	return TRUE;
}

struct wheel_state
{
	int focus;
	boolean dead;
	boolean closing;
	real alpha;
	real flash;
};

/* what is shown now: the open wheel (its 80 ms opening), or a send's flash and closing; FALSE for neither */
static boolean state_get(short local_player_index, struct wheel_state *state)
{
	int entry;
	boolean dead;
	real flash, alpha;

	if (ae_wheel_open(local_player_index))
	{
		state->focus = ae_wheel_focus(local_player_index);
		state->dead = ae_wheel_is_dead(local_player_index);
		state->closing = FALSE;
		state->alpha = ae_wheel_open_part(local_player_index);
		state->flash = 0.0f;
		return state->alpha > 0.0f;
	}
	if (ae_wheel_closing(local_player_index, &entry, &dead, &flash, &alpha))
	{
		state->focus = entry;
		state->dead = dead;
		state->closing = TRUE;
		state->alpha = alpha;
		state->flash = flash;
		return state->alpha > 0.0f;
	}
	return FALSE;
}

boolean ae_wheel_covers(short local_player_index, real x, real y)
{
	struct wheel_view view;
	struct wheel_state state;
	real ppu = ae_hud_pixels_per_unit();
	int entry;

	if (local_player_index < 0 || ppu <= 0.0f || !state_get(local_player_index, &state) || !view_get(&view))
		return FALSE;
	x = x * ppu - view.x;
	y = y * ppu - view.y;
	for (entry = 0; entry < AE_WHEEL_COUNT; entry++)
	{
		if (ae_wheel_entry_shown(entry, state.dead) &&
			ae_wheel_sector_contains(&view.layout, entry, entry == state.focus, x, y))
		{
			return TRUE;
		}
	}
	return FALSE;
}

boolean ae_wheel_text_keepout(short local_player_index, real *centre_x, real *centre_y, real *radius, real word_box[4],
	real *limit)
{
	struct wheel_view view;
	struct wheel_state state;

	if (local_player_index < 0 || !state_get(local_player_index, &state) || !view_get(&view))
		return FALSE;
	*centre_x = view.x;
	*centre_y = view.y;
	*radius = view.layout.outer + view.layout.grow;
	/* (the gap kept is the ring's own: 8 in one view, 5.44 in a split one) */
	*limit = 8.0f * view.layout.scale;
	word_box[0] = word_box[1] = word_box[2] = word_box[3] = 0.0f;
	if (state.focus != AE_WHEEL_NONE)
	{
		float cell[8][2];
		float middle_x, middle_y;
		real width = ae_hud_text_measure(ae_wheel_word(state.focus, state.dead), view.layout.word_px) + 2.0f;
		short i;

		ae_wheel_word_cell(&view.layout, width, cell, &middle_x, &middle_y);
		word_box[0] = word_box[2] = view.x + cell[0][0];
		word_box[1] = word_box[3] = view.y + cell[0][1];
		for (i = 1; i < 8; i++)
		{
			real x = view.x + cell[i][0], y = view.y + cell[i][1];

			word_box[0] = MIN(word_box[0], x);
			word_box[1] = MIN(word_box[1], y);
			word_box[2] = MAX(word_box[2], x);
			word_box[3] = MAX(word_box[3], y);
		}
	}
	return TRUE;
}

/* ---------- colours */

static void tone_rgb(short tone, real rgb[3])
{
	ae_panel_tone_rgb(tone, rgb);
	/* (CLASSIC and CE: the same forms, in their own line colour) */
	if (tone == _ae_tone_blue && !ae_hud_style_is_ae() && hud_globals)
	{
		rgb[0] = hud_globals->messaging.text_color.red;
		rgb[1] = hud_globals->messaging.text_color.green;
		rgb[2] = hud_globals->messaging.text_color.blue;
	}
}

static void make_color(real const rgb[3], real alpha, real_argb_color *color)
{
	color->alpha = alpha < 0.0f ? 0.0f : alpha > 1.0f ? 1.0f : alpha;
	color->red = rgb[0];
	color->green = rgb[1];
	color->blue = rgb[2];
}

/* a colour premultiplied by its alpha: the HUD's blend adds a sprite's colour over what is under it by 1 - alpha */
static pixel32 premultiplied(real const rgb[3], real alpha)
{
	real_argb_color color;

	make_color(rgb, alpha, &color);
	color.red *= color.alpha;
	color.green *= color.alpha;
	color.blue *= color.alpha;
	return real_argb_color_to_pixel32(&color);
}

static void mix(real const a[3], real const b[3], real part, real out[3])
{
	short i;

	for (i = 0; i < 3; i++)
		out[i] = a[i] + (b[i] - a[i]) * part;
}

/* ---------- text */

/* a text centred on x with its capitals' middle on y, on whole pixels */
static void text_centred(short view, wchar_t const *text, real x, real middle, real px, real const rgb[3], real alpha,
	enum ae_hud_text_edge edge)
{
	real_argb_color color;
	real width = ae_hud_text_measure(text, px);

	make_color(rgb, alpha, &color);
	ae_hud_text_draw_ex(view, (real)floor(x - width / 2.0f + 0.5f), (real)floor(middle + AE_HUD_TEXT_CAP * px / 2.0f + 0.5f),
		text, px, &color, edge);
}

/* a text starting at x */
static void text_left(short view, wchar_t const *text, real x, real middle, real px, real const rgb[3], real alpha)
{
	real_argb_color color;

	make_color(rgb, alpha, &color);
	ae_hud_text_draw_ex(view, (real)floor(x + 0.5f), (real)floor(middle + AE_HUD_TEXT_CAP * px / 2.0f + 0.5f), text, px,
		&color, _ae_text_outline);
}

/* ---------- shapes */

static short to_view(struct wheel_view const *view, float const (*points)[2], short count, real (*out)[2])
{
	short i;

	for (i = 0; i < count; i++)
	{
		out[i][0] = view->x + points[i][0];
		out[i][1] = view->y + points[i][1];
	}
	return count;
}

/* an outline in the HUD's line over its dark keyline, centred on the outline */
static void stroke_outline(struct wheel_view const *view, real const (*points)[2], short count, real line_alpha,
	real keyline_alpha)
{
	real_argb_color dark, line;
	real rgb[3];

	tone_rgb(_ae_tone_dark, rgb);
	make_color(rgb, keyline_alpha, &dark);
	ae_panel_stroke_polygon_soft(points, count, 0.0f, view->layout.keyline, &dark);
	tone_rgb(_ae_tone_blue, rgb);
	make_color(rgb, line_alpha, &line);
	ae_panel_stroke_polygon_soft(points, count, 0.0f, view->layout.line, &line);
}

static void draw_sector_fill(struct wheel_view const *view, int entry, boolean focused, real flash, real alpha)
{
	float points[AE_WHEEL_SECTOR_POINTS][2];
	real moved[AE_WHEEL_SECTOR_POINTS][2];
	real_argb_color fill;
	real rgb[3], line[3];
	short count = (short)ae_wheel_sector_polygon(&view->layout, entry, focused, points);

	to_view(view, (float const (*)[2])points, count, moved);
	if (focused)
	{
		/* the shield meter's fill in the HUD line; the send flashes it to white */
		tone_rgb(_ae_tone_blue, line);
		mix(line, flash_rgb, flash, rgb);
		make_color(rgb, (FOCUS_FILL_ALPHA + (1.0f - FOCUS_FILL_ALPHA) * flash) * alpha, &fill);
	}
	else
	{
		tone_rgb(_ae_tone_dark, rgb);
		make_color(rgb, SECTOR_FILL_ALPHA * alpha, &fill);
	}
	ae_panel_fill_polygon_soft((real const (*)[2])moved, count, 0.0f, &fill);
}

static void draw_sector_line(struct wheel_view const *view, int entry, boolean focused, real alpha)
{
	float points[AE_WHEEL_SECTOR_POINTS][2];
	real moved[AE_WHEEL_SECTOR_POINTS][2];
	short count = (short)ae_wheel_sector_polygon(&view->layout, entry, focused, points);

	to_view(view, (float const (*)[2])points, count, moved);
	stroke_outline(view, (real const (*)[2])moved, count, LINE_ALPHA * alpha, KEYLINE_ALPHA * alpha);
}

/* the direction wedge: where the stick or the mouse points, in the line at full strength with its keyline */
static void draw_wedge(struct wheel_view const *view, int entry, real alpha)
{
	float points[AE_WHEEL_WEDGE_POINTS][2];
	real moved[AE_WHEEL_WEDGE_POINTS][2];
	real_argb_color fill, dark, line;
	real rgb[3];
	short count = (short)ae_wheel_wedge_polygon(&view->layout, entry, points);

	to_view(view, (float const (*)[2])points, count, moved);
	tone_rgb(_ae_tone_dark, rgb);
	make_color(rgb, KEYLINE_ALPHA * alpha, &dark);
	ae_panel_stroke_polygon_soft((real const (*)[2])moved, count, 0.0f, view->layout.keyline, &dark);
	tone_rgb(_ae_tone_blue, rgb);
	make_color(rgb, alpha, &fill);
	ae_panel_fill_polygon_soft((real const (*)[2])moved, count, 0.0f, &fill);
	make_color(rgb, alpha, &line);
	ae_panel_stroke_polygon_soft((real const (*)[2])moved, count, 0.0f, view->layout.line, &line);
}

/* ---------- icons */

/* a glyph (the marks' 64 grid, its dark outline the next one) in a square about (x, y), its corner on a whole pixel */
static void draw_glyph(short glyph, real x, real y, real size, real const rgb[3], real alpha, boolean keylined)
{
	struct bitmap_data const *fill = hud_item_timers_ae_glyph(glyph);
	struct bitmap_data const *outline = keylined ? hud_item_timers_ae_glyph((short)(glyph + 1)) : NULL;
	real_rectangle2d whole;
	real dark[3];
	real left = (real)floor(x - size / 2.0f + 0.5f), top = (real)floor(y - size / 2.0f + 0.5f);

	if (!fill)
		return;
	whole.x0 = 0.0f;
	whole.x1 = 1.0f;
	whole.y0 = 0.0f;
	whole.y1 = 1.0f;
	if (outline)
	{
		tone_rgb(_ae_tone_dark, dark);
		ae_waypoint_sprite_draw_pixels(outline, &whole, left, top, left + size, top + size,
			premultiplied(dark, ICON_KEYLINE_ALPHA * alpha));
	}
	ae_waypoint_sprite_draw_pixels(fill, &whole, left, top, left + size, top + size, premultiplied(rgb, alpha));
}

/* the map's rifle sprite, its ink thickened by drawing it over itself (boost 2.5 is three passes at five sixths, 1.4 two
at 0.7), its width as laid out and its own proportions; with a dark keyline unless focused. FALSE when the map has none */
static boolean draw_rifle(struct wheel_view const *view, real x, real y, real const rgb[3], real alpha, boolean keylined)
{
	static real const around[8][2] = { { 1.0f, 0.0f }, { -1.0f, 0.0f }, { 0.0f, 1.0f }, { 0.0f, -1.0f },
		{ 0.7071f, 0.7071f }, { -0.7071f, 0.7071f }, { 0.7071f, -0.7071f }, { -0.7071f, -0.7071f } };
	real_rectangle2d clip;
	struct bitmap_data const *bitmap = ae_mark_rifle_sprite(&clip);
	real texels_wide, texels_high, width, height, boost = view->layout.rifle_boost, dark[3];
	short passes, pass;

	if (!bitmap)
		return FALSE;
	texels_wide = (clip.x1 - clip.x0) * (real)bitmap->width;
	texels_high = (clip.y1 - clip.y0) * (real)bitmap->height;
	if (texels_wide <= 0.0f || texels_high <= 0.0f)
		return FALSE;
	width = view->layout.sprite_width;
	height = width * texels_high / texels_wide;
	/* (its corner on a whole pixel) */
	x = (real)floor(x - width / 2.0f + 0.5f);
	y = (real)floor(y - height / 2.0f + 0.5f);
	passes = (short)ceil(boost);
	if (keylined)
	{
		tone_rgb(_ae_tone_dark, dark);
		for (pass = 0; pass < 8; pass++)
		{
			ae_waypoint_sprite_draw_pixels(bitmap, &clip, x + around[pass][0] * SPRITE_KEYLINE_REACH,
				y + around[pass][1] * SPRITE_KEYLINE_REACH, x + width + around[pass][0] * SPRITE_KEYLINE_REACH,
				y + height + around[pass][1] * SPRITE_KEYLINE_REACH,
				premultiplied(dark, SPRITE_KEYLINE_ALPHA * alpha * 0.5f));
		}
	}
	for (pass = 0; pass < passes; pass++)
	{
		ae_waypoint_sprite_draw_pixels(bitmap, &clip, x, y, x + width, y + height,
			premultiplied(rgb, alpha * boost / (real)passes));
	}
	return TRUE;
}

/* what a tap would call, for SMART's icon: the glyph of the aimed target's kind; FALSE for the brackets (nothing in
range, an answer to a marker rather than a new mark, or a split view's: the brackets always) */
static boolean smart_target_glyph(short local_player_index, struct wheel_view const *view, short *glyph)
{
	struct ae_mark_aim const *aim;

	if (view->layout.split)
		return FALSE;
	aim = ae_marks_aim(local_player_index);
	if (!aim || !aim->found || aim->verb != AE_CALL_MARK)
		return FALSE;
	switch (aim->mark.kind)
	{
	case AE_MARK_KIND_ENEMY: *glyph = AE_WAYPOINT_GLYPH_MARK_ENEMY; return TRUE;
	case AE_MARK_KIND_ITEM: *glyph = AE_WAYPOINT_GLYPH_MARK_ITEM; return TRUE;
	case AE_MARK_KIND_PLACE: *glyph = AE_WAYPOINT_GLYPH_MARK_PLACE; return TRUE;
	}
	return FALSE;
}

static void draw_icon(short local_player_index, struct wheel_view const *view, int entry, boolean focused, boolean dead,
	real alpha)
{
	struct ae_wheel_place place;
	real red[3], ink[3];
	short glyph = AE_WAYPOINT_GLYPH_CALL_SMART;
	real size = view->layout.icon;
	boolean enemy = FALSE;

	ae_wheel_entry_place(&view->layout, entry, &place);
	tone_rgb(_ae_tone_red, red);
	tone_rgb(focused ? _ae_tone_dark : _ae_tone_blue, ink);
	switch (entry)
	{
	case AE_WHEEL_SMART:
		if (dead)
			glyph = AE_WAYPOINT_GLYPH_CALL_HELP;
		else if (smart_target_glyph(local_player_index, view, &glyph))
			enemy = glyph == AE_WAYPOINT_GLYPH_MARK_ENEMY;
		break;
	case AE_WHEEL_ATTACK: glyph = AE_WAYPOINT_GLYPH_CALL_ATTACK; break;
	case AE_WHEEL_ENEMY: glyph = AE_WAYPOINT_GLYPH_MARK_ENEMY; enemy = TRUE; break;
	case AE_WHEEL_WEAPON:
		/* (the map's own rifle, in the ring's ink; a map without one shows the marks' item glyph) */
		if (draw_rifle(view, view->x + place.icon_x, view->y + place.icon_y, ink, alpha, !focused))
			return;
		glyph = AE_WAYPOINT_GLYPH_MARK_ITEM;
		break;
	case AE_WHEEL_DEFEND: glyph = AE_WAYPOINT_GLYPH_CALL_DEFEND; break;
	case AE_WHEEL_RIDE: glyph = AE_WAYPOINT_GLYPH_CALL_RIDE; size = view->layout.ride; break;
	case AE_WHEEL_GOING: glyph = AE_WAYPOINT_GLYPH_MARK_PLACE; break;
	case AE_WHEEL_WATCH: glyph = AE_WAYPOINT_GLYPH_CALL_WATCH; break;
	default: return;
	}
	/* (an enemy's red keeps its keyline on the focus: red on the pale fill is under 3 to 1 without it) */
	draw_glyph(glyph, view->x + place.icon_x, view->y + place.icon_y, size, enemy ? red : ink, alpha,
		!focused || enemy);
}

/* ---------- the word, the died line, the legend */

static void draw_word(short local_player_index, struct wheel_view const *view, int entry, boolean dead, real alpha)
{
	wchar_t const *word = ae_wheel_word(entry, dead);
	float cell[8][2];
	real moved[8][2];
	real_argb_color fill;
	real rgb[3], x, y;
	float middle_x, middle_y;
	short i;
	real width = ae_hud_text_measure(word, view->layout.word_px) + 2.0f;

	ae_wheel_word_cell(&view->layout, width, cell, &middle_x, &middle_y);
	x = view->x + middle_x;
	y = view->y + middle_y;
	for (i = 0; i < 8; i++)
	{
		moved[i][0] = view->x + cell[i][0];
		moved[i][1] = view->y + cell[i][1];
	}
	tone_rgb(_ae_tone_dark, rgb);
	make_color(rgb, WORD_FILL_ALPHA * alpha, &fill);
	ae_panel_fill_polygon_soft((real const (*)[2])moved, 8, 0.0f, &fill);
	stroke_outline(view, (real const (*)[2])moved, 8, LINE_ALPHA * alpha, KEYLINE_ALPHA * alpha);
	tone_rgb(_ae_tone_white, rgb);
	text_centred(local_player_index, word, x, y, view->layout.word_px, rgb, alpha, _ae_text_outline);
}

static void draw_died_line(short local_player_index, struct wheel_view const *view, real alpha)
{
	real rgb[3], y = view->y + view->layout.died_y;
	real_argb_color white;

	tone_rgb(_ae_tone_white, rgb);
	make_color(rgb, 1.0f, &white);
	ae_hud_backing_draw(local_player_index, view->x, y, DIED_BACKING_ACROSS * view->layout.scale,
		DIED_BACKING_UP * view->layout.scale, BACKING_ALPHA * alpha);
	text_centred(local_player_index, L"PLACED WHERE YOU DIED", view->x, y, view->layout.died_px, rgb, alpha,
		_ae_text_outline);
}

/* the hints under the ring: PICK alone while nothing is focused, then all three. Chips are AE's own (ae_key_prompts.c) */
static void draw_legend(short local_player_index, struct wheel_view const *view, boolean focused, real alpha)
{
	static wchar_t const *const words[3] = { L"PICK", L"RELEASE TO SEND", L"CENTRE TO CANCEL" };
	static int const actions[3] = { AE_CHIP_PICK, AE_CHIP_MARK, -1 };
	real chip_widths[3], rgb[3];
	float chips[3], word[3], at[3];
	real y = view->y + view->layout.legend_y, total;
	int count = focused ? 3 : 1, item;

	for (item = 0; item < count; item++)
	{
		chip_widths[item] = 0.0f;
		if (actions[item] >= 0 &&
			!ae_key_prompts_chip_width(local_player_index, actions[item], view->layout.chip_height, &chip_widths[item]))
		{
			chip_widths[item] = 0.0f;
		}
		chips[item] = chip_widths[item];
		word[item] = ae_hud_text_measure(words[item], view->layout.legend_px) + 2.0f;
	}
	total = ae_wheel_legend_place(&view->layout, count, chips, word, at);
	ae_hud_backing_draw(local_player_index, view->x, y, total * BACKING_ACROSS, BACKING_UP * view->layout.scale,
		BACKING_ALPHA * alpha);
	tone_rgb(_ae_tone_white, rgb);
	for (item = 0; item < count; item++)
	{
		real x = view->x + at[item];

		if (chip_widths[item] > 0.0f)
		{
			real drawn = 0.0f;

			ae_key_prompts_draw_chip(local_player_index, actions[item], x, y, view->layout.chip_height, alpha, &drawn);
			x += chip_widths[item] + view->layout.chip_gap;
		}
		text_left(local_player_index, words[item], x, y, view->layout.legend_px, rgb, alpha);
	}
}

/* ---------- the wheel */

void ae_wheel_draw(short local_player_index)
{
	struct wheel_view view;
	struct wheel_state state;
	int entry;
	real caption_rgb[3];

	if (local_player_index < 0 || !state_get(local_player_index, &state) || !view_get(&view))
		return;
	/* (the sectors' fills as one batch: nothing else draws between) */
	ae_hud_quads_begin();
	for (entry = 0; entry < AE_WHEEL_COUNT; entry++)
	{
		if (ae_wheel_entry_shown(entry, state.dead))
			draw_sector_fill(&view, entry, entry == state.focus, entry == state.focus ? state.flash : 0.0f, state.alpha);
	}
	ae_hud_quads_end();
	for (entry = 0; entry < AE_WHEEL_COUNT; entry++)
	{
		if (ae_wheel_entry_shown(entry, state.dead))
			draw_sector_line(&view, entry, entry == state.focus, state.alpha);
	}
	if (state.focus != AE_WHEEL_NONE)
		draw_wedge(&view, state.focus, state.alpha);
	for (entry = 0; entry < AE_WHEEL_COUNT; entry++)
	{
		struct ae_wheel_place place;
		boolean focused = entry == state.focus;

		if (!ae_wheel_entry_shown(entry, state.dead))
			continue;
		draw_icon(local_player_index, &view, entry, focused, state.dead, state.alpha);
		ae_wheel_entry_place(&view.layout, entry, &place);
		/* (dark on the focus' fill, plain; the HUD white outlined elsewhere) */
		tone_rgb(focused ? _ae_tone_dark : _ae_tone_white, caption_rgb);
		text_centred(local_player_index, ae_wheel_caption(entry, state.dead), view.x + place.caption_x,
			view.y + place.caption_y, view.layout.caption_px, caption_rgb, state.alpha,
			focused ? _ae_text_plain : _ae_text_outline);
	}
	if (state.focus != AE_WHEEL_NONE)
		draw_word(local_player_index, &view, state.focus, state.dead, state.alpha);
	if (state.dead)
		draw_died_line(local_player_index, &view, state.alpha);
	/* (the hints go after the first few calls, never in a split view, and with MARK PROMPT off or the game's hints off) */
	if (ae_wheel_legend_shown(ae_wheel_calls_sent(local_player_index), view.layout.split,
		ae_mark_prompt_enabled(local_player_index)))
		draw_legend(local_player_index, &view, state.focus != AE_WHEEL_NONE, state.alpha);
}
