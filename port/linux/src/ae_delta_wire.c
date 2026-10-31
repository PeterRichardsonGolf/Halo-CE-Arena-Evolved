/*
AE_DELTA_WIRE.C

Arena Evolved's range on ChupathingyCE 0.8's Delta wire (ae_delta_wire.h).
*/

#include "ae_delta_wire.h"
#include "delta.h"
#include "delta_compat.h"

#include <string.h>

/* (delta_compat.c's check byte of the range bytes: 0xA5 xor the five before) */
static unsigned char range_check(const unsigned char *range)
{
	return (unsigned char)(0xA5 ^ range[0] ^ range[1] ^ range[2] ^ range[3] ^ range[4]);
}

void ae_delta_wire_narrow(unsigned char *reserved, int size, int major, const char *wire)
{
	struct delta_wire_id own, host, oldest, first;
	unsigned char *range;
	char text[16];

	if (size < DELTA_ADVERTISED_RANGE_OFFSET + DELTA_ADVERTISED_RANGE_SIZE ||
		reserved[DELTA_ADVERTISED_WIRE_OFFSET] != (unsigned char)major ||
		!delta_wire_parse(wire, (int)strlen(wire), &own) ||
		!delta_wire_parse(AE_DELTA_WIRE_OLDEST, (int)strlen(AE_DELTA_WIRE_OLDEST), &first) ||
		!delta_compat_read_range(reserved, size, &host, &oldest) || delta_wire_compare(&host, &own))
	{
		return;
	}
	/* (the old slot: the oldest wire, ChupathingyCE 0.8's) */
	delta_wire_format(&first, text, (int)sizeof(text));
	memset(reserved + DELTA_ADVERTISED_WIRE_OFFSET + 1, 0, DELTA_ADVERTISED_WIRE_SIZE);
	memcpy(reserved + DELTA_ADVERTISED_WIRE_OFFSET + 1, text, strlen(text));
	/* (the range: the oldest to the wire) */
	range = reserved + DELTA_ADVERTISED_RANGE_OFFSET;
	range[3] = (unsigned char)first.number;
	range[4] = (unsigned char)first.letter;
	range[5] = range_check(range);
}

int ae_delta_wire_decide(int decided, const unsigned char *reserved, int size, int major, const char *own,
	char *theirs, int theirs_size)
{
	struct delta_wire_id mine, first, chupa_38, host, host_oldest;

	if (decided < 0 || !delta_wire_parse(own, (int)strlen(own), &mine) ||
		!delta_wire_parse(AE_DELTA_WIRE_OLDEST, (int)strlen(AE_DELTA_WIRE_OLDEST), &first) ||
		!delta_wire_parse(AE_DELTA_WIRE_CHUPA_38, (int)strlen(AE_DELTA_WIRE_CHUPA_38), &chupa_38))
	{
		return decided;
	}
	/* (a host whose range has this build's wire; or one of ChupathingyCE 0.8's wires, the oldest or 0.8.0h's, played
	by its network version) */
	if (delta_compat_read_range(reserved, size, &host, &host_oldest))
	{
		if ((delta_wire_compare(&mine, &host_oldest) >= 0 && delta_wire_compare(&mine, &host) <= 0) ||
			!delta_wire_compare(&host, &first) || !delta_wire_compare(&host, &chupa_38))
		{
			return 1;
		}
	}
	else if (delta_compat_host_wire(reserved, size, major, &host) &&
		(!delta_wire_compare(&host, &mine) || !delta_wire_compare(&host, &first) || !delta_wire_compare(&host, &chupa_38)))
	{
		return 1;
	}
	else
	{
		host.number = 0;
	}
	if (theirs_size > 0)
	{
		theirs[0] = 0;
		if (host.number && theirs_size >= 16)
			delta_wire_format(&host, theirs, theirs_size);
		else if (delta_compat_host_wire(reserved, size, major, &host) && theirs_size >= 16)
			delta_wire_format(&host, theirs, theirs_size);
	}
	return 0;
}

int ae_delta_wire_advertised(const unsigned char *reserved, int size)
{
	/* (the flags are the byte before the major: HALO_PORT_ADVERTISED_FLAGS_OFFSET) */
	int flags = DELTA_ADVERTISED_WIRE_OFFSET - 1, name = DELTA_ADVERTISED_WIRE_OFFSET + 1;

	if (!reserved || size <= name)
		return 0;
	return (reserved[flags] & DELTA_ADVERTISED_FLAG) && reserved[DELTA_ADVERTISED_WIRE_OFFSET] &&
		reserved[name] >= 'a' && reserved[name] <= 'z';
}
