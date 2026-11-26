#include <stdio.h>
#include <string.h>
#include "ae_extra_buttons_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	int none[AE_EXTRA_BUTTONS_ROWS] = { 0 }, two[AE_EXTRA_BUTTONS_ROWS] = { 0, 0, 0, 0, 0, 1, 1 }, all[AE_EXTRA_BUTTONS_ROWS] = { 1, 1, 1, 1, 1, 1, 1 };
	char text[200];
	int row, back = 0, front = 0;

	/* the seven rows: the keys in the order the settings have them, the labels and the helps as designed */
	CHECK(AE_EXTRA_BUTTONS_ROWS == 7 && AE_EXTRA_BUTTONS_BACK + AE_EXTRA_BUTTONS_FRONT == AE_EXTRA_BUTTONS_ROWS);
	CHECK(!strcmp(ae_extra_buttons_key(0), "input.back_left") && !strcmp(ae_extra_buttons_key(1), "input.back_right") &&
		!strcmp(ae_extra_buttons_key(2), "input.back_left_2") && !strcmp(ae_extra_buttons_key(3), "input.back_right_2") &&
		!strcmp(ae_extra_buttons_key(4), "input.back_trackpad") && !strcmp(ae_extra_buttons_key(5), "input.back_steam") &&
		!strcmp(ae_extra_buttons_key(6), "input.back_quick"));
	CHECK(!strcmp(ae_extra_buttons_label(0), "L4 (UPPER LEFT)") && !strcmp(ae_extra_buttons_label(1), "R4 (UPPER RIGHT)") &&
		!strcmp(ae_extra_buttons_label(2), "L5 (LOWER LEFT)") && !strcmp(ae_extra_buttons_label(3), "R5 (LOWER RIGHT)") &&
		!strcmp(ae_extra_buttons_label(4), "RIGHT TRACKPAD CLICK") && !strcmp(ae_extra_buttons_label(5), "STEAM BUTTON") &&
		!strcmp(ae_extra_buttons_label(6), "QUICK ACCESS (...)"));
	CHECK(!strcmp(ae_extra_buttons_help(0), "What L4 presses: the upper button on the back of the left grip."));
	CHECK(!strcmp(ae_extra_buttons_help(1), "What R4 presses: the upper button on the back of the right grip."));
	CHECK(!strcmp(ae_extra_buttons_help(2), "What L5 presses: the lower button on the back of the left grip."));
	CHECK(!strcmp(ae_extra_buttons_help(3), "What R5 presses: the lower button on the back of the right grip."));
	CHECK(!strcmp(ae_extra_buttons_help(4), "What the right trackpad's click presses (push the pad in until it clicks). Trackpad aim keeps working."));
	CHECK(!strcmp(ae_extra_buttons_help(5), "What the Steam button presses. With Steam open, it opens Steam too."));
	CHECK(!strcmp(ae_extra_buttons_help(6), "What the quick access (...) button presses. With Steam open, it opens Steam's menu too."));
	CHECK(!strcmp(ae_extra_buttons_key(7), "") && !strcmp(ae_extra_buttons_label(-1), "") && !strcmp(ae_extra_buttons_help(9), ""));
	/* the groups: BACK 4, FRONT 3 */
	for (row = 0; row < AE_EXTRA_BUTTONS_ROWS; row++)
	{
		back += ae_extra_buttons_group(row) == 0;
		front += ae_extra_buttons_group(row) == 1;
	}
	CHECK(back == AE_EXTRA_BUTTONS_BACK && front == AE_EXTRA_BUTTONS_FRONT && ae_extra_buttons_group(3) == 0 && ae_extra_buttons_group(4) == 1);
	CHECK(!strcmp(ae_extra_buttons_group_name(0), "BACK") && !strcmp(ae_extra_buttons_group_name(1), "FRONT"));
	/* the key cap text where no picture is drawn */
	CHECK(!strcmp(ae_extra_buttons_chip(4), "R PAD") && !strcmp(ae_extra_buttons_chip(0), "L4") && !strcmp(ae_extra_buttons_chip(5), "") &&
		!strcmp(ae_extra_buttons_chip(7), ""));
	/* the summary: NONE, or how many of the seven are set */
	ae_extra_buttons_summary(none, text, sizeof(text));
	CHECK(!strcmp(text, "NONE"));
	ae_extra_buttons_summary(two, text, sizeof(text));
	CHECK(!strcmp(text, "2 SET"));
	ae_extra_buttons_summary(all, text, sizeof(text));
	CHECK(!strcmp(text, "7 SET"));
	/* the dialogs */
	ae_extra_buttons_keep_body("Chief", text, sizeof(text));
	CHECK(!strcmp(text, "EXTRA BUTTONS changed for Chief. They are in use already."));
	ae_extra_buttons_restore_body("Chief", text, sizeof(text));
	CHECK(!strcmp(text, "All seven for Chief go back to NONE. You can still undo it as you leave."));
	CHECK(!strcmp(AE_EXTRA_BUTTONS_TITLE, "EXTRA BUTTONS") && !strcmp(AE_EXTRA_BUTTONS_RESTORE_TITLE, "Restore EXTRA BUTTONS defaults?") &&
		!strcmp(AE_EXTRA_BUTTONS_AT_DEFAULTS, "EXTRA BUTTONS is already at its defaults."));

	if (failures)
		return 1;
	printf("ae_extra_buttons_rules_test: all passed\n");
	return 0;
}
