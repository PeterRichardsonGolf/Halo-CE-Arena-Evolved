/*
AE_KEY_PROMPTS.C

The HUD's prompts on mouse + keyboard: the bound key in place of the pad's
button (ae_key_prompts.h).
*/

#include <math.h>
#include <string.h>

#include "cseries/cseries.h"
#include "math/real_math.h"
#include "bitmaps/bitmaps.h"
#include "bitmaps/bitmap_color_conversion.h"
#include "memory/data.h"
#include "game/players.h"
#include "input/input_abstraction.h"
#include "interface/player_ui.h"
#include "networking/network_game_globals.h"
#include "rasterizer/rasterizer.h"
#include "render/render.h"
#include "text/draw_string.h"
#include "text/font_group.h"

#include "ae_buttons_tags.h"
#include "ae_hud_digits.h"
#include "ae_hud_messages.h"
#include "ae_hud_style.h"
#include "ae_hud_text.h"
#include "ae_key_prompts.h"
#include "ae_key_prompts_rules.h"

/* the platform's (port/linux/src: ae_platform.c, ae_seats.c, xinput_sdl.c; the server's stubs) and port_config.c's */
int ae_platform_keyboard_last_used(void);
int ae_seats_active(void);
int ae_seats_keyboard_controller(void);
int halo_input_from_name(const char *name);
void halo_input_shown_name(const char *binding, char *shown, size_t size);
const char *config_string(const char *name);
unsigned long config_changes(void);
void platform_log(char const *format, ...);

#define AE_BLEND_ALPHA 0 /* _shader_framebuffer_blend_function_alpha_blend: the sheets are straight alpha */
#define KEY_NAME_MAXIMUM 32

/* CE's lines (HUD units at the chip's scale, which the hooks hand over: hud_messaging.c's high-res chip 0.75 as large in
split screen, as hud_hires_tags.c draws it; a custom line's text icon hud_globals_get_scale's, and drawn from the small
icons' sheet, CE_TEXT_ICON as large as a message's chip, measured on the box at 1080p: 25 px to 33):
the keycap's height, the room from the text before it to its body and from its body to the text after (the text
pieces' own spaces besides). Today's chip is 21 x 26 units (33 px tall drawn at "ce"'s 0.60 on 1080p): the keycap
30 px there (the approved mockups' CE case), 13 px from the words each side */
#define CE_KEY_UNITS 22.8f
#define CE_BEFORE_UNITS 6.2f
#define CE_AFTER_UNITS -0.5f
#define CE_TEXT_ICON 0.76f
/* HUD STYLE AE: the room before and after is 12 px (0.5 em) from the words' ink, less the text pieces' own spaces
(measured: "a a" less "aa") and the letters' side bearings at the 24 px em (measured on the box's 1080p shots, the
"Hold" before and the "to" after: 3 and 1 px) */
#define AE_SPACE_SAMPLE L"a a"
#define AE_SPACE_SAMPLE_BARE L"aa"
#define AE_BEARING_BEFORE_EM (3.0f / 24.0f)
#define AE_BEARING_AFTER_EM (1.0f / 24.0f)

static const real ice_rgb[3] = { 0xE8 / 255.0f, 0xF4 / 255.0f, 0xFF / 255.0f };
static const real ce_dark_rgb[3] = { 0x06 / 255.0f, 0x0E / 255.0f, 0x1E / 255.0f };

#define LOGGED_ICONS 40 /* hud_messaging.c's NUMBER_OF_ICON_TYPES */

static struct
{
	boolean noted;
	short icon_type;
	short button;
	real scale;       /* the chip's scale (CE's lines) */
	boolean text_icon; /* a custom line's (ui_widget.c), drawn from the small icons' sheet */
	/* (said once per icon and control until a setting changes: the keys the log's tests read) */
	unsigned long logged_at;
	boolean logged[LOGGED_ICONS][AE_KEY_CONTROL_COUNT];
} prompt;

void ae_key_prompts_note(short icon_type, short button, real scale)
{
	prompt.noted = TRUE;
	prompt.icon_type = icon_type;
	prompt.button = button;
	prompt.scale = scale > 0.0f ? scale : 1.0f;
	prompt.text_icon = FALSE;
}

void ae_key_prompts_note_text_icon(short icon_type, short button, real scale)
{
	ae_key_prompts_note(icon_type, button, scale);
	prompt.text_icon = TRUE;
}

/* a key a binding may name (xinput_sdl.c's own reading of the settings) */
static int names_key(const char *name)
{
	return halo_input_from_name(name) >= 0;
}

/* the keyboard's local player: its controller's (ae_seats.h) through the players' controllers (player_ui.c) */
static int keyboard_player(void)
{
	short controllers[MAXIMUM_NUMBER_OF_LOCAL_PLAYERS];
	short player;

	for (player = 0; player < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS; player++)
		controllers[player] = player_ui_get_single_player_local_player_controller(player);
	return ae_key_prompts_player_of_controller(ae_seats_keyboard_controller(), network_game_is_active(), controllers,
		MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);
}

/* the debug lines' "pad" (debug.ae_message_test): the pad's chips whatever was last used, for the comparison */
static boolean forced_pad(void)
{
	char const *test = config_string("debug.ae_message_test");
	char const *walk;

	for (walk = test; walk && *walk; walk = strchr(walk, ','), walk = walk ? walk + 1 : NULL)
		if (strcspn(walk, ",") == 3 && !strncmp(walk, "pad", 3))
			return TRUE;
	return FALSE;
}

/* the key the noted picture stands for: its name as shown (the keyboard's layout's), FALSE for none */
static boolean noted_key(short local_player_index, char *key, size_t size)
{
	struct game_input_preferences preferences;
	char binding[KEY_NAME_MAXIMUM];
	char const *setting;
	char const *list;
	int control;

	if (local_player_index < 0 || local_player_index >= MAXIMUM_LOCAL_PLAYERS)
		return FALSE;
	input_abstraction_get_local_player_preferences(local_player_index, &preferences);
	control = ae_key_prompts_control_of_icon(prompt.icon_type, prompt.button, preferences.game_control_to_xbox_buttons);
	setting = ae_key_prompts_control_setting(control);
	list = setting ? config_string(setting) : NULL;
	if (!ae_key_prompts_first_binding(list, names_key, binding, sizeof(binding)))
		return FALSE;
	halo_input_shown_name(binding, key, size);
	if (!key[0])
		return FALSE;
	if (prompt.logged_at != config_changes())
	{
		prompt.logged_at = config_changes();
		csmemset(prompt.logged, 0, sizeof(prompt.logged));
	}
	if (prompt.icon_type >= 0 && prompt.icon_type < LOGGED_ICONS && !prompt.logged[prompt.icon_type][control])
	{
		prompt.logged[prompt.icon_type][control] = TRUE;
		platform_log("key prompts: icon %d (button %d) is %s's \"%s\"", (int)prompt.icon_type, (int)prompt.button,
			setting, key);
	}
	return TRUE;
}

/* a textured quad in the HUD's pass: HUD units, the sheet's texels */
static void quad(struct bitmap_data const *sheet, enum ae_key_style style, real u0, real v0, real u1, real v1,
	real x0, real y0, real x1, real y1, real alpha)
{
	struct dynamic_screen_vertex vertices[4];
	struct rasterizer_dynamic_screen_geometry_parameters parameters;
	real_argb_color white;
	real sheet_width, sheet_height;
	pixel32 color;
	short i;

	if (x1 <= x0 || y1 <= y0)
		return;
	ae_key_prompts_sheet_size(style, &sheet_width, &sheet_height);
	white.alpha = alpha < 0.0f ? 0.0f : alpha > 1.0f ? 1.0f : alpha;
	white.red = white.green = white.blue = 1.0f;
	color = real_argb_color_to_pixel32(&white);
	/* (corners clockwise from the top-left, as hud_draw.c's sprites) */
	for (i = 0; i < 4; i++)
	{
		boolean right = (boolean)(((i + 1) & 2) != 0), bottom = (boolean)(i > 1);

		vertices[i].position.x = right ? x1 : x0;
		vertices[i].position.y = bottom ? y1 : y0;
		vertices[i].texture_coordinates.x = (right ? u1 : u0) / sheet_width;
		vertices[i].texture_coordinates.y = (bottom ? v1 : v0) / sheet_height;
		vertices[i].color = color;
	}
	csmemset(&parameters, 0, sizeof(parameters));
	parameters.map_texture_scale[0].i = 1.0f;
	parameters.map_texture_scale[0].j = 1.0f;
	parameters.map_scale[0].i = 1.0f;
	parameters.map_scale[0].j = 1.0f;
	parameters.framebuffer_blend_function = AE_BLEND_ALPHA;
	parameters.map[0] = (struct bitmap_data *)sheet;
	rasterizer_psuedo_dynamic_screen_quad_draw(&parameters, vertices);
}

/* a cell's picture over its body (left, top, width, height: units), margin the room round it */
static void draw_cell(struct bitmap_data const *sheet, enum ae_key_style style, int cell, real x, real y,
	real width, real height, real margin, real alpha)
{
	const struct ae_key_cell *entry = ae_key_prompts_cell(cell);
	struct ae_key_rect const *rect = &entry->cells[style];

	quad(sheet, style, rect->x, rect->y, (real)(rect->x + rect->w), (real)(rect->y + rect->h), x - margin, y - margin,
		x + width + margin, y + height + margin, alpha);
}

/* the plain keycap stretched to width (units): its rounded ends kept, its middle drawn longer */
static void draw_frame(struct bitmap_data const *sheet, enum ae_key_style style, real x, real y, real width,
	real height, real margin, real alpha)
{
	const struct ae_key_cell *entry = ae_key_prompts_cell(ae_key_prompts_frame_cell());
	struct ae_key_rect const *rect = &entry->cells[style];
	/* (the cell's texels a unit, and its ends: half the body's height and the margin) */
	real texels = (real)rect->h / (height + 2.0f * margin);
	real end = height / 2.0f + margin;
	real end_texels = end * texels;
	real left = x - margin, right = x + width + margin;

	quad(sheet, style, rect->x, rect->y, rect->x + end_texels, (real)(rect->y + rect->h), left, y - margin,
		left + end, y + height + margin, alpha);
	quad(sheet, style, rect->x + end_texels, rect->y, rect->x + rect->w - end_texels, (real)(rect->y + rect->h),
		left + end, y - margin, right - end, y + height + margin, alpha);
	quad(sheet, style, rect->x + rect->w - end_texels, rect->y, (real)(rect->x + rect->w), (real)(rect->y + rect->h),
		right - end, y - margin, right, y + height + margin, alpha);
}

static wchar_t const *wide_name(char const *text, wchar_t *wide, size_t size)
{
	size_t i;

	for (i = 0; i + 1 < size && text[i]; i++)
		wide[i] = (wchar_t)(unsigned char)text[i];
	wide[i] = 0;
	return wide;
}

/* a plain keycap's name: its size (em, px) for a keycap height in px */
static real frame_text_px(real key_px)
{
	return AE_KEY_PROMPT_FRAME_TEXT * key_px / AE_HUD_TEXT_CAP;
}

/* the name on a plain keycap, centred on its body (screen px; HUD STYLE AE's text) */
static void frame_text(short view, wchar_t const *name, real x, real y, real width, real height, real key_px,
	real const rgb[3], real alpha)
{
	real px = frame_text_px(key_px);
	real text_width = ae_hud_text_measure(name, px);
	real_argb_color color;

	color.alpha = alpha;
	color.red = rgb[0];
	color.green = rgb[1];
	color.blue = rgb[2];
	ae_hud_text_draw_ex(view, x + (width - text_width) / 2.0f, y + height / 2.0f + AE_HUD_TEXT_CAP * px / 2.0f, name,
		px, &color, _ae_text_plain);
}

/* (the debug lines' log: where a keycap went, when that changes) */
static void log_place(char const *key, real px, real space, real before, real after, real x, real top, real width,
	real height)
{
	static real last[4];

	if (!config_string("debug.ae_message_test")[0] || (last[0] == x && last[1] == top && last[2] == width &&
		last[3] == px))
	{
		return;
	}
	last[0] = x;
	last[1] = top;
	last[2] = width;
	last[3] = px;
	platform_log("key prompts: \"%s\" at %.1f,%.1f %.1fx%.1f px=%.1f space=%.2f before=%.2f after=%.2f", key, x, top,
		width, height, px, space, before, after);
}

/* HUD STYLE AE: the keycap on the block's pen */
static boolean draw_ae(char const *key)
{
	struct bitmap_data const *sheet = ae_buttons_own_bitmap("ae\\key_prompts\\ae");
	int cell = ae_key_prompts_cell_of(key);
	real ppu = ae_hud_pixels_per_unit();
	real px, cap, alpha, key_px, width, height, margin, space, before, after, x, top;
	short view;
	wchar_t name[KEY_NAME_MAXIMUM];
	char text[KEY_NAME_MAXIMUM];

	if (!sheet || ppu <= 0.0f || !ae_hud_message_metrics(&view, &px, &cap, &alpha))
		return FALSE;
	key_px = AE_KEY_PROMPT_HEIGHT_EM * px;
	/* (12 px from the words' ink each side: the text pieces' spaces count toward it) */
	space = ae_hud_text_measure(AE_SPACE_SAMPLE, px) - ae_hud_text_measure(AE_SPACE_SAMPLE_BARE, px);
	before = (AE_KEY_PROMPT_GAP_EM - AE_BEARING_BEFORE_EM) * px - space;
	after = (AE_KEY_PROMPT_GAP_EM - AE_BEARING_AFTER_EM) * px - space;
	if (before < 0.0f)
		before = 0.0f;
	if (after < 0.0f)
		after = 0.0f;
	if (cell >= 0)
	{
		ae_key_prompts_picture_size(cell, AE_KEY_STYLE_AE, key_px, &width, &height, &margin);
		width -= 2.0f * margin;
		height -= 2.0f * margin;
		ae_hud_message_place(width, height, before, after, &x, &top);
		/* (on whole pixels: the keycap's thin rim stays crisp) */
		x = (real)floor(x + 0.5f);
		top = (real)floor(top + 0.5f);
		log_place(key, px, space, before, after, x, top, width, height);
		draw_cell(sheet, AE_KEY_STYLE_AE, cell, x / ppu, top / ppu, width / ppu, height / ppu, margin / ppu, alpha);
		return TRUE;
	}
	ae_key_prompts_fallback_text(key, text, sizeof(text));
	wide_name(text, name, NUMBEROF(name));
	ae_key_prompts_picture_size(ae_key_prompts_frame_cell(), AE_KEY_STYLE_AE, key_px, NULL, NULL, &margin);
	height = key_px;
	width = ae_key_prompts_frame_width(ae_hud_text_measure(name, frame_text_px(key_px)), key_px);
	ae_hud_message_place(width, height, before, after, &x, &top);
	x = (real)floor(x + 0.5f);
	top = (real)floor(top + 0.5f);
	log_place(key, px, space, before, after, x, top, width, height);
	draw_frame(sheet, AE_KEY_STYLE_AE, x / ppu, top / ppu, width / ppu, height / ppu, margin / ppu, alpha);
	frame_text(view, name, x, top, width, height, key_px, ice_rgb, alpha);
	return TRUE;
}

/* CE's lines: the blue keycap at CE's cursor (HUD units) */
static boolean draw_ce(char const *key, rectangle2d *cursor_bounds, pixel32 color)
{
	struct bitmap_data const *sheet = ae_buttons_own_bitmap("ae\\key_prompts\\ce");
	int cell = ae_key_prompts_cell_of(key);
	real viewport = prompt.scale * (prompt.text_icon ? CE_TEXT_ICON : 1.0f);
	real key_units = CE_KEY_UNITS * viewport;
	real alpha = (real)((color >> 24) & 0xFF) / 255.0f;
	real middle, width, height, margin, x, ppu = ae_hud_pixels_per_unit();

	if (!sheet || ppu <= 0.0f)
		return FALSE;
	middle = (real)draw_unicode_string_capital_middle(cursor_bounds, L"");
	x = (real)cursor_bounds->x0 + CE_BEFORE_UNITS * viewport;
	if (cell >= 0)
	{
		ae_key_prompts_picture_size(cell, AE_KEY_STYLE_CE, key_units, &width, &height, &margin);
		width -= 2.0f * margin;
		height -= 2.0f * margin;
		draw_cell(sheet, AE_KEY_STYLE_CE, cell, x, middle - height / 2.0f, width, height, margin, alpha);
	}
	else
	{
		/* (the name in the line's own font and size, as the words round it: its capitals are about 0.56 of the
		keycap, the spec's) */
		long font_index = draw_string_get_font();
		struct font_header *font = font_index != NONE ? font_definition_get(font_index) : NULL;
		wchar_t name[KEY_NAME_MAXIMUM];
		char text[KEY_NAME_MAXIMUM];
		rectangle2d bounds, text_bounds, end;
		real_argb_color saved_color, dark;
		real text_width;

		if (!font)
			return FALSE;
		ae_key_prompts_fallback_text(key, text, sizeof(text));
		wide_name(text, name, NUMBEROF(name));
		ae_key_prompts_picture_size(ae_key_prompts_frame_cell(), AE_KEY_STYLE_CE, key_units, NULL, NULL, &margin);
		draw_string_set_indents(0, 0);
		bounds = *cursor_bounds;
		bounds.x0 = 0;
		bounds.x1 = SHORT_MAX / 2;
		draw_unicode_string_compute_bounds(&bounds, name, &text_bounds, &end);
		text_width = (real)(end.x0 - bounds.x0 - font->leading_width);
		height = key_units;
		width = ae_key_prompts_frame_width(text_width, key_units);
		draw_frame(sheet, AE_KEY_STYLE_CE, x, middle - height / 2.0f, width, height, margin, alpha);
		draw_string_get_color(&saved_color);
		dark.alpha = alpha;
		dark.red = ce_dark_rgb[0];
		dark.green = ce_dark_rgb[1];
		dark.blue = ce_dark_rgb[2];
		draw_string_set_color(&dark);
		bounds = *cursor_bounds;
		bounds.x0 = (short)(x + (width - text_width) / 2.0f - (real)font->leading_width + 0.5f);
		bounds.x1 = (short)(bounds.x0 + text_width + 2 * font->leading_width + 8);
		rasterizer_draw_unicode_string(&bounds, NULL, NULL, 0, name);
		draw_string_set_color(&saved_color);
	}
	cursor_bounds->x0 = (short)(x + width + CE_AFTER_UNITS * viewport + 0.5f);
	return TRUE;
}

boolean ae_key_prompts_icon(rectangle2d *cursor_bounds, pixel32 color)
{
	short view = render.local_player_index;
	char key[KEY_NAME_MAXIMUM];
	boolean noted = prompt.noted;
	boolean shared;

	prompt.noted = FALSE;
	if (!noted || !cursor_bounds)
		return FALSE;
	/* (players sharing the machine: the keyboard's player always gets keys, a pad's input does not flip them) */
	shared = (boolean)(local_player_count() > 1 || ae_seats_active());
	{
		/* (the debug lines' log: who used what last, when that changes) */
		static int logged_last = -1;
		int last = ae_platform_keyboard_last_used();

		if (last != logged_last && config_string("debug.ae_message_test")[0])
		{
			logged_last = last;
			platform_log("key prompts: view %d, the keyboard's player %d, %s used last, %s", (int)view,
				keyboard_player(), last ? "the keyboard or mouse" : "a pad", shared ? "shared" : "alone");
		}
	}
	if (!ae_key_prompts_wanted(ae_platform_keyboard_last_used(), view, keyboard_player(), shared, forced_pad()))
		return FALSE;
	if (!noted_key(view, key, sizeof(key)))
		return FALSE;
	if (ae_hud_messages_open())
	{
		/* (AE's text measures and draws through CE's draw mode: CE's kept round it, as ae_hud_messages.c keeps it) */
		long saved_font = draw_string_get_font();
		real_argb_color saved_color;
		boolean drawn;

		draw_string_get_color(&saved_color);
		draw_string_set_indents(0, 0);
		drawn = draw_ae(key);
		draw_string_set_font(saved_font);
		draw_string_set_color(&saved_color);
		return drawn;
	}
	return draw_ce(key, cursor_bounds, color);
}

void ae_key_prompts_tags_loaded(void)
{
	/* (the sheet decoded now, not at the first prompt: no hitch then) */
	if (ae_platform_keyboard_last_used() || ae_seats_active())
		ae_buttons_own_preload(ae_hud_style_is_ae() ? "ae\\key_prompts\\ae" : "ae\\key_prompts\\ce");
}

void ae_key_prompts_score_text(long player_index, wchar_t *buffer, long buffer_size)
{
	static wchar_t const word[] = L"BACK";
	static wchar_t const icon[] = L"%back-button";
	long const word_length = (long)NUMBEROF(word) - 1;
	long const icon_length = (long)NUMBEROF(icon) - 1;
	long length, at;
	short local_player_index;

	if (player_index == NONE || !buffer || buffer_size <= 0)
		return;
	local_player_index = player_get(player_index)->local_player_index;
	if (local_player_index == NONE ||
		!ae_key_prompts_wanted(ae_platform_keyboard_last_used(), local_player_index, keyboard_player(),
			(boolean)(local_player_count() > 1 || ae_seats_active()), forced_pad()))
	{
		return;
	}
	for (length = 0; length < buffer_size && buffer[length]; length++)
		;
	if (length >= buffer_size)
		return;
	/* (the word on its own: "BACK", not part of a longer one) */
	for (at = 0; at + word_length <= length; at++)
	{
		if (!csmemcmp(buffer + at, word, word_length * sizeof(wchar_t)) &&
			(at == 0 || buffer[at - 1] == L' ' || buffer[at - 1] == L'"') &&
			(buffer[at + word_length] == 0 || buffer[at + word_length] == L' ' || buffer[at + word_length] == L'"'))
		{
			break;
		}
	}
	if (at + word_length > length || length - word_length + icon_length >= buffer_size)
		return;
	csmemmove(buffer + at + icon_length, buffer + at + word_length, (length - at - word_length + 1) * sizeof(wchar_t));
	csmemcpy(buffer + at, icon, icon_length * sizeof(wchar_t));
}
