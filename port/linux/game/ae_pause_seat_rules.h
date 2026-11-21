/*
AE_PAUSE_SEAT_RULES.H

SPLIT PAUSE: the rules of the AE pause's seats (ae_screen_pause.c), pure. A local split screen's game stays stopped for
everyone while any seat's menu is open; the menu itself is the seat's own, drawn in its player's view, and its presses stay
with the seat. An online game (each player pauses alone) is not stopped. No engine includes:
port/linux/tests/ae_pause_seat_rules_test.c compiles these alone.
*/

#ifndef __AE_PAUSE_SEAT_RULES_H
#define __AE_PAUSE_SEAT_RULES_H

#define AE_PAUSE_SEATS 4

/* whether a seat opening stops the game's time: the game is one that stops (a campaign, or an offline split screen game) and
no seat was open before (the time is stopped once, by the first; a second player's pause finds it stopped) */
int ae_pause_open_stops_time(int game_stops, int seats_open_before);

/* whether a seat closing runs the game's time again: this pause stopped it and no seat is left open */
int ae_pause_close_runs_time(int time_stopped_by_pause, int seats_open_after);

/* the seat a press belongs to: the press's player when it is a seat; else, for a pad no player is bound to, the seat whose
number is the pad's when that seat is open; else -1 (not the pause's). open_mask has bit n set for seat n open */
int ae_pause_press_seat(int event_player, int event_controller, unsigned open_mask);

/* whether a press of START by a player with no pause open opens theirs: another seat's game is on screen in a game that
lets every local player pause (any game kind), and the game still runs for the player */
int ae_pause_start_opens(int event_player, unsigned open_mask, int still_on);

/* the view's slot in the screen: its column and row of the 1 x 1, 2 x 1 or 2 x 2 grid compute_window_bounds lays, and how
many columns it spans (three views: the first spans the top, the others sit in the bottom row's slots 2 and 3) */
void ae_pause_view_slot(int view, int views, int spans, int *column, int *row, int *columns, int *rows);

#endif
