/*
AE_VPN_RULES.C

Which interfaces are a VPN's (ae_vpn_rules.h).
*/

#include <ctype.h>
#include <string.h>

#include "ae_vpn_rules.h"

/* (the overlays: they add a network and leave the machine's own route alone) */
static char const *const overlays[] = { "tailscale", "zerotier", "zt", "ts" };
static char const *const vpn_prefixes[] = { "proton", "pvpn", "wg", "tun", "tap", "utun", "ppp", "nordlynx",
	"mullvad", "ipsec", "vpn", "nord", "expressvpn", "surfshark", "pia" };
static char const *const vpn_words[] = { "wireguard", "wintun", "tap-windows", "openvpn", "vpn", "nordlynx",
	"mullvad" };
static char const *const not_vpn_words[] = { "tailscale", "zerotier", "hyper-v", "virtualbox", "vmware",
	"bluetooth", "loopback" };

static int starts_with(char const *text, char const *word)
{
	size_t length = strlen(word);

	return !strncmp(text, word, length);
}

static void lower(char const *text, char *out, size_t size)
{
	size_t index;

	for (index = 0; text[index] && index + 1 < size; index++)
		out[index] = (char)tolower((unsigned char)text[index]);
	out[index] = 0;
}

int ae_vpn_interface_name(char const *name, int point_to_point)
{
	char text[64];
	size_t index;

	if (!name || !name[0])
		return 0;
	lower(name, text, sizeof(text));
	for (index = 0; index < sizeof(overlays) / sizeof(*overlays); index++)
	{
		if (starts_with(text, overlays[index]))
			return 0;
	}
	for (index = 0; index < sizeof(vpn_prefixes) / sizeof(*vpn_prefixes); index++)
	{
		if (starts_with(text, vpn_prefixes[index]))
			return 1;
	}
	return point_to_point ? 1 : 0;
}

int ae_vpn_adapter_text(char const *text)
{
	char lowered[600];
	size_t index;

	if (!text || !text[0])
		return 0;
	lower(text, lowered, sizeof(lowered));
	for (index = 0; index < sizeof(not_vpn_words) / sizeof(*not_vpn_words); index++)
	{
		if (strstr(lowered, not_vpn_words[index]))
			return 0;
	}
	for (index = 0; index < sizeof(vpn_words) / sizeof(*vpn_words); index++)
	{
		if (strstr(lowered, vpn_words[index]))
			return 1;
	}
	return 0;
}

char const *ae_vpn_label(char const *name)
{
	static const struct { char const *prefix, *label; } labels[] = { { "proton", "ProtonVPN" }, { "pvpn", "ProtonVPN" },
		{ "wg", "WireGuard" }, { "nordlynx", "NordVPN" }, { "nord", "NordVPN" }, { "mullvad", "Mullvad" } };
	char text[64];
	size_t index;

	if (!name)
		return "";
	lower(name, text, sizeof(text));
	for (index = 0; index < sizeof(labels) / sizeof(*labels); index++)
	{
		if (starts_with(text, labels[index].prefix))
			return labels[index].label;
	}
	return name;
}
