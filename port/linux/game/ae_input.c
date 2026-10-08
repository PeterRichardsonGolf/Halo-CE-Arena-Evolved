/*
AE_INPUT.C

The AE menus' input bridge (ae_input.h). Its rules, pure and unit-tested,
are in ae_input_rules.c.

Buttons: the game's event queue (interface/event_manager.c), as the
virtual keyboard and the server browser read it. A held button's event comes
again every frame with a larger value, so a press is value 1. The keyboard
reaches it as the first controller (xinput_sdl.c keyboard_gamepad): Enter and
Space are A, Escape and Backspace are B, Delete (and E) X, Tab Y, C the black
button. AE's own keys are read from the platform (ae_platform.c): Q and E are
the tabs, Page Up and Page Down page, Tab and Shift+Tab step the focus as the
d-pad does; while E or Tab is held, the first controller's X or Y (their
game mapping) is dropped. Tab's presses are counted as SDL queues them
(ae_platform.c), so a tap let go of within one frame is a step too: a
keyboard Y with a Tab press counted is dropped, and each counted press the
held directions didn't see steps (ae_input_tab_steps); any other Y is a Y (a
pad's after the keyboard was used, a touch Y). Mouse button 4 is BACK: while the menus have
the pointer, sdl_platform.c keeps it from the controller and counts it for AE,
while an AE screen is open (ae_hooks.c arms the count).

Directions: each controller's d-pad and left stick, held (and Tab on the
first), with AE's own repeat (ae_repeat: 400 ms, then 80 ms, 40 ms after
1 s), not the event queue's 250 ms one. A direction already held when a
screen opens, or held through a stall, doesn't step until pressed again; a
low frame rate drops no press.

Players: an event's player is the local player its controller drives, by the
game's bindings (player_ui's single-player controllers; ae_input_rules.c).
The pointer is the first controller's player's.

Devices: the first controller's input shows the device last used
(platform_input_scheme: the keyboard and mouse, or a pad's kind). The other
controllers' show the same kind when it is a pad's, else Xbox's: the
platform doesn't tell one pad's kind from another's (M7 may need it).

Everything read is consumed: the queue is flushed every frame a screen is
open, so no button pressed on an AE screen reaches the game's menus behind it
when the screen closes; after the last screen closes, the inputs held then
are held back until each is let go of, for at most 2 s (ae_hooks.c).
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
#include "ae_input_rules.h"
#include "ae_ui.h"

/* interface/player_ui.c: the controller a local player plays with (NONE: none) */
short player_ui_get_single_player_local_player_controller(short local_player_index);

enum
{
	/* (event_manager.c's event types, which it keeps to itself: _event_type_button; browser_screen.c keeps a copy
	too; tools/test_ae_hooks.py checks they agree) */
	AE_EVENT_TYPE_BUTTON = 3,

	/* a stick this far over is a direction held */
	AE_STICK_THRESHOLD = 16384,
	/* a poll this long after the last one: a stall (a direction held through it doesn't step) */
	AE_POLL_GAP_MS = 250,
	NUMBER_OF_DIRECTIONS = 4,
	MAXIMUM_ACTIONS = 16
};

static struct
{
	struct ae_repeat repeats[AE_INPUT_CONTROLLERS][NUMBER_OF_DIRECTIONS];
	unsigned long last_poll;
	int polled;
	int keys;
	int opening;
	struct ae_hold hold;
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

static short player_of(
	short controller)
{
	short bindings[AE_MAXIMUM_PLAYERS];
	short player;

	for (player = 0; player < AE_MAXIMUM_PLAYERS; player++)
		bindings[player] = player_ui_get_single_player_local_player_controller(player);
	return ae_input_player_of_controller(controller, bindings);
}

/* (repeat: a held direction's repeat step, not its press) */
static void send_step(
	short controller,
	int action,
	int repeat)
{
	struct ae_event event;

	event.player = player_of(controller);
	event.action = (unsigned char)action;
	event.device = device_of(controller);
	event.repeat = (unsigned char)(repeat != 0);
	ae_ui_dispatch(&event);
}

static void send_action(
	short controller,
	int action)
{
	send_step(controller, action, 0);
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

/* which directions a controller holds (the first: Tab and Shift+Tab too) */
static void held_directions(
	short controller,
	int keys,
	int held[NUMBER_OF_DIRECTIONS])
{
	struct gamepad_state const *state = controller_state(controller);
	int direction;

	held[0] = held[1] = held[2] = held[3] = 0;
	if (controller == 0)
		ae_input_key_directions(keys, held);
	if (!state)
		return;
	held[0] |= state->buttons[_gamepad_binary_button_dpad_up] || state->sticks[_gamepad_stick_left].y > AE_STICK_THRESHOLD;
	held[1] |= state->buttons[_gamepad_binary_button_dpad_down] ||
		state->sticks[_gamepad_stick_left].y < -AE_STICK_THRESHOLD;
	held[2] |= state->buttons[_gamepad_binary_button_dpad_left] ||
		state->sticks[_gamepad_stick_left].x < -AE_STICK_THRESHOLD;
	held[3] |= state->buttons[_gamepad_binary_button_dpad_right] ||
		state->sticks[_gamepad_stick_left].x > AE_STICK_THRESHOLD;
	for (direction = 0; direction < NUMBER_OF_DIRECTIONS; direction++)
		held[direction] = held[direction] != 0;
}

/* every controller's held buttons and stick directions, and the keys (ae_input_rules.h's masks) */
static void held_masks(
	unsigned int masks[AE_INPUT_HELD_MASKS])
{
	short controller;

	for (controller = 0; controller < AE_INPUT_CONTROLLERS; controller++)
	{
		struct gamepad_state const *state = controller_state(controller);
		short button;

		masks[controller] = 0;
		if (!state)
			continue;
		for (button = 0; button < NUMBER_OF_GAMEPAD_BUTTONS && button < 16; button++)
		{
			if (state->buttons[button])
				masks[controller] |= 1u << button;
		}
		if (state->sticks[_gamepad_stick_left].y > AE_STICK_THRESHOLD)
			masks[controller] |= AE_INPUT_STICK_UP;
		if (state->sticks[_gamepad_stick_left].y < -AE_STICK_THRESHOLD)
			masks[controller] |= AE_INPUT_STICK_DOWN;
		if (state->sticks[_gamepad_stick_left].x < -AE_STICK_THRESHOLD)
			masks[controller] |= AE_INPUT_STICK_LEFT;
		if (state->sticks[_gamepad_stick_left].x > AE_STICK_THRESHOLD)
			masks[controller] |= AE_INPUT_STICK_RIGHT;
	}
	masks[AE_INPUT_CONTROLLERS] = (unsigned int)ae_platform_keys();
}

void ae_input_hold_begin(
	void)
{
	unsigned int masks[AE_INPUT_HELD_MASKS];

	held_masks(masks);
	ae_hold_begin(&ae_input.hold, masks, system_milliseconds());
	/* (a button 4 press counted while a screen closed is not the menus') */
	ae_platform_take_back_presses();
}

void ae_input_screen_opened(
	void)
{
	ae_input.opening = 1;
}

int ae_input_holding(
	void)
{
	unsigned int masks[AE_INPUT_HELD_MASKS];

	if (!ae_input.hold.active)
		return 0;
	held_masks(masks);
	return ae_hold_update(&ae_input.hold, masks, system_milliseconds());
}

void ae_input_poll(
	void)
{
	struct event_record event;
	unsigned long now = system_milliseconds();
	int keys = ae_platform_keys();
	int previous_keys, tab_forward, tab_backward, tab_ignored, tab_down, tab_up;
	unsigned char actions[MAXIMUM_ACTIONS];
	int action_count, index, back_presses;
	short controller;
	/* (a screen just opened: keys held then are not pressed now, and no button 4 press counted before is; a stall,
	a low frame rate, drops no press: only directions held through it wait for a new press) */
	boolean opening = ae_input.opening || !ae_input.polled;
	boolean stalled = now - ae_input.last_poll > AE_POLL_GAP_MS;

	ae_input.opening = 0;
	previous_keys = opening ? keys : ae_input.keys;
	/* (Tab's presses since the last poll; a screen just opened takes none from before) */
	ae_platform_take_tab_presses(&tab_forward, &tab_backward, &tab_ignored);
	if (opening)
		tab_forward = tab_backward = tab_ignored = 0;
	back_presses = ae_platform_take_back_presses();
	action_count = ae_input_key_actions(keys, previous_keys, opening ? 0 : back_presses,
		actions, MAXIMUM_ACTIONS);
	ae_input.polled = 1;
	ae_input.last_poll = now;
	ae_input.keys = keys;

	while (ae_ui_depth() && get_next_event(&event, NONE))
	{
		int action;

		if (event.type != AE_EVENT_TYPE_BUTTON || event.data.button.value != 1 || event.controller_index < 0 ||
			event.controller_index >= AE_INPUT_CONTROLLERS)
		{
			continue;
		}
		action = button_action(event.data.button.index);
		if (event.controller_index == 0)
			/* (an Alt+Tab steps nothing, but its Y is dropped as well) */
			action = ae_input_key_translate(keys, tab_forward + tab_backward + tab_ignored, action,
				device_of(0) == AE_DEVICE_KEYBOARD_MOUSE);
		if (action != AE_ACTION_NONE)
			send_action(event.controller_index, action);
	}
	for (index = 0; index < action_count && ae_ui_depth(); index++)
		send_action(0, actions[index]);
	/* (Tab's taps the held directions below don't see: one step each) */
	ae_input_tab_steps(tab_forward, tab_backward, keys, previous_keys, &tab_down, &tab_up);
	for (index = 0; index < tab_down && ae_ui_depth(); index++)
		send_action(0, AE_ACTION_DOWN);
	for (index = 0; index < tab_up && ae_ui_depth(); index++)
		send_action(0, AE_ACTION_UP);
	for (controller = 0; controller < AE_INPUT_CONTROLLERS && ae_ui_depth(); controller++)
	{
		int held[NUMBER_OF_DIRECTIONS];
		int direction;

		held_directions(controller, keys, held);
		for (direction = 0; direction < NUMBER_OF_DIRECTIONS && ae_ui_depth(); direction++)
		{
			struct ae_repeat *repeat = &ae_input.repeats[controller][direction];

			/* (a step whose press began now is the press, any later one a repeat) */
			if (ae_input_direction_step(repeat, held[direction], now, opening, stalled))
				send_step(controller, direction_actions[direction], repeat->since != now);
		}
	}
	/* (whatever is left, as when a screen closed partway: the menus behind take none of it) */
	event_manager_flush();
}

void ae_input_pointer(
	struct halo_ui_pointer const *pointer)
{
	struct ae_pointer moved, clicked;
	short player = player_of(0);

	if (!pointer)
		return;
	if (pointer->moved || pointer->wheel_steps)
	{
		memset(&moved, 0, sizeof(moved));
		ae_draw_pointer_to_layout(pointer->x, pointer->y, &moved.x, &moved.y);
		moved.moved = pointer->moved;
		moved.wheel_steps = pointer->wheel_steps;
		moved.touch = pointer->touch;
		moved.player = player;
		ae_ui_dispatch_pointer(&moved);
	}
	if (pointer->left_clicks && ae_ui_depth())
	{
		memset(&clicked, 0, sizeof(clicked));
		ae_draw_pointer_to_layout(pointer->click_x, pointer->click_y, &clicked.x, &clicked.y);
		clicked.left_clicks = pointer->left_clicks;
		clicked.touch = pointer->touch;
		clicked.player = player;
		ae_ui_dispatch_pointer(&clicked);
	}
	if (pointer->right_clicks && ae_ui_depth())
	{
		struct ae_event event;

		event.player = player;
		event.action = AE_ACTION_BACK;
		event.device = (unsigned char)(pointer->touch ? device_of(0) : AE_DEVICE_KEYBOARD_MOUSE);
		event.repeat = 0;
		ae_ui_dispatch(&event);
	}
}
