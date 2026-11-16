#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ae_rating_result.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
#define EQ(a, b) do { long x_ = (long)(a), y_ = (long)(b); if (x_ != y_) { printf("FAIL %s:%d %s = %ld, want %ld\n", __FILE__, __LINE__, #a, x_, y_); failures++; } } while (0)

static struct ae_rating_result result, again;
static char text[AE_RATING_RESULT_TEXT];

static void add(char const *key, int level, int place, int team, int party)
{
	struct ae_rating_result_player *p = &result.player[result.count++];

	memset(p, 0, sizeof(*p));
	snprintf(p->key, sizeof(p->key), "%s", key);
	p->level = level; p->place = place; p->team = team; p->party = party;
	p->quit_order = -1; p->rated = 1;
}

static void build(void)
{
	ae_rating_result_init(&result);
	snprintf(result.playlist, sizeof(result.playlist), "team_slayer");
	result.duration_ms = 612000;
	add("goat", 12, 1, 0, 3);
	add("tiger", 12, 1, 0, 3);
	add("mouse", 12, 1, 0, -1);
	add("owl", 12, 0, 1, -1);
	add("fox", 12, 0, 1, -1);
}

static void test_change(void)
{
	build();
	EQ(ae_rating_result_change(&result, 0, 1000), -85);
	EQ(ae_rating_result_change(&result, 3, 1000), 100);
	EQ(ae_rating_result_find(&result, "owl"), 3);
	EQ(ae_rating_result_find(&result, "nobody"), -1);
	CHECK(!ae_rating_result_counts_quit(&result, 0));
	/* not rated: no change */
	result.player[3].rated = 0;
	EQ(ae_rating_result_change(&result, 3, 1000), 0);
	EQ(ae_rating_result_change(&result, 0, 1000), -85);   /* fox still counts as an opponent */
	result.player[4].rated = 0;
	EQ(ae_rating_result_change(&result, 0, 1000), 0);      /* no rated opponent left */
	build();
	/* mouse quits first, early: caps and protection */
	result.player[2].quit = 1; result.player[2].quit_order = 0;
	result.first_quit_ms = 30000;
	EQ(ae_rating_result_change(&result, 2, 1000), -85);
	CHECK(ae_rating_result_counts_quit(&result, 2));
	EQ(ae_rating_result_change(&result, 0, 1000), 0);
	EQ(ae_rating_result_change(&result, 3, 1000), AE_RATING_QUIT_EARLY_CAP);
	/* the same player rejoined: no quit at all, the plain change */
	result.player[2].rejoined = 1;
	result.first_quit_ms = -1;
	EQ(ae_rating_result_change(&result, 2, 1000), -85);
	EQ(ae_rating_result_change(&result, 3, 1000), 100);
	CHECK(!ae_rating_result_counts_quit(&result, 2));
}

static void test_text(void)
{
	long length;

	build();
	result.player[2].quit = 1; result.player[2].quit_order = 0;
	result.player[1].quit = 1; result.player[1].quit_order = 1; result.player[1].rejoined = 1;
	result.first_quit_ms = 45000;
	snprintf(result.player[4].key, sizeof(result.player[4].key), "f|x\ny");
	length = ae_rating_result_write(&result, text, sizeof(text));
	CHECK(length > 0);
	CHECK(strstr(text, "version = 1\n") != NULL);
	CHECK(strstr(text, "playlist = team_slayer\n") != NULL);
	CHECK(strstr(text, "player = goat | 12 1 0 3 0 -1 0 1\n") != NULL);
	CHECK(strstr(text, "player = f?x?y | ") != NULL);
	CHECK(ae_rating_result_read(&again, text));
	EQ(again.count, 5);
	EQ(again.duration_ms, 612000);
	EQ(again.first_quit_ms, 45000);
	CHECK(!strcmp(again.playlist, "team_slayer"));
	EQ(again.player[2].quit, 1);
	EQ(again.player[2].quit_order, 0);
	EQ(again.player[1].rejoined, 1);
	EQ(again.player[0].party, 3);
	EQ(again.player[3].team, 1);
	/* the same changes from the record read back */
	{
		int i;

		for (i = 0; i < 5; i++)
			EQ(ae_rating_result_change(&again, i, 1000), ae_rating_result_change(&result, i, 1000));
	}
	{
		static char second[AE_RATING_RESULT_TEXT];

		EQ(ae_rating_result_write(&again, second, sizeof(second)), length);
	}
	CHECK(ae_rating_result_write(&result, text, 40) == -1);
	/* refusals, a newer version, unknown lines and fields */
	CHECK(!ae_rating_result_read(&again, ""));
	CHECK(!ae_rating_result_read(&again, "player = a | 1 0 0 0 0 -1 0 1\n"));
	CHECK(!ae_rating_result_read(&again, "version = 1\nnonsense\n"));
	CHECK(ae_rating_result_read(&again, "version = 2\nseason = 4\nplayer = a | 1 0 0 0 0 -1 0 1 9 9\nplayer = bad |\n"));
	CHECK(again.newer);
	EQ(again.count, 1);
	EQ(again.first_quit_ms, -1);
	CHECK(ae_rating_result_read(&again, "version = 1\n"));
	EQ(again.count, 0);
}

int main(void)
{
	test_change();
	test_text();
	if (failures)
		return 1;
	printf("ok\n");
	return 0;
}
