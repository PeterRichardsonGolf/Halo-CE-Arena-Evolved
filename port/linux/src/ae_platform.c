/*
AE_PLATFORM.C

What AE's menus read from the platform layer (ae_platform.h): the keys the
game's menu keys leave out or use otherwise (Q and E for tabs, Tab and
Shift+Tab as focus steps, Page Up and Page Down), Tab's presses (counted by
an SDL event watch, so a tap within one frame counts), mouse button 4's
presses, and the device the player last used. It reads the state sdl_platform.c
already keeps (platform_input_read, without taking the mouse's motion) and
xinput_sdl.c's platform_input_scheme. (Button 4's count is ae_back_presses.c,
pure, so the unit tests build it.)
*/

#include "platform.h"
#include "sdl_platform.h"
#include "ae_platform.h"
#include "port_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if !defined(HALO_SERVER) && !defined(HALO_ANDROID)
#include <SDL3/SDL_atomic.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_joystick.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_timer.h>
#endif

#ifdef HALO_GAME_BROWSER
/* (xinput_sdl.c, with the game browser; the dedicated server has its own) */
int platform_input_scheme(void);
#endif

int ae_platform_lone_pad_first(void)
{
	static int read, value;

	if (!read)
	{
		value = config_boolean("debug.lone_pad_first");
		read = 1;
	}
	return value;
}

/* whether Num Lock is on. The dedicated server has no keyboard, and Android's guest has no SDL_GetModState (its SDL
is port/android/guest/runtime/guest_sdl.c's few functions): on there, so the keypad is digits */
static int num_lock(void)
{
#if defined(HALO_SERVER) || defined(HALO_ANDROID)
	return 1;
#else
	return (SDL_GetModState() & SDL_KMOD_NUM) != 0;
#endif
}

/* the numeric keypad's keys as AE's, with Num Lock off (SDL scancodes are where the keys are: KP_9 is the keypad's
Page Up whether or not Num Lock makes it a 9) */
static int keypad_keys(
	unsigned char const *keys,
	int numbers)
{
	int result = 0;

	if (numbers)
		return 0;
	if (keys[SDL_SCANCODE_KP_9])
		result |= AE_KEY_PAGE_UP;
	if (keys[SDL_SCANCODE_KP_3])
		result |= AE_KEY_PAGE_DOWN;
	if (keys[SDL_SCANCODE_KP_8])
		result |= AE_KEY_UP;
	if (keys[SDL_SCANCODE_KP_2])
		result |= AE_KEY_DOWN;
	if (keys[SDL_SCANCODE_KP_4])
		result |= AE_KEY_LEFT;
	if (keys[SDL_SCANCODE_KP_6])
		result |= AE_KEY_RIGHT;
	return result;
}

int ae_platform_keys(void)
{
	struct platform_input_state input;
	int keys = 0;

	platform_input_read(&input, FALSE);
	if (!input.focused)
		return 0;
	if (input.keys[SDL_SCANCODE_Q])
		keys |= AE_KEY_Q;
	if (input.keys[SDL_SCANCODE_E])
		keys |= AE_KEY_E;
	if (input.keys[SDL_SCANCODE_PAGEUP])
		keys |= AE_KEY_PAGE_UP;
	if (input.keys[SDL_SCANCODE_PAGEDOWN])
		keys |= AE_KEY_PAGE_DOWN;
	if (input.keys[SDL_SCANCODE_TAB])
		keys |= AE_KEY_TAB;
	if (input.keys[SDL_SCANCODE_LSHIFT] || input.keys[SDL_SCANCODE_RSHIFT])
		keys |= AE_KEY_SHIFT;
	return keys | keypad_keys(input.keys, num_lock());
}

#if !defined(HALO_SERVER) && !defined(HALO_ANDROID)
/* (atomics: SDL may call a watch from the thread that queues the event; the game's thread pumps them here) */
static SDL_AtomicInt tab_armed, tab_forward, tab_backward, tab_ignored;

static bool SDLCALL tab_watch(
	void *userdata,
	SDL_Event *event)
{
	(void)userdata;
	/* (Alt is either Alt key: right Alt+Tab switches windows on most desktops too; an AltGr that SDL reports as
	right Alt (some layouts) is taken as Alt, AltGr+Tab then stepping nothing, which costs nothing real) */
	if (event->type == SDL_EVENT_KEY_DOWN && event->key.scancode == SDL_SCANCODE_TAB && SDL_GetAtomicInt(&tab_armed))
	{
		switch (ae_platform_tab_press_kind(event->key.repeat, (event->key.mod & SDL_KMOD_SHIFT) != 0,
			(event->key.mod & SDL_KMOD_ALT) != 0, (event->key.mod & SDL_KMOD_GUI) != 0))
		{
		case AE_TAB_PRESS_FORWARD: SDL_AddAtomicInt(&tab_forward, 1); break;
		case AE_TAB_PRESS_BACKWARD: SDL_AddAtomicInt(&tab_backward, 1); break;
		case AE_TAB_PRESS_IGNORED: SDL_AddAtomicInt(&tab_ignored, 1); break;
		}
	}
	/* (a watch's answer is ignored: the event is queued as ever) */
	return true;
}

void ae_platform_arm_tab_presses(
	int armed)
{
	static int watching;

	if (armed && !watching)
		watching = SDL_AddEventWatch(tab_watch, NULL) ? 1 : 0;
	SDL_SetAtomicInt(&tab_armed, armed != 0);
	if (!armed)
	{
		SDL_SetAtomicInt(&tab_forward, 0);
		SDL_SetAtomicInt(&tab_backward, 0);
		SDL_SetAtomicInt(&tab_ignored, 0);
	}
}

void ae_platform_take_tab_presses(
	int *forward,
	int *backward,
	int *ignored)
{
	*forward = SDL_SetAtomicInt(&tab_forward, 0);
	*backward = SDL_SetAtomicInt(&tab_backward, 0);
	*ignored = SDL_SetAtomicInt(&tab_ignored, 0);
}
#else
void ae_platform_arm_tab_presses(
	int armed)
{
	(void)armed;
}

void ae_platform_take_tab_presses(
	int *forward,
	int *backward,
	int *ignored)
{
	*forward = *backward = *ignored = 0;
}
#endif

int ae_platform_mouse_left_held(void)
{
#if !defined(HALO_SERVER) && !defined(HALO_ANDROID)
	return (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON_LMASK) != 0;
#else
	return 0;
#endif
}

/* map sharing's key (ae_map_share_all.c): F6 held now (no button a game uses: the key also answers a question);
0 on the dedicated server and Android */
int ae_platform_map_share_key(void)
{
#if !defined(HALO_SERVER) && !defined(HALO_ANDROID)
	struct platform_input_state input;

	platform_input_read(&input, FALSE);
	return input.focused && input.keys[SDL_SCANCODE_F6];
#else
	return 0;
#endif
}

int ae_platform_input_scheme(void)
{
#ifdef HALO_GAME_BROWSER
	return platform_input_scheme();
#else
	return 1;
#endif
}

void ae_platform_test_keystroke(
	int scancode)
{
#if !defined(HALO_SERVER) && !defined(HALO_ANDROID)
	static Uint64 last[SDL_SCANCODE_COUNT];
	Uint64 now = SDL_GetTicks();
	SDL_Event event;

	if (scancode <= SDL_SCANCODE_UNKNOWN || scancode >= SDL_SCANCODE_COUNT || !config_boolean("display.arena_menus"))
		return;
	/* (a token is held for the first 250 ms of its second: one keystroke for it, the next a second later) */
	if (last[scancode] && now - last[scancode] < 500)
	{
		last[scancode] = now;
		return;
	}
	last[scancode] = now;
	SDL_zero(event);
	event.type = SDL_EVENT_KEY_DOWN;
	event.key.scancode = (SDL_Scancode)scancode;
	event.key.key = SDL_GetKeyFromScancode((SDL_Scancode)scancode, SDL_KMOD_NONE, false);
	event.key.down = true;
	event.key.timestamp = SDL_GetTicksNS();
	SDL_PushEvent(&event);
	event.type = SDL_EVENT_KEY_UP;
	event.key.down = false;
	SDL_PushEvent(&event);
#else
	(void)scancode;
#endif
}

/* debug.test_input's "pad:<button>" presses: one virtual SDL gamepad, attached at the first poll when the setting has
any, so the game finds it as a real gamepad (SDL_EVENT_GAMEPAD_ADDED: sdl_platform.c opens it) and its buttons go
through xinput_sdl.c's own mapping of pads to ports (port_gamepad: a lone pad shares port 0 with the keyboard, or is
port 1's while there are two players). Each call sets every button: the token's down (it is a pad's: "pad:b"), the
others up; SDL applies the state at the next pump of events */
void ae_platform_test_pad_press(
	char const *token,
	int length)
{
#if !defined(HALO_SERVER) && !defined(HALO_ANDROID)
	static const struct
	{
		char const *name;
		SDL_GamepadButton button;
	} buttons[] =
	{
		{ "a", SDL_GAMEPAD_BUTTON_SOUTH }, { "b", SDL_GAMEPAD_BUTTON_EAST }, { "x", SDL_GAMEPAD_BUTTON_WEST },
		{ "y", SDL_GAMEPAD_BUTTON_NORTH }, { "back", SDL_GAMEPAD_BUTTON_BACK }, { "start", SDL_GAMEPAD_BUTTON_START },
		{ "lb", SDL_GAMEPAD_BUTTON_LEFT_SHOULDER }, { "rb", SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER },
		{ "up", SDL_GAMEPAD_BUTTON_DPAD_UP }, { "down", SDL_GAMEPAD_BUTTON_DPAD_DOWN },
		{ "left", SDL_GAMEPAD_BUTTON_DPAD_LEFT }, { "right", SDL_GAMEPAD_BUTTON_DPAD_RIGHT },
	};
	/* (0: not looked at yet; 1: attached; -1: none wanted, or SDL refused it) */
	static int state;
	static SDL_Joystick *joystick;
	int pressed = -1;
	unsigned int index;

	if (!state)
	{
		char const *setting = config_string("debug.test_input");
		SDL_VirtualJoystickDesc desc;
		SDL_JoystickID id;

		state = -1;
		if (strncmp(setting, "menu:", 5) || !strstr(setting, "pad:"))
			return;
		/* (SDL drops a pad's input while no window of the game has the focus, as the test's hidden window has not) */
		SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
		SDL_INIT_INTERFACE(&desc);
		desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
		/* (the buttons and axes numbered as SDL_GamepadButton and SDL_GamepadAxis have them: the first of each) */
		desc.nbuttons = SDL_GAMEPAD_BUTTON_DPAD_RIGHT + 1;
		desc.button_mask = (1u << desc.nbuttons) - 1;
		desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
		desc.axis_mask = (1u << desc.naxes) - 1;
		desc.name = "AE test pad";
		/* ($AE_TEST_PAD_PLAYSTATION, tests only: a DualSense's ids, so SDL takes it for a PlayStation pad and the game
		draws PlayStation buttons, ae_buttons.c) */
		if (getenv("AE_TEST_PAD_PLAYSTATION"))
		{
			desc.vendor_id = 0x054C;
			desc.product_id = 0x0CE6;
		}
		id = SDL_AttachVirtualJoystick(&desc);
		joystick = id ? SDL_OpenJoystick(id) : NULL;
		platform_log("test input: virtual gamepad %s", joystick ? "attached" : SDL_GetError());
		if (!joystick)
			return;
		/* (the triggers at rest: SDL maps a virtual pad's trigger from the axis's whole range, so 0 would read as half
		pulled, and xinput_sdl.c's port_gamepad moves a pad between ports only at rest) */
		SDL_SetJoystickVirtualAxis(joystick, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, SDL_JOYSTICK_AXIS_MIN);
		SDL_SetJoystickVirtualAxis(joystick, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, SDL_JOYSTICK_AXIS_MIN);
		state = 1;
	}
	if (state < 0)
		return;
	if (token && length > 4 && !strncmp(token, "pad:", 4))
	{
		for (index = 0; index < sizeof(buttons) / sizeof(buttons[0]); index++)
		{
			if ((int)strlen(buttons[index].name) == length - 4 && !strncmp(token + 4, buttons[index].name, length - 4))
				pressed = buttons[index].button;
		}
	}
	for (index = 0; index < sizeof(buttons) / sizeof(buttons[0]); index++)
		SDL_SetJoystickVirtualButton(joystick, buttons[index].button, buttons[index].button == pressed);
#else
	(void)token;
	(void)length;
#endif
}

/* ---------- the test hooks debug.test_verdict_file and debug.test_verdict_auto */

#if !defined(HALO_SERVER) && !defined(HALO_ANDROID)
void platform_request_quit(void);

/* the verdict line, then the game quits */
static void test_verdict(int pass)
{
	static int done; /* one verdict line per run: a key held down repeats before the quit takes effect */
	FILE *file;
	time_t now = time(NULL);
	char stamp[32], name[96];
	int i;

	if (done)
		return;
	done = 1;
	/* (the name goes in a "<date> <time> <name> PASS|FAIL" line the batch parses: no spaces or control characters) */
	snprintf(name, sizeof(name), "%s", config_string("debug.test_name"));
	for (i = 0; name[i]; i++)
		if ((unsigned char)name[i] <= ' ' || name[i] == 0x7f)
			name[i] = '_';
	file = fopen(config_string("debug.test_verdict_file"), "a");
	strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M", localtime(&now));
	if (file)
	{
		fprintf(file, "%s %s %s\n", stamp, name, pass ? "PASS" : "FAIL");
		fclose(file);
	}
	platform_log("test verdict: %s %s", name, pass ? "PASS" : "FAIL");
	platform_request_quit();
}
#endif

int ae_platform_test_verdict_key(int scancode)
{
#if !defined(HALO_SERVER) && !defined(HALO_ANDROID)
	if ((scancode != SDL_SCANCODE_F7 && scancode != SDL_SCANCODE_F8) || !*config_string("debug.test_verdict_file"))
		return 0;
	test_verdict(scancode == SDL_SCANCODE_F7);
	return 1;
#else
	(void)scancode;
	return 0;
#endif
}

void ae_platform_test_verdict_auto(void)
{
#if !defined(HALO_SERVER) && !defined(HALO_ANDROID)
	static Uint64 auto_ticks = (Uint64)-1;
	static int auto_pass;

	if (auto_ticks == (Uint64)-1)
	{
		const char *text = config_string("debug.test_verdict_auto");

		auto_ticks = 0;
		if (*text && *config_string("debug.test_verdict_file") && (!strncmp(text, "PASS:", 5) || !strncmp(text, "FAIL:", 5)))
		{
			double seconds = atof(text + 5);

			if (seconds > 0.0) /* (a malformed value would give 0 s: a verdict at once) */
			{
				auto_pass = text[0] == 'P';
				auto_ticks = SDL_GetTicks() + (Uint64)(seconds * 1000.0);
			}
		}
	}
	if (auto_ticks && SDL_GetTicks() >= auto_ticks)
	{
		auto_ticks = 0;
		test_verdict(auto_pass);
	}
#endif
}
