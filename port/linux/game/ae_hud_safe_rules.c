/*
AE_HUD_SAFE_RULES.C

HUD SAFE AREA's math (ae_hud_safe_rules.h). No engine includes.
*/

#include "ae_hud_safe_rules.h"

int ae_hud_safe_percent(long value)
{
	if (value < 0)
		return 0;
	if (value > AE_HUD_SAFE_MAXIMUM)
		return AE_HUD_SAFE_MAXIMUM;
	return (int)value;
}

struct ae_safe_rect ae_hud_safe_window(struct ae_safe_rect screen, struct ae_safe_rect window, int percent)
{
	struct ae_safe_rect out = window;
	int width = screen.x1 - screen.x0;
	int height = screen.y1 - screen.y0;
	int dx, dy;

	percent = ae_hud_safe_percent(percent);
	if (percent == 0 || width <= 0 || height <= 0)
		return window;
	dx = (width * percent + 50) / 100;
	dy = (height * percent + 50) / 100;
	if (window.x0 <= screen.x0)
		out.x0 = window.x0 + dx;
	if (window.x1 >= screen.x1)
		out.x1 = window.x1 - dx;
	if (window.y0 <= screen.y0)
		out.y0 = window.y0 + dy;
	if (window.y1 >= screen.y1)
		out.y1 = window.y1 - dy;
	if (out.x1 <= out.x0 || out.y1 <= out.y0)
		return window;
	return out;
}
