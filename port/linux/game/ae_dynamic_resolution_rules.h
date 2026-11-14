/*
AE_DYNAMIC_RESOLUTION_RULES.H

TARGET 60 FPS (display.target_60_fps for one player, display.split_target_60_fps for two to four): the 3D picture of
every view is drawn at a part of its resolution each way and scaled up to the view before the HUD and menus are drawn
over it, so that a frame takes no more than AE_DYNRES_SLOW_MS (60 frames a second with room). Pure rules
(port/linux/tests/ae_dynamic_resolution_rules_test.c); ae_dynamic_resolution.c feeds them each frame's time.

- The scale is a whole percent each way, from 100 down to AE_DYNRES_FLOOR, in steps of AE_DYNRES_STEP.
- Down fast: after AE_DYNRES_SLOW_FRAMES frames in a row over AE_DYNRES_SLOW_MS, the scale drops to what the last of
  them says would fit (the pixels drawn go with the square of the scale), at least one step.
- Up slowly: after AE_DYNRES_FAST_FRAMES frames in a row under AE_DYNRES_FAST_MS, one step up.
- After a change the next AE_DYNRES_SETTLE_FRAMES frames are not counted (the GPU's timings come a few frames late).
- A frame of AE_DYNRES_HITCH_MS or more (a load, a hitch) counts neither way and starts both counts again.
- The scale is the same for every view of a frame; it goes back to 100 when the setting is off or the number of views
  changes, and the next AE_DYNRES_START_FRAMES frames are not counted (a game's first frames, its players spawning,
  are slow once: on the box's GPU three of them dropped a view to 60 % for the eight seconds it took to climb back).
*/

#ifndef AE_DYNAMIC_RESOLUTION_RULES_H
#define AE_DYNAMIC_RESOLUTION_RULES_H

#define AE_DYNRES_FULL 100
#define AE_DYNRES_FLOOR 50
#define AE_DYNRES_STEP 5
#define AE_DYNRES_SLOW_MS 15.0f
#define AE_DYNRES_SLOW_FRAMES 3
#define AE_DYNRES_FAST_MS 12.0f
#define AE_DYNRES_FAST_FRAMES 60
#define AE_DYNRES_SETTLE_FRAMES 3
#define AE_DYNRES_START_FRAMES 90
#define AE_DYNRES_HITCH_MS 250.0f

struct ae_dynres_state
{
	/* the scale, percent each way */
	int percent;
	int slow_frames, fast_frames, settle_frames;
	/* what the scale was last set up for: whether it is on, and the number of views */
	int on, views;
};

/* the state at 100 percent */
void ae_dynres_reset(struct ae_dynres_state *state);

/* one frame: whether the setting is on for these views now, how many views, the frame's time (ms; the slower of the
CPU's work and the GPU's, as measured). Returns the percent for the next frame */
int ae_dynres_frame(struct ae_dynres_state *state, int on, int views, float frame_ms);

/* the percent that would fit a frame of frame_ms drawn at percent: percent x sqrt(AE_DYNRES_SLOW_MS / frame_ms),
down to a step and kept from AE_DYNRES_FLOOR to AE_DYNRES_FULL */
int ae_dynres_fitting_percent(int percent, float frame_ms);

/* a forced percent (debug.dynamic_resolution_force, HALO_DYNAMIC_RESOLUTION_FORCE): 0 for none, else kept from
AE_DYNRES_FLOOR to AE_DYNRES_FULL (a value of 1 to 49 is the floor) */
int ae_dynres_forced(long value);

#endif
