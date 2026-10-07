/*
AE_INPUT.C

The AE menus' input bridge (ae_input.h).

Buttons: the game's event queue (interface/event_manager.c), as the
virtual keyboard and the server browser read it. A held button's event comes
again every frame with a larger value, so a press is value 1. The keyboard
reaches it as the first controller (xinput_sdl.c keyboard_gamepad): Enter and
Space are A, Escape, Backspace and mouse button 4 are B, Tab is Y, E and
Delete are X, C is the black button. Q, E (as tabs), Page Up and Page Down,
which that mapping leaves out, are read from the platform (ae_platform.c);
while E is held, the first controller's X is its tab and not X.

Directions: each controller's d-pad and left stick, held, with AE's own
repeat (ae_repeat: 400 ms, then 80 ms, 40 ms after 1 s), not the event
queue's 250 ms one.

Pointer: the menus' pointer (halo_ui_pointer) in layout units; a right click
is BACK.

Everything read is consumed: the queue is flushed every frame a screen is
open, so no button pressed on an AE screen reaches the game's menus behind it
when the screen closes (ae_hooks.c also holds input back until it is let go).
*/

#include <string.h>

#include "cseries.h"
#include "cseries/cseries_windows.h"
#include "input/input.h"
#include "interface/event_manager.h"
#include "halo_ui_pointer.h"
#include "../src/ae_draw.h"
#include "../src/ae_platform.h"
#include "ae_input.h"
#include "ae_ui.h"

enum
{
	/* (event_manager.c's event types, which it keeps to itself: _event_type_button; browser_screen.c keeps a copy
	too; tools/test_ae_hooks.py checks they agree) */
	AE_EVENT_TYPE_BUTTON = 3,

	/* a stick this far over is a direction held */
	AE_STICK_THRESHOLD = 16384,
	/* a poll this long after the last one starts over: nothing counts as held from before */
	AE_POLL_GAP_MS = 250,
	NUMBER_OF_DIRECTIONS = 4,
	MAXIMUM_CONTROLLERS = 4
};

static struct
{
	struct ae_repeat repeats[MAXIMUM_CONTROLLERS][NUMBER_OF_DIRECTIONS];
	unsigned long last_poll;
	int polled;
	int keys;
} ae_input;

static unsigned char const direction_actions[NUMBER_OF_DIRECTIONS] =
{
	AE_ACTION_UP, AE_ACTION_DOWN, AE_ACTION_LEFT, AE_ACTION_RIGHT
};

/* the device of a controller's input: the first is the keyboard's too */
static unsigned char device_of(
	short controller)
{
	int scheme = ae_platform_input_scheme();

	if (controller == 0)
		return (unsigned char)(scheme >= 0 && scheme <= 3 ? scheme : AE_DEVICE_XBOX);
	return (unsigned char)(scheme == AE_DEVICE_PLAYSTATION || scheme == AE_DEVICE_NINTENDO ? scheme : AE_DEVICE_XBOX);
}

static void send_action(
	short controller,
	int action)
{
	struct ae_event event;

	event.player = controller;
	event.action = (unsigned char)action;
	event.device = device_of(controller);
	ae_ui_dispatch(&event);
}

/* a pressed button's action (the d-pad's come from the held directions) */
static int button_action(
	short button)
{
	switch (button)
	{
	case _gamepad_analog_button_a: return AE_ACTION_ACCEPT;
	case _gamepad_analog_button_b: return AE_ACTION_BACK;
	case _gamepad_analog_button_x: return AE_ACTION_X;
	case _gamepad_analog_button_y: return AE_ACTION_Y;
	case _gamepad_analog_button_white: return AE_ACTION_TAB_PREVIOUS;
	case _gamepad_analog_button_black: return AE_ACTION_TAB_NEXT;
	case _gamepad_analog_button_left_trigger: return AE_ACTION_PAGE_UP;
	case _gamepad_analog_button_right_trigger: return AE_ACTION_PAGE_DOWN;
	case _gamepad_binary_button_start: return AE_ACTION_START;
	case _gamepad_binary_button_back: return AE_ACTION_SELECT;
	}
	return AE_ACTION_NONE;
}

static struct gamepad_state const *controller_state(
	short controller)
{
	return input_has_gamepad(controller) ? input_get_gamepad_state(controller) : NULL;
}

/* which directions a controller holds */
static void held_directions(
	short controller,
	int held[NUMBER_OF_DIRECTIONS])
{
	struct gamepad_state const *state = controller_state(controller);

	held[0] = held[1] = held[2] = held[3] = 0;
	if (!state)
		return;
	held[0] = state->buttons[_gamepad_binary_button_dpad_up] || state->sticks[_gamepad_stick_left].y > AE_STICK_THRESHOLD;
	held[1] = state->buttons[_gamepad_binary_button_dpad_down] ||
		state->sticks[_gamepad_stick_left].y < -AE_STICK_THRESHOLD;
	held[2] = state->buttons[_gamepad_binary_button_dpad_left] ||
		state->sticks[_gamepad_stick_left].x < -AE_STICK_THRESHOLD;
	held[3] = state->buttons[_gamepad_binary_button_dpad_right] ||
		state->sticks[_gamepad_stick_left].x > AE_STICK_THRESHOLD;
}

int ae_input_held(
	void)
{
	short controller;

	for (controller = 0; controller < MAXIMUM_CONTROLLERS; controller++)
	{
		struct gamepad_state const *state = controller_state(controller);
		short button;

		if (!state)
			continue;
		for (button = 0; button < NUMBER_OF_GAMEPAD_BUTTONS; button++)
		{
			if (state->buttons[button])
				return 1;
		}
		if (state->sticks[_gamepad_stick_left].x > AE_STICK_THRESHOLD ||
			state->sticks[_gamepad_stick_left].x < -AE_STICK_THRESHOLD ||
			state->sticks[_gamepad_stick_left].y > AE_STICK_THRESHOLD ||
			state->sticks[_gamepad_stick_left].y < -AE_STICK_THRESHOLD)
		{
			return 1;
		}
	}
	return ae_platform_keys() != 0;
}

void ae_input_poll(
	void)
{
	struct event_record event;
	unsigned long now = system_milliseconds();
	int keys = ae_platform_keys();
	int pressed = keys & ~ae_input.keys;
	short controller;

	/* (a poll after a gap, as when a screen opens: nothing held counts from before) */
	if (!ae_input.polled || now - ae_input.last_poll > AE_POLL_GAP_MS)
		memset(ae_input.repeats, 0, sizeof(ae_input.repeats));
	ae_input.polled = 1;
	ae_input.last_poll = now;
	ae_input.keys = keys;

	while (ae_ui_depth() && get_next_event(&event, NONE))
	{
		int action;

		if (event.type != AE_EVENT_TYPE_BUTTON || event.data.button.value != 1 || event.controller_index < 0 ||
			event.controller_index >= MAXIMUM_CONTROLLERS)
		{
			continue;
		}
		if (event.controller_index == 0 && (keys & AE_KEY_E) && event.data.button.index == _gamepad_analog_button_x)
			continue;
		action = button_action(event.data.button.index);
		if (action != AE_ACTION_NONE)
			send_action(event.controller_index, action);
	}
	if (ae_ui_depth() && (pressed & AE_KEY_Q))
		send_action(0, AE_ACTION_TAB_PREVIOUS);
	if (ae_ui_depth() && (pressed & AE_KEY_E))
		send_action(0, AE_ACTION_TAB_NEXT);
	if (ae_ui_depth() && (pressed & AE_KEY_PAGE_UP))
		send_action(0, AE_ACTION_PAGE_UP);
	if (ae_ui_depth() && (pressed & AE_KEY_PAGE_DOWN))
		send_action(0, AE_ACTION_PAGE_DOWN);
	for (controller = 0; controller < MAXIMUM_CONTROLLERS && ae_ui_depth(); controller++)
	{
		int held[NUMBER_OF_DIRECTIONS];
		int direction;

		held_directions(controller, held);
		for (direction = 0; direction < NUMBER_OF_DIRECTIONS && ae_ui_depth(); direction++)
		{
			if (ae_repeat_update(&ae_input.repeats[controller][direction], held[direction], now))
				send_action(controller, direction_actions[direction]);
		}
	}
	/* (whatever is left, as when a screen closed partway: the menus behind take none of it) */
	event_manager_flush();
}

void ae_input_pointer(
	struct halo_ui_pointer const *pointer)
{
	struct ae_pointer moved, clicked;

	if (!pointer)
		return;
	if (pointer->moved || pointer->wheel_steps)
	{
		memset(&moved, 0, sizeof(moved));
		ae_draw_pointer_to_layout(pointer->x, pointer->y, &moved.x, &moved.y);
		moved.moved = pointer->moved;
		moved.wheel_steps = pointer->wheel_steps;
		moved.touch = pointer->touch;
		ae_ui_dispatch_pointer(&moved);
	}
	if (pointer->left_clicks && ae_ui_depth())
	{
		memset(&clicked, 0, sizeof(clicked));
		ae_draw_pointer_to_layout(pointer->click_x, pointer->click_y, &clicked.x, &clicked.y);
		clicked.left_clicks = pointer->left_clicks;
		clicked.touch = pointer->touch;
		ae_ui_dispatch_pointer(&clicked);
	}
	if (pointer->right_clicks && ae_ui_depth())
	{
		struct ae_event event;

		event.player = 0;
		event.action = AE_ACTION_BACK;
		event.device = AE_DEVICE_KEYBOARD_MOUSE;
		ae_ui_dispatch(&event);
	}
}
