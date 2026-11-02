/*
AE_HUD_GLYPHS.H

The HUD's message lines draw a pad's buttons with the menus' own glyphs (ae_hud_glyphs.c).
*/

#ifndef __AE_HUD_GLYPHS_H
#define __AE_HUD_GLYPHS_H

#include "cseries/cseries.h"

struct icon_hud_element_definition;

/* hud_messaging.c's in-line icons (an AE hook): a HUD globals button icon in HUD STYLE AE's line, one player on the
screen, a pad last used: the menus' glyph for it in its family and colour, placed in the line; FALSE elsewhere (the
picture as before) */
boolean ae_hud_glyph_button(struct icon_hud_element_definition const *icon);
/* the device the glyphs are drawn for: AE_DEVICE_* (ae_ui.h), the keyboard after any pad */
int ae_hud_glyph_device(void);

#endif // __AE_HUD_GLYPHS_H
