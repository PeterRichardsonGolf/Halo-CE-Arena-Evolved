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

#include "ae_pad_notice_rules.h"
#include "ae_steam_pad_rules.h"

struct SDL_Gamepad;

/* what one local player's pad is and reports (ae_steam_pad_player_info) */
struct ae_pad_info
{
	int present;              /* a pad is on the player's port */
	unsigned id;              /* SDL's instance id of that pad (a staged pad: one of its own); 0 with no pad */
	int kind;                 /* ae_steam_pad_rules.h's AE_STEAM_PAD_* */
	int world;                /* AE_STEAM_WORLD_*: the player's text for the pad (also set without a pad: world NO_ACCESS) */
	unsigned int product;     /* the pad's USB product id (0: not known) */
	const char *mode;         /* a Steam Controller: "WIRED", "BLUETOOTH", "PUCK"; another pad "WIRED" or "WIRELESS"; NULL: not known */
	int power;                /* ae_pad_notice_rules.h's AE_PAD_POWER_* */
	int percent;              /* the battery's percent, -1 when not known */
	int has_gyro;             /* the pad reports a gyroscope */
	int staged;               /* the DEV `pad` command's staging made this */
	int after_low;            /* staged charging: a low battery notice was given before (the charging notice may show) */
	char name[24];            /* the short name of a pad that is not a Steam Controller (DUALSENSE), else "" */
};

/* ae_steam_pad_rules.h's AE_STEAM_PAD_*: what SDL's vendor and product ids say this pad is */
int ae_steam_pad_kind(struct SDL_Gamepad *gamepad);
/* a pad opened (SDL_EVENT_GAMEPAD_ADDED): a Valve pad's detection logged, once; the first Steam Controller seen has
TRACKPAD AIM switched on unless the player chose it either way already (gyro aiming is this PC's for every pad: left) */
void ae_steam_pad_seen(struct SDL_Gamepad *gamepad);
/* the turn of this pad's right trackpad since the last call, radians (yaw positive left, pitch positive up), at this
sensitivity; 0 for a pad without one */
void ae_steam_pad_trackpad_turn(struct SDL_Gamepad *gamepad, float sensitivity, float *yaw, float *pitch);

/* the pad on a local player's port as the player's text tells it: its kind, world, connection and battery. With the DEV
`pad world|battery|mode` staging set for the player, what that says instead of the pad. A Steam Controller that Linux has
not given the game access to counts as world NO_ACCESS when the player's pad is not a Steam one */
void ae_steam_pad_player_info(int player, struct ae_pad_info *info);
/* the same for one SDL pad (NULL: nothing); no staging, no machine facts */
void ae_steam_pad_info(struct SDL_Gamepad *gamepad, struct ae_pad_info *info);
/* whether a Steam Controller (2026) is plugged in that Linux has not let the game open, and no pad of that kind is read
by the game (always 0 off Linux); read again after a pad is added */
int ae_steam_pad_no_access(void);
/* whether any pad the game has open reports a gyroscope (a staged "no access" scene: none) */
int ae_steam_pad_other_gyro(void);

/* the DEV `pad` command's staging for local player 0 to 3: what the player's pad is taken to be, without the pad.
world: "b" (a Steam Controller read directly), "a" (behind Steam Input), "other" (Steam Input, pad not known), "c" (no
access), "dualsense", "keyboard"; "off" clears it. battery: a percent 0 to 100, "charging" (charging at the percent
given, else 14), "charged", "wired" (no battery) or "off". mode: "wired", "bluetooth", "puck" or "off". Each returns
whether the words were taken */
int ae_steam_pad_stage_world(int player, const char *world);
int ae_steam_pad_stage_battery(int player, const char *battery, int percent);
int ae_steam_pad_stage_mode(int player, const char *mode);
/* a line per local player: what the game takes each pad to be (the DEV `pad status` answer), written into text */
void ae_steam_pad_status(char *text, unsigned long size);

#endif
