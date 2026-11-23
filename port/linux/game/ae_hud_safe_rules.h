/*
AE_HUD_SAFE_RULES.H

HUD SAFE AREA (display.hud_safe_area): the HUD's frame pulled in from the screen's edges, 0 to 10 percent of the screen's
width on each side and of its height top and bottom, for TVs that cut the picture and very wide screens. Only the window's outer
edges move: a split screen view's edge where two views meet stays. No engine includes: port/linux/tests/ae_hud_safe_rules_test.c
compiles these alone.
*/

#ifndef __AE_HUD_SAFE_RULES_H
#define __AE_HUD_SAFE_RULES_H

#define AE_HUD_SAFE_MAXIMUM 10

struct ae_safe_rect
{
	int x0, y0, x1, y1;
};

/* the setting's value held to 0 .. AE_HUD_SAFE_MAXIMUM */
int ae_hud_safe_percent(long value);

/* the window with its edges that lie on the screen's edges moved in by percent of the screen's size (left and right by the
width's, top and bottom by the height's). An edge inside the screen stays; a window the move would empty is left whole */
struct ae_safe_rect ae_hud_safe_window(struct ae_safe_rect screen, struct ae_safe_rect window, int percent);

#endif
