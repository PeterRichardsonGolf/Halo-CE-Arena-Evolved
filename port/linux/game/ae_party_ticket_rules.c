/*
AE_PARTY_TICKET_RULES.C

See ae_party_ticket_rules.h.
*/

#include <string.h>

#include "ae_party_ticket_rules.h"
#include "mcc_sha256.h"
#include "../../third_party/monocypher/monocypher-ed25519.h"

void ae_ticket_hash(const char *text, unsigned char out[AE_TICKET_HASH])
{
	struct mcc_sha256 context;
	unsigned char digest[32];

	mcc_sha256_begin(&context);
	mcc_sha256_add(&context, text, (int)strlen(text));
	mcc_sha256_end(&context, digest);
	memcpy(out, digest, AE_TICKET_HASH);
}

/* base64url without padding into bytes: the count, -1 for anything else */
static int decode(const char *text, unsigned char *out, int maximum)
{
	int bits = 0, count = 0;
	unsigned int value = 0;

	for (; *text; text++)
	{
		int digit;

		if (*text >= 'A' && *text <= 'Z')
			digit = *text - 'A';
		else if (*text >= 'a' && *text <= 'z')
			digit = *text - 'a' + 26;
		else if (*text >= '0' && *text <= '9')
			digit = *text - '0' + 52;
		else if (*text == '-')
			digit = 62;
		else if (*text == '_')
			digit = 63;
		else
			return -1;
		value = (value << 6 | (unsigned int)digit) & 0xffffu;
		bits += 6;
		if (bits >= 8)
		{
			bits -= 8;
			if (count == maximum)
				return -1;
			out[count++] = (unsigned char)(value >> bits & 255u);
		}
	}
	return count;
}

int ae_ticket_read(const char *text, const unsigned char service_key[32], const char *server_address, unsigned long long now,
	struct ae_ticket *out)
{
	unsigned char raw[5 + 8 + 8 + 8 + 2 + 8 * AE_TICKET_MEMBERS + 64], server[AE_TICKET_HASH];
	int size, body, index;
	unsigned long long expires = 0;

	memset(out, 0, sizeof(*out));
	if (!text)
		return AE_TICKET_BAD_FORMAT;
	size = decode(text, raw, (int)sizeof(raw));
	if (size < 5 + 8 + 8 + 8 + 2 + 64 || memcmp(raw, "AEPT\x01", 5))
		return AE_TICKET_BAD_FORMAT;
	body = size - 64;
	if (body != 31 + AE_TICKET_HASH * raw[30] || raw[30] > AE_TICKET_MEMBERS)
		return AE_TICKET_BAD_FORMAT;
	if (crypto_ed25519_check(raw + body, service_key, raw, (size_t)body) != 0)
		return AE_TICKET_BAD_SIGNATURE;
	ae_ticket_hash(server_address, server);
	if (memcmp(server, raw + 13, AE_TICKET_HASH))
		return AE_TICKET_WRONG_SERVER;
	for (index = 0; index < 8; index++)
		expires = expires << 8 | raw[21 + index];
	if (expires < now)
		return AE_TICKET_EXPIRED;
	memcpy(out->party, raw + 5, AE_TICKET_HASH);
	out->expires = expires;
	out->seats = raw[29];
	out->count = raw[30];
	for (index = 0; index < out->count; index++)
		memcpy(out->member[index], raw + 31 + AE_TICKET_HASH * index, AE_TICKET_HASH);
	return AE_TICKET_OK;
}

void ae_holds_clear(struct ae_holds *holds)
{
	memset(holds, 0, sizeof(*holds));
}

static void drop_expired(struct ae_holds *holds, unsigned long long now)
{
	int index;

	for (index = 0; index < AE_HOLDS_MAX; index++)
		if (holds->hold[index].used && holds->hold[index].expires < now)
			memset(&holds->hold[index], 0, sizeof(holds->hold[index]));
}

int ae_holds_offer(struct ae_holds *holds, const struct ae_ticket *ticket, unsigned long long now)
{
	int index, free_slot = -1;

	drop_expired(holds, now);
	if (ticket->expires < now)
		return 0;
	for (index = 0; index < AE_HOLDS_MAX; index++)
	{
		if (holds->hold[index].used && !memcmp(holds->hold[index].party, ticket->party, AE_TICKET_HASH))
		{
			/* (the same party again: its time is extended, nothing else) */
			if (ticket->expires > holds->hold[index].expires)
				holds->hold[index].expires = ticket->expires;
			return 1;
		}
		if (!holds->hold[index].used && free_slot < 0)
			free_slot = index;
	}
	if (free_slot < 0)
		return 0;
	memset(&holds->hold[free_slot], 0, sizeof(holds->hold[free_slot]));
	holds->hold[free_slot].used = 1;
	memcpy(holds->hold[free_slot].party, ticket->party, AE_TICKET_HASH);
	holds->hold[free_slot].expires = ticket->expires;
	holds->hold[free_slot].seats = ticket->seats;
	holds->hold[free_slot].count = ticket->count;
	memcpy(holds->hold[free_slot].member, ticket->member, sizeof(ticket->member));
	return 1;
}

/* a hold's seats not taken yet: its seats shared out over its members (a member's seats are not in the ticket: the party's seats
split evenly, the first members taking the remainder), the ones who have not joined */
static int open_seats(const struct ae_hold *hold)
{
	int index, open = 0;

	for (index = 0; index < hold->count; index++)
		if (!hold->taken[index])
			open += hold->seats / hold->count + (index < hold->seats % hold->count ? 1 : 0);
	return open;
}

int ae_holds_reserved(struct ae_holds *holds, unsigned long long now)
{
	int index, total = 0;

	drop_expired(holds, now);
	for (index = 0; index < AE_HOLDS_MAX; index++)
		if (holds->hold[index].used && holds->hold[index].count)
			total += open_seats(&holds->hold[index]);
	return total;
}

int ae_holds_take(struct ae_holds *holds, const unsigned char member[AE_TICKET_HASH], unsigned long long now)
{
	int index, each;

	drop_expired(holds, now);
	for (index = 0; index < AE_HOLDS_MAX; index++)
	{
		if (!holds->hold[index].used)
			continue;
		for (each = 0; each < holds->hold[index].count; each++)
			if (!holds->hold[index].taken[each] && !memcmp(holds->hold[index].member[each], member, AE_TICKET_HASH))
			{
				holds->hold[index].taken[each] = 1;
				return 1;
			}
	}
	return 0;
}

int ae_holds_admit(struct ae_holds *holds, int free_seats, const unsigned char *member, unsigned long long now)
{
	int reserved = ae_holds_reserved(holds, now), index, each;

	if (member)
		for (index = 0; index < AE_HOLDS_MAX; index++)
			for (each = 0; holds->hold[index].used && each < holds->hold[index].count; each++)
				if (!holds->hold[index].taken[each] && !memcmp(holds->hold[index].member[each], member, AE_TICKET_HASH))
					return free_seats > 0;
	return free_seats - reserved > 0;
}
