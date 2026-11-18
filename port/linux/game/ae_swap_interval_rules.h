/*
AE_SWAP_INTERVAL_RULES.H

Under gamescope (the Steam Deck's Game Mode), with V-SYNC off, a display.max_fps at the display's refresh rate or a
whole fraction of it is given to the window as a swap interval, so that the display paces the frames: frames spaced by
the clock at the display's own rate drift against its refreshes, and now and then one shows twice and the next not at
all. Pure rules (port/linux/tests/ae_swap_interval_rules_test.c); ae_swap_interval.c asks the window.

From ChupathingyCE #983 by Milenko (based on work by goatbutt and the OpenH1 team): the rule and its limits. The
interval above 1 is only for SDL's x11 driver (gamescope's XWayland): the Wayland driver takes any interval above 1
as 1, so there the clock keeps the limit.
*/

#ifndef AE_SWAP_INTERVAL_RULES_H
#define AE_SWAP_INTERVAL_RULES_H

/* the most refreshes a frame is shown for with the x11 driver */
#define AE_SWAP_INTERVAL_MOST 4

/* the swap interval for a limit of maximum frames a second (display.max_fps; 0 or below: none) on a display of refresh
Hz, 0 when none fits (the limit must be within a frame a second of refresh / interval) */
int ae_swap_interval_in_step(float refresh, long maximum, int x11);

#endif
