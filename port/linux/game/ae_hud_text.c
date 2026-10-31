/*
AE_HUD_TEXT.C

AE's HUD text, the backing and simple shapes (ae_hud_text.h), drawn with the
HUD's own text and screen-quad passes.
*/

#include <math.h>
#include <string.h>

#include "cseries/cseries.h"
#include "math/real_math.h"
#include "bitmaps/bitmaps.h"
#include "bitmaps/bitmap_group.h"
#include "bitmaps/bitmap_color_conversion.h"
#include "game/game_globals.h"
#include "interface/hud.h"
#include "interface/hud_definitions.h"
#include "rasterizer/rasterizer.h"
#include "render/render.h"
#include "scenario/scenario.h"
#include "tag_files/tag_groups.h"
#include "text/draw_string.h"
#include "text/font_group.h"

#include <xtl.h>

#include "rasterizer/xbox/rasterizer_xbox.h"

#include "ae_hud_digits.h"
#include "ae_hud_text.h"

#define AE_TEXT_JUSTIFY_LEFT 0 /* _text_justification_left */
#define AE_BLEND_ALPHA 0       /* _shader_framebuffer_blend_function_alpha_blend */

static const real dark_rgb[3] = { 6.0f / 255.0f, 10.0f / 255.0f, 18.0f / 255.0f };

/* ---------- the font */

static long hud_font(void)
{
	if (!hud_globals)
		return NONE;
	return hud_globals->messaging.multi_player_font.index != NONE ? hud_globals->messaging.multi_player_font.index :
		hud_globals->messaging.single_player_font.index;
}

/* the font's capital height at full size (its "H"'s top over the baseline), in HUD units */
static real font_cap(long font_index)
{
	struct font_header *font = font_definition_get(font_index);
	struct font_character const *glyph = font ? font_get_character_by_ascii_code(font, (word)'H') : NULL;

	if (!font)
		return 1.0f;
	if (glyph && glyph->bitmap_origin_y > 0 && glyph->bitmap_origin_y <= font->ascending_height + 2)
		return (real)glyph->bitmap_origin_y;
	return (real)MAX(font->ascending_height, 1);
}

/* the scale that draws the font at px (its em, in screen pixels) */
static real font_scale(long font_index, real px)
{
	return px * AE_HUD_TEXT_CAP / ae_hud_pixels_per_unit() / font_cap(font_index);
}

real ae_hud_text_measure(const wchar_t *text, real px)
{
	long font_index = hud_font();
	real_argb_color white = { { 1.0f, 1.0f, 1.0f, 1.0f } };
	rectangle2d bounds, text_bounds, cursor_bounds;
	struct font_header *font;

	if (!text || !text[0] || font_index == NONE || px <= 0.0f)
		return 0.0f;
	font = font_definition_get(font_index);
	bounds.x0 = 0;
	bounds.y0 = 0;
	bounds.x1 = SHORT_MAX / 2;
	bounds.y1 = (short)MAX(font->ascending_height + font->descending_height + font->leading_height, 10);
	draw_string_set_draw_mode(font_index, NONE, AE_TEXT_JUSTIFY_LEFT, 0, &white);
	draw_unicode_string_compute_bounds(&bounds, text, &text_bounds, &cursor_bounds);
	return (real)(cursor_bounds.x0 - bounds.x0 - font->leading_width) * font_scale(font_index, px) *
		ae_hud_pixels_per_unit();
}

/* (0.3.9) HUD TEXT BATCHING: an outlined text's 13 passes (the shadow's 4, the outline's 8, the face) drawn as one.
The text's batch (rasterizer_xbox_text.c, a string's characters as one list of quads) is held open from the first pass
to the last while each pass begins with the same state (the same parameters: the same font bitmap, blend, scale); a
pass of another state draws what is held first. ae_hud_text_hold_begin and _end are the AE hooks there: begin TRUE =
the batch goes on (no state set again), end TRUE = not drawn yet */
void rasterizer_text_flush(void);
void rasterizer_text_end(void);
static struct
{
	int held, begun;
	int size;
	unsigned char parameters[512];
} text_hold;

int ae_hud_text_hold_begin(void const *parameters, int size)
{
	if (!text_hold.held)
		return FALSE;
	/* (the text's begin reads its caller's parameters, rasterizer_text.c's dynamic screen geometry parameters, as a
	struct of its own: compared as far as the caller's go) */
	if (size > (int)sizeof(struct rasterizer_dynamic_screen_geometry_parameters))
		size = (int)sizeof(struct rasterizer_dynamic_screen_geometry_parameters);
	if (text_hold.begun && size == text_hold.size && !memcmp(parameters, text_hold.parameters, (size_t)size))
		return TRUE;
	if (text_hold.begun)
		rasterizer_text_flush();
	text_hold.begun = size > 0 && size <= (int)sizeof(text_hold.parameters);
	if (text_hold.begun)
	{
		csmemcpy(text_hold.parameters, parameters, size);
		text_hold.size = size;
	}
	return FALSE;
}

int ae_hud_text_hold_end(void)
{
	return text_hold.held;
}

static void text_hold_open(void)
{
	text_hold.held = TRUE;
	text_hold.begun = FALSE;
}

static void text_hold_close(void)
{
	int begun = text_hold.begun;

	text_hold.held = FALSE;
	text_hold.begun = FALSE;
	if (begun)
		rasterizer_text_end();
}

/* one pass of the text: its pen's start at (x, baseline) in screen pixels, moved (dx, dy) */
static void text_pass(long font_index, real x, real baseline_y, const wchar_t *text, real px, real dx, real dy,
	real const rgb[3], real alpha)
{
	struct font_header *font = font_definition_get(font_index);
	real ppu = ae_hud_pixels_per_unit();
	real scale = font_scale(font_index, px);
	real target_x = (x + dx) / ppu, target_y = (baseline_y + dy) / ppu;
	real layout_x, layout_y, origin_x, origin_y;
	real_argb_color color;
	rectangle2d bounds;

	color.alpha = alpha < 0.0f ? 0.0f : alpha > 1.0f ? 1.0f : alpha;
	color.red = rgb[0];
	color.green = rgb[1];
	color.blue = rgb[2];
	/* (the text is laid out at full size from whole units, then scaled about an origin chosen so that its pen and
	baseline land on the target exactly: origin + (layout - origin) x scale = target) */
	if (fabsf(scale - 1.0f) < 0.002f)
		scale = 0.998f;
	bounds.x0 = (short)floor(target_x);
	bounds.y0 = (short)floor(target_y - font->ascending_height);
	bounds.x1 = (short)(bounds.x0 + SHORT_MAX / 4);
	bounds.y1 = (short)(bounds.y0 + MAX(font->ascending_height + font->descending_height + font->leading_height, 10));
	layout_x = (real)(bounds.x0 + font->leading_width);
	layout_y = (real)(bounds.y0 + font->ascending_height);
	origin_x = (target_x - layout_x * scale) / (1.0f - scale);
	origin_y = (target_y - layout_y * scale) / (1.0f - scale);
	draw_string_set_draw_mode(font_index, NONE, AE_TEXT_JUSTIFY_LEFT, 0, &color);
	rasterizer_text_set_scale(scale, origin_x, origin_y);
	rasterizer_draw_unicode_string(&bounds, NULL, NULL, 0, text);
	rasterizer_text_set_scale(1.0f, 0.0f, 0.0f);
}

void ae_hud_text_draw_ex(short local_player_index, real x, real baseline_y, const wchar_t *text, real px,
	const union real_argb_color *color, enum ae_hud_text_edge edge)
{
	static const real directions[8][2] = { { 1.0f, 0.0f }, { -1.0f, 0.0f }, { 0.0f, 1.0f }, { 0.0f, -1.0f },
		{ 0.7071f, 0.7071f }, { -0.7071f, 0.7071f }, { 0.7071f, -0.7071f }, { -0.7071f, -0.7071f } };
	long font_index = hud_font();
	real rgb[3];
	short i;

	(void)local_player_index;
	if (!text || !text[0] || !color || font_index == NONE || px <= 0.0f || color->alpha <= 0.0f)
		return;
	rgb[0] = color->red;
	rgb[1] = color->green;
	rgb[2] = color->blue;
	if (edge != _ae_text_plain)
		text_hold_open();
	if (edge == _ae_text_outline)
	{
		/* (the soft shadow: 1.5 px down, spread about 2 px) */
		static const real shadow[4][2] = { { -1.0f, 1.0f }, { 1.0f, 1.0f }, { 0.0f, 2.0f }, { 0.0f, 1.5f } };
		real width = 2.0f * (real)sqrt(px / 36.0f);

		for (i = 0; i < 4; i++)
			text_pass(font_index, x, baseline_y, text, px, shadow[i][0], shadow[i][1], dark_rgb,
				0.55f * 0.35f * color->alpha);
		for (i = 0; i < 8; i++)
			text_pass(font_index, x, baseline_y, text, px, directions[i][0] * width, directions[i][1] * width,
				dark_rgb, 0.88f * color->alpha);
	}
	else if (edge == _ae_text_keyline)
	{
		for (i = 0; i < 8; i++)
			text_pass(font_index, x, baseline_y, text, px, directions[i][0], directions[i][1], dark_rgb,
				0.90f * color->alpha);
	}
	text_pass(font_index, x, baseline_y, text, px, 0.0f, 0.0f, rgb, color->alpha);
	if (edge != _ae_text_plain)
		text_hold_close();
}

void ae_hud_text_draw(short local_player_index, real x, real baseline_y, const wchar_t *text, real px,
	const union real_argb_color *color, boolean outline)
{
	ae_hud_text_draw_ex(local_player_index, x, baseline_y, text, px, color,
		outline ? _ae_text_outline : _ae_text_plain);
}

/* ---------- shapes */

/* a white texel for flat shapes: the rasterizer's default texture, as cinematics.c's draw_quad takes it */
static struct bitmap_data *white_map(void)
{
	struct game_globals *globals = scenario_get_game_globals();
	struct game_globals_rasterizer_data *data;
	struct bitmap_group *group;

	if (!globals || globals->rasterizer_data.count <= 0)
		return NULL;
	data = TAG_BLOCK_GET_ELEMENT(&globals->rasterizer_data, 0, struct game_globals_rasterizer_data);
	if (data->default_textures[0].index == NONE)
		return NULL;
	group = bitmap_group_get(data->default_textures[0].index);
	if (!group || group->bitmaps.count < 2)
		return NULL;
	return TAG_BLOCK_GET_ELEMENT(&group->bitmaps, 1, struct bitmap_data);
}

static pixel32 to_pixel(const union real_argb_color *color)
{
	real_argb_color c = *color;

	c.alpha = c.alpha < 0.0f ? 0.0f : c.alpha > 1.0f ? 1.0f : c.alpha;
	return real_argb_color_to_pixel32(&c);
}

/* (0.3.9 frame rate) flat quads in a row drawn as one triangle list: each quad was a draw of its own (the HUD's band
ramps drew ~3,650 a frame alive, ~14 ms of CPU, most of it in the GL driver). Open from ae_hud_quads_begin to
ae_hud_quads_end, between which nothing else draws; full, it draws what it holds and goes on */
enum { QUAD_BATCH_MAXIMUM = 512 };
static struct
{
	int open, drawing;
	short count;
	struct dynamic_screen_vertex vertices[QUAD_BATCH_MAXIMUM][4];
} quad_batch;

static void quad_draw(struct dynamic_screen_vertex *vertices, struct bitmap_data *map)
{
	struct rasterizer_dynamic_screen_geometry_parameters parameters;

	csmemset(&parameters, 0, sizeof(parameters));
	parameters.framebuffer_blend_function = AE_BLEND_ALPHA;
	parameters.map_texture_scale[0].i = 1.0f;
	parameters.map_texture_scale[0].j = 1.0f;
	parameters.map_scale[0].i = 1.0f;
	parameters.map_scale[0].j = 1.0f;
	parameters.map[0] = map;
	rasterizer_psuedo_dynamic_screen_quad_draw(&parameters, vertices);
}

/* the held quads drawn: one quad draw's state, then all of them (ae_hud_quad_batch_submit, its AE hook) */
static void quad_batch_flush(void)
{
	struct bitmap_data *map = white_map();

	if (quad_batch.count && map)
	{
		quad_batch.drawing = TRUE;
		quad_draw(quad_batch.vertices[0], map);
		quad_batch.drawing = FALSE;
	}
	quad_batch.count = 0;
}

void ae_hud_quads_begin(void)
{
	quad_batch.open = TRUE;
	quad_batch.count = 0;
}

void ae_hud_quads_end(void)
{
	quad_batch_flush();
	quad_batch.open = FALSE;
}

int ae_hud_quad_batch_submit(void (*submit)(struct dynamic_screen_vertex const *vertex))
{
	static const short fan[6] = { 0, 1, 2, 0, 2, 3 };
	short quad, corner;

	if (!quad_batch.drawing)
		return FALSE;
	IDirect3DDevice8_Begin(global_d3d_device, D3DPT_TRIANGLELIST);
	for (quad = 0; quad < quad_batch.count; quad++)
	{
		for (corner = 0; corner < 6; corner++)
			submit(&quad_batch.vertices[quad][fan[corner]]);
	}
	IDirect3DDevice8_End(global_d3d_device);
	return TRUE;
}

void ae_hud_fill_quad(const real points[4][2], const union real_argb_color *colors)
{
	struct dynamic_screen_vertex vertices[4];
	struct bitmap_data *map = white_map();
	real units = 1.0f / ae_hud_pixels_per_unit();
	short i;

	real area = 0.0f;
	short order[4] = { 0, 1, 2, 3 };

	if (!map)
		return;
	/* (the screen quad wants its corners clockwise on the screen, as the HUD's own: a quad given the other way
	round, a line's or a triangle's, is turned) */
	for (i = 0; i < 4; i++)
		area += points[i][0] * points[(i + 1) % 4][1] - points[(i + 1) % 4][0] * points[i][1];
	if (area < 0.0f)
	{
		order[1] = 3;
		order[3] = 1;
	}
	for (i = 0; i < 4; i++)
	{
		vertices[i].position.x = points[order[i]][0] * units;
		vertices[i].position.y = points[order[i]][1] * units;
		vertices[i].texture_coordinates.x = 0.0f;
		vertices[i].texture_coordinates.y = 0.0f;
		vertices[i].color = to_pixel(&colors[order[i]]);
	}
	if (quad_batch.open)
	{
		if (quad_batch.count == QUAD_BATCH_MAXIMUM)
			quad_batch_flush();
		csmemcpy(quad_batch.vertices[quad_batch.count++], vertices, sizeof(vertices));
		return;
	}
	quad_draw(vertices, map);
}

void ae_hud_fill_quad_color(const real points[4][2], const union real_argb_color *color)
{
	real_argb_color colors[4];

	colors[0] = colors[1] = colors[2] = colors[3] = *color;
	ae_hud_fill_quad(points, colors);
}

void ae_hud_line(real x0, real y0, real x1, real y1, real width, const union real_argb_color *color)
{
	real dx = x1 - x0, dy = y1 - y0;
	real length = (real)sqrt(dx * dx + dy * dy);
	real nx, ny;
	real points[4][2];

	if (length <= 0.0f || width <= 0.0f)
		return;
	nx = -dy / length * width / 2.0f;
	ny = dx / length * width / 2.0f;
	points[0][0] = x0 + nx; points[0][1] = y0 + ny;
	points[1][0] = x1 + nx; points[1][1] = y1 + ny;
	points[2][0] = x1 - nx; points[2][1] = y1 - ny;
	points[3][0] = x0 - nx; points[3][1] = y0 - ny;
	ae_hud_fill_quad_color(points, color);
}

/* the backing: a grid of quads whose corners take the falloff's alpha, so the colour shades smoothly between them
(an elliptical falloff (1 - r^2)^1.6 x peak, no edge: mock4.py's backing) */
#define AE_BACKING_COLUMNS 16
#define AE_BACKING_ROWS 8

static real backing_alpha(real u, real v, real peak)
{
	real r = u * u + v * v;
	real a = r >= 1.0f ? 0.0f : (real)pow(1.0f - r, 1.6f);

	return a * peak;
}

void ae_hud_backing_draw(short local_player_index, real cx, real cy, real rx, real ry, real peak_alpha)
{
	real down = ry * 30.0f / 46.0f;
	short column, row;

	(void)local_player_index;
	if (rx <= 0.0f || ry <= 0.0f || peak_alpha <= 0.0f)
		return;
	ae_hud_quads_begin();
	for (row = 0; row < AE_BACKING_ROWS; row++)
	{
		for (column = 0; column < AE_BACKING_COLUMNS; column++)
		{
			real points[4][2];
			real_argb_color colors[4];
			short corner;

			for (corner = 0; corner < 4; corner++)
			{
				short c = (short)(column + (corner == 1 || corner == 2));
				short r = (short)(row + (corner >= 2));
				real u = -1.0f + 2.0f * (real)c / AE_BACKING_COLUMNS;
				real v = -1.0f + 2.0f * (real)r / AE_BACKING_ROWS;

				points[corner][0] = cx + u * rx;
				points[corner][1] = cy + (v < 0.0f ? v * ry : v * down);
				colors[corner].alpha = backing_alpha(u, v, peak_alpha);
				colors[corner].red = dark_rgb[0];
				colors[corner].green = dark_rgb[1];
				colors[corner].blue = dark_rgb[2];
			}
			ae_hud_fill_quad(points, colors);
		}
	}
	ae_hud_quads_end();
}
