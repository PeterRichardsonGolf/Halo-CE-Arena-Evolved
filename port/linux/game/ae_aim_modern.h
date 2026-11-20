/*
AE_AIM_MODERN.H

AIM CONTROL: MODERN (input.aim_control, input.look_acceleration, input.look_sensitivity_vertical,
input.look_radial_deadzone; Settings > Gamepads): the modern gamepad look in the game (ae_aim_modern_rules.h has the
rules). It acts for a controller only with MODERN set, and the stick layout DEFAULT or SOUTHPAW (the LEGACY layouts
keep the game's own); CLASSIC, the mouse's look and the other layouts never reach any of it, and the game's own look
stays byte for byte as it was.
*/

#ifndef AE_AIM_MODERN_H
#define AE_AIM_MODERN_H

/* whether the modern look acts for the controller now (MODERN set, a layout it covers) */
int ae_aim_modern_active(short gamepad_index);

/* (input_abstraction.c, the stick's squaring: an AE hook line) whether the controller's stick (a _gamepad_stick_
value) is its look stick left as the dead zones leave it, not squared as the game's own look squares it */
int ae_aim_modern_unsquared(short controller_index, int stick);

/* (ae_stick_dead_zones.c) the sticks' dead zones when the modern look acts: the set percent (0: DEFAULT) on each
axis and then on the stick; DEFAULT: the look stick's 12 %, the move stick's 12.5 %. The look stick may have a separate
radial percent. Returns 0 (nothing done) when it does not act */
int ae_aim_modern_dead_zones(short gamepad_index, short *left_x, short *left_y, short *right_x, short *right_y,
	long left_percent, long right_percent);

/* (player_control.c, after the stick's look rate: an AE hook line) the turn from the stick through the modern curve in
place of the tag's look function, as the game works it out: the rate (degrees a tick in radians) and the spin scales;
look_yaw and look_pitch are the stick (not squared, its length through the curve). A separate vertical sensitivity
scales pitch here. Leaves them alone unless it acts */
void ae_aim_modern_rate(short gamepad_index, float stick_yaw, float stick_pitch, float yaw_spin_scale,
	float pitch_spin_scale, float yaw_rate, float pitch_rate, float *look_yaw, float *look_pitch);

/* (player_control.c, in place of the tag's acceleration: an AE hook line) the hold-to-turn ramp on the turn (yaw
accelerating to 3 times, pitch to 2 times), with each local player's own timers; scaled_yaw and scaled_pitch are the
stick's values after the game's diagonal scaling and clamp. With the modern look not acting it clears the timers */
void ae_aim_modern_ramp_apply(short gamepad_index, short local_player_index, float scaled_yaw, float scaled_pitch,
	float time_delta_sec, float *look_yaw, float *look_pitch);

#endif
