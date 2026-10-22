/*
AE_MOUSE_AIM.C

MOUSE AIM ASSIST as the gametype's rule (ae_mouse_aim.h; the bit: ae_mouse_aim_rules.h). The host (or a local game)
reads the running game's variant; a network client takes its host's policy from message 90, as OpenCE's clients do.
*/

#include "cseries.h"
#include "cseries/errors.h"
#include "game/game_engine.h"

#include "ae_mouse_aim.h"
#include "ae_mouse_aim_rules.h"

/* (game_engine.h names the bit the rules header reads) */
typedef char ae_mouse_aim_assist_bit_check[_game_variant_block_mouse_aim_assist_bit == AE_MOUSE_AIM_ASSIST_BIT ? 1 : -1];

/* the last policy logged, by the host and by a client (-1 none yet) */
static int ae_mouse_aim_assist_host_logged = -1;
static int ae_mouse_aim_assist_client_logged = -1;

boolean ae_mouse_aim_assist_host_blocks(boolean host_setting)
{
	struct game_variant const *variant = game_engine_running() ? game_engine_get_variant() : NULL;
	int rule = variant ? ae_mouse_aim_assist_get(variant->universal_variant.flags) : _ae_mouse_aim_assist_allowed;
	int blocked = ae_mouse_aim_assist_policy(host_setting, rule);

	if (blocked != ae_mouse_aim_assist_host_logged)
	{
		ae_mouse_aim_assist_host_logged = blocked;
		error(_error_silent, "AE mouse aim assist: the host's policy: %s (gametype %s, network.block_mouse_aim_assist %s)",
			blocked ? "blocked" : "allowed", ae_mouse_aim_assist_name(rule), host_setting ? "on" : "off");
	}
	return blocked ? TRUE : FALSE;
}

void ae_mouse_aim_assist_note_policy(boolean blocked)
{
	int now = blocked ? 1 : 0;

	if (now != ae_mouse_aim_assist_client_logged)
	{
		ae_mouse_aim_assist_client_logged = now;
		error(_error_silent, "AE mouse aim assist: the host says: %s", now ? "blocked" : "allowed");
	}
}

void ae_mouse_aim_assist_log_rules(unsigned long flags)
{
	error(_error_silent, "AE mouse aim assist: %s", ae_mouse_aim_assist_name(ae_mouse_aim_assist_get(flags)));
}
