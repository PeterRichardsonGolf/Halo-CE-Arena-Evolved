/*
AE_CUSTOM_LAYOUT_RULES.C

CUSTOM LAYOUT's rules (ae_custom_layout_rules.h).
*/

#include "ae_custom_layout_rules.h"

#include <stddef.h>

const int ae_custom_layout_actions[AE_CUSTOM_LAYOUT_ROWS] = { CONTROLLER_ACTION_JUMP, CONTROLLER_ACTION_FIRE,
	CONTROLLER_ACTION_GRENADE, CONTROLLER_ACTION_MELEE, CONTROLLER_ACTION_ACTION, CONTROLLER_ACTION_SWITCH_WEAPON,
	CONTROLLER_ACTION_SWITCH_GRENADE, CONTROLLER_ACTION_CROUCH, CONTROLLER_ACTION_ZOOM, CONTROLLER_ACTION_FLASHLIGHT };

int ae_custom_layout_changes(unsigned char const layout[CONTROLLER_ACTION_COUNT])
{
	unsigned char fresh[CONTROLLER_ACTION_COUNT];
	int row, count = 0;

	controller_layout_preset(CONTROLLER_PRESET_DEFAULT, NULL, fresh);
	for (row = 0; row < AE_CUSTOM_LAYOUT_ROWS; row++)
		count += layout[ae_custom_layout_actions[row]] != fresh[ae_custom_layout_actions[row]];
	return count;
}

void ae_custom_layout_wait_begin(struct ae_custom_layout_wait *wait, unsigned long now_ms)
{
	wait->started_ms = now_ms;
	wait->released = 0;
	wait->dpad_told = 0;
}

enum ae_custom_layout_wait_result ae_custom_layout_wait_step(struct ae_custom_layout_wait *wait, int has_pad,
	unsigned long buttons, int current_button, unsigned long now_ms, int *button)
{
	unsigned long const dpad = (1UL << CONTROLLER_BUTTON_DPAD_UP) | (1UL << CONTROLLER_BUTTON_DPAD_DOWN) |
		(1UL << CONTROLLER_BUTTON_DPAD_LEFT) | (1UL << CONTROLLER_BUTTON_DPAD_RIGHT);
	int each;

	if (!has_pad)
		return AE_CUSTOM_LAYOUT_UNPLUGGED;
	if (now_ms - wait->started_ms >= AE_CUSTOM_LAYOUT_WAIT_MS)
		return AE_CUSTOM_LAYOUT_TIMED_OUT;
	if (!wait->released)
	{
		wait->released = !buttons;
		return AE_CUSTOM_LAYOUT_WAITING;
	}
	if (buttons & ((1UL << CONTROLLER_BUTTON_START) | (1UL << CONTROLLER_BUTTON_BACK)))
		return AE_CUSTOM_LAYOUT_CANCELLED;
	for (each = 0; each < CONTROLLER_BUTTON_COUNT; each++)
	{
		if (!(buttons & (1UL << each)) || !controller_layout_movable_button(each))
			continue;
		*button = each;
		return each == current_button ? AE_CUSTOM_LAYOUT_SAME : AE_CUSTOM_LAYOUT_BOUND;
	}
	if (!(buttons & dpad))
	{
		wait->dpad_told = 0;
		return AE_CUSTOM_LAYOUT_WAITING;
	}
	if (wait->dpad_told)
		return AE_CUSTOM_LAYOUT_WAITING;
	wait->dpad_told = 1;
	return AE_CUSTOM_LAYOUT_DPAD;
}

int ae_custom_layout_wait_seconds(struct ae_custom_layout_wait const *wait, unsigned long now_ms)
{
	unsigned long gone = now_ms - wait->started_ms;

	if (gone >= AE_CUSTOM_LAYOUT_WAIT_MS)
		return 0;
	return (int)((AE_CUSTOM_LAYOUT_WAIT_MS - gone + 999UL) / 1000UL);
}
