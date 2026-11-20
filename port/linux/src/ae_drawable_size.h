/*
AE_DRAWABLE_SIZE.H

(sdl_platform.c's platform_video_drawable_size, AE hooks; 0.9.4 perf audit, owner 2026-11-16 "lets optmize this")
the window's size in pixels asked of SDL once a frame, not on every call: the AE HUD asked it for each of its
thousands of quads a frame (ae_hud_pixels_per_unit), ~8 % of the game's CPU in the box's benchmark profile, SDL
checking the window and taking its locks each time. Asked again after each frame shown, after any window event, and
when the game makes, sizes or frees its window.
*/

#ifndef __AE_DRAWABLE_SIZE_H
#define __AE_DRAWABLE_SIZE_H

#include <SDL3/SDL.h>

/* the window's size may have changed: the next ask reads it from SDL */
void ae_drawable_size_changed(void);

/* the window's size in pixels */
void ae_drawable_size_get(SDL_Window *window, int *width, int *height);

#endif /* __AE_DRAWABLE_SIZE_H */
