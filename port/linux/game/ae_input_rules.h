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
	AE_INPUT_HOLD_MAXIMUM_MS = 2000
};

/* the local player a controller drives. bindings[p] is the controller of local player p (the game's: player_ui's
single-player controllers; negative: none). A controller no player has is AE_PLAYER_NONE, except while no player has
any controller (the menus before anyone is bound), when controller c is player c. */
short ae_input_player_of_controller(short controller, short const bindings[AE_MAXIMUM_PLAYERS]);

/* AE's own keys (ae_platform.h AE_KEY_*) as directions held, up, down, left, right: Tab steps down, Shift+Tab up */
void ae_input_key_directions(int keys, int held[4]);
/* the actions of the keys pressed since previous (Q, E: tabs; Page Up, Page Down) and of mouse button 4's presses
(BACK); the count written, at most maximum */
int ae_input_key_actions(int keys, int previous, int back_presses, unsigned char actions[], int maximum);
/* the action to send for a first-controller button's action, given AE's keys held and whether the keyboard is the
device last used: while E is held, its X (E drives X in the game's menu keys) is none; Tab drives Y there, so while
Tab is held Y is none (Tab's own focus step comes from the held directions), and a Y from the keyboard with Tab not
held is a Tab tapped between two polls: its focus step (down, up with Shift) */
int ae_input_key_translate(int keys, int action, int keyboard);

/* the hold-back after the last screen closes: the inputs held then reach the game's menus only once let go of (each
on its own), and nothing is held back longer than AE_INPUT_HOLD_MAXIMUM_MS; inputs pressed after the close are not
held back */
struct ae_hold { unsigned long since; unsigned int held[AE_INPUT_HELD_MASKS]; int active; };
void ae_hold_begin(struct ae_hold *hold, unsigned int const held[AE_INPUT_HELD_MASKS], unsigned long now_ms);
/* nonzero while holding back */
int ae_hold_update(struct ae_hold *hold, unsigned int const held[AE_INPUT_HELD_MASKS], unsigned long now_ms);

#endif
