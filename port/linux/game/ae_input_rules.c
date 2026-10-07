/* ae_input_rules.c: Arena Evolved menus, the input bridge's rules (see ae_input_rules.h). No engine includes. */

#include "../src/ae_platform.h"
#include "ae_input_rules.h"

short ae_input_player_of_controller(short controller, short const bindings[AE_MAXIMUM_PLAYERS])
{
	int player, any = 0;

	if (controller < 0 || controller >= AE_INPUT_CONTROLLERS)
		return AE_PLAYER_NONE;
	for (player = 0; player < AE_MAXIMUM_PLAYERS; player++)
	{
		if (bindings[player] == controller)
			return (short)player;
		if (bindings[player] >= 0)
			any = 1;
	}
	return any ? AE_PLAYER_NONE : controller;
}

void ae_input_key_directions(int keys, int held[4])
{
	int tab = (keys & AE_KEY_TAB) != 0;
	int shift = (keys & AE_KEY_SHIFT) != 0;

	held[0] = (tab && shift) || (keys & AE_KEY_UP) != 0;
	held[1] = (tab && !shift) || (keys & AE_KEY_DOWN) != 0;
	held[2] = (keys & AE_KEY_LEFT) != 0;
	held[3] = (keys & AE_KEY_RIGHT) != 0;
}

int ae_input_key_actions(int keys, int previous, int back_presses, unsigned char actions[], int maximum)
{
	static struct { int key; unsigned char action; } const presses[] =
	{
		{ AE_KEY_Q, AE_ACTION_TAB_PREVIOUS },
		{ AE_KEY_E, AE_ACTION_TAB_NEXT },
		{ AE_KEY_PAGE_UP, AE_ACTION_PAGE_UP },
		{ AE_KEY_PAGE_DOWN, AE_ACTION_PAGE_DOWN },
	};
	int pressed = keys & ~previous;
	int count = 0, index;

	for (index = 0; index < (int)(sizeof(presses) / sizeof(presses[0])) && count < maximum; index++)
	{
		if (pressed & presses[index].key)
			actions[count++] = presses[index].action;
	}
	for (index = 0; index < back_presses && count < maximum; index++)
		actions[count++] = AE_ACTION_BACK;
	return count;
}

int ae_input_key_translate(int keys, int action, int keyboard)
{
	if (action == AE_ACTION_X && (keys & AE_KEY_E))
		return AE_ACTION_NONE;
	if (action == AE_ACTION_Y && (keys & AE_KEY_TAB))
		return AE_ACTION_NONE;
	if (action == AE_ACTION_Y && keyboard)
		return keys & AE_KEY_SHIFT ? AE_ACTION_UP : AE_ACTION_DOWN;
	return action;
}

int ae_input_direction_step(struct ae_repeat *repeat, int held, unsigned long now_ms, int opening, int stalled)
{
	if (opening || (stalled && repeat->held && held))
	{
		ae_repeat_seed(repeat, held, now_ms);
		return 0;
	}
	return ae_repeat_update(repeat, held, now_ms);
}

void ae_hold_begin(struct ae_hold *hold, unsigned int const held[AE_INPUT_HELD_MASKS], unsigned long now_ms)
{
	int index;

	hold->since = now_ms;
	hold->active = 0;
	for (index = 0; index < AE_INPUT_HELD_MASKS; index++)
	{
		hold->held[index] = held[index];
		if (held[index])
			hold->active = 1;
	}
}

int ae_hold_update(struct ae_hold *hold, unsigned int const held[AE_INPUT_HELD_MASKS], unsigned long now_ms)
{
	int index, any = 0;

	if (!hold->active)
		return 0;
	/* (an input let go of is free from then: pressed again, it is the menus') */
	for (index = 0; index < AE_INPUT_HELD_MASKS; index++)
	{
		hold->held[index] &= held[index];
		if (hold->held[index])
			any = 1;
	}
	if (!any || now_ms - hold->since >= AE_INPUT_HOLD_MAXIMUM_MS)
		hold->active = 0;
	return hold->active;
}
