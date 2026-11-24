/*
AE_FIREFIGHT_DOORS.C

Firefight is a test-only mode in this build: a host starts it only with DEV
MODE on (ae_firefight_doors_rules.h). Every way a variant gets built or kept
asks here; a refused Firefight game falls back to what an unknown gametype
name gives (no game engine), or, for a variant read from a file, to engine 5.
*/

#include "cseries.h"
#include "networking/network_game_globals.h"
#include "game/game_engine.h"

#include "ae_dev_mode.h"
#include "ae_firefight_doors.h"
#include "ae_firefight_doors_rules.h"
#include "halo_firefight.h"

void platform_log(char const *format, ...);
extern int config_boolean(char const *name);

#define FIREFIGHT_REFUSED_ENGINE_FALLBACK 5

boolean firefight_engine_allowed(void)
{
	return ae_firefight_doors_engine_allowed(ae_dev_mode_setting(), network_game_distributed_client()) != 0;
}

boolean firefight_engine_refused(void)
{
	if (firefight_engine_allowed())
		return FALSE;
	platform_log("firefight: not started: it is a DEV MODE test mode in this version");
	return TRUE;
}

short firefight_engine_index_kept(short engine_index)
{
	short kept = (short)ae_firefight_doors_engine_index(engine_index, game_engine_firefight,
		FIREFIGHT_REFUSED_ENGINE_FALLBACK, firefight_engine_allowed());

	if (kept != engine_index)
		platform_log("firefight: not started: it is a DEV MODE test mode in this version");
	return kept;
}

int ae_firefight_doors_pack_on(void)
{
	static int told;
	int setting = config_boolean("game.firefight_pack") != 0;
	int on = ae_firefight_doors_pack_build(setting, ae_dev_mode_setting());

	if (setting && !on && !told)
	{
		told = 1;
		platform_log("firefight: its data is not built: game.firefight_pack is a DEV MODE test setting in this version");
	}
	return on;
}

int ae_firefight_doors_skulls_send_on(void)
{
	return ae_firefight_doors_skulls_send(config_boolean("game.skulls") != 0);
}

int ae_firefight_doors_skulls_pack_early_on(void)
{
	return ae_firefight_doors_skulls_pack_early(config_boolean("game.skulls") != 0, ae_dev_mode_setting());
}
