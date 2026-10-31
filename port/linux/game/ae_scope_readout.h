/*
AE_SCOPE_READOUT.H

SCOPE READOUTS (ae_scope_readout.c): what the AE hook line in hud_weapon.c
calls (port/linux/game/ae_hooks.txt lists it). Included after cseries.h.
*/

#ifndef __AE_SCOPE_READOUT_H
#define __AE_SCOPE_READOUT_H

/* a local player's zoomed AE rifle whose profile has scope readouts: its zoom and its rounds loaded drawn in the
frames its zoom overlay has at the window's sides; anything else nothing */
void ae_scope_readout_draw(short local_player_index, long unit_index, long weapon_index);

#endif // __AE_SCOPE_READOUT_H
