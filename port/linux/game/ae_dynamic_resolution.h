/*
AE_DYNAMIC_RESOLUTION.H

TARGET FRAMERATE: which setting holds for the views on screen (display.target_framerate with one player, where
display.resolution_scaling is "native"; display.split_target_framerate with two to four) and the rate it holds there
(ae_target_framerate_rules.h: no more than the refresh with V-SYNC, FRAME RATE LIMIT's cap without), the rules
(ae_dynamic_resolution_rules.h) run each frame, the log's line when the scale changes, and the performance line's
RES field. debug.dynamic_resolution_force (50 to 100) holds the scale at that percent while a game is drawn, whatever
the settings (tests). The renderer's side: port/linux/src/ae_dynamic_resolution_gl.h.
*/

#ifndef AE_DYNAMIC_RESOLUTION_H
#define AE_DYNAMIC_RESOLUTION_H

/* (ae_dynamic_resolution_gl.c, at each swap) the percent for the next frame, from this one's time (ms; below 0: not
known) */
int ae_dynamic_resolution_policy(float frame_ms, float shown_ms);

/* the percent the performance line shows, -1 while TARGET FRAMERATE is not on for the views on screen */
int ae_dynamic_resolution_shown_percent(void);

/* (0.9.1) whether the scale is in play now (on for these views, or held by debug.dynamic_resolution_force): the GPU is
timed only then, or for LOW LATENCY (ae_frame_pacer_armed); a GPU timer each frame costs on some drivers */
int ae_dynamic_resolution_timing_wanted(void);

/* (main.c's main_framerate_render) "  RES 75%" after the line's FRAME field (MINIMAL and FULL: level 2 and 3), while
TARGET FRAMERATE is on; the line unchanged otherwise */
void ae_dynamic_resolution_append(char *line, long size, long level);

#endif
