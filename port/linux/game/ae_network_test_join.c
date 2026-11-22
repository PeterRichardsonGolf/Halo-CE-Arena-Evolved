/*
AE_NETWORK_TEST_JOIN.C

Arena Evolved (0.7.7): the network test's join by address (network_test.c's "join", debug.network_test):
HALO_NETWORK_TEST_JOIN_ADDRESS=<a.b.c.d>[:<port>] joins the game at that address and port, TCP and UDP both, as JOIN
BY ADDRESS does (ae_direct_join_body.h), not the first game a search finds: a host behind a forwarded port or a
tunnel that carries its game port alone, which answers no search, is joined as its players join it.
*/

#include "cseries.h"
#include "networking/network_game_protocol.h"

#include <stdio.h>
#include <stdlib.h>

/* ae_direct_join_body.h's (inside network_client_manager.c) and the game's */
long ae_network_game_client_join_address(unsigned long address, unsigned short port);
void platform_log(char const *format, ...);

enum
{
	/* the game's port when none is named */
	DEFAULT_PORT = NETWORK_GAME_SERVER_PORT,
};

static boolean join_address_read(
	unsigned long *address,
	unsigned short *port)
{
	char const *text = getenv("HALO_NETWORK_TEST_JOIN_ADDRESS");
	unsigned int a, b, c, d, number = DEFAULT_PORT;
	char end;
	int fields;

	if (!text || !*text)
		return FALSE;
	fields = sscanf(text, "%u.%u.%u.%u:%u%c", &a, &b, &c, &d, &number, &end);
	if ((fields != 4 && fields != 5) || a > 255 || b > 255 || c > 255 || d > 255 || !number || number > 65535)
		return FALSE;
	/* (the game's addresses are in host order: 127.0.0.1 is 0x7F000001) */
	*address = (unsigned long)a << 24 | (unsigned long)b << 16 | (unsigned long)c << 8 | (unsigned long)d;
	*port = (unsigned short)number;
	return TRUE;
}

boolean ae_network_test_join_address_set(
	void)
{
	unsigned long address;
	unsigned short port;

	return join_address_read(&address, &port);
}

boolean ae_network_test_join_address(
	void)
{
	unsigned long address;
	unsigned short port;
	long started;

	if (!join_address_read(&address, &port))
		return FALSE;
	/* (0: this machine's client is not searching yet, asked again next frame) */
	started = ae_network_game_client_join_address(address, port);
	if (started)
		platform_log("network test: joining %s by address: %s", getenv("HALO_NETWORK_TEST_JOIN_ADDRESS"),
			started > 0 ? "connecting" : "could not");
	return started > 0;
}
