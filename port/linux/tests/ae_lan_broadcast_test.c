/* ae_lan_broadcast.h's merge and title address pick. The merge: 255.255.255.255 first, each local network once, none 0 or a second 255.255.255.255,
never past the room given (tools/test_ae_units.py) */
#include <stdio.h>
#include "ae_lan_broadcast.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	unsigned long targets[4] = { 7, 7, 7, 7 };
	unsigned long none[1] = { 0 };
	unsigned long lan[5] = { 0x0A0A0B0AUL, 0, 0xFFFFFFFFUL, 0x0A0A0B0AUL, 0x0B0B0C0CUL };

	/* no network: the plain broadcast alone (as before AE's change) */
	CHECK(ae_lan_broadcast_merge(none, 0, targets, 4) == 1 && targets[0] == 0xFFFFFFFFUL && targets[1] == 7);
	/* a network: after it; 0, 255.255.255.255 and a second copy left out */
	CHECK(ae_lan_broadcast_merge(lan, 5, targets, 4) == 3);
	CHECK(targets[0] == 0xFFFFFFFFUL && targets[1] == 0x0A0A0B0AUL && targets[2] == 0x0B0B0C0CUL && targets[3] == 7);
	/* the room given: one, and none */
	targets[1] = 7;
	CHECK(ae_lan_broadcast_merge(lan, 5, targets, 1) == 1 && targets[0] == 0xFFFFFFFFUL && targets[1] == 7);
	CHECK(ae_lan_broadcast_merge(lan, 5, targets, 0) == 0);
	{
		/* the title address, network byte order as the machine stores it (first octet in the first byte) */
		unsigned long wlan = 0, killswitch = 0, docker = 0, tunnel = 0;
		unsigned char *b;
		unsigned long own[3];

		b = (unsigned char *)&wlan; b[0] = 192; b[1] = 168; b[2] = 11; b[3] = 208;
		b = (unsigned char *)&killswitch; b[0] = 100; b[1] = 85; b[2] = 0; b[3] = 1;
		b = (unsigned char *)&docker; b[0] = 172; b[1] = 17; b[2] = 0; b[3] = 1;
		b = (unsigned char *)&tunnel; b[0] = 10; b[1] = 2; b[2] = 0; b[3] = 2;
		own[0] = killswitch; own[1] = wlan; own[2] = docker;
		/* the route's address is a local network's: kept */
		CHECK(ae_lan_pick_title_address(wlan, own, 3) == wlan);
		/* a VPN's tunnel (no local network's): the home network's, before the kill switch's and docker's */
		CHECK(ae_lan_pick_title_address(tunnel, own, 3) == wlan);
		own[1] = 0;
		CHECK(ae_lan_pick_title_address(tunnel, own, 3) == docker);
		/* no local network known (the web, Android): the route's */
		CHECK(ae_lan_pick_title_address(tunnel, own, 0) == tunnel);
		CHECK(ae_lan_address_rank(wlan) == 0 && ae_lan_address_rank(tunnel) == 1 && ae_lan_address_rank(docker) == 2 &&
			ae_lan_address_rank(killswitch) == 3);
	}
	if (failures)
		return 1;
	printf("ae_lan_broadcast: all checks passed\n");
	return 0;
}
