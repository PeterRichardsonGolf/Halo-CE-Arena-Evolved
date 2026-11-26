/*
AE_THEATER_STRIP.H

The lines over a playing replay (ae_theater_strip.c), drawn with AE's renderer (ae_draw.h) in layout units over the whole frame: a chip at
the top left naming who is watched, the view and the speed; a bar at the bottom with the controls' prompts and the seek bar (the time,
a line with the playhead on it, the length); and in the pause a card with what Start and B do. B hides them. Nothing is drawn while no
replay plays or while it loads.
*/

#ifndef AE_THEATER_STRIP_H
#define AE_THEATER_STRIP_H

/* once a frame, from the frame's drawing (ae_hooks.c ae_ui_render, player 1's view): asks the renderer to draw the lines at this frame's Present */
void ae_theater_strip_draw(void);

#endif
