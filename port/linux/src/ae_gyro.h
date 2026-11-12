/*
AE_GYRO.H

Gyro aiming on PC (Linux, Windows, macOS): a controller's gyroscope (DualSense,
DualShock 4, Switch Pro Controller, Joy-Con) read through SDL's gamepad
sensors. xinput_sdl.c's halo_linux_gyro_look (input.gyro_*) asks for each pad's
turn; the Switch console's host reads its own sensor (host_sdl_gamepad_gyro),
and Android, the web build and the server turn nothing.
*/

#ifndef AE_GYRO_H
#define AE_GYRO_H

/* gamepad: an SDL_Gamepad. The turn since this pad's last read, radians (ae_gyro_rules.h's axes); 0 for a pad
without a gyroscope and on the read that switches its sensor on */
void ae_gyro_turn(void *gamepad, float *yaw, float *pitch);

#endif
