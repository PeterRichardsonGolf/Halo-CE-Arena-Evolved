/*
AE_EXTRA_BUTTONS_RULES.C

A Steam Controller's EXTRA BUTTONS screen's words (ae_extra_buttons_rules.h).
*/

#include <stdio.h>

#include "ae_extra_buttons_rules.h"

static const struct { const char *key, *label, *help; int group; } rows[AE_EXTRA_BUTTONS_ROWS] =
{
	{ "input.back_left", "L4 (UPPER LEFT)", "What L4 presses: the upper button on the back of the left grip.", 0 },
	{ "input.back_right", "R4 (UPPER RIGHT)", "What R4 presses: the upper button on the back of the right grip.", 0 },
	{ "input.back_left_2", "L5 (LOWER LEFT)", "What L5 presses: the lower button on the back of the left grip.", 0 },
	{ "input.back_right_2", "R5 (LOWER RIGHT)", "What R5 presses: the lower button on the back of the right grip.", 0 },
	{ "input.back_trackpad", "RIGHT TRACKPAD CLICK",
		"What the right trackpad's click presses (push the pad in until it clicks). Trackpad aim keeps working.", 1 },
	{ "input.back_steam", "STEAM BUTTON", "What the Steam button presses. With Steam open, it opens Steam too.", 1 },
	{ "input.back_quick", "QUICK ACCESS (...)",
		"What the quick access (...) button presses. With Steam open, it opens Steam's menu too.", 1 },
};

const char *ae_extra_buttons_key(int row)
{
	return row >= 0 && row < AE_EXTRA_BUTTONS_ROWS ? rows[row].key : "";
}

const char *ae_extra_buttons_label(int row)
{
	return row >= 0 && row < AE_EXTRA_BUTTONS_ROWS ? rows[row].label : "";
}

const char *ae_extra_buttons_help(int row)
{
	return row >= 0 && row < AE_EXTRA_BUTTONS_ROWS ? rows[row].help : "";
}

int ae_extra_buttons_group(int row)
{
	return row >= 0 && row < AE_EXTRA_BUTTONS_ROWS ? rows[row].group : 0;
}

const char *ae_extra_buttons_chip(int row)
{
	static const char *const chips[AE_EXTRA_BUTTONS_ROWS] = { "L4", "R4", "L5", "R5", "R PAD", "", "" };

	return row >= 0 && row < AE_EXTRA_BUTTONS_ROWS ? chips[row] : "";
}

const char *ae_extra_buttons_group_name(int group)
{
	return group == 1 ? "FRONT" : "BACK";
}

void ae_extra_buttons_summary(const int set[AE_EXTRA_BUTTONS_ROWS], char *text, size_t size)
{
	int row, count = 0;

	for (row = 0; row < AE_EXTRA_BUTTONS_ROWS; row++)
		count += set[row] != 0;
	if (count)
		snprintf(text, size, "%d SET", count);
	else
		snprintf(text, size, "NONE");
}

void ae_extra_buttons_keep_body(const char *name, char *text, size_t size)
{
	snprintf(text, size, "EXTRA BUTTONS changed for %s. They are in use already.", name);
}

void ae_extra_buttons_restore_body(const char *name, char *text, size_t size)
{
	snprintf(text, size, "All seven for %s go back to NONE. You can still undo it as you leave.", name);
}
