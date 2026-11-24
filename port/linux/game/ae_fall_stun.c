/*
AE_FALL_STUN.C

FALL STUN (ae_fall_stun.h): the glue between bipeds.c's landing code and ae_fall_stun_rules.h. Every machine runs
the biped's landing for the bipeds it simulates, a client's own player included, and reads the gametype's byte
(the host's, on a client), so a host with FALL STUN OFF is predicted without the lock too. On the host, a player
whose machine runs an earlier build (no FALL STUN there) keeps the lock, as that machine predicts it.
*/

#include "cseries.h"

#include "ae_fall_stun.h"
#include "ae_traits.h"
#include "ae_fall_stun_rules.h"
#include "ae_variant_ext.h"
#include "ae_variant_ext_rules.h"

int ae_fall_stun_spared(long player_index)
{
	if (player_index == -1 || !ae_traits_player_has_them(player_index))
		return 0;
	return ae_fall_stun_off(ae_variant_ext_game()->fall_stun);
}
