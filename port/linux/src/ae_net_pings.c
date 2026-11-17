/*
AE_NET_PINGS.C

Arena Evolved: SERVER PINGS, the server browser's opt-in (ae_net_pings.h).
*/

#include "ae_net_pings.h"
#include "port_config.h"

int ae_browser_pings_on(void)
{
	return config_boolean("network.browser_pings") != 0;
}
