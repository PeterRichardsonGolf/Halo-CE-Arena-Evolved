/*
AE_DELTA_WIRE_TEST.C

Arena Evolved's range on ChupathingyCE 0.8's wire (port/linux/src/ae_delta_wire.c;
tools/test_ae_delta_wire.py runs it): what an Arena Evolved host advertises,
what ChupathingyCE's clients (0.8's ranged decision, 0.7.5's exact slot) and
Arena Evolved 0.3.5's (no wire check) make of it, and which hosts an Arena
Evolved client joins.

    ae_delta_wire_test
*/

#include "../src/ae_delta_wire.h"
#include "../src/delta_compat.h"
#include "delta.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(condition) \
	do \
	{ \
		if (!(condition)) \
		{ \
			fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #condition); \
			failures++; \
		} \
	} while (0)

enum { RESERVED = 0x1A };

/* an Arena Evolved host's advertisement (delta_peer_advertised_wire's) */
static void ae_host(unsigned char *reserved)
{
	memset(reserved, 0, RESERVED);
	delta_compat_advertise(reserved, RESERVED, DELTA_MAJOR, DELTA_WIRE);
	ae_delta_wire_narrow(reserved, RESERVED, DELTA_MAJOR, DELTA_WIRE);
}

/* a ChupathingyCE host's (of the wire) */
static void chupa_host(unsigned char *reserved, const char *wire)
{
	memset(reserved, 0, RESERVED);
	delta_compat_advertise(reserved, RESERVED, DELTA_MAJOR, wire);
}

/* a host whose wire is in the old slot alone (ChupathingyCE before the
ranges, Arena Evolved 0.3.5) */
static void slot_host(unsigned char *reserved, const char *wire)
{
	memset(reserved, 0, RESERVED);
	reserved[DELTA_ADVERTISED_WIRE_OFFSET] = DELTA_MAJOR;
	memcpy(reserved + DELTA_ADVERTISED_WIRE_OFFSET + 1, wire, strlen(wire));
}

/* a ChupathingyCE client of wire own */
static int chupa_decides(const unsigned char *reserved, const char *own)
{
	char theirs[24];

	return delta_compat_decide(reserved, RESERVED, DELTA_MAJOR, own, theirs, (int)sizeof(theirs));
}

/* ChupathingyCE 0.7.5a's exact match of the old slot */
static int exact(const unsigned char *reserved, const char *own)
{
	return !strncmp((const char *)reserved + DELTA_ADVERTISED_WIRE_OFFSET + 1, own, DELTA_ADVERTISED_WIRE_SIZE);
}

/* an Arena Evolved client (delta_peer_advertised_wire_matches') */
static int ae_decides(const unsigned char *reserved, char *theirs, int size)
{
	return ae_delta_wire_decide(delta_compat_decide(reserved, RESERVED, DELTA_MAJOR, DELTA_WIRE, theirs, size),
		reserved, RESERVED, DELTA_MAJOR, DELTA_WIRE, theirs, size);
}

int main(void)
{
	unsigned char reserved[RESERVED];
	struct delta_wire_id wire, oldest;
	char theirs[24];

	CHECK(!strcmp(DELTA_WIRE, "chupa-32a"));

	/* the host: its slot and its range the wire alone */
	ae_host(reserved);
	CHECK(reserved[DELTA_ADVERTISED_WIRE_OFFSET] == DELTA_MAJOR);
	CHECK(!strcmp((const char *)reserved + DELTA_ADVERTISED_WIRE_OFFSET + 1, "chupa-32a"));
	CHECK(delta_compat_read_range(reserved, RESERVED, &wire, &oldest));
	CHECK(wire.number == 32 && wire.letter == 'a' && oldest.number == 32 && oldest.letter == 'a');
	/* ... ChupathingyCE 0.8's clients join it; 0.7.5's (exact slot, and its
	0.8.0a range check of 29a) and 0.7.4's never */
	CHECK(chupa_decides(reserved, "chupa-32a") == 1);
	CHECK(chupa_decides(reserved, "chupa-29a") == 0);
	CHECK(chupa_decides(reserved, "chupa-28a") == 0);
	CHECK(!exact(reserved, "chupa-29a") && !exact(reserved, "chupa-28a"));
	/* ... a ChupathingyCE host's own range is left as it is */
	chupa_host(reserved, "chupa-32a");
	CHECK(delta_compat_read_range(reserved, RESERVED, &wire, &oldest) && oldest.number == 28);

	/* the client: a ChupathingyCE 0.8 host (32a, its range from 28a) joined */
	chupa_host(reserved, "chupa-32a");
	CHECK(ae_decides(reserved, theirs, (int)sizeof(theirs)) == 1);
	/* ... another Arena Evolved 0.3.6 host */
	ae_host(reserved);
	CHECK(ae_decides(reserved, theirs, (int)sizeof(theirs)) == 1);
	/* ... Arena Evolved 0.3.5's own wire */
	slot_host(reserved, "ae-32a");
	CHECK(ae_decides(reserved, theirs, (int)sizeof(theirs)) == 1);
	/* ... a ChupathingyCE 0.7.5 host (29a, by its slot or by its range) never:
	its 29 does not play with OpenCE's 30 to 32, which this build plays */
	slot_host(reserved, "chupa-29a");
	CHECK(ae_decides(reserved, theirs, (int)sizeof(theirs)) == 0 && !strcmp(theirs, "chupa-29a"));
	chupa_host(reserved, "chupa-29a");
	CHECK(ae_decides(reserved, theirs, (int)sizeof(theirs)) == 0 && !strcmp(theirs, "chupa-29a"));
	chupa_host(reserved, "chupa-28a");
	CHECK(ae_decides(reserved, theirs, (int)sizeof(theirs)) == 0 && !strcmp(theirs, "chupa-28a"));
	/* ... a newer wire whose range still has 32a joined; one that has not, not */
	chupa_host(reserved, "chupa-33a");
	CHECK(ae_decides(reserved, theirs, (int)sizeof(theirs)) == 0);
	reserved[DELTA_ADVERTISED_RANGE_OFFSET + 3] = 32;
	reserved[DELTA_ADVERTISED_RANGE_OFFSET + 4] = 'a';
	reserved[DELTA_ADVERTISED_RANGE_OFFSET + 5] = (unsigned char)(0xA5 ^ reserved[DELTA_ADVERTISED_RANGE_OFFSET] ^
		reserved[DELTA_ADVERTISED_RANGE_OFFSET + 1] ^ reserved[DELTA_ADVERTISED_RANGE_OFFSET + 2] ^
		reserved[DELTA_ADVERTISED_RANGE_OFFSET + 3] ^ reserved[DELTA_ADVERTISED_RANGE_OFFSET + 4]);
	CHECK(ae_decides(reserved, theirs, (int)sizeof(theirs)) == 1);
	/* ... a host that says nothing of a wire: the numbers decide */
	memset(reserved, 0, RESERVED);
	CHECK(ae_decides(reserved, theirs, (int)sizeof(theirs)) == -1);

	if (failures)
	{
		printf("ae_delta_wire_test: %d failed\n", failures);
		return 1;
	}
	printf("ae_delta_wire_test: ok\n");
	return 0;
}
