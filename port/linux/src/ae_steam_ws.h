/*
AE_STEAM_WS.H

The Steam client's WebSocket connection (ae_steam_ws_rules.h has the frames): a TLS connection to host:port, the
certificate and the host name checked, upgraded to a
WebSocket at a path, then whole binary messages each way. Blocking calls in slices of a tenth of a second, so a cancel
(stop returns nonzero) is seen at once. Built with the Steam client (HALO_STEAM_CLIENT), the host's ABI.
*/

#ifndef AE_STEAM_WS_H
#define AE_STEAM_WS_H

#include <stddef.h>

struct ae_steam_ws;

/* the connection made, its TLS handshake done and its upgrade accepted, all within budget_ms (0: none); NULL and why
in error (a stop: "Signing in was cancelled." for the caller to read from *stopped: 1 stopped, 2 out of time) */
struct ae_steam_ws *ae_steam_ws_open(char const *host, int port, char const *path, int (*stop)(void),
	unsigned long budget_ms, int *stopped, char *error, int error_size);
/* one binary message out, within budget_ms (0: 30 s): 1, 0 on a failure, a stop or the budget */
int ae_steam_ws_send(struct ae_steam_ws *ws, unsigned char const *data, size_t size, int (*stop)(void),
	unsigned long budget_ms);
/* the next whole message (malloc'd; pings answered, text and pongs skipped): 1, 0 when none came within wait_ms or the
stop came (stopped says), -1 when the connection ended or failed */
int ae_steam_ws_receive(struct ae_steam_ws *ws, unsigned long wait_ms, int (*stop)(void), unsigned char **data,
	size_t *size, int *stopped);
void ae_steam_ws_close(struct ae_steam_ws *ws);
/* the last failure's words */
char const *ae_steam_ws_error(struct ae_steam_ws const *ws);

#endif
