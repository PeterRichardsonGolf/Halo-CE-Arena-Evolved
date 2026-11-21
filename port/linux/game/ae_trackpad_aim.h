/*
AE_TRACKPAD_AIM.H

What the AE hook line of the Steam Controller's right trackpad calls in the game's sources (ae_hooks.txt lists that
line). Plain types only.
*/

#ifndef __AE_TRACKPAD_AIM_H
#define __AE_TRACKPAD_AIM_H

/* (player_control.c, after the gyroscope's turn) the right trackpad's turn added to the facing, divided by the zoom's
magnification as the gyroscope's is (a unit and a zoom level, guarded as that division is). input.trackpad_aim off,
or a pad without one: the facing is left as it is. yaw and pitch: the facing's change, radians */
void ae_trackpad_aim(short gamepad_index, long unit_index, short zoom_level, float *yaw, float *pitch);

#endif
