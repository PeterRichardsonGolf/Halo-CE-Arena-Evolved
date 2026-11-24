/*
AE_RETICLE_GROWTH.H

RETICLE GROWTH (ae_reticle_growth.c): what the AE hook line in hud_weapon.c
calls (port/linux/game/ae_hooks.txt lists it). Included after cseries.h.
*/

#ifndef __AE_RETICLE_GROWTH_H
#define __AE_RETICLE_GROWTH_H

/* the size (1 its own) a local player's crosshair item is drawn at: an aim
reticle's item of an AE rifle grows a little with each shot and comes back
(its profile's growth); anything else 1 */
real ae_reticle_growth_scale(short local_player_index, long weapon_index, boolean aim, short item_index);

#endif // __AE_RETICLE_GROWTH_H
