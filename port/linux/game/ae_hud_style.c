/*
AE_HUD_STYLE.C

HUD STYLE (display.hud_style): AE's restyled HUD (the score strip and the AE
look of everything Arena Evolved draws) or CLASSIC (the HUD exactly as
before). See ae_hud_style.h.
*/

#include <string.h>

#include "ae_hud_style.h"

const char *config_string(const char *name);
unsigned long config_changes(void);
unsigned long ae_config_player_changes(void);

/* (asked many times a view a frame: read again only when a setting changed) */
boolean ae_hud_style_is_ae(void)
{
	static unsigned long read_at = (unsigned long)-1;
	static boolean ae = TRUE;

	if (read_at != ae_config_player_changes())
	{
		const char *value = config_string("display.hud_style");

		read_at = ae_config_player_changes();
		ae = (boolean)!(value && strcmp(value, "classic") == 0);
	}
	return ae;
}

const char *ae_hud_style_name(void)
{
	return ae_hud_style_is_ae() ? "ae" : "classic";
}
