/*
AE_WHEEL_RULES.H

The CALLOUT WHEEL (ae_wheel.c): the button's press, which entry the stick or mouse points at, and which entries are
shown, as pure rules (port/linux/tests/ae_wheel_rules_test.c runs them). The button is the mark's (ae_mark_rules.h): a
tap marks, a double tap and a hold are the wheel's.
*/

#ifndef __AE_WHEEL_RULES_H
#define __AE_WHEEL_RULES_H

/* clockwise from up, 45 degrees each, the first one's middle straight up */
enum
{
	AE_WHEEL_SMART,
	AE_WHEEL_ATTACK,
	AE_WHEEL_ENEMY,
	AE_WHEEL_WEAPON,
	AE_WHEEL_DEFEND,
	AE_WHEEL_RIDE,
	AE_WHEEL_GOING,
	AE_WHEEL_WATCH,
	AE_WHEEL_COUNT,
	AE_WHEEL_NONE = -1
};

/* what a tick of the button gives */
enum
{
	AE_PRESS_NOTHING,
	AE_PRESS_TAP,		/* a short press ended: at once, never held back to wait for a double */
	AE_PRESS_DOUBLE,	/* a short press ended inside AE_WHEEL_DOUBLE_TICKS of the last tap's end */
	AE_PRESS_OPEN,		/* held for AE_MARK_HOLD_TICKS: the wheel opens */
	AE_PRESS_SEND,		/* released with an entry focused: send it */
	AE_PRESS_CANCEL		/* released with none focused, or interrupted while open */
};

/* all in game ticks, 30 a second */
#define AE_WHEEL_DOUBLE_TICKS 12	/* a press starting this soon after a tap's end makes it a double */
#define AE_WHEEL_GRACE_TICKS 6		/* the stick recentred keeps its focus this long */

/* the stick: travel (of 1) before an entry is focused, and the degrees the current entry keeps past its borders */
#define AE_WHEEL_STICK_TRAVEL 0.35f
#define AE_WHEEL_STICKY_DEGREES 4.0f

/* the mouse: pixels of travel the wheel's pointer is held within, and the distance out from which it focuses */
#define AE_WHEEL_MOUSE_RADIUS 120.0f
#define AE_WHEEL_MOUSE_FOCUS 40.0f

/* the legend of hints under the wheel goes after this many calls sent */
#define AE_WHEEL_LEGEND_CALLS 5

/* all zero is idle */
struct ae_wheel_press
{
	int down;
	int open;
	int ignore;		/* an interrupted press: nothing of it counts, to its release */
	int second;		/* this press began in time to make a double */
	int pending;		/* a tap ended and a double may follow */
	long since;
	long released;
};

/* a tick of the button: down, whether it is down; interrupted, that the wheel cannot be (dead, a menu, a scope): ends
the press, so its release gives nothing. focus is the entry focused now, or AE_WHEEL_NONE; the release that returns
SEND is for that focus. */
int ae_wheel_press_step(struct ae_wheel_press *press, int down, int interrupted, long now, int focus);

/* the entry the stick points at (x and y of -1 to 1, y up) or AE_WHEEL_NONE. current is the entry focused now, or none:
it keeps the focus a few degrees past its borders. */
int ae_wheel_pick(int current, float x, float y);

/* the mouse's travel since the wheel opened, in pixels, held within AE_WHEEL_MOUSE_RADIUS */
void ae_wheel_mouse_add(float *x, float *y, float dx, float dy);
/* as ae_wheel_pick, for that travel: an entry from AE_WHEEL_MOUSE_FOCUS pixels out */
int ae_wheel_pick_mouse(int current, float x, float y);

/* all zero is idle */
struct ae_wheel_focus
{
	int held;
	int entry;
	int lost;
	long lost_at;
};

/* the focus to use this tick, from what was picked. Only a stick has the grace: recentred, it keeps the last focus
for AE_WHEEL_GRACE_TICKS. */
int ae_wheel_focus_held(struct ae_wheel_focus *state, int picked, int from_stick, long now);

/* whether the wheel has this entry: dead, only SMART (it reads HELP ME), ENEMY and GOING */
int ae_wheel_entry_shown(int entry, int dead);

/* whether the legend of hints shows: not after AE_WHEEL_LEGEND_CALLS calls sent, in split screen, or with hints off */
int ae_wheel_legend_shown(int calls_sent, int split, int hints_on);
/* the calls sent kept in a profile: the count after one more (to AE_WHEEL_LEGEND_CALLS, no more), and a stored count's
text read within 0 and that limit (0 for text that is no number) */
int ae_wheel_calls_after(int calls_sent);
int ae_wheel_calls_from_text(char const *text);

#endif
