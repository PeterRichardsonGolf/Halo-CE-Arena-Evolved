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
	struct delta_wire_id own, host, oldest;
	unsigned char *range;
	char text[16];

	if (size < DELTA_ADVERTISED_RANGE_OFFSET + DELTA_ADVERTISED_RANGE_SIZE ||
		reserved[DELTA_ADVERTISED_WIRE_OFFSET] != (unsigned char)major ||
		!delta_wire_parse(wire, (int)strlen(wire), &own) ||
		!delta_compat_read_range(reserved, size, &host, &oldest) || delta_wire_compare(&host, &own))
	{
		return;
	}
	/* (the old slot: the wire itself) */
	delta_wire_format(&own, text, (int)sizeof(text));
	memset(reserved + DELTA_ADVERTISED_WIRE_OFFSET + 1, 0, DELTA_ADVERTISED_WIRE_SIZE);
	memcpy(reserved + DELTA_ADVERTISED_WIRE_OFFSET + 1, text, strlen(text));
	/* (the range: the wire to the wire) */
	range = reserved + DELTA_ADVERTISED_RANGE_OFFSET;
	range[3] = (unsigned char)own.number;
	range[4] = (unsigned char)own.letter;
	range[5] = range_check(range);
}

int ae_delta_wire_decide(int decided, const unsigned char *reserved, int size, int major, const char *own,
	char *theirs, int theirs_size)
{
	struct delta_wire_id mine, host, host_oldest;

	if (decided == 0 && !strcmp(theirs, AE_DELTA_WIRE_0_3_5))
		return 1;
	if (decided != 1 || !delta_wire_parse(own, (int)strlen(own), &mine))
		return decided;
	/* (joined by delta_compat_decide: also by this build's own range alone?
	the host's range has this wire, or its slot is this wire) */
	if (delta_compat_read_range(reserved, size, &host, &host_oldest))
	{
		if (delta_wire_compare(&mine, &host_oldest) >= 0 && delta_wire_compare(&mine, &host) <= 0)
			return 1;
	}
	else if (delta_compat_host_wire(reserved, size, major, &host) && !delta_wire_compare(&host, &mine))
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
