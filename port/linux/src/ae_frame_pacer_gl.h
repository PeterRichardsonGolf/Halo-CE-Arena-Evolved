/*
AE_FRAME_PACER_GL.H

LOW LATENCY in the platform (the rules: port/linux/game/ae_frame_pacer_rules.h; the settings, the log and the wait
before input: port/linux/game/ae_frame_pacer.c). The swap tells the pacer when a frame's work ends and when the swap
returns; BOOST adds a glFlush before the swap and one frame in flight (d3d8_gl.c's frame_pacing_wait). Desktop OpenGL
only: Android and the browser have empty ones.
*/

#ifndef AE_FRAME_PACER_GL_H
#define AE_FRAME_PACER_GL_H

/* sdl_platform.c's platform_video_swap: the frame is drawn and about to be shown (after ae_dynamic_resolution_swap_begin,
whose GPU time this reads); and the swap has returned (real_vsync: V-SYNC on and the window focused, so that it
waited for a blank) */
void ae_frame_pacer_swap_begin(void);
void ae_frame_pacer_swap_end(int real_vsync);

/* d3d8_gl.c's frame_pacing_wait: the frames in flight to wait for (BOOST: 1 while it works), configured otherwise */
long ae_frame_pacer_frames_in_flight(long configured);

/* (desktop OpenGL's frame_pacing_wait) its fences fitted to this frame's limit: those at or past it deleted (BOOST's
1 after the configured 3), and the next slot kept under it */
struct __GLsync;
void ae_frame_pacer_fences_fit(struct __GLsync **fences, long count, long limit, long *next);

#endif
