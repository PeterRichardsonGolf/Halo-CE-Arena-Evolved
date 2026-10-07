#include <stdio.h>
#include "../src/ae_platform.h"
#include "ae_input_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	short none[4] = { -1, -1, -1, -1 };
	short split[4] = { 2, 0, -1, -1 };
	int held[4];
	unsigned char actions[8];
	unsigned int masks[AE_INPUT_HELD_MASKS] = { 0, 0, 0, 0, 0 };
	unsigned int now[AE_INPUT_HELD_MASKS] = { 0, 0, 0, 0, 0 };
	struct ae_hold hold;

	/* controllers to players: nobody bound yet, controller c is player c; else the game's bindings */
	CHECK(ae_input_player_of_controller(0, none) == 0 && ae_input_player_of_controller(3, none) == 3);
	CHECK(ae_input_player_of_controller(2, split) == 0);     /* player 1 plays on the third pad */
	CHECK(ae_input_player_of_controller(0, split) == 1);
	CHECK(ae_input_player_of_controller(1, split) == AE_PLAYER_NONE);
	CHECK(ae_input_player_of_controller(-1, none) == AE_PLAYER_NONE);
	CHECK(ae_input_player_of_controller(4, none) == AE_PLAYER_NONE);

	/* Tab steps down, Shift+Tab up; Shift alone nothing */
	ae_input_key_directions(AE_KEY_TAB, held);
	CHECK(!held[0] && held[1] && !held[2] && !held[3]);
	ae_input_key_directions(AE_KEY_TAB | AE_KEY_SHIFT, held);
	CHECK(held[0] && !held[1]);
	ae_input_key_directions(AE_KEY_SHIFT | AE_KEY_Q, held);
	CHECK(!held[0] && !held[1]);

	/* key presses (not holds) are actions; mouse button 4 is BACK */
	CHECK(ae_input_key_actions(AE_KEY_Q, 0, 0, actions, 8) == 1 && actions[0] == AE_ACTION_TAB_PREVIOUS);
	CHECK(ae_input_key_actions(AE_KEY_Q, AE_KEY_Q, 0, actions, 8) == 0);
	CHECK(ae_input_key_actions(AE_KEY_E | AE_KEY_PAGE_DOWN, AE_KEY_PAGE_DOWN, 0, actions, 8) == 1 &&
		actions[0] == AE_ACTION_TAB_NEXT);
	CHECK(ae_input_key_actions(AE_KEY_PAGE_UP | AE_KEY_PAGE_DOWN, 0, 0, actions, 8) == 2 &&
		actions[0] == AE_ACTION_PAGE_UP && actions[1] == AE_ACTION_PAGE_DOWN);
	CHECK(ae_input_key_actions(0, 0, 2, actions, 8) == 2 && actions[0] == AE_ACTION_BACK && actions[1] == AE_ACTION_BACK);
	CHECK(ae_input_key_actions(AE_KEY_Q | AE_KEY_E, 0, 3, actions, 2) == 2);   /* never past maximum */
	CHECK(ae_input_key_actions(AE_KEY_TAB | AE_KEY_SHIFT, 0, 0, actions, 8) == 0); /* Tab is a direction */

	/* the game's menu-key mapping's X (E) and Y (Tab) are dropped while those keys are held */
	CHECK(ae_input_key_drops(AE_KEY_E, AE_ACTION_X) && !ae_input_key_drops(0, AE_ACTION_X));
	CHECK(ae_input_key_drops(AE_KEY_TAB, AE_ACTION_Y) && !ae_input_key_drops(AE_KEY_TAB, AE_ACTION_X));
	CHECK(!ae_input_key_drops(AE_KEY_E | AE_KEY_TAB, AE_ACTION_ACCEPT));

	/* the hold-back: nothing held at the close, none */
	ae_hold_begin(&hold, masks, 1000);
	CHECK(!hold.active && !ae_hold_update(&hold, now, 1001));
	/* B held on pad 1 and Down on pad 2 at the close: held back until each is let go of */
	masks[0] = 1 << 1; masks[1] = AE_INPUT_STICK_DOWN;
	ae_hold_begin(&hold, masks, 1000);
	now[0] = 1 << 1; now[1] = AE_INPUT_STICK_DOWN;
	CHECK(ae_hold_update(&hold, now, 1100));
	now[0] = 0;                                              /* B let go of */
	CHECK(ae_hold_update(&hold, now, 1200));
	now[0] = 1 << 1;                                         /* pressed again: the menus', not held back */
	now[1] = 0;
	CHECK(!ae_hold_update(&hold, now, 1300));
	/* a new button pressed after the close doesn't extend it */
	masks[0] = 1; masks[1] = 0; now[0] = 1 | (1 << 3); now[1] = 0;
	ae_hold_begin(&hold, masks, 5000);
	now[0] = 1 << 3;
	CHECK(!ae_hold_update(&hold, now, 5010));
	/* a stuck button: held back at most 2 s */
	masks[0] = 0; masks[4] = AE_KEY_Q; now[0] = 0; now[4] = AE_KEY_Q;
	ae_hold_begin(&hold, masks, 10000);
	CHECK(ae_hold_update(&hold, now, 11999));
	CHECK(!ae_hold_update(&hold, now, 12000));
	CHECK(!ae_hold_update(&hold, now, 12500));
	if (failures) return 1;
	printf("ae_input_rules: ok\n");
	return 0;
}
