#include <stdio.h>
#include <string.h>
#include "../include/halo_port_limits.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	unsigned int flag = HALO_PORT_ADVERTISED_DISTRIBUTED_FLAG;
	unsigned int minimum = HALO_PORT_NETWORK_VERSION_MINIMUM, maximum = HALO_PORT_NETWORK_VERSION_MAXIMUM;
	unsigned int floor = HALO_PORT_NETWORK_VERSION, version;

	/* a host below this build's version (the published 0.2.0-beta and ChupathingyCE 0.7.1e are 24, 0.1.0-beta
	20) is refused, though inside the range */
	CHECK(floor == 25 && minimum <= 20 && 24 <= maximum);
	for (version = 0; version < floor; version++)
		CHECK(!halo_port_advertised_joinable(version, flag, minimum, maximum));
	/* this build's own version and a newer one inside the range: joined; beyond the range, or off the distributed
	netcode: not */
	CHECK(halo_port_advertised_joinable(floor, flag, minimum, maximum));
	CHECK(halo_port_advertised_joinable(floor, flag | 0x02, minimum, maximum));
	CHECK(!halo_port_advertised_joinable(floor, 0, minimum, maximum));
	CHECK(!halo_port_advertised_joinable(maximum + 1, flag, minimum, maximum));
	CHECK(halo_port_advertised_joinable(floor + 1, flag, minimum, floor + 1));
	/* a legacy table's wider minimum cannot lift a host under the floor */
	CHECK(!halo_port_advertised_joinable(floor - 1, flag, floor - 1, maximum));
	/* the game lists' rows: joined exactly where the test above joins, and the reason otherwise (a row is dimmed and
	says it: browser_screen.c, menu_functions.c) */
	for (version = 0; version <= maximum + 3; version++)
	{
		unsigned int flags;

		for (flags = 0; flags < 4; flags++)
		{
			int state = halo_port_advertised_join_state(version, flags, minimum, maximum);

			CHECK((state == HALO_PORT_JOIN_OK) == (halo_port_advertised_joinable(version, flags, minimum, maximum) != 0));
			CHECK((halo_port_join_reason_format(state) == NULL) == (state == HALO_PORT_JOIN_OK));
			if (version < floor)
				CHECK(state == HALO_PORT_JOIN_HOST_OLDER);
			else if (version > maximum)
				CHECK(state == HALO_PORT_JOIN_HOST_NEWER);
			else
				CHECK(state == ((flags & flag) ? HALO_PORT_JOIN_OK : HALO_PORT_JOIN_HOST_LOCKSTEP));
		}
	}
	/* (the published 0.2.0-beta and ChupathingyCE 0.7.1e, 24: the host updates; a host one past this build: this game) */
	{
		char text[64];

		CHECK(halo_port_advertised_join_state(24, flag, minimum, maximum) == HALO_PORT_JOIN_HOST_OLDER);
		snprintf(text, sizeof(text), halo_port_join_reason_format(HALO_PORT_JOIN_HOST_OLDER), 24u);
		CHECK(!strcmp(text, "HOST NEEDS TO UPDATE (VERSION 24)"));
		snprintf(text, sizeof(text), halo_port_join_reason_format(HALO_PORT_JOIN_HOST_NEWER), maximum + 1);
		CHECK(!strncmp(text, "UPDATE THIS GAME TO JOIN", 24));
		/* a legacy table's wider range: a host inside it and under the floor still must update, one over it is newer */
		CHECK(halo_port_advertised_join_state(floor - 1, flag, floor - 1, floor + 1) == HALO_PORT_JOIN_HOST_OLDER);
		CHECK(halo_port_advertised_join_state(floor + 2, flag, floor - 1, floor + 1) == HALO_PORT_JOIN_HOST_NEWER);
	}
	if (!failures)
		printf("ae_lobby_compat_test: ok\n");
	return failures ? 1 : 0;
}
