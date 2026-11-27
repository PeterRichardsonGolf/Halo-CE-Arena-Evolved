/*
AE_HUD_DIGITS.C

CE's counter digits at any size and colour (ae_hud_digits.h): the sheet's
sprites drawn as the HUD draws its numbers (hud_draw_numbers,
source/interface/hud_draw.c), through the HUD's screen-quad pass with the
same blend, but at real (sub-pixel) positions and any scale.
*/

#include <stdio.h>
#include <string.h>

#include "cseries/cseries.h"
#include "math/real_math.h"
#include "bitmaps/bitmaps.h"
#include "bitmaps/bitmap_group.h"
#include "bitmaps/bitmap_color_conversion.h"
#include "cache/texture_cache.h"
#include "interface/hud_draw.h"
#include "interface/interface.h"
#include "rasterizer/rasterizer.h"
#include "render/render.h"
#include "tag_files/tag_files.h"
#include "tag_files/tag_groups.h"

#include "ae_hud_digits.h"

const char *config_string(const char *name);
int config_boolean(char const *name);
/* the high-res HUD's (port/linux/src/hud_hires.c) */
long hud_hires_asset_count(void);
char const *hud_hires_asset_tag(long asset);
long hud_hires_asset_bitmap(long asset);
void hud_hires_register_placeholder(long asset, const unsigned long *texture);
boolean rasterizer_bitmap_new(struct bitmap_data *bitmap);
unsigned long config_changes(void);
void platform_log(char const *format, ...);
void platform_video_drawable_size(int *width, int *height);

/* the HUD's units: the screen is 480 of them high (hud_item_timers.c's hud_waypoint_pixels_per_unit) */
real ae_hud_pixels_per_unit(void)
{
	int width = 0, height = 0;

	platform_video_drawable_size(&width, &height);
	return height > 0 ? (real)height / 480.0f : 1080.0f / 480.0f;
}

/* the HUD digits' definition (hud_draw.c's hud_number_definition): its sheet comes first */
struct ae_hud_number_head { struct tag_reference number_bitmap; };
#define AE_HUD_NUMBER_GROUP_TAG 'hud#'
#define AE_BITMAP_GROUP_INTERFACE 4 /* _bitmap_group_type_interface_bitmaps */
#define AE_BLEND_ALPHA_MULTIPLY_ADD 7 /* _shader_framebuffer_blend_function_alpha_multiply_add, the HUD's */

/* DARK (6,10,18), the keyline's and the glow's */
static const real_rgb_color dark = { 6.0f / 255.0f, 10.0f / 255.0f, 18.0f / 255.0f };

real ae_hud_digits_measure(const wchar_t *text, real cap_px)
{
	return ae_digits_measure_cells(text, cap_px, 0.0f);
}

boolean ae_hud_digits_ink_bounds(const wchar_t *text, real cap_px, real cell_px, real *left, real *right)
{
	return ae_digits_ink_bounds_cells(text, cap_px, cell_px, FALSE, left, right);
}

/* (the HUD's blend for its sheets, alpha multiply add, adds the colour to what is under it by (1 - alpha): the
colour goes in premultiplied by its alpha, or a translucent glyph is drawn too bright) */
static pixel32 pixel(real_rgb_color const *rgb, real alpha)
{
	real_argb_color color;

	color.alpha = alpha < 0.0f ? 0.0f : alpha > 1.0f ? 1.0f : alpha;
	color.red = rgb->red * color.alpha;
	color.green = rgb->green * color.alpha;
	color.blue = rgb->blue * color.alpha;
	return real_argb_color_to_pixel32(&color);
}

/* a glyph's sprite: its bitmap (texture ready) and texture rectangle, and its width over its height */
struct glyph_sprite { struct bitmap_data const *bitmap; real_rectangle2d clip; real aspect; };

/* AE'S DIGITS ON EVERY MAP (owner 2026-10-20: "always AE's digits"): a map with a counter sheet of its own (h2_coagulation's
"geno lightning vinx hud") still draws the score strip and the panels with the high-res HUD's copy of the stock sheet,
through a placeholder bitmap the texture cache draws that texture for (hud_hires_tags.c's buttons' way). Its sprites
are the stock sheet's (128 by 128; sequence 0's frames, left right top bottom), which the custom sheets keep. With
display.high_res_hud off the map's own sheet is drawn, as the classic HUD draws it. */
#define STOCK_SHEET_TAG "ui\\hud\\bitmaps\\combined\\hud_counter_numbers"
#define STOCK_SHEET_FRAMES 15
static const real stock_sprites[STOCK_SHEET_FRAMES][4] = {
	{ 0.000000f, 0.140625f, 0.187500f, 0.281250f }, { 0.000000f, 0.140625f, 0.000000f, 0.093750f },
	{ 0.140625f, 0.281250f, 0.000000f, 0.093750f }, { 0.281250f, 0.421875f, 0.000000f, 0.093750f },
	{ 0.265625f, 0.414062f, 0.093750f, 0.187500f }, { 0.554688f, 0.695312f, 0.093750f, 0.187500f },
	{ 0.835938f, 0.976562f, 0.093750f, 0.187500f }, { 0.140625f, 0.281250f, 0.187500f, 0.281250f },
	{ 0.843750f, 0.984375f, 0.000000f, 0.093750f }, { 0.421875f, 0.562500f, 0.000000f, 0.093750f },
	{ 0.562500f, 0.703125f, 0.000000f, 0.093750f }, { 0.414062f, 0.554688f, 0.093750f, 0.187500f },
	{ 0.703125f, 0.843750f, 0.000000f, 0.093750f }, { 0.695312f, 0.835938f, 0.093750f, 0.187500f },
	{ 0.000000f, 0.265625f, 0.093750f, 0.187500f },
};
static struct
{
	struct bitmap_data *placeholder;
	long asset;
	boolean attempted;
} stock_sheet;

/* the placeholder for the stock sheet's high-res texture, made when first needed; NULL for none */
static struct bitmap_data *stock_sheet_placeholder(void)
{
	long asset;

	if (stock_sheet.attempted)
		return stock_sheet.placeholder;
	stock_sheet.attempted = TRUE;
	for (asset = 0; asset < hud_hires_asset_count(); asset++)
	{
		struct bitmap_data *bitmap;

		if (strcmp(hud_hires_asset_tag(asset), STOCK_SHEET_TAG) || hud_hires_asset_bitmap(asset) != 0)
			continue;
		/* (a small square a8r8g8b8 bitmap: the quads take the texture's coordinates 0 to 1, whatever its size) */
		bitmap = bitmap_2d_new(64, 64, 0, 11);
		if (bitmap && !rasterizer_bitmap_new(bitmap))
		{
			bitmap_delete(bitmap);
			bitmap = NULL;
		}
		if (!bitmap)
			break;
		hud_hires_register_placeholder(asset, (unsigned long const *)xbox_pointer(bitmap->hardware_format));
		stock_sheet.placeholder = bitmap;
		stock_sheet.asset = asset;
		platform_log("hud digits: the map's counter sheet is its own; AE's digits drawn from the stock sheet's high-res copy");
		break;
	}
	return stock_sheet.placeholder;
}

void ae_hud_digits_tags_unloaded(void)
{
	if (stock_sheet.placeholder)
	{
		hud_hires_register_placeholder(stock_sheet.asset, NULL);
		bitmap_delete(stock_sheet.placeholder);
	}
	csmemset(&stock_sheet, 0, sizeof(stock_sheet));
}

/* a map's own counter sheet, drawn from the stock sheet's high-res copy (FALSE: the map's own sheet is drawn) */
static boolean stock_sheet_sprite(long bitmap_tag_index, short frame, struct glyph_sprite *sprite)
{
	char const *name = tag_get_name(bitmap_tag_index);
	struct bitmap_data *placeholder;

	if (!name || !_stricmp(name, STOCK_SHEET_TAG) || frame >= STOCK_SHEET_FRAMES ||
		!config_boolean("display.high_res_hud") || !(placeholder = stock_sheet_placeholder()))
	{
		return FALSE;
	}
	sprite->bitmap = placeholder;
	sprite->clip.x0 = stock_sprites[frame][0];
	sprite->clip.x1 = stock_sprites[frame][1];
	sprite->clip.y0 = stock_sprites[frame][2];
	sprite->clip.y1 = stock_sprites[frame][3];
	sprite->aspect = (sprite->clip.x1 - sprite->clip.x0) / (sprite->clip.y1 - sprite->clip.y0);
	return TRUE;
}

static boolean glyph_sprite(wchar_t character, struct glyph_sprite *sprite)
{
	long number_index = interface_get_tag_index(_interface_hud_digits);
	struct ae_hud_number_head const *number;
	struct bitmap_group const *group;
	struct bitmap_data const *bitmap = NULL;
	real_rectangle2d const *clip = NULL;
	short frame = ae_digit_frame(character);
	real width, height;
	boolean interface_bitmap;

	if (number_index == NONE || frame < 0)
		return FALSE;
	number = (struct ae_hud_number_head const *)tag_get(AE_HUD_NUMBER_GROUP_TAG, number_index);
	if (!number || number->number_bitmap.index == NONE)
		return FALSE;
	if (stock_sheet_sprite(number->number_bitmap.index, frame, sprite))
		return TRUE;
	group = bitmap_group_get(number->number_bitmap.index);
	hud_retrieve_bitmap_and_bounding_rect(number->number_bitmap.index, 0, frame, &bitmap, &clip);
	if (!group || !bitmap || !clip || !_texture_cache_bitmap_get_hardware_format((struct bitmap_data *)bitmap,
		FALSE, TRUE))
	{
		return FALSE;
	}
	interface_bitmap = (boolean)(group->type == AE_BITMAP_GROUP_INTERFACE);
	width = (clip->x1 - clip->x0) * (interface_bitmap ? 1.0f : (real)bitmap->width);
	height = (clip->y1 - clip->y0) * (interface_bitmap ? 1.0f : (real)bitmap->height);
	if (width <= 0.0f || height <= 0.0f)
		return FALSE;
	sprite->bitmap = bitmap;
	sprite->clip = *clip;
	sprite->aspect = width / height;
	return TRUE;
}

/* one textured quad, in the view's screen pixels (drawn in the HUD's units), as hud_draw.c's
hud_draw_bitmap_internal draws it */
static void draw_quad(struct glyph_sprite const *sprite, real x0, real y0, real x1, real y1, pixel32 color)
{
	struct dynamic_screen_vertex vertices[4];
	struct rasterizer_dynamic_screen_geometry_parameters parameters;
	real units = 1.0f / ae_hud_pixels_per_unit();
	short i;

	for (i = 0; i < 4; i++)
	{
		boolean right = (boolean)(((i + 1) & 2) != 0);
		boolean bottom = (boolean)(i > 1);

		vertices[i].position.x = (right ? x1 : x0) * units;
		vertices[i].position.y = (bottom ? y1 : y0) * units;
		vertices[i].texture_coordinates.x = right ? sprite->clip.x1 : sprite->clip.x0;
		vertices[i].texture_coordinates.y = bottom ? sprite->clip.y1 : sprite->clip.y0;
		vertices[i].color = color;
	}
	csmemset(&parameters, 0, sizeof(parameters));
	parameters.map_texture_scale[0].i = 1.0f;
	parameters.map_texture_scale[0].j = 1.0f;
	parameters.map_scale[0].i = 1.0f;
	parameters.map_scale[0].j = 1.0f;
	parameters.framebuffer_blend_function = AE_BLEND_ALPHA_MULTIPLY_ADD;
	parameters.map[0] = (struct bitmap_data *)sprite->bitmap;
	rasterizer_psuedo_dynamic_screen_quad_draw(&parameters, vertices);
}

/* the text's sprites, looked up once a draw (ae_hud_digits_draw_ex) for its 10 to 34 passes: the first
text_sprite_count characters' */
#define TEXT_SPRITES 64
static struct { struct glyph_sprite sprite; boolean found; } text_sprites[TEXT_SPRITES];
static long text_sprite_count;

static void text_sprites_find(wchar_t const *text)
{
	for (text_sprite_count = 0; text_sprite_count < TEXT_SPRITES && text[text_sprite_count]; text_sprite_count++)
		text_sprites[text_sprite_count].found = glyph_sprite(text[text_sprite_count],
			&text_sprites[text_sprite_count].sprite);
}

/* every glyph of the text, moved by (dx, dy), in one colour */
static void draw_pass(wchar_t const *text, real x, real baseline_y, real cap_px,
	struct ae_hud_digits_options const *options, real dx, real dy, pixel32 color)
{
	long i;

	for (i = 0; text[i]; i++)
	{
		struct glyph_sprite sprite;
		struct ae_digit_quad quad;

		if (i < text_sprite_count)
		{
			if (!text_sprites[i].found)
				continue;
			sprite = text_sprites[i].sprite;
		}
		else if (!glyph_sprite(text[i], &sprite))
			continue;
		ae_digit_quad(text, i, x, baseline_y, cap_px, options->cell_px, options->ink_left, sprite.aspect, &quad);
		if (quad.visible)
			draw_quad(&sprite, quad.x0 + dx, quad.y0 + dy, quad.x1 + dx, quad.y1 + dy, color);
	}
}

/* the glyphs, emboldened: again a share of the cap height to the right */
static void draw_fill(wchar_t const *text, real x, real baseline_y, real cap_px,
	struct ae_hud_digits_options const *options, real dx, real dy, pixel32 color)
{
	draw_pass(text, x, baseline_y, cap_px, options, dx, dy, color);
	draw_pass(text, x, baseline_y, cap_px, options, dx + AE_DIGITS_EMBOLDEN * cap_px, dy, color);
}

/* eight passes round a point, r out */
static void draw_ring(wchar_t const *text, real x, real baseline_y, real cap_px,
	struct ae_hud_digits_options const *options, real r, pixel32 color, boolean emboldened)
{
	static const real directions[8][2] = { { 1.0f, 0.0f }, { -1.0f, 0.0f }, { 0.0f, 1.0f }, { 0.0f, -1.0f },
		{ 0.7071f, 0.7071f }, { -0.7071f, 0.7071f }, { 0.7071f, -0.7071f }, { -0.7071f, -0.7071f } };
	short i;

	for (i = 0; i < 8; i++)
	{
		if (emboldened)
			draw_fill(text, x, baseline_y, cap_px, options, directions[i][0] * r, directions[i][1] * r, color);
		else
			draw_pass(text, x, baseline_y, cap_px, options, directions[i][0] * r, directions[i][1] * r, color);
	}
}

void ae_hud_digits_draw_ex(short local_player_index, real x, real baseline_y, const wchar_t *text, real cap_px,
	const union real_argb_color *color, const struct ae_hud_digits_options *options)
{
	pixel32 fill;

	(void)local_player_index;
	if (!text || !text[0] || !color || !options || cap_px <= 0.0f)
		return;
	fill = pixel(&color->rgb, color->alpha * options->fill_alpha);
	text_sprites_find(text);
	switch (options->style)
	{
	case _ae_digits_glow:
	{
		/* (a halo, not a contour: four passes 1.5 x cap / 24 out, at half the glow's alpha) */
		real r = 1.5f * cap_px / 24.0f;
		pixel32 glow = pixel(&dark, options->glow_alpha * 0.5f * color->alpha);

		draw_fill(text, x, baseline_y, cap_px, options, r, 0.0f, glow);
		draw_fill(text, x, baseline_y, cap_px, options, -r, 0.0f, glow);
		draw_fill(text, x, baseline_y, cap_px, options, 0.0f, r, glow);
		draw_fill(text, x, baseline_y, cap_px, options, 0.0f, -r, glow);
		draw_fill(text, x, baseline_y, cap_px, options, 0.0f, 0.0f, fill);
		break;
	}
	case _ae_digits_keyline:
		draw_ring(text, x, baseline_y, cap_px, options, 1.0f, pixel(&dark, 0.90f * color->alpha), TRUE);
		draw_fill(text, x, baseline_y, cap_px, options, 0.0f, 0.0f, fill);
		break;
	case _ae_digits_outline:
		/* (the 1.5 px line: the glyph widened 0.75 px each way in colour, its inside dark again, keylined) */
		draw_ring(text, x, baseline_y, cap_px, options, 1.75f, pixel(&dark, 0.90f * color->alpha), TRUE);
		draw_ring(text, x, baseline_y, cap_px, options, 0.75f, fill, TRUE);
		draw_fill(text, x, baseline_y, cap_px, options, 0.0f, 0.0f, pixel(&dark, 0.90f * color->alpha));
		break;
	default:
		draw_fill(text, x, baseline_y, cap_px, options, 0.0f, 0.0f, fill);
		break;
	}
	text_sprite_count = 0;
}

void ae_hud_digits_draw(short local_player_index, real x, real baseline_y, const wchar_t *text, real cap_px,
	const union real_argb_color *color, real glow_alpha)
{
	struct ae_hud_digits_options options;

	csmemset(&options, 0, sizeof(options));
	options.style = _ae_digits_glow;
	options.fill_alpha = 1.0f;
	options.glow_alpha = glow_alpha;
	ae_hud_digits_draw_ex(local_player_index, x, baseline_y, text, cap_px, color, &options);
}

/* ---------- debug.digits */

#define AE_DIGITS_DEBUG_X 40.0f
#define AE_DIGITS_DEBUG_BASELINE 150.0f
#define AE_DIGITS_DEBUG_CAP 24.0f

void ae_hud_digits_debug_draw(short local_player_index)
{
	static boolean logged = FALSE;
	static unsigned long read_at = (unsigned long)-1;
	static char setting[32];
	wchar_t text[32];
	real_argb_color magenta;
	struct ae_hud_digits_options options;
	real left = 0.0f, right = 0.0f;
	short i;

	if (local_player_index != 0)
		return;
	/* (read again only when a setting changed) */
	if (read_at != config_changes())
	{
		char const *value = config_string("debug.digits");

		read_at = config_changes();
		snprintf(setting, sizeof(setting), "%s", value ? value : "");
	}
	if (!setting[0])
		return;
	for (i = 0; i < (short)(NUMBEROF(text) - 1) && setting[i]; i++)
		text[i] = (wchar_t)(unsigned char)setting[i];
	text[i] = 0;
	magenta.alpha = 1.0f;
	magenta.red = 1.0f;
	magenta.green = 0.0f;
	magenta.blue = 1.0f;
	csmemset(&options, 0, sizeof(options));
	options.style = _ae_digits_plain;
	options.fill_alpha = 1.0f;
	ae_hud_digits_draw_ex(local_player_index, AE_DIGITS_DEBUG_X, AE_DIGITS_DEBUG_BASELINE, text, AE_DIGITS_DEBUG_CAP,
		&magenta, &options);
	/* (under it, to look at: the strip's GLOW in white, the panels' KEYLINE in a fixed 21 px cell and OUTLINE in
	the HUD's blue) */
	{
		real_argb_color white = { { 1.0f, 229.0f / 255.0f, 240.0f / 255.0f, 1.0f } };
		real_argb_color blue = { { 1.0f, 178.0f / 255.0f, 224.0f / 255.0f, 1.0f } };

		ae_hud_digits_draw(local_player_index, AE_DIGITS_DEBUG_X, AE_DIGITS_DEBUG_BASELINE + 50.0f, text,
			AE_DIGITS_DEBUG_CAP, &white, 0.85f);
		options.style = _ae_digits_keyline;
		options.cell_px = 21.0f;
		ae_hud_digits_draw_ex(local_player_index, AE_DIGITS_DEBUG_X, AE_DIGITS_DEBUG_BASELINE + 100.0f, text,
			AE_DIGITS_DEBUG_CAP, &white, &options);
		options.style = _ae_digits_outline;
		ae_hud_digits_draw_ex(local_player_index, AE_DIGITS_DEBUG_X, AE_DIGITS_DEBUG_BASELINE + 150.0f, text,
			AE_DIGITS_DEBUG_CAP, &blue, &options);
		options.cell_px = 0.0f;
	}
	if (!logged)
	{
		struct glyph_sprite sprite;

		logged = TRUE;
		ae_hud_digits_ink_bounds(text, AE_DIGITS_DEBUG_CAP, 0.0f, &left, &right);
		platform_log("hud digits: '%s' at %.1f px cap, x %.1f baseline %.1f: advance %.2f, ink %.2f..%.2f, "
			"emboldened %.2f, sheet %s", setting, AE_DIGITS_DEBUG_CAP, AE_DIGITS_DEBUG_X, AE_DIGITS_DEBUG_BASELINE,
			ae_hud_digits_measure(text, AE_DIGITS_DEBUG_CAP), AE_DIGITS_DEBUG_X + left, AE_DIGITS_DEBUG_X + right,
			AE_DIGITS_EMBOLDEN * AE_DIGITS_DEBUG_CAP, glyph_sprite(L'0', &sprite) ? "found" : "missing");
	}
}
