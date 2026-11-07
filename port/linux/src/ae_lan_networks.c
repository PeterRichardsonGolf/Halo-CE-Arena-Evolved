/*
AE_LAN_NETWORKS.C

The local networks (ae_lan_broadcast.h): this machine's address on each and its broadcast address, from getifaddrs
on Linux and macOS; none on the web or Android (its guest C library has no
getifaddrs: the plain broadcast alone, as before). Apart from ae_lan_broadcast.c:
the system's socket headers and the port's Winsock names (platform.h) can't
meet in one file. Windows' are port/windows/src/win32_ae_lan.c's.
*/

#include <stdio.h>

#include "ae_lan_broadcast.h"
#include "ae_vpn_rules.h"

#if !defined(_WIN32) && (defined(__linux__) || defined(__APPLE__)) && !defined(__EMSCRIPTEN__) && !defined(HALO_ANDROID)
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>

int ae_lan_networks(unsigned long *own, unsigned long *broadcasts, int maximum_count)
{
	struct ifaddrs *interfaces, *entry;
	int count = 0;

	if (getifaddrs(&interfaces) != 0)
		return 0;
	for (entry = interfaces; entry && count < maximum_count; entry = entry->ifa_next)
	{
		/* (a VPN's tunnel is point to point, without broadcasts: its broadcast address field is the far end's) */
		if (!entry->ifa_addr || entry->ifa_addr->sa_family != AF_INET || !entry->ifa_broadaddr ||
			entry->ifa_broadaddr->sa_family != AF_INET ||
			(entry->ifa_flags & (IFF_UP | IFF_RUNNING | IFF_BROADCAST)) != (IFF_UP | IFF_RUNNING | IFF_BROADCAST) ||
			(entry->ifa_flags & (IFF_LOOPBACK | IFF_POINTOPOINT)))
		{
			continue;
		}
		own[count] = ((struct sockaddr_in *)entry->ifa_addr)->sin_addr.s_addr;
		broadcasts[count++] = ((struct sockaddr_in *)entry->ifa_broadaddr)->sin_addr.s_addr;
	}
	freeifaddrs(interfaces);
	return count;
}

int ae_lan_vpn(char *name, int size)
{
	struct ifaddrs *interfaces, *entry;
	int found = 0;

	if (name && size > 0)
		name[0] = 0;
	if (getifaddrs(&interfaces) != 0)
		return 0;
	for (entry = interfaces; entry && !found; entry = entry->ifa_next)
	{
		/* (an interface with an IPv4 or IPv6 address, up and running) */
		if (!entry->ifa_addr || (entry->ifa_addr->sa_family != AF_INET && entry->ifa_addr->sa_family != AF_INET6) ||
			(entry->ifa_flags & (IFF_UP | IFF_RUNNING)) != (IFF_UP | IFF_RUNNING) || (entry->ifa_flags & IFF_LOOPBACK) ||
			!ae_vpn_interface_name(entry->ifa_name, (entry->ifa_flags & IFF_POINTOPOINT) != 0))
		{
			continue;
		}
		found = 1;
		if (name && size > 0)
			snprintf(name, (size_t)size, "%s", ae_vpn_label(entry->ifa_name));
	}
	freeifaddrs(interfaces);
	return found;
}
#elif !defined(_WIN32)
int ae_lan_networks(unsigned long *own, unsigned long *broadcasts, int maximum_count)
{
	(void)own;
	(void)broadcasts;
	(void)maximum_count;
	return 0;
}

int ae_lan_vpn(char *name, int size)
{
	if (name && size > 0)
		name[0] = 0;
	return 0;
}
#endif
