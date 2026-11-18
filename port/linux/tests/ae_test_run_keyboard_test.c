/* ae_test_run_keyboard_test.c: TEST RUN's pad keyboard (ae_test_run_keyboard.h): the cursor over the table (wrapping, the
wide last row), the case and the symbols' page, what A, X, Y, START and B do to the note. */

#include <stdio.h>
#include <string.h>

#include "ae_test_run_input.h"
#include "ae_test_run_keyboard.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	struct ae_test_run_keyboard keyboard;
	struct ae_test_run_edit note;
	char buffer[16];

	ae_test_run_keyboard_open(&keyboard);
	CHECK(keyboard.row == 1 && keyboard.column == 0);
	CHECK(ae_test_run_keyboard_character(&keyboard, 1, 0) == 'q');
	/* the cursor wraps over the rows and the columns */
	ae_test_run_keyboard_move(&keyboard, -1, 0);
	CHECK(keyboard.row == 0);
	ae_test_run_keyboard_move(&keyboard, -1, 0);
	CHECK(keyboard.row == AE_TEST_RUN_KEYBOARD_ROWS - 1);
	ae_test_run_keyboard_move(&keyboard, 1, 0);
	CHECK(keyboard.row == 0);
	ae_test_run_keyboard_move(&keyboard, 0, -1);
	CHECK(keyboard.column == AE_TEST_RUN_KEYBOARD_COLUMNS - 1);
	ae_test_run_keyboard_move(&keyboard, 0, 1);
	CHECK(keyboard.column == 0);
	/* the last row goes a wide key at a time, and a column in a wide key is its first */
	keyboard.row = 3;
	keyboard.column = 5;
	ae_test_run_keyboard_move(&keyboard, 1, 0);
	CHECK(keyboard.row == 4 && keyboard.column == 4);
	ae_test_run_keyboard_move(&keyboard, 0, 1);
	CHECK(keyboard.column == 6);
	ae_test_run_keyboard_move(&keyboard, 0, 1);
	ae_test_run_keyboard_move(&keyboard, 0, 1);
	CHECK(keyboard.column == 0);
	ae_test_run_keyboard_move(&keyboard, 0, -1);
	CHECK(keyboard.column == 8);
	ae_test_run_keyboard_move(&keyboard, -1, 0);
	CHECK(keyboard.row == 3 && keyboard.column == 8);
	/* the keys: letters, capitals, the symbols' page, none on the last row */
	CHECK(ae_test_run_keyboard_character(&keyboard, 3, 4) == 'b');
	keyboard.capitals = 1;
	CHECK(ae_test_run_keyboard_character(&keyboard, 3, 4) == 'B');
	CHECK(ae_test_run_keyboard_character(&keyboard, 0, 0) == '1');
	keyboard.capitals = 0;
	keyboard.symbols = 1;
	CHECK(ae_test_run_keyboard_character(&keyboard, 1, 0) == '!');
	CHECK(ae_test_run_keyboard_character(&keyboard, 4, 0) == 0);
	CHECK(ae_test_run_keyboard_character(&keyboard, 9, 0) == 0 && ae_test_run_keyboard_character(&keyboard, 0, 10) == 0);
	CHECK(ae_test_run_keyboard_special(4, 0) == AE_TEST_RUN_KEY_CASE && ae_test_run_keyboard_special(4, 9) == AE_TEST_RUN_KEY_DONE);
	CHECK(ae_test_run_keyboard_special(4, 5) == AE_TEST_RUN_KEY_SPACE && ae_test_run_keyboard_special(3, 5) == -1);

	/* A types, X deletes, Y spaces, LB and RB switch the case and the page, the d-pad moves */
	ae_test_run_keyboard_open(&keyboard);
	buffer[0] = 0;
	ae_test_run_edit_init(&note, buffer, sizeof(buffer));
	CHECK(ae_test_run_keyboard_poll(&keyboard, AE_TEST_RUN_PAD_A, &note) == 0 && !strcmp(buffer, "q"));
	CHECK(ae_test_run_keyboard_poll(&keyboard, AE_TEST_RUN_PAD_RIGHT | AE_TEST_RUN_PAD_A, &note) == 0 && !strcmp(buffer, "qw"));
	CHECK(ae_test_run_keyboard_poll(&keyboard, AE_TEST_RUN_PAD_Y, &note) == 0 && !strcmp(buffer, "qw "));
	CHECK(ae_test_run_keyboard_poll(&keyboard, AE_TEST_RUN_PAD_X, &note) == 0 && !strcmp(buffer, "qw"));
	CHECK(ae_test_run_keyboard_poll(&keyboard, AE_TEST_RUN_PAD_LB | AE_TEST_RUN_PAD_A, &note) == 0 && !strcmp(buffer, "qwW"));
	CHECK(ae_test_run_keyboard_poll(&keyboard, AE_TEST_RUN_PAD_RB, &note) == 0 && keyboard.symbols == 1);
	CHECK(ae_test_run_keyboard_poll(&keyboard, AE_TEST_RUN_PAD_A, &note) == 0 && !strcmp(buffer, "qwW@"));
	CHECK(ae_test_run_keyboard_poll(&keyboard, AE_TEST_RUN_PAD_RB, &note) == 0 && keyboard.symbols == 0);
	/* the last row's keys: the case, the page, a space, a delete, DONE */
	keyboard.row = 4;
	keyboard.column = 0;
	CHECK(ae_test_run_keyboard_poll(&keyboard, AE_TEST_RUN_PAD_A, &note) == 0 && keyboard.capitals == 0);
	keyboard.column = 2;
	CHECK(ae_test_run_keyboard_poll(&keyboard, AE_TEST_RUN_PAD_A, &note) == 0 && keyboard.symbols == 1);
	keyboard.column = 4;
	CHECK(ae_test_run_keyboard_poll(&keyboard, AE_TEST_RUN_PAD_A, &note) == 0 && !strcmp(buffer, "qwW@ "));
	keyboard.column = 6;
	CHECK(ae_test_run_keyboard_poll(&keyboard, AE_TEST_RUN_PAD_A, &note) == 0 && !strcmp(buffer, "qwW@"));
	keyboard.column = 8;
	CHECK(ae_test_run_keyboard_poll(&keyboard, AE_TEST_RUN_PAD_A, &note) == 1);
	/* START saves, B leaves (the caller drops the note), the buffer's end stops typing without harm */
	CHECK(ae_test_run_keyboard_poll(&keyboard, AE_TEST_RUN_PAD_START, &note) == 1);
	CHECK(ae_test_run_keyboard_poll(&keyboard, AE_TEST_RUN_PAD_B, &note) == -1);
	keyboard.row = 1;
	keyboard.column = 0;
	keyboard.symbols = 0;
	{
		int count;

		for (count = 0; count < 40; count++)
			ae_test_run_keyboard_poll(&keyboard, AE_TEST_RUN_PAD_A, &note);
		CHECK(strlen(buffer) == sizeof(buffer) - 1);
	}
	/* a stick's or the d-pad's edges together move once each way */
	ae_test_run_keyboard_open(&keyboard);
	ae_test_run_keyboard_poll(&keyboard, AE_TEST_RUN_PAD_DOWN, &note);
	CHECK(keyboard.row == 2);
	ae_test_run_keyboard_poll(&keyboard, AE_TEST_RUN_PAD_DOWN | AE_TEST_RUN_PAD_UP, &note);
	CHECK(keyboard.row == 2);
	CHECK(!strcmp(ae_test_run_keyboard_special_label(&keyboard, AE_TEST_RUN_KEY_CASE), "ABC"));
	keyboard.capitals = 1;
	CHECK(!strcmp(ae_test_run_keyboard_special_label(&keyboard, AE_TEST_RUN_KEY_CASE), "abc"));
	CHECK(!strcmp(ae_test_run_keyboard_special_label(&keyboard, AE_TEST_RUN_KEY_SYMBOLS), "123"));
	CHECK(!strcmp(ae_test_run_keyboard_special_label(&keyboard, AE_TEST_RUN_KEY_DONE), "DONE"));

	if (failures)
	{
		printf("%d failed\n", failures);
		return 1;
	}
	printf("ae_test_run_keyboard_test: ok\n");
	return 0;
}
