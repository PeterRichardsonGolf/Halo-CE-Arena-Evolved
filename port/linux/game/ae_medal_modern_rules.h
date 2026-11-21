/*
AE_MEDAL_MODERN_RULES.H

The MODERN medal stack as pure rules (port/linux/tests/ae_medal_modern_rules_test.c runs them): the medals a player earns as rows of a
stack by their reticle, with a "Killed" row for each of their kills and their points added above. Right aligned: each row's name ends
at a line left of the reticle's centre and its picture (where there is one) stands right of it. The numbers are a stage of 1080 lines,
scaled by the view's height, with the text sizes held at floors.

A row lives three seconds (two for the oldest of a full stack), then fades. The stack is at most AE_MM_MOST rows and nothing is drawn
below its limit: a row that would pass it takes the oldest rows' places first. Rows grow down from a fixed top, the newest at the
bottom, and close up when one goes. A new row's name pops in (1.3 times its size, settling in 100 ms) and the points pop when they
change (200 ms). Time is the game's ticks (30 a second).
*/

#ifndef __AE_MEDAL_MODERN_RULES_H
#define __AE_MEDAL_MODERN_RULES_H

#include <wchar.h>

#include "ae_pure_types.h"

/* ---------- the stage's numbers (1080 lines; x from the reticle's centre, y from the view's top) */

#define AE_MM_NAME_PX 19.0f
#define AE_MM_NAME_FLOOR_PX 16.0f
/* two views, one above the other, hold their text at this */
#define AE_MM_SPLIT_NAME_PX 18.0f
#define AE_MM_SCORE_PX 28.0f
#define AE_MM_SCORE_FLOOR_PX 20.0f
#define AE_MM_NAME_RIGHT 240.0f
#define AE_MM_PICTURE_GAP 7.0f
#define AE_MM_PICTURE 56.0f
#define AE_MM_PICTURE_FLOOR 20.0f
#define AE_MM_ROW_PICTURE 58.0f
#define AE_MM_ROW_TEXT 40.0f
#define AE_MM_ROW_FLOOR 26.0f
#define AE_MM_SCORE_MIDDLE 468.0f
#define AE_MM_ROWS_GAP 4.0f
#define AE_MM_LIMIT 667.0f
/* the reticle's zone, a circle round the view's centre that nothing of the stack comes into */
#define AE_MM_ZONE 66.0f
/* the space a stack keeps clear of what else is drawn */
#define AE_MM_CLEAR 8.0f
/* the widest a name's row is taken to be when the stack is checked against other lines, in text sizes */
#define AE_MM_WIDEST_EM 13.0f

/* ---------- time (game ticks) */

#define AE_MM_TICKS_PER_SECOND 30
#define AE_MM_MOST 5
#define AE_MM_ROWS 10
#define AE_MM_LIFE (3 * AE_MM_TICKS_PER_SECOND)
#define AE_MM_FULL_LIFE (2 * AE_MM_TICKS_PER_SECOND)
#define AE_MM_FADE_TICKS 15
#define AE_MM_GONE_TICKS 3
#define AE_MM_CLOSE_TICKS 4
#define AE_MM_NAME_POP_TICKS 3
#define AE_MM_SCORE_POP_TICKS 6
#define AE_MM_POP 1.3f

/* ---------- metrics */

struct ae_mm_metrics
{
	real name_px, score_px;
	real picture;			/* a picture's size, square */
	real row_picture, row_text;
	real name_right;		/* x from the view's left, where a name's right edge is */
	real picture_left, picture_right;
	real score_middle;		/* y from the view's top: the points' middle */
	real rows_top;			/* the first row's top */
	real limit;				/* nothing below this */
	real zone;				/* the reticle's zone's radius */
	real centre_x;
};

/* the view's numbers: split is 2 or more views on the screen (their text held at the split floors) */
void ae_mm_metrics_of(real view_width, real view_height, boolean split, struct ae_mm_metrics *metrics);

/* ---------- rows */

enum
{
	AE_MM_KIND_MEDAL,
	AE_MM_KIND_KILLED
};

#define AE_MM_NAME 13

struct ae_mm_row
{
	boolean used;
	int kind;
	int medal;
	boolean has_picture;
	int points;
	long born;
	long life_end;
	long gone_at;			/* -1 while it was not pushed out */
	real y_from;			/* from the rows' top: where the row is moving from and to, and since when */
	real y_to;
	long moved_at;
	/* a KILLED row: who was killed (for the colour of their name), and whether it was a team mate */
	long victim;
	short victim_team;
	boolean betrayal;
	wchar_t victim_name[AE_MM_NAME];
};

struct ae_mm_stack
{
	struct ae_mm_row rows[AE_MM_ROWS];
	long last;
	int score;				/* the points shown, as they stood at the last call */
	long score_at;			/* the tick they last changed */
};

void ae_mm_reset(struct ae_mm_stack *stack);
/* a row's height at these metrics */
real ae_mm_row_height(struct ae_mm_metrics const *metrics, struct ae_mm_row const *row);
/* the row joins at the bottom. now before the last call is a new game (the stack empties first). The oldest rows go (in
AE_MM_GONE_TICKS) until the row would not pass the limit and the stack would hold AE_MM_MOST; a full stack's oldest then has
AE_MM_FULL_LIFE to live. Returns how many were pushed out */
int ae_mm_add(struct ae_mm_stack *stack, struct ae_mm_metrics const *metrics, struct ae_mm_row const *row, long now);
void ae_mm_step(struct ae_mm_stack *stack, struct ae_mm_metrics const *metrics, long now);
int ae_mm_count(struct ae_mm_stack const *stack);
/* the rows holding a place now */
int ae_mm_live(struct ae_mm_stack const *stack, long now);
/* a row's top from the rows' top, its alpha and its name's size now (1 to AE_MM_POP); FALSE past its time */
boolean ae_mm_row_pose(struct ae_mm_row const *row, long now, real *y, real *alpha, real *name_scale);
/* the points of the medal rows holding a place now, and whether they show: their alpha (that of the clearest medal row) and their
size (1 to AE_MM_POP) */
int ae_mm_score(struct ae_mm_stack const *stack, long now);
boolean ae_mm_score_pose(struct ae_mm_stack const *stack, long now, real *alpha, real *scale);

/* ---------- what it keeps clear of */

struct ae_mm_box
{
	real x0, y0, x1, y1;
};

/* the space between two boxes: the distance across when they are apart, a negative number (the overlap's depth) when they meet */
real ae_mm_gap(struct ae_mm_box const *a, struct ae_mm_box const *b);
/* the box the stack can come to fill, its names widest_em text sizes wide: from the points' top to the limit, from the widest name to
the pictures' right edge */
void ae_mm_box_of(struct ae_mm_metrics const *metrics, real widest_em, struct ae_mm_box *box);
/* the gap between that box (or the picture column of it) and the reticle's zone: a circle round the view's centre, as its box */
real ae_mm_zone_gap(struct ae_mm_metrics const *metrics);
/* whether the stack's box is at least AE_MM_CLEAR from every one of these (the boxes of other lines drawn in the view) */
boolean ae_mm_clears(struct ae_mm_box const *stack, struct ae_mm_box const *others, int count, real clear);

/* ---------- a medal as a word line (three or four views, or where the stack does not clear) */

/* the medal's name after a run of its colour (rgb, 0 to 1: ae_marker_rules.h's AE_RUN_RGB), for a line of the player's message list.
Returns the length written, 0 when size has no room */
int ae_mm_word_line(wchar_t *out, int size, wchar_t const *name, float const rgb[3]);

#endif /* __AE_MEDAL_MODERN_RULES_H */
