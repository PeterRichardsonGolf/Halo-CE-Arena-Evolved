/*
AE_GYRO.C

Gyro aiming on PC: each pad's gyroscope through SDL (ae_gyro.h). A pad's
sensor is switched on at its first read; each read takes the sensor's rate now
and the time since that pad's last read (ae_gyro_rules.c).
*/

#include "ae_gyro.h"
#include "ae_gyro_rules.h"

#if !defined(HALO_SERVER) && !defined(HALO_ANDROID) && !defined(HALO_WEB) && !defined(HALO_SWITCH_CONSOLE)
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_sensor.h>
#include <SDL3/SDL_timer.h>

enum { GYRO_PADS = 16 };

/* each pad read so far (a pad unplugged keeps its slot: SDL never gives its id again) */
static struct
{
	SDL_JoystickID id;
	Uint64 read_at;
	bool on;
} pads[GYRO_PADS];

void ae_gyro_turn(
	void *gamepad_handle,
	float *yaw,
	float *pitch)
{
	SDL_Gamepad *gamepad = gamepad_handle;
	SDL_JoystickID id;
	Uint64 now;
	float rate[3];
	int index;

	*yaw = 0.0f;
	*pitch = 0.0f;
	if (!gamepad || !SDL_GamepadHasSensor(gamepad, SDL_SENSOR_GYRO))
		return;
	id = SDL_GetGamepadID(gamepad);
	for (index = 0; index < GYRO_PADS && pads[index].id && pads[index].id != id; index++);
	if (index == GYRO_PADS)
		return;
	now = SDL_GetTicksNS();
	if (pads[index].id != id || !pads[index].on || !SDL_GamepadSensorEnabled(gamepad, SDL_SENSOR_GYRO))
	{
		/* (the first read switches the sensor on and starts the pad's clock) */
		pads[index].id = id;
		pads[index].on = SDL_SetGamepadSensorEnabled(gamepad, SDL_SENSOR_GYRO, true);
		pads[index].read_at = now;
		return;
	}
	if (!SDL_GetGamepadSensorData(gamepad, SDL_SENSOR_GYRO, rate, 3))
		return;
	ae_gyro_rules_turn(rate, (double)(now - pads[index].read_at) / 1e9, yaw, pitch);
	pads[index].read_at = now;
}
#else
void ae_gyro_turn(
	void *gamepad,
	float *yaw,
	float *pitch)
{
	(void)gamepad;
	*yaw = 0.0f;
	*pitch = 0.0f;
}
#endif
