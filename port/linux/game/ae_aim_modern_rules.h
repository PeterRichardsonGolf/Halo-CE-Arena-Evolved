/*
AE_AIM_MODERN_RULES.H

AIM CONTROL: MODERN (Settings > Gamepads), the modern gamepad look, as pure rules (no game types;
ae_aim_modern.c wires them in): the dead zones (an axial one on each axis, then a radial one on the stick), the radial
curve in place of the tag's look function, the vertical rate and the hold-to-turn ramp (the turn speeds up while the
stick is held all the way). CLASSIC never reaches any of it.
*/

#ifndef AE_AIM_MODERN_RULES_H
#define AE_AIM_MODERN_RULES_H

/* the look stick counts as held all the way from this (the diagonal-scaled, clamped value) */
#define AE_AIM_FULL_THRESHOLD 0.95f
/* the ramp's length at k 1, in seconds */
#define AE_AIM_RAMP_SECONDS 1.15f
/* the ramp's top: yaw turns up to 3 times and pitch up to 2 times as fast */
#define AE_AIM_RAMP_YAW 3.0f
#define AE_AIM_RAMP_PITCH 2.0f
/* the dead zones when DEFAULT is set (the fractions of the throw): the look stick's, and the move stick's */
#define AE_AIM_DEFAULT_LOOK_ZONE 0.12f
#define AE_AIM_DEFAULT_MOVE_ZONE 0.125f

/* one axis past its axial dead zone (the fraction of the throw, 0 to 1): the game's fix_dead_zone's formula, with the
32768 side for a negative value (pure) */
short ae_aim_dead_zone_axial(short value, float axial);
/* a stick past its radial dead zone (the fraction of the throw): its length less the zone, rescaled, the direction
kept; whole shorts, the length at most 32767; a zone of 0 does nothing (pure) */
void ae_aim_dead_zone_radial(short *x, short *y, float radial);
/* both steps on a raw stick: the axial one on each axis, then the radial one (pure) */
void ae_aim_dead_zone_stick(short *x, short *y, float axial, float radial);
/* the modern look's dead zone: its axial fraction, then the radial percent; -1 makes the radial step the same as
the axial one (pure) */
void ae_aim_modern_dead_zone(short *x, short *y, float axial, long radial_percent);
/* both physical sticks' modern zones: move and look percents (0: their defaults), and the look radial percent (-1:
same as its axial); southpaw puts the look settings on the left stick (pure) */
void ae_aim_modern_dead_zone_sticks(short *left_x, short *left_y, short *right_x, short *right_y, long move_percent,
	long look_percent, long look_radial_percent, int southpaw);

/* the modern curve: the rate's fraction (1 at 1) for the stick's length r, no clamp (past 1 on a diagonal) (pure) */
float ae_aim_modern_curve(float r);
/* the stick (yaw, pitch; not squared) through the curve on its length, its direction kept (pure) */
void ae_aim_modern_stick(float yaw, float pitch, float *curve_yaw, float *curve_pitch);

/* the ramp's speed factor for k from LOOK ACCELERATION: 0 at OFF, 0.70 at 1, 1.375 at 10 (pure) */
float ae_aim_modern_k(long menu);
/* one tick of an axis's ramp: the multiplier for the rate (1 to scale_max). While scaled_abs is held all the way the
multiplier follows the timer as it was before this tick, then the timer gains k * dt; below it the timer is 0 and the
multiplier 1 (pure) */
float ae_aim_modern_ramp(float *timer, float scaled_abs, float scale_max, float k, float dt);

/* VERTICAL LOOK SENSITIVITY's CE pitch-table rate over LOOK SENSITIVITY's rate; 0 is SAME (pure) */
float ae_aim_modern_pitch_ratio(long menu, float current_rate);

#endif
