/*
AE_HUD_GLYPHS.C

The HUD's message lines draw a pad's buttons ("Press %y-button to switch to") with the menus' own glyphs: Kenney's
Input Prompts in the family of the pad last used (Xbox letters, PlayStation shapes, Nintendo letters), in the same
colours as the menus' footers and the TEST RUN card (ae_prompt_tint), so every map's prompts (Xbox, Custom Edition,
MCC) and AE's screens show one set (owner 2026-11-01). The glyph goes in the line through the HUD messages' own
slot (ae_hud_message_place) and is drawn by AE's overlay (ae_draw), over the HUD and under AE's menus.

Only HUD STYLE AE's message lines with one player on the screen; the keyboard keeps its keycaps
(ae_key_prompts.c); the buttons the menus have no glyph for (BLACK, WHITE, the sticks, the d-pad's up and down)
keep their pictures.
*/

#include "cseries/cseries.h"
#include "interface/hud.h"
#include "interface/hud_definitions.h"
#include "interface/hud_messaging.h"

#include "ae_hud_digits.h"
#include "ae_hud_glyphs.h"
#include "ae_hud_messages.h"
#include "ae_style.h"
#include "ae_ui.h"
#include "ae_widgets.h"
#include "../src/ae_draw.h"
#include "../src/ae_platform.h"

short local_player_count(void);

/* the glyph's height, of the line's text size (the TEST RUN card: 22 beside 18) */
#define GLYPH_EM 1.22f
/* the room before and after it, of the text's size (the words' spaces count toward it) */
#define GLYPH_GAP_EM 0.10f

/* the HUD globals' button icons (hud_definitions.h's button_icons, in the Xbox's order) as the menus' buttons; -1:
none (BLACK, WHITE, the d-pad's up and down, the thumbsticks) */
static int menu_button(short index)
{
	static const int buttons[] =
	{
		AE_BUTTON_A, AE_BUTTON_B, AE_BUTTON_X, AE_BUTTON_Y, -1, -1,
		AE_BUTTON_LEFT_TRIGGER, AE_BUTTON_RIGHT_TRIGGER, -1, -1, AE_BUTTON_DPAD_LEFT, AE_BUTTON_DPAD_RIGHT,
		AE_BUTTON_START, AE_BUTTON_BACK,
	};

	return index >= 0 && index < (short)NUMBEROF(buttons) ? buttons[index] : -1;
}

int ae_hud_glyph_device(void)
{
	int scheme;

	if (ae_platform_keyboard_last_used())
		return AE_DEVICE_KEYBOARD_MOUSE;
	scheme = ae_platform_input_scheme();
	return scheme >= AE_DEVICE_XBOX && scheme <= AE_DEVICE_NINTENDO ? scheme : AE_DEVICE_XBOX;
}

boolean ae_hud_glyph_button(struct icon_hud_element_definition const *icon)
{
	struct icon_hud_element_definition const *icons;
	short index, count, view;
	int device, button, font;
	real px, cap, alpha, size, width, gap, x, top, units;
	unsigned int tint;

	if (!icon || !hud_globals || !ae_draw_available() || local_player_count() > 1)
		return FALSE;
	icons = (struct icon_hud_element_definition const *)xbox_pointer(hud_globals->messaging.button_icons.address);
	count = (short)hud_globals->messaging.button_icons.count;
	for (index = 0; index < count && icons + index != icon; index++)
		;
	button = icons ? menu_button(index < count ? index : NONE) : -1;
	device = ae_hud_glyph_device();
	if (button < 0 || device == AE_DEVICE_KEYBOARD_MOUSE || !ae_hud_message_metrics(&view, &px, &cap, &alpha))
		return FALSE;
	font = device == AE_DEVICE_PLAYSTATION ? AE_FONT_PLAYSTATION : device == AE_DEVICE_NINTENDO ? AE_FONT_NINTENDO :
		AE_FONT_XBOX;

	/* (window pixels for the line's slot; the overlay's units to draw) */
	ae_draw_view_full();
	units = ae_draw_units_per_pixel();
	if (units <= 0.0f)
		return FALSE;
	size = GLYPH_EM * px;
	width = ae_draw_button_width(font, button, size * units) / units;
	gap = GLYPH_GAP_EM * px;
	if (width <= 0.0f)
		return FALSE;
	ae_hud_message_place(width, size, gap, gap, &x, &top);
	tint = ae_prompt_tint(device, button);
	tint = (tint & 0xFFFFFF00u) | (unsigned int)(alpha * (real)(tint & 0xFFu) + 0.5f);
	ae_draw_button(font, button, size * units, x * units, top * units, tint);
	(void)view;
	(void)cap;
	return TRUE;
}
