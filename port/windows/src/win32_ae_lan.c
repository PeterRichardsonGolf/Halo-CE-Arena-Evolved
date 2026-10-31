/*
WIN32_AE_LAN.C

Windows' local networks for a system link game behind a VPN
(port/linux/src/ae_lan_broadcast.h): each IPv4 address of an Ethernet or
Wi-Fi adapter that is up, and its broadcast address (address | ~mask). A
VPN's adapter (WireGuard's and Wintun's, as ProtonVPN's) is another type and
left out; an OpenVPN TAP adapter calls itself Ethernet, and its network's
broadcasts go into its tunnel, which costs nothing.
*/

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <stdlib.h>

#include "ae_lan_broadcast.h"

int ae_lan_networks(unsigned long *own, unsigned long *broadcasts, int maximum_count)
{
	ULONG size = 16 * 1024;
	IP_ADAPTER_ADDRESSES *adapters = NULL, *adapter;
	ULONG status;
	int count = 0;

	do
	{
		free(adapters);
		adapters = malloc(size);
		if (!adapters)
			return 0;
		status = GetAdaptersAddresses(AF_INET,
			GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER, NULL, adapters, &size);
	}
	while (status == ERROR_BUFFER_OVERFLOW);
	if (status == NO_ERROR)
	{
		for (adapter = adapters; adapter && count < maximum_count; adapter = adapter->Next)
		{
			IP_ADAPTER_UNICAST_ADDRESS *address;

			if (adapter->OperStatus != IfOperStatusUp ||
				(adapter->IfType != IF_TYPE_ETHERNET_CSMACD && adapter->IfType != IF_TYPE_IEEE80211))
			{
				continue;
			}
			for (address = adapter->FirstUnicastAddress; address && count < maximum_count; address = address->Next)
			{
				struct sockaddr_in *ipv4 = (struct sockaddr_in *)address->Address.lpSockaddr;
				unsigned long host, mask;

				/* (a /31 or /32 has no broadcast address) */
				if (ipv4->sin_family != AF_INET || address->OnLinkPrefixLength < 1 || address->OnLinkPrefixLength > 30)
					continue;
				host = ntohl(ipv4->sin_addr.s_addr);
				/* (169.254/16: a network with no DHCP answer, no one to play with) */
				if ((host >> 16) == 0xA9FE)
					continue;
				mask = 0xFFFFFFFFUL << (32 - address->OnLinkPrefixLength);
				own[count] = ipv4->sin_addr.s_addr;
				broadcasts[count++] = htonl((host | ~mask) & 0xFFFFFFFFUL);
			}
		}
	}
	free(adapters);
	return count;
}
