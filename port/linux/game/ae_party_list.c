/*
AE_PARTY_LIST.C

MY PARTY's rows (ae_party_list.h). Pure.
*/

#include "ae_party_list.h"

static short add(struct ae_party_item *items, short count, short kind, short card)
{
	if (count >= AE_PARTY_ITEMS)
		return count;
	items[count].kind = kind;
	items[count].card = card;
	return (short)(count + 1);
}

short ae_party_items(struct ae_lobby_roster const *roster, struct ae_party_rules const *rules,
	struct ae_party_item *items)
{
	short count = 0, index, players = roster ? roster->count : 0;
	int pass;

	if (players > AE_LOBBY_MAXIMUM_PLAYERS)
		players = AE_LOBBY_MAXIMUM_PLAYERS;
	/* the people: this PC's first, then the others (each in the roster's order) */
	for (pass = 0; pass < 2; pass++)
		for (index = 0; index < players; index++)
			if (!roster->players[index].bot && (roster->players[index].local != 0) == (pass == 0))
				count = add(items, count, AE_PARTY_ITEM_PERSON, index);
	if (rules->view)
	{
		if (rules->add)
			count = add(items, count, AE_PARTY_ITEM_ADD, -1);
		if (rules->host && rules->shuffle)
			count = add(items, count, AE_PARTY_ITEM_SHUFFLE, -1);
		if (rules->host)
		{
			count = add(items, count, AE_PARTY_ITEM_BOTS, -1);
			count = add(items, count, AE_PARTY_ITEM_BOT_SKILL, -1);
			if (rules->bot_setup)
				count = add(items, count, AE_PARTY_ITEM_BOT_SETUP, -1);
		}
	}
	for (index = 0; index < players; index++)
		if (roster->players[index].bot)
			count = add(items, count, AE_PARTY_ITEM_BOT, index);
	if (rules->view && players > 0)
		count = add(items, count, AE_PARTY_ITEM_SEE_ALL, -1);
	return count;
}

short ae_party_find(struct ae_party_item const *items, short count, struct ae_party_item wanted, short fallback)
{
	short index;

	if (count <= 0)
		return -1;
	for (index = 0; index < count; index++)
		if (items[index].kind == wanted.kind && (items[index].card == wanted.card ||
			(wanted.kind != AE_PARTY_ITEM_PERSON && wanted.kind != AE_PARTY_ITEM_BOT)))
			return index;
	if (fallback >= count)
		fallback = (short)(count - 1);
	return fallback < 0 ? 0 : fallback;
}

static int bots_section(short kind)
{
	return kind == AE_PARTY_ITEM_BOTS || kind == AE_PARTY_ITEM_BOT_SKILL || kind == AE_PARTY_ITEM_BOT_SETUP ||
		kind == AE_PARTY_ITEM_BOT;
}

int ae_party_section_starts(struct ae_party_item const *items, short count, short index)
{
	if (index < 0 || index >= count)
		return 0;
	if (index == 0)
		return 1;
	if (items[index].kind == AE_PARTY_ITEM_PERSON)
		return 0;
	return bots_section(items[index].kind) && !bots_section(items[index - 1].kind);
}

short ae_party_first(float const *heights, short count, short focus, short first, float room)
{
	float used;
	short index;

	if (count <= 0)
		return 0;
	if (first >= count)
		first = (short)(count - 1);
	if (first < 0)
		first = 0;
	if (focus < 0 || focus >= count)
		return first;
	if (focus < first)
		return focus;
	/* (down: the fewest rows off the top that bring the focus in) */
	for (;;)
	{
		used = 0.0f;
		for (index = first; index <= focus; index++)
			used += heights[index];
		if (used <= room || first >= focus)
			return first;
		first++;
	}
}
