/*
AE_STEAM_PAD.H

The Steam Controller (2026) on PC (Linux, Windows, macOS), through SDL's own driver of it (the HIDAPI one: no Steam
Input, no driver of AE's): which pad it is, what it gets the first time it is plugged in, and its right trackpad as an
aim (ae_steam_pad_rules.h has the arithmetic). Its four grips are SDL's left and right paddles 1 and 2, read by
ae_platform.c's ae_platform_back_buttons; its gyroscope by ae_gyro.c. Android, the web build and the server have no
such pad: the functions here say none.
*/

#ifndef AE_STEAM_PAD_H
#define AE_STEAM_PAD_H

struct SDL_Gamepad;

/* ae_steam_pad_rules.h's AE_STEAM_PAD_*: what SDL's vendor and product ids say this pad is */
int ae_steam_pad_kind(struct SDL_Gamepad *gamepad);
/* a pad opened (SDL_EVENT_GAMEPAD_ADDED): a Valve pad's detection logged, once; the first Steam Controller seen has
TRACKPAD AIM switched on unless the player chose it either way already (gyro aiming is this PC's for every pad: left) */
void ae_steam_pad_seen(struct SDL_Gamepad *gamepad);
/* the turn of this pad's right trackpad since the last call, radians (yaw positive left, pitch positive up), at this
sensitivity; 0 for a pad without one */
void ae_steam_pad_trackpad_turn(struct SDL_Gamepad *gamepad, float sensitivity, float *yaw, float *pitch);

#endif
