/*
AE_DEV_HIT_MARKERS_RULES.H

Pure rules for DEV MODE's hit markers (ae_dev_hit_markers.c). No engine includes.

The look (owner 2026-10-24, draft 2's D): four corner brackets boxing the
reticle, AE white on a hit, AE red and a little larger on a kill, each
stroke on the HUD's dark keyline. A hit shows at once, holds 0.10 s and
fades over 0.10 s; another hit starts it again. A kill pops in at 125 % and
settles to 100 % in 0.08 s, holds to 0.28 s and fades over 0.15 s.
*/

#ifndef AE_DEV_HIT_MARKERS_RULES_H
#define AE_DEV_HIT_MARKERS_RULES_H

#define AE_DEV_HIT_MARKER_HIT_HOLD_MS 100
#define AE_DEV_HIT_MARKER_HIT_FADE_MS 100
#define AE_DEV_HIT_MARKER_KILL_POP_MS 80
#define AE_DEV_HIT_MARKER_KILL_HOLD_MS 280
#define AE_DEV_HIT_MARKER_KILL_FADE_MS 150
#define AE_DEV_HIT_MARKER_KILL_POP_SCALE 1.25f

/* the marker's alpha (0 when it is over) and size scale, elapsed_ms after its hit or kill */
void ae_dev_hit_marker_shape(long elapsed_ms, int kill, float *alpha, float *scale);

/* whether a damage counts: dealt by a player of this machine (owner_local), to anything but that player's own unit or
the vehicle it rides, some damage or a kill; a unit already dead counts only as its kill */
int ae_dev_hit_marker_counts(int owner_local, int own_unit, int own_vehicle, float damage, int kill, int already_dead);

/* the brackets' half-size in pixels: the reticle's drawn half-extent (0 when none was drawn: half_default) plus the gap,
both at the HUD's scale */
float ae_dev_hit_marker_half_size(float reticle_half_px, float half_default_px, float gap_px, float scale);

#endif
