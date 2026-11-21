/*
AE_STEAM_PAD_RULES.H

The Steam Controller (2026) on PC, the arithmetic (ae_steam_pad.c reads SDL): which pad it is, and what its right
trackpad does to the view. No SDL here (port/linux/tests/ae_steam_pad_rules_test.c).

The trackpad aims like a mouse that is lifted and put down: a finger that moves turns the view by how far it moved,
a finger that lands and rests turns nothing (a thumb resting on the pad is not aiming), and a finger that leaves the pad
at speed leaves the view gliding on, slowing to a stop. The pad's position is SDL's: x and y from 0 to 1, x to the right
and y down, across the pad's whole width and height. A turn is in radians, yaw positive to the left and pitch positive
upward, as ae_gyro_rules.h has them (so a finger moving right turns the view right and a finger moving up looks up).
*/

#ifndef AE_STEAM_PAD_RULES_H
#define AE_STEAM_PAD_RULES_H

/* what kind of pad SDL's vendor and product ids say */
enum
{
	AE_STEAM_PAD_NONE = 0,
	AE_STEAM_PAD_CONTROLLER = 1,   /* the Steam Controller (2026) */
	AE_STEAM_PAD_OTHER_VALVE = 2   /* another Valve pad (a Steam Deck, Steam's virtual pad) */
};

#define AE_STEAM_PAD_VENDOR 0x28de
/* the 2026 Steam Controller's USB product ids (wired, its wireless receiver, the second receiver); the 2015 Steam
Controller (0x1102 wired, 0x1142 wireless) has the same SDL name and is not it */
#define AE_STEAM_PAD_PRODUCT_WIRED 0x1302
#define AE_STEAM_PAD_PRODUCT_RECEIVER 0x1304
#define AE_STEAM_PAD_PRODUCT_RECEIVER_2 0x1305

/* vendor, product: the USB ids (0: not known) */
int ae_steam_pad_rules_kind(unsigned int vendor, unsigned int product);

/* a finger that has not moved this far, in pad widths, from where it landed is resting (about a millimetre and a half) */
#define AE_TRACKPAD_RULES_DEAD_ZONE 0.04f
/* radians the view turns for a finger crossing the whole pad, at sensitivity 1 */
#define AE_TRACKPAD_RULES_TURN 2.4f
/* a finger leaving the pad faster than this, pad widths a second, glides on; one that rested a moment before lifting
(longer than AE_TRACKPAD_RULES_REST seconds without moving) does not */
#define AE_TRACKPAD_RULES_FLICK 3.5f
#define AE_TRACKPAD_RULES_REST 0.06
/* the glide's speed falls by e each AE_TRACKPAD_RULES_GLIDE seconds, and ends under AE_TRACKPAD_RULES_GLIDE_END */
#define AE_TRACKPAD_RULES_GLIDE 0.12
#define AE_TRACKPAD_RULES_GLIDE_END 0.15f
/* the finger's travel is turned over a short time, not all in its read: a slow finger (under AE_TRACKPAD_RULES_SLOW pad
widths a second) over AE_TRACKPAD_RULES_SMOOTH_SLOW seconds, a fast one (over AE_TRACKPAD_RULES_FAST) over
AE_TRACKPAD_RULES_SMOOTH_FAST, in between in proportion: a still finger's sensor noise and reads that fall unevenly
between frames no longer jerk the view, and every bit of the travel is still turned */
#define AE_TRACKPAD_RULES_SLOW 0.25f
#define AE_TRACKPAD_RULES_FAST 1.0f
#define AE_TRACKPAD_RULES_SMOOTH_SLOW 0.05
#define AE_TRACKPAD_RULES_SMOOTH_FAST 0.012
/* a finger that moves further than this, pad widths, between two reads has lifted and landed again */
#define AE_TRACKPAD_RULES_JUMP 0.25f
/* the longest step a read covers, seconds: a read after a pause (a menu, a loading screen) starts afresh */
#define AE_TRACKPAD_RULES_LONGEST_STEP 0.1

struct ae_trackpad_rules
{
	int down;            /* the finger was on the pad at the last read */
	int moving;          /* it has left its landing's dead zone */
	float anchor_x, anchor_y;   /* where it landed */
	float last_x, last_y;       /* where it was at the last read */
	float speed_x, speed_y;     /* its smoothed speed, pad widths a second */
	double still;        /* seconds since it last moved */
	float glide_x, glide_y;     /* the glide's speed, pad widths a second */
	float pending_x, pending_y; /* the finger's travel not turned yet (the smoothing's), pad widths */
};

/* down: a finger is on the pad now; x, y: where (ignored when not down); seconds: since the last read (0 or less: none).
sensitivity: a multiplier of the turn (1: AE_TRACKPAD_RULES_TURN a pad width). yaw, pitch: the turn, radians */
void ae_trackpad_rules_step(struct ae_trackpad_rules *state, int down, float x, float y, double seconds,
	float sensitivity, float *yaw, float *pitch);

#endif
