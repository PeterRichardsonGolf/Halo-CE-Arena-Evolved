/*
AE_JUMP.H

The gametype's JUMP and the map layout's jump line, as a player's jump multiplier (ae_jump.c).
*/

#ifndef __AE_JUMP_H
#define __AE_JUMP_H

#include "cseries.h"

/* (game_engine_bomb.c's game_engine_traits_jump_multiplier, AE hook: biped_jump and the host's bound on a guest's
movement) the player's jump speed multiplier: the map layout's jump line, else the gametype's JUMP for a player
whose machine runs this game, else 1 */
real ae_jump_multiplier(long player_index);

#endif // __AE_JUMP_H
