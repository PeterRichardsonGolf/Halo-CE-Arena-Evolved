/*
AE_FIREFIGHT_DOORS.C

A host starts a Firefight game only from the FIREFIGHT lobby or with DEV MODE on
(ae_firefight_doors_rules.h). Every way a variant gets built or kept asks here;
a refused Firefight game falls back to what an unknown gametype name gives (no
game engine), or, for a variant read from a file, to engine 5. The FIREFIGHT
row's wait on Firefight's data (the campaign maps' tags) is read here too.
*/

#include "cseries.h"
#include "networking/network_game_globals.h"
#include "game/game_engine.h"

#include "ae_dev_mode.h"
#include "ae_firefight_doors.h"
#include "ae_firefight_doors_rules.h"
#include "ae_randomizer_glue.h"
#include "halo_firefight.h"
#include "tag_pack.h"

void platform_log(char const *format, ...);
extern int config_boolean(char const *name);
unsigned long system_milliseconds(void);

#define FIREFIGHT_REFUSED_ENGINE_FALLBACK 5

static int lobby_is_open;

void ae_firefight_doors_lobby_set(int open)
{
	lobby_is_open = open != 0;
}

int ae_firefight_doors_lobby_open(void)
{
	return lobby_is_open;
}

boolean firefight_engine_allowed(void)
{
	return ae_firefight_doors_engine_allowed(ae_dev_mode_setting(), network_game_distributed_client(), lobby_is_open) != 0;
}

boolean firefight_engine_refused(void)
{
	if (firefight_engine_allowed())
		return FALSE;
	platform_log("firefight: not started: a host starts it from the FIREFIGHT menu");
	return TRUE;
}

short firefight_engine_index_kept(short engine_index)
{
	short kept = (short)ae_firefight_doors_engine_index(engine_index, game_engine_firefight,
		FIREFIGHT_REFUSED_ENGINE_FALLBACK, firefight_engine_allowed());

	if (kept != engine_index)
		platform_log("firefight: not started: a host starts it from the FIREFIGHT menu");
	return kept;
}

int ae_firefight_doors_pack_on(void)
{
	static int told;
	int setting = config_boolean("game.firefight_pack") != 0;
	int on = ae_firefight_doors_pack_build(setting, ae_dev_mode_setting(), lobby_is_open);

	if (setting && !on && !told)
	{
		told = 1;
		platform_log("firefight: its data is not built: game.firefight_pack builds it with DEV MODE on, or from the FIREFIGHT menu");
	}
	return on;
}

static int data_forced = -1;

void ae_firefight_doors_data_force(int state)
{
	data_forced = state >= 0 && state <= AE_FFD_DATA_FAILED ? state : -1;
}

int ae_firefight_doors_data(void)
{
	static unsigned long read_at;
	static int state = AE_FFD_DATA_PREPARING, known;
	unsigned long now = system_milliseconds();

	if (data_forced >= 0)
		return data_forced;

	if (!known || now - read_at >= 1000)
	{
		struct tag_pack_state pack;

		tag_pack_state(FIREFIGHT_PACK, &pack);
		state = ae_firefight_doors_data_state(pack.maps_wanted, pack.done, pack.writing, pack.failed);
		known = 1;
		read_at = now;
	}
	return state;
}

int ae_firefight_doors_skulls_send_on(void)
{
	return ae_firefight_doors_skulls_send(config_boolean("game.skulls") != 0);
}

int ae_firefight_doors_skulls_pack_early_on(void)
{
	return ae_firefight_doors_skulls_pack_early(config_boolean("game.skulls") != 0, ae_dev_mode_setting());
}
