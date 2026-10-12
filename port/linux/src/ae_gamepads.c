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
it loads when the pad is plugged in, or built in (this kernel's modules.dep,
modules.builtin, each read once). (AE: DamnationCE looked for a loaded driver
alone, which a pad not yet plugged in at the start has not made load) */
static int kernel_driver(const char *driver, const char *module, const char *modules_dep, const char *modules_builtin)
{
	char path[256];

	SDL_snprintf(path, sizeof(path), "/sys/bus/hid/drivers/%s", driver);
	return SDL_GetPathInfo(path, NULL) || ae_gamepads_kernel_has(modules_dep, modules_builtin, module);
}
#endif

void ae_gamepads_hints(
	void)
{
#if defined(__linux__) && !defined(HALO_SERVER) && !defined(HALO_ANDROID)
	struct utsname system;
	char path[256];
	char *modules_dep = NULL;
	char *modules_builtin = NULL;
	int playstation, sony;

	if (uname(&system) == 0)
	{
		SDL_snprintf(path, sizeof(path), "/lib/modules/%s/modules.dep", system.release);
		modules_dep = SDL_LoadFile(path, NULL);
		SDL_snprintf(path, sizeof(path), "/lib/modules/%s/modules.builtin", system.release);
		modules_builtin = SDL_LoadFile(path, NULL);
	}
	playstation = kernel_driver("playstation", "hid-playstation", modules_dep, modules_builtin);
	sony = kernel_driver("sony", "hid-sony", modules_dep, modules_builtin);
	SDL_free(modules_dep);
	SDL_free(modules_builtin);
	/* (a PlayStation pad through the kernel's driver of it where the kernel
	has one: SDL's own reads its hidraw node, which only udev rules open.
	hid-playstation drives the DualShock 4 too, since Linux 6.2) */
	if (playstation)
		SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS5, "0");
	if (playstation || sony)
		SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4, "0");
	SDL_snprintf(drivers, sizeof(drivers), "gamepads: DualSense through %s, DualShock 4 through %s",
		playstation ? "the kernel's driver (hid-playstation)" : "SDL's own driver",
		playstation && sony ? "the kernel's driver (hid-playstation or hid-sony)" :
		playstation ? "the kernel's driver (hid-playstation)" : sony ? "the kernel's driver (hid-sony)" :
		"SDL's own driver");
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
