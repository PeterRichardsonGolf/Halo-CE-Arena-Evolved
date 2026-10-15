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
#include "tag_files/tag_groups.h"
#include "text/draw_string.h"
#include "units/unit_definitions.h"
#include "units/units.h"

#include "ae_hud_digits.h"
#include "ae_hud_message_flow.h"
#include "ae_hud_messages.h"
#include "ae_hud_text.h"
#include "ae_strip_layout.h"

const char *config_string(const char *name);
void platform_log(char const *format, ...);
long game_time_get(void);

#define AE_BLEND_ALPHA_MULTIPLY_ADD 7  /* the HUD's, as ae_panel_draw.c */
#define AE_WEAPON_HUD_INTERFACE_TAG 'wphi'
#define AE_VEHICLE_TAG 'vehi'
/* hud.c's hud_state_message_type: the debug lines below set the game's own messages */
#define AE_MESSAGE_ENTER_VEHICLE 6
#define AE_MESSAGE_SWAP_WEAPON 4

#define LINE_PITCH 18.0f      /* baseline to baseline, px x scale (at the strip's sub-line size) */
#define PICTURE_HEIGHT 1.25f  /* a button picture's height, of the text's capitals */
#define PICTURE_HEIGHT_WIDE 2.00f /* a wide picture's (a weapon's, twice as wide as tall or more): at the buttons'
                                     height it is a smudge (first pictures, 10-15) */
#define PICTURE_WIDE_ASPECT 2.0f
#define PICTURE_BIG 1.36f     /* the debug comparison's ("bigicons": buttons 1.7 x the capitals) */
#define PICTURE_GAP 0.12f     /* the room after a picture, of the text's size */
#define PICTURE_HALO_ALPHA 0.50f
#define TEXT_MAXIMUM 512

static const real white_rgb[3] = { 229.0f / 255.0f, 240.0f / 255.0f, 255.0f / 255.0f };
static const real dark_rgb[3] = { 6.0f / 255.0f, 10.0f / 255.0f, 18.0f / 255.0f };

static struct
{
	boolean open;
	short view;
	real scale;
	real px;   /* the text's size (its em) */
	real cap;  /* its capitals' height */
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

/* the game's own messages set for the pictures and the tests, as hud.c sets them: the state line of a vehicle in
reach ("vehicle"), of a weapon to swap for (the player's own: "swap"), or a long custom line with a button picture
("long"); a list line now and then: an ammunition pickup ("pickup") */
static void test_messages(short view)
{
	static long last_tick[MAXIMUM_LOCAL_PLAYERS] = { -1000, -1000, -1000, -1000 };
	long tick = game_time_get();

	if (test_has("long"))
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
	test_px = ae_strip_text_px(AE_STRIP_SUB_PX, scale, scale);
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
	pitch = LINE_PITCH * scale;
	if (pitch < block.px * LINE_PITCH / AE_STRIP_SUB_PX)
		pitch = block.px * LINE_PITCH / AE_STRIP_SUB_PX; /* (the quarter views' floor: lines as far apart as the
		                                                       text is big) */
	/* the wrap: no wider than twice the ammo group (owner 10-15: a block under the counter), never past the view's
	edge */
	right = view_width - left;
	if (group_width > 0.0f && left + 2.0f * group_width < right)
		right = left + 2.0f * group_width;
	if (right < left + 8.0f * block.px)
		right = left + 8.0f * block.px;
	ae_hud_message_flow_start(&block.flow, left, right, first_cap_top + block.cap, pitch);
	block.open = TRUE;

	if (config_string("debug.ae_message_test")[0] && tick >= 0 && tick / 60 != logged_tick[local_player_index])
	{
		logged_tick[local_player_index] = tick / 60;
		platform_log("ae messages: tick %ld view %d left=%.1f cap_top=%.1f right=%.1f px=%.1f pitch=%.1f "
			"texts=%ld pictures=%ld list=%ld", tick / 60 * 60, local_player_index, left, first_cap_top, right,
			block.px, pitch, block.texts, block.pictures, block.list_lines);
		block.texts = block.pictures = block.list_lines = 0;
	}
	return TRUE;
}

void ae_hud_messages_end(void)
{
	block.open = FALSE;
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
	long length = 0;
	wchar_t const *walk;
	real_argb_color color;
	wchar_t *at;

	/* (the text without carriage returns; tabs as spaces) */
	for (walk = source; *walk && length < TEXT_MAXIMUM - 1; walk++)
		if (*walk != L'\r')
			text[length++] = *walk == L'\t' ? L' ' : *walk;
	text[length] = 0;
	color.alpha = block.saved_color.alpha;
	color.red = white_rgb[0];
	color.green = white_rgb[1];
	color.blue = white_rgb[2];

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

boolean ae_hud_message_icon(struct bitmap_data const *bitmap, union real_rectangle2d const *clip,
	pixel32 color, boolean own_color)
{
	static const real around[8][2] = { { 1.0f, 0.0f }, { -1.0f, 0.0f }, { 0.0f, 1.0f }, { 0.0f, -1.0f },
		{ 0.7071f, 0.7071f }, { -0.7071f, 0.7071f }, { 0.7071f, -0.7071f }, { -0.7071f, -0.7071f } };
	real texture[4], texels_wide, texels_high, height, width, x, top, halo, alpha;
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
	height = block.cap * (texels_wide >= PICTURE_WIDE_ASPECT * texels_high ? PICTURE_HEIGHT_WIDE : PICTURE_HEIGHT);
	if (test_has("bigicons"))
		height *= PICTURE_BIG; /* (taller pictures, to compare) */
	width = height * texels_wide / texels_high;
	ae_hud_message_flow_picture(&block.flow, width, PICTURE_GAP * block.px, &x);
	top = block.flow.baseline - block.cap / 2.0f - height / 2.0f;

	/* (the line's alpha: CE's draw colour's, an objective's fade) */
	draw_string_get_color(&text_color);
	alpha = text_color.alpha;
	halo = 2.0f * (real)sqrt(block.px / 36.0f);
	for (pass = 0; pass < 8; pass++)
		picture_quad(bitmap, texture, x + around[pass][0] * halo, top + around[pass][1] * halo,
			x + width + around[pass][0] * halo, top + height + around[pass][1] * halo,
			premultiplied(dark_rgb, PICTURE_HALO_ALPHA * alpha));
	picture_quad(bitmap, texture, x, top, x + width, top + height,
		own_color ? color : premultiplied(white_rgb, alpha));
	block.pictures++;
	return TRUE;
}
