/* ae_test_run_keyboard.c: TEST RUN's pad keyboard (ae_test_run_keyboard.h). The tables are the text chat's (network_chat.c). */

#include "ae_test_run_keyboard.h"

#include "ae_test_run_input.h"

/* the characters but the last row's, the letters' and the symbols' pages */
static const char *const letters[AE_TEST_RUN_KEYBOARD_ROWS - 1] = { "1234567890", "qwertyuiop", "asdfghjkl'", "zxcvbnm,.?" };
static const char *const symbols[AE_TEST_RUN_KEYBOARD_ROWS - 1] = { "1234567890", "!@#$%^&*()", "-_=+[]{};:", "\"'<>/\\?~,." };

void ae_test_run_keyboard_open(struct ae_test_run_keyboard *keyboard)
{
	keyboard->row = 1;
	keyboard->column = 0;
	keyboard->symbols = 0;
	keyboard->capitals = 0;
}

void ae_test_run_keyboard_move(struct ae_test_run_keyboard *keyboard, int rows_down, int columns_right)
{
	int width;

	keyboard->row = (keyboard->row + rows_down % AE_TEST_RUN_KEYBOARD_ROWS + AE_TEST_RUN_KEYBOARD_ROWS) % AE_TEST_RUN_KEYBOARD_ROWS;
	width = keyboard->row == AE_TEST_RUN_KEYBOARD_ROWS - 1 ? AE_TEST_RUN_KEYBOARD_WIDE : 1;
	keyboard->column -= keyboard->column % width;
	keyboard->column = (keyboard->column + columns_right * width % AE_TEST_RUN_KEYBOARD_COLUMNS + AE_TEST_RUN_KEYBOARD_COLUMNS) %
		AE_TEST_RUN_KEYBOARD_COLUMNS;
}

char ae_test_run_keyboard_character(const struct ae_test_run_keyboard *keyboard, int row, int column)
{
	char character;

	if (row < 0 || row >= AE_TEST_RUN_KEYBOARD_ROWS - 1 || column < 0 || column >= AE_TEST_RUN_KEYBOARD_COLUMNS)
		return 0;
	character = (keyboard->symbols ? symbols : letters)[row][column];
	if (keyboard->capitals && character >= 'a' && character <= 'z')
		character = (char)(character - 'a' + 'A');
	return character;
}

int ae_test_run_keyboard_special(int row, int column)
{
	if (row != AE_TEST_RUN_KEYBOARD_ROWS - 1 || column < 0 || column >= AE_TEST_RUN_KEYBOARD_COLUMNS)
		return -1;
	return column / AE_TEST_RUN_KEYBOARD_WIDE;
}

const char *ae_test_run_keyboard_special_label(const struct ae_test_run_keyboard *keyboard, int special)
{
	switch (special)
	{
	case AE_TEST_RUN_KEY_CASE:
		return keyboard->capitals ? "abc" : "ABC";
	case AE_TEST_RUN_KEY_SYMBOLS:
		return keyboard->symbols ? "abc" : "123";
	case AE_TEST_RUN_KEY_SPACE:
		return "SPACE";
	case AE_TEST_RUN_KEY_DELETE:
		return "DEL";
	case AE_TEST_RUN_KEY_DONE:
		return "DONE";
	}
	return "";
}

int ae_test_run_keyboard_poll(struct ae_test_run_keyboard *keyboard, unsigned edges, struct ae_test_run_edit *note)
{
	int rows_down = ((edges & AE_TEST_RUN_PAD_DOWN) != 0) - ((edges & AE_TEST_RUN_PAD_UP) != 0);
	int columns_right = ((edges & AE_TEST_RUN_PAD_RIGHT) != 0) - ((edges & AE_TEST_RUN_PAD_LEFT) != 0);

	if (edges & AE_TEST_RUN_PAD_B)
		return -1;
	if (edges & AE_TEST_RUN_PAD_START)
		return 1;
	if (rows_down || columns_right)
		ae_test_run_keyboard_move(keyboard, rows_down, columns_right);
	if (edges & AE_TEST_RUN_PAD_LB)
		keyboard->capitals = !keyboard->capitals;
	if (edges & AE_TEST_RUN_PAD_RB)
		keyboard->symbols = !keyboard->symbols;
	if (edges & AE_TEST_RUN_PAD_X)
		ae_test_run_edit_backspace(note);
	if (edges & AE_TEST_RUN_PAD_Y)
		ae_test_run_edit_insert(note, " ", 1);
	if (edges & AE_TEST_RUN_PAD_A)
	{
		int special = ae_test_run_keyboard_special(keyboard->row, keyboard->column);
		char typed = ae_test_run_keyboard_character(keyboard, keyboard->row, keyboard->column);

		if (typed)
			ae_test_run_edit_insert(note, &typed, 1);
		else if (special == AE_TEST_RUN_KEY_CASE)
			keyboard->capitals = !keyboard->capitals;
		else if (special == AE_TEST_RUN_KEY_SYMBOLS)
			keyboard->symbols = !keyboard->symbols;
		else if (special == AE_TEST_RUN_KEY_SPACE)
			ae_test_run_edit_insert(note, " ", 1);
		else if (special == AE_TEST_RUN_KEY_DELETE)
			ae_test_run_edit_backspace(note);
		else if (special == AE_TEST_RUN_KEY_DONE)
			return 1;
	}
	return 0;
}
