/*
AE_REJOIN_SOCKET.C

(rc6) REJOIN AFTER A DROP's silent test drop (port/linux/game/ae_rejoin.c, debug.network_test_drop "<seconds>:silent"):
the client's stream socket kept open as the game closes it, so the host hears nothing, as when a machine's network
goes away. Here, not in the game layer: the game's files see the Winsock names, not the system's sockets.
*/

#ifndef _WIN32
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif

/* a second descriptor of the socket when it is a stream (closing the first then sends nothing); -1 otherwise, and on
Windows */
int ae_rejoin_socket_keep(long socket)
{
#ifndef _WIN32
	int type = 0;
	socklen_t size = sizeof(type);

	if (socket < 0 || getsockopt((int)socket, SOL_SOCKET, SO_TYPE, &type, &size) != 0 || type != SOCK_STREAM)
		return -1;
	return dup((int)socket);
#else
	(void)socket;
	return -1;
#endif
}
