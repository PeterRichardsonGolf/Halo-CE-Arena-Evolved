/*
AE_STEAM_PAD.C

The Steam Controller (2026) on PC (ae_steam_pad.h).
*/

#include "platform.h"
#include "port_config.h"
#include "ae_steam_pad.h"
#include "ae_steam_pad_rules.h"

/* port_config.c: whether a setting was set (the file, the environment or SETTINGS), not left at its default */
int ae_config_chosen(const char *name);

#if !defined(HALO_SERVER) && !defined(HALO_ANDROID) && !defined(HALO_WEB) && !defined(HALO_SWITCH_CONSOLE)
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_sensor.h>
#include <SDL3/SDL_timer.h>
#include <string.h>

/* the right trackpad of a Valve pad: SDL's second touchpad (the first is the left one), its first finger */
#define RIGHT_TOUCHPAD 1

enum { STEAM_PADS = 16 };

/* each pad read so far (a pad unplugged keeps its slot: SDL never gives its id again) */
static struct
{
	SDL_JoystickID id;
	Uint64 read_at;
	struct ae_trackpad_rules rules;
} pads[STEAM_PADS];
/* the pads whose detection is logged */
static SDL_JoystickID logged[STEAM_PADS];

int ae_steam_pad_kind(
	struct SDL_Gamepad *gamepad)
{
	if (!gamepad)
		return AE_STEAM_PAD_NONE;
	return ae_steam_pad_rules_kind(SDL_GetGamepadVendor(gamepad), SDL_GetGamepadProduct(gamepad));
}

void ae_steam_pad_seen(
	struct SDL_Gamepad *gamepad)
{
	SDL_JoystickID id;
	int kind = ae_steam_pad_kind(gamepad), index;

	if (kind == AE_STEAM_PAD_NONE)
		return;
	id = SDL_GetGamepadID(gamepad);
	for (index = 0; index < STEAM_PADS && logged[index] && logged[index] != id; index++);
	if (index == STEAM_PADS)
	{
		for (index = 0; index < STEAM_PADS && logged[index] && SDL_GetGamepadFromID(logged[index]); index++);
		if (index == STEAM_PADS)
			return;
	}
	else if (logged[index] == id)
		return;
	logged[index] = id;
	platform_log("gamepad: Valve vendor id 0x%04x, product 0x%04x, SDL name \"%s\": %s (touchpads %d, gyroscope %s)",
		(unsigned int)SDL_GetGamepadVendor(gamepad), (unsigned int)SDL_GetGamepadProduct(gamepad),
		SDL_GetGamepadName(gamepad) ? SDL_GetGamepadName(gamepad) : "",
		kind == AE_STEAM_PAD_CONTROLLER ? "Steam Controller (2026)" : "another Valve pad",
		SDL_GetNumGamepadTouchpads(gamepad), SDL_GamepadHasSensor(gamepad, SDL_SENSOR_GYRO) ? "yes" : "no");
	if (kind == AE_STEAM_PAD_CONTROLLER && !ae_config_chosen("input.trackpad_aim"))
	{
		/* (the first Steam Controller: its trackpad aims, unless the player has chosen either way; gyro aiming is
		this PC's for every pad, so it is left as it is) */
		config_write("input.trackpad_aim", "true");
		platform_log("steam pad: first Steam Controller: trackpad aim on");
	}
}

void ae_steam_pad_trackpad_turn(
	struct SDL_Gamepad *gamepad,
	float sensitivity,
	float *yaw,
	float *pitch)
{
	SDL_JoystickID id;
	Uint64 now;
	bool down = false;
	float x = 0.0f, y = 0.0f, pressure = 0.0f;
	int index;

	*yaw = 0.0f;
	*pitch = 0.0f;
	if (!gamepad || ae_steam_pad_kind(gamepad) == AE_STEAM_PAD_NONE || SDL_GetNumGamepadTouchpads(gamepad) <= RIGHT_TOUCHPAD)
		return;
	id = SDL_GetGamepadID(gamepad);
	for (index = 0; index < STEAM_PADS && pads[index].id && pads[index].id != id; index++);
	if (index == STEAM_PADS)
	{
		/* (a slot of a pad that is gone, its SDL id never given again, is free) */
		for (index = 0; index < STEAM_PADS && pads[index].id && SDL_GetGamepadFromID(pads[index].id); index++);
		if (index == STEAM_PADS)
			return;
		memset(&pads[index], 0, sizeof(pads[index]));
	}
	now = SDL_GetTicksNS();
	if (pads[index].id != id)
	{
		/* (the first read starts the pad's clock) */
		memset(&pads[index], 0, sizeof(pads[index]));
		pads[index].id = id;
		pads[index].read_at = now;
		return;
	}
	if (!SDL_GetGamepadTouchpadFinger(gamepad, RIGHT_TOUCHPAD, 0, &down, &x, &y, &pressure))
		down = false;
	ae_trackpad_rules_step(&pads[index].rules, down, x, y, (double)(now - pads[index].read_at) / 1e9, sensitivity, yaw,
		pitch);
	pads[index].read_at = now;
}
#else
int ae_steam_pad_kind(
	struct SDL_Gamepad *gamepad)
{
	(void)gamepad;
	return AE_STEAM_PAD_NONE;
}

void ae_steam_pad_seen(
	struct SDL_Gamepad *gamepad)
{
	(void)gamepad;
}

void ae_steam_pad_trackpad_turn(
	struct SDL_Gamepad *gamepad,
	float sensitivity,
	float *yaw,
	float *pitch)
{
	(void)gamepad;
	(void)sensitivity;
	*yaw = 0.0f;
	*pitch = 0.0f;
}
#endif
