/* ae_motion.c: Arena Evolved menus, motion (see ae_motion.h). No engine includes.

Offsets and scales are eased (cubic ease-out); fades are linear in the motion's progress, reaching their end by the
spec's share of the time (screen: the old gone by 60 %, the new in by 80 %). */

#include "ae_motion.h"

/* the screen transition's fades, as shares of its time */
#define SCREEN_OLD_GONE 0.6f
#define SCREEN_NEW_IN 0.8f
#define DIALOG_SCALE_FROM 0.96f

static unsigned long now;
static int reduced;

static float clamp01(float t)
{
	return t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t;
}

float ae_ease_out(float t)
{
	float rest = 1.0f - clamp01(t);

	return 1.0f - rest * rest * rest;
}

void ae_motion_set_now(unsigned long now_ms)
{
	now = now_ms;
}

unsigned long ae_motion_now(void)
{
	return now;
}

void ae_motion_set_reduced(int reduce)
{
	reduced = reduce != 0;
}

int ae_motion_reduced(void)
{
	return reduced;
}

float ae_motion_progress(struct ae_motion const *motion)
{
	unsigned long elapsed;

	if (reduced || !motion->duration)
		return 1.0f;
	/* (offsets from the start: a wrapping clock is fine) */
	elapsed = now - motion->start;
	return elapsed >= motion->duration ? 1.0f : (float)elapsed / (float)motion->duration;
}

float ae_motion_value(struct ae_motion const *motion)
{
	return motion->from + (motion->to - motion->from) * ae_ease_out(ae_motion_progress(motion));
}

int ae_motion_running(struct ae_motion const *motion)
{
	return ae_motion_progress(motion) < 1.0f;
}

void ae_motion_start(struct ae_motion *motion, float from, float to, unsigned short duration_ms)
{
	if (ae_motion_running(motion))
		from = ae_motion_value(motion);
	motion->start = now;
	motion->duration = reduced ? 0 : duration_ms;
	motion->from = from;
	motion->to = to;
}

void ae_motion_finish(struct ae_motion *motion)
{
	motion->duration = 0;
}

int ae_caret_visible(unsigned long since_ms)
{
	if (reduced)
		return 1;
	return ((now - since_ms) / AE_MOTION_CARET_MS) % 2 == 0;
}

void ae_motion_screen(float t, int direction, float *old_x_u, float *old_alpha, float *new_x_u, float *new_alpha)
{
	float eased = ae_ease_out(t);
	float shift = (float)AE_MOTION_SCREEN_SHIFT_U * (float)(direction < 0 ? -1 : 1);

	t = clamp01(t);
	*old_x_u = -shift * eased;
	*old_alpha = 1.0f - clamp01(t / SCREEN_OLD_GONE);
	*new_x_u = shift * (1.0f - eased);
	*new_alpha = clamp01(t / SCREEN_NEW_IN);
}

void ae_motion_dialog(float t, int opening, float *scrim, float *scale, float *alpha)
{
	float shown = ae_ease_out(t);

	if (!opening)
		shown = 1.0f - shown;
	*scrim = shown;
	*scale = DIALOG_SCALE_FROM + (1.0f - DIALOG_SCALE_FROM) * shown;
	*alpha = shown;
}

void ae_motion_value_tick(float t, int side, float *old_x_u, float *old_alpha, float *new_x_u, float *new_alpha)
{
	float eased = ae_ease_out(t);
	float shift = (float)AE_MOTION_VALUE_SHIFT_U * (float)(side < 0 ? -1 : 1);

	t = clamp01(t);
	*old_x_u = -shift * eased;
	*old_alpha = 1.0f - t;
	*new_x_u = shift * (1.0f - eased);
	*new_alpha = t;
}
