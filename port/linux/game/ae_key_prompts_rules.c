/*
AE_KEY_PROMPTS_RULES.C

The HUD's key prompts on mouse + keyboard: the decisions
(ae_key_prompts_rules.h).
*/

#include <string.h>

#include "ae_key_prompts_rules.h"

#include "ae_key_sheet.inc"

#define NUMBEROF_CELLS ((int)(sizeof(ae_key_cells) / sizeof(ae_key_cells[0])))
#define NUMBEROF_NAMES ((int)(sizeof(ae_key_names) / sizeof(ae_key_names[0])))

/* hud_messaging.c's button_mappings for its actions (action, throw grenade, primary trigger, flashlight, jump, use
equipment, rotate weapons, rotate grenades, crouch, zoom): the game control each names (input_abstraction.c's order).
(CE's own table: its "crouch" names control 11, the zoom, and its "zoom" control 10; the pad's chip shows those, and
so does the key) */
static const signed char action_controls[AE_KEY_ICON_ZOOM - AE_KEY_ICON_ACTION + 1] = { 2, 6, 7, 5, 0, 4, 3, 1, 11, 10 };

/* the controls' keyboard settings (port_config.c), in input_abstraction.c's order: jump, switch grenades, action,
switch weapons, melee, flashlight, grenade, primary trigger, start (the pause key), back (the scores' key), crouch,
zoom */
static const char *const control_settings[AE_KEY_CONTROL_COUNT] =
{
	"controls.jump", "controls.switch_grenade", "controls.action", "controls.switch_weapon", "controls.melee",
	"controls.flashlight", "controls.throw_grenade", "controls.fire", "controls.pause", "controls.scoreboard",
	"controls.crouch", "controls.zoom",
};

int ae_key_prompts_control_of_icon(int icon_type, int button, const unsigned char *controls_to_buttons)
{
	int control;

	if (icon_type >= AE_KEY_ICON_ACTION && icon_type <= AE_KEY_ICON_ZOOM)
		return action_controls[icon_type - AE_KEY_ICON_ACTION];
	if (icon_type < 0 || icon_type > AE_KEY_ICON_LOOK)
		return -1;
	/* (a physical button, or accept / back / move / look, which CE resolved to one: the first control the layout
	puts on it; the sticks have none) */
	if (!controls_to_buttons || button < 0 || button >= AE_KEY_BUTTON_COUNT)
		return -1;
	for (control = 0; control < AE_KEY_CONTROL_COUNT; control++)
		if (controls_to_buttons[control] == button)
			return control;
	return -1;
}

const char *ae_key_prompts_control_setting(int control)
{
	return control >= 0 && control < AE_KEY_CONTROL_COUNT ? control_settings[control] : NULL;
}

int ae_key_prompts_first_binding(const char *list, int (*valid)(const char *name), char *key, size_t size)
{
	int piece;

	if (key && size)
		key[0] = 0;
	if (!list || !key || size < 2)
		return 0;
	/* (xinput_sdl.c's bindings_read: two pieces at most, each a key or none) */
	for (piece = 0; piece < 2; piece++)
	{
		size_t length;

		while (*list == ' ' || *list == ',')
			list++;
		length = strcspn(list, ",");
		if (!length)
			continue;
		{
			size_t kept = length;

			while (kept && list[kept - 1] == ' ')
				kept--;
			if (kept > size - 1)
				kept = size - 1;
			memcpy(key, list, kept);
			key[kept] = 0;
		}
		list += length;
		if (key[0] && (!valid || valid(key)))
			return 1;
		key[0] = 0;
	}
	return 0;
}

static int same_name(const char *a, const char *b)
{
	for (; *a && *b; a++, b++)
	{
		char x = *a >= 'a' && *a <= 'z' ? (char)(*a - 32) : *a;
		char y = *b >= 'a' && *b <= 'z' ? (char)(*b - 32) : *b;

		if (x != y)
			return 0;
	}
	return *a == *b;
}

int ae_key_prompts_cell_of(const char *key)
{
	int index;

	if (!key || !key[0])
		return -1;
	for (index = 0; index < NUMBEROF_NAMES; index++)
		if (same_name(key, ae_key_names[index].name))
			return ae_key_names[index].cell;
	return -1;
}

const struct ae_key_cell *ae_key_prompts_cell(int cell)
{
	return cell >= 0 && cell < NUMBEROF_CELLS ? &ae_key_cells[cell] : NULL;
}

int ae_key_prompts_frame_cell(void)
{
	return AE_KEY_FRAME_CELL;
}

void ae_key_prompts_fallback_text(const char *key, char *text, size_t size)
{
	size_t limit = 11;
	size_t length;

	if (!text || !size)
		return;
	text[0] = 0;
	if (!key)
		return;
	if (!strncmp(key, "Keypad ", 7) && key[7])
	{
		/* ("Num 7": the keypad's name, short) */
		key += 7;
		if (size < 5)
			return;
		memcpy(text, "Num ", 4);
		text += 4;
		size -= 4;
		limit -= 4;
	}
	length = strlen(key);
	if (length > limit)
		length = limit;
	if (length > size - 1)
		length = size - 1;
	memcpy(text, key, length);
	text[length] = 0;
}

int ae_key_prompts_wanted(int keyboard_last, int local_player, int keyboard_player, int shared, int force_pad)
{
	if (force_pad || local_player < 0 || local_player != keyboard_player)
		return 0;
	return shared || keyboard_last;
}

int ae_key_prompts_player_of_controller(int controller, int network_game, const short *single_player_controllers,
	int count)
{
	int player, any = 0;

	if (controller < 0)
		return -1;
	if (network_game || !single_player_controllers)
		return controller;
	for (player = 0; player < count; player++)
	{
		if (single_player_controllers[player] == controller)
			return player;
		if (single_player_controllers[player] >= 0)
			any = 1;
	}
	return any ? -1 : controller;
}

/* ---------- sizes */

void ae_key_prompts_picture_size(int cell, enum ae_key_style style, float key_height, float *width, float *height,
	float *margin)
{
	const struct ae_key_cell *entry = ae_key_prompts_cell(cell);
	float nominal_key = style == AE_KEY_STYLE_CE ? AE_KEY_CE_KEY : AE_KEY_AE_KEY;
	float nominal_margin = style == AE_KEY_STYLE_CE ? AE_KEY_CE_MARGIN : AE_KEY_AE_MARGIN;
	float k, w = 0.0f, h = 0.0f, m = 0.0f;

	if (entry && key_height > 0.0f && (int)style >= 0 && style < AE_KEY_STYLE_COUNT)
	{
		/* (the sheet's texels at 1080p, scaled to the keycap's height: a mouse keeps its own taller height) */
		k = key_height / nominal_key;
		w = (float)entry->cells[style].w / AE_KEY_TEXELS * k;
		h = (float)entry->cells[style].h / AE_KEY_TEXELS * k;
		m = nominal_margin * k;
	}
	if (width)
		*width = w;
	if (height)
		*height = h;
	if (margin)
		*margin = m;
}

void ae_key_prompts_sheet_size(enum ae_key_style style, float *width, float *height)
{
	*width = style == AE_KEY_STYLE_CE ? (float)AE_KEY_SHEET_CE_WIDTH : (float)AE_KEY_SHEET_AE_WIDTH;
	*height = style == AE_KEY_STYLE_CE ? (float)AE_KEY_SHEET_CE_HEIGHT : (float)AE_KEY_SHEET_AE_HEIGHT;
}

float ae_key_prompts_frame_width(float text_width, float key_height)
{
	float width = text_width + 0.62f * key_height;

	return width < key_height ? key_height : width;
}

int ae_key_prompts_pad_kind(int split, int view_kind, int last_kind)
{
	return split && view_kind >= 0 ? view_kind : last_kind;
}

/* ---------- the pad's buttons as glyphs */

#define XBOX_SET 0
#define XBOX_360_SET 1
#define DECK_SET 2
#define DUALSENSE_SET 3
#define DUALSHOCK_SET 4
#define NINTENDO_SET 5
#define DUKE_SET 6

/* a glyph set's names, by hud_icon_type 0 to 17: A, B, X, Y, black, white, left and right trigger, the d-pad's up, down, left and
right, start, back, the left and right thumbs (the sticks' clicks), the left and right sticks; NULL for none */
static const char *const pad_sets[][AE_PAD_SLOT_COUNT] =
{
	/* an Xbox One's and Series' */
	{ "xbox_button_a", "xbox_button_b", "xbox_button_x", "xbox_button_y", "xbox_rb", "xbox_lb", "xbox_lt", "xbox_rt",
	  "xbox_dpad_up", "xbox_dpad_down", "xbox_dpad_left", "xbox_dpad_right", "xbox_button_menu", "xbox_button_view",
	  "xbox_ls", "xbox_rs", "xbox_stick_l", "xbox_stick_r" },
	/* an Xbox 360's, and any pad SDL maps as one: START and BACK in words */
	{ "xbox_button_a", "xbox_button_b", "xbox_button_x", "xbox_button_y", "xbox_rb", "xbox_lb", "xbox_lt", "xbox_rt",
	  "xbox_dpad_up", "xbox_dpad_down", "xbox_dpad_left", "xbox_dpad_right", "xbox_button_start", "xbox_button_back",
	  "xbox_ls", "xbox_rs", "xbox_stick_l", "xbox_stick_r" },
	/* the Steam Deck: the Xbox's faces, d-pad and sticks, PlayStation's shoulders, triggers and clicks */
	{ "xbox_button_a", "xbox_button_b", "xbox_button_x", "xbox_button_y", "playstation_trigger_r1",
	  "playstation_trigger_l1", "playstation_trigger_l2", "playstation_trigger_r2", "xbox_dpad_up", "xbox_dpad_down",
	  "xbox_dpad_left", "xbox_dpad_right", "xbox_button_menu", "xbox_button_view", "playstation_button_l3",
	  "playstation_button_r3", "xbox_stick_l", "xbox_stick_r" },
	/* a DualSense's */
	{ "playstation_button_cross", "playstation_button_circle", "playstation_button_square", "playstation_button_triangle",
	  "playstation_trigger_r1", "playstation_trigger_l1", "playstation_trigger_l2", "playstation_trigger_r2",
	  "playstation_dpad_up", "playstation_dpad_down", "playstation_dpad_left", "playstation_dpad_right",
	  "playstation5_button_options", "playstation5_button_create", "playstation_button_l3", "playstation_button_r3",
	  "playstation_stick_l", "playstation_stick_r" },
	/* a DualShock 4's */
	{ "playstation_button_cross", "playstation_button_circle", "playstation_button_square", "playstation_button_triangle",
	  "playstation_trigger_r1", "playstation_trigger_l1", "playstation_trigger_l2", "playstation_trigger_r2",
	  "playstation_dpad_up", "playstation_dpad_down", "playstation_dpad_left", "playstation_dpad_right",
	  "playstation4_button_options", "playstation4_button_share", "playstation_button_l3", "playstation_button_r3",
	  "playstation_stick_l", "playstation_stick_r" },
	/* a Switch's and a Wii U Pro's: A is the bottom button's B, as the kind names them */
	{ "wiiu_button_b", "wiiu_button_a", "wiiu_button_y", "wiiu_button_x", "wiiu_button_r", "wiiu_button_l",
	  "wiiu_button_zl", "wiiu_button_zr", "wiiu_dpad_up", "wiiu_dpad_down", "wiiu_dpad_left", "wiiu_dpad_right",
	  "wiiu_button_plus", "wiiu_button_minus", "wiiu_stick_top_l", "wiiu_stick_top_r", "wiiu_stick_l", "wiiu_stick_r" },
	/* the Duke's and Controller S's: no glyph for black, white or the triggers (their words are on a keycap) */
	{ "xbox_button_a", "xbox_button_b", "xbox_button_x", "xbox_button_y", NULL, NULL, NULL, NULL, "xbox_dpad_up",
	  "xbox_dpad_down", "xbox_dpad_left", "xbox_dpad_right", "xbox_button_start", "xbox_button_back", "xbox_ls", "xbox_rs",
	  "xbox_stick_l", "xbox_stick_r" },
};

static int pad_set_of(int kind)
{
	switch (kind)
	{
	case AE_PAD_KIND_XBOX: return XBOX_SET;
	case AE_PAD_KIND_DUKE:
	case AE_PAD_KIND_CONTROLLER_S: return DUKE_SET;
	case AE_PAD_KIND_DUALSENSE: return DUALSENSE_SET;
	case AE_PAD_KIND_DUALSHOCK: return DUALSHOCK_SET;
	case AE_PAD_KIND_SWITCH_PRO:
	case AE_PAD_KIND_WII_U_PRO: return NINTENDO_SET;
	case AE_PAD_KIND_STEAM_DECK: return DECK_SET;
	}
	return XBOX_360_SET;
}

const char *ae_key_prompts_pad_glyph(int kind, int slot)
{
	return slot >= 0 && slot < AE_PAD_SLOT_COUNT ? pad_sets[pad_set_of(kind)][slot] : NULL;
}

int ae_key_prompts_cell_of_glyph(const char *glyph)
{
	int index;

	if (!glyph)
		return -1;
	for (index = 0; index < NUMBEROF_CELLS; index++)
		if (!strcmp(ae_key_cells[index].glyph, glyph))
			return index;
	return -1;
}

int ae_key_prompts_pad_cell(int kind, int slot)
{
	return ae_key_prompts_cell_of_glyph(ae_key_prompts_pad_glyph(kind, slot));
}
