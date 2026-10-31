/*
AE_ALLY_RETICLE.H

ALLY RETICLE (ae_ally_reticle.c): what the AE hook line in hud_weapon.c calls
(port/linux/game/ae_hooks.txt lists it). Included after cseries.h.
*/

#ifndef __AE_ALLY_RETICLE_H
#define __AE_ALLY_RETICLE_H

/* the colour a local player's aim reticle piece is drawn in: green when the
setting is on and an ally is under it and the piece is the plain frame
(aim_frame 0), else the colour given */
unsigned long ae_ally_reticle_color(short local_player_index, short aim_frame, unsigned long color);

#endif // __AE_ALLY_RETICLE_H
