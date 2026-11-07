/*
AE_CROUCH.C

CONTROLLER CROUCH as the gametype's rule (ae_crouch.h; the bit: ae_crouch_rules.h). Every machine reads the running
game's variant, the host's, so a network client's controllers crouch by the host's rule.
*/

#include "cseries.h"
#include "cseries/errors.h"
#include "game/game_engine.h"

#include "ae_crouch.h"
#include "ae_crouch_rules.h"

/* (game_engine.h names the bit the rules header reads) */
typedef char ae_controller_crouch_bit_check[_game_variant_controller_crouch_bit == AE_CONTROLLER_CROUCH_BIT ? 1 : -1];

/* (the gate's first read since the game's rules were logged, logged once: what player control plays, on a network
client the host's variant. Halo's own terms come first in the gate, so it is read once a grounded player's stick is
pushed all the way) */
static boolean ae_controller_crouch_gate_logged = FALSE;

boolean ae_controller_crouch_holds_down(boolean crouch_held)
{
	struct game_variant const *variant = game_engine_running() ? game_engine_get_variant() : NULL;
	int rule = variant ? ae_controller_crouch_get(variant->universal_variant.flags) : _ae_controller_crouch_halo_ce;

	if (!ae_controller_crouch_gate_logged)
	{
		ae_controller_crouch_gate_logged = TRUE;
		error(_error_silent, "AE controller crouch gate: %s (player control's first read)",
			ae_controller_crouch_name(rule));
	}
	return ae_controller_crouch_holds(rule, crouch_held) ? TRUE : FALSE;
}

void ae_controller_crouch_log_rules(unsigned long flags)
{
	ae_controller_crouch_gate_logged = FALSE;
	error(_error_silent, "AE controller crouch: %s", ae_controller_crouch_name(ae_controller_crouch_get(flags)));
}
