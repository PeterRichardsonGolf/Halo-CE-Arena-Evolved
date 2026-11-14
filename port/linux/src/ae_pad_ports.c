/*
AE_PAD_PORTS.C

The controller ports' pads (ae_pad_ports.h). No SDL here: ae_platform.c hands
in the pads' ids and states (port/linux/tests/ae_pad_ports_test.c).
*/

#include "ae_pad_ports.h"

static int listed_index(
	unsigned long const *ids,
	int count,
	unsigned long id)
{
	int index;

	for (index = 0; index < count; index++)
	{
		if (ids[index] == id)
			return index;
	}
	return -1;
}

static int port_of(
	unsigned long const *port_ids,
	int ports,
	unsigned long id)
{
	int port;

	for (port = 0; port < ports; port++)
	{
		if (port_ids[port] == id)
			return port;
	}
	return -1;
}

static int pad_rests(
	unsigned long const *ids,
	unsigned char const *at_rest,
	int count,
	unsigned long id)
{
	int index = listed_index(ids, count, id);

	return index >= 0 && at_rest && at_rest[index];
}

int ae_pad_ports_assign(
	unsigned long *port_ids,
	int ports,
	unsigned long const *ids,
	unsigned char const *at_rest,
	int count,
	int split,
	int lone_first)
{
	int changes = 0, index, port, used = 0;

	if (!port_ids || ports <= 0)
		return 0;
	if (ports > AE_PAD_PORTS_MAXIMUM)
		ports = AE_PAD_PORTS_MAXIMUM;
	if (!ids || count < 0)
		count = 0;
	/* a pad gone frees its port; the others keep theirs */
	for (port = 0; port < ports; port++)
	{
		if (port_ids[port] && listed_index(ids, count, port_ids[port]) < 0)
			port_ids[port] = 0;
	}
	/* a new pad: the first free port (while split, port 1 last) */
	for (index = 0; index < count; index++)
	{
		int order;

		if (!ids[index] || port_of(port_ids, ports, ids[index]) >= 0)
			continue;
		for (order = 0; order < ports; order++)
		{
			port = split ? (order + 1) % ports : order;
			if (!port_ids[port])
				break;
		}
		if (order == ports)
			continue;
		port_ids[port] = ids[index];
		changes++;
	}
	for (port = 0; port < ports; port++)
		used += port_ids[port] != 0;
	/* one player: port 1 gets the lowest port's pad, at rest */
	if (!split && used && !port_ids[0])
	{
		for (port = 1; port < ports && !port_ids[port]; port++)
			;
		if (port < ports && pad_rests(ids, at_rest, count, port_ids[port]))
		{
			port_ids[0] = port_ids[port];
			port_ids[port] = 0;
			changes++;
		}
	}
	/* split with one pad on port 1: port 2, at rest */
	if (split && !lone_first && used == 1 && port_ids[0] && ports > 1 && pad_rests(ids, at_rest, count, port_ids[0]))
	{
		port_ids[1] = port_ids[0];
		port_ids[0] = 0;
		changes++;
	}
	return changes;
}
