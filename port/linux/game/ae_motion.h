/*
AE_MOTION.H

The AE menus' motion (ae_motion.c, spec 7): cubic ease-out, the frame's clock,
REDUCE MOTION, motions that restart from where they are, the caret's blink, and
the shapes of the screen, dialog and value-tick transitions. Nothing waits on
motion: a press during one finishes it (ae_ui.c). Pure C, no engine includes
(unit test: port/linux/tests/ae_motion_test.c).
*/

#ifndef __AE_MOTION_H
#define __AE_MOTION_H

enum
{
	AE_MOTION_SELECTION_MS = 100, AE_MOTION_SCROLL_MS = 100, AE_MOTION_SCREEN_MS = 200,
	AE_MOTION_DIALOG_OPEN_MS = 150, AE_MOTION_DIALOG_CLOSE_MS = 100, AE_MOTION_VALUE_TICK_MS = 120,
	AE_MOTION_CARET_MS = 530, AE_MOTION_SCREEN_SHIFT_U = 120, AE_MOTION_VALUE_SHIFT_U = 12
};
float ae_ease_out(float t);                          /* 1 - (1 - t)^3, t clamped to 0..1 */
void ae_motion_set_now(unsigned long now_ms);        /* the frame's clock: ae_hooks before input and drawing */
unsigned long ae_motion_now(void);
void ae_motion_set_reduced(int reduced);             /* REDUCE MOTION: every duration 0 */
int ae_motion_reduced(void);
struct ae_motion { unsigned long start; unsigned short duration; float from, to; };
/* starts from the value shown now when one is running (no jump), else from `from` */
void ae_motion_start(struct ae_motion *motion, float from, float to, unsigned short duration_ms);
float ae_motion_value(struct ae_motion const *motion);      /* eased */
float ae_motion_progress(struct ae_motion const *motion);   /* 0..1, linear */
int ae_motion_running(struct ae_motion const *motion);
void ae_motion_finish(struct ae_motion *motion);            /* jumps to the end */
int ae_caret_visible(unsigned long since_ms);               /* 530 ms on, 530 off; always 1 when reduced */
/* forward (direction 1) / back (-1) at linear progress t: the old screen moves 0 -> -120 u x direction and fades
1 -> 0 by t = 0.6; the new comes from +120 u x direction to 0 and fades in 0 -> 1 by t = 0.8 (offsets eased) */
void ae_motion_screen(float t, int direction, float *old_x_u, float *old_alpha, float *new_x_u, float *new_alpha);
/* dialog: scrim 0 -> 1 (of the scrim's own .62), scale .96 -> 1, alpha 0 -> 1 (opening); closing the reverse */
void ae_motion_dialog(float t, int opening, float *scrim, float *scale, float *alpha);
/* value tick, side -1 / 1 the pressed side: old value 0 -> -12 u x side, alpha 1 -> 0; new from +12 u x side -> 0,
alpha 0 -> 1 */
void ae_motion_value_tick(float t, int side, float *old_x_u, float *old_alpha, float *new_x_u, float *new_alpha);

#endif
