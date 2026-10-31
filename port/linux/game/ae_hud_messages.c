/*
AE_HUD_MESSAGES.C

HUD STYLE AE's CE message lines in the score strip's text style
(ae_hud_messages.h).
*/

#include <math.h>
#include <string.h>

#include "cseries/cseries.h"
#include "math/real_math.h"
#include "bitmaps/bitmaps.h"
#include "bitmaps/bitmap_group.h"
#include "bitmaps/bitmap_color_conversion.h"
#include "game/players.h"
#include "interface/hud.h"
#include "interface/hud_definitions.h"
#include "interface/hud_messaging.h"
#include "interface/weapon_hud_interface_definition.h"
#include "items/weapon_definitions.h"
#include "items/weapons.h"
#include "objects/object_definitions.h"
#include "objects/objects.h"
#include "rasterizer/rasterizer.h"
#include "render/render.h"
#include "scenario/scenario.h"
#include "scenario/scenario_definitions.h"
#include "tag_files/tag_groups.h"
#include "text/draw_string.h"
#include "units/unit_definitions.h"
#include "units/units.h"

#include "interface/hud_draw.h"

#include "ae_hud_digits.h"
#include "ae_hud_message_flow.h"
#include "ae_hud_messages.h"
#include "ae_hud_text.h"

const char *config_string(const char *name);
void platform_log(char const *format, ...);
long game_time_get(void);

#define AE_BLEND_ALPHA_MULTIPLY_ADD 7  /* the HUD's, as ae_panel_draw.c */
#define AE_WEAPON_HUD_INTERFACE_TAG 'wphi'
#define AE_VEHICLE_TAG 'vehi'
/* hud.c's hud_state_message_type: the debug lines below set the game's own messages */
#define AE_MESSAGE_ENTER_VEHICLE 6
#define AE_MESSAGE_SWAP_WEAPON 4
#define AE_HUD_FLASH_REVERSE_COLORS_BIT 0 /* hud_draw.c's _hud_flash_reverse_colors_bit */

/* (the sizes: ae_hud_message_flow.h, the lines' own since the campaign review of 2026-10-18; they had the score
strip's sub-line 14 px, 0.36 of stock's campaign text) */
#define PICTURE_BIG 1.36f     /* the debug comparison's ("bigicons": buttons 1.7 x the capitals) */
#define PICTURE_GAP 0.12f     /* the room after a picture, of the text's size */
#define PICTURE_HALO_ALPHA 0.50f
#define TEXT_MAXIMUM 512

static const real white_rgb[3] = { 229.0f / 255.0f, 240.0f / 255.0f, 255.0f / 255.0f };
static const real dark_rgb[3] = { 6.0f / 255.0f, 10.0f / 255.0f, 18.0f / 255.0f };
/* a flashing help line (the scripts' enable_hud_help_flash) goes from the white toward the PC menus' ice-blue focus
glow, never CE's own blue-grey or an amber */
static const real flash_rgb[3] = { 161.0f / 255.0f, 206.0f / 255.0f, 255.0f / 255.0f };

static struct
{
	boolean open;
	short view;
	real scale;
	real px;   /* the text's size (its em) */
	real cap;  /* its capitals' height */
	real flash; /* a flashing help line's phase: 0 the white, 1 the ice-blue (ae_hud_messages_colour) */
	boolean opaque; /* a help or state line: drawn at full alpha, not its colour's constant 0.5 */
	real picture_wide, picture_high, picture_px; /* the last picture's texels and drawn height (the log line) */
	real drawn_alpha, flash_most; /* CE's last line colour's alpha, the most flash since the last log line */
	long saved_font;
	real_argb_color saved_color;
	struct ae_hud_message_flow flow;
	long texts, pictures, list_lines; /* drawn since the last log line */
} block;

boolean ae_hud_messages_open(void)
{
	return block.open;
}

/* ---------- the debug lines (debug.ae_message_test) */

static boolean test_has(char const *word)
{
	char const *test = config_string("debug.ae_message_test");
	size_t length = strlen(word);
	char const *walk;

	if (!test || !test[0])
		return FALSE;
	for (walk = test; walk && *walk; walk = strchr(walk, ','), walk = walk ? walk + 1 : NULL)
		if (strcspn(walk, ",") == length && !strncmp(walk, word, length))
			return TRUE;
	return FALSE;
}

static long test_unit(short view)
{
	long player_index = local_player_get_player_index(view);

	if (player_index == NONE)
		return NONE;
	return player_get(player_index)->unit_index;
}

static long test_weapon_definition(short view)
{
	long unit_index = test_unit(view);
	long weapon_index;

	if (unit_index == NONE)
		return NONE;
	weapon_index = unit_inventory_get_weapon(unit_index, unit_get(unit_index)->unit.current_weapon_index);
	if (weapon_index == NONE)
		return NONE;
	return weapon_get(weapon_index)->definition_index;
}

/* the campaign's help text (hud_messaging.c's, its tag group 'hmt '): a message picked by a piece of its text, as the
scripts' hud_set_help_text picks it by name. (The tag's own layout, as hs.c keeps it: hud_messaging.c's structures are
its own) */
#define AE_HUD_MESSAGE_TEXT_TAG 'hmt '

struct ae_test_hud_message_text
{
	struct tag_data text_data;
	struct tag_block elements;
	struct tag_block messages;
};

struct ae_test_hud_message
{
	char name[32];
	word text_start_index;
	word element_start_index;
	byte element_count;
	byte pad25[3];
	long unused28[6];
};

struct ae_test_hud_message_element
{
	byte type; /* 0 text (data: its length), 1 an icon */
	byte data;
};

static short test_help_message(wchar_t const *piece)
{
	struct scenario *scenario = global_scenario_get();
	struct ae_test_hud_message_text *text;
	long piece_length = (long)wcslen(piece);
	short i;

	if (!scenario || scenario->hud_messages.index == NONE)
		return NONE;
	text = (struct ae_test_hud_message_text *)tag_get(AE_HUD_MESSAGE_TEXT_TAG, scenario->hud_messages.index);
	for (i = 0; i < (short)text->messages.count; i++)
	{
		struct ae_test_hud_message *message = TAG_BLOCK_GET_ELEMENT(&text->messages, i, struct ae_test_hud_message);
		long position = message->text_start_index;
		short e;

		for (e = 0; e < message->element_count; e++)
		{
			struct ae_test_hud_message_element *element = TAG_BLOCK_GET_ELEMENT(&text->elements,
				message->element_start_index + e, struct ae_test_hud_message_element);
			wchar_t const *run;
			long j;

			if (element->type != 0)
				continue;
			run = (wchar_t const *)tag_data_get_pointer(&text->text_data, position * (long)sizeof(wchar_t),
				element->data * (long)sizeof(wchar_t));
			for (j = 0; j + piece_length <= element->data; j++)
				if (!memcmp(run + j, piece, (size_t)piece_length * sizeof(wchar_t)))
					return i;
			position += element->data;
		}
	}
	return NONE;
}

/* the game's own messages set for the pictures and the tests, as hud.c sets them: the state line of a vehicle in
reach ("vehicle"), of a weapon to swap for (the player's own: "swap"), or a long custom line with a button picture
("long"); a list line now and then: an ammunition pickup ("pickup"). The campaign's help text with its line breaks
(a10's, "|n" in the tag's text): the look inversion question ("help_invert"), the shields' "Press A to continue"
("help_shield"), move and look ("help_move"); a custom line with the same breaks in any game ("breaks") */
static void test_messages(short view)
{
	static long last_tick[MAXIMUM_LOCAL_PLAYERS] = { -1000, -1000, -1000, -1000 };
	long tick = game_time_get();
	wchar_t const *help = test_has("help_invert") ? L"inverted|n|nPress " :
		test_has("help_shield") ? L"Charged|n|nPress " : test_has("help_move") ? L" to move|n|nUse " : NULL;

	if (help)
	{
		short message = test_help_message(help);

		if (message != NONE)
		{
			scripted_show_hud_help_text(TRUE);
			scripted_hud_set_state_message(message);
		}
	}
	else if (test_has("breaks"))
	{
		hud_enable_custom_state_message(view, TRUE);
		hud_set_state_text(view, L"Vertical looking is now inverted|n|nPress %a-button to try it the other way|n|n"
			L"Press %b-button to leave it like it is");
	}
	else if (test_has("long"))
	{
		/* (a custom line, as the game engine's "Hold BACK for score": its %icons go through
		draw_string_and_hack_in_icons) */
		hud_enable_custom_state_message(view, TRUE);
		hud_set_state_text(view, L"Hold %x-button to see where a long line breaks: a custom prompt with a button "
			L"picture, long enough to reach past the ammo group and on toward the far side of the view");
	}
	else if (test_has("vehicle"))
	{
		static char const *const names[] = { "vehicles\\scorpion\\scorpion_mp", "vehicles\\warthog\\mp_warthog",
			"vehicles\\ghost\\ghost_mp", "vehicles\\scorpion\\scorpion", "vehicles\\warthog\\warthog" };
		long vehicle = NONE;
		short i;

		for (i = 0; i < (short)NUMBEROF(names) && vehicle == NONE; i++)
			vehicle = tag_loaded(AE_VEHICLE_TAG, names[i]);
		if (vehicle != NONE && unit_definition_get(vehicle)->unit.seats.count > 0)
		{
			struct unit_seat *seat = TAG_BLOCK_GET_ELEMENT(&unit_definition_get(vehicle)->unit.seats, 0,
				struct unit_seat);

			hud_set_state_message(view, AE_MESSAGE_ENTER_VEHICLE);
			hud_set_state_message_text(view, 0, seat->icon_text_index, FALSE);
			hud_set_state_message_text(view, 1, object_definition_get(vehicle)->object.icon_text_index, FALSE);
		}
	}
	else if (test_has("swap"))
	{
		long weapon = test_weapon_definition(view);
		long hud_index = weapon == NONE ? NONE :
			weapon_definition_get(weapon)->weapon.interface_definition.hud_interface.index;

		if (hud_index != NONE)
		{
			struct weapon_hud_interface_definition *hud =
				(struct weapon_hud_interface_definition *)tag_get(AE_WEAPON_HUD_INTERFACE_TAG, hud_index);

			if (hud->messaging_icon.sequence_index != NONE)
			{
				hud_set_state_message(view, AE_MESSAGE_SWAP_WEAPON);
				hud_set_state_message_icon(view, 0, &hud->messaging_icon);
			}
		}
	}
	if (tick >= 0 && (tick < last_tick[view] || tick - last_tick[view] >= 4 * 30))
	{
		last_tick[view] = tick;
		if (test_has("pickup"))
		{
			long weapon = test_weapon_definition(view);

			if (weapon != NONE)
				hud_picked_up_ammunition(view, weapon, 60);
		}
	}
}

/* ---------- the block */

boolean ae_hud_messages_begin(short local_player_index, real left, real first_cap_top, real view_width,
	real group_width, real scale)
{
	static long logged_tick[MAXIMUM_LOCAL_PLAYERS] = { -1, -1, -1, -1 };
	real right, pitch, test_px;
	long tick = game_time_get();

	block.open = FALSE;
	if (local_player_index < 0 || local_player_index >= MAXIMUM_LOCAL_PLAYERS || scale <= 0.0f)
		return FALSE;
	/* (no font, no AE text: CE's lines as before) */
	test_px = ae_hud_message_px(scale);
	if (ae_hud_text_measure(L"H", test_px) <= 0.0f)
		return FALSE;
	test_messages(local_player_index);
	/* ("ce": the lines as before the restyle, CE's own at 0.60, for the before-and-after pictures) */
	if (test_has("ce"))
		return FALSE;

	block.view = local_player_index;
	block.scale = scale;
	block.px = test_px;
	block.cap = AE_HUD_TEXT_CAP * block.px;
	block.flash = 0.0f;
	block.opaque = FALSE;
	pitch = ae_hud_message_pitch(block.px);
	/* the wrap: no wider than twice the ammo group (owner 10-15: a block under the counter); with no group (the
	campaign's start, no weapon yet) 26 em, not the whole view (campaign review F3) */
	right = ae_hud_message_wrap_right(left, view_width, group_width, block.px, scale);
	ae_hud_message_flow_start(&block.flow, left, right, first_cap_top + block.cap, pitch);
	block.open = TRUE;

	if (config_string("debug.ae_message_test")[0] && tick >= 0 && tick / 60 != logged_tick[local_player_index])
	{
		logged_tick[local_player_index] = tick / 60;
		platform_log("ae messages: tick %ld view %d left=%.1f cap_top=%.1f right=%.1f px=%.1f pitch=%.1f "
			"texts=%ld pictures=%ld list=%ld picture=%.0fx%.0f@%.1f alpha=%.2f flash=%.2f", tick / 60 * 60,
			local_player_index, left, first_cap_top, right, block.px, pitch, block.texts, block.pictures,
			block.list_lines, block.picture_wide, block.picture_high, block.picture_px, block.drawn_alpha,
			block.flash_most);
		block.texts = block.pictures = block.list_lines = 0;
		block.flash_most = 0.0f;
	}
	return TRUE;
}

void ae_hud_messages_end(void)
{
	block.open = FALSE;
}

void ae_hud_messages_colour(boolean objective, boolean help, boolean flashing,
	struct hud_color_definition const *help_color, union real_argb_color const *drawn)
{
	real_argb_color base, flash;
	real axis[3], length = 0.0f, along = 0.0f, t;
	short i;

	block.flash = 0.0f;
	block.opaque = FALSE;
	if (drawn)
		block.drawn_alpha = drawn->alpha;
	if (!block.open || objective)
		return;
	/* (a help or state line's colour has a constant alpha, 0.5 in the stock maps, not a fade: AE's white at it was a
	grey (117,123,134) on a10's cryo bay. An objective's alpha is its fade and stays, as the list lines' do) */
	block.opaque = TRUE;
	if (!help || !flashing || !help_color || !drawn)
		return;
	/* (get_flash_color mixes the colour toward its flash colour by the phase: read the phase back from the colour CE
	drew, so AE's white mixes toward the ice-blue in the same beat; reversed colours start at the flash colour) */
	pixel32_to_real_argb_color(help_color->color, &base);
	pixel32_to_real_argb_color(help_color->flash_color, &flash);
	axis[0] = flash.red - base.red;
	axis[1] = flash.green - base.green;
	axis[2] = flash.blue - base.blue;
	for (i = 0; i < 3; i++)
		length += axis[i] * axis[i];
	if (length <= 0.0001f)
		return;
	along = (drawn->red - base.red) * axis[0] + (drawn->green - base.green) * axis[1] +
		(drawn->blue - base.blue) * axis[2];
	t = along / length;
	t = t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t;
	if (TEST_FLAG(help_color->flash_flags, AE_HUD_FLASH_REVERSE_COLORS_BIT))
		t = 1.0f - t;
	block.flash = t;
	if (t > block.flash_most)
		block.flash_most = t;
}

/* the text's colour: AE's white, or toward the ice-blue while a help line flashes */
static void text_rgb(real rgb[3])
{
	short i;

	for (i = 0; i < 3; i++)
		rgb[i] = white_rgb[i] + (flash_rgb[i] - white_rgb[i]) * block.flash;
}

/* CE's draw mode kept round AE's text (its font and colour; AE's text sets its own, with no indents) */
static void save_mode(void)
{
	block.saved_font = draw_string_get_font();
	draw_string_get_color(&block.saved_color);
	draw_string_set_indents(0, 0);
}

static void restore_mode(void)
{
	draw_string_set_font(block.saved_font);
	draw_string_set_color(&block.saved_color);
}

static real measure_run(const wchar_t *text, long count, void *context)
{
	wchar_t run[TEXT_MAXIMUM];

	(void)context;
	if (count <= 0)
		return 0.0f;
	if (count > TEXT_MAXIMUM - 1)
		count = TEXT_MAXIMUM - 1;
	memcpy(run, text, (size_t)count * sizeof(wchar_t));
	run[count] = 0;
	return ae_hud_text_measure(run, block.px);
}

/* a piece of text on the pen, broken between words where it reaches the wrap */
static void flow_text(wchar_t const *source)
{
	wchar_t text[TEXT_MAXIMUM];
	long length;
	real_argb_color color;
	real rgb[3];
	wchar_t *at;

	/* (the text with CE's line ends as '\n': the campaign's help text breaks its lines with "|n". AE's text measured
	such a piece as 0 px (a10's "Energy Shields Charged|n|nPress ") and drew only its first line, so the button and the
	next piece started back at the line's left, over the first words) */
	length = ae_hud_message_flow_clean(source, text, TEXT_MAXIMUM);
	text_rgb(rgb);
	color.alpha = block.opaque ? 1.0f : block.saved_color.alpha;
	color.red = rgb[0];
	color.green = rgb[1];
	color.blue = rgb[2];

	at = text;
	while (length > 0)
	{
		long skip, count;

		if (*at == L'\n')
		{
			ae_hud_message_flow_new_line(&block.flow);
			at++;
			length--;
			continue;
		}
		ae_hud_message_flow_line(&block.flow);
		count = ae_hud_message_flow_fit(&block.flow, at, length, measure_run, NULL, &skip);
		at += skip;
		length -= skip;
		if (count == 0)
		{
			if (length > 0 && *at != L'\n')
				ae_hud_message_flow_new_line(&block.flow);
			continue;
		}
		{
			wchar_t saved = at[count];

			at[count] = 0;
			ae_hud_text_draw_ex(block.view, block.flow.x, block.flow.baseline, at, block.px, &color,
				_ae_text_outline);
			block.flow.x += ae_hud_text_measure(at, block.px);
			at[count] = saved;
		}
		block.flow.line_empty = FALSE;
		at += count;
		length -= count;
		if (length > 0 && *at != L'\n')
			ae_hud_message_flow_new_line(&block.flow);
	}
}

boolean ae_hud_message_text(wchar_t const *text)
{
	if (!block.open)
		return FALSE;
	if (!text || !text[0])
		return TRUE;
	save_mode();
	flow_text(text);
	restore_mode();
	block.texts++;
	return TRUE;
}

boolean ae_hud_message_list_line(wchar_t const *text)
{
	if (!block.open)
		return FALSE;
	/* (the list's lines after the help line keep their own fade and AE's white) */
	block.opaque = FALSE;
	block.flash = 0.0f;
	ae_hud_message_flow_new_line(&block.flow);
	if (text && text[0])
	{
		save_mode();
		flow_text(text);
		restore_mode();
	}
	block.list_lines++;
	return TRUE;
}

void ae_hud_messages_keep_first_line(void)
{
	if (block.open)
		ae_hud_message_flow_line(&block.flow);
}

/* ---------- pictures */

boolean ae_hud_message_metrics(short *view, real *px, real *cap, real *alpha)
{
	real_argb_color text_color;

	if (!block.open)
		return FALSE;
	draw_string_get_color(&text_color);
	*view = block.view;
	*px = block.px;
	*cap = block.cap;
	*alpha = block.opaque ? 1.0f : text_color.alpha;
	return TRUE;
}

void ae_hud_message_place(real width, real height, real before, real after, real *x, real *top)
{
	ae_hud_message_flow_line(&block.flow);
	if (block.flow.line_empty)
		ae_hud_message_flow_picture(&block.flow, width, after, x);
	else if (ae_hud_message_flow_picture(&block.flow, before + width, after, x))
		block.flow.x -= before; /* (sent to the next line: it starts it, no room before) */
	else
		*x += before;
	*top = block.flow.baseline - block.cap / 2.0f - height / 2.0f;
	block.pictures++;
}

static void picture_quad(struct bitmap_data const *bitmap, real const texture[4], real x0, real y0, real x1, real y1,
	pixel32 color)
{
	struct dynamic_screen_vertex vertices[4];
	struct rasterizer_dynamic_screen_geometry_parameters parameters;
	real units = 1.0f / ae_hud_pixels_per_unit();
	short i;

	/* (corners clockwise from the top-left, as hud_draw.c's sprites) */
	for (i = 0; i < 4; i++)
	{
		boolean right = (boolean)(((i + 1) & 2) != 0), bottom = (boolean)(i > 1);

		vertices[i].position.x = (right ? x1 : x0) * units;
		vertices[i].position.y = (bottom ? y1 : y0) * units;
		vertices[i].texture_coordinates.x = right ? texture[2] : texture[0];
		vertices[i].texture_coordinates.y = bottom ? texture[3] : texture[1];
		vertices[i].color = color;
	}
	csmemset(&parameters, 0, sizeof(parameters));
	parameters.map_texture_scale[0].i = 1.0f;
	parameters.map_texture_scale[0].j = 1.0f;
	parameters.map_scale[0].i = 1.0f;
	parameters.map_scale[0].j = 1.0f;
	parameters.framebuffer_blend_function = AE_BLEND_ALPHA_MULTIPLY_ADD;
	parameters.map[0] = (struct bitmap_data *)bitmap;
	rasterizer_psuedo_dynamic_screen_quad_draw(&parameters, vertices);
}

/* a colour premultiplied by its alpha (the HUD's blend adds the colour over 1 - alpha of what is under it) */
static pixel32 premultiplied(real const rgb[3], real alpha)
{
	real_argb_color color;

	color.alpha = alpha < 0.0f ? 0.0f : alpha > 1.0f ? 1.0f : alpha;
	color.red = rgb[0] * color.alpha;
	color.green = rgb[1] * color.alpha;
	color.blue = rgb[2] * color.alpha;
	return real_argb_color_to_pixel32(&color);
}

/* the A button's height in texels when bitmap is a page of the HUD's message icons sheet, else 0: CE's pictures keep
their proportions to it (ae_hud_message_picture_height). (A page, or a stand-in of one: hud_hires_tags.c's and the
PlayStation pictures' placeholders are new bitmaps of the page's size, so a page is known by its size too) */
static real sheet_button_texels(struct bitmap_data const *bitmap)
{
	long group_index = hud_globals ? hud_globals->messaging.messaging_icons.index : NONE;
	struct bitmap_group *group;
	struct bitmap_data const *button = NULL;
	real_rectangle2d const *clip = NULL;
	boolean member = FALSE;
	long i;

	if (group_index == NONE || !bitmap)
		return 0.0f;
	group = bitmap_group_get(group_index);
	for (i = 0; i < group->bitmaps.count && !member; i++)
	{
		struct bitmap_data const *page = TAG_BLOCK_GET_ELEMENT(&group->bitmaps, i, struct bitmap_data);

		member = (boolean)(page == bitmap || (page->width == bitmap->width && page->height == bitmap->height));
	}
	if (!member)
		return 0.0f;
	hud_retrieve_bitmap_and_bounding_rect(group_index, 0, 0, &button, &clip);
	if (!button || !clip || button->height <= 0)
		return 0.0f;
	return (clip->y1 - clip->y0) * (real)button->height;
}

boolean ae_hud_message_icon(struct bitmap_data const *bitmap, union real_rectangle2d const *clip,
	pixel32 color, boolean own_color)
{
	static const real around[8][2] = { { 1.0f, 0.0f }, { -1.0f, 0.0f }, { 0.0f, 1.0f }, { 0.0f, -1.0f },
		{ 0.7071f, 0.7071f }, { -0.7071f, 0.7071f }, { 0.7071f, -0.7071f }, { -0.7071f, -0.7071f } };
	real texture[4], texels_wide, texels_high, height, width, x, top, halo, alpha, rgb[3];
	real_argb_color text_color;
	short pass;

	if (!block.open)
		return FALSE;
	if (!bitmap || bitmap->width <= 0 || bitmap->height <= 0)
		return TRUE;
	/* (CE's own sizes: hud_draw.c's hud_calculate_bitmap_bounds of a sprite that is not an interface bitmap) */
	texture[0] = clip ? clip->x0 : 0.0f;
	texture[1] = clip ? clip->y0 : 0.0f;
	texture[2] = clip ? clip->x1 : 1.0f;
	texture[3] = clip ? clip->y1 : 1.0f;
	texels_wide = (texture[2] - texture[0]) * (real)bitmap->width;
	texels_high = (texture[3] - texture[1]) * (real)bitmap->height;
	if (texels_wide <= 0.0f || texels_high <= 0.0f)
		return TRUE;
	block.picture_wide = texels_wide;
	block.picture_high = texels_high;
	height = ae_hud_message_picture_height(block.cap, texels_wide, texels_high, sheet_button_texels(bitmap));
	if (test_has("bigicons"))
		height *= PICTURE_BIG; /* (taller pictures, to compare) */
	block.picture_px = height;
	width = height * texels_wide / texels_high;
	ae_hud_message_flow_picture(&block.flow, width, PICTURE_GAP * block.px, &x);
	top = block.flow.baseline - block.cap / 2.0f - height / 2.0f;

	/* (the line's alpha: CE's draw colour's, an objective's fade) */
	draw_string_get_color(&text_color);
	alpha = block.opaque ? 1.0f : text_color.alpha;
	halo = 2.0f * (real)sqrt(block.px / 36.0f);
	for (pass = 0; pass < 8; pass++)
		picture_quad(bitmap, texture, x + around[pass][0] * halo, top + around[pass][1] * halo,
			x + width + around[pass][0] * halo, top + height + around[pass][1] * halo,
			premultiplied(dark_rgb, PICTURE_HALO_ALPHA * alpha));
	text_rgb(rgb);
	picture_quad(bitmap, texture, x, top, x + width, top + height,
		own_color ? color : premultiplied(rgb, alpha));
	block.pictures++;
	return TRUE;
}
