/* ae_list.c: Arena Evolved menus, list arithmetic (see ae_list.h). No engine includes. */

#include "ae_list.h"

static short margin(struct ae_list const *list)
{
	return (short)(list->rows >= 3 ? 1 : 0);
}

/* the last window start: the last item on the last row */
static short last_first(struct ae_list const *list)
{
	return (short)(list->count > list->rows ? list->count - list->rows : 0);
}

static short clamp(int value, int low, int high)
{
	return (short)(value < low ? low : value > high ? high : value);
}

/* the window moved to show the focus with its margin, within the list */
static void follow_focus(struct ae_list *list)
{
	int first = list->first;

	if (list->count <= 0)
	{
		list->first = 0;
		list->focus = -1;
		return;
	}
	if (list->focus < first + margin(list))
		first = list->focus - margin(list);
	if (list->focus > first + list->rows - 1 - margin(list))
		first = list->focus - (list->rows - 1 - margin(list));
	list->first = clamp(first, 0, last_first(list));
}

/* the focus moved into the window with its margin (the window stays), except at the list's ends */
static void follow_window(struct ae_list *list)
{
	int low = list->first + (list->first > 0 ? margin(list) : 0);
	int high = list->first + list->rows - 1 - (list->first < last_first(list) ? margin(list) : 0);

	if (list->count <= 0)
	{
		list->first = 0;
		list->focus = -1;
		return;
	}
	list->focus = clamp(list->focus, low, high < list->count - 1 ? high : list->count - 1);
}

void ae_list_init(struct ae_list *list, short count, short rows)
{
	list->count = (short)(count > 0 ? count : 0);
	list->rows = (short)(rows > 0 ? rows : 1);
	list->first = 0;
	list->focus = (short)(list->count ? 0 : -1);
}

void ae_list_set_focus(struct ae_list *list, short focus)
{
	if (list->count <= 0)
	{
		follow_focus(list);
		return;
	}
	list->focus = clamp(focus, 0, list->count - 1);
	follow_focus(list);
}

void ae_list_move(struct ae_list *list, short delta, int wrap)
{
	int target;

	if (list->count <= 0)
		return;
	target = list->focus + delta;
	if (wrap && (target < 0 || target >= list->count))
		target = ((target % list->count) + list->count) % list->count;
	ae_list_set_focus(list, clamp(target, 0, list->count - 1));
}

void ae_list_page(struct ae_list *list, short direction)
{
	if (list->count <= 0 || !direction)
		return;
	ae_list_set_focus(list, clamp(list->focus + (direction < 0 ? -list->rows : list->rows), 0, list->count - 1));
}

void ae_list_set_count(struct ae_list *list, short count)
{
	list->count = (short)(count > 0 ? count : 0);
	if (list->count <= 0)
	{
		follow_focus(list);
		return;
	}
	ae_list_set_focus(list, (short)(list->focus < 0 ? 0 : list->focus));
}

short ae_list_item_at_row(struct ae_list const *list, short row)
{
	int item = list->first + row;

	if (row < 0 || row >= list->rows || item >= list->count)
		return -1;
	return (short)item;
}

void ae_list_scroll(struct ae_list *list, short steps)
{
	if (list->count <= 0)
		return;
	list->first = clamp(list->first + steps, 0, last_first(list));
	follow_window(list);
}
