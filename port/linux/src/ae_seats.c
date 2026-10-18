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

void platform_log(char const *format, ...);

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
	} pads[WATCHED_PADS];
	int pad_count;
	int keyboard_join, keyboard_back;
	/* the keyboard's keys were seen up since the map went on */
	int keyboard_ready;
} seat_map;

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
	memset(&seat_map, 0, sizeof(seat_map));
	seat_map.on = 1;
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
static void pad_presses(
	SDL_Gamepad *gamepad,
	int *start,
	int *back)
{
	unsigned long id = (unsigned long)SDL_GetGamepadID(gamepad);
	int now_start = SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_START);
	int now_back = SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_EAST);
	int index;

	*start = *back = 0;
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
	}
	seat_map.pads[index].start = now_start;
	seat_map.pads[index].back = now_back;
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
	for (index = 0; index < count; index++)
	{
		int start, back, seat;
		char key[AE_SEAT_KEY_LENGTH];

		pad_presses(gamepads[index], &start, &back);
		if (ae_seats_of_pad(&seat_map.seats, (unsigned long)SDL_GetGamepadID(gamepads[index])) >= 0)
			continue;
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
	platform_input_read(&input, 0);
	keyboard_join = input.focused && (input.keys[SDL_SCANCODE_RETURN] || input.keys[SDL_SCANCODE_KP_ENTER] ||
		input.keys[SDL_SCANCODE_SPACE]);
	keyboard_back = input.focused && (input.keys[SDL_SCANCODE_ESCAPE] || input.keys[SDL_SCANCODE_BACKSPACE]);
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
	if (seat < 0 || seat >= AE_SEATS_MAXIMUM || seat_map.seats.seat[seat].kind == AE_SEAT_EMPTY)
		return;
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
	int seat = seat_map.on ? ae_seats_keyboard(&seat_map.seats) : -1;

	return seat >= 0 ? seat : 0;
}

int ae_seats_keyboard_plays(
	void)
{
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

	for (seat = 0; seat_map.on && seat < AE_SEATS_MAXIMUM; seat++)
	{
		if (seat_map.seats.seat[seat].kind != AE_SEAT_EMPTY && !seat_map.seats.seat[seat].held)
			mask |= 1UL << seat;
	}
	return mask;
}

int ae_seats_fill(
	struct SDL_Gamepad **gamepads,
	int count,
	int ports)
{
	/* (every pad connected, not only sdl_gamepads' first few: a seat's pad may be the fifth SDL lists) */
	SDL_Gamepad *listed[WATCHED_PADS];
	int index, seat, highest = 0;

	(void)count;
	count = connected_pads(listed);
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

#else

/* (the dedicated server and Android: no seats) */
void ae_seats_begin(void) {}
void ae_seats_end(void) {}
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

#endif
