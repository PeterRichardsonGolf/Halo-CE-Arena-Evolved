/* The party record and its rules (see the header). Pure code. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ae_party_model_rules.h"

static int same_text(char const *a, char const *b)
{
	for (; *a && *b; a++, b++)
		if ((*a | 0x20) != (*b | 0x20))
			return 0;
	return !*a && !*b;
}

void ae_party_clear(struct ae_party *party)
{
	memset(party, 0, sizeof(*party));
}

int ae_party_find_id(struct ae_party const *party, char const *id)
{
	int at;

	for (at = 0; at < party->count; at++)
		if (!strcmp(party->member[at].id, id))
			return at;
	return -1;
}

int ae_party_add(struct ae_party *party, struct ae_party_member const *member)
{
	if (party->count >= AE_PARTY_MAX || ae_party_find_id(party, member->id) >= 0)
		return -1;
	party->member[party->count] = *member;
	party->member[party->count].leader = party->count == 0;
	if (party->member[party->count].seats < 1)
		party->member[party->count].seats = 1;
	return party->count++;
}

int ae_party_remove(struct ae_party *party, int index)
{
	if (index < 0 || index >= party->count || party->member[index].leader)
		return 0;
	memmove(&party->member[index], &party->member[index + 1], sizeof(party->member[0]) * (size_t)(party->count - index - 1));
	party->count--;
	return 1;
}

void ae_party_bot(struct ae_party_member *member, char const *name, int tier)
{
	memset(member, 0, sizeof(*member));
	snprintf(member->name, sizeof(member->name), "%s", name);
	snprintf(member->id, sizeof(member->id), "bot:%s", name);
	member->kind = AE_MEMBER_BOT;
	member->seats = 1;
	member->tier = tier;
	member->ping_ms = -1;
}

int ae_party_takes_part(struct ae_party_member const *member, int source)
{
	if (source == AE_PARTY_SOURCE_OFFLINE)
		return member->kind != AE_MEMBER_FRIEND;
	return member->kind != AE_MEMBER_BOT && member->state != AE_PARTY_INVITED && member->state != AE_PARTY_OFFLINE;
}

int ae_party_search_seats(struct ae_party const *party, int source)
{
	int at, seats = 0;

	for (at = 0; at < party->count; at++)
		if (ae_party_takes_part(&party->member[at], source))
			seats += party->member[at].seats;
	return seats;
}

int ae_party_tag(struct ae_party_member const *member, int source)
{
	if (member->leader)
		return AE_TAG_LEADER;
	if (member->kind == AE_MEMBER_BOT && source == AE_PARTY_SOURCE_ONLINE)
		return AE_TAG_OFFLINE_ONLY;
	if (member->state == AE_PARTY_INVITED)
		return AE_TAG_PENDING;
	if (member->kind == AE_MEMBER_FRIEND && member->state == AE_PARTY_READY)
		return AE_TAG_READY;
	if (member->saved)
		return AE_TAG_FAVOURITE;
	return AE_TAG_NONE;
}

char const *ae_party_tag_word(int tag)
{
	static char const *const words[] = { "", "YOU", "LEADER", "PARTY", "FAVOURITE", "READY", "PENDING", "OFFLINE ONLY", "BOT", "NEW",
		"STAYING" };

	return tag >= 0 && tag <= AE_TAG_STAYING ? words[tag] : "";
}

char const *ae_party_state_word(int state)
{
	static char const *const words[] = { "in lobby", "invited", "ready", "searching", "in game", "offline" };

	return state >= 0 && state <= AE_PARTY_OFFLINE ? words[state] : "";
}

int ae_party_rule_parse(char const *text, struct ae_party_rule *rule)
{
	struct ae_party_rule read = { AE_PARTY_RULE_TEAM, 0, 0 };
	char word[16];
	int length = 0;

	while (*text == ' ' || *text == '\t')
		text++;
	while (text[length] && text[length] != ' ' && text[length] != '\t' && length < (int)sizeof(word) - 1)
	{
		word[length] = text[length];
		length++;
	}
	word[length] = 0;
	text += length;
	while (*text == ' ' || *text == '\t')
		text++;
	if (same_text(word, "team"))
		read.kind = AE_PARTY_RULE_TEAM;
	else if (same_text(word, "solo"))
		read.kind = AE_PARTY_RULE_SOLO;
	else if (same_text(word, "any"))
	{
		read.kind = AE_PARTY_RULE_ANY;
		read.high = atoi(text);
		if (read.high < 0)
			read.high = 0;
	}
	else if (same_text(word, "range"))
	{
		char *dash;

		read.kind = AE_PARTY_RULE_RANGE;
		read.low = atoi(text);
		dash = strchr(text, '-');
		if (!dash || read.low < 1)
			return 0;
		read.high = atoi(dash + 1);
		if (read.high < read.low)
			return 0;
	}
	else
		return 0;
	*rule = read;
	return 1;
}

void ae_party_rule_derive(int teams, struct ae_party_rule *rule)
{
	rule->kind = teams ? AE_PARTY_RULE_TEAM : AE_PARTY_RULE_SOLO;
	rule->low = rule->high = 0;
}

int ae_party_rule_limit(struct ae_party_rule const *rule, int teams, int size)
{
	int game = teams ? teams * size : size, limit;

	switch (rule->kind)
	{
	case AE_PARTY_RULE_SOLO:
		limit = 1;
		break;
	case AE_PARTY_RULE_ANY:
		limit = rule->high > 0 && rule->high < game ? rule->high : game;
		break;
	case AE_PARTY_RULE_RANGE:
		limit = rule->high;
		break;
	default:
		limit = teams ? size : 1;
		break;
	}
	if (limit > game)
		limit = game;
	if (limit > AE_PARTY_MAX)
		limit = AE_PARTY_MAX;
	return limit < 1 ? 1 : limit;
}

int ae_party_sit_out(struct ae_party const *party, int source, int limit, int *out, int max)
{
	int at, seats = ae_party_search_seats(party, source), count = 0;

	for (at = party->count - 1; at > 0 && seats > limit && count < max; at--)
	{
		if (!ae_party_takes_part(&party->member[at], source))
			continue;
		out[count++] = at;
		seats -= party->member[at].seats;
	}
	return count;
}

int ae_party_keep_on_team(int *team, int const *party, int count, int leader_team)
{
	int at, other, swaps = 0;

	for (at = 0; at < count; at++)
	{
		if (!party[at] || team[at] == leader_team)
			continue;
		for (other = 0; other < count; other++)
			if (!party[other] && team[other] == leader_team)
			{
				team[other] = team[at];
				team[at] = leader_team;
				swaps++;
				break;
			}
	}
	return swaps;
}

int ae_party_tier_drift(int tier, int player_tier, int shared_games)
{
	int steps = shared_games / 5, gap = player_tier - tier;

	if (steps <= 0 || gap == 0)
		return 0;
	if (steps > (gap < 0 ? -gap : gap))
		steps = gap < 0 ? -gap : gap;
	return gap < 0 ? -steps : steps;
}

void ae_party_initial(char const *name, char *out, int size)
{
	unsigned char const *at = (unsigned char const *)name;
	unsigned int code;
	int length;

	if (size < 5)
	{
		if (size > 0)
			out[0] = 0;
		return;
	}
	if (!at || !at[0])
	{
		snprintf(out, (size_t)size, "?");
		return;
	}
	if (at[0] < 0x80)
		length = 1;
	else if ((at[0] & 0xE0) == 0xC0 && (at[1] & 0xC0) == 0x80)
		length = 2;
	else if ((at[0] & 0xF0) == 0xE0 && (at[1] & 0xC0) == 0x80 && (at[2] & 0xC0) == 0x80)
		length = 3;
	else if ((at[0] & 0xF8) == 0xF0 && (at[1] & 0xC0) == 0x80 && (at[2] & 0xC0) == 0x80 && (at[3] & 0xC0) == 0x80)
		length = 4;
	else
	{
		/* (a stray byte: no character) */
		snprintf(out, (size_t)size, "?");
		return;
	}
	if (length == 1)
	{
		out[0] = (char)(at[0] >= 'a' && at[0] <= 'z' ? at[0] - 32 : at[0]);
		out[1] = 0;
		return;
	}
	memcpy(out, at, (size_t)length);
	out[length] = 0;
	if (length == 2)
	{
		code = (unsigned int)((at[0] & 0x1F) << 6 | (at[1] & 0x3F));
		/* (Latin-1 letters, Greek and Cyrillic small letters: upper case) */
		if ((code >= 0xE0 && code <= 0xFE && code != 0xF7) || (code >= 0x3B1 && code <= 0x3C9 && code != 0x3C2) ||
			(code >= 0x430 && code <= 0x44F))
			code -= 0x20;
		else if (code >= 0x450 && code <= 0x45F)
			code -= 0x50;
		else
			return;
		out[0] = (char)(0xC0 | (code >> 6));
		out[1] = (char)(0x80 | (code & 0x3F));
	}
}
