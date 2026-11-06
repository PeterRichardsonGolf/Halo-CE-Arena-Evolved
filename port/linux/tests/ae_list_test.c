#include <stdio.h>
#include "ae_list.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

/* the invariant every operation keeps (it restates ae_list.c's margin and window rules, so a shared misreading
of them would pass both; it checks independently that the focus is a valid item inside the window): focus a valid item (or -1 with none), the window inside the list, and the
focus inside the window with its margin, except at the list's ends */
static int valid(struct ae_list const *l)
{
	short margin = l->rows >= 3 ? 1 : 0;
	short last_first = l->count > l->rows ? (short)(l->count - l->rows) : 0;

	if (l->count <= 0)
		return l->focus == -1 && l->first == 0;
	if (l->focus < 0 || l->focus >= l->count || l->first < 0 || l->first > last_first)
		return 0;
	if (l->focus < l->first + margin && l->first > 0)
		return 0;
	if (l->focus > l->first + l->rows - 1 - margin && l->first < last_first)
		return 0;
	return l->focus >= l->first && l->focus < l->first + l->rows;
}

int main(void)
{
	struct ae_list l;
	short count, focus, rows;

	ae_list_init(&l, 0, 10);  CHECK(l.focus == -1 && l.first == 0);
	ae_list_move(&l, 1, 0);   CHECK(l.focus == -1);                 /* empty stays empty */
	ae_list_move(&l, 1, 1);   CHECK(l.focus == -1);
	ae_list_page(&l, 1);      CHECK(l.focus == -1 && l.first == 0);
	ae_list_scroll(&l, 3);    CHECK(l.focus == -1 && l.first == 0);
	CHECK(ae_list_item_at_row(&l, 0) == -1);
	ae_list_init(&l, 3, 10);  CHECK(l.focus == 0);
	ae_list_move(&l, 5, 0);   CHECK(l.focus == 2 && l.first == 0);  /* clamps, short list never scrolls */
	ae_list_move(&l, 1, 1);   CHECK(l.focus == 0);                  /* wraps when asked */
	ae_list_move(&l, -1, 1);  CHECK(l.focus == 2);                  /* both ways */
	ae_list_move(&l, -1, 0);  CHECK(l.focus == 1);
	ae_list_scroll(&l, 4);    CHECK(l.first == 0 && l.focus == 1);  /* a short list doesn't scroll */
	CHECK(ae_list_item_at_row(&l, 2) == 2 && ae_list_item_at_row(&l, 3) == -1);
	ae_list_init(&l, 30, 10);
	ae_list_move(&l, 8, 0);   CHECK(l.focus == 8 && l.first == 0);  /* row 8 of 10: margin row 9 still free */
	ae_list_move(&l, 1, 0);   CHECK(l.focus == 9 && l.first == 1);  /* scrolls before the edge */
	ae_list_page(&l, 1);      CHECK(l.focus == 19 && l.first >= 10);
	ae_list_move(&l, 100, 0); CHECK(l.focus == 29 && l.first == 20);/* last item may sit on the last row */
	ae_list_move(&l, -9, 0);  CHECK(l.focus == 20 && l.first == 19);/* going up keeps the margin above */
	ae_list_page(&l, -1);     CHECK(l.focus == 10 && l.first == 9 && valid(&l));
	ae_list_page(&l, -1);     ae_list_page(&l, -1);
	CHECK(l.focus == 0 && l.first == 0);                            /* the first item may sit on the first row */
	ae_list_move(&l, 100, 0);
	ae_list_set_count(&l, 12);CHECK(l.focus == 11 && l.first == 2); /* shrink under focus: last item, window fixed */
	ae_list_set_count(&l, 5); CHECK(l.focus == 4 && l.first == 0);  /* shorter than the rows */
	ae_list_set_count(&l, 0); CHECK(l.focus == -1 && l.first == 0);
	ae_list_set_count(&l, 4); CHECK(l.focus == 0 && l.first == 0);  /* items again: the first focused */
	ae_list_init(&l, 30, 10); ae_list_move(&l, 15, 0);
	ae_list_set_count(&l, 40);CHECK(l.focus == 15 && valid(&l));    /* growth keeps the focus on its item */
	ae_list_init(&l, 30, 10); ae_list_scroll(&l, 5);
	CHECK(l.first == 5 && l.focus == 6);                            /* focus pushed inside the window (margin) */
	CHECK(ae_list_item_at_row(&l, 0) == 5 && ae_list_item_at_row(&l, 10) == -1);
	CHECK(ae_list_item_at_row(&l, -1) == -1);
	ae_list_scroll(&l, 100);  CHECK(l.first == 20 && l.focus == 21 && valid(&l));
	ae_list_scroll(&l, -100); CHECK(l.first == 0 && l.focus == 8 && valid(&l));
	ae_list_init(&l, 30, 2);  ae_list_move(&l, 1, 0);               /* two rows: no margin */
	CHECK(l.focus == 1 && l.first == 0);
	ae_list_move(&l, 1, 0);   CHECK(l.focus == 2 && l.first == 1);
	ae_list_init(&l, 5, 0);   CHECK(l.focus == 0 && valid(&l));     /* no rows: as one */
	ae_list_move(&l, 3, 0);   CHECK(l.focus == 3 && l.first == 3);
	ae_list_init(&l, -4, 10); CHECK(l.count == 0 && l.focus == -1);
	/* the focus set directly (focus memory): clamped into the list, the window following */
	ae_list_init(&l, 30, 10);
	ae_list_set_focus(&l, -5);  CHECK(l.focus == 0 && l.first == 0);
	ae_list_set_focus(&l, 30);  CHECK(l.focus == 29 && l.first == 20);
	ae_list_set_focus(&l, 500); CHECK(l.focus == 29 && l.first == 20);
	ae_list_set_focus(&l, 15);  CHECK(l.focus == 15 && valid(&l));
	ae_list_init(&l, 0, 10);
	ae_list_set_focus(&l, 3);   CHECK(l.focus == -1 && l.first == 0);
	/* every row count, count, focus and shrink keeps the invariant */
	for (rows = 1; rows <= 12; rows++)
	for (count = 0; count <= 25; count++)
		for (focus = 0; focus < 25; focus++)
		{
			short shrunk;

			for (shrunk = 0; shrunk <= 25; shrunk++)
			{
				ae_list_init(&l, count, rows);
				ae_list_move(&l, focus, 0);
				if (!valid(&l)) { CHECK(valid(&l)); break; }
				ae_list_set_count(&l, shrunk);
				if (!valid(&l)) { CHECK(valid(&l)); break; }
				ae_list_page(&l, 1);
				ae_list_scroll(&l, -2);
				if (!valid(&l)) { CHECK(valid(&l)); break; }
				ae_list_set_focus(&l, (short)(focus - 3));
				ae_list_scroll(&l, 3);
				ae_list_page(&l, -1);
				if (!valid(&l)) { CHECK(valid(&l)); break; }
			}
		}
	if (failures) return 1;
	printf("ae_list: ok\n");
	return 0;
}
