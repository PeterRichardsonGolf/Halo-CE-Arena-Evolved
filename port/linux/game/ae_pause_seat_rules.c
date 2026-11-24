/*
AE_PAUSE_SEAT_RULES.C

SPLIT PAUSE's seat rules (ae_pause_seat_rules.h). No engine includes.
*/

#include "ae_pause_seat_rules.h"

int ae_pause_open_stops_time(int game_stops, int seats_open_before)
{
	return game_stops && seats_open_before <= 0;
}

int ae_pause_close_runs_time(int time_stopped_by_pause, int seats_open_after)
{
	return time_stopped_by_pause && seats_open_after <= 0;
}

int ae_pause_press_seat(int event_player, int event_controller, unsigned open_mask)
{
	if (event_player >= 0 && event_player < AE_PAUSE_SEATS)
		return event_player;
	if (event_controller >= 0 && event_controller < AE_PAUSE_SEATS && (open_mask & (1u << event_controller)))
		return event_controller;
	return -1;
}

int ae_pause_start_opens(int event_player, unsigned open_mask, int still_on)
{
	if (event_player < 0 || event_player >= AE_PAUSE_SEATS || !open_mask || !still_on)
		return 0;
	return !(open_mask & (1u << event_player));
}

void ae_pause_view_slot(int view, int views, int spans, int *column, int *row, int *columns, int *rows)
{
	int slot = view;

	*columns = views <= 2 ? 1 : 2;
	*rows = views <= 1 ? 1 : 2;
	if (views == 3 && view > 0)
		slot = view + 1;
	*column = slot % *columns;
	*row = slot / *columns;
	if (spans)
		*column = 0;
}
