/*
AE_NET_FLOW_RULES.C

See ae_net_flow_rules.h.
*/

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "ae_net_flow_rules.h"

long ae_flow_pick(struct ae_flow_slot *slots, long count, unsigned long address, unsigned short port, int prefer_remote)
{
	long index, found = AE_FLOW_NONE, at = 0, remote = 0, unbound = 0, unbound_index = AE_FLOW_NONE;
	long loaded_unbound = 0, loaded_index = AE_FLOW_NONE;

	if (!address)
		return AE_FLOW_NONE;
	for (index = 0; index < count; index++)
	{
		if (!slots[index].joined || slots[index].address != address)
			continue;
		at++;
		if (!slots[index].local)
			remote++;
		if (port && slots[index].port == port && (found == AE_FLOW_NONE || (prefer_remote && slots[found].local)))
			found = index;
	}
	if (found != AE_FLOW_NONE)
		return found;
	if (!at)
		return AE_FLOW_NONE;
	/* (the machines to choose among) */
	for (index = 0; index < count; index++)
	{
		if (!slots[index].joined || slots[index].address != address || (prefer_remote && remote && slots[index].local))
			continue;
		if (!slots[index].port)
		{
			unbound++;
			unbound_index = index;
			if (slots[index].loaded)
			{
				loaded_unbound++;
				loaded_index = index;
			}
		}
		if (found == AE_FLOW_NONE)
			found = index;
	}
	if (prefer_remote && remote)
		at = remote;
	if (at == 1)
	{
		/* (a lone machine of the address: any port is its, the last kept; but one whose port a bind said keeps it: a
		datagram from another port is not taken for its) */
		if (slots[found].bound)
			return slots[found].port == port ? found : AE_FLOW_NONE;
		if (port)
			slots[found].port = port;
		return found;
	}
	/* (one not known yet; or one of those not known that has loaded, which is where datagrams begin) */
	if (port && (unbound == 1 || loaded_unbound == 1))
	{
		long chosen = unbound == 1 ? unbound_index : loaded_index;

		slots[chosen].port = port;
		return chosen;
	}
	return AE_FLOW_NONE;
}

long ae_flow_bind(struct ae_flow_slot *slots, long count, unsigned long address, unsigned short port,
	unsigned long token)
{
	long index, found = AE_FLOW_NONE;

	if (!token || !address || !port)
		return AE_FLOW_NONE;
	for (index = 0; index < count && found == AE_FLOW_NONE; index++)
	{
		if (slots[index].joined && slots[index].address == address && slots[index].token == token)
			found = index;
	}
	if (found == AE_FLOW_NONE)
		return AE_FLOW_NONE;
	for (index = 0; index < count; index++)
	{
		if (index != found && slots[index].address == address && slots[index].port == port)
		{
			slots[index].port = 0;
			slots[index].bound = 0;
		}
	}
	slots[found].port = port;
	slots[found].bound = 1;
	return found;
}

int ae_flow_same_machine(char const *hardware_id, char const *other_hardware_id)
{
	/* (an id not told says nothing: a machine whose join request carries none, or one of only characters the host
	leaves out, is not taken for every other machine; the old connection is found dead on its own) */
	if (!hardware_id || !hardware_id[0] || !other_hardware_id || !other_hardware_id[0])
		return 0;
	return !strcmp(hardware_id, other_hardware_id);
}

static int parse_ipv4(char const *text, unsigned long *address, size_t *used)
{
	unsigned long value = 0;
	int part;
	char const *cursor = text;

	for (part = 0; part < 4; part++)
	{
		char *end;
		unsigned long number;

		if (!isdigit((unsigned char)*cursor))
			return 0;
		number = strtoul(cursor, &end, 10);
		if (number > 255 || end - cursor > 3)
			return 0;
		value = (value << 8) | number;
		cursor = end;
		if (part < 3)
		{
			if (*cursor != '.')
				return 0;
			cursor++;
		}
	}
	*address = value;
	*used = (size_t)(cursor - text);
	return 1;
}

int ae_flow_address_is_tunnel(char const *setting, int dedicated, unsigned long address, unsigned long const *own,
	long own_count)
{
	char const *cursor = setting;
	long index;

	if (!address || !setting)
		return 0;
	while (*cursor)
	{
		unsigned long listed;
		size_t used;

		while (*cursor == ',' || isspace((unsigned char)*cursor))
			cursor++;
		if (!*cursor)
			break;
		if (!strncmp(cursor, "auto", 4) && (!cursor[4] || cursor[4] == ',' || isspace((unsigned char)cursor[4])))
		{
			if (dedicated)
			{
				if (address == 0x7F000001UL)
					return 1;
				for (index = 0; own && index < own_count; index++)
				{
					if (own[index] == address)
						return 1;
				}
			}
			cursor += 4;
		}
		else if (parse_ipv4(cursor, &listed, &used))
		{
			if (listed == address)
				return 1;
			cursor += used;
		}
		else
		{
			/* (what is not an address: skipped to the next separator) */
			while (*cursor && *cursor != ',' && !isspace((unsigned char)*cursor))
				cursor++;
		}
	}
	return 0;
}
