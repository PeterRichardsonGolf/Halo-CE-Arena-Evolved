/*
AE_ZOOM.H

The zoom action's hold mode of Arena Evolved, as a pure state machine (no engine
or config calls: port/linux/tests/ae_zoom_test.c runs it). input.zoom_mode picks
how the zoom action works, for the mouse and the gamepad alike:

  TOGGLE  each press steps the zoom level, as Halo does (the engine's own code
          runs; ae_zoom_step is its model here, never called in this mode)
  HOLD    the press zooms to the first level, the release returns to unzoomed;
          no second level
  BOTH    a tap (released within the hold time) steps the level as TOGGLE does;
          a hold zooms to the first level when it began unzoomed, keeps the
          level when it began zoomed, and the release returns to unzoomed

Anything that forces the zoom off while the action is down (a weapon switch, a
reload, a melee, a hit, a vehicle, a death) ends the press: its release does
nothing, and the next press is a fresh one. A press that cannot act when it is
made (the weapon reloading, no camera control) is also left alone until its
release.

The zoom level is the engine's: -1 unzoomed, 0 the first level, up to count - 1
(count is the weapon's zoom level count).
*/

#ifndef __AE_ZOOM_H
#define __AE_ZOOM_H

enum
{
	AE_ZOOM_TOGGLE = 0,
	AE_ZOOM_HOLD = 1,
	AE_ZOOM_BOTH = 2
};

#define AE_ZOOM_HOLD_TIME_DEFAULT 0.25
#define AE_ZOOM_HOLD_TIME_MINIMUM 0.1
#define AE_ZOOM_HOLD_TIME_MAXIMUM 1.0

struct ae_zoom_state
{
	int phase;
	int was_held;
	long press_ms;
	short start_level;
	short expected;
};

/* "toggle", "hold" or "both" (any case); anything else toggle */
int ae_zoom_mode_from_text(char const *text);
/* the hold time in seconds (0.1 to 1, 0.25 when not a number) as milliseconds */
long ae_zoom_hold_ms(double seconds);

void ae_zoom_reset(struct ae_zoom_state *state);
/* Halo's rotation of the level by one press: unzoomed to the first, each level to the next, the last out */
short ae_zoom_rotate(short level, short count);
/* One look at the button: held (whether it is down), the time in milliseconds, the level the game holds now and the
weapon's zoom level count; blocked when a press cannot act now. Returns the level the game should hold. A tap is a
press released after at most hold_ms milliseconds. */
short ae_zoom_step(struct ae_zoom_state *state, int mode, long hold_ms, int held, long now_ms, short level,
	short count, int blocked);

#endif
