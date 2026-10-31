/*
CAPTURE.H

Screenshots and video recording (capture.c): the Screenshot action
(controls.screenshot, F10 unless rebound) writes the whole frame as a
PNG to screenshots/ beside config.toml; F9 starts and stops a recording,
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

/* by name, for the test runner (ae_test_run): a screenshot at the next frame, as the Screenshot action takes it, saved
at config_folder()/relative_png ("test-runs/shots/a.png"; its folders are made, a file there is replaced); a
recording that starts at the next frame into config_folder()/relative_mp4, the muxed file there when it is saved (not
if one is running or still being saved); the recording stopped at the next frame. A name must be letters, digits,
"-", "_", "." and "/" only, relative, with no ".." (capture_name.h): else nothing is taken and a notice tells.
capture_recording_state: 0 idle, 1 recording, 2 stopped and the writer still saving, -1 no ffmpeg (the dedicated
server and Android: always -1, and the three requests do nothing) */
void capture_request_screenshot_as(const char *relative_png);
void capture_request_recording_as(const char *relative_mp4);
void capture_request_recording_stop(void);
int capture_recording_state(void);

/* the test hook debug.capture_on_event (OFF by default: the call does nothing
while it is empty): a game event of this kind ("hud", "pickup", "kill",
"death", "start", "postgame") with its text, a screenshot delay_ms later.
The setting is a comma list of kinds, a kind optionally followed by ":text"
to need that text in the event's ("hud:Spree"); each capture is a line in the
log ("test capture: ...") and a PNG in screenshots/ */
void capture_test_event(const char *kind, const char *detail, int delay_ms);

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
top (malloc'd), to be written as a PNG at path by the screenshot thread,
never on the caller's: 1 queued (pixels are the thread's to free), 0
skipped and counted (the queue full), -1 no PNG here (the server's and
Android's builds); on 0 and -1 the pixels stay the caller's */
int capture_png_queue_bgra(const char *path, unsigned char *pixels, int width, int height);

/* quitting (atexit, and platform_restart before it replaces the game): a
recording stopped and saved, the screenshots written, each within a bounded
wait, after which only the capture's own ffmpeg is killed and unfinished
files removed */
void capture_shutdown(void);

/* platform_restart: the restart failed (execv returned), so capture goes on
as before, unless a thread of the shutdown is still running */
void capture_resume(void);

/* the notice to draw now ("SCREENSHOT SAVED", "RECORDING NEEDS FFMPEG"),
for about 1.5 seconds (main.c, main_framerate_render): 0 none, 1 news, 2 a
failure */
int halo_capture_notice(char *text, int size);

#endif
