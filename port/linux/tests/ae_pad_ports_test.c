#include <stdio.h>
#include <string.h>
#include "ae_pad_ports.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static unsigned long ports[4];
static const unsigned char rest[8] = { 1, 1, 1, 1, 1, 1, 1, 1 };
static const unsigned char busy[8] = { 0 };

static void assign(unsigned long const *ids, int count, int split)
{
	ae_pad_ports_assign(ports, 4, ids, rest, count, split, 0);
}

int main(void)
{
	/* pads in connection order take ports 1, 2, 3 */
	{
		static const unsigned long ids[] = { 11, 12, 13 };

		memset(ports, 0, sizeof(ports));
		assign(ids, 3, 0);
		CHECK(ports[0] == 11 && ports[1] == 12 && ports[2] == 13 && ports[3] == 0);
	}
	/* the LAN party: a Switch Pro controller appears late and SDL lists it first; nobody moves, it takes port 4 */
	{
		static const unsigned long ids[] = { 14, 11, 12, 13 };

		assign(ids, 4, 1);
		CHECK(ports[0] == 11 && ports[1] == 12 && ports[2] == 13 && ports[3] == 14);
	}
	/* port 2's pad unplugged: the others keep their ports; plugged in again (a new id), it takes port 2 back */
	{
		static const unsigned long gone[] = { 14, 11, 13 };
		static const unsigned long back[] = { 21, 14, 11, 13 };

		assign(gone, 3, 1);
		CHECK(ports[0] == 11 && ports[1] == 0 && ports[2] == 13 && ports[3] == 14);
		assign(back, 4, 1);
		CHECK(ports[0] == 11 && ports[1] == 21 && ports[2] == 13 && ports[3] == 14);
	}
	/* a fifth pad: no port, nothing moves */
	{
		static const unsigned long ids[] = { 31, 21, 14, 11, 13 };

		CHECK(ae_pad_ports_assign(ports, 4, ids, rest, 5, 1, 0) == 0);
		CHECK(ports[0] == 11 && ports[1] == 21 && ports[2] == 13 && ports[3] == 14);
	}
	/* split: the keyboard's player has port 1, a new pad takes port 2, port 1 only when the rest are taken */
	{
		static const unsigned long one[] = { 41 };
		static const unsigned long two[] = { 42, 41 };

		memset(ports, 0, sizeof(ports));
		ports[1] = 40;
		{
			static const unsigned long first[] = { 40 };

			assign(first, 1, 1);
		}
		CHECK(ports[1] == 40);
		memset(ports, 0, sizeof(ports));
		assign(one, 1, 1);
		CHECK(ports[0] == 0 && ports[1] == 41);
		assign(two, 2, 1);
		CHECK(ports[0] == 0 && ports[1] == 41 && ports[2] == 42);
	}
	/* one player: the lone pad left on port 3 goes to port 1, only at rest */
	{
		static const unsigned long ids[] = { 51 };

		memset(ports, 0, sizeof(ports));
		ports[2] = 51;
		CHECK(ae_pad_ports_assign(ports, 4, ids, busy, 1, 0, 0) == 0);
		CHECK(ports[0] == 0 && ports[2] == 51);
		CHECK(ae_pad_ports_assign(ports, 4, ids, rest, 1, 0, 0) == 1);
		CHECK(ports[0] == 51 && ports[2] == 0);
	}
	/* one player with two pads on ports 2 and 4: port 2's goes to port 1, port 4's stays */
	{
		static const unsigned long ids[] = { 61, 62 };

		memset(ports, 0, sizeof(ports));
		ports[1] = 61;
		ports[3] = 62;
		assign(ids, 2, 0);
		CHECK(ports[0] == 61 && ports[1] == 0 && ports[3] == 62);
	}
	/* split with one pad on port 1: it becomes port 2's at rest; not with lone_first; back to port 1 when one player */
	{
		static const unsigned long ids[] = { 71 };

		memset(ports, 0, sizeof(ports));
		ports[0] = 71;
		CHECK(ae_pad_ports_assign(ports, 4, ids, rest, 1, 1, 1) == 0);
		CHECK(ports[0] == 71);
		CHECK(ae_pad_ports_assign(ports, 4, ids, busy, 1, 1, 0) == 0);
		CHECK(ports[0] == 71);
		assign(ids, 1, 1);
		CHECK(ports[0] == 0 && ports[1] == 71);
		assign(ids, 1, 0);
		CHECK(ports[0] == 71 && ports[1] == 0);
	}
	/* two pads on ports 1 and 2, then a second player joins (split on): nobody moves, player 1 keeps port 1's pad (only
	a lone pad goes to port 2, the keyboard's player being 1) */
	{
		static const unsigned long ids[] = { 91, 92 };

		memset(ports, 0, sizeof(ports));
		assign(ids, 2, 0);
		CHECK(ports[0] == 91 && ports[1] == 92);
		CHECK(ae_pad_ports_assign(ports, 4, ids, rest, 2, 1, 0) == 0);
		CHECK(ports[0] == 91 && ports[1] == 92 && ports[2] == 0);
	}
	/* split with a pad on port 2 for player 2 and a second pad joining: player 2 keeps theirs */
	{
		static const unsigned long one[] = { 81 };
		static const unsigned long two[] = { 82, 81 };

		memset(ports, 0, sizeof(ports));
		ports[1] = 81;
		assign(one, 1, 1);
		assign(two, 2, 1);
		CHECK(ports[0] == 0 && ports[1] == 81 && ports[2] == 82);
	}
	/* nothing connected: every port free; nonsense in, nothing out */
	{
		ae_pad_ports_assign(ports, 4, NULL, NULL, 0, 0, 0);
		CHECK(!ports[0] && !ports[1] && !ports[2] && !ports[3]);
		CHECK(ae_pad_ports_assign(NULL, 4, NULL, NULL, 0, 0, 0) == 0);
		CHECK(ae_pad_ports_assign(ports, 0, NULL, NULL, 0, 0, 0) == 0);
	}

	if (failures)
		return 1;
	printf("ae_pad_ports_test: all passed\n");
	return 0;
}
