/* CUSTOM LAYOUT's rules: the rows, what counts as changed, the longest layout's text, the wait for a press. */

#include <stdio.h>
#include <string.h>

#include "ae_custom_layout_rules.h"

static int failures;

#define CHECK(condition) \
	do \
	{ \
		if (!(condition)) \
		{ \
			printf("FAILED %s:%d: %s\n", __FILE__, __LINE__, #condition); \
			failures++; \
		} \
	} while (0)

#define BIT(button) (1UL << (button))

static void test_rows(void)
{
	int row, other;

	for (row = 0; row < AE_CUSTOM_LAYOUT_ROWS; row++)
	{
		CHECK(controller_layout_movable_action(ae_custom_layout_actions[row]));
		for (other = 0; other < row; other++)
			CHECK(ae_custom_layout_actions[row] != ae_custom_layout_actions[other]);
	}
}

static void test_changes(void)
{
	unsigned char layout[CONTROLLER_ACTION_COUNT];

	controller_layout_preset(CONTROLLER_PRESET_DEFAULT, NULL, layout);
	CHECK(ae_custom_layout_changes(layout) == 0);
	/* (a swap moves two) */
	controller_layout_assign(layout, CONTROLLER_ACTION_MELEE, CONTROLLER_BUTTON_A);
	CHECK(ae_custom_layout_changes(layout) == 2);
	/* (and back: none, whatever it was when the page opened) */
	controller_layout_assign(layout, CONTROLLER_ACTION_MELEE, CONTROLLER_BUTTON_B);
	CHECK(ae_custom_layout_changes(layout) == 0);
}

/* every action moved: the text fits, whole, and reads back as the same layout */
static void test_longest_text(void)
{
	static int const buttons[AE_CUSTOM_LAYOUT_ROWS] = { CONTROLLER_BUTTON_LEFT_STICK, CONTROLLER_BUTTON_RIGHT_STICK,
		CONTROLLER_BUTTON_LEFT_TRIGGER, CONTROLLER_BUTTON_RIGHT_TRIGGER, CONTROLLER_BUTTON_WHITE,
		CONTROLLER_BUTTON_BLACK, CONTROLLER_BUTTON_A, CONTROLLER_BUTTON_B, CONTROLLER_BUTTON_X, CONTROLLER_BUTTON_Y };
	unsigned char layout[CONTROLLER_ACTION_COUNT], read[CONTROLLER_ACTION_COUNT];
	char text[1024];
	int row;

	controller_layout_preset(CONTROLLER_PRESET_DEFAULT, NULL, layout);
	for (row = 0; row < AE_CUSTOM_LAYOUT_ROWS; row++)
		controller_layout_assign(layout, ae_custom_layout_actions[row], buttons[row]);
	controller_layout_format(layout, text, sizeof(text));
	printf("the longest layout's text: %d characters\n", (int)strlen(text));
	CHECK(strlen(text) < AE_CUSTOM_LAYOUT_TEXT);
	controller_layout_parse(text, read);
	CHECK(!memcmp(layout, read, sizeof(read)));
}

static void test_wait(void)
{
	struct ae_custom_layout_wait wait;
	int button = -1;

	/* the press that opened it is let go first, and is no bind */
	ae_custom_layout_wait_begin(&wait, 1000);
	CHECK(ae_custom_layout_wait_step(&wait, 1, BIT(CONTROLLER_BUTTON_A), CONTROLLER_BUTTON_B, 1010, &button) ==
		AE_CUSTOM_LAYOUT_WAITING);
	CHECK(ae_custom_layout_wait_step(&wait, 1, 0, CONTROLLER_BUTTON_B, 1020, &button) == AE_CUSTOM_LAYOUT_WAITING);
	CHECK(ae_custom_layout_wait_step(&wait, 1, BIT(CONTROLLER_BUTTON_A), CONTROLLER_BUTTON_B, 1030, &button) ==
		AE_CUSTOM_LAYOUT_BOUND && button == CONTROLLER_BUTTON_A);

	/* every movable button binds */
	{
		static int const movable[] = { CONTROLLER_BUTTON_X, CONTROLLER_BUTTON_Y, CONTROLLER_BUTTON_WHITE,
			CONTROLLER_BUTTON_BLACK, CONTROLLER_BUTTON_LEFT_TRIGGER, CONTROLLER_BUTTON_RIGHT_TRIGGER,
			CONTROLLER_BUTTON_LEFT_STICK, CONTROLLER_BUTTON_RIGHT_STICK };
		size_t index;

		for (index = 0; index < sizeof(movable) / sizeof(movable[0]); index++)
		{
			ae_custom_layout_wait_begin(&wait, 0);
			ae_custom_layout_wait_step(&wait, 1, 0, CONTROLLER_BUTTON_A, 1, &button);
			CHECK(ae_custom_layout_wait_step(&wait, 1, BIT(movable[index]), CONTROLLER_BUTTON_A, 2, &button) ==
				AE_CUSTOM_LAYOUT_BOUND && button == movable[index]);
		}
	}

	/* the button it has already */
	ae_custom_layout_wait_begin(&wait, 0);
	ae_custom_layout_wait_step(&wait, 1, 0, CONTROLLER_BUTTON_B, 1, &button);
	CHECK(ae_custom_layout_wait_step(&wait, 1, BIT(CONTROLLER_BUTTON_B), CONTROLLER_BUTTON_B, 2, &button) ==
		AE_CUSTOM_LAYOUT_SAME);

	/* START and BACK cancel */
	ae_custom_layout_wait_begin(&wait, 0);
	ae_custom_layout_wait_step(&wait, 1, 0, CONTROLLER_BUTTON_B, 1, &button);
	CHECK(ae_custom_layout_wait_step(&wait, 1, BIT(CONTROLLER_BUTTON_START), CONTROLLER_BUTTON_B, 2, &button) ==
		AE_CUSTOM_LAYOUT_CANCELLED);
	CHECK(ae_custom_layout_wait_step(&wait, 1, BIT(CONTROLLER_BUTTON_BACK), CONTROLLER_BUTTON_B, 3, &button) ==
		AE_CUSTOM_LAYOUT_CANCELLED);

	/* the d-pad: told once a press, still waiting */
	ae_custom_layout_wait_begin(&wait, 0);
	ae_custom_layout_wait_step(&wait, 1, 0, CONTROLLER_BUTTON_B, 1, &button);
	CHECK(ae_custom_layout_wait_step(&wait, 1, BIT(CONTROLLER_BUTTON_DPAD_UP), CONTROLLER_BUTTON_B, 2, &button) ==
		AE_CUSTOM_LAYOUT_DPAD);
	CHECK(ae_custom_layout_wait_step(&wait, 1, BIT(CONTROLLER_BUTTON_DPAD_UP), CONTROLLER_BUTTON_B, 3, &button) ==
		AE_CUSTOM_LAYOUT_WAITING);
	CHECK(ae_custom_layout_wait_step(&wait, 1, 0, CONTROLLER_BUTTON_B, 4, &button) == AE_CUSTOM_LAYOUT_WAITING);
	CHECK(ae_custom_layout_wait_step(&wait, 1, BIT(CONTROLLER_BUTTON_DPAD_LEFT), CONTROLLER_BUTTON_B, 5, &button) ==
		AE_CUSTOM_LAYOUT_DPAD);

	/* the time: 8 s, even with the opening press still held */
	ae_custom_layout_wait_begin(&wait, 100);
	CHECK(ae_custom_layout_wait_seconds(&wait, 100) == 8);
	CHECK(ae_custom_layout_wait_seconds(&wait, 2100) == 6);
	CHECK(ae_custom_layout_wait_seconds(&wait, 8099) == 1);
	CHECK(ae_custom_layout_wait_step(&wait, 1, BIT(CONTROLLER_BUTTON_A), CONTROLLER_BUTTON_B, 8099, &button) ==
		AE_CUSTOM_LAYOUT_WAITING);
	CHECK(ae_custom_layout_wait_step(&wait, 1, BIT(CONTROLLER_BUTTON_A), CONTROLLER_BUTTON_B, 8100, &button) ==
		AE_CUSTOM_LAYOUT_TIMED_OUT);

	/* unplugged */
	ae_custom_layout_wait_begin(&wait, 0);
	CHECK(ae_custom_layout_wait_step(&wait, 0, 0, CONTROLLER_BUTTON_B, 1, &button) == AE_CUSTOM_LAYOUT_UNPLUGGED);
}

int main(void)
{
	test_rows();
	test_changes();
	test_longest_text();
	test_wait();
	if (failures)
	{
		printf("%d failed\n", failures);
		return 1;
	}
	printf("ok\n");
	return 0;
}
