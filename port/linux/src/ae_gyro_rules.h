/*
AE_GYRO_RULES.H

Gyro aiming on PC, the arithmetic (ae_gyro.c reads the sensor): a pad's
gyroscope rate, radians a second about SDL's axes (x across the pad: pitch;
y up through it: yaw; counter-clockwise looking down an axis is positive), and
the seconds since the last read give the turn in radians. Yaw positive turns
left and pitch positive looks up, as the game's facing does, so turning the pad
right looks right and tilting its front up looks up (xinput_sdl.c's
halo_linux_gyro_look then applies input.gyro_* sensitivity and invert).
*/

#ifndef AE_GYRO_RULES_H
#define AE_GYRO_RULES_H

/* a rate under this, radians a second (about half a degree), is a pad at rest: the sensor's own drift, no turn */
#define AE_GYRO_RULES_STILL 0.01f
/* the longest step a read covers, seconds: a pad read again after a pause (a menu, a loading screen) does not
turn the view by everything it turned meanwhile */
#define AE_GYRO_RULES_LONGEST_STEP 0.1

/* rate: x, y and z, radians a second; seconds since the last read (0 or less: none). yaw, pitch: radians */
void ae_gyro_rules_turn(float const rate[3], double seconds, float *yaw, float *pitch);

#endif
