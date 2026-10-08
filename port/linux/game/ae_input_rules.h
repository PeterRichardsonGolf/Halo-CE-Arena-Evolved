/*
AE_INPUT_RULES.H

The input bridge's rules (ae_input_rules.c), pure C with no engine includes so
the unit tests build them alone (port/linux/tests/ae_input_rules_test.c):
which local player a controller drives, what AE's own keys do, and how long
input is held back from the game's menus after the last AE screen closes.
*/

#ifndef __AE_INPUT_RULES_H
#define __AE_INPUT_RULES_H

#include "ae_ui.h"

enum
{
	AE_INPUT_CONTROLLERS = 4,
	/* the held-input masks: one per controller (buttons 0-15, the left stick's directions 16-19), then the keys */
	AE_INPUT_HELD_MASKS = AE_INPUT_CONTROLLERS + 1,
	AE_INPUT_STICK_UP = 1 << 16,
	AE_INPUT_STICK_DOWN = 1 << 17,
	AE_INPUT_STICK_LEFT = 1 << 18,
	AE_INPUT_STICK_RIGHT = 1 << 19,
	/* the hold-back's limit: a stuck button can't keep the game's menus from their input longer */
	AE_INPUT_HOLD_MAXIMUM_MS = 2000,
	/* the most of Tab's counted presses one poll steps (a stall can't release a burst) */
	AE_INPUT_TAB_STEPS_MAXIMUM = 4
};

/* the local player a controller drives. bindings[p] is the controller of local player p (the game's: player_ui's
single-player controllers; negative: none). A controller no player has is AE_PLAYER_NONE, except while no player has
any controller (the menus before anyone is bound), when controller c is player c. */
short ae_input_player_of_controller(short controller, short const bindings[AE_MAXIMUM_PLAYERS]);

/* AE's own keys (ae_platform.h AE_KEY_*) as directions held, up, down, left, right: Tab steps down, Shift+Tab up;
the keypad's arrows (Num Lock off) as the arrows */
void ae_input_key_directions(int keys, int held[4]);
/* the actions of the keys pressed since previous (Q, E: tabs; Page Up, Page Down) and of mouse button 4's presses
(BACK); the count written, at most maximum */
int ae_input_key_actions(int keys, int previous, int back_presses, unsigned char actions[], int maximum);
/* the keys this poll acts on and the keys it compares them with. raw: AE's keys held now; typing: a field is being
typed into (it takes the keys, so the acted-on keys are none); stored_raw: the raw keys of the last poll (kept raw
even while typing, so a key still held when typing ends is not a new press); opening: the first poll of a screen */
void ae_input_poll_keys(int raw, int typing, int stored_raw, int opening, int *keys, int *previous_keys);
/* the action to send for a first-controller button's action, given AE's keys held now, the Tab presses counted since
the last poll (ae_platform_take_tab_presses: key downs, so a tap let go of within one frame counts too) and whether
the keyboard is the device last used: while E is held, its X (E drives X in the game's menu keys) is none; Tab drives
Y there, so while Tab is held Y is none, and a keyboard Y with a Tab press counted is none too (Tab's steps come from
the held directions and ae_input_tab_steps); any other Y is a real Y (a pad on the first controller after keyboard or
mouse use, a touch Y; M1 review M2, Task 4 fix round I1) */
int ae_input_key_translate(int keys, int tab_presses, int action, int keyboard);
/* Tab's focus steps from its presses counted since the last poll, forward (down) and backward (up: Shift held at
the press), beyond the step the held directions give: a press still held at this poll that was not at the last
(previous_keys) is the held direction's step (and its repeat), the others (taps let go of between the polls, two in
one frame are two) step here, at most AE_INPUT_TAB_STEPS_MAXIMUM in all (forward first) */
void ae_input_tab_steps(int forward, int backward, int keys, int previous_keys, int *down, int *up);

/* a held direction this poll: whether it steps (AE's repeat). opening: the first poll of a screen just opened: what
is held then doesn't step (it is seeded as held). stalled: the last poll was long ago (a stall, a very low frame
rate): a direction held then and still held doesn't step either (no burst), but one pressed since steps as any
press does */
int ae_input_direction_step(struct ae_repeat *repeat, int held, unsigned long now_ms, int opening, int stalled);

/* the hold-back after the last screen closes: the inputs held then reach the game's menus only once let go of (each
on its own), and nothing is held back longer than AE_INPUT_HOLD_MAXIMUM_MS; inputs pressed after the close are not
held back */
struct ae_hold { unsigned long since; unsigned int held[AE_INPUT_HELD_MASKS]; int active; };
void ae_hold_begin(struct ae_hold *hold, unsigned int const held[AE_INPUT_HELD_MASKS], unsigned long now_ms);
/* nonzero while holding back */
int ae_hold_update(struct ae_hold *hold, unsigned int const held[AE_INPUT_HELD_MASKS], unsigned long now_ms);

#endif
