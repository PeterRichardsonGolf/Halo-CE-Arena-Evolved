/*
AE_PARTY_FOLLOW_RULES.H

What a party member's PC does with the service's view (ae_party_client_rules.h): join the server a QUICK PLAY found, follow the
leader into a game the leader hosts, go on to the next game after the post-game's CONTINUE. Pure (port/linux/tests/
ae_party_follow_rules_test.c); the glue that carries an action out is ae_party_follow.c.

- An ACTION is taken once: the search's and the follow's counters (n) are remembered, so a poll that repeats an answer does nothing.
- Nothing is taken while this PC is in a game or another join is going on: the answer waits (its counter is not remembered).
- The leader joins the server a search found like everyone else; a follow the leader made itself (it hosts the game) is not followed.
- CONTINUE: only the leader asks for the next game (QUICK PLAY on the playlist again, preferring the server just played); members
  wait in the menus for the leader's answer. A playlist the service does not know is not asked for.
*/

#ifndef __AE_PARTY_FOLLOW_RULES_H
#define __AE_PARTY_FOLLOW_RULES_H

#include "ae_party_client_rules.h"

enum ae_follow_kind
{
	AE_FOLLOW_NONE,
	AE_FOLLOW_SERVER,        /* join address (host:port) */
	AE_FOLLOW_INVITE,        /* join the invite code of the leader's hosted game */
	AE_FOLLOW_BOTS_OFFER,    /* nothing had room: the leader may play AE TOURNAMENT (ask: choose) */
	AE_FOLLOW_STOPPED        /* the search ended without a server (reason) */
};

enum
{
	AE_FOLLOW_GAP_MS = 10000,        /* at most one automatic follow this often */
	AE_FOLLOW_FAILURES = 3,          /* this many failed follows ... */
	AE_FOLLOW_FAILURE_WINDOW_MS = 120000   /* ... within this long pause the automatic follow */
};

struct ae_follow_state
{
	long seen_search, seen_follow;
	int started;                       /* an automatic follow has begun */
	unsigned last_start_ms;
	int failed;                        /* failures held in failed_ms */
	unsigned failed_ms[AE_FOLLOW_FAILURES];
	int paused;                        /* paused after the failures, until the player acts in the party UI */
};

struct ae_follow_action
{
	int kind;
	char address[AE_PC_ADDRESS];
	char invite[AE_PC_ADDRESS];
	int ask;
	char reason[24];
};

void ae_follow_reset(struct ae_follow_state *state);
/* the next action for a view (none when there is nothing new): me is this PC's friend code; busy is 1 in a game or a join going on */
void ae_follow_decide(const struct ae_pc_view *view, const char *me, int busy, struct ae_follow_state *state, struct ae_follow_action *out);

/* whether a follow may begin at now_ms (a clock that only goes forward, wrapping at 2^32): not while paused, not within
AE_FOLLOW_GAP_MS of the last one begun */
int ae_follow_may_start(const struct ae_follow_state *state, unsigned now_ms);
void ae_follow_started(struct ae_follow_state *state, unsigned now_ms);
/* a follow failed: the third within AE_FOLLOW_FAILURE_WINDOW_MS pauses the automatic follow */
void ae_follow_failed(struct ae_follow_state *state, unsigned now_ms);
/* the player pressed something in the party UI: the pause ends */
void ae_follow_resume(struct ae_follow_state *state);

/* whether a server address may be followed to: a numeric public IPv4 "a.b.c.d:port" (no host name, so no lookup), not 0/8, 10/8,
100.64/10, 127/8, 169.254/16, 172.16/12, 192.168/16, 224/4 and up. service_is_loopback (the party service is on this machine: a
DEV test) allows those ranges */
int ae_follow_target_ok(const char *address, int service_is_loopback);
/* whether a hosted game's invite is what make_invite writes: 64 lower-case hex digits */
int ae_follow_invite_ok(const char *invite);
/* the codes of a party's members as last seen, to notice one gone */
struct ae_follow_members
{
	int count;
	char code[AE_PNET_MAX][AE_FRIEND_CODE_SIZE];
};
/* 1 when a member in before is not in the view's party any more (left or kicked); the leader of a hosted game rotates the invite then */
int ae_follow_member_left(const struct ae_follow_members *before, const struct ae_pc_view *view);
/* before = the view's members (none when it has no party) */
void ae_follow_members_note(struct ae_follow_members *before, const struct ae_pc_view *view);

/* whether a service address (ae_pc_origin's form) is this machine's: http://127.0.0.1 or http://localhost */
int ae_follow_service_is_loopback(const char *origin);

/* whether this PC, hosting a game with this invite, tells its party to follow: the leader of a party of two or more, an invite not yet
announced (last: the one told last, "" for none) */
int ae_follow_should_announce(const struct ae_pc_view *view, const char *me, const char *invite, const char *last);

/* whether the service knows a playlist id (casual, btb, comp, classic, community) */
int ae_follow_playlist_known(const char *id);
/* the post-game's CONTINUE of a party: 1 = the party carries on by itself (this PC does not reopen its own lobby): a leader asks the
service (ask = 1: a QUICK PLAY call for playlist, preferring prefer), a member waits (ask = 0). 0 = not in an online party, or the
playlist is not one the service serves: the caller's own CONTINUE runs as before */
int ae_follow_continue(const struct ae_pc_view *view, const char *me, const char *playlist, int *ask);

#endif
