/*
AE_SEAT_PANEL_RULES.H

THE SEAT PANEL (ae_seat_panel.c), as pure rules (port/linux/tests/ae_seat_panel_rules_test.c runs them): a name cut to a seat's
bar, the health pips, and where each seat sits. All in the view's pixels.
*/

#ifndef __AE_SEAT_PANEL_RULES_H
#define __AE_SEAT_PANEL_RULES_H

#include <wchar.h>

#define AE_SEAT_PIPS 8
#define AE_SEAT_PIPS_RED 2		/* this many lit, or fewer: red */
#define AE_SEAT_BAR_WIDTH 164.0f	/* not scaled, nor is the pitch: their text is 16 px in every view */
#define AE_SEAT_PITCH 180.0f
#define AE_SEAT_WRAP_MESSAGE_WIDTH 225.0f	/* x scale: how wide the driver's text lines may run under the panel */

/* "ROLE name", the role whole and the name cut to fit bar_width, ending in "...". name may be empty: the role alone */
void ae_seat_name_clip(wchar_t const *role, wchar_t const *name, float bar_width, float (*measure)(wchar_t const *),
	wchar_t *out, int size);

/* how many of the pips are lit at this vitality (0 to 1, more is 1): none only at zero */
int ae_seat_pips_lit(float vitality);
int ae_seat_pips_red(int lit);

/* the seats' top: the ammo panel's, a little higher for two rows in a quarter view */
float ae_seat_panel_top(float origin_y, int seats, float scale);
/* how many seats go in a row: two in a quarter view, else what fits from left to the view's edge */
int ae_seat_columns(int seats, float scale, float left, float view_width);
/* a seat's top left */
void ae_seat_cell(int index, int columns, float left, float top, float row_height, float *x, float *y);

/* with more seats than this, the open seats of one kind roll into one line */
#define AE_SEAT_ROLL_ABOVE 3
#define AE_SEAT_ROLL_MOST 8

struct ae_seat_kind
{
	int gunner;		/* 1 a gunner's seat, 0 a passenger's */
	int open;		/* 1 nobody in it */
};
struct ae_seat_line
{
	int seat;		/* the seat this line draws: itself, or the first of the open ones it holds */
	int open;		/* 0: the seat is taken; else how many open seats of its kind this line stands for */
};

/* the lines of the panel for these seats (at most AE_SEAT_ROLL_MOST, as many as the seats): with up to AE_SEAT_ROLL_ABOVE seats each seat
is a line (open 1 if it is open); with more, the open seats of one kind (gunner, passenger) are one line, where the first of them is, and
a taken seat stays its own line. Returns the lines' count; 0 for no seats */
int ae_seat_roll(int count, struct ae_seat_kind const *seats, struct ae_seat_line *lines);
/* an open line's words after the role: OPEN, or OPEN x6 for 6 (up to 99) */
void ae_seat_open_text(int open, wchar_t *out, int size);

#endif
