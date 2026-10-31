/*
AE_LAN_BROADCAST.H

Arena Evolved: a system link game is seen and joined on the local network
behind a VPN. The game sends its searches and its game's adverts to
255.255.255.255; a VPN client (ProtonVPN, WireGuard, OpenVPN) that takes the
default route takes that address into its tunnel too, so no machine on the
LAN hears them and no one sees the game. With network.broadcast unset (and
no network.address), xnet.c's broadcast_targets (an AE hook) sends each
broadcast to 255.255.255.255 and also to the broadcast address of every
local network this machine is on (192.168.1.255 for 192.168.1.20/24), which
leaves by that network's own interface whatever the default route is.

The same VPN's default route made the game's own address (the one a
hosted game is joined at, xnet.c's title_address: an AE hook) the tunnel's,
which no machine on the LAN can reach: with network.address unset it is
this machine's address on a local network when the route's is none.

The interfaces: up, with broadcasts, not loopback and not point to point (a
VPN's tunnel on Linux and macOS); on Windows Ethernet and Wi-Fi adapters
(port/windows/src/win32_ae_lan.c). Looked up again every few seconds (a
laptop changes networks). A machine on a plain LAN sends each broadcast
twice (both reach the same machines): the game's searches and adverts are
the same each time, so a second copy changes nothing.

The merge and the address pick are static inline, so
port/linux/tests/ae_lan_broadcast_test.c checks them without a link.
*/

#ifndef __AE_LAN_BROADCAST_H
#define __AE_LAN_BROADCAST_H

enum
{
	/* the local networks looked at (a machine on more is rare) */
	AE_LAN_BROADCAST_NETWORKS = 8,
	/* how often the interfaces are looked up again (seconds) */
	AE_LAN_BROADCAST_REFRESH_SECONDS = 5,
};

/* each local network this machine is on (network byte order): its own
address there and the network's broadcast address, up to maximum_count;
returns their count (0 where the platform has none to give: the web,
Android) */
int ae_lan_networks(unsigned long *own, unsigned long *broadcasts, int maximum_count);

/* xnet.c's broadcast_targets with network.broadcast unset: 255.255.255.255,
then each local network's broadcast address; returns their count */
int ae_lan_broadcast_targets(unsigned long *targets, int maximum_count);

/* xnet.c's title address (the address a hosted game is joined at) with network.address unset: the default route's
address, unless that is a VPN's (no local network's): then this machine's address on a local network */
unsigned long ae_lan_title_address(unsigned long route_address);

/* a local network address's rank as the one to be joined at: 192.168/16 first, then 10/8, then 172.16/12, then any
other (100.64/10 is a VPN's or a kill switch's more often than a home network's) */
static inline int ae_lan_address_rank(unsigned long address)
{
	const unsigned char *bytes = (const unsigned char *)&address;

	if (bytes[0] == 192 && bytes[1] == 168)
		return 0;
	if (bytes[0] == 10)
		return 1;
	if (bytes[0] == 172 && (bytes[1] & 0xF0) == 16)
		return 2;
	return 3;
}

/* the title address: the route's, if it is a local network's (or there are none), else the best ranked local one */
static inline unsigned long ae_lan_pick_title_address(unsigned long route_address, const unsigned long *own,
	int own_count)
{
	unsigned long best = 0;
	int index, best_rank = 4;

	for (index = 0; index < own_count; index++)
	{
		int rank;

		if (own[index] == route_address)
			return route_address;
		if (own[index] != 0 && (rank = ae_lan_address_rank(own[index])) < best_rank)
		{
			best = own[index];
			best_rank = rank;
		}
	}
	return best ? best : route_address;
}

/* the targets: 255.255.255.255 first, then the networks' addresses, each
once, none 0 or 255.255.255.255 again, up to maximum_count (at least 1) */
static inline int ae_lan_broadcast_merge(const unsigned long *networks, int network_count, unsigned long *targets,
	int maximum_count)
{
	int count = 0, index, seen;

	if (maximum_count < 1)
		return 0;
	targets[count++] = 0xFFFFFFFFUL;
	for (index = 0; index < network_count && count < maximum_count; index++)
	{
		if (networks[index] == 0 || networks[index] == 0xFFFFFFFFUL)
			continue;
		for (seen = 0; seen < count && targets[seen] != networks[index]; seen++)
		{
		}
		if (seen == count)
			targets[count++] = networks[index];
	}
	return count;
}

#endif
