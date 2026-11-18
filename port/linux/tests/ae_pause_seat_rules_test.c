#include <stdio.h>

#include "ae_pause_seat_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	int column, row, columns, rows;

	/* the game's time: stopped once, by the first seat open, only in a game that stops; run again by the last seat shut */
	CHECK(ae_pause_open_stops_time(1, 0));
	CHECK(!ae_pause_open_stops_time(1, 1));
	CHECK(!ae_pause_open_stops_time(1, 3));
	CHECK(!ae_pause_open_stops_time(0, 0));
	CHECK(!ae_pause_close_runs_time(1, 1));
	CHECK(ae_pause_close_runs_time(1, 0));
	CHECK(!ae_pause_close_runs_time(0, 0));

	/* a press stays with the seat of its player; an unbound pad goes to its own number's seat when that is open */
	CHECK(ae_pause_press_seat(2, 0, 0x1) == 2);
	CHECK(ae_pause_press_seat(-1, 1, 0x2) == 1);
	CHECK(ae_pause_press_seat(-1, 1, 0x1) == -1);
	CHECK(ae_pause_press_seat(-1, -1, 0xF) == -1);
	CHECK(ae_pause_press_seat(7, 7, 0xF) == -1);

	/* START of a player with no pause open opens theirs once another seat's is open and the game runs for them */
	CHECK(ae_pause_start_opens(1, 0x1, 1));
	CHECK(!ae_pause_start_opens(0, 0x1, 1));
	CHECK(!ae_pause_start_opens(1, 0x0, 1));
	CHECK(!ae_pause_start_opens(1, 0x1, 0));
	CHECK(!ae_pause_start_opens(4, 0x1, 1));
	CHECK(ae_pause_start_opens(3, 0x7, 1));

	/* the views' slots as compute_window_bounds lays them */
	ae_pause_view_slot(0, 1, 0, &column, &row, &columns, &rows);
	CHECK(column == 0 && row == 0 && columns == 1 && rows == 1);
	ae_pause_view_slot(1, 2, 0, &column, &row, &columns, &rows);
	CHECK(column == 0 && row == 1 && columns == 1 && rows == 2);
	ae_pause_view_slot(0, 3, 1, &column, &row, &columns, &rows);
	CHECK(column == 0 && row == 0 && columns == 2 && rows == 2);
	ae_pause_view_slot(1, 3, 0, &column, &row, &columns, &rows);
	CHECK(column == 0 && row == 1);
	ae_pause_view_slot(2, 3, 0, &column, &row, &columns, &rows);
	CHECK(column == 1 && row == 1);
	ae_pause_view_slot(3, 4, 0, &column, &row, &columns, &rows);
	CHECK(column == 1 && row == 1 && columns == 2 && rows == 2);
	ae_pause_view_slot(1, 4, 0, &column, &row, &columns, &rows);
	CHECK(column == 1 && row == 0);

	if (!failures)
		printf("ae_pause_seat_rules: all passed\n");
	return failures ? 1 : 0;
}
