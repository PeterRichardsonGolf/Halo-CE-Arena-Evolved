/*
AE_STEAM_WS_RULES.H

The Steam client's WebSocket route to a connection server (Steam's own client uses it: wss://<server>/cmsocket/, TLS
as the channel, no ChannelEncrypt; a Steam message is one binary frame). Pure: stdlib only, no sockets (unit test:
port/linux/tests/ae_steam_ws_rules_test.c). RFC 6455's frames, the upgrade's request and its check, and the order in
which the routes are tried.
*/

#ifndef AE_STEAM_WS_RULES_H
#define AE_STEAM_WS_RULES_H

#include <stddef.h>

enum { AE_WS_CONTINUATION = 0, AE_WS_TEXT = 1, AE_WS_BINARY = 2, AE_WS_CLOSE = 8, AE_WS_PING = 9, AE_WS_PONG = 10 };
/* the routes (ae_steam_route_next) */
enum { AE_ROUTE_NONE = 0, AE_ROUTE_WEBSOCKET = 1, AE_ROUTE_TCP = 2 };
/* at most this many WebSocket servers are tried before the TCP ones, and only while less than half the budget is gone */
enum { AE_ROUTE_WEBSOCKET_TRIES = 3 };

/* the answer to a key: base64 of SHA-1(key + RFC 6455's GUID); 1, or 0 when out holds fewer than 29 bytes */
int ae_ws_accept_key(char const *key, char *out, size_t size);

/* a client frame (masked with mask) of opcode with payload, whole (FIN set), into out: its size, 0 if it does not fit */
size_t ae_ws_encode(unsigned char *out, size_t capacity, int opcode, unsigned char const *payload, size_t size,
	unsigned char const mask[4]);

struct ae_ws_frame
{
	int opcode;
	int fin;
	int masked;                 /* the 4 mask bytes are the last of the header */
	size_t header_size;
	size_t payload_size;
};

/* the frame at the start of data: its whole size (header and payload) with frame filled; 0 when more bytes are needed;
-1 when it is not one to take (reserved bits, an unknown opcode, a control frame that is long or split, a length not
written the shortest way or over maximum_payload) */
long ae_ws_decode(unsigned char const *data, size_t size, size_t maximum_payload, struct ae_ws_frame *frame);

/* the mask taken off (or put on) a payload in place */
void ae_ws_unmask(unsigned char *payload, size_t size, unsigned char const mask[4]);

/* the upgrade request for the path of host:port with the key (base64 of 16 random bytes); 1, or 0 if it does not fit */
int ae_ws_request(char *out, size_t capacity, char const *host, int port, char const *path, char const *key);

/* whether the answer's head (up to the empty line) is the server agreeing: 101, Upgrade: websocket, and the accept
value of this key */
int ae_ws_response_ok(char const *head, char const *key);

/* whether a host the directory named may be connected to over the WebSocket route: letters, digits, hyphens and dots
only, labels not empty or starting with a hyphen, and ending in ".steamserver.net" with a label before it (so no
address literal and no other domain) */
int ae_ws_host_allowed(char const *host);

/* the next route to try: the attempts made on the WebSocket servers and on the TCP ones, how many of each the directory
listed, the milliseconds spent and the budget (0: none, no clock). AE_ROUTE_*; the next server of that route is the one
after those made */
int ae_steam_route_next(int websocket_made, int tcp_made, int websocket_count, int tcp_count, unsigned long elapsed,
	unsigned long budget);

#endif
