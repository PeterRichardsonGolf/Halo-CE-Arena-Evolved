/*
AE_PARTY_STORE_RULES_TEST.C

The party store (port/linux/game/ae_party_store_rules.c): recent players, favourites, blocks, the record, the text.
*/

#include <stdio.h>
#include <string.h>

#include "../game/ae_party_store_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
#define EQ(a, b) do { long x_ = (long)(a), y_ = (long)(b); if (x_ != y_) { printf("FAIL %s:%d %s = %ld, wanted %ld\n", __FILE__, __LINE__, #a, x_, y_); failures++; } } while (0)

static void test_recent(void)
{
	static struct ae_party_store store;
	struct ae_party_record const *out[AE_STORE_RECORDS];
	char id[24];
	int at;

	ae_party_store_clear(&store);
	CHECK(ae_party_store_touch(&store, "", "x", AE_MEMBER_BOT, 3) == NULL);
	for (at = 0; at < 130; at++)
	{
		snprintf(id, sizeof(id), "p%d", at);
		ae_party_store_touch(&store, id, id, AE_MEMBER_FRIEND, 0);
	}
	/* (the last AE_STORE_RECENT; the oldest are dropped) */
	EQ(store.count, AE_STORE_RECORDS);
	EQ(ae_party_store_recent(&store, NULL, out, AE_STORE_RECORDS), AE_STORE_RECORDS);
	CHECK(!strcmp(out[0]->id, "p129") && !strcmp(out[1]->id, "p128"));
	CHECK(ae_party_store_find(&store, "p0") == NULL);
	/* (touched again: first) */
	ae_party_store_touch(&store, "p60", NULL, AE_MEMBER_FRIEND, 0);
	EQ(ae_party_store_recent(&store, NULL, out, 3), 3);
	CHECK(!strcmp(out[0]->id, "p60") && !strcmp(out[1]->id, "p129"));
}

static void test_saved_and_blocked(void)
{
	static struct ae_party_store store;
	struct ae_party_record const *out[AE_STORE_RECORDS];
	struct ae_party party;
	struct ae_party_member member;
	char id[24];
	int at;

	ae_party_store_clear(&store);
	for (at = 0; at < 12; at++)
	{
		snprintf(id, sizeof(id), "bot:%d", at);
		ae_party_store_touch(&store, id, id, AE_MEMBER_BOT, 7);
	}
	/* (eight favourites at most) */
	for (at = 0; at < 8; at++)
	{
		snprintf(id, sizeof(id), "bot:%d", at);
		EQ(ae_party_store_set_saved(&store, id, 1), 1);
	}
	EQ(ae_party_store_set_saved(&store, "bot:9", 1), 0);
	EQ(ae_party_store_set_saved(&store, "nobody", 1), 0);
	EQ(ae_party_store_saved(&store, out, 20), 8);
	CHECK(!strcmp(out[0]->id, "bot:7"));
	EQ(ae_party_store_set_saved(&store, "bot:0", 0), 1);
	EQ(ae_party_store_set_saved(&store, "bot:9", 1), 1);
	/* (a block is never offered and takes the favourite off) */
	EQ(ae_party_store_set_blocked(&store, "bot:11", 1), 1);
	EQ(ae_party_store_set_blocked(&store, "bot:9", 1), 1);
	EQ(ae_party_store_set_saved(&store, "bot:9", 1), 0);
	EQ(ae_party_store_recent(&store, NULL, out, 20), 10);
	for (at = 0; at < 10; at++)
		CHECK(strcmp(out[at]->id, "bot:11") && strcmp(out[at]->id, "bot:9"));
	/* (a party's members are left out of the recent list) */
	ae_party_clear(&party);
	ae_party_bot(&member, "x", 5);
	snprintf(member.id, sizeof(member.id), "bot:10");
	ae_party_add(&party, &member);
	EQ(ae_party_store_recent(&store, &party, out, 20), 9);
	/* (a full saved list does not drop a favourite for a new record) */
	ae_party_store_touch(&store, "bot:new", "new", AE_MEMBER_BOT, 5);
	CHECK(ae_party_store_find(&store, "bot:1") != NULL);
}

static void test_record(void)
{
	static struct ae_party_store store;
	struct ae_party_member member;
	struct ae_party_record *record;

	ae_party_store_clear(&store);
	ae_party_store_touch(&store, "bot:TheRhino", "TheRhino", AE_MEMBER_BOT, 7);
	ae_party_store_game(&store, "bot:nobody", 1, 1);
	ae_party_store_game(&store, "bot:TheRhino", 1, 1);
	record = ae_party_store_find(&store, "bot:TheRhino");
	CHECK(record && !record->saved && record->together_games == 1 && record->together_wins == 1);
	/* (the second game together: a favourite) */
	ae_party_store_game(&store, "bot:TheRhino", 1, 0);
	CHECK(record->saved && record->together_games == 2 && record->together_wins == 1);
	ae_party_store_game(&store, "bot:TheRhino", 0, 1);
	ae_party_store_game(&store, "bot:TheRhino", 0, 0);
	ae_party_store_game(&store, "bot:TheRhino", 0, 0);
	EQ(record->against_games, 3);
	EQ(record->against_wins, 1);
	ae_party_store_member(record, &member);
	CHECK(!strcmp(member.name, "TheRhino") && member.kind == AE_MEMBER_BOT && member.saved && member.tier == 7);
	EQ(member.together_games, 2);
	EQ(member.against_games, 3);
	/* (a person is not made a favourite by games) */
	ae_party_store_touch(&store, "K7QD", "Kestrel44", AE_MEMBER_FRIEND, 0);
	ae_party_store_game(&store, "K7QD", 1, 1);
	ae_party_store_game(&store, "K7QD", 1, 1);
	CHECK(!ae_party_store_find(&store, "K7QD")->saved);
}

static void test_read_limits(void)
{
	static struct ae_party_store store;
	static char text[AE_STORE_TEXT * 2];
	char line[160];
	int at, saved = 0;

	/* (a file of 130 lines: the newest 108 are kept, at most 8 favourites, the newest) */
	text[0] = 0;
	for (at = 0; at < 130; at++)
	{
		snprintf(line, sizeof(line), "p%d\tName%d\t2\t%d\t0\t0\t%d\t0\t0\t0\t0\n", at, at, at < 20 ? 1 : 0, at + 1);
		strcat(text, line);
	}
	EQ(ae_party_store_read(&store, text), AE_STORE_RECORDS);
	CHECK(ae_party_store_find(&store, "p129") != NULL);
	CHECK(ae_party_store_find(&store, "p30") != NULL && ae_party_store_find(&store, "p22") == NULL);
	CHECK(ae_party_store_find(&store, "p19") != NULL && ae_party_store_find(&store, "p19")->saved);
	CHECK(ae_party_store_find(&store, "p0") == NULL);
	for (at = 0; at < store.count; at++)
		saved += store.record[at].saved;
	CHECK(saved <= AE_STORE_SAVED);
	/* (a seen value beyond the clock's limit is clamped; the clock stays inside a 32-bit long) */
	EQ(ae_party_store_read(&store, "a\tA\t1\t0\t0\t0\t2147483647\t0\t0\t0\t0\nb\tB\t1\t0\t0\t0\t-5\t0\t0\t0\t0\n"), 2);
	CHECK(store.record[0].seen == AE_STORE_CLOCK_MAX && store.record[1].seen == 0 && store.clock == AE_STORE_CLOCK_MAX);
	ae_party_store_touch(&store, "b", NULL, AE_MEMBER_BOT, 0);
	CHECK(store.clock < 100 && store.record[1].seen > store.record[0].seen);
	{
		struct ae_party_record const *out[4];

		EQ(ae_party_store_recent(&store, NULL, out, 4), 2);
		CHECK(!strcmp(out[0]->id, "b") && !strcmp(out[1]->id, "a"));
	}
}

static void test_text(void)
{
	static struct ae_party_store store, again;
	static char text[AE_STORE_TEXT];
	struct ae_party_record const *out[8];
	int length;

	ae_party_store_clear(&store);
	ae_party_store_touch(&store, "bot:TheRhino", "TheRhino", AE_MEMBER_BOT, 7);
	ae_party_store_game(&store, "bot:TheRhino", 1, 1);
	ae_party_store_game(&store, "bot:TheRhino", 1, 1);
	ae_party_store_touch(&store, "K7QD-2MXA-9PWE", "Kestrel\t44\n", AE_MEMBER_FRIEND, 0);
	ae_party_store_set_blocked(&store, "K7QD-2MXA-9PWE", 1);
	length = ae_party_store_write(&store, text, sizeof(text));
	CHECK(length > 0);
	EQ(ae_party_store_read(&again, text), 2);
	CHECK(again.record[0].saved && again.record[0].together_games == 2 && !strcmp(again.record[0].name, "TheRhino"));
	CHECK(!strcmp(again.record[1].name, "Kestrel 44 ") && again.record[1].blocked);
	/* (the clock goes on: a touch after a restart is the newest) */
	ae_party_store_touch(&again, "bot:TheRhino", NULL, AE_MEMBER_BOT, 0);
	EQ(ae_party_store_recent(&again, NULL, out, 8), 1);
	EQ(ae_party_store_write(&store, text, 10), -1);
	EQ(ae_party_store_read(&again, "garbage\nonly\ttwo\n\n"), 0);
	EQ(ae_party_store_read(&again, ""), 0);
}

int main(void)
{
	test_recent();
	test_saved_and_blocked();
	test_record();
	test_read_limits();
	test_text();
	if (failures)
		printf("%d failures\n", failures);
	else
		printf("ok\n");
	return failures ? 1 : 0;
}
