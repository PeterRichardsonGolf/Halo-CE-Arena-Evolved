/*
AE_LIST.H

The AE menus' list arithmetic (ae_list.c): which items a list of `rows`
visible rows shows, and which one has the focus, as the player moves, pages,
scrolls the wheel, and as the list's items come and go (a server vanishes).
Pure C, no engine includes (unit test: port/linux/tests/ae_list_test.c).

Rules: the focus is always a valid item, or -1 when there are none; the window
(`first`) stays within the list; while there are more items beyond an edge,
the focus keeps one row of margin from that edge (lists of 3 rows or more),
so the next item is always in sight; at the list's ends the first and last
items may sit on the first and last rows.

Focus memory (ae_ui.h): a list screen's `ae_list.focus` is the live value. The
screen copies it into its `ae_screen.focus` in `leave` and restores it from
there in `enter` (ae_list_set_focus), so BACK returns to the row left.
*/

#ifndef __AE_LIST_H
#define __AE_LIST_H

struct ae_list { short count, rows, first, focus; };   /* focus -1: nothing to focus (count 0) */

/* count items (none below 0) in rows visible rows (at least 1); the focus on the first */
void ae_list_init(struct ae_list *list, short count, short rows);
/* moves the focus by delta (wraps only when wrap), keeping one row of margin before the edges while there is more */
void ae_list_move(struct ae_list *list, short delta, int wrap);
/* a page up (direction < 0) or down: the focus moves by the rows, the window follows */
void ae_list_page(struct ae_list *list, short direction);
/* a new count: the focus stays on its index, or the last item if that is gone */
void ae_list_set_count(struct ae_list *list, short count);
/* the focus on an item (clamped into the list; focus memory), the window following */
void ae_list_set_focus(struct ae_list *list, short focus);
/* the item under a visible row, or -1 */
short ae_list_item_at_row(struct ae_list const *list, short row);
void ae_list_scroll(struct ae_list *list, short steps);   /* wheel: moves the window, focus follows if it leaves */

#endif
