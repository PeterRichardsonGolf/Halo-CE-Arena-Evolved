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
