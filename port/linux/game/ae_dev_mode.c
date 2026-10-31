/*
AE_DEV_MODE.C

DEV MODE's switch (ae_dev_mode.h).
*/

#include "cseries.h"
#include "networking/network_game_globals.h"
#include "../src/port_config.h"

#include "ae_dev_mode.h"

int ae_dev_mode_setting(void)
{
	return config_boolean("system.dev_mode") != 0;
}

int ae_dev_mode_on(void)
{
	return ae_dev_mode_setting() && !network_game_distributed_client();
}
