/*
AE_CUSTOM_LAYOUT_RULES.H

CUSTOM LAYOUT's rules (ae_custom_layout_rules.c), pure C with no engine includes so the unit tests build them alone
(port/linux/tests/ae_custom_layout_rules_test.c): the rows' order, what counts as changed, and the wait for a press.
*/

#ifndef __AE_CUSTOM_LAYOUT_RULES_H
#define __AE_CUSTOM_LAYOUT_RULES_H

#include "../src/controller_layout.h"

enum
{
	AE_CUSTOM_LAYOUT_ROWS = 10,
	/* how long a row waits for a press */
	AE_CUSTOM_LAYOUT_WAIT_MS = 8000,
	/* room for the longest layout's text, its end too (the test checks it; AE_PLAYER_SETTINGS_VALUE holds as much) */
	AE_CUSTOM_LAYOUT_TEXT = 160
};

/* the actions that move, in the order they are played with */
extern const int ae_custom_layout_actions[AE_CUSTOM_LAYOUT_ROWS];

/* how many actions are not on their DEFAULT button */
int ae_custom_layout_changes(unsigned char const layout[CONTROLLER_ACTION_COUNT]);

/* the wait for a press: begun with the press that opened it possibly still down, which must be let go first */
struct ae_custom_layout_wait
{
	unsigned long started_ms;
	int released;
	int dpad_told;
};
enum ae_custom_layout_wait_result
{
	AE_CUSTOM_LAYOUT_WAITING,
	AE_CUSTOM_LAYOUT_BOUND,      /* *button is the action's new button */
	AE_CUSTOM_LAYOUT_SAME,       /* the button the action already has: nothing changes */
	AE_CUSTOM_LAYOUT_CANCELLED,  /* START or BACK */
	AE_CUSTOM_LAYOUT_TIMED_OUT,
	AE_CUSTOM_LAYOUT_UNPLUGGED,
	AE_CUSTOM_LAYOUT_DPAD        /* still waiting: once a press of the d-pad, which can't take an action */
};
void ae_custom_layout_wait_begin(struct ae_custom_layout_wait *wait, unsigned long now_ms);
/* a frame of the wait. buttons: the pad's buttons down, a bit for each CONTROLLER_BUTTON_* */
enum ae_custom_layout_wait_result ae_custom_layout_wait_step(struct ae_custom_layout_wait *wait, int has_pad,
	unsigned long buttons, int current_button, unsigned long now_ms, int *button);
/* the whole seconds left, rounded up (8 at the start, 1 at the end) */
int ae_custom_layout_wait_seconds(struct ae_custom_layout_wait const *wait, unsigned long now_ms);

#endif
