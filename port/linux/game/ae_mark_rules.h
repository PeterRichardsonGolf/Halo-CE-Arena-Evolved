/*
AE_MARK_RULES.H

MARKS (ae_marks.c): the press, the mark's life and its place on the screen, as
pure rules (port/linux/tests/ae_mark_rules_test.c runs them).
*/

#ifndef __AE_MARK_RULES_H
#define __AE_MARK_RULES_H

enum
{
	AE_MARK_KIND_ENEMY,
	AE_MARK_KIND_ITEM,
	AE_MARK_KIND_PLACE,
	AE_MARK_KIND_COUNT
};

/* all in game ticks, 30 a second */
#define AE_MARK_HOLD_TICKS 8	/* a press this long on the shared button is the flashlight's */
#define AE_MARK_APPEAR_TICKS 4
#define AE_MARK_FADE_AT_TICKS 132
#define AE_MARK_LIFE_TICKS 150

struct ae_mark_press
{
	int down;
	int taken;
	int held;
	long since;
};

/* a tick of the button the mark shares with the flashlight: down, whether it is down; taken, that a press starting
now belongs to something else (the zoom's steps). *light says whether the flashlight sees the button this tick.
Returns 1 on the tick a tap ends: mark. */
int ae_mark_press_step(struct ae_mark_press *press, int down, int taken, long now, int *light);

/* how opaque a mark of this age is, 0 to 1: 0 when it is over */
float ae_mark_alpha(long age);
/* its size against its resting size: it settles from a little larger as it appears */
float ae_mark_appear_scale(long age);

/* x and y from the view's middle: when off_screen, or outside the ellipse of these radii, moves them along their
direction onto it and returns 1. Marks stay in view for their short life. */
int ae_mark_clamp(float *x, float *y, float radius_x, float radius_y, int off_screen);

/* whether a new press means the marker's own mark: same object, or a place this near (squared world units) */
int ae_mark_same(int kind_a, long object_a, int kind_b, long object_b, float distance_squared);

/* the spam guard: marks inside the window; the warning's mark is let through, the limit's and those after are not */
#define AE_MARK_SPAM_LIMIT 15
#define AE_MARK_SPAM_WARN_AT 13
#define AE_MARK_SPAM_WINDOW_TICKS 600
#define AE_MARK_SPAM_BLOCK_TICKS 900

enum { AE_MARK_SPAM_OK, AE_MARK_SPAM_WARN, AE_MARK_SPAM_NOW_BLOCKED, AE_MARK_SPAM_STILL_BLOCKED };

struct ae_mark_spam
{
	long times[AE_MARK_SPAM_LIMIT];
	int next;
	int used;
	long blocked_until;
	int blocked;
};

/* a new mark at this tick: what the guard says (OK and WARN: it may go) */
int ae_mark_spam_take(struct ae_mark_spam *spam, long now);

#endif
