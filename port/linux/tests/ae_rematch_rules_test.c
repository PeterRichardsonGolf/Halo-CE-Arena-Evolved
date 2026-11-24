/*
AE_REMATCH_RULES_TEST.C

The post-game lobby's rules (port/linux/game/ae_rematch_rules.c): leavers, the Still-there counter, the header's words.
*/

#include <stdio.h>
#include <string.h>

#include "../game/ae_rematch_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
#define EQ(a, b) do { long x_ = (long)(a), y_ = (long)(b); if (x_ != y_) { printf("FAIL %s:%d %s = %ld, wanted %ld\n", __FILE__, __LINE__, #a, x_, y_); failures++; } } while (0)

static void test_leavers(void)
{
	int may[8] = { 0, 0, 0, 0, 1, 1, 1, 1 }, none[8] = { 0 }, out[8];
	unsigned long seed;
	int games = 0, left = 0, total_games = 4000, at, count;

	/* (never a seat that may not leave; at most 2; the same seed and game, the same leavers) */
	for (seed = 1; seed <= 200; seed++)
	{
		int again[8], again_count;

		count = ae_rematch_leavers(seed, 3, 8, may, out, 8);
		again_count = ae_rematch_leavers(seed, 3, 8, may, again, 8);
		CHECK(count <= AE_REMATCH_LEAVE_MAX && count == again_count);
		for (at = 0; at < count; at++)
			CHECK(out[at] >= 4 && out[at] == again[at] && (at == 0 || out[at] > out[at - 1]));
	}
	/* (about 15 percent of the seats that may leave, over many games) */
	for (games = 0; games < total_games; games++)
		left += ae_rematch_leavers(77, games, 8, may, out, 8) > 0;
	CHECK(left > total_games * 30 / 100 && left < total_games * 60 / 100);
	EQ(ae_rematch_leavers(5, 1, 8, none, out, 8), 0);
	/* (a quarter of the seats at most: 4 seats, 1 may go) */
	for (seed = 1; seed <= 300; seed++)
	{
		int all[16] = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };

		CHECK(ae_rematch_leavers(seed, 0, 4, all, out, 8) <= 1);
		CHECK(ae_rematch_leavers(seed, 0, 3, all, out, 8) == 0);
		CHECK(ae_rematch_leavers(seed, 0, 16, all, out, 1) <= 1);
	}
}

/* the whole rematch path of a real lobby: leavers from a roster of bots, each replaced by a fill-in no seat has */
static void test_fill_ins(void)
{
	unsigned long seed;
	int leavers_seen = 0, filled = 0;

	for (seed = 1; seed <= 400; seed++)
	{
		char names[8][24], pool_taken[16][24];
		int may[8] = { 0, 0, 1, 1, 1, 1, 1, 1 }, out[2], count, at, each;

		/* (the bots' names as a plan has them: the seed's first 7) */
		{
			char seeded[16][AE_SOCIAL_LOBBY_NAME];

			CHECK(ae_social_lobby_names(seed, 7, seeded));
			for (at = 0; at < 7; at++)
				snprintf(names[at], 24, "%s", seeded[at]);
		}
		count = ae_rematch_leavers(seed, 1, 8, may, out, 2);
		leavers_seen += count;
		for (at = 0; at < count; at++)
		{
			char fill[24];

			for (each = 0; each < 7; each++)
				snprintf(pool_taken[each], 24, "%s", names[each]);
			CHECK(ae_rematch_fill_in(seed + (unsigned long)at, (char const (*)[24])pool_taken, 7, fill));
			for (each = 0; each < 7; each++)
				CHECK(strcmp(fill, names[each]));
			filled += fill[0] != 0;
		}
	}
	/* (a leaver and its fill-in happen: the path is not dead) */
	CHECK(leavers_seen > 50 && filled == leavers_seen);
	/* every name of the pool taken: no fill-in */
	{
		char all[16][AE_SOCIAL_LOBBY_NAME], fill[24];

		CHECK(ae_social_lobby_names(5, 16, all));
		CHECK(!ae_rematch_fill_in(5, (char const (*)[24])all, 16, fill));
		CHECK(!ae_social_lobby_names(5, 17, all));
	}
}

static void test_still_there(void)
{
	struct ae_still_there st;

	ae_still_there_init(&st);
	/* two auto-started games with no input are allowed; the third lobby stops and asks */
	EQ(ae_still_there_auto_start(&st, 0), 1);
	EQ(ae_still_there_auto_start(&st, 0), 1);
	EQ(ae_still_there_auto_start(&st, 0), 0);
	EQ(ae_still_there_asking(&st), 1);
	/* the answer: the count starts again */
	ae_still_there_input(&st);
	EQ(ae_still_there_asking(&st), 0);
	EQ(ae_still_there_auto_start(&st, 0), 1);
	/* input in a lobby resets the count */
	EQ(ae_still_there_auto_start(&st, 1), 1);
	EQ(ae_still_there_auto_start(&st, 0), 1);
	EQ(ae_still_there_auto_start(&st, 0), 1);
	EQ(ae_still_there_auto_start(&st, 0), 0);
}

static void test_words(void)
{
	char text[96];

	ae_rematch_staying_text(5, 8, text, sizeof(text));
	CHECK(!strcmp(text, "5 of 8 staying"));
	ae_rematch_sub_text(0, 3, 8, 8, 5, 8, text, sizeof(text));
	CHECK(!strcmp(text, "Rematch  /  your party of 3 stays  /  votes in 5 / 8"));
	ae_rematch_sub_text(1, 3, 5, 8, 5, 0, text, sizeof(text));
	CHECK(!strcmp(text, "5 of 8 staying  /  your party of 3 stays"));
	ae_rematch_sub_text(0, 1, 8, 8, 0, 0, text, sizeof(text));
	CHECK(!strcmp(text, "Rematch"));
	ae_rematch_news(AE_NEWS_LEFT, "RowdyMuffin", 0, text, sizeof(text));
	CHECK(!strcmp(text, "RowdyMuffin left the lobby."));
	ae_rematch_news(AE_NEWS_JOINED_BOT, "Dusk44", 9, text, sizeof(text));
	CHECK(!strcmp(text, "Dusk44 joined (BOT, tier 9)."));
	ae_rematch_news(AE_NEWS_INVITE_SENT, "Dawnbreaker", 0, text, sizeof(text));
	CHECK(!strcmp(text, "Invite sent to Dawnbreaker."));
	ae_rematch_news(AE_NEWS_STAYING, "Mossy7", 0, text, sizeof(text));
	CHECK(!strcmp(text, "Mossy7 is staying."));
	ae_rematch_news(AE_NEWS_PARTY_LEFT, "xMirax", 0, text, sizeof(text));
	CHECK(!strcmp(text, "xMirax left your party."));
	ae_rematch_news(AE_NEWS_PARTY_FULL, "", 4, text, sizeof(text));
	CHECK(!strcmp(text, "Your party is full (4 seats)."));
	ae_rematch_news(99, "x", 0, text, sizeof(text));
	CHECK(!text[0]);
	CHECK(!strcmp(ae_rematch_choice_word(0), "SAME AGAIN") && !ae_rematch_choice_word(1)[0]);
}

int main(void)
{
	test_leavers();
	test_fill_ins();
	test_still_there();
	test_words();
	if (failures)
		printf("%d failures\n", failures);
	else
		printf("ok\n");
	return failures ? 1 : 0;
}
