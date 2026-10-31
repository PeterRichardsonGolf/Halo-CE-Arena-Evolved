/*
AE_LAN_BROADCAST.C

Arena Evolved: the local networks' broadcast addresses for a system link
game's broadcasts behind a VPN (ae_lan_broadcast.h): the targets, looked up
again every few seconds and logged when they change. The interfaces are
ae_lan_networks.c's (Linux, Android, macOS, the web) and
port/windows/src/win32_ae_lan.c's.
*/

#include "ae_lan_broadcast.h"
#include "log_address.h"
#include "platform.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

static struct
{
	int looked;
	time_t looked_at;
	int count;
	unsigned long networks[AE_LAN_BROADCAST_NETWORKS];
	unsigned long own[AE_LAN_BROADCAST_NETWORKS];
} ae_lan_broadcast;

/* the local networks, looked up again every few seconds (a laptop changes networks) */
static void ae_lan_broadcast_look(void)
{
	time_t now = time(NULL);

	if (!ae_lan_broadcast.looked || now < ae_lan_broadcast.looked_at ||
		now - ae_lan_broadcast.looked_at >= AE_LAN_BROADCAST_REFRESH_SECONDS)
	{
		unsigned long networks[AE_LAN_BROADCAST_NETWORKS], own[AE_LAN_BROADCAST_NETWORKS];
		int count = ae_lan_networks(own, networks, AE_LAN_BROADCAST_NETWORKS);

		/* (logged when the networks change: the first look, a new Wi-Fi, a cable) */
		if (!ae_lan_broadcast.looked || count != ae_lan_broadcast.count ||
			memcmp(networks, ae_lan_broadcast.networks, (size_t)count * sizeof(*networks)) != 0)
		{
			char line[AE_LAN_BROADCAST_NETWORKS * (LOG_ADDRESS_SIZE + 2) + 1];
			size_t length = 0;
			int index;

			line[0] = 0;
			for (index = 0; index < count; index++)
			{
				char text[LOG_ADDRESS_SIZE];

				log_address((const unsigned char *)&networks[index], 4, -1, text, sizeof(text));
				length += (size_t)snprintf(line + length, sizeof(line) - length, "%s%s", index ? ", " : "", text);
			}
			platform_log("network: broadcasts go to 255.255.255.255%s%s", count ? " and the local networks' " : "",
				line);
		}
		memcpy(ae_lan_broadcast.networks, networks, (size_t)count * sizeof(*networks));
		memcpy(ae_lan_broadcast.own, own, (size_t)count * sizeof(*own));
		ae_lan_broadcast.count = count;
		ae_lan_broadcast.looked = 1;
		ae_lan_broadcast.looked_at = now;
	}
}

int ae_lan_broadcast_targets(unsigned long *targets, int maximum_count)
{
	ae_lan_broadcast_look();
	return ae_lan_broadcast_merge(ae_lan_broadcast.networks, ae_lan_broadcast.count, targets, maximum_count);
}

unsigned long ae_lan_title_address(unsigned long route_address)
{
	static unsigned long logged;
	unsigned long address;

	ae_lan_broadcast_look();
	address = ae_lan_pick_title_address(route_address, ae_lan_broadcast.own, ae_lan_broadcast.count);
	/* (logged when it is not the route's: a VPN's, said once per address) */
	if (address != route_address && address != logged)
	{
		char route_text[LOG_ADDRESS_SIZE], text[LOG_ADDRESS_SIZE];

		log_address((const unsigned char *)&route_address, 4, -1, route_text, sizeof(route_text));
		log_address((const unsigned char *)&address, 4, -1, text, sizeof(text));
		platform_log("network: a hosted game is joined at %s on the local network (the default route's %s is no "
			"local network's: a VPN)", text, route_text);
		logged = address;
	}
	return address;
}
