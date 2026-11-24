/*
AE_DEV_MODE.C

DEV MODE's switch (ae_dev_mode.h).
*/

#include "cseries.h"
#include "networking/network_game_globals.h"
#include "../src/port_config.h"

#include "ae_dev_mode.h"
#include "network_replay.h"

int ae_dev_mode_setting(void)
{
	return config_boolean("system.dev_mode") != 0;
}

int ae_dev_mode_on(void)
{
	/* (a replay's playback is a client with no host: this machine's game, its commands this machine's) */
	return ae_dev_mode_setting() && (!network_game_distributed_client() || network_replay_playing());
}
