/*
AE_RECORD_MEDALS_RULES.C

The service record's MEDALS page as pure rules (ae_record_medals_rules.h).
*/

#include <stdio.h>
#include <string.h>

#include "ae_medals.h"
#include "ae_pgcr_screen_rules.h"
#include "ae_record_medals_rules.h"

/* ---------- what shows */

int ae_rm_in_category(int medal, int category)
{
	if (medal < 0 || medal >= AE_MEDAL_COUNT || category < 0 || category >= AE_RM_CATEGORIES)
		return 0;
	return category == AE_RM_ALL || (int)ae_pgr_medal_family(medal) == category - 1;
}

/* whether medal a goes before medal b among the earned */
static int before(unsigned long const *counts, int sort, int a, int b)
{
	int pa = ae_pgr_medal_points(a), pb = ae_pgr_medal_points(b);

	if (sort == AE_RM_SORT_MOST)
	{
		if (counts[a] != counts[b])
			return counts[a] > counts[b];
		if (pa != pb)
			return pa > pb;
		return a < b;
	}
	if (pa != pb)
		return pa > pb;
	if (counts[a] != counts[b])
		return counts[a] > counts[b];
	return a < b;
}

int ae_rm_list(unsigned long const *counts, int sort, int category, int *ids)
{
	int count = 0, id, index, other;

	if (!counts || !ids)
		return 0;
	for (id = 0; id < AE_MEDAL_COUNT; id++)
	{
		if (counts[id] && ae_rm_in_category(id, category))
			ids[count++] = id;
	}
	/* (an insertion: the lists are short) */
	for (index = 1; index < count; index++)
	{
		int value = ids[index];

		for (other = index - 1; other >= 0 && before(counts, sort, value, ids[other]); other--)
			ids[other + 1] = ids[other];
		ids[other + 1] = value;
	}
	/* (then the kinds not earned yet, by number) */
	for (id = 0; id < AE_MEDAL_COUNT; id++)
	{
		if (!counts[id] && ae_rm_in_category(id, category))
			ids[count++] = id;
	}
	return count;
}

int ae_rm_earned(unsigned long const *counts, int category)
{
	int count = 0, id;

	for (id = 0; counts && id < AE_MEDAL_COUNT; id++)
	{
		if (counts[id] && ae_rm_in_category(id, category))
			count++;
	}
	return count;
}

int ae_rm_kinds(int category)
{
	int count = 0, id;

	for (id = 0; id < AE_MEDAL_COUNT; id++)
	{
		if (ae_rm_in_category(id, category))
			count++;
	}
	return count;
}

unsigned long ae_rm_total(unsigned long const *counts)
{
	unsigned long total = 0;
	int id;

	for (id = 0; counts && id < AE_MEDAL_COUNT; id++)
		total += counts[id];
	return total;
}

char const *ae_rm_category_name(int category)
{
	static char const *const names[AE_RM_CATEGORIES] = { "ALL", "MULTIKILL", "SPREE", "SKILL", "OBJECTIVE" };

	return category >= 0 && category < AE_RM_CATEGORIES ? names[category] : "";
}

int ae_rm_category_step(int category, int direction)
{
	if (category < 0 || category >= AE_RM_CATEGORIES)
		return AE_RM_ALL;
	return (category + (direction < 0 ? AE_RM_CATEGORIES - 1 : 1)) % AE_RM_CATEGORIES;
}

int ae_rm_find(int const *ids, int count, int medal)
{
	int index;

	for (index = 0; ids && index < count; index++)
	{
		if (ids[index] == medal)
			return index;
	}
	return -1;
}

/* ---------- the grid */

int ae_rm_rows(int count)
{
	return count > 0 ? (count + AE_RM_COLUMNS - 1) / AE_RM_COLUMNS : 0;
}

int ae_rm_last_first(int count)
{
	int rows = ae_rm_rows(count);

	return rows > AE_RM_ROWS ? rows - AE_RM_ROWS : 0;
}

static int first_held(int count, int first)
{
	int last = ae_rm_last_first(count);

	return first < 0 ? 0 : first > last ? last : first;
}

int ae_rm_faded(int count, int first)
{
	return first_held(count, first) < ae_rm_last_first(count);
}

int ae_rm_more(int count, int first)
{
	int shown = (first_held(count, first) + AE_RM_ROWS) * AE_RM_COLUMNS;

	return count > shown ? count - shown : 0;
}

int ae_rm_keep_in_view(int count, int cursor, int first)
{
	int row, last = ae_rm_last_first(count);

	first = first_held(count, first);
	if (count <= 0)
		return 0;
	if (cursor < 0)
		cursor = 0;
	if (cursor >= count)
		cursor = count - 1;
	row = cursor / AE_RM_COLUMNS;
	if (row < first)
		return row;
	/* (the last row shown is faded while more follow: the cursor may sit on the rows above it) */
	if (row > first + AE_RM_ROWS - 2)
	{
		first = row - (AE_RM_ROWS - 2);
		if (first > last)
			first = last;
	}
	return first;
}

static int held(int count, int cursor)
{
	return cursor < 0 ? 0 : cursor >= count ? count - 1 : cursor;
}

int ae_rm_step(int count, int cursor, int dx, int dy)
{
	int row, column, rows = ae_rm_rows(count);

	if (count <= 0)
		return 0;
	cursor = held(count, cursor);
	row = cursor / AE_RM_COLUMNS;
	column = cursor % AE_RM_COLUMNS;
	if (dx < 0 && column > 0)
		cursor--;
	else if (dx > 0 && column < AE_RM_COLUMNS - 1 && cursor + 1 < count)
		cursor++;
	if (dy < 0 && row > 0)
		cursor -= AE_RM_COLUMNS;
	else if (dy > 0 && row + 1 < rows)
		cursor = held(count, cursor + AE_RM_COLUMNS);
	return cursor;
}

int ae_rm_page(int count, int cursor, int direction)
{
	int row, rows = ae_rm_rows(count);

	if (count <= 0)
		return 0;
	cursor = held(count, cursor);
	row = cursor / AE_RM_COLUMNS + (direction < 0 ? -AE_RM_PAGE_ROWS : AE_RM_PAGE_ROWS);
	if (row < 0)
		row = 0;
	if (row > rows - 1)
		row = rows - 1;
	return held(count, row * AE_RM_COLUMNS + cursor % AE_RM_COLUMNS);
}

int ae_rm_scroll(int count, int first, int rows)
{
	return first_held(count, first + rows);
}

/* ---------- words */

void ae_rm_count_text(unsigned long count, char *out, size_t size)
{
	char digits[24];
	size_t length, at = 0, index;

	if (!out || !size)
		return;
	snprintf(digits, sizeof(digits), "%lu", count);
	length = strlen(digits);
	for (index = 0; index < length && at + 1 < size; index++)
	{
		if (index > 0 && (length - index) % 3 == 0)
		{
			if (at + 2 >= size)
				break;
			out[at++] = ',';
		}
		out[at++] = digits[index];
	}
	out[at] = 0;
}

void ae_rm_range_text(int count, int first, char *out, size_t size)
{
	int from, to;

	if (!out || !size)
		return;
	if (count <= 0)
	{
		snprintf(out, size, "0 OF 0");
		return;
	}
	from = first_held(count, first) * AE_RM_COLUMNS + 1;
	to = from - 1 + AE_RM_ROWS * AE_RM_COLUMNS;
	if (to > count)
		to = count;
	snprintf(out, size, "%d-%d OF %d", from, to, count);
}

void ae_rm_points_text(int points, char *out, size_t size)
{
	if (!out || !size)
		return;
	if (points <= 0)
		snprintf(out, size, "NO POINTS");
	else
		snprintf(out, size, "%d POINT%s EACH", points, points == 1 ? "" : "S");
}

/* ---------- DEV MODE */

static unsigned long next_random(unsigned long *state)
{
	/* (a 32-bit linear congruential step: the same on every build) */
	*state = (*state * 1664525UL + 1013904223UL) & 0xFFFFFFFFUL;
	return *state >> 8;
}

int ae_rm_dev_counts(unsigned long *counts, int kinds, unsigned long seed)
{
	int order[AE_MEDAL_COUNT], index;
	unsigned long state = seed ^ 0x5EEDu;

	if (!counts)
		return 0;
	if (kinds < 0)
		kinds = 0;
	if (kinds > AE_MEDAL_COUNT)
		kinds = AE_MEDAL_COUNT;
	memset(counts, 0, sizeof(*counts) * AE_MEDAL_COUNT);
	for (index = 0; index < AE_MEDAL_COUNT; index++)
		order[index] = index;
	/* (a shuffle; the first kinds are earned) */
	for (index = AE_MEDAL_COUNT - 1; index > 0; index--)
	{
		int other = (int)(next_random(&state) % (unsigned long)(index + 1)), value = order[index];

		order[index] = order[other];
		order[other] = value;
	}
	for (index = 0; index < kinds; index++)
	{
		int points = ae_pgr_medal_points(order[index]);
		unsigned long most = points >= 25 ? 4 : points >= 15 ? 12 : points >= 10 ? 60 : 180;

		counts[order[index]] = 1 + next_random(&state) % most;
	}
	if (kinds >= 100)
		counts[order[0]] = 1000 + next_random(&state) % 2000;
	return kinds;
}
