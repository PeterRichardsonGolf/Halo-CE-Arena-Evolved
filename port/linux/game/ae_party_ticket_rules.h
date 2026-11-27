/*
AE_PARTY_TICKET_RULES.H

A party's SEAT TICKET as the party service signs it (tools/ae_party_service make_ticket) and what a server does with it: read the
ticket's text, check the service's signature, the server it names and its expiry, keep a table of HOLDS (a party's seats kept
for its members for the ticket's minute), and decide a join: a member of a hold takes a held seat, anybody else only a seat no
hold keeps. Pure but for monocypher and SHA-256 (port/linux/tests/ae_party_ticket_rules_test.c).

Ticket bytes: "AEPT", version 1, party hash (8), server hash (8), expiry (8, big endian Unix seconds), seats (1), member count (1),
member hashes (8 each), then the 64-byte Ed25519 signature of everything before it; written in base64url without padding.
A hash is the first 8 bytes of SHA-256: of the party id text, of the server's address text ("host:port"), of a friend code text
("XXXX-XXXX-XXXX").
*/

#ifndef __AE_PARTY_TICKET_RULES_H
#define __AE_PARTY_TICKET_RULES_H

enum
{
	AE_TICKET_HASH = 8,
	AE_TICKET_MEMBERS = 8,
	AE_HOLDS_MAX = 16
};

enum ae_ticket_result
{
	AE_TICKET_OK,
	AE_TICKET_BAD_FORMAT,
	AE_TICKET_BAD_SIGNATURE,
	AE_TICKET_WRONG_SERVER,
	AE_TICKET_EXPIRED
};

struct ae_ticket
{
	unsigned char party[AE_TICKET_HASH];
	unsigned long long expires;
	int seats, count;
	unsigned char member[AE_TICKET_MEMBERS][AE_TICKET_HASH];
};

/* the hash of a text (an address, a party id, a friend code) */
void ae_ticket_hash(const char *text, unsigned char out[AE_TICKET_HASH]);

/* a ticket's text read and checked against the service's public key, this server's address and the time: AE_TICKET_OK with out filled */
int ae_ticket_read(const char *text, const unsigned char service_key[32], const char *server_address, unsigned long long now,
	struct ae_ticket *out);

struct ae_hold
{
	int used;
	unsigned char party[AE_TICKET_HASH];
	unsigned long long expires;
	int seats, count;
	unsigned char member[AE_TICKET_MEMBERS][AE_TICKET_HASH];
	unsigned char taken[AE_TICKET_MEMBERS];
};

struct ae_holds
{
	struct ae_hold hold[AE_HOLDS_MAX];
};

void ae_holds_clear(struct ae_holds *holds);
/* a read ticket's party takes its holds (the same party again only extends it); 1 when kept, 0 for a full table or a ticket past its time */
int ae_holds_offer(struct ae_holds *holds, const struct ae_ticket *ticket, unsigned long long now);
/* seats held for members who have not joined yet (expired holds dropped first) */
int ae_holds_reserved(struct ae_holds *holds, unsigned long long now);
/* a member (hash) of a live hold joins: its seat is taken from the hold, 1; 0 when no hold lists it or it joined already */
int ae_holds_take(struct ae_holds *holds, const unsigned char member[AE_TICKET_HASH], unsigned long long now);
/* whether a join is admitted given the free seats the server has before it counts any hold: a member of a hold always (its seat is held),
anybody else (member NULL) only when free seats exceed the seats held for others */
int ae_holds_admit(struct ae_holds *holds, int free_seats, const unsigned char *member, unsigned long long now);

#endif
