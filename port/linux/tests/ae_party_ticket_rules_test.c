/* ae_party_ticket_rules.c: a party's seat ticket read against the service's key, and a server's holds */
#include <stdio.h>
#include <string.h>

#include "ae_party_ticket_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

/* made by the service's make_ticket (tools/ae_party_service) with the seed 0..31, party PARTYID123AB, server play.example:10660,
members AAAA-BBBB-CCCC and DDDD-EEEE-FFFF, 2 seats, expiry 1000060 */
static const char ticket_text[] =
	"QUVQVAETRo5Zngt-PxQZ5g_-hFSUAAAAAAAPQnwCAte1bdD0BcLfckDs1k-ahTSWFnsGfs07p5RT3TGuvZHMZ4to-YKNcNQpnaJAqRvZmsHmNZmhRXwqFdX6603riGA4bk93NYAH7InKjb1mWjoL";

static const unsigned char service_key[32] = { 0x03, 0xa1, 0x07, 0xbf, 0xf3, 0xce, 0x10, 0xbe, 0x1d, 0x70, 0xdd, 0x18, 0xe7, 0x4b, 0xc0, 0x99,
	0x67, 0xe4, 0xd6, 0x30, 0x9b, 0xa5, 0x0d, 0x5f, 0x1d, 0xdc, 0x86, 0x64, 0x12, 0x55, 0x31, 0xb8 };

int main(void)
{
	struct ae_ticket ticket;
	struct ae_holds holds;
	unsigned char a[AE_TICKET_HASH], d[AE_TICKET_HASH], z[AE_TICKET_HASH], party[AE_TICKET_HASH];
	unsigned char other_key[32];
	char broken[sizeof(ticket_text)];

	ae_ticket_hash("AAAA-BBBB-CCCC", a);
	ae_ticket_hash("DDDD-EEEE-FFFF", d);
	ae_ticket_hash("ZZZZ-ZZZZ-ZZZZ", z);
	ae_ticket_hash("PARTYID123AB", party);
	/* the hash is the service's (first 8 bytes of SHA-256) */
	CHECK(a[0] == 0xd7 && a[1] == 0xb5 && a[7] == 0xdf && party[0] == 0x13 && party[7] == 0x3f);

	/* a good ticket */
	CHECK(ae_ticket_read(ticket_text, service_key, "play.example:10660", 1000000, &ticket) == AE_TICKET_OK);
	CHECK(ticket.seats == 2 && ticket.count == 2 && ticket.expires == 1000060 && !memcmp(ticket.party, party, 8));
	CHECK(!memcmp(ticket.member[0], a, 8) && !memcmp(ticket.member[1], d, 8));
	CHECK(ae_ticket_read(ticket_text, service_key, "play.example:10660", 1000060, &ticket) == AE_TICKET_OK);

	/* every refusal */
	CHECK(ae_ticket_read(ticket_text, service_key, "play.example:10660", 1000061, &ticket) == AE_TICKET_EXPIRED);
	CHECK(ae_ticket_read(ticket_text, service_key, "other.example:10660", 1000000, &ticket) == AE_TICKET_WRONG_SERVER);
	memcpy(other_key, service_key, 32);
	other_key[0] ^= 1;
	CHECK(ae_ticket_read(ticket_text, other_key, "play.example:10660", 1000000, &ticket) == AE_TICKET_BAD_SIGNATURE);
	memcpy(broken, ticket_text, sizeof(broken));
	broken[60] = broken[60] == 'A' ? 'B' : 'A';
	CHECK(ae_ticket_read(broken, service_key, "play.example:10660", 1000000, &ticket) == AE_TICKET_BAD_SIGNATURE);
	CHECK(ae_ticket_read("", service_key, "x", 1, &ticket) == AE_TICKET_BAD_FORMAT);
	CHECK(ae_ticket_read(NULL, service_key, "x", 1, &ticket) == AE_TICKET_BAD_FORMAT);
	CHECK(ae_ticket_read("QUVQVAET!!", service_key, "x", 1, &ticket) == AE_TICKET_BAD_FORMAT);
	CHECK(ae_ticket_read("QUVQVAETRo5Zngt", service_key, "x", 1, &ticket) == AE_TICKET_BAD_FORMAT);
	{
		char cut[80];

		memcpy(cut, ticket_text, 79);
		cut[79] = 0;
		CHECK(ae_ticket_read(cut, service_key, "play.example:10660", 1000000, &ticket) == AE_TICKET_BAD_FORMAT);
	}

	/* holds: a party's seats held for its members; others only get what is left */
	ae_holds_clear(&holds);
	ae_ticket_read(ticket_text, service_key, "play.example:10660", 1000000, &ticket);
	CHECK(ae_holds_offer(&holds, &ticket, 1000000) && ae_holds_offer(&holds, &ticket, 1000001));   /* the same party twice: one hold */
	CHECK(ae_holds_reserved(&holds, 1000000) == 2);
	/* a server with 3 free seats: a stranger takes the one the hold does not keep, then none */
	CHECK(ae_holds_admit(&holds, 3, z, 1000000) == 1 && ae_holds_admit(&holds, 2, z, 1000000) == 0);
	CHECK(ae_holds_admit(&holds, 2, NULL, 1000000) == 0 && ae_holds_admit(&holds, 3, NULL, 1000000) == 1);
	/* the party's own members take their held seats even on a server whose free seats are all held */
	CHECK(ae_holds_admit(&holds, 2, a, 1000000) == 1 && ae_holds_admit(&holds, 0, a, 1000000) == 0);
	CHECK(ae_holds_take(&holds, a, 1000000) == 1 && ae_holds_take(&holds, a, 1000000) == 0);   /* a seat is taken once */
	CHECK(ae_holds_reserved(&holds, 1000000) == 1);
	CHECK(ae_holds_take(&holds, z, 1000000) == 0);
	CHECK(ae_holds_take(&holds, d, 1000001) == 1 && ae_holds_reserved(&holds, 1000001) == 0);
	/* the minute ends: the hold goes, its seats are free again */
	ae_holds_clear(&holds);
	ae_holds_offer(&holds, &ticket, 1000000);
	CHECK(ae_holds_reserved(&holds, 1000060) == 2 && ae_holds_reserved(&holds, 1000061) == 0);
	CHECK(ae_holds_admit(&holds, 1, NULL, 1000061) == 1);
	/* a ticket past its time is not kept; a full table refuses a new party */
	ae_holds_clear(&holds);
	CHECK(!ae_holds_offer(&holds, &ticket, 1000100));
	{
		int index;

		for (index = 0; index < AE_HOLDS_MAX; index++)
		{
			ticket.party[0] = (unsigned char)index;
			CHECK(ae_holds_offer(&holds, &ticket, 1000000));
		}
		ticket.party[0] = 200;
		CHECK(!ae_holds_offer(&holds, &ticket, 1000000));
		CHECK(ae_holds_offer(&holds, &ticket, 1000061) == 0);
	}

	printf(failures ? "FAILED\n" : "ok\n");
	return failures != 0;
}
