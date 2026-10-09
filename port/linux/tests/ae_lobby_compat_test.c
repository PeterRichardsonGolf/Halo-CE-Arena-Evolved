#include <stdio.h>
#include "../include/halo_port_limits.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	unsigned int flag = HALO_PORT_ADVERTISED_DISTRIBUTED_FLAG;
	unsigned int minimum = HALO_PORT_NETWORK_VERSION_MINIMUM, maximum = HALO_PORT_NETWORK_VERSION_MAXIMUM;
	unsigned int floor = HALO_PORT_NETWORK_VERSION, version;

	/* a host below this build's version (the published 0.1.0-beta is 20) is refused, though inside the range */
	CHECK(floor == 24 && minimum <= 20 && 20 <= maximum);
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
	if (!failures)
		printf("ae_lobby_compat_test: ok\n");
	return failures ? 1 : 0;
}
