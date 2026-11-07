/* ae_vpn_rules.c: which network interfaces are a VPN's (CREATE GAME's warning when hosting online) */
#include <stdio.h>
#include <string.h>

#include "ae_vpn_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	/* Linux and macOS names */
	CHECK(ae_vpn_interface_name("proton0", 0));
	CHECK(ae_vpn_interface_name("pvpnksintrf0", 0));
	CHECK(ae_vpn_interface_name("wg0", 0));
	CHECK(ae_vpn_interface_name("tun0", 0));
	CHECK(ae_vpn_interface_name("utun3", 0));
	CHECK(ae_vpn_interface_name("nordlynx", 0));
	CHECK(ae_vpn_interface_name("some-tunnel", 1));
	CHECK(!ae_vpn_interface_name("tailscale0", 1));
	CHECK(!ae_vpn_interface_name("zt5u4y6abc", 1));
	CHECK(!ae_vpn_interface_name("eth0", 0));
	CHECK(!ae_vpn_interface_name("wlan0", 0));
	CHECK(!ae_vpn_interface_name("enp3s0", 0));
	CHECK(!ae_vpn_interface_name("docker0", 0));
	CHECK(!ae_vpn_interface_name("", 1));
	CHECK(!ae_vpn_interface_name(NULL, 1));
	/* Windows adapters */
	CHECK(ae_vpn_adapter_text("WireGuard Tunnel"));
	CHECK(ae_vpn_adapter_text("ProtonVPN"));
	CHECK(ae_vpn_adapter_text("TAP-Windows Adapter V9"));
	CHECK(ae_vpn_adapter_text("Wintun Userspace Tunnel"));
	CHECK(ae_vpn_adapter_text("NordLynx Tunnel"));
	CHECK(!ae_vpn_adapter_text("Tailscale Tunnel"));
	CHECK(!ae_vpn_adapter_text("ZeroTier Virtual Port"));
	CHECK(!ae_vpn_adapter_text("Hyper-V Virtual Ethernet Adapter"));
	CHECK(!ae_vpn_adapter_text("Intel(R) Wi-Fi 6E AX211 160MHz"));
	CHECK(!ae_vpn_adapter_text("Realtek PCIe 2.5GbE Family Controller"));
	CHECK(!ae_vpn_adapter_text(""));
	/* the names a player knows */
	CHECK(!strcmp(ae_vpn_label("pvpnksintrf0"), "ProtonVPN"));
	CHECK(!strcmp(ae_vpn_label("proton0"), "ProtonVPN"));
	CHECK(!strcmp(ae_vpn_label("wg0"), "WireGuard"));
	CHECK(!strcmp(ae_vpn_label("nordlynx"), "NordVPN"));
	CHECK(!strcmp(ae_vpn_label("tun0"), "tun0"));
	CHECK(!strcmp(ae_vpn_label(NULL), ""));
	if (failures)
		return 1;
	printf("ae_vpn_rules_test: ok\n");
	return 0;
}
