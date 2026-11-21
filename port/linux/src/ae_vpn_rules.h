/*
AE_VPN_RULES.H

(0.7.1, owner 2026-10-31) Whether a network interface is a VPN's that takes a hosted game's traffic: a host on
ProtonVPN, WireGuard, OpenVPN and the like is not reached by the players who join it over the internet (a
VPN's tunnel answers for it), so CREATE GAME warns when it hosts online with one up (ae_lan_vpn, ae_lan_networks.c
and win32_ae_lan.c). Overlay networks that keep the machine's own route (Tailscale, ZeroTier) are not VPNs here.
Plain C: tools/test_ae_units.py builds it with port/linux/tests/ae_vpn_rules_test.c.
*/

#ifndef __AE_VPN_RULES_H
#define __AE_VPN_RULES_H

/* a Linux or macOS interface's name (proton0, wg0, tun0, tap0, utun3, ppp0, nordlynx, pvpnksintrf0, ...): 1 a VPN's,
0 not (tailscale0, zt..., eth0, wlan0, lo, docker0, ...). point_to_point: the interface's IFF_POINTOPOINT flag, which
alone makes a VPN's (but for the overlays) */
int ae_vpn_interface_name(char const *name, int point_to_point);

/* a Windows adapter's description or friendly name (UTF-8, any case): 1 a VPN's ("WireGuard Tunnel", "Wintun",
"TAP-Windows Adapter V9", "ProtonVPN", "NordLynx", any "VPN"), 0 not (Tailscale, ZeroTier, Hyper-V, VirtualBox,
VMware, Bluetooth, Ethernet and Wi-Fi adapters) */
int ae_vpn_adapter_text(char const *text);

/* the name a player knows for a Linux or macOS VPN interface: "ProtonVPN" (proton*, pvpn*), "WireGuard" (wg*),
"NordVPN" (nordlynx, nord*), "Mullvad" (mullvad*); any other, its own name */
char const *ae_vpn_label(char const *name);

#endif
