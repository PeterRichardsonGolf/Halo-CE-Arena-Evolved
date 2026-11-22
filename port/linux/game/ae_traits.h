/*
AE_TRAITS.H

The host's player traits GRAVITY and PLAYER SPEED (ae_traits_rules.h) as a player's multipliers (ae_traits.c).
JUMP HEIGHT is the gametype's JUMP byte, which ae_jump.c reads.
*/

#ifndef __AE_TRAITS_H
#define __AE_TRAITS_H

#include "cseries.h"

/* (game_engine_bomb.c's game_engine_traits_gravity_multiplier, AE hook: a player's biped in the air and the host's
bound on a guest's rise) the gravity multiplier on the player: the gametype's GRAVITY for a player whose machine
plays it, else 1 */
real ae_traits_gravity_multiplier(long player_index);

/* (bipeds.c's movement speed and the host's bound on a guest's run, AE hook) the speed multiplier on the player:
the gametype's PLAYER SPEED for a player whose machine plays it, else 1 */
real ae_traits_speed_multiplier(long player_index);

/* (the bots) how a jump on this map, under its gametype, reaches against Halo CE's: the rise (the jump's height)
and the run (how far a running jump carries), each 1 for Halo CE's. From the map's player biped's jump speed and the
gravity, with the gametype's JUMP HEIGHT (or the map layout's jump), GRAVITY and PLAYER SPEED */
void ae_traits_game_jump_scales(real *rise, real *across);
/* (a new game's bots, before their first ask: the map and its biped may have changed) */
void ae_traits_game_jump_scales_forget(void);

/* (the bots) the gametype's PLAYER SPEED as a multiplier, 1 for the map's own */
real ae_traits_game_speed(void);

#endif // __AE_TRAITS_H
