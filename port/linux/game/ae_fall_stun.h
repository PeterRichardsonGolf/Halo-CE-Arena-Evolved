/*
AE_FALL_STUN.H

FALL STUN (ae_fall_stun_rules.h): biped_start_landing's AE hook in source/units/bipeds.c.
*/

#ifndef AE_FALL_STUN_H
#define AE_FALL_STUN_H

/* whether a hard landing of the biped's player skips the movement and jump lock under the gametype's FALL STUN
(the host's byte on a client: ae_variant_ext_game). Not a player's biped: never */
int ae_fall_stun_spared(long player_index);

#endif
