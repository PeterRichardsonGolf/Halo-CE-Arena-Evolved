/*
AE_DS5.C

Arena Evolved: a PS5 DualSense's adaptive triggers, lightbar and player lights (ae_ds5.h). The bytes are
ae_ds5_rules.c's; what each player wants is the game's (port/linux/game/ae_ds5_game.c).
*/

#include <string.h>

#include "platform.h"
#include "port_config.h"
#include "ae_ds5.h"

#if !defined(HALO_SERVER) && !defined(HALO_ANDROID) && !defined(HALO_WEB)
#include <stdlib.h>
#include <SDL3/SDL_atomic.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_timer.h>
#ifdef __linux__
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#endif

/* the udev rule that lets the player at the seat write the pad's hidraw node (the Linux downloads'
70-ae-dualsense.rules, port/assets/udev/: tools/test_ae_ds5.py checks they agree) */
#define AE_DS5_UDEV_RULE \
	"KERNEL==\"hidraw*\", KERNELS==\"*:054C:0CE6.*|*:054C:0DF2.*\", MODE=\"0660\", TAG+=\"uaccess\""

#define PORTS 4
#define PADS 8
/* a port's wish this old (the player died, a menu, the game left) lets the triggers go */
#define WISH_STALE_MS 300
/* the triggers sent again though unchanged, while both are up: this long after the game's rumble changed (the
kernel's own report), and at least this often */
#define REFRESH_AFTER_RUMBLE_MS 40
#define REFRESH_EVERY_MS 1000
/* a trigger counts as up below this (SDL's axis, 0-32767) */
#define TRIGGER_UP 3000

enum backend
{
	BACKEND_UNKNOWN,
	BACKEND_NONE,   /* not a DualSense, or no way to write to it */
	BACKEND_SDL,    /* SDL's own PS5 driver */
	BACKEND_HIDRAW  /* Linux: the kernel's driver reads it, we write its hidraw node */
};

struct pad
{
	SDL_JoystickID id;
	int used;
	int port;
	enum backend backend;
	int fd;
	int bluetooth;
	int sequence;
	char node[32];
	/* what the pad has now (as far as we know) */
	int sent_any;
	int sent_rgb;
	int sent_player;
	unsigned char red, green, blue, player;
	/* (SHIELD: the bar dark while there is no player) */
	int dark;
	unsigned char right[AE_DS5_EFFECT_SIZE];
	unsigned char left[AE_DS5_EFFECT_SIZE];
	int failed_logged;
	/* the kick under way (until kick_until, SDL_GetTicks ms) and the shots seen */
	int shots_known;
	unsigned long shots;
	Uint64 kick_until;
	unsigned char kick[AE_DS5_EFFECT_SIZE];
	/* the rumble seen, and when the triggers are next sent again though unchanged */
	int rumbling;
	unsigned short rumble_left, rumble_right;
	Uint64 refresh_at;
	Uint64 triggers_sent_at;
	/* (debug.ds5_log) */
	unsigned long kicks_sent, lightbar_sends, rumble_changes, refreshes;
	int bytes_logged;
	char logged[sizeof(((struct ae_ds5_wish *)0)->what)];
};

/* shared with the game's thread and XInputSetState's (spinlock); the pads are the event thread's */
static SDL_SpinLock shared_lock;
static struct
{
	SDL_JoystickID bound[PORTS];
	struct ae_ds5_wish wish[PORTS];
	Uint64 wish_ms[PORTS];
	int wished[PORTS];
	/* the game's motors on each port (XInputSetState): the kernel sends its own report when they change */
	unsigned short rumble_left[PORTS], rumble_right[PORTS];
	/* whether the port's pad is a DualSense this machine drives (the game works out a wish for it only then) */
	int active[PORTS];
} shared;
static struct pad pads[PADS];
static int exit_registered;
/* the settings as last sent (for the change to OFF: the triggers let go, the lightbar given back once) */
static struct ae_ds5_settings applied;
static int applied_known;

/* (read by the game's thread and the event thread: their own lock) */
static SDL_SpinLock settings_lock;

struct ae_ds5_settings ae_ds5_settings_get(void)
{
	static struct ae_ds5_settings settings, logged;
	static unsigned long read_at;
	static int read;
	struct ae_ds5_settings result;

	SDL_LockSpinlock(&settings_lock);
	if (!read || read_at != config_changes())
	{
		char const *strength = config_string("input.trigger_strength");
		char const *lightbar = config_string("input.lightbar");

		settings.triggers = config_boolean("input.adaptive_triggers");
		settings.strength = ae_ds5_strength_percent(strength ? strength : "");
		settings.lights = !lightbar || strcmp(lightbar, "off");
		settings.shield = lightbar && !strcmp(lightbar, "shield");
		settings.log = config_boolean("debug.ds5_log");
		if (!read || settings.triggers != logged.triggers || settings.lights != logged.lights ||
			settings.shield != logged.shield || settings.strength != logged.strength)
			platform_log("dualsense: adaptive triggers %s (strength %d%%), lightbar %s (a DualSense's only)",
				settings.triggers ? "on" : "off", settings.strength, ae_ds5_lightbar_mode_name(&settings));
		logged = settings;
		read = 1;
		read_at = config_changes();
	}
	result = settings;
	SDL_UnlockSpinlock(&settings_lock);
	return result;
}

char const *ae_ds5_lightbar_mode_name(struct ae_ds5_settings const *settings)
{
	return !settings->lights ? "off" : settings->shield ? "shield (the shield meter)" :
		"player color (and the player lights)";
}

int ae_ds5_on(void)
{
	struct ae_ds5_settings settings = ae_ds5_settings_get();

	return settings.triggers || settings.lights;
}

int ae_ds5_port_active(int port)
{
	int active;

	if (port < 0 || port >= PORTS)
		return 0;
	SDL_LockSpinlock(&shared_lock);
	active = shared.active[port];
	SDL_UnlockSpinlock(&shared_lock);
	return active;
}

void ae_ds5_wish_set(int port, struct ae_ds5_wish const *wish)
{
	if (port < 0 || port >= PORTS || !wish)
		return;
	SDL_LockSpinlock(&shared_lock);
	shared.wish[port] = *wish;
	shared.wish[port].what[sizeof(shared.wish[port].what) - 1] = 0;
	shared.wish_ms[port] = SDL_GetTicks();
	shared.wished[port] = 1;
	SDL_UnlockSpinlock(&shared_lock);
}

void ae_ds5_port(int port, struct SDL_Gamepad *gamepad, unsigned short left_motor, unsigned short right_motor)
{
	if (port < 0 || port >= PORTS)
		return;
	SDL_LockSpinlock(&shared_lock);
	shared.bound[port] = gamepad ? SDL_GetGamepadID(gamepad) : 0;
	shared.rumble_left[port] = left_motor;
	shared.rumble_right[port] = right_motor;
	SDL_UnlockSpinlock(&shared_lock);
}

#ifdef __linux__
/* the pad's hidraw node from its input event node (/dev/input/eventN -> /sys/class/input/eventN/device/device is
the HID device, whose hidraw/ folder names the node), whether it is on Bluetooth (HID_ID's bus 0005), and whether
it is a DualSense (054C:0CE6) or DualSense Edge (054C:0DF2) */
static int hidraw_find(char const *event_path, char *node, size_t node_size, int *bluetooth)
{
	char const *name = strrchr(event_path, '/');
	char path[160];
	char line[160];
	FILE *uevent;
	DIR *folder;
	struct dirent *entry;
	unsigned int bus = 0, vendor = 0, product = 0;
	int found = 0;

	name = name ? name + 1 : event_path;
	if (strncmp(name, "event", 5))
		return 0;
	snprintf(path, sizeof(path), "/sys/class/input/%s/device/device/uevent", name);
	uevent = fopen(path, "r");
	if (!uevent)
		return 0;
	while (fgets(line, sizeof(line), uevent))
	{
		if (!strncmp(line, "HID_ID=", 7))
			sscanf(line + 7, "%x:%x:%x", &bus, &vendor, &product);
	}
	fclose(uevent);
	if (vendor != 0x054c || (product != 0x0ce6 && product != 0x0df2))
		return 0;
	*bluetooth = bus == 0x0005;
	snprintf(path, sizeof(path), "/sys/class/input/%s/device/device/hidraw", name);
	folder = opendir(path);
	if (!folder)
		return 0;
	while ((entry = readdir(folder)) != NULL)
	{
		if (!strncmp(entry->d_name, "hidraw", 6))
		{
			snprintf(node, node_size, "/dev/%s", entry->d_name);
			found = 1;
			break;
		}
	}
	closedir(folder);
	return found;
}
#endif

/* how this pad is driven, said once in debug.txt: the way, the bus, write access, the settings */
static void pad_choose_backend(struct pad *pad, SDL_Gamepad *gamepad, struct ae_ds5_settings const *settings)
{
	int log = settings->log;

	char const *name = SDL_GetGamepadName(gamepad);
	SDL_GamepadType type = SDL_GetGamepadType(gamepad);
	SDL_PropertiesID properties = SDL_GetGamepadProperties(gamepad);

	if (!name)
		name = "(unnamed)";
	pad->backend = BACKEND_NONE;
	pad->fd = -1;
	if (type != SDL_GAMEPAD_TYPE_PS5)
	{
		if (log)
			platform_log("dualsense: %s on port %d is not a DualSense: left alone", name, pad->port + 1);
		return;
	}
	if (properties && SDL_GetBooleanProperty(properties, SDL_PROP_GAMEPAD_CAP_RGB_LED_BOOLEAN, false))
	{
		pad->backend = BACKEND_SDL;
		platform_log("dualsense: %s on port %d: through SDL's own PS5 driver (effects with SDL_SendGamepadEffect); "
			"adaptive triggers %s, lightbar %s", name, pad->port + 1, settings->triggers ? "on" : "off",
			ae_ds5_lightbar_mode_name(settings));
		return;
	}
#ifdef __linux__
	{
		char const *path = SDL_GetGamepadPath(gamepad);

		if (!path || !hidraw_find(path, pad->node, sizeof(pad->node), &pad->bluetooth))
		{
			platform_log("dualsense: %s on port %d: no hidraw node found for %s: no adaptive triggers or lightbar",
				name, pad->port + 1, path ? path : "(no path)");
			return;
		}
		pad->fd = open(pad->node, O_WRONLY | O_CLOEXEC | O_NONBLOCK);
		if (pad->fd < 0)
		{
			platform_log("dualsense: %s on port %d: input through the kernel's driver (%s, %s); cannot write %s (%s): "
				"no adaptive triggers or lightbar", name, pad->port + 1, path, pad->bluetooth ? "Bluetooth" : "USB",
				pad->node, strerror(errno));
			/* (the one-line rule: USB and Bluetooth, DualSense and DualSense Edge; the HID device's own name,
			bus:vendor:product.instance, is the same on both buses) */
			platform_log("dualsense: for them, once: sudo cp 70-ae-dualsense.rules /etc/udev/rules.d/ && sudo udevadm "
				"control --reload (the file is in the download), then plug the pad in again. Its line: %s",
				AE_DS5_UDEV_RULE);
			return;
		}
		pad->backend = BACKEND_HIDRAW;
		platform_log("dualsense: %s on port %d: input through the kernel's driver (%s), effects written to %s (%s); "
			"adaptive triggers %s, lightbar %s", name, pad->port + 1, path, pad->node, pad->bluetooth ? "Bluetooth" : "USB",
			settings->triggers ? "on" : "off", ae_ds5_lightbar_mode_name(settings));
	}
#else
	platform_log("dualsense: %s on port %d: SDL's driver does not drive it: no adaptive triggers or lightbar", name,
		pad->port + 1);
#endif
}

/* sends the body (only the enable bits set in it count); 0 on failure */
static int pad_send(struct pad *pad, SDL_Gamepad *gamepad, unsigned char const body[AE_DS5_BODY_SIZE])
{
	if (pad->backend == BACKEND_SDL)
	{
		int ok = 1;

		if (gamepad && (body[AE_DS5_BODY_ENABLE1] & (AE_DS5_ENABLE1_RIGHT_TRIGGER | AE_DS5_ENABLE1_LEFT_TRIGGER)))
		{
			unsigned char triggers[AE_DS5_BODY_SIZE];

			memcpy(triggers, body, sizeof(triggers));
			triggers[AE_DS5_BODY_ENABLE2] = 0;
			ok = SDL_SendGamepadEffect(gamepad, triggers, (int)sizeof(triggers));
		}
		/* (SDL keeps its own LED state: its calls, not our bytes) */
		if (gamepad && (body[AE_DS5_BODY_ENABLE2] & AE_DS5_ENABLE2_LIGHTBAR))
			ok = SDL_SetGamepadLED(gamepad, body[AE_DS5_BODY_RED], body[AE_DS5_BODY_GREEN], body[AE_DS5_BODY_BLUE]) &&
				ok;
		if (gamepad && (body[AE_DS5_BODY_ENABLE2] & AE_DS5_ENABLE2_PLAYER_LIGHTS))
			ok = SDL_SetGamepadPlayerIndex(gamepad, ae_ds5_player_index_of_lights(body[AE_DS5_BODY_PLAYER_LIGHTS])) &&
				ok;
		if (!ok && !pad->failed_logged)
		{
			platform_log("dualsense: SDL refused the effect: %s", SDL_GetError());
			pad->failed_logged = 1;
		}
		return ok;
	}
#ifdef __linux__
	if (pad->backend == BACKEND_HIDRAW && pad->fd >= 0)
	{
		unsigned char report[AE_DS5_BLUETOOTH_REPORT_SIZE];
		size_t size = ae_ds5_output_report(report, sizeof(report), pad->bluetooth, pad->sequence, body);
		ssize_t written;

		pad->sequence = (pad->sequence + 1) & 0x0f;
		written = write(pad->fd, report, size);
		if (written != (ssize_t)size)
		{
			if (!pad->failed_logged)
				platform_log("dualsense: writing %s failed (%s)", pad->node, written < 0 ? strerror(errno) : "short");
			pad->failed_logged = 1;
			return 0;
		}
		return 1;
	}
#endif
	(void)gamepad;
	return 0;
}

/* an effect's 11 bytes in hex */
static void hex(char *text, size_t size, unsigned char const effect[AE_DS5_EFFECT_SIZE])
{
	size_t at = 0;
	int i;

	text[0] = 0;
	for (i = 0; i < AE_DS5_EFFECT_SIZE && at + 4 < size; i++)
		at += (size_t)SDL_snprintf(text + at, size - at, i ? " %02x" : "%02x", effect[i]);
}

static void pad_close(struct pad *pad)
{
#ifdef __linux__
	if (pad->fd >= 0)
		close(pad->fd);
#endif
	memset(pad, 0, sizeof(*pad));
	pad->fd = -1;
}

/* the triggers let go (if sent), and the lightbar given back (if sent): the kernel's own blue (hid-playstation's
default) on the hidraw way; SDL's driver keeps the last colour set */
static void pad_reset(struct pad *pad, SDL_Gamepad *gamepad, int triggers, int lights)
{
	unsigned char body[AE_DS5_BODY_SIZE];

	triggers = triggers && pad->sent_any;
	lights = lights && pad->sent_rgb && pad->backend == BACKEND_HIDRAW;
	if (!triggers && !lights)
		return;
	memset(body, 0, sizeof(body));
	if (triggers)
	{
		body[AE_DS5_BODY_ENABLE1] = AE_DS5_ENABLE1_RIGHT_TRIGGER | AE_DS5_ENABLE1_LEFT_TRIGGER;
		ae_ds5_effect_off(body + AE_DS5_BODY_RIGHT_TRIGGER);
		ae_ds5_effect_off(body + AE_DS5_BODY_LEFT_TRIGGER);
		ae_ds5_effect_off(pad->right);
		ae_ds5_effect_off(pad->left);
	}
	if (lights)
	{
		body[AE_DS5_BODY_ENABLE2] = AE_DS5_ENABLE2_LIGHTBAR;
		body[AE_DS5_BODY_BLUE] = 128;
		pad->sent_rgb = 0;
	}
	if (pad_send(pad, gamepad, body))
		platform_log("dualsense: port %d reset:%s%s", pad->port + 1, triggers ? " triggers off" : "",
			lights ? " lightbar blue" : "");
	if (triggers)
		pad->sent_any = 0;
}

static void ds5_exit(void)
{
	int i;

	for (i = 0; i < PADS; i++)
	{
		if (pads[i].used)
		{
			pad_reset(&pads[i], SDL_GetGamepadFromID(pads[i].id), 1, 1);
			pad_close(&pads[i]);
		}
	}
}

static struct pad *pad_for(SDL_JoystickID id, int port)
{
	int i, free_slot = -1;

	for (i = 0; i < PADS; i++)
	{
		if (pads[i].used && pads[i].id == id)
		{
			pads[i].port = port;
			return &pads[i];
		}
		if (!pads[i].used && free_slot < 0)
			free_slot = i;
	}
	if (free_slot < 0)
		return NULL;
	memset(&pads[free_slot], 0, sizeof(pads[free_slot]));
	pads[free_slot].used = 1;
	pads[free_slot].id = id;
	pads[free_slot].port = port;
	pads[free_slot].fd = -1;
	pads[free_slot].backend = BACKEND_UNKNOWN;
	return &pads[free_slot];
}

/* one pad on its port this frame */
static void pad_frame(struct pad *pad, SDL_Gamepad *gamepad, int port, struct ae_ds5_wish const *wish, int fresh,
	unsigned short rumble_left, unsigned short rumble_right, struct ae_ds5_settings const *settings, Uint64 now)
{
	unsigned char body[AE_DS5_BODY_SIZE];
	unsigned char right[AE_DS5_EFFECT_SIZE], left[AE_DS5_EFFECT_SIZE];
	int triggers = 0, rgb = 0, player = 0, refresh = 0, kicking = 0, darken;
	unsigned char dark[3];

	/* the game's rumble changed: the kernel sends a report of its own (its enable bits: rumble only); the trigger
	effects are sent again once both triggers are up, in case the pad dropped them */
	if (rumble_left != pad->rumble_left || rumble_right != pad->rumble_right)
	{
		int rumbling = rumble_left || rumble_right;

		pad->rumble_changes++;
		if (settings->log && rumbling != pad->rumbling && pad->rumble_changes <= 40)
			platform_log("dualsense: port %d rumble %s (%u %u)", port + 1, rumbling ? "on" : "off",
				(unsigned int)rumble_left, (unsigned int)rumble_right);
		pad->rumbling = rumbling;
		pad->rumble_left = rumble_left;
		pad->rumble_right = rumble_right;
		pad->refresh_at = now + REFRESH_AFTER_RUMBLE_MS;
	}

	if (settings->triggers)
	{
		if (fresh)
		{
			/* a new round: the profile's kick now, the held shape again after its time */
			if (pad->shots_known && wish->shots != pad->shots && wish->kick_ms > 0)
			{
				pad->kick_until = now + (Uint64)wish->kick_ms;
				memcpy(pad->kick, wish->kick, sizeof(pad->kick));
			}
			pad->shots = wish->shots;
			pad->shots_known = 1;
			kicking = now < pad->kick_until;
			memcpy(right, kicking ? pad->kick : wish->right, sizeof(right));
			memcpy(left, wish->left, sizeof(left));
		}
		else
		{
			pad->kick_until = 0;
			pad->shots_known = 0;
			ae_ds5_effect_off(right);
			ae_ds5_effect_off(left);
		}
		triggers = !pad->sent_any || memcmp(right, pad->right, sizeof(right)) || memcmp(left, pad->left, sizeof(left));
		if (!triggers && fresh && !kicking &&
			((pad->refresh_at && now >= pad->refresh_at) || now - pad->triggers_sent_at >= REFRESH_EVERY_MS) &&
			SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) < TRIGGER_UP &&
			SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) < TRIGGER_UP)
		{
			triggers = refresh = 1;
			pad->refresh_at = 0;
		}
	}
	if (settings->lights && fresh)
	{
		rgb = !pad->sent_rgb || wish->red != pad->red || wish->green != pad->green || wish->blue != pad->blue;
		player = wish->player_lights && (!pad->sent_player || wish->player_lights != pad->player);
		pad->dark = 0;
	}
	/* (SHIELD: no player (dead, a menu): the bar goes dark, its last colour at 6 %, until the player is back) */
	darken = settings->lights && settings->shield && !fresh && pad->sent_rgb && !pad->dark;
	if (darken)
	{
		dark[0] = (unsigned char)(pad->red * 6 / 100);
		dark[1] = (unsigned char)(pad->green * 6 / 100);
		dark[2] = (unsigned char)(pad->blue * 6 / 100);
		rgb = 1;
	}
	if (!triggers && !rgb && !player)
		return;

	memset(body, 0, sizeof(body));
	if (triggers)
	{
		body[AE_DS5_BODY_ENABLE1] = AE_DS5_ENABLE1_RIGHT_TRIGGER | AE_DS5_ENABLE1_LEFT_TRIGGER;
		memcpy(body + AE_DS5_BODY_RIGHT_TRIGGER, right, sizeof(right));
		memcpy(body + AE_DS5_BODY_LEFT_TRIGGER, left, sizeof(left));
	}
	if (rgb)
	{
		body[AE_DS5_BODY_ENABLE2] |= AE_DS5_ENABLE2_LIGHTBAR;
		body[AE_DS5_BODY_RED] = darken ? dark[0] : wish->red;
		body[AE_DS5_BODY_GREEN] = darken ? dark[1] : wish->green;
		body[AE_DS5_BODY_BLUE] = darken ? dark[2] : wish->blue;
	}
	if (player)
	{
		body[AE_DS5_BODY_ENABLE2] |= AE_DS5_ENABLE2_PLAYER_LIGHTS;
		body[AE_DS5_BODY_PLAYER_LIGHTS] = wish->player_lights;
	}
	if (!pad_send(pad, gamepad, body))
	{
		/* (one try: a pad that refuses is left alone until it comes back) */
		pad->backend = BACKEND_NONE;
		return;
	}
	if (triggers)
	{
		pad->sent_any = 1;
		memcpy(pad->right, right, sizeof(right));
		memcpy(pad->left, left, sizeof(left));
		pad->triggers_sent_at = now;
	}
	if (darken)
		pad->dark = 1;
	else if (rgb)
	{
		pad->sent_rgb = 1;
		pad->red = wish->red;
		pad->green = wish->green;
		pad->blue = wish->blue;
	}
	if (rgb)
		pad->lightbar_sends++;
	if (player)
	{
		pad->sent_player = 1;
		pad->player = wish->player_lights;
	}
	if (refresh)
		pad->refreshes++;
	if (kicking && triggers)
		pad->kicks_sent++;
	if (!settings->log)
		return;

	/* (debug.ds5_log: the bytes of the first sends and of every change of what the player wants; kicks, refreshes and
	lightbar steps counted, not logged one by one: a battle rifle burst is six reports) */
	if (pad->bytes_logged < 12 || (fresh && !kicking && !refresh && strcmp(pad->logged, wish->what)))
	{
		char right_hex[40], left_hex[40];

		pad->bytes_logged++;
		hex(right_hex, sizeof(right_hex), right);
		hex(left_hex, sizeof(left_hex), left);
		platform_log("dualsense: port %d wrote%s%s%s%s: enable %02x %02x, R2 [%s], L2 [%s], player lights %02x%s",
			port + 1, triggers ? " triggers" : "", rgb ? " lightbar" : "", player ? " player lights" : "",
			refresh ? " (refresh)" : kicking ? " (kick)" : "", body[AE_DS5_BODY_ENABLE1], body[AE_DS5_BODY_ENABLE2],
			triggers ? right_hex : "-", triggers ? left_hex : "-", body[AE_DS5_BODY_PLAYER_LIGHTS],
			pad->backend == BACKEND_SDL ? " through SDL" : "");
	}
	if (fresh && !kicking && !refresh && strcmp(pad->logged, wish->what))
	{
		strcpy(pad->logged, wish->what);
		platform_log("dualsense: port %d: %s; lightbar %d %d %d; %u kicks, %u refreshes, %u lightbar steps, "
			"%u rumble changes so far", port + 1, wish->what, wish->red, wish->green, wish->blue,
			(unsigned int)pad->kicks_sent, (unsigned int)pad->refreshes, (unsigned int)pad->lightbar_sends,
			(unsigned int)pad->rumble_changes);
	}
	else if (!fresh && triggers)
	{
		pad->logged[0] = 0;
		platform_log("dualsense: port %d: no player (menu, dead or no game): triggers off", port + 1);
	}
}

void ae_ds5_frame(void)
{
	struct ae_ds5_settings settings = ae_ds5_settings_get();
	SDL_JoystickID bound[PORTS];
	struct ae_ds5_wish wish[PORTS];
	int fresh[PORTS];
	unsigned short rumble_left[PORTS], rumble_right[PORTS];
	Uint64 now;
	int port, i;

	/* a setting turned off: the triggers let go and the lightbar given back, once */
	if (applied_known && ((applied.triggers && !settings.triggers) || (applied.lights && !settings.lights)))
	{
		for (i = 0; i < PADS; i++)
		{
			if (pads[i].used)
				pad_reset(&pads[i], SDL_GetGamepadFromID(pads[i].id), applied.triggers && !settings.triggers,
					applied.lights && !settings.lights);
		}
	}
	applied = settings;
	applied_known = 1;
	if (!settings.triggers && !settings.lights)
	{
		SDL_LockSpinlock(&shared_lock);
		memset(shared.active, 0, sizeof(shared.active));
		SDL_UnlockSpinlock(&shared_lock);
		return;
	}
	if (!exit_registered)
	{
		atexit(ds5_exit);
		exit_registered = 1;
	}
	now = SDL_GetTicks();
	SDL_LockSpinlock(&shared_lock);
	for (port = 0; port < PORTS; port++)
	{
		bound[port] = shared.bound[port];
		wish[port] = shared.wish[port];
		fresh[port] = shared.wished[port] && now - shared.wish_ms[port] < WISH_STALE_MS;
		rumble_left[port] = shared.rumble_left[port];
		rumble_right[port] = shared.rumble_right[port];
	}
	SDL_UnlockSpinlock(&shared_lock);

	/* a pad no longer bound to a port lets its triggers go */
	for (i = 0; i < PADS; i++)
	{
		int still = 0;

		if (!pads[i].used)
			continue;
		for (port = 0; port < PORTS; port++)
			still |= bound[port] == pads[i].id && pads[i].port == port;
		if (!still && pads[i].sent_any)
			pad_reset(&pads[i], SDL_GetGamepadFromID(pads[i].id), 1, 0);
	}

	for (port = 0; port < PORTS; port++)
	{
		SDL_Gamepad *gamepad = bound[port] ? SDL_GetGamepadFromID(bound[port]) : NULL;
		struct pad *pad = gamepad ? pad_for(bound[port], port) : NULL;
		int active;

		if (pad && pad->backend == BACKEND_UNKNOWN)
			pad_choose_backend(pad, gamepad, &settings);
		active = pad && pad->backend != BACKEND_NONE;
		SDL_LockSpinlock(&shared_lock);
		shared.active[port] = active;
		SDL_UnlockSpinlock(&shared_lock);
		if (active)
			pad_frame(pad, gamepad, port, &wish[port], fresh[port], rumble_left[port], rumble_right[port], &settings,
				now);
	}
}

void ae_ds5_removed(struct SDL_Gamepad *gamepad)
{
	SDL_JoystickID id;
	int i;

	if (!gamepad)
		return;
	id = SDL_GetGamepadID(gamepad);
	for (i = 0; i < PADS; i++)
	{
		if (pads[i].used && pads[i].id == id)
		{
			/* (the pad may still be there: SDL closing it for another reason) */
			pad_reset(&pads[i], gamepad, 1, 1);
			if (pads[i].backend != BACKEND_NONE)
				platform_log("dualsense: the pad on port %d left", pads[i].port + 1);
			pad_close(&pads[i]);
		}
	}
	SDL_LockSpinlock(&shared_lock);
	for (i = 0; i < PORTS; i++)
	{
		if (shared.bound[i] == id)
		{
			shared.bound[i] = 0;
			shared.active[i] = 0;
		}
	}
	SDL_UnlockSpinlock(&shared_lock);
}

#else

struct ae_ds5_settings ae_ds5_settings_get(void)
{
	struct ae_ds5_settings settings;

	memset(&settings, 0, sizeof(settings));
	settings.strength = 100;
	return settings;
}

char const *ae_ds5_lightbar_mode_name(struct ae_ds5_settings const *settings)
{
	(void)settings;
	return "off";
}

int ae_ds5_on(void)
{
	return 0;
}

int ae_ds5_port_active(int port)
{
	(void)port;
	return 0;
}

void ae_ds5_wish_set(int port, struct ae_ds5_wish const *wish)
{
	(void)port;
	(void)wish;
}

void ae_ds5_port(int port, struct SDL_Gamepad *gamepad, unsigned short left_motor, unsigned short right_motor)
{
	(void)port;
	(void)gamepad;
	(void)left_motor;
	(void)right_motor;
}

void ae_ds5_frame(void)
{
}

void ae_ds5_removed(struct SDL_Gamepad *gamepad)
{
	(void)gamepad;
}

#endif
