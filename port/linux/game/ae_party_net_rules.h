/*
AE_PARTY_NET_RULES.H

The rules of an ONLINE PARTY (people on other PCs, found by friend code or link): members, seats, the leader, who may invite,
joining, leaving, kicking, promoting and the members that fit a game with less room. The party service (tools/ae_party_service)
keeps the same table; this is the game's copy, used to answer a button (the disabled row's reason) before the service is asked
and to check what the service sent. Pure (no engine includes; port/linux/tests/ae_party_net_rules_test.c).

- SEATS: a member holds 1-4 seats (split screen on its PC); a party holds at most AE_PNET_MAX seats, never split by a game.
- LEADER: picks the playlist, searches, invites under INVITE ONLY, sets WHO CAN JOIN, kicks and promotes. Leaving hands the lead
  to the member who joined first. The last member leaving ends the party.
- PRIVACY: INVITE ONLY (the leader invites), FRIENDS OF THE PARTY (any member invites), CLOSED (nobody joins).
*/

#ifndef __AE_PARTY_NET_RULES_H
#define __AE_PARTY_NET_RULES_H

#include "ae_friend_code.h"

enum
{
	AE_PNET_MAX = 8,
	AE_PNET_NAME = 24,
	AE_PNET_OFFLINE_SECONDS = 120,
	AE_PNET_INVITE_SECONDS = 300
};

enum ae_pnet_privacy { AE_PNET_INVITE_ONLY, AE_PNET_FRIENDS, AE_PNET_CLOSED };

/* why an action is refused; the stable words of the service are ae_pnet_refusal_code's */
enum ae_pnet_result
{
	AE_PNET_OK,
	AE_PNET_NOT_LEADER,
	AE_PNET_NOT_IN_PARTY,
	AE_PNET_IN_PARTY,
	AE_PNET_FULL,
	AE_PNET_TOO_BIG,
	AE_PNET_CLOSED_PARTY,
	AE_PNET_SELF,
	AE_PNET_UNKNOWN_MEMBER,
	AE_PNET_BAD_SEATS
};

struct ae_pnet_member
{
	char code[AE_FRIEND_CODE_SIZE];
	char name[AE_PNET_NAME];
	int seats;     /* 1-4 */
	int online;    /* 0 when no word came for a while */
};

struct ae_pnet
{
	int count;                            /* 0: no party */
	int privacy;
	int leader;                           /* index into member */
	struct ae_pnet_member member[AE_PNET_MAX];  /* in the order they joined */
};

void ae_pnet_clear(struct ae_pnet *party);
/* a party of one: this member leads, privacy INVITE ONLY. 1 when made, 0 for seats outside 1-4 */
int ae_pnet_create(struct ae_pnet *party, const char *code, const char *name, int seats);
/* the seats the party holds; the free seats of the cap */
int ae_pnet_seats(const struct ae_pnet *party);
int ae_pnet_room(const struct ae_pnet *party);
/* the member with this code, -1 none */
int ae_pnet_find(const struct ae_pnet *party, const char *code);
int ae_pnet_is_leader(const struct ae_pnet *party, const char *code);

/* whether by may invite someone now (ignoring whether anyone is free to be invited): AE_PNET_OK, NOT_IN_PARTY, NOT_LEADER (under INVITE ONLY and
by not leading), CLOSED_PARTY, FULL (no seat left) */
int ae_pnet_can_invite(const struct ae_pnet *party, const char *by);
/* a member joins with this many seats: OK (taken at the end), IN_PARTY (code already in), CLOSED_PARTY, FULL (the free seats are not
enough; ae_pnet_room says how many are) , BAD_SEATS. A no-party record is left alone */
int ae_pnet_join(struct ae_pnet *party, const char *code, const char *name, int seats);
/* a member leaves (or is kicked): the lead passes to the member who joined first when the leader went; the last one out clears the
party. UNKNOWN_MEMBER when code is not in it */
int ae_pnet_leave(struct ae_pnet *party, const char *code);
/* the leader removes another member / passes the lead: NOT_LEADER, SELF, UNKNOWN_MEMBER, OK */
int ae_pnet_kick(struct ae_pnet *party, const char *by, const char *code);
int ae_pnet_promote(struct ae_pnet *party, const char *by, const char *code);
/* the leader changes who can join: NOT_LEADER or OK */
int ae_pnet_set_privacy(struct ae_pnet *party, const char *by, int privacy);
/* a member's split-screen seats changed: OK, UNKNOWN_MEMBER, BAD_SEATS, FULL (the party would pass its cap; seats unchanged) */
int ae_pnet_set_seats(struct ae_pnet *party, const char *code, int seats);
/* the party may search a playlist whose party limit is limit seats: NOT_LEADER (who is not the leader), TOO_BIG (seats over limit) */
int ae_pnet_can_search(const struct ae_pnet *party, const char *who, int limit);
/* a member's connection state: offline members whose silence passed AE_PNET_OFFLINE_SECONDS are removed by ae_pnet_expire; online flips
the flag. Returns 1 when something changed */
int ae_pnet_set_online(struct ae_pnet *party, const char *code, int online);
/* the members (indexes) that fit a game with room free seats: the leader first, then in join order while seats last; returns how many
fit (up to max written to out); the others sit out and stay in the party */
int ae_pnet_fit(const struct ae_pnet *party, int room, int *out, int max);

/* the service's stable word for a result ("party_full", "not_leader"; "" for OK) and the disabled row's reason in the menus
("THE LEADER INVITES", "THE PARTY IS FULL") */
const char *ae_pnet_refusal_code(int result);
const char *ae_pnet_reason_text(int result);
/* privacy names: "invite", "friends", "closed" (service) and the menus' words */
const char *ae_pnet_privacy_word(int privacy);
int ae_pnet_privacy_parse(const char *word);
const char *ae_pnet_privacy_label(int privacy);

#endif
