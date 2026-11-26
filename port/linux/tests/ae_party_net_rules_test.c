/* ae_party_net_rules.c: an online party's members, seats, leader, privacy and what fits */
#include <stdio.h>
#include <string.h>

#include "ae_party_net_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static void make(struct ae_pnet *p, int privacy)
{
	/* Chief leads with Rook (2 seats) and Pickle (1) behind */
	CHECK(ae_pnet_create(p, "CHIEF", "Chief", 1));
	CHECK(ae_pnet_join(p, "ROOK", "Rook", 2) == AE_PNET_OK);
	CHECK(ae_pnet_join(p, "PICKLE", "Pickle", 1) == AE_PNET_OK);
	p->privacy = privacy;
}

int main(void)
{
	struct ae_pnet p;
	int out[AE_PNET_MAX];

	/* create: one seat to four; a leader of INVITE ONLY */
	CHECK(!ae_pnet_create(&p, "A", "A", 0) && !ae_pnet_create(&p, "A", "A", 5) && !ae_pnet_create(&p, "", "A", 1));
	make(&p, AE_PNET_INVITE_ONLY);
	CHECK(ae_pnet_seats(&p) == 4 && ae_pnet_room(&p) == 4 && p.count == 3);
	CHECK(ae_pnet_is_leader(&p, "CHIEF") && !ae_pnet_is_leader(&p, "ROOK") && !ae_pnet_is_leader(&p, "NOBODY"));

	/* party_seats_count_guests: a seat is a split-screen seat; full counts seats, not members */
	CHECK(ae_pnet_join(&p, "JUNO", "Juno", 4) == AE_PNET_OK && ae_pnet_seats(&p) == 8 && ae_pnet_room(&p) == 0);
	CHECK(ae_pnet_join(&p, "KES", "Kes", 1) == AE_PNET_FULL);
	CHECK(ae_pnet_leave(&p, "JUNO") == AE_PNET_OK);
	CHECK(ae_pnet_join(&p, "KES", "Kes", 5) == AE_PNET_BAD_SEATS && ae_pnet_join(&p, "KES", "Kes", 0) == AE_PNET_BAD_SEATS);
	CHECK(ae_pnet_join(&p, "ROOK", "Rook", 1) == AE_PNET_IN_PARTY);
	/* party_full_counts_seats: 5 seats in, a member of 4 would pass 8 */
	CHECK(ae_pnet_join(&p, "KES", "Kes", 1) == AE_PNET_OK);
	CHECK(ae_pnet_seats(&p) == 5 && ae_pnet_join(&p, "BIG", "Big", 4) == AE_PNET_FULL && ae_pnet_room(&p) == 3);
	CHECK(ae_pnet_join(&p, "BIG", "Big", 3) == AE_PNET_OK && ae_pnet_room(&p) == 0);

	/* member_invite_needs_friends_privacy */
	make(&p, AE_PNET_INVITE_ONLY);
	CHECK(ae_pnet_can_invite(&p, "CHIEF") == AE_PNET_OK);
	CHECK(ae_pnet_can_invite(&p, "ROOK") == AE_PNET_NOT_LEADER);
	CHECK(!strcmp(ae_pnet_reason_text(AE_PNET_NOT_LEADER), "THE LEADER INVITES"));
	CHECK(ae_pnet_can_invite(&p, "STRANGER") == AE_PNET_NOT_IN_PARTY);
	CHECK(ae_pnet_set_privacy(&p, "ROOK", AE_PNET_FRIENDS) == AE_PNET_NOT_LEADER);
	CHECK(ae_pnet_set_privacy(&p, "CHIEF", AE_PNET_FRIENDS) == AE_PNET_OK && ae_pnet_can_invite(&p, "ROOK") == AE_PNET_OK);
	CHECK(ae_pnet_set_privacy(&p, "CHIEF", AE_PNET_CLOSED) == AE_PNET_OK);
	CHECK(ae_pnet_can_invite(&p, "CHIEF") == AE_PNET_CLOSED_PARTY && ae_pnet_join(&p, "NEW", "New", 1) == AE_PNET_CLOSED_PARTY);
	CHECK(ae_pnet_set_privacy(&p, "CHIEF", 9) != AE_PNET_OK);

	/* leader_leave_passes_to_longest */
	make(&p, AE_PNET_INVITE_ONLY);
	CHECK(ae_pnet_leave(&p, "CHIEF") == AE_PNET_OK && p.count == 2 && ae_pnet_is_leader(&p, "ROOK"));
	CHECK(ae_pnet_leave(&p, "NOBODY") == AE_PNET_UNKNOWN_MEMBER);
	/* a member behind the leader leaves: the lead stays; one ahead leaves: the lead keeps its person */
	make(&p, AE_PNET_INVITE_ONLY);
	CHECK(ae_pnet_promote(&p, "CHIEF", "PICKLE") == AE_PNET_OK && ae_pnet_is_leader(&p, "PICKLE"));
	CHECK(ae_pnet_leave(&p, "ROOK") == AE_PNET_OK && ae_pnet_is_leader(&p, "PICKLE") && p.count == 2);
	CHECK(ae_pnet_leave(&p, "CHIEF") == AE_PNET_OK && ae_pnet_is_leader(&p, "PICKLE") && p.count == 1);
	CHECK(ae_pnet_leave(&p, "PICKLE") == AE_PNET_OK && p.count == 0 && ae_pnet_room(&p) == AE_PNET_MAX);

	/* kick and promote: leader only, never self */
	make(&p, AE_PNET_INVITE_ONLY);
	CHECK(ae_pnet_kick(&p, "ROOK", "PICKLE") == AE_PNET_NOT_LEADER && ae_pnet_kick(&p, "CHIEF", "CHIEF") == AE_PNET_SELF);
	CHECK(ae_pnet_kick(&p, "CHIEF", "NOBODY") == AE_PNET_UNKNOWN_MEMBER);
	CHECK(ae_pnet_kick(&p, "CHIEF", "ROOK") == AE_PNET_OK && p.count == 2 && ae_pnet_find(&p, "ROOK") < 0);
	CHECK(ae_pnet_promote(&p, "PICKLE", "CHIEF") == AE_PNET_NOT_LEADER && ae_pnet_promote(&p, "CHIEF", "CHIEF") == AE_PNET_SELF);
	CHECK(ae_pnet_promote(&p, "CHIEF", "NOBODY") == AE_PNET_UNKNOWN_MEMBER);

	/* seats change: a guest joins or goes; the cap holds */
	make(&p, AE_PNET_INVITE_ONLY);
	CHECK(ae_pnet_set_seats(&p, "PICKLE", 4) == AE_PNET_OK && ae_pnet_seats(&p) == 7);
	CHECK(ae_pnet_set_seats(&p, "CHIEF", 3) == AE_PNET_FULL && p.member[0].seats == 1);
	CHECK(ae_pnet_set_seats(&p, "ROOK", 1) == AE_PNET_OK && ae_pnet_seats(&p) == 6);
	CHECK(ae_pnet_set_seats(&p, "ROOK", 0) == AE_PNET_BAD_SEATS && ae_pnet_set_seats(&p, "NOBODY", 1) == AE_PNET_UNKNOWN_MEMBER);

	/* search: the leader only, and the playlist's party limit counts seats (4 here: 1 + 2 + 1) */
	make(&p, AE_PNET_INVITE_ONLY);
	CHECK(ae_pnet_can_search(&p, "CHIEF", 4) == AE_PNET_OK && ae_pnet_can_search(&p, "CHIEF", 3) == AE_PNET_TOO_BIG);
	CHECK(ae_pnet_can_search(&p, "ROOK", 4) == AE_PNET_NOT_LEADER && ae_pnet_can_search(&p, "X", 4) == AE_PNET_NOT_IN_PARTY);

	/* online flag changes once */
	CHECK(ae_pnet_set_online(&p, "ROOK", 0) == 1 && ae_pnet_set_online(&p, "ROOK", 0) == 0 && !p.member[1].online);
	CHECK(ae_pnet_set_online(&p, "NOBODY", 0) == 0);

	/* what fits a game with less room: the leader first, then as they joined; the rest sit out */
	CHECK(ae_pnet_fit(&p, 4, out, 8) == 3 && out[0] == 0 && out[1] == 1 && out[2] == 2);
	CHECK(ae_pnet_fit(&p, 3, out, 8) == 2 && out[0] == 0 && out[1] == 1);
	CHECK(ae_pnet_fit(&p, 2, out, 8) == 2 && out[0] == 0 && out[1] == 2);   /* Rook (2) does not fit after Chief; Pickle does */
	CHECK(ae_pnet_fit(&p, 0, out, 8) == 0);
	ae_pnet_promote(&p, "CHIEF", "PICKLE");
	CHECK(ae_pnet_fit(&p, 2, out, 8) == 2 && out[0] == 2 && out[1] == 0);   /* the leader is Pickle now */
	CHECK(ae_pnet_fit(&p, 4, out, 2) == 2);

	/* words */
	CHECK(!strcmp(ae_pnet_refusal_code(AE_PNET_FULL), "party_full") && !strcmp(ae_pnet_refusal_code(AE_PNET_OK), ""));
	CHECK(ae_pnet_privacy_parse("friends") == AE_PNET_FRIENDS && ae_pnet_privacy_parse("closed") == AE_PNET_CLOSED);
	CHECK(ae_pnet_privacy_parse("invite") == AE_PNET_INVITE_ONLY && ae_pnet_privacy_parse("x") == -1 && ae_pnet_privacy_parse(NULL) == -1);
	CHECK(!strcmp(ae_pnet_privacy_word(AE_PNET_FRIENDS), "friends") && !strcmp(ae_pnet_privacy_label(AE_PNET_FRIENDS), "FRIENDS OF THE PARTY"));

	printf(failures ? "FAILED\n" : "ok\n");
	return failures != 0;
}
