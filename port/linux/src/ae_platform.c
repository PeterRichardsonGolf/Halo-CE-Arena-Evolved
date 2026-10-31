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
#include "ae_config_players.h"
#include "ae_pad_ports.h"
#include "ae_seats.h"
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

/* a back button's Xbox button (input.back_right, input.back_left): a digital one's mask, or an analog one's index
plus 0x100; 0 for none or a word that is none of these (the log line says the word read) */
static int back_button_target(char const *word)
{
	static const struct
	{
		char const *name;
		int target;
	} targets[] =
	{
		{ "a", 0x100 + XINPUT_GAMEPAD_A }, { "b", 0x100 + XINPUT_GAMEPAD_B }, { "x", 0x100 + XINPUT_GAMEPAD_X },
		{ "y", 0x100 + XINPUT_GAMEPAD_Y }, { "black", 0x100 + XINPUT_GAMEPAD_BLACK },
		{ "white", 0x100 + XINPUT_GAMEPAD_WHITE }, { "left_stick", XINPUT_GAMEPAD_LEFT_THUMB },
		{ "right_stick", XINPUT_GAMEPAD_RIGHT_THUMB }, { "back", XINPUT_GAMEPAD_BACK },
		{ "start", XINPUT_GAMEPAD_START }, { "up", XINPUT_GAMEPAD_DPAD_UP }, { "down", XINPUT_GAMEPAD_DPAD_DOWN },
		{ "left", XINPUT_GAMEPAD_DPAD_LEFT }, { "right", XINPUT_GAMEPAD_DPAD_RIGHT },
	};
	unsigned int index;

	for (index = 0; index < sizeof(targets) / sizeof(targets[0]); index++)
	{
		if (!SDL_strcasecmp(word, targets[index].name))
			return targets[index].target;
	}
	return 0;
}

void ae_platform_back_buttons(
	struct SDL_Gamepad *gamepad,
	unsigned short *buttons,
	unsigned char *analog_buttons)
{
#if !defined(HALO_SERVER) && !defined(HALO_ANDROID)
	/* (SDL's names: a Switch 2 Pro Controller's GR is its right paddle, GL its left; an Xbox Elite's upper paddles
	are the same two, a DualSense Edge's back buttons too) */
	static const SDL_GamepadButton paddles[2] = { SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1, SDL_GAMEPAD_BUTTON_LEFT_PADDLE1 };
	/* (0.3.7: each player's own, BACK PADDLES: read as the pad's player's, xinput_sdl.c entering the port's player) */
	static unsigned long read_at[AE_CONFIG_PLAYERS] = { (unsigned long)-1, (unsigned long)-1, (unsigned long)-1,
		(unsigned long)-1 };
	static int targets[AE_CONFIG_PLAYERS][2];
	int player = ae_config_player_now(), side;

	if (read_at[player] != ae_config_player_changes())
	{
		read_at[player] = ae_config_player_changes();
		targets[player][0] = back_button_target(config_string("input.back_right"));
		targets[player][1] = back_button_target(config_string("input.back_left"));
		platform_log("back buttons: player %d right %s, left %s", player + 1, config_string("input.back_right"),
			config_string("input.back_left"));
	}
	for (side = 0; side < 2; side++)
	{
		if (!targets[player][side] || !SDL_GetGamepadButton(gamepad, paddles[side]))
			continue;
		if (targets[player][side] & 0x100)
			analog_buttons[targets[player][side] & 0xff] = 0xff;
		else
			*buttons |= (unsigned short)targets[player][side];
	}
#else
	(void)gamepad;
	(void)buttons;
	(void)analog_buttons;
#endif
}

/* ---------- pad ports: a pad keeps its port while it is connected (ae_pad_ports.c) */

#if !defined(HALO_SERVER) && !defined(HALO_ANDROID)
/* (two or more players on this machine; menu_functions.c's) */
extern unsigned char pc_menu_split_players(void);

/* each port's pad by its SDL id (0: none), kept from call to call */
static unsigned long stable_port_ids[AE_PAD_PORTS_MAXIMUM];
static SDL_SpinLock stable_port_lock;

/* no button of the pad held, its sticks and triggers at rest */
static int pad_at_rest(
	SDL_Gamepad *gamepad)
{
	int index;

	for (index = 0; index < SDL_GAMEPAD_BUTTON_COUNT; index++)
	{
		if (SDL_GetGamepadButton(gamepad, (SDL_GamepadButton)index))
			return 0;
	}
	for (index = 0; index < SDL_GAMEPAD_AXIS_COUNT; index++)
	{
		if (abs(SDL_GetGamepadAxis(gamepad, (SDL_GamepadAxis)index)) > 8000)
			return 0;
	}
	return 1;
}
#endif

#if !defined(HALO_SERVER) && !defined(HALO_ANDROID)
/* the LAN lobby's picker (ae_platform.h) */
#define PORT_PICK_WAIT_MS 10000
#define PORT_PICK_PADS 16
static struct
{
	/* the port waiting + 1 (0 none), since when; its press not yet let go */
	int waiting;
	Uint64 since;
	int swallow;
	/* each pad's buttons last seen, for a press's edge */
	unsigned long ids[PORT_PICK_PADS], buttons[PORT_PICK_PADS];
	int count;
	int escape_before;
} port_pick;

static int port_pick_blocks(void)
{
	return port_pick.waiting || port_pick.swallow;
}

unsigned long ae_platform_port_pad(
	int port)
{
	unsigned long id = 0;

	if (port < 0 || port >= AE_PAD_PORTS_MAXIMUM)
		return 0;
	SDL_LockSpinlock(&stable_port_lock);
	id = stable_port_ids[port];
	SDL_UnlockSpinlock(&stable_port_lock);
	return id;
}

int ae_platform_port_of_pad(
	unsigned long pad_id)
{
	int port, found = -1;

	SDL_LockSpinlock(&stable_port_lock);
	for (port = 0; pad_id && port < AE_PAD_PORTS_MAXIMUM; port++)
	{
		if (stable_port_ids[port] == pad_id)
			found = port;
	}
	SDL_UnlockSpinlock(&stable_port_lock);
	return found;
}

int ae_platform_port_give(
	int port,
	unsigned long pad_id)
{
	int other = -1, index;

	if (port < 0 || port >= AE_PAD_PORTS_MAXIMUM || !pad_id)
		return -1;
	SDL_LockSpinlock(&stable_port_lock);
	for (index = 0; index < AE_PAD_PORTS_MAXIMUM; index++)
	{
		if (stable_port_ids[index] == pad_id)
			other = index;
	}
	if (other != port)
	{
		if (other >= 0)
			stable_port_ids[other] = stable_port_ids[port];
		stable_port_ids[port] = pad_id;
	}
	SDL_UnlockSpinlock(&stable_port_lock);
	if (other == port)
		return port;
	platform_log("pad ports: port %d given its pad by the picker%s", port + 1, other >= 0 ? " (swapped)" : "");
	return other >= 0 ? other : port;
}

void ae_platform_port_pick_wait(
	int port)
{
	if (port < 0 || port >= AE_PAD_PORTS_MAXIMUM)
		return;
	port_pick.waiting = port + 1;
	port_pick.since = SDL_GetTicks();
	port_pick.swallow = 1;
	platform_log("pad ports: P%d's ASSIGN: waiting for a press", port + 1);
}

void ae_platform_port_pick_cancel(
	void)
{
	if (!port_pick.waiting)
		return;
	platform_log("pad ports: P%d's ASSIGN cancelled", port_pick.waiting);
	port_pick.waiting = 0;
	port_pick.swallow = 1;
}

int ae_platform_port_pick_waiting(
	int *left_ms)
{
	long left = 0;

	if (port_pick.waiting)
	{
		left = PORT_PICK_WAIT_MS - (long)(SDL_GetTicks() - port_pick.since);
		if (left < 0)
			left = 0;
	}
	if (left_ms)
		*left_ms = (int)left;
	return port_pick.waiting ? port_pick.waiting - 1 : -1;
}

unsigned ae_platform_port_pick_update(
	int *changed_port,
	int *changed_other)
{
	SDL_JoystickID *ids;
	struct platform_input_state input;
	int count = 0, index, held = 0, escape;
	unsigned long pressed_id = 0;
	unsigned result = 0;

	if (changed_port)
		*changed_port = -1;
	if (changed_other)
		*changed_other = -1;
	ids = SDL_GetGamepads(&count);
	for (index = 0; ids && index < count && index < PORT_PICK_PADS; index++)
	{
		SDL_Gamepad *gamepad = SDL_GetGamepadFromID(ids[index]);
		unsigned long id = (unsigned long)ids[index], buttons = 0, before = 0;
		int button, known;

		for (button = 0; gamepad && button < SDL_GAMEPAD_BUTTON_COUNT && button < 32; button++)
		{
			if (SDL_GetGamepadButton(gamepad, (SDL_GamepadButton)button))
				buttons |= 1UL << button;
		}
		for (known = 0; known < port_pick.count && port_pick.ids[known] != id; known++)
			;
		if (known < port_pick.count)
			before = port_pick.buttons[known];
		else if (port_pick.count < PORT_PICK_PADS)
		{
			known = port_pick.count++;
			port_pick.ids[known] = id;
			/* (first seen: what it holds is no press) */
			before = buttons;
		}
		else
			continue;
		port_pick.buttons[known] = buttons;
		held |= buttons != 0;
		if ((buttons & ~before) && !pressed_id)
			pressed_id = id;
	}
	SDL_free(ids);
	platform_input_read(&input, 0);
	escape = input.focused && input.keys[SDL_SCANCODE_ESCAPE];
	if (!port_pick.waiting)
	{
		if (port_pick.swallow && !held && !escape)
			port_pick.swallow = 0;
		port_pick.escape_before = escape;
		return 0;
	}
	if (SDL_GetTicks() - port_pick.since >= PORT_PICK_WAIT_MS || (escape && !port_pick.escape_before))
	{
		platform_log("pad ports: P%d's ASSIGN ended with no pad", port_pick.waiting);
		port_pick.waiting = 0;
		port_pick.swallow = 1;
		port_pick.escape_before = escape;
		return AE_SEATS_WAIT_ENDED;
	}
	port_pick.escape_before = escape;
	/* (the press that chose ASSIGN is let go first: until then nothing counts) */
	if (port_pick.swallow)
	{
		if (!held)
			port_pick.swallow = 0;
		return 0;
	}
	if (pressed_id)
	{
		int port = port_pick.waiting - 1, other = ae_platform_port_give(port, pressed_id);

		port_pick.waiting = 0;
		port_pick.swallow = 1;
		if (other < 0)
			return AE_SEATS_WAIT_ENDED;
		if (changed_port)
			*changed_port = port;
		if (changed_other)
			*changed_other = other != port ? other : -1;
		return AE_SEATS_ASSIGNED | (other != port ? AE_SEATS_SWAPPED : 0);
	}
	return 0;
}
int ae_platform_input_held_back(
	void)
{
	return port_pick_blocks() || ae_seats_input_held_back();
}
#else
/* (the dedicated server and Android: no picker) */
int ae_platform_input_held_back(void) { return 0; }
unsigned long ae_platform_port_pad(int port) { (void)port; return 0; }
int ae_platform_port_of_pad(unsigned long pad_id) { (void)pad_id; return -1; }
int ae_platform_port_give(int port, unsigned long pad_id) { (void)port; (void)pad_id; return -1; }
void ae_platform_port_pick_wait(int port) { (void)port; }
void ae_platform_port_pick_cancel(void) {}
int ae_platform_port_pick_waiting(int *left_ms) { if (left_ms) *left_ms = 0; return -1; }
unsigned ae_platform_port_pick_update(int *port, int *other) { if (port) *port = -1; if (other) *other = -1; return 0; }
#endif

int ae_platform_stable_ports(
	struct SDL_Gamepad **gamepads,
	int count,
	int ports)
{
#if !defined(HALO_SERVER) && !defined(HALO_ANDROID)
	SDL_Gamepad *listed[AE_PAD_PORTS_MAXIMUM];
	unsigned long ids[AE_PAD_PORTS_MAXIMUM], before[AE_PAD_PORTS_MAXIMUM], after[AE_PAD_PORTS_MAXIMUM];
	unsigned char at_rest[AE_PAD_PORTS_MAXIMUM];
	int listed_count, index, port, highest = 0;

	/* (split screen's seats, while they are on, put each seat's pad in its port: ae_seats.c) */
	if (ae_seats_active())
	{
		int filled = ae_seats_fill(gamepads, count, ports);

		if (ae_seats_active())
			return filled;
	}
	if (ports > AE_PAD_PORTS_MAXIMUM)
		ports = AE_PAD_PORTS_MAXIMUM;
	listed_count = count < ports ? count : ports;
	for (index = 0; index < listed_count; index++)
	{
		listed[index] = gamepads[index];
		ids[index] = (unsigned long)SDL_GetGamepadID(gamepads[index]);
		at_rest[index] = (unsigned char)pad_at_rest(gamepads[index]);
	}
	SDL_LockSpinlock(&stable_port_lock);
	memcpy(before, stable_port_ids, sizeof(before));
	ae_pad_ports_assign(stable_port_ids, ports, ids, at_rest, listed_count, pc_menu_split_players() != 0,
		ae_platform_lone_pad_first());
	for (port = 0; port < ports; port++)
	{
		gamepads[port] = NULL;
		for (index = 0; stable_port_ids[port] && index < listed_count; index++)
		{
			if (ids[index] == stable_port_ids[port])
				gamepads[port] = listed[index];
		}
		if (gamepads[port])
			highest = port + 1;
	}
	memcpy(after, stable_port_ids, sizeof(after));
	SDL_UnlockSpinlock(&stable_port_lock);

	for (port = 0; port < ports; port++)
	{
		char const *name;

		if (before[port] == after[port])
			continue;
		name = gamepads[port] ? SDL_GetGamepadName(gamepads[port]) : NULL;
		if (after[port])
			platform_log("pad ports: port %d is %s's", port + 1, name ? name : "a pad");
		else
			platform_log("pad ports: port %d has no pad", port + 1);
	}
	return highest;
#else
	(void)gamepads;
	(void)ports;
	return count;
#endif
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

/* (the modifiers a test's token holds: ae_platform_test_pad_press) */
static int test_modifiers;

int ae_platform_modifiers(void)
{
	struct platform_input_state input;
	int modifiers = test_modifiers;

	platform_input_read(&input, FALSE);
	if (!input.focused)
		return modifiers;
	if (input.keys[SDL_SCANCODE_LSHIFT] || input.keys[SDL_SCANCODE_RSHIFT])
		modifiers |= AE_MODIFIER_SHIFT;
	if (input.keys[SDL_SCANCODE_LCTRL] || input.keys[SDL_SCANCODE_RCTRL])
		modifiers |= AE_MODIFIER_CONTROL;
	return modifiers;
}

#if !defined(HALO_SERVER) && !defined(HALO_ANDROID)
/* a token's "+shift" / "+ctrl" (key:Right+shift, wheel:1+ctrl) */
static int token_modifiers(char const *token, int length)
{
	int modifiers = 0, at;

	for (at = 0; token && at < length; at++)
	{
		if (token[at] != '+')
			continue;
		if (length - at - 1 >= 5 && !strncmp(token + at + 1, "shift", 5))
			modifiers |= AE_MODIFIER_SHIFT;
		else if (length - at - 1 >= 4 && !strncmp(token + at + 1, "ctrl", 4))
			modifiers |= AE_MODIFIER_CONTROL;
	}
	return modifiers;
}
#endif

int ae_platform_menu_cursor_height(int lines, int height)
{
	int ae_height = lines * 16 / 480;

	if (!config_boolean("display.arena_menus"))
		return height;
	return ae_height < 16 ? 16 : ae_height;
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
	if (input.keys[SDL_SCANCODE_R] || input.keys[SDL_SCANCODE_Y])
		keys |= AE_KEY_R;
	if (input.keys[SDL_SCANCODE_F] || input.keys[SDL_SCANCODE_X])
		keys |= AE_KEY_F;
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

/* (ae_platform_keyboard_last_used) the order of the last uses: a count, so whichever was noted later wins */
static unsigned long input_uses, keyboard_used_at, pad_used_at;

void ae_platform_note_keyboard_used(void)
{
	keyboard_used_at = ++input_uses;
}

void ae_platform_note_pad_used(void)
{
	pad_used_at = ++input_uses;
}

void ae_platform_note_keyboard_input(const unsigned char *keys, const unsigned char *mouse_buttons, int mouse_live)
{
	int index;

	/* (any key from A to the modifiers but the system keys (a screenshot's Command), any mouse button) */
	for (index = SDL_SCANCODE_A; keys && index <= SDL_SCANCODE_RALT; index++)
	{
		if (keys[index] && index != SDL_SCANCODE_LGUI)
		{
			ae_platform_note_keyboard_used();
			return;
		}
	}
	for (index = 0; mouse_live && mouse_buttons && index < PLATFORM_MOUSE_BUTTON_COUNT; index++)
	{
		if (mouse_buttons[index])
		{
			ae_platform_note_keyboard_used();
			return;
		}
	}
}

int ae_platform_keyboard_last_used(void)
{
	if (!keyboard_used_at && !pad_used_at)
		return ae_platform_input_scheme() == 0;
	return keyboard_used_at > pad_used_at;
}

int ae_platform_keyboard_used(void)
{
	return keyboard_used_at != 0;
}

int ae_platform_pad_start_held(void)
{
#if !defined(HALO_SERVER) && !defined(HALO_ANDROID)
	SDL_JoystickID *ids;
	int count = 0, index, held = 0;

	ids = SDL_GetGamepads(&count);
	for (index = 0; ids && index < count && !held; index++)
	{
		SDL_Gamepad *gamepad = SDL_GetGamepadFromID(ids[index]);

		held = gamepad && SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_START);
	}
	SDL_free(ids);
	return held;
#else
	/* (no pads: the dedicated server; Android's are its own) */
	return 0;
#endif
}

int ae_platform_test_scheme(void)
{
	/* (debug.ae_message_test's words "playstation" and "nintendo": the pad whose art the HUD and the menus draw, with
	no such pad: the headless shots of each family) */
	const char *test = config_string("debug.ae_message_test");

	if (!test || !test[0])
		return -1;
	if (strstr(test, "nintendo"))
		return 3;
	return strstr(test, "playstation") ? 2 : -1;
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
others up; a trigger's token ("pad:lt", "pad:rt") pulls it, the other tokens leave both at rest; SDL applies the state
at the next pump of events */
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

	/* (the token's modifiers, held for AE while it lasts) */
	test_modifiers = token_modifiers(token, length);
	/* ("type:<a key's SDL name>": that key typed once as a real key event, as a player's key reaches KEY BINDINGS'
	PRESS A KEY (platform_binding_capture_begin takes key events only; "key:" holds a key in the keyboard's state) */
	{
		static char const *typed;

		if (token && length > 5 && length < 64 && !strncmp(token, "type:", 5) && token != typed)
		{
			char name[64];
			SDL_Event event;
			SDL_Scancode scancode;

			typed = token;
			memcpy(name, token + 5, (size_t)length - 5);
			name[length - 5] = 0;
			scancode = SDL_GetScancodeFromName(name);
			if (scancode == SDL_SCANCODE_UNKNOWN)
				platform_log("test input: no key named \"%s\"", name);
			else
			{
				SDL_zero(event);
				event.type = SDL_EVENT_KEY_DOWN;
				event.key.scancode = scancode;
				event.key.key = SDL_GetKeyFromScancode(scancode, SDL_KMOD_NONE, false);
				event.key.down = true;
				event.key.timestamp = SDL_GetTicksNS();
				SDL_PushEvent(&event);
				event.type = SDL_EVENT_KEY_UP;
				event.key.down = false;
				SDL_PushEvent(&event);
				platform_log("test input: typed %s", name);
			}
		}
	}
	/* (split screen's seats see the test's keys: ae_seats.c) */
	ae_seats_test_keys(token && ((length == 10 && !strncmp(token, "key:Return", 10)) ||
		(length == 9 && !strncmp(token, "key:Space", 9))), token && length == 10 && !strncmp(token, "key:Escape", 10));
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
	/* (the triggers, "pad:lt" / "pad:rt": pulled all the way for the token, at rest otherwise) */
	SDL_SetJoystickVirtualAxis(joystick, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, token && length == 6 &&
		!strncmp(token, "pad:lt", 6) ? SDL_JOYSTICK_AXIS_MAX : SDL_JOYSTICK_AXIS_MIN);
	SDL_SetJoystickVirtualAxis(joystick, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, token && length == 6 &&
		!strncmp(token, "pad:rt", 6) ? SDL_JOYSTICK_AXIS_MAX : SDL_JOYSTICK_AXIS_MIN);
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
	/* (a TEST RUN has F7 and F8 for itself: PASS and FAIL, not a verdict that quits the game) */
	if ((scancode != SDL_SCANCODE_F7 && scancode != SDL_SCANCODE_F8) || !*config_string("debug.test_verdict_file") ||
		ae_test_run_active())
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
