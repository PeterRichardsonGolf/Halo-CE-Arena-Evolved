/*
AE_FRAME_PACER.H

LOW LATENCY (display.low_latency "off", "on" or "boost"; the rules: ae_frame_pacer_rules.h): in a running game with
V-SYNC and SMOOTH MOTION on, the game sleeps before it reads input so that the frame ends just before the next blank.
BOOST adds one frame in flight, a glFlush before the swap and a spin for the last half millisecond of the sleep. This
file reads the settings, measures the frames, runs the rules and writes the log and the performance line's LATE count;
the platform's side (the swap's two calls, the clock, the sleep): port/linux/src/ae_frame_pacer_gl.h. Desktop OpenGL
only; "off" runs none of it.
*/

#ifndef AE_FRAME_PACER_H
#define AE_FRAME_PACER_H

/* (main.c's main_loop_iteration, before input is read) the sleep, when there is one */
void ae_frame_pacer_wait(void);

/* (main.c's main_framerate_render) "  LATE 3" after the line's other fields at level 3 (FULL), while LOW LATENCY waits:
the frames that missed the blank since the game began; the line unchanged otherwise */
void ae_frame_pacer_append(char *line, long size, long level);

#endif
