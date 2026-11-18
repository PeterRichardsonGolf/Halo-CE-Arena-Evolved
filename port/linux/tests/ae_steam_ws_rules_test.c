/* ae_steam_ws_rules.c: the WebSocket route to Steam's connection servers (RFC 6455 frames, the upgrade's key and
check, the order the transports are tried in) */
#include <stdio.h>
#include <string.h>

#include "ae_steam_ws_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	unsigned char out[70000], payload[70000];
	unsigned char const mask[4] = { 0x37, 0xfa, 0x21, 0x3d };
	struct ae_ws_frame frame;
	char accept[32];
	char request[512];
	size_t size, i;

	/* RFC 6455 section 1.3's example key */
	CHECK(ae_ws_accept_key("dGhlIHNhbXBsZSBub25jZQ==", accept, sizeof(accept)));
	CHECK(!strcmp(accept, "s3pPLMBiTxaQ9kYGzzhZRbK+xOo="));
	CHECK(!ae_ws_accept_key("dGhlIHNhbXBsZSBub25jZQ==", accept, 10));

	/* RFC 6455 section 5.7: a masked "Hello" text frame */
	size = ae_ws_encode(out, sizeof(out), AE_WS_TEXT, (unsigned char const *)"Hello", 5, mask);
	CHECK(size == 11);
	CHECK(!memcmp(out, "\x81\x85\x37\xfa\x21\x3d\x7f\x9f\x4d\x51\x58", 11));
	/* and the server's unmasked one */
	CHECK(ae_ws_decode((unsigned char const *)"\x81\x05Hello", 7, 1000, &frame) == 7);
	CHECK(frame.opcode == AE_WS_TEXT && frame.fin && !frame.masked && frame.header_size == 2 && frame.payload_size == 5);

	/* a binary frame's lengths: 125, 126 (16 bits), 65536 (64 bits) */
	for (i = 0; i < sizeof(payload); i++)
		payload[i] = (unsigned char)(i * 7);
	size = ae_ws_encode(out, sizeof(out), AE_WS_BINARY, payload, 125, mask);
	CHECK(size == 2 + 4 + 125 && out[1] == (0x80 | 125));
	size = ae_ws_encode(out, sizeof(out), AE_WS_BINARY, payload, 126, mask);
	CHECK(size == 4 + 4 + 126 && out[1] == (0x80 | 126) && out[2] == 0 && out[3] == 126);
	size = ae_ws_encode(out, sizeof(out), AE_WS_BINARY, payload, 65536, mask);
	CHECK(size == 10 + 4 + 65536 && out[1] == (0x80 | 127) && out[7] == 1 && out[8] == 0 && out[9] == 0);
	/* decoding it back: the mask taken off */
	CHECK(ae_ws_decode(out, size, 1 << 20, &frame) == (long)size);
	CHECK(frame.masked && frame.payload_size == 65536 && frame.header_size == 14);
	ae_ws_unmask(out + frame.header_size, frame.payload_size, out + frame.header_size - 4);
	CHECK(!memcmp(out + frame.header_size, payload, 65536));
	/* too small an output */
	CHECK(ae_ws_encode(out, 10, AE_WS_BINARY, payload, 100, mask) == 0);

	/* a frame cut anywhere asks for more; never reads past the bytes it was given */
	size = ae_ws_encode(out, sizeof(out), AE_WS_BINARY, payload, 300, mask);
	for (i = 0; i < size; i++)
		CHECK(ae_ws_decode(out, i, 1000, &frame) == 0);
	CHECK(ae_ws_decode(out, size, 1000, &frame) == (long)size);
	/* a payload over the limit, reserved bits, a control frame that is long or fragmented: refused */
	CHECK(ae_ws_decode(out, size, 299, &frame) == -1);
	CHECK(ae_ws_decode((unsigned char const *)"\xc1\x00", 2, 100, &frame) == -1);
	CHECK(ae_ws_decode((unsigned char const *)"\x09\x7e\x00\x80", 4, 1000, &frame) == -1);
	CHECK(ae_ws_decode((unsigned char const *)"\x09\x00", 2, 100, &frame) == -1);
	CHECK(ae_ws_decode((unsigned char const *)"\x89\x00", 2, 100, &frame) == 2 && frame.opcode == AE_WS_PING);
	/* a 64-bit length with the top bit set */
	CHECK(ae_ws_decode((unsigned char const *)"\x82\x7f\x80\x00\x00\x00\x00\x00\x00\x00", 10, 1000, &frame) == -1);
	/* a 16-bit length where 7 bits would do: refused */
	CHECK(ae_ws_decode((unsigned char const *)"\x82\x7e\x00\x05", 4, 1000, &frame) == -1);

	/* the upgrade request and its answer */
	CHECK(ae_ws_request(request, sizeof(request), "cm1.example.net", 443, "/cmsocket/", "dGhlIHNhbXBsZSBub25jZQ=="));
	CHECK(strstr(request, "GET /cmsocket/ HTTP/1.1\r\n") == request);
	CHECK(strstr(request, "Host: cm1.example.net\r\n"));
	CHECK(strstr(request, "Upgrade: websocket\r\n") && strstr(request, "Sec-WebSocket-Version: 13\r\n"));
	CHECK(strstr(request, "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"));
	CHECK(strstr(request, "\r\n\r\n"));
	CHECK(ae_ws_request(request, sizeof(request), "cm1.example.net", 27021, "/cmsocket/", "k") &&
		strstr(request, "Host: cm1.example.net:27021\r\n"));
	CHECK(!ae_ws_request(request, 20, "cm1.example.net", 443, "/cmsocket/", "k"));
	CHECK(ae_ws_response_ok("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
		"Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n\r\n", "dGhlIHNhbXBsZSBub25jZQ=="));
	CHECK(ae_ws_response_ok("HTTP/1.1 101 x\r\nupgrade: WebSocket\r\nsec-websocket-accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n\r\n",
		"dGhlIHNhbXBsZSBub25jZQ=="));
	CHECK(!ae_ws_response_ok("HTTP/1.1 200 OK\r\nUpgrade: websocket\r\n"
		"Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n\r\n", "dGhlIHNhbXBsZSBub25jZQ=="));
	CHECK(!ae_ws_response_ok("HTTP/1.1 101 x\r\nUpgrade: websocket\r\nSec-WebSocket-Accept: wrong\r\n\r\n",
		"dGhlIHNhbXBsZSBub25jZQ=="));
	CHECK(!ae_ws_response_ok("HTTP/1.1 101 x\r\nUpgrade: websocket\r\n\r\n", "dGhlIHNhbXBsZSBub25jZQ=="));

	/* the order the routes are tried in (the attempts made so far on each, the servers listed on each, the time spent and
	the budget): up to 3 WebSocket servers inside the first half of the budget, then the TCP servers; no WebSocket server
	listed: TCP only; no TCP server listed: WebSocket as far as it goes; 0 when there is nothing left */
	CHECK(ae_steam_route_next(0, 0, 5, 5, 0, 20000) == AE_ROUTE_WEBSOCKET);
	CHECK(ae_steam_route_next(2, 0, 5, 5, 4000, 20000) == AE_ROUTE_WEBSOCKET);
	CHECK(ae_steam_route_next(3, 0, 5, 5, 9000, 20000) == AE_ROUTE_TCP);
	CHECK(ae_steam_route_next(3, 4, 5, 5, 9000, 20000) == AE_ROUTE_TCP);
	CHECK(ae_steam_route_next(3, 5, 5, 5, 9000, 20000) == 0);
	/* WebSocket attempts that took half the budget: TCP now */
	CHECK(ae_steam_route_next(1, 0, 5, 5, 10000, 20000) == AE_ROUTE_TCP);
	CHECK(ae_steam_route_next(1, 0, 5, 5, 9999, 20000) == AE_ROUTE_WEBSOCKET);
	CHECK(ae_steam_route_next(0, 0, 0, 4, 0, 20000) == AE_ROUTE_TCP);
	CHECK(ae_steam_route_next(0, 3, 0, 4, 0, 20000) == AE_ROUTE_TCP);
	CHECK(ae_steam_route_next(0, 4, 0, 4, 0, 20000) == 0);
	CHECK(ae_steam_route_next(1, 0, 2, 0, 0, 20000) == AE_ROUTE_WEBSOCKET);
	CHECK(ae_steam_route_next(2, 0, 2, 0, 0, 20000) == 0);
	CHECK(ae_steam_route_next(5, 0, 8, 0, 0, 20000) == AE_ROUTE_WEBSOCKET);
	CHECK(ae_steam_route_next(0, 0, 0, 0, 0, 20000) == 0);
	/* two WebSocket servers only, TCP after them */
	CHECK(ae_steam_route_next(2, 0, 2, 3, 100, 20000) == AE_ROUTE_TCP);
	/* no budget (0): no clock, the count rule only */
	CHECK(ae_steam_route_next(1, 0, 5, 5, 999999, 0) == AE_ROUTE_WEBSOCKET);
	CHECK(ae_steam_route_next(3, 0, 5, 5, 999999, 0) == AE_ROUTE_TCP);

	/* the hosts the WebSocket route may connect to: names under steamserver.net only (the directory's, but never taken on
	trust); no address literal, no other domain, no odd characters */
	CHECK(ae_ws_host_allowed("cmp2-sea1.steamserver.net"));
	CHECK(ae_ws_host_allowed("CMP1-DFW2.STEAMSERVER.NET"));
	CHECK(ae_ws_host_allowed("a.b.steamserver.net"));
	CHECK(!ae_ws_host_allowed("steamserver.net"));
	CHECK(!ae_ws_host_allowed(".steamserver.net"));
	CHECK(!ae_ws_host_allowed("evilsteamserver.net"));
	CHECK(!ae_ws_host_allowed("cmp1.steamserver.net.evil.com"));
	CHECK(!ae_ws_host_allowed("cmp1.steamserver.net."));
	CHECK(!ae_ws_host_allowed("a..steamserver.net"));
	CHECK(!ae_ws_host_allowed("203.0.113.9"));
	CHECK(!ae_ws_host_allowed("[::1]"));
	CHECK(!ae_ws_host_allowed("a b.steamserver.net"));
	CHECK(!ae_ws_host_allowed("a/b.steamserver.net"));
	CHECK(!ae_ws_host_allowed("a@b.steamserver.net"));
	CHECK(!ae_ws_host_allowed("a:443.steamserver.net"));
	CHECK(!ae_ws_host_allowed("-a.steamserver.net"));
	CHECK(!ae_ws_host_allowed(""));
	CHECK(!ae_ws_host_allowed("cmp1.steamserver.net\r\nX: y"));

	if (failures)
		return 1;
	printf("ok\n");
	return 0;
}
