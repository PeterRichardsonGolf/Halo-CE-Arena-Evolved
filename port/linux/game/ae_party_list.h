/*
AE_PARTY_LIST.H

(0.4.2) MY PARTY's rows in CREATE GAME's lobby, in plain types (ae_party_list.c; unit test
port/linux/tests/ae_party_list_test.c). Folded (the party only shows): the people, this PC's first, then the bots. The
party view (the focus in it): the people, ADD PLAYER, SHUFFLE TEAMS, then BOTS, BOT SKILL, BOT SETUP..., the bots,
and SEE ALL (the whole lobby, its own screen). A joiner's: the people, ADD PLAYER, the bots, SEE ALL. Pure, no engine
includes.
*/

#ifndef __AE_PARTY_LIST_H
#define __AE_PARTY_LIST_H

#include "ae_lobby_roster.h"

enum
{
	AE_PARTY_ITEM_PERSON, AE_PARTY_ITEM_ADD, AE_PARTY_ITEM_SHUFFLE, AE_PARTY_ITEM_BOTS, AE_PARTY_ITEM_BOT_SKILL,
	AE_PARTY_ITEM_BOT_SETUP, AE_PARTY_ITEM_BOT, AE_PARTY_ITEM_SEE_ALL
};
enum { AE_PARTY_ITEMS = AE_LOBBY_MAXIMUM_PLAYERS + 8 };

/* a row: its kind, and the roster's index of a person's or bot's (-1 for the others) */
struct ae_party_item { short kind, card; };

/* what the lobby offers now: the party view (else folded), the host's rows (BOTS, BOT SKILL), ADD PLAYER, SHUFFLE
TEAMS (a team game's), BOT SETUP... (while there are bots) */
struct ae_party_rules { int view, host, add, shuffle, bot_setup; };

/* the rows; returns their count (at most AE_PARTY_ITEMS) */
short ae_party_items(struct ae_lobby_roster const *roster, struct ae_party_rules const *rules,
	struct ae_party_item *items);
/* the row of wanted again (the same kind, and the same player for a person's or bot's), else fallback within the
rows (-1 when there are none) */
short ae_party_find(struct ae_party_item const *items, short count, struct ae_party_item wanted, short fallback);
/* whether the row starts a section: PLAYERS (the first person's, or the first row) or BOTS (the first of BOTS, BOT
SKILL, BOT SETUP... and the bots) */
int ae_party_section_starts(struct ae_party_item const *items, short count, short index);
/* the first row drawn so that focus shows: rows of heights[] (their gaps in them) in room, from first; focus -1
keeps first (in range) */
short ae_party_first(float const *heights, short count, short focus, short first, float room);

#endif
