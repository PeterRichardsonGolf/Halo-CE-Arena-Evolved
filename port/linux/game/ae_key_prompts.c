/*
AE_KEY_PROMPTS.C

The HUD's prompts on mouse + keyboard: the bound key in place of the pad's
button (ae_key_prompts.h).
*/

#include <math.h>
#include <string.h>

#include "cseries/cseries.h"
#include "math/real_math.h"
#include "bitmaps/bitmap_group.h"
#include "bitmaps/bitmaps.h"
#include "bitmaps/bitmap_color_conversion.h"
#include "cache/texture_cache.h"
#include "memory/data.h"
#include "game/players.h"
#include "input/input_abstraction.h"
#include "interface/hud.h"
#include "interface/hud_definitions.h"
#include "interface/hud_draw.h"
#include "interface/player_ui.h"
#include "networking/network_game_globals.h"
#include "rasterizer/rasterizer.h"
#include "render/render.h"
#include "text/draw_string.h"
#include "text/font_group.h"

#include "../src/controller_layout.h"
#include "ae_buttons_tags.h"
#include "ae_hud_digits.h"
#include "ae_hud_messages.h"
#include "ae_hud_style.h"
#include "ae_hud_text.h"
#include "ae_key_prompts.h"
#include "ae_key_prompts_rules.h"
#include "ae_waypoint_ink.h"

/* the platform's (port/linux/src: ae_platform.c, ae_seats.c, xinput_sdl.c; the server's stubs) and port_config.c's */
int ae_platform_keyboard_last_used(void);
int ae_seats_active(void);
int ae_seats_keyboard_controller(void);
int halo_input_from_name(const char *name);
void halo_input_shown_name(const char *binding, char *shown, size_t size);
const char *config_string(const char *name);
unsigned long config_changes(void);
void platform_log(char const *format, ...);
int platform_gamepad_describe(int index, char *name, size_t size);
/* (ui_widget.c's: the stick a look or move icon is, for a southpaw's swapped) and hud_hires_tags.c's sprite for a pad */
short remap_sticks_for_local_player(short icon, short local_player_index);
struct bitmap_data const *hud_hires_sprite_bitmap(struct bitmap_data const *bitmap, short sequence_index);

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
int ae_key_prompts_keyboard_player(void)
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
				ae_key_prompts_keyboard_player(), last ? "the keyboard or mouse" : "a pad", shared ? "shared" : "alone");
		}
	}
	if (!ae_key_prompts_wanted(ae_platform_keyboard_last_used(), view, ae_key_prompts_keyboard_player(), shared, forced_pad()))
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
		!ae_key_prompts_wanted(ae_platform_keyboard_last_used(), local_player_index, ae_key_prompts_keyboard_player(),
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

boolean mcc_cache_tags_loaded(void);
boolean game_engine_running(void);
int ae_buttons_playstation_now(void);
boolean hud_hires_button_draw(struct icon_hud_element_definition const *icon, rectangle2d *cursor, pixel32 color);
char const *hud_hires_button_why(void);
struct bitmap_data const *hud_hires_button_picture(struct icon_hud_element_definition const *icon, real_rectangle2d *clip);

boolean ae_mcc_prompt_button(struct icon_hud_element_definition const *icon, union rectangle2d *cursor_bounds, pixel32 color,
	boolean own_color)
{
	static int logged;
	boolean mcc = mcc_cache_tags_loaded(), running = game_engine_running(), drawn = FALSE;

	if (mcc && running && ae_hud_messages_open())
	{
		/* (AE's HUD messages: the picture flows in its line as a Custom Edition map's sheet sprite does) */
		real_rectangle2d clip;
		struct bitmap_data const *picture = hud_hires_button_picture(icon, &clip);

		if (picture)
			drawn = ae_hud_message_icon(picture, &clip, color, own_color);
	}
	else if (mcc && running)
		drawn = hud_hires_button_draw(icon, cursor_bounds, color);
	/* (debug, the first 4 a run: which way a prompt's button went) */
	if (logged < 4)
	{
		logged++;
		platform_log("ae key prompts: prompt button: mcc map %d, game %d, playstation %d: %s%s", mcc, running,
			ae_buttons_playstation_now(), drawn ? "AE's button" : "not AE's: ", drawn ? "" : hud_hires_button_why());
	}
	return drawn;
}

boolean ae_mcc_message_picture(struct bitmap_data const *bitmap, union real_rectangle2d const *clip, pixel32 color,
	boolean own_color)
{
	if (!mcc_cache_tags_loaded() || !game_engine_running() || !ae_hud_messages_open())
		return FALSE;
	return ae_hud_message_icon(bitmap, clip, color, own_color);
}

/* ---------- the chips of the wheel's legend and the mark prompt */

/* hud_messaging.c's icon types of the pad's left bumper (the white button) and of the look stick, and its icons'
text flag */
#define CHIP_ICON_WHITE_BUTTON 5
#define CHIP_ICON_LEFT_STICK 16
#define CHIP_ICON_LOOK 31
#define CHIP_ICON_USES_TEXT 1
/* the picture's dark halo: the HUD's pictures' own (ae_hud_messages.c), a keyline round the ink */
#define CHIP_HALO 1.5f
#define CHIP_HALO_ALPHA 0.50f

static pixel32 chip_premultiplied(real const rgb[3], real alpha)
{
	real_argb_color color;

	color.alpha = alpha < 0.0f ? 0.0f : alpha > 1.0f ? 1.0f : alpha;
	color.red = rgb[0] * color.alpha;
	color.green = rgb[1] * color.alpha;
	color.blue = rgb[2] * color.alpha;
	return real_argb_color_to_pixel32(&color);
}

static char const *chip_sheet_tag(enum ae_key_style style)
{
	return style == AE_KEY_STYLE_AE ? "ae\\key_prompts\\ae" : "ae\\key_prompts\\ce";
}

/* a plain keycap with a name on it, its body's left at x and its middle on the capitals' middle (whole pixels); draw
FALSE only measures */
static boolean chip_frame(short view, enum ae_key_style style, wchar_t const *name, real x, real middle, real height,
	real alpha, boolean draw, real *width)
{
	struct bitmap_data const *sheet = ae_buttons_own_bitmap(chip_sheet_tag(style));
	real ppu = ae_hud_pixels_per_unit();
	real margin, body, top;

	if (!sheet || ppu <= 0.0f)
		return FALSE;
	ae_key_prompts_picture_size(ae_key_prompts_frame_cell(), style, height, NULL, NULL, &margin);
	body = ae_key_prompts_frame_width(ae_hud_text_measure(name, frame_text_px(height)), height);
	if (draw)
	{
		x = (real)floor(x + 0.5f);
		top = (real)floor(middle - height / 2.0f + 0.5f);
		draw_frame(sheet, style, x / ppu, top / ppu, body / ppu, height / ppu, margin / ppu, alpha);
		frame_text(view, name, x, top, body, height, height, style == AE_KEY_STYLE_AE ? ice_rgb : ce_dark_rgb, alpha);
	}
	*width = body;
	return TRUE;
}

/* a key's cap (Kenney's glyph where the sheet has one, else a plain keycap with its name) */
static boolean chip_key(short view, enum ae_key_style style, char const *key, real x, real middle, real height,
	real alpha, boolean draw, real *width)
{
	struct bitmap_data const *sheet = ae_buttons_own_bitmap(chip_sheet_tag(style));
	int cell = ae_key_prompts_cell_of(key);
	real ppu = ae_hud_pixels_per_unit();
	real body_width, body_height, margin;
	wchar_t name[KEY_NAME_MAXIMUM];
	char text[KEY_NAME_MAXIMUM];

	if (!sheet || ppu <= 0.0f)
		return FALSE;
	if (cell < 0)
	{
		ae_key_prompts_fallback_text(key, text, sizeof(text));
		return chip_frame(view, style, wide_name(text, name, NUMBEROF(name)), x, middle, height, alpha, draw, width);
	}
	ae_key_prompts_picture_size(cell, style, height, &body_width, &body_height, &margin);
	body_width -= 2.0f * margin;
	body_height -= 2.0f * margin;
	if (draw)
	{
		x = (real)floor(x + 0.5f);
		draw_cell(sheet, style, cell, x / ppu, (real)floor(middle - body_height / 2.0f + 0.5f) / ppu,
			body_width / ppu, body_height / ppu, margin / ppu, alpha);
	}
	*width = body_width;
	return TRUE;
}

/* one icon of the map's message icons: the sheet's bitmap and the sprite's part of it, its sequence, and its height in the
A button's (the first icon's) */
static boolean chip_sprite(short icon_index, struct bitmap_data const **bitmap, real_rectangle2d const **clip,
	short *sequence, real *relative_height)
{
	struct bitmap_data const *button_bitmap = NULL;
	real_rectangle2d const *button_clip = NULL;
	struct icon_hud_element_definition const *icon;
	long group_index;

	if (!hud_globals || (group_index = hud_globals->messaging.messaging_icons.index) == NONE ||
		icon_index < 0 || icon_index >= hud_globals->messaging.button_icons.count)
	{
		return FALSE;
	}
	icon = TAG_BLOCK_GET_ELEMENT(&hud_globals->messaging.button_icons, icon_index, struct icon_hud_element_definition);
	if ((icon->flags & CHIP_ICON_USES_TEXT) || icon->sequence_index < 0)
		return FALSE;
	*sequence = icon->sequence_index;
	hud_retrieve_bitmap_and_bounding_rect(group_index, icon->sequence_index, 0, bitmap, clip);
	if (!*bitmap || !*clip || (*bitmap)->width <= 0 || (*bitmap)->height <= 0 ||
		!_texture_cache_bitmap_get_hardware_format((struct bitmap_data *)*bitmap, FALSE, TRUE))
	{
		return FALSE;
	}
	/* (the pictures keep their proportions to the A button's: a button is its height, a wider one more) */
	*relative_height = 1.0f;
	{
		struct icon_hud_element_definition const *first = TAG_BLOCK_GET_ELEMENT(&hud_globals->messaging.button_icons, 0,
			struct icon_hud_element_definition);

		hud_retrieve_bitmap_and_bounding_rect(group_index, first->sequence_index, 0, &button_bitmap, &button_clip);
		if (button_bitmap && button_clip && button_bitmap->height > 0 &&
			(button_clip->y1 - button_clip->y0) * (real)button_bitmap->height > 0.0f)
		{
			*relative_height = ((*clip)->y1 - (*clip)->y0) * (real)(*bitmap)->height /
				((button_clip->y1 - button_clip->y0) * (real)button_bitmap->height);
		}
	}
	return TRUE;
}

/* a pad's chip: the map's picture of the button (another pad kind's through AE's placeholder, as the HUD's own
button pictures), else the pad kind's own name for the button on a plain keycap */
static boolean chip_pad(short view, enum ae_key_style style, int action, real x, real middle, real height, real alpha,
	boolean draw, real *width)
{
	static real const white_rgb[3] = { 229.0f / 255.0f, 240.0f / 255.0f, 1.0f };
	static real const dark_rgb[3] = { 6.0f / 255.0f, 10.0f / 255.0f, 18.0f / 255.0f };
	static real const around[8][2] = { { 1.0f, 0.0f }, { -1.0f, 0.0f }, { 0.0f, 1.0f }, { 0.0f, -1.0f },
		{ 0.7071f, 0.7071f }, { -0.7071f, 0.7071f }, { 0.7071f, -0.7071f }, { -0.7071f, -0.7071f } };
	short icon_index = action == AE_CHIP_PICK ? remap_sticks_for_local_player(CHIP_ICON_LOOK, view) :
		CHIP_ICON_WHITE_BUTTON;
	struct bitmap_data const *bitmap = NULL, *sprite;
	real_rectangle2d const *clip = NULL;
	short sequence = 0, pass;
	real relative = 1.0f, texels_wide, texels_high, picture_height, picture_width, top, left;

	/* (the mark button: the white button's picture is a plain oval, which says nothing on this pad; its name does) */
	if (action != AE_CHIP_MARK && chip_sprite(icon_index, &bitmap, &clip, &sequence, &relative))
	{
		texels_wide = (clip->x1 - clip->x0) * (real)bitmap->width;
		texels_high = (clip->y1 - clip->y0) * (real)bitmap->height;
		if (texels_wide > 0.0f && texels_high > 0.0f)
		{
			picture_height = height * relative;
			picture_width = picture_height * texels_wide / texels_high;
			if (draw)
			{
				sprite = hud_hires_sprite_bitmap(bitmap, sequence);
				if (!sprite)
					sprite = bitmap;
				left = (real)floor(x + 0.5f);
				top = (real)floor(middle - picture_height / 2.0f + 0.5f);
				for (pass = 0; pass < 8; pass++)
				{
					ae_waypoint_sprite_draw_pixels(sprite, clip, left + around[pass][0] * CHIP_HALO,
						top + around[pass][1] * CHIP_HALO, left + picture_width + around[pass][0] * CHIP_HALO,
						top + picture_height + around[pass][1] * CHIP_HALO,
						chip_premultiplied(dark_rgb, CHIP_HALO_ALPHA * alpha));
				}
				ae_waypoint_sprite_draw_pixels(sprite, clip, left, top, left + picture_width, top + picture_height,
					chip_premultiplied(white_rgb, alpha));
			}
			*width = picture_width;
			return TRUE;
		}
	}
	{
		/* (the mark button, or a pad kind with no picture: its own name for the button, "LB" or "L1", in the keycap's frame) */
		char label[16];
		wchar_t name[16];
		int button = action == AE_CHIP_PICK ? (icon_index == CHIP_ICON_LEFT_STICK ? CONTROLLER_BUTTON_LEFT_STICK :
			CONTROLLER_BUTTON_RIGHT_STICK) : CONTROLLER_BUTTON_WHITE;

		strncpy(label, controller_layout_button_glyph(platform_gamepad_describe(-1, NULL, 0), button),
			sizeof(label) - 1);
		label[sizeof(label) - 1] = 0;
		return chip_frame(view, style, wide_name(label, name, NUMBEROF(name)), x, middle, height, alpha, draw, width);
	}
}

static boolean chip_place(short local_player_index, int action, real x, real capitals_middle, real height, real alpha,
	boolean draw, real *width)
{
	enum ae_key_style style = ae_hud_style_is_ae() ? AE_KEY_STYLE_AE : AE_KEY_STYLE_CE;
	boolean shared = (boolean)(local_player_count() > 1 || ae_seats_active());
	real drawn = 0.0f;
	boolean done;

	if (width)
		*width = 0.0f;
	if (local_player_index < 0 || local_player_index >= MAXIMUM_LOCAL_PLAYERS || height <= 0.0f ||
		(action != AE_CHIP_MARK && action != AE_CHIP_PICK))
	{
		return FALSE;
	}
	if (ae_key_prompts_wanted(ae_platform_keyboard_last_used(), local_player_index, ae_key_prompts_keyboard_player(),
		shared, forced_pad()))
	{
		/* (the keyboard's: the mark key's cap, B by default; the mouse picks, a plain keycap MOVE) */
		if (action == AE_CHIP_PICK)
			done = chip_frame(local_player_index, style, L"MOVE", x, capitals_middle, height, alpha, draw, &drawn);
		else
		{
			char key[KEY_NAME_MAXIMUM], shown[KEY_NAME_MAXIMUM];

			done = ae_key_prompts_first_binding(config_string("controls.mark"), names_key, key, sizeof(key)) != 0;
			if (done)
			{
				halo_input_shown_name(key, shown, sizeof(shown));
				done = (boolean)(shown[0] &&
					chip_key(local_player_index, style, shown, x, capitals_middle, height, alpha, draw, &drawn));
			}
		}
	}
	else
		done = chip_pad(local_player_index, style, action, x, capitals_middle, height, alpha, draw, &drawn);
	if (done && width)
		*width = drawn;
	return done;
}

boolean ae_key_prompts_draw_chip(short local_player_index, int action, real x, real capitals_middle, real height,
	real alpha, real *width)
{
	if (alpha <= 0.0f)
	{
		if (width)
			*width = 0.0f;
		return FALSE;
	}
	return chip_place(local_player_index, action, x, capitals_middle, height, alpha, TRUE, width);
}

boolean ae_key_prompts_chip_width(short local_player_index, int action, real height, real *width)
{
	return chip_place(local_player_index, action, 0.0f, 0.0f, height, 1.0f, FALSE, width);
}
