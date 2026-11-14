/*
AE_DYNAMIC_RESOLUTION.H

TARGET 60 FPS: which setting holds for the views on screen (display.target_60_fps with one player, where
display.resolution_scaling is "native"; display.split_target_60_fps with two to four), the rules
(ae_dynamic_resolution_rules.h) run each frame, the log's line when the scale changes, and the performance line's
RES field. debug.dynamic_resolution_force (50 to 100) holds the scale at that percent while a game is drawn, whatever
the settings (tests). The renderer's side: port/linux/src/ae_dynamic_resolution_gl.h.
*/

#ifndef AE_DYNAMIC_RESOLUTION_H
#define AE_DYNAMIC_RESOLUTION_H

/* (ae_dynamic_resolution_gl.c, at each swap) the percent for the next frame, from this one's time (ms; below 0: not
known) */
int ae_dynamic_resolution_policy(float frame_ms);

/* the percent the performance line shows, -1 while TARGET 60 FPS is not on for the views on screen */
int ae_dynamic_resolution_shown_percent(void);

/* (main.c's main_framerate_render) "  RES 75%" after the line's FRAME field (MINIMAL and FULL: level 2 and 3), while
TARGET 60 FPS is on; the line unchanged otherwise */
void ae_dynamic_resolution_append(char *line, long size, long level);

#endif
