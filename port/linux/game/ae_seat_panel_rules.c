/*
AE_SEAT_PANEL_RULES.C

THE SEAT PANEL's pure rules (ae_seat_panel_rules.h).
*/

#include <math.h>
#include <wchar.h>

#include "ae_seat_panel_rules.h"

#define QUARTER_VIEW_SCALE 0.8f
#define QUARTER_TWO_ROWS_RISE 6.0f

static void put(wchar_t const *role, wchar_t const *name, int name_length, int dots, wchar_t *out, int size)
{
	int length = 0;
	int index;

	for (index = 0; role[index] && length < size - 1; index++)
		out[length++] = role[index];
	if (name_length > 0 && length < size - 1)
		out[length++] = L' ';
	for (index = 0; index < name_length && length < size - 1; index++)
		out[length++] = name[index];
	for (index = 0; index < dots && length < size - 1; index++)
		out[length++] = L'.';
	out[length] = 0;
}

void ae_seat_name_clip(wchar_t const *role, wchar_t const *name, float bar_width, float (*measure)(wchar_t const *),
	wchar_t *out, int size)
{
	int name_length = (int)wcslen(name);
	int keep;

	if (size <= 0)
		return;
	put(role, name, name_length, 0, out, size);
	if (measure(out) <= bar_width)
		return;
	for (keep = name_length - 1; keep > 0; keep--)
	{
		put(role, name, keep, 3, out, size);
		if (measure(out) <= bar_width)
			return;
	}
	put(role, name, 0, 0, out, size);
}

int ae_seat_pips_lit(float vitality)
{
	int lit;

	if (!(vitality > 0.0f))
		return 0;
	lit = (int)ceilf(vitality * (float)AE_SEAT_PIPS);
	return lit > AE_SEAT_PIPS ? AE_SEAT_PIPS : lit;
}

int ae_seat_pips_red(int lit)
{
	return lit <= AE_SEAT_PIPS_RED;
}

float ae_seat_panel_top(float origin_y, int seats, float scale)
{
	return origin_y - (seats > 2 && scale < QUARTER_VIEW_SCALE ? QUARTER_TWO_ROWS_RISE : 0.0f);
}

int ae_seat_columns(int seats, float scale, float left, float view_width)
{
	int fit = (int)((view_width - left) / AE_SEAT_PITCH);
	int columns = scale < QUARTER_VIEW_SCALE && seats > 2 ? 2 : seats;

	if (columns > fit)
		columns = fit;
	return columns < 1 ? 1 : columns;
}

void ae_seat_cell(int index, int columns, float left, float top, float row_height, float *x, float *y)
{
	*x = left + (float)(index % columns) * AE_SEAT_PITCH;
	*y = top + (float)(index / columns) * row_height;
}

int ae_seat_roll(int count, struct ae_seat_kind const *seats, struct ae_seat_line *lines)
{
	int lines_count = 0;
	int index, other;

	if (count < 1 || !seats || !lines)
		return 0;
	if (count > AE_SEAT_ROLL_MOST)
		count = AE_SEAT_ROLL_MOST;
	for (index = 0; index < count; index++)
	{
		int rolled = 0;

		if (count > AE_SEAT_ROLL_ABOVE && seats[index].open)
		{
			/* (an open seat of a kind already shown is added to that line) */
			for (other = 0; other < lines_count; other++)
			{
				int first = lines[other].seat;

				if (lines[other].open > 0 && seats[first].gunner == seats[index].gunner)
				{
					lines[other].open++;
					rolled = 1;
					break;
				}
			}
		}
		if (!rolled)
		{
			lines[lines_count].seat = index;
			lines[lines_count].open = seats[index].open ? 1 : 0;
			lines_count++;
		}
	}
	return lines_count;
}

void ae_seat_open_text(int open, wchar_t *out, int size)
{
	wchar_t const word[] = L"OPEN";
	int length = 0, index;

	if (!out || size < 1)
		return;
	for (index = 0; word[index] && length < size - 1; index++)
		out[length++] = word[index];
	if (open > 1)
	{
		if (open > 99)
			open = 99;
		if (length < size - 1)
			out[length++] = L' ';
		if (length < size - 1)
			out[length++] = L'x';
		if (open > 9 && length < size - 1)
			out[length++] = (wchar_t)(L'0' + open / 10);
		if (length < size - 1)
			out[length++] = (wchar_t)(L'0' + open % 10);
	}
	out[length] = 0;
}
