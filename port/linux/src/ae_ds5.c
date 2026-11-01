/*
AE_DS5.C

Arena Evolved: a PS5 DualSense's adaptive triggers, lightbar and player lights (ae_ds5.h). The bytes are
ae_ds5_rules.c's; what each player wants is the game's (port/linux/game/ae_ds5_game.c).
*/

#include <string.h>

#include "platform.h"
#include "port_config.h"
#include "ae_config_players.h"
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
#include <linux/hidraw.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif
#ifdef _WIN32
/* Windows: the pad's own HID device, opened beside SDL's (Windows shares it), written through SDL's hidapi */
#define AE_DS5_HID
#include <SDL3/SDL_hidapi.h>
#include <SDL3/SDL_mutex.h>
#include <SDL3/SDL_thread.h>
#endif

/* the udev rule that lets the player at the seat write the pad's hidraw node (the Linux downloads'
70-ae-dualsense.rules, port/assets/udev/: tools/test_ae_ds5.py checks they agree) */
#define AE_DS5_UDEV_RULE \
	"KERNEL==\"hidraw*\", KERNELS==\"*:054C:0CE6.*|*:054C:0DF2.*\", MODE=\"0660\", TAG+=\"uaccess\""

#define PORTS 4
#define PADS 8
/* a port's wish this old (the player died, a menu, the game left) lets the triggers go */
#define WISH_STALE_MS 300
/* the triggers sent again though unchanged, while both are up: this long after the game's rumble changed (through
SDL's driver its own report; on the hidraw way kept, as it costs little: merged), and at least this often */
#define REFRESH_AFTER_RUMBLE_MS 40
#define REFRESH_EVERY_MS 1000
/* a trigger counts as up below this (SDL's axis, 0-32767) */
#define TRIGGER_UP 3000
/* the game's motors this old (it stopped setting them) are stopped (SDL's rumble lasts 100 ms a call) */
#define RUMBLE_STALE_MS 150
/* a node that went is looked for again this often */
#define REOPEN_MS 1000
/* (debug.ds5_log) the writes counted over this long */
#define MINUTE_MS 60000
/* (the HID way) the lightbar and player lights sent again this often, though unchanged: SDL's driver sets its own
once in a while (at a new player number; on Bluetooth as the pad's enhanced reports start, after the pad's own
connection animation) */
#define LIGHTS_REFRESH_MS 1000

enum backend
{
	BACKEND_UNKNOWN,
	BACKEND_NONE,   /* not a DualSense, or no way to write to it */
	BACKEND_SDL,    /* SDL's own PS5 driver */
	BACKEND_HIDRAW, /* Linux: the kernel's driver reads it, we write its hidraw node */
	BACKEND_HID     /* Windows: SDL's driver reads it, we write the same full reports to its HID device */
};

#ifdef AE_DS5_HID
/* the HID way's writer thread: SDL_hid_write waits for the pad (up to a second), so never on the game's thread. One
report waits while one is written; a report handed over while one still waits is refused (busy: the link keeps its
fields for the next window), never merged into or put in place of another */
struct hid_writer
{
	SDL_hid_device *device;
	SDL_Thread *thread;
	SDL_Mutex *lock;
	SDL_Condition *wake;
	unsigned char report[AE_DS5_BLUETOOTH_REPORT_SIZE];
	size_t size;
	int pending, quit, failed;
	Uint64 failed_ms;
	/* (debug.ds5_log) the slowest write so far this minute, ms, and the reports refused as one still waited */
	Uint64 slowest_ms;
	unsigned long busy;
};
#endif

struct pad
{
	SDL_JoystickID id;
	int used;
	int port;
	enum backend backend;
	int fd;
	int bluetooth;
	int product;
	int sequence;
	char node[32];
	/* ONE WRITER (ae_ds5_rules.h's link): what the pad has from us and what is wanted, by field; on Bluetooth one report
	every 50 ms at most */
	struct ae_ds5_link link;
	/* the hidraw way: the game's rumble goes in our reports (the kernel's own reports for SDL's rumble stopped) */
	int owns_rumble;
	/* the hidraw node went (the pad left: ENODEV, EIO): closed, and opened again when the pad is back */
	int gone;
	Uint64 reopen_at;
	int reopen_logged;
#ifdef AE_DS5_HID
	struct hid_writer *hid;
	/* (when the lightbar and player lights are sent again though unchanged) */
	Uint64 lights_refresh_at;
#endif
	/* the player's colour last asked for (SHIELD darkens it while there is no player) */
	int has_colour;
	unsigned char red, green, blue;
	int failed_logged;
	/* each trigger's kick under way (until kick_until, SDL_GetTicks ms) and the events seen; the rumble pulses seen */
	int events_known;
	unsigned long kicks_right, kicks_left, rumbles;
	Uint64 kick_until, kick_left_until;
	unsigned char kick[AE_DS5_EFFECT_SIZE], kick_left[AE_DS5_EFFECT_SIZE];
	/* the rumble seen, and when the triggers are next sent again though unchanged */
	int rumbling;
	unsigned short rumble_left, rumble_right;
	Uint64 refresh_at;
	Uint64 triggers_sent_at;
	/* (debug.ds5_log) */
	unsigned long kicks_sent, lightbar_sends, rumble_changes, refreshes;
	Uint64 minute_at;
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
	/* the motors on each port (XInputSetState: the game's, raised by an event pulse) and when they were set: SDL's
	rumble sends them (the kernel's own report when they change), or, where the pad's rumble is ours (owns_rumble:
	the hidraw way), our reports, and SDL is given zeros for that pad */
	unsigned short rumble_left[PORTS], rumble_right[PORTS];
	Uint64 rumble_ms[PORTS];
	SDL_JoystickID owns_rumble[PORTS];
	/* whether the port's pad is a DualSense this machine drives (the game works out a wish for it only then) */
	int active[PORTS];
	/* the event rumble pulse under way on each port (0-255 a motor, until pulse_until, SDL_GetTicks ms) */
	int pulse_left[PORTS], pulse_right[PORTS];
	Uint64 pulse_until[PORTS];
	/* (Bluetooth: a pulse not yet in a report written, and how long it lasts from that report: one report every 50 ms
	there, and a pulse that ended before its report went out was lost, owner 10-15 "very inconsistent") */
	int pulse_pending[PORTS];
	Uint64 pulse_ms[PORTS];
} shared;
static struct pad pads[PADS];
static int exit_registered;
/* the settings as last sent, each port's (for the change to OFF: the triggers let go, the lightbar given back once) */
static struct ae_ds5_settings applied[PORTS];
static int applied_known;

/* (read by the game's thread and the event thread: their own lock) */
static SDL_SpinLock settings_lock;

/* (0.3.7) a port's player's own settings (SETTINGS > CONTROLS > DUALSENSE are each player's: the port is the player's,
as the game's controller n is local player n) */
struct ae_ds5_settings ae_ds5_settings_port(int port)
{
	static struct ae_ds5_settings settings[PORTS], logged[PORTS];
	static unsigned long read_at[PORTS];
	static int read[PORTS];
	struct ae_ds5_settings result;
	int previous;

	if (port < 0 || port >= PORTS)
		port = 0;
	SDL_LockSpinlock(&settings_lock);
	previous = ae_config_player_enter(port);
	if (!read[port] || read_at[port] != ae_config_player_changes())
	{
		struct ae_ds5_settings *now = &settings[port];
		char const *strength = config_string("input.trigger_strength");
		char const *lightbar = config_string("input.lightbar");

		now->triggers = config_boolean("input.adaptive_triggers");
		now->strength = ae_ds5_strength_percent(strength ? strength : "");
		now->lights = !lightbar || strcmp(lightbar, "off");
		now->shield = lightbar && !strcmp(lightbar, "shield");
		now->log = config_boolean("debug.ds5_log");
		if (!read[port] || now->triggers != logged[port].triggers || now->lights != logged[port].lights ||
			now->shield != logged[port].shield || now->strength != logged[port].strength)
			platform_log("dualsense: adaptive triggers %s (strength %d%%), lightbar %s (a DualSense's only; player %d)",
				now->triggers ? "on" : "off", now->strength, ae_ds5_lightbar_mode_name(now), port + 1);
		logged[port] = *now;
		read[port] = 1;
		read_at[port] = ae_config_player_changes();
	}
	result = settings[port];
	ae_config_player_leave(previous);
	SDL_UnlockSpinlock(&settings_lock);
	return result;
}

struct ae_ds5_settings ae_ds5_settings_get(void)
{
	return ae_ds5_settings_port(0);
}

char const *ae_ds5_lightbar_mode_name(struct ae_ds5_settings const *settings)
{
	return !settings->lights ? "off" : settings->shield ? "shield (the shield meter)" :
		"player color (and the player lights)";
}

int ae_ds5_on(void)
{
	int port;

	/* (any player's: each port then follows its own player's) */
	for (port = 0; port < PORTS; port++)
	{
		struct ae_ds5_settings settings = ae_ds5_settings_port(port);

		if (settings.triggers || settings.lights)
			return 1;
	}
	return 0;
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

void ae_ds5_pulse(int port, int left, int right, int ms)
{
	if (port < 0 || port >= PORTS || ms <= 0)
		return;
	SDL_LockSpinlock(&shared_lock);
	shared.pulse_left[port] = left;
	shared.pulse_right[port] = right;
	shared.pulse_until[port] = SDL_GetTicks() + (Uint64)ms;
	shared.pulse_pending[port] = 0;
	shared.pulse_ms[port] = (Uint64)ms;
	SDL_UnlockSpinlock(&shared_lock);
}

void ae_ds5_port(int port, struct SDL_Gamepad *gamepad, unsigned short *left_motor, unsigned short *right_motor)
{
	if (port < 0 || port >= PORTS || !left_motor || !right_motor)
		return;
	Uint64 now = SDL_GetTicks();
	SDL_JoystickID id = gamepad ? SDL_GetGamepadID(gamepad) : 0;

	SDL_LockSpinlock(&shared_lock);
	/* (one pad, one port: a pad that moved here is no longer its old port's) */
	ae_ds5_bind(shared.bound, PORTS, port, id);
	/* (a DualSense's event pulse, or ae_ds5_pulse's on any pad) */
	if (now < shared.pulse_until[port])
	{
		unsigned int left = (unsigned int)shared.pulse_left[port] * 257u;
		unsigned int right = (unsigned int)shared.pulse_right[port] * 257u;

		if (left > *left_motor)
			*left_motor = (unsigned short)left;
		if (right > *right_motor)
			*right_motor = (unsigned short)right;
	}
	shared.rumble_left[port] = *left_motor;
	shared.rumble_right[port] = *right_motor;
	shared.rumble_ms[port] = now;
	/* (ONE WRITER: this pad's rumble is in our reports; SDL, given zeros, sends nothing more once they are zero) */
	if (id && shared.owns_rumble[port] == id)
		*left_motor = *right_motor = 0;
	SDL_UnlockSpinlock(&shared_lock);
}

#ifdef __linux__
/* the pad's hidraw node from its input event node (/dev/input/eventN -> /sys/class/input/eventN/device/device is
the HID device, whose hidraw/ folder names the node), whether it is on Bluetooth (HID_ID's bus 0005), and whether
it is a DualSense (054C:0CE6) or DualSense Edge (054C:0DF2) */
static int hidraw_find(char const *event_path, char *node, size_t node_size, int *bluetooth, int *product_id)
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
	*product_id = (int)product;
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

#ifdef __linux__
/* whether the pad's rumble takes the second compatible vibration (enable3 0x04), as the kernel's hid-playstation
decides it: the DualSense Edge always, a DualSense from firmware feature version 2.21 on (its firmware feature report
0x20, bytes 44-45). Unread: the first one, which every firmware takes */
static int hidraw_vibration_v2(int fd, int product, int *read)
{
	unsigned char report[64];

	*read = 1;
	if (product == 0x0df2)
		return 1;
	memset(report, 0, sizeof(report));
	report[0] = 0x20;
	if (ioctl(fd, HIDIOCGFEATURE(sizeof(report)), report) < 46)
	{
		*read = 0;
		return 0;
	}
	return ((int)report[44] | ((int)report[45] << 8)) >= 0x0215;
}

/* the hidraw way's link: the bus, the rumble's kind */
static void pad_hidraw_link(struct pad *pad, int *firmware_read)
{
	int v2 = hidraw_vibration_v2(pad->fd, pad->product, firmware_read);

	ae_ds5_link_init(&pad->link, pad->bluetooth, v2);
	pad->owns_rumble = 1;
}
#endif

#ifdef AE_DS5_HID
/* the writer thread: the report waiting is taken (the next one may wait meanwhile), written, and a failed write kept
for the game's thread to see; at quit the report still waiting is written first (the triggers let go at exit) */
static int SDLCALL hid_writer_main(void *data)
{
	struct hid_writer *writer = (struct hid_writer *)data;
	unsigned char report[AE_DS5_BLUETOOTH_REPORT_SIZE];

	SDL_LockMutex(writer->lock);
	for (;;)
	{
		size_t size;
		Uint64 started, took;
		int written;

		while (!writer->pending && !writer->quit)
			SDL_WaitCondition(writer->wake, writer->lock);
		if (!writer->pending)
			break;
		size = writer->size;
		memcpy(report, writer->report, size);
		writer->pending = 0;
		SDL_UnlockMutex(writer->lock);
		started = SDL_GetTicks();
		written = SDL_hid_write(writer->device, report, size);
		took = SDL_GetTicks() - started;
		SDL_LockMutex(writer->lock);
		if (took > writer->slowest_ms)
			writer->slowest_ms = took;
		if (written < 0 && !writer->failed)
		{
			writer->failed = 1;
			writer->failed_ms = took;
		}
	}
	SDL_UnlockMutex(writer->lock);
	return 0;
}

/* the writer stopped (after its last report) and the HID device closed */
static void hid_close(struct pad *pad)
{
	struct hid_writer *writer = pad->hid;

	if (!writer)
		return;
	pad->hid = NULL;
	SDL_LockMutex(writer->lock);
	writer->quit = 1;
	SDL_SignalCondition(writer->wake);
	SDL_UnlockMutex(writer->lock);
	SDL_WaitThread(writer->thread, NULL);
	SDL_hid_close(writer->device);
	SDL_DestroyCondition(writer->wake);
	SDL_DestroyMutex(writer->lock);
	SDL_free(writer);
	SDL_hid_exit();
}

/* a DualSense (054C:0CE6) or DualSense Edge (054C:0DF2) */
static int hid_is_dualsense(SDL_hid_device_info const *info)
{
	return info && info->vendor_id == 0x054c && (info->product_id == 0x0ce6 || info->product_id == 0x0df2);
}

static int hid_bluetooth(SDL_hid_device_info const *info, int wireless)
{
	return info->bus_type == SDL_HID_API_BUS_BLUETOOTH || (info->bus_type == SDL_HID_API_BUS_UNKNOWN && wireless);
}

/* the pad's HID device, opened beside SDL's: SDL's own path to it (SDL's driver reads the pad through hidapi), else
the DualSense hidapi lists when it lists one only; its bus and product. NULL, with why, when there is none to write */
static SDL_hid_device *hid_find(SDL_Gamepad *gamepad, int *bluetooth, int *product, char *why, size_t why_size)
{
	char const *path = SDL_GetGamepadPath(gamepad);
	int wireless = SDL_GetGamepadConnectionState(gamepad) == SDL_JOYSTICK_CONNECTION_WIRELESS;
	SDL_hid_device *device = path && *path ? SDL_hid_open_path(path) : NULL;
	SDL_hid_device_info *list, *entry, *only = NULL;
	int count = 0;

	if (device)
	{
		SDL_hid_device_info *info = SDL_hid_get_device_info(device);

		if (hid_is_dualsense(info))
		{
			*bluetooth = hid_bluetooth(info, wireless);
			*product = info->product_id;
			return device;
		}
		SDL_hid_close(device);
		device = NULL;
	}
	list = SDL_hid_enumerate(0x054c, 0);
	for (entry = list; entry; entry = entry->next)
	{
		if (hid_is_dualsense(entry) && entry->path)
		{
			only = entry;
			count++;
		}
	}
	if (count == 1)
	{
		device = SDL_hid_open_path(only->path);
		if (device)
		{
			*bluetooth = hid_bluetooth(only, wireless);
			*product = only->product_id;
		}
		else
			SDL_snprintf(why, why_size, "SDL's path %s and hidapi's %s could not be opened", path ? path : "(none)",
				only->path);
	}
	else
		SDL_snprintf(why, why_size, "SDL's path %s could not be opened, and hidapi lists %d DualSenses",
			path ? path : "(none)", count);
	SDL_hid_free_enumeration(list);
	return device;
}

/* the rumble's kind, as hidraw_vibration_v2 decides it (the firmware feature report 0x20, bytes 44-45) */
static int hid_vibration_v2(SDL_hid_device *device, int product, int *read)
{
	unsigned char report[64];

	*read = 1;
	if (product == 0x0df2)
		return 1;
	memset(report, 0, sizeof(report));
	report[0] = 0x20;
	if (SDL_hid_get_feature_report(device, report, sizeof(report)) < 46)
	{
		*read = 0;
		return 0;
	}
	return ((int)report[44] | ((int)report[45] << 8)) >= 0x0215;
}

/* the pad's HID device opened and its writer thread started (the bus, the product and the rumble's kind found);
0, with why, when not */
static int pad_hid_open(struct pad *pad, SDL_Gamepad *gamepad, int *v2, int *firmware_read, char *why,
	size_t why_size)
{
	struct hid_writer *writer;
	SDL_hid_device *device;
	int bluetooth = 0, product = 0;

	if (SDL_hid_init() < 0)
	{
		SDL_snprintf(why, why_size, "SDL_hid_init failed: %s", SDL_GetError());
		return 0;
	}
	device = hid_find(gamepad, &bluetooth, &product, why, why_size);
	if (!device)
	{
		SDL_hid_exit();
		return 0;
	}
	/* (read before the writer thread has the device) */
	*v2 = hid_vibration_v2(device, product, firmware_read);
	writer = (struct hid_writer *)SDL_calloc(1, sizeof(*writer));
	if (writer)
	{
		writer->device = device;
		writer->lock = SDL_CreateMutex();
		writer->wake = SDL_CreateCondition();
		if (writer->lock && writer->wake)
			writer->thread = SDL_CreateThread(hid_writer_main, "ae_ds5_hid", writer);
	}
	if (!writer || !writer->thread)
	{
		SDL_snprintf(why, why_size, "no writer thread: %s", SDL_GetError());
		if (writer)
		{
			if (writer->wake)
				SDL_DestroyCondition(writer->wake);
			if (writer->lock)
				SDL_DestroyMutex(writer->lock);
			SDL_free(writer);
		}
		SDL_hid_close(device);
		SDL_hid_exit();
		return 0;
	}
	pad->hid = writer;
	pad->bluetooth = bluetooth;
	pad->product = product;
	pad->sequence = 0;
	pad->gone = 0;
	pad->failed_logged = 0;
	pad->lights_refresh_at = 0;
	pad->owns_rumble = 1;
	return 1;
}

/* the pad's HID device again (a failed write closed it); everything is sent again */
static void pad_hid_reopen(struct pad *pad, SDL_Gamepad *gamepad, Uint64 now)
{
	char why[512];
	int v2 = 0, firmware_read = 0;

	pad->reopen_at = now + REOPEN_MS;
	why[0] = 0;
	if (!pad_hid_open(pad, gamepad, &v2, &firmware_read, why, sizeof(why)))
	{
		if (!pad->reopen_logged)
			platform_log("dualsense: port %d: waiting for the pad's HID device to come back (tried every %d ms): %s",
				pad->port + 1, REOPEN_MS, why);
		pad->reopen_logged = 1;
		return;
	}
	pad->link.bluetooth = pad->bluetooth;
	pad->link.vibration_v2 = v2;
	ae_ds5_link_forget(&pad->link);
	platform_log("dualsense: port %d: the pad's HID device opened again (%s): everything sent again", pad->port + 1,
		pad->bluetooth ? "Bluetooth" : "USB");
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
	pad->minute_at = SDL_GetTicks() + MINUTE_MS;
	ae_ds5_link_init(&pad->link, 0, 0);
	if (type != SDL_GAMEPAD_TYPE_PS5)
	{
		if (log)
			platform_log("dualsense: %s on port %d is not a DualSense: left alone", name, pad->port + 1);
		return;
	}
	if (properties && SDL_GetBooleanProperty(properties, SDL_PROP_GAMEPAD_CAP_RGB_LED_BOOLEAN, false))
	{
		int wireless = SDL_GetGamepadConnectionState(gamepad) == SDL_JOYSTICK_CONNECTION_WIRELESS;

#ifdef AE_DS5_HID
		{
			char why[512];
			int v2 = 0, firmware_read = 0;

			why[0] = 0;
			if (pad_hid_open(pad, gamepad, &v2, &firmware_read, why, sizeof(why)))
			{
				char const *path = SDL_GetGamepadPath(gamepad);

				pad->backend = BACKEND_HID;
				ae_ds5_link_init(&pad->link, pad->bluetooth, v2);
				platform_log("dualsense: %s on port %d: backend hid: input through SDL's PS5 driver, effects written "
					"straight to the pad's HID device (%s, report 0x%02x, the Linux way's whole report); adaptive triggers "
					"%s, lightbar %s", name, pad->port + 1, pad->bluetooth ? "Bluetooth" : "USB",
					pad->bluetooth ? 0x31 : 0x02, settings->triggers ? "on" : "off", ae_ds5_lightbar_mode_name(settings));
				platform_log("dualsense: port %d: one writer: the game's rumble goes in the same reports (compatible "
					"vibration %s%s), not through SDL; no SDL effect, LED or rumble calls for this pad; %s", pad->port + 1,
					v2 ? "2" : "1", firmware_read ? "" : ", the firmware's version unread", pad->bluetooth ?
					"Bluetooth: one report every 50 ms at most, changes in between merged" :
					"USB: one report a frame at most");
				if (log)
					platform_log("dualsense: port %d: HID path %s", pad->port + 1, path ? path : "(none from SDL)");
				return;
			}
			platform_log("dualsense: %s on port %d: the pad's HID device cannot be written (%s): through SDL's own PS5 "
				"driver instead", name, pad->port + 1, why);
		}
#endif
		pad->backend = BACKEND_SDL;
		ae_ds5_link_init(&pad->link, wireless, 0);
		platform_log("dualsense: %s on port %d: backend sdl: through SDL's own PS5 driver (effects with SDL_SendGamepadEffect%s); "
			"adaptive triggers %s, lightbar %s", name, pad->port + 1,
			wireless ? "; wireless: one report every 50 ms at most" : "", settings->triggers ? "on" : "off",
			ae_ds5_lightbar_mode_name(settings));
		return;
	}
#ifdef __linux__
	{
		char const *path = SDL_GetGamepadPath(gamepad);
		int firmware_read;

		if (!path || !hidraw_find(path, pad->node, sizeof(pad->node), &pad->bluetooth, &pad->product))
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
		pad_hidraw_link(pad, &firmware_read);
		platform_log("dualsense: %s on port %d: backend hidraw: input through the kernel's driver (%s), effects written to %s (%s); "
			"adaptive triggers %s, lightbar %s", name, pad->port + 1, path, pad->node, pad->bluetooth ? "Bluetooth" : "USB",
			settings->triggers ? "on" : "off", ae_ds5_lightbar_mode_name(settings));
		platform_log("dualsense: port %d: one writer: the game's rumble goes in the same reports (compatible vibration "
			"%s%s), not through SDL and the kernel; %s", pad->port + 1, pad->link.vibration_v2 ? "2" : "1",
			firmware_read ? "" : ", the firmware's version unread", pad->bluetooth ?
			"Bluetooth: one report every 50 ms at most, changes in between merged" : "USB: one report a frame at most");
	}
#else
	platform_log("dualsense: %s on port %d: SDL's driver does not drive it: no adaptive triggers or lightbar", name,
		pad->port + 1);
#endif
}

/* sends the body (only the enable bits set in it count) */
static enum ae_ds5_write_result pad_send(struct pad *pad, SDL_Gamepad *gamepad,
	unsigned char const body[AE_DS5_BODY_SIZE])
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
		return ok ? AE_DS5_WRITE_SENT : AE_DS5_WRITE_REFUSED;
	}
#ifdef __linux__
	if (pad->backend == BACKEND_HIDRAW)
	{
		unsigned char report[AE_DS5_BLUETOOTH_REPORT_SIZE];
		size_t size = ae_ds5_output_report(report, sizeof(report), pad->bluetooth, pad->sequence, body);
		enum ae_ds5_write_result result;
		ssize_t written;
		int error;

		if (pad->fd < 0)
			return AE_DS5_WRITE_GONE;
		written = write(pad->fd, report, size);
		error = errno;
		/* (the next sequence number only once this report went: a busy write is sent again with the same one) */
		if (written == (ssize_t)size)
		{
			pad->sequence = (pad->sequence + 1) & 0x0f;
			return AE_DS5_WRITE_SENT;
		}
		/* (a short write: tried again as the next window opens) */
		result = written < 0 ? ae_ds5_write_result_of_errno(error) : AE_DS5_WRITE_BUSY;
		if (result == AE_DS5_WRITE_GONE)
			platform_log("dualsense: port %d: writing %s failed (%s): the pad went; its node is opened again when it "
				"is back, and everything sent again", pad->port + 1, pad->node, strerror(error));
		else if (result == AE_DS5_WRITE_REFUSED)
			platform_log("dualsense: port %d: writing %s failed (%s): the pad is left alone", pad->port + 1, pad->node,
				strerror(error));
		else if (!pad->failed_logged)
		{
			platform_log("dualsense: port %d: writing %s: %s: kept and tried again in the next window (said once)",
				pad->port + 1, pad->node, written < 0 ? strerror(error) : "a short write");
			pad->failed_logged = 1;
		}
		return result;
	}
#endif
#ifdef AE_DS5_HID
	if (pad->backend == BACKEND_HID)
	{
		unsigned char report[AE_DS5_BLUETOOTH_REPORT_SIZE];
		size_t size = ae_ds5_output_report(report, sizeof(report), pad->bluetooth, pad->sequence, body);
		struct hid_writer *writer = pad->hid;
		enum ae_ds5_write_result result = AE_DS5_WRITE_SENT;
		Uint64 failed_ms = 0;

		if (!writer || !size)
			return AE_DS5_WRITE_GONE;
		SDL_LockMutex(writer->lock);
		if (writer->failed)
		{
			result = AE_DS5_WRITE_GONE;
			failed_ms = writer->failed_ms;
		}
		else if (writer->pending)
		{
			/* (one still waits: kept for the next window, nothing merged away) */
			result = AE_DS5_WRITE_BUSY;
			writer->busy++;
		}
		else
		{
			memcpy(writer->report, report, size);
			writer->size = size;
			writer->pending = 1;
			SDL_SignalCondition(writer->wake);
		}
		SDL_UnlockMutex(writer->lock);
		if (result == AE_DS5_WRITE_SENT)
			pad->sequence = (pad->sequence + 1) & 0x0f;
		else if (result == AE_DS5_WRITE_GONE)
			platform_log("dualsense: port %d: a write to the pad's HID device failed (after %u ms): the pad went; it is "
				"opened again when it is back, and everything sent again", pad->port + 1, (unsigned int)failed_ms);
		return result;
	}
#endif
	(void)gamepad;
	return AE_DS5_WRITE_REFUSED;
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
#ifdef AE_DS5_HID
	hid_close(pad);
#endif
	memset(pad, 0, sizeof(*pad));
	pad->fd = -1;
}

/* the hidraw node went (ENODEV, EIO), or a write to the HID device failed: closed; opened again when the pad is back */
static void pad_lose(struct pad *pad, Uint64 now)
{
#ifdef __linux__
	if (pad->fd >= 0)
		close(pad->fd);
#endif
#ifdef AE_DS5_HID
	hid_close(pad);
#endif
	pad->fd = -1;
	pad->gone = 1;
	pad->reopen_at = now + REOPEN_MS;
	pad->reopen_logged = 0;
}

#ifdef __linux__
/* the pad's node again (its path may have changed: a new hidraw number, or the other bus); everything is sent again */
static void pad_reopen(struct pad *pad, SDL_Gamepad *gamepad, Uint64 now)
{
	char const *path = SDL_GetGamepadPath(gamepad);
	char node[sizeof(pad->node)];
	int bluetooth = 0, product = 0, firmware_read, fd = -1;

	pad->reopen_at = now + REOPEN_MS;
	if (path && hidraw_find(path, node, sizeof(node), &bluetooth, &product))
		fd = open(node, O_WRONLY | O_CLOEXEC | O_NONBLOCK);
	if (fd < 0)
	{
		if (!pad->reopen_logged)
			platform_log("dualsense: port %d: waiting for the pad's node to come back (tried every %d ms)",
				pad->port + 1, REOPEN_MS);
		pad->reopen_logged = 1;
		return;
	}
	pad->fd = fd;
	memcpy(pad->node, node, sizeof(node));
	pad->bluetooth = bluetooth;
	pad->product = product;
	pad->gone = 0;
	pad->failed_logged = 0;
	pad->sequence = 0;
	{
		int v2 = hidraw_vibration_v2(fd, product, &firmware_read);

		pad->link.bluetooth = bluetooth;
		pad->link.vibration_v2 = v2;
		ae_ds5_link_forget(&pad->link);
	}
	pad->owns_rumble = 1;
	platform_log("dualsense: port %d: %s opened again (%s): everything sent again", pad->port + 1, pad->node,
		bluetooth ? "Bluetooth" : "USB");
}
#endif

/* one write if one is due (immediate: the window ignored); the fields written, 0 for none */
static int pad_write(struct pad *pad, SDL_Gamepad *gamepad, Uint64 now, int immediate,
	unsigned char body[AE_DS5_BODY_SIZE])
{
	enum ae_ds5_write_result result;
	int fields;

	if (pad->gone || pad->backend == BACKEND_NONE || pad->backend == BACKEND_UNKNOWN)
		return 0;
	fields = immediate ? ae_ds5_link_due_now(&pad->link, now, body) : ae_ds5_link_due(&pad->link, now, body);
	if (!fields)
		return 0;
	result = pad_send(pad, gamepad, body);
	ae_ds5_link_result(&pad->link, fields, result, now);
	if (result == AE_DS5_WRITE_SENT)
		return fields;
	if (result == AE_DS5_WRITE_GONE)
		pad_lose(pad, now);
	else if (result == AE_DS5_WRITE_REFUSED)
	{
		/* (a pad that refuses is left alone until it comes back) */
		pad->backend = BACKEND_NONE;
		pad->owns_rumble = 0;
	}
	return 0;
}

/* (debug.ds5_log) a minute's writes, merged changes, waits and failed writes; final: the part of a minute before the
pad left or the game closed */
static void pad_minute(struct pad *pad, Uint64 now, int final, int log)
{
	struct ae_ds5_link *link = &pad->link;

	if (!final && now < pad->minute_at)
		return;
	if (log && pad->backend != BACKEND_UNKNOWN && (link->writes || link->merged || link->deferred || link->errors))
		platform_log("dualsense: port %d: writes %lu, merged %lu, deferred %lu, errors %lu (%s%s)", pad->port + 1,
			link->writes, link->merged, link->deferred, link->errors, link->bluetooth ? "Bluetooth" : "USB",
			final ? ", the last part of a minute" : ", a minute");
#ifdef AE_DS5_HID
	/* (the HID way: the writer thread's slowest write and the reports refused as one still waited) */
	if (pad->hid)
	{
		Uint64 slowest;
		unsigned long busy;

		SDL_LockMutex(pad->hid->lock);
		slowest = pad->hid->slowest_ms;
		busy = pad->hid->busy;
		pad->hid->slowest_ms = 0;
		pad->hid->busy = 0;
		SDL_UnlockMutex(pad->hid->lock);
		if (log && link->writes)
			platform_log("dualsense: port %d: HID writer: slowest write %u ms, %lu reports kept back as one still waited",
				pad->port + 1, (unsigned int)slowest, busy);
	}
#endif
	link->writes = link->merged = link->deferred = link->errors = 0;
	pad->minute_at = now + MINUTE_MS;
}

/* these fields given back, at once (the window ignored), where the pad has them from us: the triggers let go, the
rumble stopped (where it is ours), the lightbar the kernel's own blue (hid-playstation's default; the hidraw and HID
ways only: SDL's driver keeps the last colour set) */
static void pad_reset(struct pad *pad, SDL_Gamepad *gamepad, int fields, Uint64 now)
{
	unsigned char body[AE_DS5_BODY_SIZE];
	unsigned char off[AE_DS5_EFFECT_SIZE];
	int wanted, forced, sent;

	fields &= pad->link.known & (AE_DS5_FIELD_TRIGGERS | AE_DS5_FIELD_RUMBLE | AE_DS5_FIELD_LIGHTBAR);
	if (pad->backend != BACKEND_HIDRAW && pad->backend != BACKEND_HID)
		fields &= ~(AE_DS5_FIELD_RUMBLE | AE_DS5_FIELD_LIGHTBAR);
	if (!fields)
		return;
	wanted = pad->link.wanted;
	forced = pad->link.forced;
	if (fields & AE_DS5_FIELD_TRIGGERS)
	{
		ae_ds5_effect_off(off);
		ae_ds5_link_want_triggers(&pad->link, off, off);
	}
	if (fields & AE_DS5_FIELD_RUMBLE)
		ae_ds5_link_want_rumble(&pad->link, 0, 0);
	if (fields & AE_DS5_FIELD_LIGHTBAR)
		ae_ds5_link_want_lightbar(&pad->link, 0, 0, 128);
	/* (only these fields, sent though the pad may have them already) */
	pad->link.wanted = fields;
	pad->link.forced = fields;
	sent = pad_write(pad, gamepad, now, 1, body);
	pad->link.wanted = wanted & ~fields;
	pad->link.forced = forced & ~fields;
	/* (driven again later: sent again then) */
	pad->link.known &= ~fields;
	if (sent)
		platform_log("dualsense: port %d reset:%s%s%s", pad->port + 1,
			(sent & AE_DS5_FIELD_TRIGGERS) ? " triggers off" : "", (sent & AE_DS5_FIELD_RUMBLE) ? " rumble off" : "",
			(sent & AE_DS5_FIELD_LIGHTBAR) ? " lightbar blue" : "");
}

static void ds5_exit(void)
{
	Uint64 now = SDL_GetTicks();
	int i;

	for (i = 0; i < PADS; i++)
	{
		if (pads[i].used)
		{
			pad_reset(&pads[i], SDL_GetGamepadFromID(pads[i].id), AE_DS5_FIELDS, now);
			/* (the settings as last applied: the config is not read at exit) */
			pad_minute(&pads[i], now, 1, applied[pads[i].port >= 0 && pads[i].port < PORTS ? pads[i].port : 0].log);
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

/* both triggers up (SDL's axes) */
static int triggers_up(SDL_Gamepad *gamepad)
{
	return SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) < TRIGGER_UP &&
		SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) < TRIGGER_UP;
}

/* one pad on its port this frame: what is wanted, then at most one report (ONE WRITER) */
static void pad_frame(struct pad *pad, SDL_Gamepad *gamepad, int port, struct ae_ds5_wish const *wish, int fresh,
	unsigned short rumble_left, unsigned short rumble_right, struct ae_ds5_settings const *settings, Uint64 now)
{
	unsigned char body[AE_DS5_BODY_SIZE];
	unsigned char right[AE_DS5_EFFECT_SIZE], left[AE_DS5_EFFECT_SIZE];
	int refresh = 0, kicking = 0, written;

	/* the rumble changed: where it is the kernel's (SDL's rumble), its own report, so the trigger effects are sent
	again once both triggers are up, in case the pad dropped them (kept where the rumble is ours: cheap, merged) */
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
	/* (ONE WRITER: the motors in our report, as the kernel scales them: a 16-bit speed / 256) */
	if (pad->owns_rumble)
		ae_ds5_link_want_rumble(&pad->link, rumble_left >> 8, rumble_right >> 8);
	else
		ae_ds5_link_drop(&pad->link, AE_DS5_FIELD_RUMBLE);

	if (settings->triggers)
	{
		int unchanged;

		if (fresh)
		{
			/* an event: its kick now, the held shape again after its time; its rumble pulse into the game's */
			if (pad->events_known && wish->kicks_right != pad->kicks_right && wish->kick_right_ms > 0)
			{
				pad->kick_until = now + (Uint64)wish->kick_right_ms;
				memcpy(pad->kick, wish->kick_right, sizeof(pad->kick));
			}
			if (pad->events_known && wish->kicks_left != pad->kicks_left && wish->kick_left_ms > 0)
			{
				pad->kick_left_until = now + (Uint64)wish->kick_left_ms;
				memcpy(pad->kick_left, wish->kick_left, sizeof(pad->kick_left));
			}
			if (pad->events_known && wish->rumbles != pad->rumbles && wish->rumble.ms > 0)
			{
				SDL_LockSpinlock(&shared_lock);
				shared.pulse_left[port] = wish->rumble.left;
				shared.pulse_right[port] = wish->rumble.right;
				shared.pulse_until[port] = now + (Uint64)wish->rumble.ms;
				/* (Bluetooth: kept on until a report carries it, then its full length from that report) */
				shared.pulse_pending[port] = pad->bluetooth;
				shared.pulse_ms[port] = (Uint64)wish->rumble.ms;
				if (pad->bluetooth)
					shared.pulse_until[port] += 2 * AE_DS5_BLUETOOTH_INTERVAL_MS;
				SDL_UnlockSpinlock(&shared_lock);
				/* (the rumble reports for it: the triggers sent again after, once they are up) */
				pad->refresh_at = now + (Uint64)wish->rumble.ms + REFRESH_AFTER_RUMBLE_MS;
			}
			pad->kicks_right = wish->kicks_right;
			pad->kicks_left = wish->kicks_left;
			pad->rumbles = wish->rumbles;
			pad->events_known = 1;
			kicking = now < pad->kick_until || now < pad->kick_left_until;
			memcpy(right, now < pad->kick_until ? pad->kick : wish->right, sizeof(right));
			memcpy(left, now < pad->kick_left_until ? pad->kick_left : wish->left, sizeof(left));
		}
		else
		{
			pad->kick_until = pad->kick_left_until = 0;
			pad->events_known = 0;
			ae_ds5_effect_off(right);
			ae_ds5_effect_off(left);
		}
		ae_ds5_link_want_triggers(&pad->link, right, left);
		unchanged = (pad->link.known & AE_DS5_FIELD_TRIGGERS) &&
			!(ae_ds5_link_dirty(&pad->link) & AE_DS5_FIELD_TRIGGERS);
		/* (a refresh waiting for the Bluetooth window is let go if a trigger is pulled meanwhile) */
		if ((pad->link.forced & AE_DS5_FIELD_TRIGGERS) && !triggers_up(gamepad))
			pad->link.forced &= ~AE_DS5_FIELD_TRIGGERS;
		if (unchanged && fresh && !kicking && !(pad->link.forced & AE_DS5_FIELD_TRIGGERS) &&
			((pad->refresh_at && now >= pad->refresh_at) || now - pad->triggers_sent_at >= REFRESH_EVERY_MS) &&
			triggers_up(gamepad))
		{
			ae_ds5_link_force(&pad->link, AE_DS5_FIELD_TRIGGERS);
			refresh = 1;
			pad->refresh_at = 0;
			pad->refreshes++;
		}
	}
	else
		ae_ds5_link_drop(&pad->link, AE_DS5_FIELD_TRIGGERS);

	if (settings->lights)
	{
		if (fresh)
		{
			ae_ds5_link_want_lightbar(&pad->link, wish->red, wish->green, wish->blue);
			pad->has_colour = 1;
			pad->red = wish->red;
			pad->green = wish->green;
			pad->blue = wish->blue;
			if (wish->player_lights)
				ae_ds5_link_want_player_lights(&pad->link, wish->player_lights);
		}
		/* (SHIELD: no player (dead, a menu): the bar goes dark, its last colour at 6 %, until the player is back) */
		else if (settings->shield && pad->has_colour)
			ae_ds5_link_want_lightbar(&pad->link, pad->red * 6 / 100, pad->green * 6 / 100, pad->blue * 6 / 100);
	}
	else
		ae_ds5_link_drop(&pad->link, AE_DS5_FIELD_LIGHTBAR | AE_DS5_FIELD_PLAYER_LIGHTS);
#ifdef AE_DS5_HID
	/* (SDL's driver sets its own lights now and then: ours sent again, though unchanged, once a second) */
	if (pad->backend == BACKEND_HID && settings->lights && now >= pad->lights_refresh_at)
	{
		int lights = pad->link.known & (AE_DS5_FIELD_LIGHTBAR | AE_DS5_FIELD_PLAYER_LIGHTS);

		if (lights)
			ae_ds5_link_force(&pad->link, lights);
		pad->lights_refresh_at = now + LIGHTS_REFRESH_MS;
	}
#endif

	written = pad_write(pad, gamepad, now, 0, body);
	if (!written)
		return;
	if (written & AE_DS5_FIELD_TRIGGERS)
		pad->triggers_sent_at = now;
	/* (a Bluetooth pulse now in a report: its length counts from here) */
	if (written & AE_DS5_FIELD_RUMBLE)
	{
		SDL_LockSpinlock(&shared_lock);
		if (shared.pulse_pending[port] && (body[AE_DS5_BODY_RUMBLE_LEFT] || body[AE_DS5_BODY_RUMBLE_RIGHT]))
		{
			shared.pulse_pending[port] = 0;
			shared.pulse_until[port] = now + shared.pulse_ms[port];
		}
		SDL_UnlockSpinlock(&shared_lock);
	}
	if (written & AE_DS5_FIELD_LIGHTBAR)
		pad->lightbar_sends++;
	if (kicking && (written & AE_DS5_FIELD_TRIGGERS))
		pad->kicks_sent++;
	if (!settings->log)
		return;

	/* (debug.ds5_log: the bytes of the first sends and of every change of what the player wants; kicks, refreshes and
	lightbar steps counted, not logged one by one: a battle rifle burst is six reports) */
	if (pad->bytes_logged < 12 || (fresh && !kicking && !refresh && strcmp(pad->logged, wish->what)))
	{
		char right_hex[40], left_hex[40];

		pad->bytes_logged++;
		hex(right_hex, sizeof(right_hex), body + AE_DS5_BODY_RIGHT_TRIGGER);
		hex(left_hex, sizeof(left_hex), body + AE_DS5_BODY_LEFT_TRIGGER);
		platform_log("dualsense: port %d wrote%s%s%s%s%s: enable %02x %02x %02x, R2 [%s], L2 [%s], rumble %u %u, "
			"player lights %02x%s", port + 1, (written & AE_DS5_FIELD_TRIGGERS) ? " triggers" : "",
			(written & AE_DS5_FIELD_RUMBLE) ? " rumble" : "", (written & AE_DS5_FIELD_LIGHTBAR) ? " lightbar" : "",
			(written & AE_DS5_FIELD_PLAYER_LIGHTS) ? " player lights" : "",
			refresh ? " (refresh)" : kicking ? " (kick)" : "", body[AE_DS5_BODY_ENABLE1], body[AE_DS5_BODY_ENABLE2],
			body[AE_DS5_BODY_ENABLE3], (written & AE_DS5_FIELD_TRIGGERS) ? right_hex : "-",
			(written & AE_DS5_FIELD_TRIGGERS) ? left_hex : "-", (unsigned int)body[AE_DS5_BODY_RUMBLE_LEFT],
			(unsigned int)body[AE_DS5_BODY_RUMBLE_RIGHT], body[AE_DS5_BODY_PLAYER_LIGHTS],
			pad->backend == BACKEND_SDL ? " through SDL" : pad->backend == BACKEND_HID ? " to the HID device" : "");
	}
	if (fresh && !kicking && !refresh && strcmp(pad->logged, wish->what))
	{
		strcpy(pad->logged, wish->what);
		platform_log("dualsense: port %d: %s; lightbar %d %d %d; %u kicks, %u refreshes, %u lightbar steps, "
			"%u rumble changes so far", port + 1, wish->what, wish->red, wish->green, wish->blue,
			(unsigned int)pad->kicks_sent, (unsigned int)pad->refreshes, (unsigned int)pad->lightbar_sends,
			(unsigned int)pad->rumble_changes);
	}
	else if (!fresh && (written & AE_DS5_FIELD_TRIGGERS))
	{
		pad->logged[0] = 0;
		platform_log("dualsense: port %d: no player (menu, dead or no game): triggers off", port + 1);
	}
}

void ae_ds5_frame(void)
{
	struct ae_ds5_settings port_settings[PORTS];
	int any_on = 0;
	SDL_JoystickID bound[PORTS];
	struct ae_ds5_wish wish[PORTS];
	int fresh[PORTS];
	unsigned short rumble_left[PORTS], rumble_right[PORTS];
	Uint64 now = SDL_GetTicks();
	int port, i;

	/* a setting turned off (each port's player's own): that port's pad's triggers let go and the lightbar given back,
	once; both off, the rumble (ours on the hidraw way) stopped too: SDL's rumble is the pad's again */
	for (port = 0; port < PORTS; port++)
	{
		port_settings[port] = ae_ds5_settings_port(port);
		any_on |= port_settings[port].triggers || port_settings[port].lights;
	}
	for (i = 0; i < PADS; i++)
	{
		struct ae_ds5_settings const *was, *settings;
		int fields = 0;

		if (!applied_known || !pads[i].used || pads[i].port < 0 || pads[i].port >= PORTS)
			continue;
		was = &applied[pads[i].port];
		settings = &port_settings[pads[i].port];
		if (was->triggers && !settings->triggers)
			fields |= AE_DS5_FIELD_TRIGGERS;
		if (was->lights && !settings->lights)
			fields |= AE_DS5_FIELD_LIGHTBAR;
		if (fields && !settings->triggers && !settings->lights)
			fields |= AE_DS5_FIELD_RUMBLE;
		if (fields)
			pad_reset(&pads[i], SDL_GetGamepadFromID(pads[i].id), fields, now);
	}
	memcpy(applied, port_settings, sizeof(applied));
	applied_known = 1;
	if (!any_on)
	{
		SDL_LockSpinlock(&shared_lock);
		memset(shared.active, 0, sizeof(shared.active));
		memset(shared.owns_rumble, 0, sizeof(shared.owns_rumble));
		SDL_UnlockSpinlock(&shared_lock);
		return;
	}
	if (!exit_registered)
	{
		atexit(ds5_exit);
		exit_registered = 1;
	}
	SDL_LockSpinlock(&shared_lock);
	for (port = 0; port < PORTS; port++)
	{
		int rumble_fresh = now - shared.rumble_ms[port] < RUMBLE_STALE_MS;

		bound[port] = shared.bound[port];
		wish[port] = shared.wish[port];
		fresh[port] = shared.wished[port] && now - shared.wish_ms[port] < WISH_STALE_MS;
		/* (the game no longer setting the motors: stopped, as SDL's rumble would after its 100 ms) */
		rumble_left[port] = rumble_fresh ? shared.rumble_left[port] : 0;
		rumble_right[port] = rumble_fresh ? shared.rumble_right[port] : 0;
	}
	SDL_UnlockSpinlock(&shared_lock);

	/* a pad no longer bound to a port lets its triggers go (and its rumble, where it is ours) */
	for (i = 0; i < PADS; i++)
	{
		int still = 0;

		if (!pads[i].used)
			continue;
		for (port = 0; port < PORTS; port++)
			still |= bound[port] == pads[i].id && pads[i].port == port;
		if (!still && (pads[i].link.known & (AE_DS5_FIELD_TRIGGERS | AE_DS5_FIELD_RUMBLE)))
			pad_reset(&pads[i], SDL_GetGamepadFromID(pads[i].id), AE_DS5_FIELD_TRIGGERS | AE_DS5_FIELD_RUMBLE, now);
	}

	for (port = 0; port < PORTS; port++)
	{
		SDL_Gamepad *gamepad = bound[port] ? SDL_GetGamepadFromID(bound[port]) : NULL;
		struct pad *pad = gamepad ? pad_for(bound[port], port) : NULL;
		SDL_JoystickID owns = 0;
		int active;

		if (pad && pad->backend == BACKEND_UNKNOWN)
			pad_choose_backend(pad, gamepad, &port_settings[port]);
#ifdef __linux__
		if (pad && pad->backend == BACKEND_HIDRAW && pad->gone && now >= pad->reopen_at)
			pad_reopen(pad, gamepad, now);
#endif
#ifdef AE_DS5_HID
		if (pad && pad->backend == BACKEND_HID && pad->gone && now >= pad->reopen_at)
			pad_hid_reopen(pad, gamepad, now);
#endif
		active = pad && pad->backend != BACKEND_NONE;
		if (active && (pad->backend == BACKEND_HIDRAW || pad->backend == BACKEND_HID) && !pad->gone &&
			pad->owns_rumble)
			owns = bound[port];
		SDL_LockSpinlock(&shared_lock);
		shared.active[port] = active;
		shared.owns_rumble[port] = owns;
		SDL_UnlockSpinlock(&shared_lock);
		if (active && !pad->gone)
			pad_frame(pad, gamepad, port, &wish[port], fresh[port], rumble_left[port], rumble_right[port],
				&port_settings[port], now);
		if (pad)
			pad_minute(pad, now, 0, port_settings[port].log);
	}
}

void ae_ds5_removed(struct SDL_Gamepad *gamepad)
{
	Uint64 now = SDL_GetTicks();
	SDL_JoystickID id;
	int i;

	if (!gamepad)
		return;
	id = SDL_GetGamepadID(gamepad);
	for (i = 0; i < PADS; i++)
	{
		if (pads[i].used && pads[i].id == id)
		{
			/* (the pad may still be there: SDL closing it for another reason; its hidraw node gone, it is not) */
#ifdef __linux__
			if (pads[i].backend != BACKEND_HIDRAW || (!pads[i].gone && access(pads[i].node, F_OK) == 0))
#endif
				pad_reset(&pads[i], gamepad, AE_DS5_FIELDS, now);
			if (pads[i].backend != BACKEND_NONE)
				platform_log("dualsense: the pad on port %d left", pads[i].port + 1);
			pad_minute(&pads[i], now, 1, applied[pads[i].port >= 0 && pads[i].port < PORTS ? pads[i].port : 0].log);
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
			shared.owns_rumble[i] = 0;
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

struct ae_ds5_settings ae_ds5_settings_port(int port)
{
	(void)port;
	return ae_ds5_settings_get();
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

void ae_ds5_pulse(int port, int left, int right, int ms)
{
	(void)port;
	(void)left;
	(void)right;
	(void)ms;
}

void ae_ds5_port(int port, struct SDL_Gamepad *gamepad, unsigned short *left_motor, unsigned short *right_motor)
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
