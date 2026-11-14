/*
AE_SWAP_INTERVAL.H

A frame limit in step with the display under gamescope (the rules: port/linux/game/ae_swap_interval_rules.h), from
ChupathingyCE #983 by Milenko. sdl_platform.c's platform_swap_interval and frame_interval_ns call these; nothing
changes anywhere but under gamescope with V-SYNC off and display.max_fps at the display's refresh rate or a whole
fraction of it.
*/

#ifndef AE_SWAP_INTERVAL_H
#define AE_SWAP_INTERVAL_H

/* the swap interval this frame's window should have: 0 unfocused; 1 with V-SYNC; with V-SYNC off, under gamescope, the
in step interval of display.max_fps (above 0), else 0. The refresh rate is read from the window's display once a
second (gamescope tells it only after the window is shown) */
int ae_swap_interval_wanted(int vsync, int focused);

/* the driver refused interval (SDL_GL_SetSwapInterval failed): 1 when it was an in step one (logged once, and not
asked for again until what is wanted changes: the clock paces the frames as before), else 0 */
int ae_swap_interval_refused(int interval);

/* whether the display paces the frames now (an in step interval is given): the frame limit's clock must not */
int ae_swap_interval_in_use(void);

#endif
