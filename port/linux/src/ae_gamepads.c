/*
AE_GAMEPADS.C

Arena Evolved: gamepads on the platform layer (ae_gamepads.h). From
DamnationCE v0.3.41 (CC0).
*/

#include "platform.h"
#include "ae_gamepads.h"

#if !defined(HALO_SERVER) && !defined(HALO_ANDROID)
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_stdinc.h>
#ifdef __linux__
#include <sys/utsname.h>
#endif

/* which driver reads PlayStation pads (said in debug.txt with the first pad:
the hints are given before debug.txt is open) */
static char drivers[160];
#endif

#if defined(__linux__) && !defined(HALO_SERVER) && !defined(HALO_ANDROID)
/* whether the kernel has a HID driver: loaded (its sysfs folder), or a module
it loads when the pad is plugged in, or built in (this kernel's
modules.dep, modules.builtin). (AE: DamnationCE looked for a loaded driver
alone, which a pad not yet plugged in at the start has not made load) */
static int kernel_driver(const char *driver, const char *module)
{
	char path[256];
	char file[64];
	struct utsname system;
	int found = 0;
	int index;

	SDL_snprintf(path, sizeof(path), "/sys/bus/hid/drivers/%s", driver);
	if (SDL_GetPathInfo(path, NULL))
		return 1;
	if (uname(&system) != 0)
		return 0;
	SDL_snprintf(file, sizeof(file), "/%s.ko", module);
	for (index = 0; index < 2 && !found; index++)
	{
		char *text;

		SDL_snprintf(path, sizeof(path), "/lib/modules/%s/%s", system.release, index ? "modules.builtin" : "modules.dep");
		text = SDL_LoadFile(path, NULL);
		found = text && SDL_strstr(text, file);
		SDL_free(text);
	}
	return found;
}
#endif

void ae_gamepads_hints(
	void)
{
#if defined(__linux__) && !defined(HALO_SERVER) && !defined(HALO_ANDROID)
	int playstation = kernel_driver("playstation", "hid-playstation");
	int sony = kernel_driver("sony", "hid-sony");

	/* (a PlayStation pad through the kernel's driver of it where the kernel
	has one: SDL's own reads its hidraw node, which only udev rules open) */
	if (playstation)
		SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS5, "0");
	if (sony)
		SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4, "0");
	SDL_snprintf(drivers, sizeof(drivers), "gamepads: PlayStation pads through %s (DualSense), %s (DualShock 4)",
		playstation ? "the kernel's hid-playstation" : "SDL's own driver",
		sony ? "the kernel's hid-sony" : "SDL's own driver");
	platform_log("%s", drivers);
#endif
}

void ae_gamepads_log(
	struct SDL_Gamepad *gamepad)
{
#if !defined(HALO_SERVER) && !defined(HALO_ANDROID)
	const char *name;
	const char *type;
	const char *path;

	if (!gamepad)
		return;
	if (drivers[0])
	{
		platform_log("%s", drivers);
		drivers[0] = 0;
	}
	name = SDL_GetGamepadName(gamepad);
	type = SDL_GetGamepadStringForType(SDL_GetGamepadType(gamepad));
	path = SDL_GetGamepadPath(gamepad);
	platform_log("gamepad: %s is a %s pad%s%s", name ? name : "(unnamed)", type && type[0] ? type : "unknown",
		path ? ", read through " : "", path ? path : "");
#else
	(void)gamepad;
#endif
}
