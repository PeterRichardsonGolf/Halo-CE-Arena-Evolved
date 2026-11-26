/* ae_party_follow_rules.c: what a party member's PC does with the service's view */
#include <stdio.h>
#include <string.h>

#include "ae_party_follow_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static void party(struct ae_pc_view *v)
{
	memset(v, 0, sizeof(*v));
	v->ok = 1;
	v->has_party = 1;
	v->member_count = 2;
	snprintf(v->leader, sizeof(v->leader), "%s", "LEAD");
}

static void found(struct ae_pc_view *v, long n, const char *address)
{
	v->search.present = 1;
	v->search.n = n;
	snprintf(v->search.state, sizeof(v->search.state), "found");
	snprintf(v->search.server_address, sizeof(v->search.server_address), "%s", address);
}

int main(void)
{
	struct ae_pc_view v;
	struct ae_follow_state st;
	struct ae_follow_action a;
	int ask;

	/* a search found: everyone, the leader too, joins that server once */
	ae_follow_reset(&st);
	party(&v);
	found(&v, 1, "play.example:10660");
	ae_follow_decide(&v, "ME", 0, &st, &a);
	CHECK(a.kind == AE_FOLLOW_SERVER && !strcmp(a.address, "play.example:10660"));
	ae_follow_decide(&v, "ME", 0, &st, &a);
	CHECK(a.kind == AE_FOLLOW_NONE);                                   /* the same answer again: nothing */
	ae_follow_decide(&v, "LEAD", 0, &st, &a);
	CHECK(a.kind == AE_FOLLOW_NONE);
	found(&v, 2, "play.example:10661");                                /* the next game (CONTINUE): taken again */
	ae_follow_decide(&v, "LEAD", 0, &st, &a);
	CHECK(a.kind == AE_FOLLOW_SERVER && !strcmp(a.address, "play.example:10661"));

	/* the targets a follow may go to: numeric public IPv4 with a port, nothing local, no name, no lookup */
	CHECK(ae_follow_target_ok("203.0.113.5:10660", 0) && ae_follow_target_ok("8.8.8.8:1", 0) && ae_follow_target_ok("1.2.3.4:65535", 0));
	CHECK(!ae_follow_target_ok("play.example:10660", 0) && !ae_follow_target_ok("203.0.113.5", 0) && !ae_follow_target_ok("203.0.113.5:0", 0));
	CHECK(!ae_follow_target_ok("203.0.113.5:65536", 0) && !ae_follow_target_ok("203.0.113.256:1", 0) && !ae_follow_target_ok("203.0.113:1", 0));
	CHECK(!ae_follow_target_ok("0.1.2.3:1", 0) && !ae_follow_target_ok("10.0.0.1:1", 0) && !ae_follow_target_ok("100.64.0.1:1", 0));
	CHECK(ae_follow_target_ok("100.63.255.255:1", 0) && ae_follow_target_ok("100.128.0.1:1", 0) && !ae_follow_target_ok("100.127.255.255:1", 0));
	CHECK(!ae_follow_target_ok("127.0.0.1:10660", 0) && !ae_follow_target_ok("169.254.1.1:1", 0) && ae_follow_target_ok("169.253.1.1:1", 0));
	CHECK(!ae_follow_target_ok("172.16.0.1:1", 0) && !ae_follow_target_ok("172.31.255.1:1", 0) && ae_follow_target_ok("172.32.0.1:1", 0));
	CHECK(ae_follow_target_ok("172.15.0.1:1", 0) && !ae_follow_target_ok("192.168.1.1:1", 0) && ae_follow_target_ok("192.169.1.1:1", 0));
	CHECK(!ae_follow_target_ok("224.0.0.1:1", 0) && !ae_follow_target_ok("255.255.255.255:1", 0) && !ae_follow_target_ok("", 0) && !ae_follow_target_ok(NULL, 0));
	/* a party service on this machine (a DEV test) may send to a local server, still only numeric */
	CHECK(ae_follow_target_ok("127.0.0.1:10660", 1) && ae_follow_target_ok("192.168.1.1:1", 1) && !ae_follow_target_ok("play.example:1", 1));
	CHECK(ae_follow_service_is_loopback("http://127.0.0.1:8780/party") && ae_follow_service_is_loopback("http://localhost") &&
		!ae_follow_service_is_loopback("https://127.0.0.1") && !ae_follow_service_is_loopback("http://127.0.0.1.evil.example/party") &&
		!ae_follow_service_is_loopback("https://master.example/party"));
	CHECK(ae_follow_invite_ok("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"));
	CHECK(!ae_follow_invite_ok("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcde") &&
		!ae_follow_invite_ok("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0") &&
		!ae_follow_invite_ok("0123456789ABCDEF0123456789abcdef0123456789abcdef0123456789abcdef") && !ae_follow_invite_ok("abcd1234") && !ae_follow_invite_ok(NULL));

	/* one automatic follow a 10 s; three failures in two minutes pause it until the player acts in the party UI */
	ae_follow_reset(&st);
	CHECK(ae_follow_may_start(&st, 5));
	ae_follow_started(&st, 1000);
	CHECK(!ae_follow_may_start(&st, 1000) && !ae_follow_may_start(&st, 10999) && ae_follow_may_start(&st, 11000));
	ae_follow_started(&st, 0xfffffff0u);
	CHECK(!ae_follow_may_start(&st, 5) && ae_follow_may_start(&st, 0x10000u));                  /* the clock wraps */
	ae_follow_reset(&st);
	ae_follow_failed(&st, 1000);
	ae_follow_failed(&st, 50000);
	CHECK(!st.paused);
	ae_follow_failed(&st, 100000);
	CHECK(st.paused && !ae_follow_may_start(&st, 500000));
	ae_follow_resume(&st);
	CHECK(!st.paused && ae_follow_may_start(&st, 500000));
	ae_follow_failed(&st, 1000);
	ae_follow_failed(&st, 50000);
	ae_follow_failed(&st, 200000);                                                              /* 199 s after the first: no pause */
	CHECK(!st.paused);
	ae_follow_failed(&st, 210000);                                                              /* 50000, 200000, 210000: 160 s */
	CHECK(!st.paused);
	ae_follow_failed(&st, 215000);                                                              /* 200000, 210000, 215000: three in 15 s */
	CHECK(st.paused);

	/* a member gone (left or kicked) after the invite was announced: rotate, and announce the new invite once (only a NEW one) */
	{
		struct ae_follow_members before;
		struct ae_pc_view w;

		party(&w);
		w.member_count = 3;
		snprintf(w.member[0].code, sizeof(w.member[0].code), "LEAD");
		snprintf(w.member[1].code, sizeof(w.member[1].code), "ONE");
		snprintf(w.member[2].code, sizeof(w.member[2].code), "TWO");
		ae_follow_members_note(&before, &w);
		CHECK(before.count == 3 && !ae_follow_member_left(&before, &w));
		CHECK(ae_follow_should_announce(&w, "LEAD", "invite-a", ""));
		CHECK(!ae_follow_should_announce(&w, "LEAD", "invite-a", "invite-a"));
		w.member_count = 2;                                        /* TWO is gone */
		snprintf(w.member[1].code, sizeof(w.member[1].code), "ONE");
		CHECK(ae_follow_member_left(&before, &w));
		CHECK(!ae_follow_should_announce(&w, "LEAD", "invite-a", "invite-a"));    /* the old invite is not told again */
		CHECK(ae_follow_should_announce(&w, "LEAD", "invite-b", "invite-a"));     /* the rotated one is */
		ae_follow_members_note(&before, &w);
		CHECK(before.count == 2 && !ae_follow_member_left(&before, &w));
		/* a member joining is not a member gone; the party ending notes nobody */
		w.member_count = 3;
		snprintf(w.member[2].code, sizeof(w.member[2].code), "NEW");
		CHECK(!ae_follow_member_left(&before, &w));
		w.has_party = 0;
		CHECK(!ae_follow_member_left(&before, &w));
		ae_follow_members_note(&before, &w);
		CHECK(before.count == 0);
	}

	/* busy: the answer waits, and is taken when the PC is free */
	ae_follow_reset(&st);
	ae_follow_decide(&v, "ME", 1, &st, &a);
	CHECK(a.kind == AE_FOLLOW_NONE && st.seen_search == 0);
	ae_follow_decide(&v, "ME", 0, &st, &a);
	CHECK(a.kind == AE_FOLLOW_SERVER);

	/* no party, an error reply and a poll that changed nothing do nothing */
	ae_follow_reset(&st);
	v.has_party = 0;
	ae_follow_decide(&v, "ME", 0, &st, &a);
	CHECK(a.kind == AE_FOLLOW_NONE);
	party(&v);
	found(&v, 3, "x:1");
	v.same = 1;
	ae_follow_decide(&v, "ME", 0, &st, &a);
	CHECK(a.kind == AE_FOLLOW_NONE);
	v.same = 0;
	v.ok = 0;
	ae_follow_decide(&v, "ME", 0, &st, &a);
	CHECK(a.kind == AE_FOLLOW_NONE);

	/* bots: only the leader is asked; the ask flag carries (COMP asks); a member just remembers */
	ae_follow_reset(&st);
	party(&v);
	v.search.present = 1;
	v.search.n = 4;
	v.search.ask = 1;
	snprintf(v.search.state, sizeof(v.search.state), "bots");
	snprintf(v.search.reason, sizeof(v.search.reason), "no_room");
	ae_follow_decide(&v, "ME", 0, &st, &a);
	CHECK(a.kind == AE_FOLLOW_NONE && st.seen_search == 4);
	ae_follow_reset(&st);
	ae_follow_decide(&v, "LEAD", 0, &st, &a);
	CHECK(a.kind == AE_FOLLOW_BOTS_OFFER && a.ask == 1 && !strcmp(a.reason, "no_room"));

	/* a search stopped: the party stays together, the reason shown */
	ae_follow_reset(&st);
	party(&v);
	v.search.present = 1;
	v.search.n = 5;
	snprintf(v.search.state, sizeof(v.search.state), "none");
	snprintf(v.search.reason, sizeof(v.search.reason), "leader_left");
	ae_follow_decide(&v, "ME", 0, &st, &a);
	CHECK(a.kind == AE_FOLLOW_STOPPED && !strcmp(a.reason, "leader_left"));

	/* the leader's follow: a member joins the hosted game by invite, a server by address; the leader's own is not followed */
	ae_follow_reset(&st);
	party(&v);
	v.follow.present = 1;
	v.follow.n = 1;
	snprintf(v.follow.kind, sizeof(v.follow.kind), "hosted");
	snprintf(v.follow.invite, sizeof(v.follow.invite), "abcd1234");
	ae_follow_decide(&v, "LEAD", 0, &st, &a);
	CHECK(a.kind == AE_FOLLOW_NONE && st.seen_follow == 1);
	ae_follow_reset(&st);
	ae_follow_decide(&v, "ME", 0, &st, &a);
	CHECK(a.kind == AE_FOLLOW_INVITE && !strcmp(a.invite, "abcd1234"));
	ae_follow_decide(&v, "ME", 0, &st, &a);
	CHECK(a.kind == AE_FOLLOW_NONE);
	v.follow.n = 2;
	snprintf(v.follow.kind, sizeof(v.follow.kind), "server");
	snprintf(v.follow.address, sizeof(v.follow.address), "play.example:10660");
	ae_follow_decide(&v, "ME", 0, &st, &a);
	CHECK(a.kind == AE_FOLLOW_SERVER && !strcmp(a.address, "play.example:10660"));
	v.follow.n = 3;
	snprintf(v.follow.kind, sizeof(v.follow.kind), "lan");
	ae_follow_decide(&v, "ME", 0, &st, &a);
	CHECK(a.kind == AE_FOLLOW_STOPPED && !strcmp(a.reason, "not_followable"));
	v.follow.n = 4;
	snprintf(v.follow.kind, sizeof(v.follow.kind), "hosted");
	v.follow.invite[0] = 0;
	ae_follow_decide(&v, "ME", 0, &st, &a);
	CHECK(a.kind == AE_FOLLOW_STOPPED);

	/* a leader hosting a game tells the party once per invite */
	party(&v);
	CHECK(ae_follow_should_announce(&v, "LEAD", "abcd", "") == 1 && ae_follow_should_announce(&v, "LEAD", "abcd", "abcd") == 0);
	CHECK(ae_follow_should_announce(&v, "LEAD", "efgh", "abcd") == 1 && ae_follow_should_announce(&v, "ME", "abcd", "") == 0);
	CHECK(ae_follow_should_announce(&v, "LEAD", "", "") == 0 && ae_follow_should_announce(&v, "LEAD", NULL, NULL) == 0);
	v.member_count = 1;
	CHECK(ae_follow_should_announce(&v, "LEAD", "abcd", "") == 0);

	/* CONTINUE: a leader of a party of two on a known playlist asks; a member waits; alone or an unknown playlist: as before */
	party(&v);
	CHECK(ae_follow_continue(&v, "LEAD", "casual", &ask) == 1 && ask == 1);
	CHECK(ae_follow_continue(&v, "ME", "comp", &ask) == 1 && ask == 0);
	CHECK(ae_follow_continue(&v, "LEAD", "tournament-x", &ask) == 0 && ask == 0);
	CHECK(ae_follow_continue(&v, "LEAD", NULL, &ask) == 0);
	v.member_count = 1;
	CHECK(ae_follow_continue(&v, "LEAD", "casual", &ask) == 0);
	v.member_count = 2;
	v.has_party = 0;
	CHECK(ae_follow_continue(&v, "LEAD", "casual", &ask) == 0);
	party(&v);
	v.ok = 0;
	CHECK(ae_follow_continue(&v, "LEAD", "casual", &ask) == 0);
	CHECK(ae_follow_playlist_known("classic") && ae_follow_playlist_known("community") && !ae_follow_playlist_known(""));

	printf(failures ? "FAILED\n" : "ok\n");
	return failures != 0;
}
