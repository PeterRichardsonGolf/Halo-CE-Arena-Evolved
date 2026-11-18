#include "harness.h"
#include <math.h>
/* JSON numbers kept in range before they become integers (master_list.c's network_version, server_http.c's
WebSocket "after"): the lines as the sources have them run on edge values; a value outside the integer's range,
cast as it is, is undefined behaviour in C */

static unsigned long latest = 500;
static unsigned long read_from;
static unsigned long server_events_latest(void) { return latest; }
static void server_events_read(unsigned long after, int size, int addresses, int one) { read_from = after; }
enum { SERVER_EVENTS_RING_SIZE = 4096 };
static int ws_addresses(void *connection) { return 0; }
static int ws_replay_one;

static double version_kept(double version)
{
#include "version_line.inc"
	return version;
}

static unsigned long after_read(double after)
{
	void *connection = NULL;
	read_from = 12345;
#include "after_line.inc"
	SERVER_EVENTS_RING_SIZE, 0, 0);
	return read_from;
}

int main(int argc, char **argv)
{
	char const *case_name = argv[1];
	double values[] = { 1e300, -1.0, -1e300, 2147483648.0, NAN, INFINITY };
	int index;

	CASE("version")
	{
		CHECK(version_kept(44) == 44, "a real version changed");
		CHECK(version_kept(0) == 0, "0 changed");
		for (index = 0; index < (int)(sizeof(values) / sizeof(values[0])); index++)
			CHECK(version_kept(values[index]) >= 0 && version_kept(values[index]) <= 2147483647.0,
				"version %g kept out of int range", values[index]);
		return 0;
	}
	CASE("after")
	{
		CHECK(after_read(10) == 10, "a cursor within the ring changed");
		CHECK(after_read(500) == 500, "the latest changed");
		CHECK(after_read(1e30) == 0, "a cursor past the latest not from the start");
		CHECK(after_read(18446744073709551616.0) == 0, "2^64 not from the start");
		return 0;
	}
	fprintf(stderr, "unknown case %s\n", case_name);
	return 2;
}
