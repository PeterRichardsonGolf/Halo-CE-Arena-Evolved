/*
AE_STEAM_PAD.C

The Steam Controller (2026) on PC (ae_steam_pad.h).
*/

#include "platform.h"
#include "port_config.h"
#include "ae_steam_pad.h"
#include "ae_steam_pad_rules.h"
#include "ae_seats.h"

/* port_config.c: whether a setting was set (the file, the environment or SETTINGS), not left at its default */
int ae_config_chosen(const char *name);

#include <stdio.h>
#include <string.h>

#if !defined(HALO_SERVER) && !defined(HALO_ANDROID) && !defined(HALO_WEB) && !defined(HALO_SWITCH_CONSOLE)
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_joystick.h>
#include <SDL3/SDL_sensor.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_version.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __linux__
#include <dirent.h>
#include <unistd.h>
#endif

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
		kind == AE_STEAM_PAD_CONTROLLER ? "Steam Controller (2026)" :
		kind == AE_STEAM_PAD_STEAM_INPUT ? "Steam Input's virtual pad" : "another Valve pad",
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

/* ---------- what a pad is and reports */

/* the pad on a port (xinput_sdl.c's hook); the pad's info without staging */
#ifdef HALO_GAME_BROWSER
int halo_gamepad_pad_info(int port, struct ae_pad_info *info);
#endif

/* whether SDL says a Steam Controller is behind this pad (Steam Input's virtual pad has a real type). An SDL older than
3.4.18 has no type for it: the pad behind stays unknown there */
static int real_type_is_steam_controller(SDL_Gamepad *gamepad)
{
#if SDL_VERSION_ATLEAST(3, 4, 18)
	return SDL_GetRealGamepadType(gamepad) == SDL_GAMEPAD_TYPE_STEAM;
#else
	(void)gamepad;
	return 0;
#endif
}

static int power_of(SDL_PowerState state)
{
	switch (state)
	{
	case SDL_POWERSTATE_ON_BATTERY: return AE_PAD_POWER_BATTERY;
	case SDL_POWERSTATE_CHARGING: return AE_PAD_POWER_CHARGING;
	case SDL_POWERSTATE_CHARGED: return AE_PAD_POWER_CHARGED;
	case SDL_POWERSTATE_NO_BATTERY: return AE_PAD_POWER_NO_BATTERY;
	default: return AE_PAD_POWER_UNKNOWN;
	}
}

static int seat_family(SDL_Gamepad *gamepad)
{
	switch (SDL_GetGamepadType(gamepad))
	{
	case SDL_GAMEPAD_TYPE_XBOX360:
	case SDL_GAMEPAD_TYPE_XBOXONE: return AE_SEAT_FAMILY_XBOX;
	case SDL_GAMEPAD_TYPE_PS3:
	case SDL_GAMEPAD_TYPE_PS4:
	case SDL_GAMEPAD_TYPE_PS5: return AE_SEAT_FAMILY_PLAYSTATION;
	case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO:
	case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
	case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
	case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR: return AE_SEAT_FAMILY_NINTENDO;
	default: return AE_SEAT_FAMILY_OTHER;
	}
}

void ae_steam_pad_info(
	struct SDL_Gamepad *gamepad,
	struct ae_pad_info *info)
{
	int percent = -1;

	memset(info, 0, sizeof(*info));
	info->percent = -1;
	if (!gamepad)
		return;
	info->present = 1;
	info->id = (unsigned)SDL_GetGamepadID(gamepad);
	info->product = SDL_GetGamepadProduct(gamepad);
	info->kind = ae_steam_pad_kind(gamepad);
	info->has_gyro = SDL_GamepadHasSensor(gamepad, SDL_SENSOR_GYRO) ? 1 : 0;
	/* (assumed: SDL's real type of Steam Input's virtual pad says what is behind it; STEAM is a Steam Controller) */
	info->world = ae_steam_pad_rules_world(info->kind, real_type_is_steam_controller(gamepad), 0);
	info->power = power_of(SDL_GetJoystickPowerInfo(SDL_GetGamepadJoystick(gamepad), &percent));
	info->percent = info->power == AE_PAD_POWER_BATTERY || info->power == AE_PAD_POWER_CHARGING ||
		info->power == AE_PAD_POWER_CHARGED ? (percent >= 0 && percent <= 100 ? percent : -1) : -1;
	if (info->kind == AE_STEAM_PAD_CONTROLLER)
		info->mode = ae_steam_pad_rules_mode(info->product);
	else if (SDL_GetGamepadConnectionState(gamepad) == SDL_JOYSTICK_CONNECTION_WIRED)
		info->mode = "WIRED";
	else if (SDL_GetGamepadConnectionState(gamepad) == SDL_JOYSTICK_CONNECTION_WIRELESS)
		info->mode = "WIRELESS";
	if (info->kind != AE_STEAM_PAD_CONTROLLER)
		ae_seats_short_name(seat_family(gamepad), SDL_GetGamepadName(gamepad), 0, info->name, (int)sizeof(info->name));
}

/* ---------- Linux: a Steam Controller on the system the game may not open (world NO_ACCESS) */

static struct
{
	int scanned;
	int blocked;        /* a hidraw node of a Steam Controller (2026) cannot be opened by this user */
	unsigned long scanned_at;
} access_scan;

#ifdef __linux__
/* the USB ids of a hidraw node: its device's uevent has "HID_ID=0003:000028DE:00001302" (the bus, vendor, product) */
static int hidraw_ids(const char *node, unsigned int *vendor, unsigned int *product)
{
	char path[96], line[160];
	FILE *file;
	int found = 0;

	snprintf(path, sizeof(path), "/sys/class/hidraw/%s/device/uevent", node);
	file = fopen(path, "r");
	if (!file)
		return 0;
	while (fgets(line, sizeof(line), file))
	{
		unsigned int bus, v, p;

		if (sscanf(line, "HID_ID=%x:%x:%x", &bus, &v, &p) == 3)
		{
			*vendor = v;
			*product = p;
			found = 1;
			break;
		}
	}
	fclose(file);
	return found;
}

static int scan_blocked(void)
{
	DIR *directory = opendir("/sys/class/hidraw");
	struct dirent *entry;
	int blocked = 0;

	if (!directory)
		return 0;
	while ((entry = readdir(directory)) != NULL)
	{
		unsigned int vendor = 0, product = 0;
		char device[96];

		if (strncmp(entry->d_name, "hidraw", 6) || !hidraw_ids(entry->d_name, &vendor, &product))
			continue;
		if (ae_steam_pad_rules_kind(vendor, product) != AE_STEAM_PAD_CONTROLLER)
			continue;
		snprintf(device, sizeof(device), "/dev/%s", entry->d_name);
		/* (the node is there and this user cannot read and write it: SDL's driver cannot start) */
		if (access(device, R_OK | W_OK) != 0)
			blocked = 1;
	}
	closedir(directory);
	return blocked;
}
#else
static int scan_blocked(void)
{
	return 0;
}
#endif

/* whether SDL has a Steam Controller (2026) open */
static int controller_read(void)
{
	int count = 0, index, found = 0;
	SDL_JoystickID *ids = SDL_GetGamepads(&count);

	for (index = 0; ids && index < count && !found; index++)
		found = ae_steam_pad_rules_kind(SDL_GetGamepadVendorForID(ids[index]), SDL_GetGamepadProductForID(ids[index])) ==
			AE_STEAM_PAD_CONTROLLER;
	SDL_free(ids);
	return found;
}

static int staged_no_access(void);

int ae_steam_pad_other_gyro(void)
{
	int count = 0, index, found = 0;
	SDL_JoystickID *ids;

	if (staged_no_access())
		return 0;
	ids = SDL_GetGamepads(&count);
	for (index = 0; ids && index < count && !found; index++)
	{
		SDL_Gamepad *gamepad = SDL_GetGamepadFromID(ids[index]);

		found = gamepad && SDL_GamepadHasSensor(gamepad, SDL_SENSOR_GYRO);
	}
	SDL_free(ids);
	return found;
}

int ae_steam_pad_no_access(void)
{
	unsigned long now = (unsigned long)(SDL_GetTicks() & 0x7fffffff);

	/* (read at the first ask and again each few seconds: a pad plugged in or a rule installed shows without a restart) */
	if (!access_scan.scanned || now - access_scan.scanned_at > 3000)
	{
		int blocked = scan_blocked();

		if (!access_scan.scanned || blocked != access_scan.blocked)
			platform_log("steam pad: %s", blocked ? "a Steam Controller is plugged in and Linux has not let the game open it (the udev rules are missing)" :
				"no Steam Controller waits for access");
		access_scan.scanned = 1;
		access_scan.blocked = blocked;
		access_scan.scanned_at = now;
	}
	return access_scan.blocked && !controller_read();
}

/* ---------- the DEV `pad` command's staging */


enum { STAGE_PLAYERS = 4 };

enum { STAGE_NONE = 0, STAGE_B, STAGE_A, STAGE_OTHER, STAGE_C, STAGE_DUALSENSE, STAGE_KEYBOARD };

static struct
{
	int world;                 /* STAGE_* */
	int power_set, power, percent;
	int mode_set;
	const char *mode;
} stage[STAGE_PLAYERS];

static int staged_no_access(void)
{
	int player;

	for (player = 0; player < STAGE_PLAYERS; player++)
	{
		if (stage[player].world == STAGE_C)
			return 1;
	}
	return 0;
}

static int same(const char *a, const char *b)
{
	for (; *a && *b; a++, b++)
	{
		if ((*a >= 'A' && *a <= 'Z' ? *a + 32 : *a) != (*b >= 'A' && *b <= 'Z' ? *b + 32 : *b))
			return 0;
	}
	return !*a && !*b;
}

int ae_steam_pad_stage_world(int player, const char *world)
{
	static const struct { const char *word; int value; } words[] =
	{
		{ "off", STAGE_NONE }, { "b", STAGE_B }, { "a", STAGE_A }, { "other", STAGE_OTHER }, { "c", STAGE_C },
		{ "dualsense", STAGE_DUALSENSE }, { "keyboard", STAGE_KEYBOARD },
	};
	int index;

	if (player < 0 || player >= STAGE_PLAYERS || !world)
		return 0;
	for (index = 0; index < (int)(sizeof(words) / sizeof(words[0])); index++)
	{
		if (same(world, words[index].word))
		{
			stage[player].world = words[index].value;
			return 1;
		}
	}
	return 0;
}

int ae_steam_pad_stage_battery(int player, const char *battery, int percent)
{
	if (player < 0 || player >= STAGE_PLAYERS || !battery)
		return 0;
	if (same(battery, "off"))
	{
		stage[player].power_set = 0;
		return 1;
	}
	stage[player].power_set = 1;
	stage[player].percent = percent >= 0 && percent <= 100 ? percent : -1;
	if (same(battery, "charging"))
		stage[player].power = AE_PAD_POWER_CHARGING;
	else if (same(battery, "charged"))
		stage[player].power = AE_PAD_POWER_CHARGED;
	else if (same(battery, "wired"))
	{
		stage[player].power = AE_PAD_POWER_NO_BATTERY;
		stage[player].percent = -1;
	}
	else if (same(battery, "level"))
		stage[player].power = AE_PAD_POWER_BATTERY;
	else
	{
		stage[player].power_set = 0;
		return 0;
	}
	return 1;
}

int ae_steam_pad_stage_mode(int player, const char *mode)
{
	if (player < 0 || player >= STAGE_PLAYERS || !mode)
		return 0;
	if (same(mode, "off"))
		stage[player].mode_set = 0;
	else if (same(mode, "wired") || same(mode, "bluetooth") || same(mode, "puck"))
	{
		stage[player].mode_set = 1;
		stage[player].mode = same(mode, "wired") ? "WIRED" : same(mode, "bluetooth") ? "BLUETOOTH" : "PUCK";
	}
	else
		return 0;
	return 1;
}

/* a staged world's pad */
static void stage_world(int staged, struct ae_pad_info *info)
{
	const char *mode = info->mode;

	switch (staged)
	{
	case STAGE_B:
		info->present = 1;
		info->kind = AE_STEAM_PAD_CONTROLLER;
		info->product = AE_STEAM_PAD_PRODUCT_RECEIVER;
		info->has_gyro = 1;
		info->name[0] = 0;
		mode = ae_steam_pad_rules_mode(info->product);
		break;
	case STAGE_A:
	case STAGE_OTHER:
		info->present = 1;
		info->kind = AE_STEAM_PAD_STEAM_INPUT;
		info->product = AE_STEAM_PAD_PRODUCT_STEAM_INPUT;
		info->has_gyro = 0;
		info->name[0] = 0;
		mode = NULL;
		break;
	case STAGE_C:
		info->present = 0;
		info->kind = AE_STEAM_PAD_NONE;
		info->has_gyro = 0;
		info->name[0] = 0;
		mode = NULL;
		break;
	case STAGE_DUALSENSE:
		info->present = 1;
		info->kind = AE_STEAM_PAD_NONE;
		info->product = 0x0ce6;
		info->has_gyro = 1;
		snprintf(info->name, sizeof(info->name), "DUALSENSE");
		mode = "WIRELESS";
		break;
	default:
		info->present = 0;
		info->kind = AE_STEAM_PAD_NONE;
		info->has_gyro = 0;
		info->name[0] = 0;
		mode = NULL;
		break;
	}
	info->mode = mode;
	info->world = staged == STAGE_C ? AE_STEAM_WORLD_NO_ACCESS :
		ae_steam_pad_rules_world(info->kind, staged == STAGE_A, 0);
	info->power = AE_PAD_POWER_UNKNOWN;
	info->percent = -1;
}

void ae_steam_pad_player_info(
	int player,
	struct ae_pad_info *info)
{
	memset(info, 0, sizeof(*info));
	info->percent = -1;
#ifdef HALO_GAME_BROWSER
	if (!halo_gamepad_pad_info(player, info))
	{
		memset(info, 0, sizeof(*info));
		info->percent = -1;
	}
#endif
	if (player >= 0 && player < STAGE_PLAYERS)
	{
		if (stage[player].world != STAGE_NONE)
		{
			stage_world(stage[player].world, info);
			info->staged = 1;
		}
		if (stage[player].power_set)
		{
			info->power = stage[player].power;
			info->percent = stage[player].percent;
			info->staged = 1;
			info->after_low = stage[player].power == AE_PAD_POWER_CHARGING;
		}
		if (stage[player].mode_set)
		{
			info->mode = stage[player].mode;
			info->staged = 1;
		}
	}
	/* (a staged pad has an id of its own per player and world; no pad, none) */
	if (info->staged && info->present)
		info->id = 0xF0000000u + (unsigned)(player < 0 ? 0 : player) * 16u + (unsigned)stage[player >= 0 && player < STAGE_PLAYERS ? player : 0].world;
	else if (info->staged)
		info->id = 0;
	/* (a Steam Controller on the system that the game cannot open, for a player whose own pad is not a Steam one) */
	if (!info->staged)
		info->world = ae_steam_pad_rules_with_access(info->world, info->present, info->world == AE_STEAM_WORLD_NONE && ae_steam_pad_no_access());
}

void ae_steam_pad_status(
	char *text,
	unsigned long size)
{
	static const char *const worlds[] = { "none", "read directly (B)", "Steam Input (A)", "Steam Input, pad not known",
		"no access (C)" };
	static const char *const powers[] = { "unknown", "on battery", "charging", "charged", "no battery" };
	unsigned long used = 0;
	int player;

	if (size)
		text[0] = 0;
	for (player = 0; player < STAGE_PLAYERS && used + 1 < size; player++)
	{
		struct ae_pad_info info;
		int written;

		ae_steam_pad_player_info(player, &info);
		if (!info.present && !info.staged && info.world == AE_STEAM_WORLD_NONE)
			continue;
		written = snprintf(text + used, size - used, "%sp%d: %s, kind %d, id %04x, world %s, %s, %s %d%%, gyro %s%s",
			used ? "; " : "", player + 1, info.present ? "pad" : "no pad", info.kind, info.product,
			worlds[info.world >= 0 && info.world <= AE_STEAM_WORLD_NO_ACCESS ? info.world : 0], info.mode ? info.mode : "link not known",
			powers[info.power >= 0 && info.power <= AE_PAD_POWER_NO_BATTERY ? info.power : 0], info.percent,
			info.has_gyro ? "yes" : "no", info.staged ? " (staged)" : "");
		if (written < 0)
			break;
		used += (unsigned long)written;
	}
	if (!used && size)
		snprintf(text, size, "no pad on any local player's port");
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

void ae_steam_pad_info(
	struct SDL_Gamepad *gamepad,
	struct ae_pad_info *info)
{
	(void)gamepad;
	memset(info, 0, sizeof(*info));
	info->percent = -1;
}

int ae_steam_pad_no_access(void)
{
	return 0;
}

int ae_steam_pad_other_gyro(void)
{
	return 0;
}

int ae_steam_pad_stage_world(int player, const char *world)
{
	(void)player;
	(void)world;
	return 0;
}

int ae_steam_pad_stage_battery(int player, const char *battery, int percent)
{
	(void)player;
	(void)battery;
	(void)percent;
	return 0;
}

int ae_steam_pad_stage_mode(int player, const char *mode)
{
	(void)player;
	(void)mode;
	return 0;
}

void ae_steam_pad_player_info(int player, struct ae_pad_info *info)
{
	(void)player;
	memset(info, 0, sizeof(*info));
	info->percent = -1;
}

void ae_steam_pad_status(char *text, unsigned long size)
{
	if (size)
		snprintf(text, size, "no pads on this build");
}
#endif
