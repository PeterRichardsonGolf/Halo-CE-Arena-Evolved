/*
AE_SEATS.C

Split screen's seat map (ae_seats.h): the devices read from SDL, the rules (ae_seats_rules.c) told what they do,
and the ports routed by seat while it is on (xinput_sdl.c's hooks).
*/

#include <stdio.h>
#include <string.h>

#include "ae_seats.h"

#if !defined(HALO_SERVER) && !defined(HALO_ANDROID)
#include "platform.h"
#include "sdl_platform.h"

#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_joystick.h>
#include <SDL3/SDL_scancode.h>
#include <SDL3/SDL_timer.h>

void platform_log(char const *format, ...);
/* (ae_split_screen.c's) */
int ae_split_screen_seats_wanted(void);

/* the pads seen, for their presses' edges: a button held when a pad is first seen is not a press (the press that
opened the screen does not join) */
#define WATCHED_PADS 16

static struct
{
	int on;
	struct ae_seats seats;
	struct
	{
		unsigned long id;
		int start, back;
		/* every button held, for any press's edge (the picker) */
		unsigned long buttons;
	} pads[WATCHED_PADS];
	int pad_count;
	int keyboard_join, keyboard_back;
	/* the keyboard's keys were seen up since the map went on */
	int keyboard_ready;
	/* (debug.test_input's keys now: they go into the controller's state, not SDL's keyboard) */
	int test_join, test_back;
	/* since when neither the JOIN screen nor a split screen game wants the map (0: they do) */
	Uint64 unwanted_since;
	/* asked to end once no key or button that backs out or joins is held (ae_seats_end_released) */
	int closing;
	/* the picker: ASSIGN's seat + 1 (0: none) and since when; every device's input kept from the game until all is
	let go after the press that assigned; the last change; when each seat's device was last pressed; each empty
	seat's candidate */
	int waiting;
	Uint64 waiting_since;
	int swallow;
	int last_seat, last_other;
	Uint64 pressed_at[AE_SEATS_MAXIMUM];
	struct
	{
		int kind;
		unsigned long pad_id;
	} candidate[AE_SEATS_MAXIMUM];
	unsigned char keys_before[SDL_SCANCODE_COUNT];
} seat_map;

/* the map lets go after this long unwanted (a screen changing, a game being made, are shorter) */
#define SEATS_LET_GO_MS 2000

void ae_seats_test_keys(
	int join,
	int back)
{
	seat_map.test_join = join;
	seat_map.test_back = back;
}

static int family_of(
	SDL_Gamepad *gamepad)
{
	switch (SDL_GetGamepadType(gamepad))
	{
	case SDL_GAMEPAD_TYPE_XBOX360:
	case SDL_GAMEPAD_TYPE_XBOXONE:
		return AE_SEAT_FAMILY_XBOX;
	case SDL_GAMEPAD_TYPE_PS3:
	case SDL_GAMEPAD_TYPE_PS4:
	case SDL_GAMEPAD_TYPE_PS5:
		return AE_SEAT_FAMILY_PLAYSTATION;
	case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO:
	case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
	case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
	case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:
		return AE_SEAT_FAMILY_NINTENDO;
	default:
		return AE_SEAT_FAMILY_OTHER;
	}
}

static char const *family_name(
	int family)
{
	switch (family)
	{
	case AE_SEAT_FAMILY_KEYBOARD: return "keyboard";
	case AE_SEAT_FAMILY_XBOX: return "xbox pad";
	case AE_SEAT_FAMILY_PLAYSTATION: return "playstation pad";
	case AE_SEAT_FAMILY_NINTENDO: return "nintendo pad";
	default: return "pad";
	}
}

/* a pad's own key: its GUID, and its serial (else its path), to know it again when it comes back */
static void pad_key(
	SDL_Gamepad *gamepad,
	char *key,
	size_t size)
{
	char guid[40];
	char const *serial = SDL_GetGamepadSerial(gamepad);
	char const *path = SDL_GetGamepadPath(gamepad);

	SDL_GUIDToString(SDL_GetJoystickGUID(SDL_GetGamepadJoystick(gamepad)), guid, (int)sizeof(guid));
	snprintf(key, size, "%s/%s", guid, serial && serial[0] ? serial : path ? path : "");
}

static char const *pad_name(
	SDL_Gamepad *gamepad)
{
	char const *name = gamepad ? SDL_GetGamepadName(gamepad) : NULL;

	return name ? name : "a pad";
}

/* the pads connected now (at most WATCHED_PADS) */
static int connected_pads(
	SDL_Gamepad **gamepads)
{
	SDL_JoystickID *ids;
	int count = 0, index, found = 0;

	ids = SDL_GetGamepads(&count);
	if (!ids)
		return 0;
	for (index = 0; index < count && found < WATCHED_PADS; index++)
	{
		SDL_Gamepad *gamepad = SDL_GetGamepadFromID(ids[index]);

		if (gamepad)
			gamepads[found++] = gamepad;
	}
	SDL_free(ids);
	return found;
}

/* a seat's pad unplugged, or the same pad back: TRUE when a seat changed */
static int seats_follow_pads(
	SDL_Gamepad **gamepads,
	int count)
{
	int seat, index, changed = 0;

	for (seat = 0; seat < AE_SEATS_MAXIMUM; seat++)
	{
		struct ae_seat *taken = &seat_map.seats.seat[seat];

		if (taken->kind != AE_SEAT_PAD || taken->held)
			continue;
		for (index = 0; index < count && (unsigned long)SDL_GetGamepadID(gamepads[index]) != taken->pad_id; index++)
			;
		if (index < count)
			continue;
		ae_seats_unplugged(&seat_map.seats, taken->pad_id);
		platform_log("seats: P%d unplugged (held)", seat + 1);
		changed = 1;
	}
	for (index = 0; index < count; index++)
	{
		unsigned long id = (unsigned long)SDL_GetGamepadID(gamepads[index]);
		char key[AE_SEAT_KEY_LENGTH];

		if (ae_seats_of_pad(&seat_map.seats, id) >= 0)
			continue;
		pad_key(gamepads[index], key, sizeof(key));
		seat = ae_seats_plugged(&seat_map.seats, id, key);
		if (seat < 0)
			continue;
		platform_log("seats: P%d taken back by the same pad", seat + 1);
		changed = 1;
	}
	return changed;
}

void ae_seats_begin(
	void)
{
	/* (the test's keys now are kept: the key that opened the screen is still down, and no press) */
	int test_join = seat_map.test_join, test_back = seat_map.test_back;

	memset(&seat_map, 0, sizeof(seat_map));
	seat_map.test_join = test_join;
	seat_map.test_back = test_back;
	seat_map.on = 1;
	seat_map.last_seat = seat_map.last_other = -1;
	platform_log("seats: on (the first to press START, ENTER or SPACE is PLAYER 1)");
}

void ae_seats_end(
	void)
{
	if (!seat_map.on)
		return;
	memset(&seat_map, 0, sizeof(seat_map));
	platform_log("seats: off");
}

int ae_seats_active(
	void)
{
	return seat_map.on;
}

/* a watched pad's press of START and of B since the last frame (its first sighting is no press) */
static unsigned long pad_buttons(
	SDL_Gamepad *gamepad)
{
	unsigned long buttons = 0;
	int button;

	for (button = 0; button < SDL_GAMEPAD_BUTTON_COUNT && button < 32; button++)
	{
		if (SDL_GetGamepadButton(gamepad, (SDL_GamepadButton)button))
			buttons |= 1UL << button;
	}
	return buttons;
}

static void pad_presses(
	SDL_Gamepad *gamepad,
	int *start,
	int *back,
	unsigned long *pressed)
{
	unsigned long id = (unsigned long)SDL_GetGamepadID(gamepad);
	int now_start = SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_START);
	int now_back = SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_EAST);
	unsigned long now_buttons = pad_buttons(gamepad);
	int index;

	*start = *back = 0;
	*pressed = 0;
	for (index = 0; index < seat_map.pad_count && seat_map.pads[index].id != id; index++)
		;
	if (index == seat_map.pad_count)
	{
		if (seat_map.pad_count == WATCHED_PADS)
			return;
		seat_map.pad_count++;
		seat_map.pads[index].id = id;
	}
	else
	{
		*start = now_start && !seat_map.pads[index].start;
		*back = now_back && !seat_map.pads[index].back;
		*pressed = now_buttons & ~seat_map.pads[index].buttons;
	}
	seat_map.pads[index].start = now_start;
	seat_map.pads[index].back = now_back;
	seat_map.pads[index].buttons = now_buttons;
}

/* a key newly down on the keyboard since the last look (any key: the picker) */
static int keyboard_pressed(
	struct platform_input_state const *input)
{
	int scancode, pressed = 0;

	for (scancode = 0; scancode < SDL_SCANCODE_COUNT; scancode++)
	{
		int down = input->focused && input->keys[scancode];

		/* (debug.test_input's ENTER / SPACE and ESC, which reach the controller's state, not SDL's keyboard) */
		if (scancode == SDL_SCANCODE_SPACE)
			down |= seat_map.test_join;
		if (scancode == SDL_SCANCODE_ESCAPE)
			down |= seat_map.test_back;
		pressed |= down && !seat_map.keys_before[scancode];
		seat_map.keys_before[scancode] = (unsigned char)down;
	}
	return pressed;
}

static int keyboard_any_held(
	struct platform_input_state const *input)
{
	int scancode;

	if (seat_map.test_join || seat_map.test_back)
		return 1;
	for (scancode = 0; input->focused && scancode < SDL_SCANCODE_COUNT; scancode++)
	{
		if (input->keys[scancode])
			return 1;
	}
	return 0;
}

static SDL_Gamepad *gamepad_of(
	unsigned long pad_id)
{
	return pad_id ? SDL_GetGamepadFromID((SDL_JoystickID)pad_id) : NULL;
}

/* a device given to a seat (the rules' ae_seats_assign): the change kept, both seats' names flashed */
static unsigned assign_device(
	int seat,
	int kind,
	unsigned long pad_id)
{
	char key[AE_SEAT_KEY_LENGTH] = "";
	SDL_Gamepad *gamepad = kind == AE_SEAT_PAD ? gamepad_of(pad_id) : NULL;
	int other;
	Uint64 now = SDL_GetTicks() | 1;

	if (gamepad)
		pad_key(gamepad, key, sizeof(key));
	other = ae_seats_assign(&seat_map.seats, seat, kind, pad_id, key);
	if (other < 0)
		return 0;
	seat_map.last_seat = seat;
	seat_map.last_other = other != seat ? other : -1;
	seat_map.pressed_at[seat] = now;
	if (other != seat)
	{
		seat_map.pressed_at[other] = now;
		platform_log("seats: P%d and P%d swapped controllers", seat + 1, other + 1);
		return AE_SEATS_ASSIGNED | AE_SEATS_SWAPPED;
	}
	platform_log("seats: P%d = %s", seat + 1, kind == AE_SEAT_KEYBOARD ? "keyboard + mouse" :
		gamepad ? pad_name(gamepad) : "a pad");
	return AE_SEATS_ASSIGNED;
}

/* ASSIGN's wait and the input kept from the game after it: TRUE when it took this frame (nothing else joins or backs
out) */
static int picker_update(
	SDL_Gamepad **gamepads,
	int count,
	struct platform_input_state const *input,
	unsigned *result)
{
	int index, seat = seat_map.waiting - 1;
	int key_pressed;

	if (!seat_map.waiting && !seat_map.swallow)
		return 0;
	key_pressed = keyboard_pressed(input);
	if (seat_map.swallow)
	{
		int held = keyboard_any_held(input);

		for (index = 0; index < count; index++)
		{
			int start, back;
			unsigned long pressed;

			pad_presses(gamepads[index], &start, &back, &pressed);
			held |= pad_buttons(gamepads[index]) != 0;
		}
		if (!held)
			seat_map.swallow = 0;
		/* (let go: the join keys too are taken as up, so nothing joins or backs out on the same press) */
		seat_map.keyboard_join = seat_map.keyboard_back = 1;
		return 1;
	}
	if (SDL_GetTicks() - seat_map.waiting_since >= AE_SEATS_ASSIGN_WAIT_MS)
	{
		platform_log("seats: P%d's ASSIGN: no press in time", seat + 1);
		seat_map.waiting = 0;
		*result |= AE_SEATS_WAIT_ENDED;
		return 1;
	}
	for (index = 0; index < count; index++)
	{
		int start, back;
		unsigned long pressed, id = (unsigned long)SDL_GetGamepadID(gamepads[index]);

		pad_presses(gamepads[index], &start, &back, &pressed);
		if (!pressed)
			continue;
		seat_map.waiting = 0;
		seat_map.swallow = 1;
		if (back && seat_map.seats.seat[seat].kind == AE_SEAT_PAD && seat_map.seats.seat[seat].pad_id == id)
		{
			platform_log("seats: P%d's ASSIGN cancelled", seat + 1);
			*result |= AE_SEATS_WAIT_ENDED;
			return 1;
		}
		*result |= assign_device(seat, AE_SEAT_PAD, id);
		if (!(*result & AE_SEATS_ASSIGNED))
			*result |= AE_SEATS_WAIT_ENDED;
		return 1;
	}
	if (key_pressed)
	{
		seat_map.waiting = 0;
		seat_map.swallow = 1;
		if (input->keys[SDL_SCANCODE_ESCAPE] || seat_map.test_back)
		{
			platform_log("seats: P%d's ASSIGN cancelled", seat + 1);
			*result |= AE_SEATS_WAIT_ENDED;
			return 1;
		}
		*result |= assign_device(seat, AE_SEAT_KEYBOARD, 0);
		if (!(*result & AE_SEATS_ASSIGNED))
			*result |= AE_SEATS_WAIT_ENDED;
	}
	return 1;
}

unsigned ae_seats_update(
	void)
{
	SDL_Gamepad *gamepads[WATCHED_PADS];
	struct platform_input_state input;
	unsigned result = 0;
	int count, index, keyboard_join, keyboard_back;

	if (!seat_map.on)
		return 0;
	count = connected_pads(gamepads);
	if (seats_follow_pads(gamepads, count))
		result |= AE_SEATS_CHANGED;
	platform_input_read(&input, 0);
	if (picker_update(gamepads, count, &input, &result))
		return result;
	for (index = 0; index < count; index++)
	{
		int start, back, seat;
		unsigned long pressed;
		char key[AE_SEAT_KEY_LENGTH];

		pad_presses(gamepads[index], &start, &back, &pressed);
		seat = ae_seats_of_pad(&seat_map.seats, (unsigned long)SDL_GetGamepadID(gamepads[index]));
		if (seat >= 0)
		{
			if (pressed)
				seat_map.pressed_at[seat] = SDL_GetTicks() | 1;
			continue;
		}
		if (back)
			result |= AE_SEATS_BACK;
		if (!start)
			continue;
		pad_key(gamepads[index], key, sizeof(key));
		seat = ae_seats_join(&seat_map.seats, AE_SEAT_PAD, (unsigned long)SDL_GetGamepadID(gamepads[index]), key);
		if (seat < 0)
		{
			result |= AE_SEATS_FULL;
			continue;
		}
		platform_log("seats: P%d = %s \"%s\" joined", seat + 1, family_name(family_of(gamepads[index])),
			pad_name(gamepads[index]));
		result |= AE_SEATS_JOINED;
	}
	/* (the keyboard: ENTER or SPACE joins, ESC or BACKSPACE with no seat asks to go back; only keys pressed after
	all were seen up) */
	keyboard_join = input.focused && (input.keys[SDL_SCANCODE_RETURN] || input.keys[SDL_SCANCODE_KP_ENTER] ||
		input.keys[SDL_SCANCODE_SPACE]);
	keyboard_back = input.focused && (input.keys[SDL_SCANCODE_ESCAPE] || input.keys[SDL_SCANCODE_BACKSPACE]);
	keyboard_join |= seat_map.test_join;
	keyboard_back |= seat_map.test_back;
	if (!keyboard_join && !keyboard_back)
		seat_map.keyboard_ready = 1;
	if (seat_map.keyboard_ready && ae_seats_keyboard(&seat_map.seats) < 0)
	{
		if (keyboard_back && !seat_map.keyboard_back)
			result |= AE_SEATS_BACK;
		if (keyboard_join && !seat_map.keyboard_join)
		{
			int seat = ae_seats_join(&seat_map.seats, AE_SEAT_KEYBOARD, 0, NULL);

			if (seat < 0)
				result |= AE_SEATS_FULL;
			else
			{
				platform_log("seats: P%d = keyboard + mouse joined", seat + 1);
				result |= AE_SEATS_JOINED;
			}
		}
	}
	seat_map.keyboard_join = keyboard_join;
	seat_map.keyboard_back = keyboard_back;
	if (keyboard_pressed(&input) && ae_seats_keyboard(&seat_map.seats) >= 0)
		seat_map.pressed_at[ae_seats_keyboard(&seat_map.seats)] = SDL_GetTicks() | 1;
	return result;
}

void ae_seats_get(
	struct ae_seats *seats)
{
	if (seats)
		*seats = seat_map.seats;
}

void ae_seats_view(
	int seat,
	struct ae_seat_view *view)
{
	struct ae_seat const *taken;

	memset(view, 0, sizeof(*view));
	if (seat < 0 || seat >= AE_SEATS_MAXIMUM)
		return;
	taken = &seat_map.seats.seat[seat];
	view->kind = taken->kind;
	view->held = taken->held;
	if (taken->kind == AE_SEAT_KEYBOARD)
	{
		view->family = AE_SEAT_FAMILY_KEYBOARD;
		snprintf(view->name, sizeof(view->name), "keyboard + mouse");
	}
	else if (taken->kind == AE_SEAT_PAD)
	{
		SDL_Gamepad *gamepad = taken->pad_id ? SDL_GetGamepadFromID((SDL_JoystickID)taken->pad_id) : NULL;

		view->family = gamepad ? family_of(gamepad) : AE_SEAT_FAMILY_OTHER;
		snprintf(view->name, sizeof(view->name), "%s", gamepad ? pad_name(gamepad) : "");
	}
}

void ae_seats_leave_seat(
	int seat)
{
	int index;

	if (seat < 0 || seat >= AE_SEATS_MAXIMUM || seat_map.seats.seat[seat].kind == AE_SEAT_EMPTY)
		return;
	/* (the press that left, B or ESC, is held still: the device counts as pressing nothing until it lets go, else
	that B would also ask, seat-less now, to go back) */
	if (seat_map.seats.seat[seat].kind == AE_SEAT_KEYBOARD)
	{
		seat_map.keyboard_join = seat_map.keyboard_back = 1;
	}
	for (index = 0; index < seat_map.pad_count; index++)
	{
		if (seat_map.pads[index].id == seat_map.seats.seat[seat].pad_id)
			seat_map.pads[index].start = seat_map.pads[index].back = 1;
	}
	ae_seats_leave(&seat_map.seats, seat);
	platform_log("seats: P%d left", seat + 1);
}

int ae_seats_close(
	int moved[AE_SEATS_MAXIMUM])
{
	int seat, taken = ae_seats_close_gaps(&seat_map.seats, moved);

	for (seat = 0; moved && seat < AE_SEATS_MAXIMUM; seat++)
	{
		if (moved[seat] >= 0 && moved[seat] != seat)
			platform_log("seats: P%d is now P%d", seat + 1, moved[seat] + 1);
	}
	return taken;
}

int ae_seats_keyboard_port(
	void)
{
	static int logged = -2;
	int seat = seat_map.on ? ae_seats_keyboard(&seat_map.seats) : -1;

	/* (said when it changes: which controller the keyboard and mouse are) */
	if (seat != logged)
	{
		logged = seat;
		if (seat >= 0)
			platform_log("seats: the keyboard and mouse are controller %d (P%d)", seat + 1, seat + 1);
		else if (seat_map.on)
			platform_log("seats: the keyboard and mouse have no seat");
	}
	return seat >= 0 ? seat : 0;
}

int ae_seats_keyboard_plays(
	void)
{
	/* (ASSIGN's wait, and its press until let go: no device drives the game) */
	if (seat_map.on && (seat_map.waiting || seat_map.swallow))
		return 0;
	return !seat_map.on || ae_seats_keyboard(&seat_map.seats) >= 0;
}

int ae_seats_keyboard_controller(
	void)
{
	return seat_map.on ? ae_seats_keyboard(&seat_map.seats) : 0;
}

unsigned long ae_seats_connected(
	void)
{
	unsigned long mask = 0;
	int seat;

	if (!seat_map.on)
		return 0;
	for (seat = 0; seat < AE_SEATS_MAXIMUM; seat++)
	{
		if (seat_map.seats.seat[seat].kind != AE_SEAT_EMPTY && !seat_map.seats.seat[seat].held)
			mask |= 1UL << seat;
	}
	/* (port 1 stays, but for an unplugged pad's held seat: the menus are the first controller's, and the game takes
	it gone at the main menu for the player leaving, back out of every screen) */
	if (!seat_map.seats.seat[0].held)
		mask |= 1UL;
	return mask;
}

/* nothing held that backs out or joins: the keyboard's ENTER, SPACE, ESC, BACKSPACE, any pad's START or B */
static int nothing_held(
	SDL_Gamepad **gamepads,
	int count)
{
	struct platform_input_state input;
	int index;

	if (seat_map.test_join || seat_map.test_back)
		return 0;
	platform_input_read(&input, 0);
	if (input.keys[SDL_SCANCODE_RETURN] || input.keys[SDL_SCANCODE_KP_ENTER] || input.keys[SDL_SCANCODE_SPACE] ||
		input.keys[SDL_SCANCODE_ESCAPE] || input.keys[SDL_SCANCODE_BACKSPACE])
		return 0;
	for (index = 0; index < count; index++)
	{
		if (SDL_GetGamepadButton(gamepads[index], SDL_GAMEPAD_BUTTON_START) ||
			SDL_GetGamepadButton(gamepads[index], SDL_GAMEPAD_BUTTON_EAST))
			return 0;
	}
	return 1;
}

void ae_seats_end_released(
	void)
{
	if (seat_map.on)
		seat_map.closing = 1;
}

int ae_seats_fill(
	struct SDL_Gamepad **gamepads,
	int count,
	int ports)
{
	/* (every pad connected, not only sdl_gamepads' first few: a seat's pad may be the fifth SDL lists) */
	SDL_Gamepad *listed[WATCHED_PADS];
	int index, seat, highest = 0;

	/* (the map lets go by itself once neither the JOIN screen nor a split screen game wants it: a game left any
	way at all gives the keyboard back to port 0) */
	if (ae_split_screen_seats_wanted())
		seat_map.unwanted_since = 0;
	else if (!seat_map.unwanted_since)
		seat_map.unwanted_since = SDL_GetTicks() | 1;
	else if (SDL_GetTicks() - seat_map.unwanted_since >= SEATS_LET_GO_MS)
	{
		platform_log("seats: no JOIN screen and no split screen game for %d ms", SEATS_LET_GO_MS);
		ae_seats_end();
		return count;
	}
	(void)count;
	count = connected_pads(listed);
	/* (backed out of the JOIN screen: off once the B or ESC that did it is let go, else it would press B again on
	the first controller, the screen under) */
	if (seat_map.closing && nothing_held(listed, count))
	{
		ae_seats_end();
		return 0;
	}
	seats_follow_pads(listed, count);
	for (seat = 0; seat < ports; seat++)
	{
		struct ae_seat const *taken = seat < AE_SEATS_MAXIMUM ? &seat_map.seats.seat[seat] : NULL;

		gamepads[seat] = NULL;
		for (index = 0; taken && taken->kind == AE_SEAT_PAD && taken->pad_id && index < count; index++)
		{
			if ((unsigned long)SDL_GetGamepadID(listed[index]) == taken->pad_id)
				gamepads[seat] = listed[index];
		}
		if (gamepads[seat])
			highest = seat + 1;
	}
	return highest;
}

/* ---- the controller picker */

int ae_seats_devices(
	struct ae_seat_device *list,
	int maximum)
{
	SDL_Gamepad *gamepads[WATCHED_PADS];
	int count, index, listed = 0, other;

	if (!list || maximum <= 0)
		return 0;
	memset(&list[0], 0, sizeof(list[0]));
	list[0].kind = AE_SEAT_KEYBOARD;
	list[0].family = AE_SEAT_FAMILY_KEYBOARD;
	list[0].seat = ae_seats_keyboard(&seat_map.seats);
	ae_seats_short_name(AE_SEAT_FAMILY_KEYBOARD, NULL, 0, list[0].name, (int)sizeof(list[0].name));
	listed = 1;
	count = connected_pads(gamepads);
	for (index = 0; index < count && listed < maximum; index++)
	{
		struct ae_seat_device *device = &list[listed++];

		memset(device, 0, sizeof(*device));
		device->kind = AE_SEAT_PAD;
		device->pad_id = (unsigned long)SDL_GetGamepadID(gamepads[index]);
		device->family = family_of(gamepads[index]);
		device->seat = ae_seats_of_pad(&seat_map.seats, device->pad_id);
		ae_seats_short_name(device->family, pad_name(gamepads[index]), 0, device->name, (int)sizeof(device->name));
	}
	/* (numbered when two share a name: XBOX 1, XBOX 2) */
	for (index = 1; index < listed; index++)
	{
		int same = 0, number = 0;
		char base[24];

		snprintf(base, sizeof(base), "%s", list[index].name);
		for (other = 1; other < listed; other++)
		{
			char name[24];

			ae_seats_short_name(list[other].family, list[other].family == list[index].family ?
				pad_name(gamepad_of(list[other].pad_id)) : NULL, 0, name, (int)sizeof(name));
			if (strcmp(name, base))
				continue;
			same++;
			if (other <= index)
				number++;
		}
		if (same > 1)
			ae_seats_short_name(list[index].family, pad_name(gamepad_of(list[index].pad_id)), number,
				list[index].name, (int)sizeof(list[index].name));
	}
	return listed;
}

static int device_index(
	struct ae_seat_device const *list,
	int count,
	int kind,
	unsigned long pad_id)
{
	int index;

	for (index = 0; index < count; index++)
	{
		if (list[index].kind == kind && (kind != AE_SEAT_PAD || list[index].pad_id == pad_id))
			return index;
	}
	return -1;
}

unsigned ae_seats_cycle(
	int seat,
	int direction)
{
	struct ae_seat_device list[AE_SEATS_DEVICES_MAXIMUM];
	struct ae_seat const *taken;
	int count, start, step;

	if (!seat_map.on || seat < 0 || seat >= AE_SEATS_MAXIMUM || seat_map.waiting)
		return 0;
	taken = &seat_map.seats.seat[seat];
	if (taken->kind == AE_SEAT_EMPTY)
		return 0;
	count = ae_seats_devices(list, AE_SEATS_DEVICES_MAXIMUM);
	start = taken->held ? -1 : device_index(list, count, taken->kind, taken->pad_id);
	if (start < 0)
		start = direction < 0 ? 0 : count - 1;
	/* (the next that may be its: a held seat takes only a free one) */
	for (step = 1; step < count + (taken->held ? 1 : 0); step++)
	{
		int index = ((start + (direction < 0 ? -step : step)) % count + count) % count;
		unsigned bits = assign_device(seat, list[index].kind, list[index].pad_id);

		if (bits)
			return bits;
	}
	return 0;
}

static int candidate_free(
	struct ae_seat_device const *list,
	int count,
	int seat)
{
	int index = device_index(list, count, seat_map.candidate[seat].kind, seat_map.candidate[seat].pad_id);

	return index >= 0 && list[index].seat < 0 ? index : -1;
}

void ae_seats_candidate(
	int seat,
	struct ae_seat_device *device)
{
	struct ae_seat_device list[AE_SEATS_DEVICES_MAXIMUM];
	int count, index;

	memset(device, 0, sizeof(*device));
	device->seat = -1;
	if (!seat_map.on || seat < 0 || seat >= AE_SEATS_MAXIMUM)
		return;
	count = ae_seats_devices(list, AE_SEATS_DEVICES_MAXIMUM);
	index = candidate_free(list, count, seat);
	/* (none chosen, or taken since: the first free pad, the keyboard last) */
	for (int pass = 0; index < 0 && pass < 2; pass++)
	{
		for (int other = 0; other < count && index < 0; other++)
		{
			if (list[other].seat < 0 && (pass ? list[other].kind == AE_SEAT_KEYBOARD : list[other].kind == AE_SEAT_PAD))
				index = other;
		}
	}
	if (index < 0)
	{
		seat_map.candidate[seat].kind = AE_SEAT_EMPTY;
		return;
	}
	seat_map.candidate[seat].kind = list[index].kind;
	seat_map.candidate[seat].pad_id = list[index].pad_id;
	*device = list[index];
}

void ae_seats_candidate_cycle(
	int seat,
	int direction)
{
	struct ae_seat_device list[AE_SEATS_DEVICES_MAXIMUM], current;
	int count, start, step;

	ae_seats_candidate(seat, &current);
	if (current.kind == AE_SEAT_EMPTY)
		return;
	count = ae_seats_devices(list, AE_SEATS_DEVICES_MAXIMUM);
	start = device_index(list, count, current.kind, current.pad_id);
	for (step = 1; start >= 0 && step < count; step++)
	{
		int index = ((start + (direction < 0 ? -step : step)) % count + count) % count;

		if (list[index].seat >= 0)
			continue;
		seat_map.candidate[seat].kind = list[index].kind;
		seat_map.candidate[seat].pad_id = list[index].pad_id;
		return;
	}
}

unsigned ae_seats_candidate_take(
	int seat)
{
	struct ae_seat_device device;

	ae_seats_candidate(seat, &device);
	if (device.kind == AE_SEAT_EMPTY || seat_map.seats.seat[seat].kind != AE_SEAT_EMPTY)
		return 0;
	return assign_device(seat, device.kind, device.pad_id) ? AE_SEATS_ASSIGNED | AE_SEATS_JOINED : 0;
}

void ae_seats_assign_wait(
	int seat)
{
	if (!seat_map.on || seat < 0 || seat >= AE_SEATS_MAXIMUM || seat_map.seats.seat[seat].kind == AE_SEAT_EMPTY)
		return;
	seat_map.waiting = seat + 1;
	seat_map.waiting_since = SDL_GetTicks();
	/* (the press that chose ASSIGN is let go before any press counts: until then, nothing reaches the game either) */
	seat_map.swallow = 1;
	platform_log("seats: P%d's ASSIGN: waiting for a press", seat + 1);
}

void ae_seats_assign_cancel(
	void)
{
	if (!seat_map.waiting)
		return;
	platform_log("seats: P%d's ASSIGN cancelled", seat_map.waiting);
	seat_map.waiting = 0;
	seat_map.swallow = 1;
}

int ae_seats_assign_waiting(
	int *left_ms)
{
	long left = 0;

	if (seat_map.on && seat_map.waiting)
	{
		left = AE_SEATS_ASSIGN_WAIT_MS - (long)(SDL_GetTicks() - seat_map.waiting_since);
		if (left < 0)
			left = 0;
	}
	if (left_ms)
		*left_ms = (int)left;
	return seat_map.on && seat_map.waiting ? seat_map.waiting - 1 : -1;
}

void ae_seats_last_change(
	int *seat,
	int *other)
{
	if (seat)
		*seat = seat_map.on ? seat_map.last_seat : -1;
	if (other)
		*other = seat_map.on ? seat_map.last_other : -1;
}

long ae_seats_pressed_ago(
	int seat)
{
	if (!seat_map.on || seat < 0 || seat >= AE_SEATS_MAXIMUM || !seat_map.pressed_at[seat])
		return -1;
	return (long)(SDL_GetTicks() - seat_map.pressed_at[seat]);
}

int ae_seats_input_held_back(
	void)
{
	return seat_map.on && (seat_map.waiting || seat_map.swallow);
}

#else

/* (the dedicated server and Android: no seats) */
int ae_seats_input_held_back(void) { return 0; }
void ae_seats_begin(void) {}
void ae_seats_end(void) {}
void ae_seats_end_released(void) {}
void ae_seats_test_keys(int join, int back) { (void)join; (void)back; }
int ae_seats_active(void) { return 0; }
unsigned ae_seats_update(void) { return 0; }
void ae_seats_get(struct ae_seats *seats) { if (seats) memset(seats, 0, sizeof(*seats)); }
void ae_seats_view(int seat, struct ae_seat_view *view) { (void)seat; memset(view, 0, sizeof(*view)); }
void ae_seats_leave_seat(int seat) { (void)seat; }
int ae_seats_close(int moved[AE_SEATS_MAXIMUM]) { int seat; for (seat = 0; moved && seat < AE_SEATS_MAXIMUM; seat++) moved[seat] = -1; return 0; }
int ae_seats_keyboard_port(void) { return 0; }
int ae_seats_keyboard_plays(void) { return 1; }
int ae_seats_keyboard_controller(void) { return 0; }
unsigned long ae_seats_connected(void) { return 0; }
int ae_seats_fill(struct SDL_Gamepad **gamepads, int count, int ports) { (void)gamepads; (void)ports; return count; }
int ae_seats_devices(struct ae_seat_device *list, int maximum) { (void)list; (void)maximum; return 0; }
unsigned ae_seats_cycle(int seat, int direction) { (void)seat; (void)direction; return 0; }
void ae_seats_candidate(int seat, struct ae_seat_device *device) { (void)seat; memset(device, 0, sizeof(*device)); }
void ae_seats_candidate_cycle(int seat, int direction) { (void)seat; (void)direction; }
unsigned ae_seats_candidate_take(int seat) { (void)seat; return 0; }
void ae_seats_assign_wait(int seat) { (void)seat; }
void ae_seats_assign_cancel(void) {}
int ae_seats_assign_waiting(int *left_ms) { if (left_ms) *left_ms = 0; return -1; }
void ae_seats_last_change(int *seat, int *other) { if (seat) *seat = -1; if (other) *other = -1; }
long ae_seats_pressed_ago(int seat) { (void)seat; return -1; }

#endif
