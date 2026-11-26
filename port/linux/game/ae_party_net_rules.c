/*
AE_PARTY_NET_RULES.C

The rules of an online party (ae_party_net_rules.h). Pure.
*/

#include <string.h>

#include "ae_party_net_rules.h"

static void copy(char *to, int size, const char *from)
{
	strncpy(to, from ? from : "", (size_t)size - 1);
	to[size - 1] = 0;
}

void ae_pnet_clear(struct ae_pnet *party)
{
	memset(party, 0, sizeof(*party));
}

static void fill(struct ae_pnet_member *member, const char *code, const char *name, int seats)
{
	copy(member->code, sizeof(member->code), code);
	copy(member->name, sizeof(member->name), name);
	member->seats = seats;
	member->online = 1;
}

int ae_pnet_create(struct ae_pnet *party, const char *code, const char *name, int seats)
{
	if (seats < 1 || seats > 4 || !code || !*code)
		return 0;
	ae_pnet_clear(party);
	fill(&party->member[0], code, name, seats);
	party->count = 1;
	party->leader = 0;
	party->privacy = AE_PNET_INVITE_ONLY;
	return 1;
}

int ae_pnet_seats(const struct ae_pnet *party)
{
	int index, seats = 0;

	for (index = 0; index < party->count; index++)
		seats += party->member[index].seats;
	return seats;
}

int ae_pnet_room(const struct ae_pnet *party)
{
	return party->count ? AE_PNET_MAX - ae_pnet_seats(party) : AE_PNET_MAX;
}

int ae_pnet_find(const struct ae_pnet *party, const char *code)
{
	int index;

	for (index = 0; code && index < party->count; index++)
		if (!strcmp(party->member[index].code, code))
			return index;
	return -1;
}

int ae_pnet_is_leader(const struct ae_pnet *party, const char *code)
{
	return party->count && ae_pnet_find(party, code) == party->leader;
}

int ae_pnet_can_invite(const struct ae_pnet *party, const char *by)
{
	if (!party->count || ae_pnet_find(party, by) < 0)
		return AE_PNET_NOT_IN_PARTY;
	if (party->privacy == AE_PNET_CLOSED)
		return AE_PNET_CLOSED_PARTY;
	if (party->privacy == AE_PNET_INVITE_ONLY && !ae_pnet_is_leader(party, by))
		return AE_PNET_NOT_LEADER;
	return ae_pnet_room(party) > 0 ? AE_PNET_OK : AE_PNET_FULL;
}

int ae_pnet_join(struct ae_pnet *party, const char *code, const char *name, int seats)
{
	if (seats < 1 || seats > 4 || !code || !*code)
		return AE_PNET_BAD_SEATS;
	if (!party->count)
		return AE_PNET_NOT_IN_PARTY;
	if (ae_pnet_find(party, code) >= 0)
		return AE_PNET_IN_PARTY;
	if (party->privacy == AE_PNET_CLOSED)
		return AE_PNET_CLOSED_PARTY;
	if (party->count == AE_PNET_MAX || seats > ae_pnet_room(party))
		return AE_PNET_FULL;
	fill(&party->member[party->count++], code, name, seats);
	return AE_PNET_OK;
}

int ae_pnet_leave(struct ae_pnet *party, const char *code)
{
	int at = ae_pnet_find(party, code), index;

	if (at < 0)
		return AE_PNET_UNKNOWN_MEMBER;
	for (index = at; index + 1 < party->count; index++)
		party->member[index] = party->member[index + 1];
	party->count--;
	memset(&party->member[party->count], 0, sizeof(party->member[0]));
	if (!party->count)
	{
		ae_pnet_clear(party);
		return AE_PNET_OK;
	}
	if (at == party->leader)
		party->leader = 0;
	else if (at < party->leader)
		party->leader--;
	return AE_PNET_OK;
}

int ae_pnet_kick(struct ae_pnet *party, const char *by, const char *code)
{
	if (!party->count || ae_pnet_find(party, by) < 0)
		return AE_PNET_NOT_IN_PARTY;
	if (!ae_pnet_is_leader(party, by))
		return AE_PNET_NOT_LEADER;
	if (!strcmp(by, code))
		return AE_PNET_SELF;
	return ae_pnet_leave(party, code);
}

int ae_pnet_promote(struct ae_pnet *party, const char *by, const char *code)
{
	int at;

	if (!party->count || ae_pnet_find(party, by) < 0)
		return AE_PNET_NOT_IN_PARTY;
	if (!ae_pnet_is_leader(party, by))
		return AE_PNET_NOT_LEADER;
	at = ae_pnet_find(party, code);
	if (at < 0)
		return AE_PNET_UNKNOWN_MEMBER;
	if (at == party->leader)
		return AE_PNET_SELF;
	party->leader = at;
	return AE_PNET_OK;
}

int ae_pnet_set_privacy(struct ae_pnet *party, const char *by, int privacy)
{
	if (!party->count || ae_pnet_find(party, by) < 0)
		return AE_PNET_NOT_IN_PARTY;
	if (!ae_pnet_is_leader(party, by))
		return AE_PNET_NOT_LEADER;
	if (privacy < AE_PNET_INVITE_ONLY || privacy > AE_PNET_CLOSED)
		return AE_PNET_BAD_SEATS;
	party->privacy = privacy;
	return AE_PNET_OK;
}

int ae_pnet_set_seats(struct ae_pnet *party, const char *code, int seats)
{
	int at = ae_pnet_find(party, code);

	if (at < 0)
		return AE_PNET_UNKNOWN_MEMBER;
	if (seats < 1 || seats > 4)
		return AE_PNET_BAD_SEATS;
	if (seats > party->member[at].seats && seats - party->member[at].seats > ae_pnet_room(party))
		return AE_PNET_FULL;
	party->member[at].seats = seats;
	return AE_PNET_OK;
}

int ae_pnet_can_search(const struct ae_pnet *party, const char *who, int limit)
{
	if (!party->count || ae_pnet_find(party, who) < 0)
		return AE_PNET_NOT_IN_PARTY;
	if (!ae_pnet_is_leader(party, who))
		return AE_PNET_NOT_LEADER;
	return ae_pnet_seats(party) > limit ? AE_PNET_TOO_BIG : AE_PNET_OK;
}

int ae_pnet_set_online(struct ae_pnet *party, const char *code, int online)
{
	int at = ae_pnet_find(party, code);

	online = online != 0;
	if (at < 0 || party->member[at].online == online)
		return 0;
	party->member[at].online = online;
	return 1;
}

int ae_pnet_fit(const struct ae_pnet *party, int room, int *out, int max)
{
	int order, count = 0;

	for (order = 0; order < party->count && count < max; order++)
	{
		/* the leader first, then the others as they joined */
		int index = order == 0 ? party->leader : (order - 1 < party->leader ? order - 1 : order);

		if (party->member[index].seats <= room)
		{
			room -= party->member[index].seats;
			out[count++] = index;
		}
	}
	return count;
}

const char *ae_pnet_refusal_code(int result)
{
	switch (result)
	{
	case AE_PNET_NOT_LEADER: return "not_leader";
	case AE_PNET_NOT_IN_PARTY: return "not_in_party";
	case AE_PNET_IN_PARTY: return "in_party";
	case AE_PNET_FULL: return "party_full";
	case AE_PNET_TOO_BIG: return "party_too_big";
	case AE_PNET_CLOSED_PARTY: return "party_closed";
	case AE_PNET_SELF: return "self";
	case AE_PNET_UNKNOWN_MEMBER: return "unknown_member";
	case AE_PNET_BAD_SEATS: return "bad_value";
	default: return "";
	}
}

const char *ae_pnet_reason_text(int result)
{
	switch (result)
	{
	case AE_PNET_NOT_LEADER: return "THE LEADER INVITES";
	case AE_PNET_NOT_IN_PARTY: return "NOT IN A PARTY";
	case AE_PNET_IN_PARTY: return "ALREADY IN YOUR PARTY";
	case AE_PNET_FULL: return "THE PARTY IS FULL";
	case AE_PNET_TOO_BIG: return "THE PARTY IS TOO BIG";
	case AE_PNET_CLOSED_PARTY: return "THE PARTY IS CLOSED";
	case AE_PNET_SELF: return "THAT IS YOU";
	default: return "";
	}
}

const char *ae_pnet_privacy_word(int privacy)
{
	return privacy == AE_PNET_FRIENDS ? "friends" : privacy == AE_PNET_CLOSED ? "closed" : "invite";
}

int ae_pnet_privacy_parse(const char *word)
{
	if (word && !strcmp(word, "friends"))
		return AE_PNET_FRIENDS;
	if (word && !strcmp(word, "closed"))
		return AE_PNET_CLOSED;
	return word && !strcmp(word, "invite") ? AE_PNET_INVITE_ONLY : -1;
}

const char *ae_pnet_privacy_label(int privacy)
{
	return privacy == AE_PNET_FRIENDS ? "FRIENDS OF THE PARTY" : privacy == AE_PNET_CLOSED ? "CLOSED" : "INVITE ONLY";
}
