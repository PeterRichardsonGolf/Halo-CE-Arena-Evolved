/* A smoke check of the Steam client's WebSocket route, by hand (not a test: it reaches a Steam server): TLS to the
server's host:port (a *.steamserver.net name) with the certificate checked against the pinned roots, the upgrade at
/cmsocket/, and a close. No sign-in, no message sent. Usage: steam_ws_smoke host port. Link it with the Steam client's
objects of a linux64 build (ae_steam_ws.o, ae_steam_ws_rules.o, steam_net.o, steam_crypto.o, posix_net.o, Mbed TLS) and
stubs for host_logf and posix_browser_lock / posix_browser_unlock. */
#include <stdio.h>
#include <stdlib.h>
#include "ae_steam_ws.h"
#include "steam_net.h"

int main(int argc, char **argv)
{
	char error[256] = "";
	int stopped = 0;
	unsigned long started = steam_milliseconds();
	struct ae_steam_ws *ws;
	unsigned char *data;
	size_t size;
	int result;

	if (argc < 3)
		return 2;
	ws = ae_steam_ws_open(argv[1], atoi(argv[2]), "/cmsocket/", NULL, 10000, &stopped, error, sizeof(error));
	if (!ws)
	{
		printf("FAILED after %lu ms: %s\n", steam_milliseconds() - started, error);
		return 1;
	}
	printf("OK: TLS + WebSocket upgrade accepted in %lu ms\n", steam_milliseconds() - started);
	result = ae_steam_ws_receive(ws, 2000, NULL, &data, &size, &stopped);
	printf("receive in 2 s: %d (%lu bytes)\n", result, (unsigned long)size);
	if (result > 0)
		free(data);
	ae_steam_ws_close(ws);
	return 0;
}
