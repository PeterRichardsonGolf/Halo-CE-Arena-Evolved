/* ae_party_list_test.c: MY PARTY's rows (ae_party_list.h) */

#include <stdio.h>
#include <string.h>

#include "ae_party_list.h"

static int failures;
#define CHECK(condition) do { if (!(condition)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
	failures++; } } while (0)

static void player(struct ae_lobby_roster *roster, int local, int host, int bot)
{
	struct ae_lobby_player *p = &roster->players[roster->count++];

	memset(p, 0, sizeof(*p));
	p->local = local;
	p->host = host;
	p->bot = bot;
	roster->people += !bot;
}

static struct ae_lobby_roster roster;

int main(void)
{
	struct ae_party_item items[AE_PARTY_ITEMS];
	struct ae_party_rules folded = { 0, 1, 1, 1, 1 }, view = { 1, 1, 1, 1, 1 }, joiner = { 1, 0, 1, 1, 0 };
	short count;

	/* the host (this PC), a joined player, a bot, this PC's second player, a bot */
	player(&roster, 1, 1, 0);
	player(&roster, 0, 0, 0);
	player(&roster, 0, 0, 1);
	player(&roster, 1, 0, 0);
	player(&roster, 0, 0, 1);

	/* folded: this PC's people first, then the others, then the bots; no rows of actions, no SEE ALL */
	count = ae_party_items(&roster, &folded, items);
	CHECK(count == 5);
	CHECK(items[0].kind == AE_PARTY_ITEM_PERSON && items[0].card == 0);
	CHECK(items[1].kind == AE_PARTY_ITEM_PERSON && items[1].card == 3);
	CHECK(items[2].kind == AE_PARTY_ITEM_PERSON && items[2].card == 1);
	CHECK(items[3].kind == AE_PARTY_ITEM_BOT && items[3].card == 2);
	CHECK(items[4].kind == AE_PARTY_ITEM_BOT && items[4].card == 4);

	/* the party view: people, ADD, SHUFFLE, BOTS, BOT SKILL, BOT SETUP, the bots, SEE ALL */
	count = ae_party_items(&roster, &view, items);
	CHECK(count == 11);
	CHECK(items[3].kind == AE_PARTY_ITEM_ADD && items[4].kind == AE_PARTY_ITEM_SHUFFLE);
	CHECK(items[5].kind == AE_PARTY_ITEM_BOTS && items[6].kind == AE_PARTY_ITEM_BOT_SKILL);
	CHECK(items[7].kind == AE_PARTY_ITEM_BOT_SETUP && items[8].kind == AE_PARTY_ITEM_BOT);
	CHECK(items[10].kind == AE_PARTY_ITEM_SEE_ALL && items[10].card == -1);
	/* sections: PLAYERS at the first row, BOTS at BOTS; not at the bots under it */
	CHECK(ae_party_section_starts(items, count, 0) && !ae_party_section_starts(items, count, 1));
	CHECK(ae_party_section_starts(items, count, 5) && !ae_party_section_starts(items, count, 8));
	CHECK(!ae_party_section_starts(items, count, 3));

	/* a joiner: no host's rows, no SHUFFLE */
	count = ae_party_items(&roster, &joiner, items);
	CHECK(count == 7);
	CHECK(items[3].kind == AE_PARTY_ITEM_ADD && items[4].kind == AE_PARTY_ITEM_BOT);
	/* (folded, the first bot starts BOTS) */
	CHECK(ae_party_section_starts(items, count, 4));

	/* found again: a player by their roster index, a row by its kind; else the fallback in range */
	count = ae_party_items(&roster, &view, items);
	{
		struct ae_party_item bot = { AE_PARTY_ITEM_BOT, 4 }, skill = { AE_PARTY_ITEM_BOT_SKILL, -1 };
		struct ae_party_item gone = { AE_PARTY_ITEM_PERSON, 9 };

		CHECK(ae_party_find(items, count, bot, 0) == 9);
		CHECK(ae_party_find(items, count, skill, 0) == 6);
		CHECK(ae_party_find(items, count, gone, 40) == count - 1);
		CHECK(ae_party_find(items, 0, gone, 0) == -1);
	}

	/* the window: 10 rows of 10 in 35: the focus at 6 shows rows 4-6; back up to 2 shows from 2 */
	{
		float heights[10] = { 10, 10, 10, 10, 10, 10, 10, 10, 10, 10 };

		CHECK(ae_party_first(heights, 10, 6, 0, 35.0f) == 4);
		CHECK(ae_party_first(heights, 10, 2, 4, 35.0f) == 2);
		CHECK(ae_party_first(heights, 10, 5, 4, 35.0f) == 4);
		CHECK(ae_party_first(heights, 10, -1, 12, 35.0f) == 9);
		/* (a row taller than the room: the focus first) */
		heights[7] = 50.0f;
		CHECK(ae_party_first(heights, 10, 7, 0, 35.0f) == 7);
	}
	/* no roster: no rows */
	CHECK(ae_party_items(NULL, &folded, items) == 0);

	if (failures)
		return 1;
	printf("ae_party_list_test: ok\n");
	return 0;
}
