/*
AE_CROSSHAIR.H

CROSSHAIR v3 in the game (ae_crosshair.c): what the AE hook lines in hud_weapon.c and hud_draw.c call, and what the
menus' preview card paints. Included after cseries.h.
*/

#ifndef __AE_CROSSHAIR_H
#define __AE_CROSSHAIR_H

#include "ae_crosshair_rules.h"

/* the aim state's items of the held weapon: TRUE when the player's CROSSHAIR draws in their place (the stock items are
skipped); *drawn keeps it to once a view. color is the item's colour (its alpha is the HUD's fade) */
boolean ae_crosshair_replaced(short local_player_index, long weapon_index, short frame_index, unsigned long color,
	boolean *drawn);

/* the stock reticle's rows (STOCK): RETICLE SIZE as a multiplier, the colour of an aim item (ORIGINAL keeps it;
red over an enemy and green over an ally stay), and the reticle's FOV scaling (FIXED: 1) */
real ae_crosshair_stock_size(short local_player_index);
unsigned long ae_crosshair_stock_color(short local_player_index, short frame_index, unsigned long color);
real ae_crosshair_fov_scale(short local_player_index);
/* (0.7.7) RETICLE MOTION of the stock reticle (AE_XH_STOCK_MOTION_) */
int ae_crosshair_stock_motion(short local_player_index);

/* paints a crosshair: centred at (x, y) in the draw layer's units, its pieces built at build_scale (window pixels per
1080p pixel; 1 in a menu, where the units are the pieces') and placed unit_factor units a pixel */
void ae_crosshair_paint(const struct ae_crosshair *xh, const struct ae_xh_motion *motion, unsigned long rgb, float x,
	float y, float build_scale, float unit_factor, float alpha);

#endif // __AE_CROSSHAIR_H
