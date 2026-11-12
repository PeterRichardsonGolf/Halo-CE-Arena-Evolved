#include <stdio.h>
#include <string.h>
#include "ae_medals.h"
#include "ae_pgcr_screen_rules.h"
#include "ae_record_medals_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static unsigned long counts[AE_MEDAL_COUNT];
static int ids[AE_MEDAL_COUNT];

/* the record pane's own order before this page (ae_screen_record.c 0.7.6's medals_sorted): points, then the count, then
the number, the earned alone */
static int pane_order(int *medals)
{
	int count = 0, id, index, other;

	for (id = 0; id < AE_MEDAL_COUNT; id++)
	{
		if (counts[id])
			medals[count++] = id;
	}
	for (index = 1; index < count; index++)
	{
		int value = medals[index];

		for (other = index - 1; other >= 0; other--)
		{
			int p0 = ae_pgr_medal_points(medals[other]), p1 = ae_pgr_medal_points(value);

			if (p0 > p1 || (p0 == p1 && counts[medals[other]] >= counts[value]))
				break;
			medals[other + 1] = medals[other];
		}
		medals[other + 1] = value;
	}
	return count;
}

static void test_lists(void)
{
	int pane[AE_MEDAL_COUNT], n, index, category, sum, earned;

	memset(counts, 0, sizeof(counts));
	counts[AE_MEDAL_DOUBLE_KILL] = 17;        /* 5 points */
	counts[AE_MEDAL_OVERKILL] = 2;            /* 10 */
	counts[AE_MEDAL_HEADSHOT] = 42;           /* 10 */
	counts[AE_MEDAL_KILL] = 13;               /* 10 */
	counts[AE_MEDAL_TRIPLE_OF_EACH_KILL] = 3; /* 25 */
	counts[AE_MEDAL_KILLING_SPREE] = 8;       /* 5 */
	counts[AE_MEDAL_SNIPER_KILL] = 90;        /* 0 */
	/* every kind shows; the earned first */
	n = ae_rm_list(counts, AE_RM_SORT_RAREST, AE_RM_ALL, ids);
	CHECK(n == AE_MEDAL_COUNT && ae_rm_kinds(AE_RM_ALL) == AE_MEDAL_COUNT);
	CHECK(ae_rm_earned(counts, AE_RM_ALL) == 7 && ae_rm_total(counts) == 17 + 2 + 42 + 13 + 3 + 8 + 90);
	/* RAREST FIRST: points, more first, then the count, then the number */
	CHECK(ids[0] == AE_MEDAL_TRIPLE_OF_EACH_KILL && ids[1] == AE_MEDAL_HEADSHOT && ids[2] == AE_MEDAL_KILL &&
		ids[3] == AE_MEDAL_OVERKILL && ids[4] == AE_MEDAL_DOUBLE_KILL && ids[5] == AE_MEDAL_KILLING_SPREE &&
		ids[6] == AE_MEDAL_SNIPER_KILL);
	/* the same order as the pane's: its first cells are the pane's rows */
	CHECK(pane_order(pane) == 7);
	for (index = 0; index < 7; index++)
		CHECK(pane[index] == ids[index]);
	/* then the kinds not earned, by number, each once */
	for (index = 7; index + 1 < n; index++)
		CHECK(!counts[ids[index]] && ids[index] < ids[index + 1]);
	/* MOST EARNED: the count, then the points, then the number */
	n = ae_rm_list(counts, AE_RM_SORT_MOST, AE_RM_ALL, ids);
	CHECK(ids[0] == AE_MEDAL_SNIPER_KILL && ids[1] == AE_MEDAL_HEADSHOT && ids[2] == AE_MEDAL_DOUBLE_KILL &&
		ids[3] == AE_MEDAL_KILL && ids[4] == AE_MEDAL_KILLING_SPREE && ids[5] == AE_MEDAL_TRIPLE_OF_EACH_KILL &&
		ids[6] == AE_MEDAL_OVERKILL);
	counts[AE_MEDAL_KILL] = 42;
	ae_rm_list(counts, AE_RM_SORT_MOST, AE_RM_ALL, ids);
	/* (a tie of count and points: the lower number first) */
	CHECK(ids[1] == AE_MEDAL_KILL && ids[2] == AE_MEDAL_HEADSHOT);
	counts[AE_MEDAL_KILL] = 13;
	/* a category: its kinds alone, the same sort; the four add up to ALL */
	n = ae_rm_list(counts, AE_RM_SORT_RAREST, 1 + AE_PGR_FAMILY_MULTIKILL, ids);
	CHECK(n == ae_rm_kinds(1 + AE_PGR_FAMILY_MULTIKILL) && ae_rm_earned(counts, 1 + AE_PGR_FAMILY_MULTIKILL) == 3);
	CHECK(ids[0] == AE_MEDAL_TRIPLE_OF_EACH_KILL && ids[1] == AE_MEDAL_OVERKILL && ids[2] == AE_MEDAL_DOUBLE_KILL);
	for (index = 0; index < n; index++)
		CHECK(ae_pgr_medal_family(ids[index]) == AE_PGR_FAMILY_MULTIKILL);
	for (category = 1, sum = 0, earned = 0; category < AE_RM_CATEGORIES; category++)
	{
		sum += ae_rm_kinds(category);
		earned += ae_rm_earned(counts, category);
		CHECK(ae_rm_kinds(category) > 0);
	}
	CHECK(sum == AE_MEDAL_COUNT && earned == 7);
	CHECK(!ae_rm_in_category(-1, AE_RM_ALL) && !ae_rm_in_category(AE_MEDAL_COUNT, AE_RM_ALL) &&
		!ae_rm_in_category(0, AE_RM_CATEGORIES));
	/* the chips' words, round both ways */
	CHECK(!strcmp(ae_rm_category_name(AE_RM_ALL), "ALL") && !strcmp(ae_rm_category_name(4), "OBJECTIVE") &&
		!strcmp(ae_rm_category_name(5), ""));
	CHECK(ae_rm_category_step(AE_RM_ALL, -1) == 4 && ae_rm_category_step(4, 1) == AE_RM_ALL &&
		ae_rm_category_step(1, 1) == 2 && ae_rm_category_step(9, 1) == AE_RM_ALL);
	CHECK(ae_rm_find(ids, n, AE_MEDAL_OVERKILL) == 1 && ae_rm_find(ids, n, AE_MEDAL_HEADSHOT) == -1);
	/* nothing earned: every kind, by number */
	memset(counts, 0, sizeof(counts));
	n = ae_rm_list(counts, AE_RM_SORT_RAREST, AE_RM_ALL, ids);
	CHECK(n == AE_MEDAL_COUNT && ids[0] == 0 && ids[AE_MEDAL_COUNT - 1] == AE_MEDAL_COUNT - 1);
	CHECK(ae_rm_list(NULL, 0, 0, ids) == 0 && ae_rm_earned(NULL, 0) == 0);
}

static void test_grid(void)
{
	/* 36 cells: 9 rows; 8 show, the last faded until the end */
	CHECK(ae_rm_rows(36) == 9 && ae_rm_rows(0) == 0 && ae_rm_rows(1) == 1 && ae_rm_rows(33) == 9);
	CHECK(ae_rm_last_first(36) == 1 && ae_rm_last_first(32) == 0 && ae_rm_last_first(118) == 22);
	CHECK(ae_rm_faded(36, 0) && !ae_rm_faded(36, 1) && !ae_rm_faded(32, 0) && !ae_rm_faded(5, 0));
	CHECK(ae_rm_more(36, 0) == 4 && ae_rm_more(36, 1) == 0 && ae_rm_more(32, 0) == 0);
	CHECK(ae_rm_more(104, 4) == 56 && ae_rm_more(118, 22) == 0 && ae_rm_more(118, 99) == 0);
	/* the cursor going into the faded row scrolls a row; never onto a faded row */
	CHECK(ae_rm_keep_in_view(36, 24, 0) == 0);
	CHECK(ae_rm_keep_in_view(36, 28, 0) == 1);
	CHECK(ae_rm_keep_in_view(36, 35, 0) == 1);
	CHECK(ae_rm_keep_in_view(36, 0, 1) == 0);
	CHECK(ae_rm_keep_in_view(118, 117, 0) == 22 && ae_rm_keep_in_view(118, 60, 22) == 15);
	CHECK(ae_rm_keep_in_view(118, 60, 5) == 9 && ae_rm_keep_in_view(118, 60, 10) == 10);
	CHECK(ae_rm_keep_in_view(0, 5, 3) == 0 && ae_rm_keep_in_view(10, 50, 0) == 0);
	{
		int cursor, first = 0;

		/* down every row of 118 kinds: the cursor's row always shows, and never under the fade */
		for (cursor = 0; cursor < 118; cursor = ae_rm_step(118, cursor, 0, 1))
		{
			first = ae_rm_keep_in_view(118, cursor, first);
			CHECK(cursor / 4 >= first && cursor / 4 < first + AE_RM_ROWS);
			CHECK(!(ae_rm_faded(118, first) && cursor / 4 == first + AE_RM_ROWS - 1));
			if (cursor / 4 == 29)
				break;
		}
		CHECK(cursor == 116 && first == 22);
	}
	/* left and right stop at the row's ends; up and down a row, onto the last cell of a shorter row */
	CHECK(ae_rm_step(36, 4, -1, 0) == 4 && ae_rm_step(36, 3, 1, 0) == 3 && ae_rm_step(36, 5, 1, 0) == 6);
	CHECK(ae_rm_step(36, 1, 0, -1) == 1 && ae_rm_step(36, 33, 0, 1) == 33 && ae_rm_step(36, 30, 0, 1) == 34);
	CHECK(ae_rm_step(35, 31, 0, 1) == 34 && ae_rm_step(35, 34, 1, 0) == 34 && ae_rm_step(35, 33, 0, -1) == 29);
	CHECK(ae_rm_step(0, 5, 1, 0) == 0 && ae_rm_step(1, 0, 1, 1) == 0);
	/* LT / RT: seven rows, the column kept, held to the ends */
	CHECK(ae_rm_page(118, 1, 1) == 29 && ae_rm_page(118, 29, -1) == 1 && ae_rm_page(118, 9, -1) == 1);
	CHECK(ae_rm_page(118, 113, 1) == 117 && ae_rm_page(118, 111, 1) == 117 && ae_rm_page(118, 112, 1) == 116);
	CHECK(ae_rm_page(104, 0, 1) == 28 && ae_rm_page(104, 100, 1) == 100);
	/* the wheel: the window alone, held */
	CHECK(ae_rm_scroll(118, 0, 3) == 3 && ae_rm_scroll(118, 2, -5) == 0 && ae_rm_scroll(118, 20, 5) == 22);
	CHECK(ae_rm_scroll(30, 0, 4) == 0);
}

static void test_words(void)
{
	char text[32];

	ae_rm_count_text(0, text, sizeof(text));
	CHECK(!strcmp(text, "0"));
	ae_rm_count_text(999, text, sizeof(text));
	CHECK(!strcmp(text, "999"));
	ae_rm_count_text(2841, text, sizeof(text));
	CHECK(!strcmp(text, "2,841"));
	ae_rm_count_text(1234567, text, sizeof(text));
	CHECK(!strcmp(text, "1,234,567"));
	/* cut short, never past the room, always ended */
	ae_rm_count_text(1234567, text, 4);
	CHECK(strlen(text) < 4);
	ae_rm_range_text(36, 0, text, sizeof(text));
	CHECK(!strcmp(text, "1-32 OF 36"));
	ae_rm_range_text(104, 4, text, sizeof(text));
	CHECK(!strcmp(text, "17-48 OF 104"));
	ae_rm_range_text(118, 22, text, sizeof(text));
	CHECK(!strcmp(text, "89-118 OF 118"));
	ae_rm_range_text(5, 0, text, sizeof(text));
	CHECK(!strcmp(text, "1-5 OF 5"));
	ae_rm_range_text(0, 0, text, sizeof(text));
	CHECK(!strcmp(text, "0 OF 0"));
	ae_rm_points_text(10, text, sizeof(text));
	CHECK(!strcmp(text, "10 POINTS EACH"));
	ae_rm_points_text(1, text, sizeof(text));
	CHECK(!strcmp(text, "1 POINT EACH"));
	ae_rm_points_text(0, text, sizeof(text));
	CHECK(!strcmp(text, "NO POINTS"));
}

static void test_dev(void)
{
	unsigned long again[AE_MEDAL_COUNT];
	int id, big = 0;

	CHECK(ae_rm_dev_counts(counts, 36, 7) == 36 && ae_rm_earned(counts, AE_RM_ALL) == 36);
	/* the same seed, the same record; another seed, another */
	CHECK(ae_rm_dev_counts(again, 36, 7) == 36 && !memcmp(again, counts, sizeof(counts)));
	ae_rm_dev_counts(again, 36, 8);
	CHECK(memcmp(again, counts, sizeof(counts)) != 0);
	CHECK(ae_rm_dev_counts(counts, 104, 7) == 104 && ae_rm_earned(counts, AE_RM_ALL) == 104);
	for (id = 0; id < AE_MEDAL_COUNT; id++)
		big += counts[id] >= 1000;
	CHECK(big == 1);
	CHECK(ae_rm_dev_counts(counts, 0, 1) == 0 && ae_rm_total(counts) == 0);
	CHECK(ae_rm_dev_counts(counts, 500, 1) == AE_MEDAL_COUNT && ae_rm_earned(counts, AE_RM_ALL) == AE_MEDAL_COUNT);
	CHECK(ae_rm_dev_counts(counts, -3, 1) == 0 && ae_rm_dev_counts(NULL, 3, 1) == 0);
}

int main(void)
{
	test_lists();
	test_grid();
	test_words();
	test_dev();
	if (failures)
		return 1;
	printf("ae_record_medals_rules_test: ok\n");
	return 0;
}
