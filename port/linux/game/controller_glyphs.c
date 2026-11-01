/* Controller-family artwork for MCC HUD prompts (mcc_hud_draw.c). GulchCE's version draws from its controller
remapping's glyph sets, which Arena Evolved does not take (its prompts are its own: ae_key_prompts.c): here no
glyph set is chosen, so the MCC HUD draws the map's own buttons, as its legacy path does. */
#include "cseries.h"
#include "controller_glyphs.h"

/* (2: the legacy Xbox style, mcc_hud_draw.c's fallback) */
short controller_glyph_style(void)
{
	return 2;
}

boolean controller_glyph_draw(short style, short index,
	struct icon_hud_element_definition const *icon, rectangle2d *cursor, pixel32 color,
	void (*unbind)(unsigned long const *))
{
	(void)style;
	(void)index;
	(void)icon;
	(void)cursor;
	(void)color;
	(void)unbind;
	return FALSE;
}

void controller_glyphs_dispose(void)
{
}
