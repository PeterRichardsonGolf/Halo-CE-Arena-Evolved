/*
AE_RECORD_PAGES.H

(rc2) The pages of a SERVICE RECORD, a player's (ae_screen_record.c) and a bot's (ae_screen_bot_record.c), on one grid in the
post-game report's 1920 x 1080 design units: OVERVIEW | COMBAT | MODES & MAPS. A page chip trio in the head, right edge 1824,
centre y 150, 34 u tall, round, 12 u apart, the current one an accent edge with a .16 accent fill (the medals page's chips);
Y (pad), Tab (keyboard) or a click on a chip goes on (the pages go round). The panes sit at LEFT 96, TOP 232, 520 | 664 | 480 wide
and 440 tall, 24 apart, a second row at 696; every pane is the record page's (fill 0x10161CF2 and a 4 u accent top).

COMBAT and MODES & MAPS are drawn here from the record's words (ae_record_view.h) for both kinds of record; OVERVIEW's first and
third panes are each screen's own (a player's LAST GAME and HIGHLIGHTS, a bot's THIS GAME and IDENTITY), its CAREER TOTALS the
shared ae_rp_totals. A block whose numbers cover fewer games than the record has says so after its head, muted: "FROM 3 GAMES".
*/

#ifndef __AE_RECORD_PAGES_H
#define __AE_RECORD_PAGES_H

#include "ae_record_view.h"

enum
{
	AE_RP_OVERVIEW,
	AE_RP_COMBAT,
	AE_RP_MODES,
	AE_RP_PAGES
};

enum
{
	AE_RP_LEFT = 96,
	AE_RP_TOP = 232,
	AE_RP_PANE_PAD = 28,
	AE_RP_PANE_H = 440,
	AE_RP_ROW_B_Y = 696,
	AE_RP_ROW_B_H = 264,
	AE_RP_FIRST_W = 520,
	AE_RP_SECOND_X = 648,
	AE_RP_SECOND_W = 664,
	AE_RP_THIRD_X = 1344,
	AE_RP_THIRD_W = 480,
	/* the medals pane's rows */
	AE_RP_MEDAL_LINES = 10
};

struct frame_metrics;

/* a page's chip word and its crumb after SERVICE RECORD (NULL for OVERVIEW) */
char const *ae_rp_page_name(int page);
char const *ae_rp_crumb(int page);
/* the next page, round */
int ae_rp_next(int page);

/* the head's chips; `hit_id` is the screen's hit id (a click is its AE_PART_CHIP hit, index the page); mouse/hover the
pointer's look (hover: the page under it, -1 for none) */
void ae_rp_chips(struct frame_metrics const *f, int current, int mouse, int hover, int hit_id);

/* the building blocks */
void ae_rp_pane(struct frame_metrics const *f, float x, float y, float w, float h);
/* a pane's label with its muted tail ("FROM 3 GAMES") after it; the y after the label (the first row's top) */
float ae_rp_label(struct frame_metrics const *f, float x, float y, char const *label, char const *tail);
/* a label left, its value right (a dash muted); the value fitted to the room */
void ae_rp_cell(struct frame_metrics const *f, float left, float right, float cy, struct ae_rv_cell const *cell);

/* OVERVIEW's CAREER TOTALS pane (its second slot): 8 rows of two columns */
void ae_rp_totals(struct frame_metrics const *f, struct ae_rv_in const *in);
/* the COMBAT page's six panes */
void ae_rp_combat(struct frame_metrics const *f, struct ae_rv_in const *in);

/* the MODES & MAPS page: OBJECTIVES, BY KIND, MEDALS, BY MAP, TOP WEAPONS. Returns the medal kinds shown */
int ae_rp_modes(struct frame_metrics const *f, struct ae_rv_in const *in);

/* the boxes of the page that have a details page (the `has` mask, bit n for ae_record_focus.h's box n): each is a hit (id hit_id,
AE_PART_CARD, index the box); the focused one has the ice-blue ring 6 u out (pad and keys), the one under the mouse the hover wash */
void ae_rp_boxes(struct frame_metrics const *f, int page, unsigned long has, int focus, int mouse, int hover, int hit_id);

#endif
