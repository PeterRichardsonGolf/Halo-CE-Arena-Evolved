/*
CAPTURE.H

Screenshots and video recording (capture.c): F9 writes the whole frame as a
PNG to screenshots/ beside config.toml; F10 starts and stops a recording,
the frames and the game's sound piped to ffmpeg (not bundled: on the PATH,
beside the game, or capture.ffmpeg_path) into recordings/<time>.mp4. The
desktop builds only; the dedicated server and Android have the calls, which
do nothing.
*/

#ifndef __HALO_LINUX_CAPTURE_H
#define __HALO_LINUX_CAPTURE_H

/* the keys (sdl_platform.c): a screenshot at the next frame; recording on
or off at the next frame */
void capture_request_screenshot(void);
void capture_request_recording_toggle(void);

/* D3DDevice_Present (d3d8_gl.c), once a frame: the picture is the back
buffer's framebuffer, width by height, its row 0 the top; before the
red dot (capture_present_overlay) is drawn */
void capture_frame(unsigned int framebuffer, int width, int height);

/* D3DDevice_Present, over the picture in the window (its bounds in the
window's framebuffer, from the bottom left), before the swap: the red dot
while recording (capture.record_indicator), never in the recording */
void capture_present_overlay(int x, int y, int width, int height);

/* the mixer (dsound_sdl.c), from its thread: interleaved float samples */
void capture_audio(const float *samples, unsigned int frames, unsigned int channels, unsigned int rate);

/* the debug screenshots (debug.screenshot_format = "png"): the whole
picture, 4 bytes a pixel, blue green red and an ignored byte, row 0 the
top; 1 on success */
int capture_png_write_bgra(const char *path, const unsigned char *pixels, int width, int height);

/* the notice to draw now ("SCREENSHOT SAVED", "RECORDING NEEDS FFMPEG"),
for about 1.5 seconds (main.c, main_framerate_render): 0 none, 1 news, 2 a
failure */
int halo_capture_notice(char *text, int size);

#endif
