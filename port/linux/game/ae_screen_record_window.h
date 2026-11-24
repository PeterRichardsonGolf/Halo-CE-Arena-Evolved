/*
AE_SCREEN_RECORD_WINDOW.H

(rc2) The SERVICE RECORD's detail window, for a player's record and a bot's alike: A on a box of the record pages opens it on the
box's page with that block ringed. A header band (the emblem, the name and its YOU / RIVAL / BOT tag, a live subtitle, GAMES,
WIN RATE, K/D and BEST SPREE on the right, the page chips between the LB and RB marks, the place in the list) over one of four
pages: MODES (a column for each objective kind played, the kinds not played yet in one quiet column; the kinds as chips over
them), COMBAT (ACCURACY, KILL KINDS, DAMAGE, PER GAME), WEAPONS & MAPS, MEDALS (one column a family; A opens every medal). The
words are ae_record_columns_rules.h's.

LB / RB (Tab, a click on a chip) change the page; LT / RT (Page Up / Page Down) step to the previous or next record of the list
the record page came from, the page and the ringed column kept; left / right move the ring between the columns; B goes back.
The window asks its source for the record on every draw, so a step made by the source shows at once.
*/

#ifndef __AE_SCREEN_RECORD_WINDOW_H
#define __AE_SCREEN_RECORD_WINDOW_H

#include "ae_record_view.h"

/* the record the window shows, filled by its source */
struct ae_rw_view
{
	int bot;            /* the BOT tag, "Bot" on LT / RT */
	int own;            /* the viewer's own record: the YOU tag */
	int rival;          /* one of the viewer's rivals: the RIVAL tag */
	char name[96];
	/* the subtitle's first part: "SEEN IN 218 GAMES ON THIS PC" */
	char sub[96];
	/* the right note under the numbers: "2 OF 6" ("" for none) */
	char position[48];
	struct ae_rv_in in;
	/* a player's last game (has 0 for none: a bot) */
	struct ae_sr_last last;
	/* room for a bot's weapons (in.weapons may point here) */
	struct ae_rs_weapon weapons[AE_RS_WEAPONS];
};

struct ae_rw_source
{
	/* the current record into view: 0 when there is none to show */
	int (*fill)(struct ae_rw_view *view);
	/* LT / RT: a step of delta (+1 next, -1 previous); 1 when it moved */
	int (*step)(int delta);
	/* whether a step has somewhere to go (the prompt shows) */
	int (*can_step)(void);
	/* the MEDALS page's door: every medal (NULL for none) */
	void (*open_medals)(int category);
};

/* opens the window over the stack on a page (ae_record_columns_rules.h's AE_RC_*) with a column ringed (its index, -1 for none,
or for MODES the kind's AE_RS_KIND_* through ae_screen_record_window_open_kind). 0 when the stack is full or a window is open */
int ae_screen_record_window_open(short owner, struct ae_rw_source const *source, int page, int column);
/* opens MODES with the column of that kind ringed (the favourite kind's when that kind has no column) */
int ae_screen_record_window_open_kind(short owner, struct ae_rw_source const *source, int kind);
/* rings the MODES column of that kind (AE_RS_KIND_*) in the open window (DEV MODE's record_medals ring); 0 when the window is
not open on MODES */
int ae_screen_record_window_ring_kind(int kind);
int ae_screen_record_window_is_open(void);
/* a column of the window (ae_record_columns_rules.h) drawn in its pane at x, y (design units): CAREER's RECORDS card uses it */
struct frame_metrics;
struct ae_rc_column;
void ae_rw_draw_column(struct frame_metrics const *f, float x, float y, float w, float h, struct ae_rc_column const *c);
void ae_screen_record_window_remove(void);

#endif
