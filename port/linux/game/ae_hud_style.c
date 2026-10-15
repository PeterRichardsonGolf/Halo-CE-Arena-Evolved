/*
AE_HUD_STYLE.C

HUD STYLE (display.hud_style): AE's restyled HUD (the score strip and the AE
look of everything Arena Evolved draws) or CLASSIC (the HUD exactly as
before). See ae_hud_style.h.
*/

#include <string.h>

#include "ae_hud_style.h"

const char *config_string(const char *name);

boolean ae_hud_style_is_ae(void)
{
	const char *value = config_string("display.hud_style");

	return !(value && strcmp(value, "classic") == 0);
}

const char *ae_hud_style_name(void)
{
	return ae_hud_style_is_ae() ? "ae" : "classic";
}
