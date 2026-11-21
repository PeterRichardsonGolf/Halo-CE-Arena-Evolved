/*
AE_STEAM_WS_RULES.C

ae_steam_ws_rules.h.
*/

#include "ae_steam_ws_rules.h"

#include <stdio.h>
#include <string.h>

/* ---------- SHA-1 and base64, for the upgrade's accept value only */

static unsigned int rotate_left(unsigned int value, int bits)
{
	return (value << bits) | (value >> (32 - bits));
}

static void sha1(unsigned char const *data, size_t size, unsigned char digest[20])
{
	unsigned int h[5] = { 0x67452301u, 0xefcdab89u, 0x98badcfeu, 0x10325476u, 0xc3d2e1f0u };
	unsigned char block[128];
	size_t total = size + 1 + 8;
	size_t padded = (total + 63) / 64 * 64;
	size_t offset, i;

	for (offset = 0; offset < padded; offset += 64)
	{
		unsigned int w[80];
		unsigned int a, b, c, d, e;
		int t;

		/* (the message, its 0x80, zeros and its length in bits, built a block at a time) */
		memset(block, 0, 64);
		for (i = 0; i < 64; i++)
		{
			size_t at = offset + i;

			if (at < size)
				block[i] = data[at];
			else if (at == size)
				block[i] = 0x80;
		}
		if (offset + 64 == padded)
		{
			unsigned long long bits = (unsigned long long)size * 8ull;

			for (i = 0; i < 8; i++)
				block[56 + i] = (unsigned char)(bits >> (56 - 8 * i));
		}
		for (t = 0; t < 16; t++)
			w[t] = (unsigned int)block[t * 4] << 24 | (unsigned int)block[t * 4 + 1] << 16 |
				(unsigned int)block[t * 4 + 2] << 8 | (unsigned int)block[t * 4 + 3];
		for (t = 16; t < 80; t++)
			w[t] = rotate_left(w[t - 3] ^ w[t - 8] ^ w[t - 14] ^ w[t - 16], 1);
		a = h[0]; b = h[1]; c = h[2]; d = h[3]; e = h[4];
		for (t = 0; t < 80; t++)
		{
			unsigned int f, k, next;

			if (t < 20) { f = (b & c) | (~b & d); k = 0x5a827999u; }
			else if (t < 40) { f = b ^ c ^ d; k = 0x6ed9eba1u; }
			else if (t < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8f1bbcdcu; }
			else { f = b ^ c ^ d; k = 0xca62c1d6u; }
			next = rotate_left(a, 5) + f + e + k + w[t];
			e = d; d = c; c = rotate_left(b, 30); b = a; a = next;
		}
		h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
	}
	for (i = 0; i < 20; i++)
		digest[i] = (unsigned char)(h[i / 4] >> (24 - 8 * (i % 4)));
}

int ae_ws_accept_key(char const *key, char *out, size_t size)
{
	static char const guid[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
	static char const alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	unsigned char joined[128], digest[20];
	size_t length = strlen(key), i, at = 0;

	if (size < 29 || length + sizeof(guid) > sizeof(joined))
		return 0;
	memcpy(joined, key, length);
	memcpy(joined + length, guid, sizeof(guid) - 1);
	sha1(joined, length + sizeof(guid) - 1, digest);
	for (i = 0; i < 18; i += 3)
	{
		unsigned int v = (unsigned int)digest[i] << 16 | (unsigned int)digest[i + 1] << 8 | digest[i + 2];

		out[at++] = alphabet[v >> 18];
		out[at++] = alphabet[(v >> 12) & 63];
		out[at++] = alphabet[(v >> 6) & 63];
		out[at++] = alphabet[v & 63];
	}
	{
		unsigned int v = (unsigned int)digest[18] << 16 | (unsigned int)digest[19] << 8;

		out[at++] = alphabet[v >> 18];
		out[at++] = alphabet[(v >> 12) & 63];
		out[at++] = alphabet[(v >> 6) & 63];
		out[at++] = '=';
	}
	out[at] = 0;
	return 1;
}

/* ---------- frames */

void ae_ws_unmask(unsigned char *payload, size_t size, unsigned char const mask[4])
{
	size_t i;

	for (i = 0; i < size; i++)
		payload[i] = (unsigned char)(payload[i] ^ mask[i & 3]);
}

size_t ae_ws_encode(unsigned char *out, size_t capacity, int opcode, unsigned char const *payload, size_t size,
	unsigned char const mask[4])
{
	size_t head = size < 126 ? 2 : size < 65536 ? 4 : 10;
	size_t i;

	if (capacity < head + 4 + size)
		return 0;
	out[0] = (unsigned char)(0x80 | (opcode & 15));
	if (size < 126)
		out[1] = (unsigned char)(0x80 | size);
	else if (size < 65536)
	{
		out[1] = 0x80 | 126;
		out[2] = (unsigned char)(size >> 8);
		out[3] = (unsigned char)size;
	}
	else
	{
		unsigned long long wide = (unsigned long long)size;

		out[1] = 0x80 | 127;
		for (i = 0; i < 8; i++)
			out[2 + i] = (unsigned char)(wide >> (56 - 8 * i));
	}
	memcpy(out + head, mask, 4);
	for (i = 0; i < size; i++)
		out[head + 4 + i] = (unsigned char)(payload[i] ^ mask[i & 3]);
	return head + 4 + size;
}

long ae_ws_decode(unsigned char const *data, size_t size, size_t maximum_payload, struct ae_ws_frame *frame)
{
	size_t head = 2, length, i;
	int opcode, masked;

	if (size < 2)
		return 0;
	if (data[0] & 0x70)
		return -1;
	opcode = data[0] & 15;
	if (opcode == 3 || opcode == 4 || opcode == 5 || opcode == 6 || opcode == 7 || opcode > 10)
		return -1;
	masked = (data[1] & 0x80) != 0;
	length = data[1] & 0x7f;
	if (length == 126)
	{
		head = 4;
		if (size < head)
			return 0;
		length = (size_t)data[2] << 8 | data[3];
		if (length < 126)
			return -1;
	}
	else if (length == 127)
	{
		unsigned long long wide = 0;

		head = 10;
		if (size < head)
			return 0;
		if (data[2] & 0x80)
			return -1;
		for (i = 0; i < 8; i++)
			wide = wide << 8 | data[2 + i];
		if (wide < 65536ull || wide > (unsigned long long)maximum_payload)
			return -1;
		length = (size_t)wide;
	}
	/* a control frame: whole and short */
	if (opcode >= 8 && (!(data[0] & 0x80) || length > 125))
		return -1;
	if (length > maximum_payload)
		return -1;
	if (masked)
		head += 4;
	if (size < head + length)
		return 0;
	frame->opcode = opcode;
	frame->fin = (data[0] & 0x80) != 0;
	frame->masked = masked;
	frame->header_size = head;
	frame->payload_size = length;
	return (long)(head + length);
}

/* ---------- the upgrade */

int ae_ws_request(char *out, size_t capacity, char const *host, int port, char const *path, char const *key)
{
	int written = snprintf(out, capacity,
		"GET %s HTTP/1.1\r\nHost: %s%s%.0d\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: %s\r\n"
		"Sec-WebSocket-Version: 13\r\nUser-Agent: opence\r\n\r\n",
		path, host, port == 443 ? "" : ":", port == 443 ? 0 : port, key);

	return written > 0 && (size_t)written < capacity;
}

/* the value of a header of the head, case-insensitively (a name at the start of a line): into value, "" when none */
static void header_value(char const *head, char const *name, char *value, size_t size)
{
	size_t length = strlen(name);
	char const *at = head;

	value[0] = 0;
	while (*at)
	{
		char const *end = strstr(at, "\r\n");
		size_t line = end ? (size_t)(end - at) : strlen(at);
		size_t i;

		if (line > length && at[length] == ':')
		{
			for (i = 0; i < length && ((at[i] | 32) == (name[i] | 32)); i++)
				;
			if (i == length)
			{
				char const *start = at + length + 1;

				while (start < at + line && (*start == ' ' || *start == '\t'))
					start++;
				line -= (size_t)(start - at);
				if (line >= size)
					line = size - 1;
				memcpy(value, start, line);
				value[line] = 0;
				return;
			}
		}
		if (!end)
			break;
		at = end + 2;
	}
}

int ae_ws_response_ok(char const *head, char const *key)
{
	char value[128], accept[32];
	int status = 0, i;

	if (strncmp(head, "HTTP/1.", 7) || sscanf(head + 7, "%*d %d", &status) != 1 || status != 101)
		return 0;
	header_value(head, "Upgrade", value, sizeof(value));
	for (i = 0; value[i]; i++)
		value[i] = (char)(value[i] | 32);
	if (strcmp(value, "websocket"))
		return 0;
	if (!ae_ws_accept_key(key, accept, sizeof(accept)))
		return 0;
	header_value(head, "Sec-WebSocket-Accept", value, sizeof(value));
	return !strcmp(value, accept);
}

/* ---------- the hosts */

int ae_ws_host_allowed(char const *host)
{
	static char const suffix[] = ".steamserver.net";
	size_t length = strlen(host), suffix_length = sizeof(suffix) - 1, i;
	int label_start = 1;

	if (length <= suffix_length || length > 253)
		return 0;
	for (i = 0; i < length; i++)
	{
		char ch = host[i];

		if (ch == '.')
		{
			/* (no empty label, none that ends in a hyphen either) */
			if (label_start || host[i - 1] == '-')
				return 0;
			label_start = 1;
			continue;
		}
		if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '-'))
			return 0;
		if (label_start && ch == '-')
			return 0;
		label_start = 0;
	}
	if (label_start)
		return 0;
	for (i = 0; i < suffix_length; i++)
		if ((host[length - suffix_length + i] | 32) != suffix[i])
			return 0;
	return 1;
}

/* ---------- the routes */

int ae_steam_route_next(int websocket_made, int tcp_made, int websocket_count, int tcp_count, unsigned long elapsed,
	unsigned long budget)
{
	int tries = websocket_count < AE_ROUTE_WEBSOCKET_TRIES ? websocket_count : AE_ROUTE_WEBSOCKET_TRIES;
	int on_time = !budget || elapsed < budget / 2;

	/* no TCP server listed: the WebSocket ones are all there is, as far as the list goes */
	if (!tcp_count)
		return websocket_made < websocket_count ? AE_ROUTE_WEBSOCKET : AE_ROUTE_NONE;
	if (websocket_made < tries && on_time)
		return AE_ROUTE_WEBSOCKET;
	return tcp_made < tcp_count ? AE_ROUTE_TCP : AE_ROUTE_NONE;
}
