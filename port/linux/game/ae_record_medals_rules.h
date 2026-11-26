/*
AE_RECORD_MEDALS_RULES.H

The service record's MEDALS page (0.8.1, ae_screen_record_medals.c) as pure rules (port/linux/tests/
ae_record_medals_rules_test.c runs them): which medals a category shows and in what order, the counts of its chips and its
head, the grid's cursor and its window of rows, and the words of the numbers. No engine headers: the page draws what these
decide.

Every kind shows, the ones not earned yet too: the earned kinds first in the sort, then the others (dimmed, a count of 0) by
their number. RAREST FIRST is the record pane's own order (points, more first, then the count, more first, then the
number), so the page's first cells match the pane the player left; MOST EARNED is the count, then the points, then the
number.

The grid is AE_RM_COLUMNS wide and shows AE_RM_ROWS rows. While more rows follow, its last row is under the overflow fade:
the cursor going into it scrolls the grid by a row, so the cursor is never on a faded row.
*/

#ifndef __AE_RECORD_MEDALS_RULES_H
#define __AE_RECORD_MEDALS_RULES_H

#include <stddef.h>

enum
{
	AE_RM_COLUMNS = 4,
	AE_RM_ROWS = 8,
	/* LT / RT and PgUp / PgDn: this many rows */
	AE_RM_PAGE_ROWS = 7,
	/* the chips: ALL, then the medals' four families (ae_pgr_family) as categories */
	AE_RM_ALL = 0,
	AE_RM_CATEGORIES = 5
};

enum ae_rm_sort { AE_RM_SORT_RAREST, AE_RM_SORT_MOST };

/* ---------- what shows */

/* whether a medal is in a category (AE_RM_ALL: every one; 1 + its family otherwise) */
int ae_rm_in_category(int medal, int category);
/* the medals of a category in the sort, the earned first, then the others by number: their ids into ids (room for
AE_MEDAL_COUNT), returning how many */
int ae_rm_list(unsigned long const *counts, int sort, int category, int *ids);
/* the kinds of a category earned (a count of 1 or more), and all its kinds */
int ae_rm_earned(unsigned long const *counts, int category);
int ae_rm_kinds(int category);
/* the medals earned, all their counts added */
unsigned long ae_rm_total(unsigned long const *counts);
/* a chip's word: "ALL", "MULTIKILL", "SPREE", "SKILL", "OBJECTIVE"; "" out of range */
char const *ae_rm_category_name(int category);
/* the next category, LB (-1) / RB (+1), round past the ends */
int ae_rm_category_step(int category, int direction);
/* the place of a medal in a list, -1 when it is not in it */
int ae_rm_find(int const *ids, int count, int medal);

/* ---------- the grid's cursor and window (cells of a list of count; first: the first row shown) */

/* the rows of count cells */
int ae_rm_rows(int count);
/* the last first row: the grid's end at its bottom */
int ae_rm_last_first(int count);
/* whether the grid's last row shows under the fade: more rows follow it */
int ae_rm_faded(int count, int first);
/* the cells after the grid's last row: "N more" (0: none) */
int ae_rm_more(int count, int first);
/* the first row that shows the cursor's row on a row that is not faded, moving the window as little as it can */
int ae_rm_keep_in_view(int count, int cursor, int first);
/* the cursor moved a cell: dx -1 / +1 stops at the row's ends (no wrap), dy -1 / +1 a row (onto the last cell when the
row below is shorter); the same at the grid's ends */
int ae_rm_step(int count, int cursor, int dx, int dy);
/* LT (-1) / RT (+1): AE_RM_PAGE_ROWS rows, held to the first and last rows, the column kept where the row has it */
int ae_rm_page(int count, int cursor, int direction);
/* the window scrolled by rows (the wheel), held to its ends; the cursor stays */
int ae_rm_scroll(int count, int first, int rows);

/* ---------- words */

/* a count with thousands' commas: "2,841" */
void ae_rm_count_text(unsigned long count, char *out, size_t size);
/* the cells shown of count: "1-32 OF 118"; "0 OF 0" for none */
void ae_rm_range_text(int count, int first, char *out, size_t size);
/* a medal's points: "10 POINTS EACH", "1 POINT EACH", "NO POINTS" */
void ae_rm_points_text(int points, char *out, size_t size);

/* ---------- DEV MODE (record_medals <kinds> [seed]) */

/* a made-up player's medals: kinds kinds earned (0 to AE_MEDAL_COUNT), picked by seed, with counts by how rare each is
(one of them past a thousand when there are 100 kinds or more, for the commas); the counts into counts, all others 0.
Returns the kinds earned */
int ae_rm_dev_counts(unsigned long *counts, int kinds, unsigned long seed);

#endif
