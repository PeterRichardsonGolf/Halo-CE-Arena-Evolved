/*
AE_RECORD_FOCUS.H

(rc2) The BOXES of the SERVICE RECORD pages that have a DETAILS page, and the focus between them as pure rules
(port/linux/tests/ae_record_focus_test.c runs them). A page's boxes sit on the grid of ae_record_pages.h; the focus moves to the
nearest box in the direction pressed (up, down, left, right) and stops at the edge, never wrapping. A box with nothing more to show
is no focus stop: the caller's `has` mask names the boxes that have a details page now. LB / RB (and Tab or Y) change the page,
and the focus stays on the same box when the new page has it, else it is on the page's first box.
*/

#ifndef __AE_RECORD_FOCUS_H
#define __AE_RECORD_FOCUS_H

enum ae_rf_box
{
	AE_RF_NONE = -1,
	AE_RF_LAST,         /* OVERVIEW (a player): LAST GAME */
	AE_RF_VERSUS,       /* OVERVIEW (a bot): VS YOU */
	AE_RF_ACCURACY,     /* COMBAT */
	AE_RF_KILL_KINDS,
	AE_RF_ASSISTS,
	AE_RF_DAMAGE,
	AE_RF_PER_GAME,
	AE_RF_OBJECTIVES,   /* MODES & MAPS */
	AE_RF_KINDS,
	AE_RF_MEDALS,
	AE_RF_MAPS,
	AE_RF_WEAPONS,
	/* (rc2) OVERVIEW's other panes: CAREER TOTALS (both records), a player's HIGHLIGHTS, a bot's IDENTITY (the same place) */
	AE_RF_TOTALS,
	AE_RF_HIGHLIGHTS,
	AE_RF_IDENTITY,
	/* a player's own YOUR RIVALS strip: each rival row a stop (up / down inside it), A that rival's record */
	AE_RF_RIVALS,
	AE_RF_BOXES
};

enum { AE_RF_UP, AE_RF_DOWN, AE_RF_LEFT, AE_RF_RIGHT };

struct ae_rf_rect
{
	float x, y, w, h;
};

/* the box's pane in design units (the same as the pane drawn) */
struct ae_rf_rect ae_rf_rect_of(int box);
/* the box's name for the log and the page's crumb: "LAST GAME", "TOP WEAPONS" */
char const *ae_rf_name(int box);
/* the page (ae_record_pages.h's AE_RP_) a box is on, -1 for none */
int ae_rf_page_of(int box);
/* the boxes of a page that are in `has` (bit n for box n), top to bottom and left to right: their ids into out (room for
AE_RF_BOXES); the count */
int ae_rf_list(int page, unsigned long has, int *out);
/* the box the focus goes to from `current` (AE_RF_NONE: none yet) in a direction, among the boxes of the page that are in
`has`; `current` itself when none lies that way, the page's first when current is not one of them, AE_RF_NONE with no box */
int ae_rf_move(int page, unsigned long has, int current, int direction);
/* the box the focus is on after the page changed (or the bot): the same box when the page has it, else the first, else none */
int ae_rf_keep(int page, unsigned long has, int current);

#endif
