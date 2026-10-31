/*
AE_TEST_RUN_KEYBOARD.H
TEST RUN's pad keyboard (DEV MODE): the letter picker behind TYPE… in the note box, modelled on the text chat's (network_chat.c:
ten columns, four rows of characters and a last row of five wide keys). Pure: no engine calls, so it is unit tested
(port/linux/tests/ae_test_run_keyboard_test.c). The note module feeds it the pad's presses (ae_test_run_input.h's
AE_TEST_RUN_PAD_*) and the note's editor; the card draws it.

Controls: d-pad or left stick moves the cursor (wrapping), A types the key, X is Backspace, Y a space, LB switches
the letters' case, RB the symbols' page, START (or the DONE key) saves the note, B leaves the picker and keeps nothing.
*/
#ifndef AE_TEST_RUN_KEYBOARD_H
#define AE_TEST_RUN_KEYBOARD_H

#include "ae_test_run_edit.h"

enum { AE_TEST_RUN_KEYBOARD_COLUMNS = 10, AE_TEST_RUN_KEYBOARD_ROWS = 5, AE_TEST_RUN_KEYBOARD_WIDE = 2,
	AE_TEST_RUN_KEYBOARD_KEYS = 5 /* the last row's */ };
/* the last row's keys, left to right */
enum ae_test_run_keyboard_key { AE_TEST_RUN_KEY_CASE, AE_TEST_RUN_KEY_SYMBOLS, AE_TEST_RUN_KEY_SPACE, AE_TEST_RUN_KEY_DELETE,
	AE_TEST_RUN_KEY_DONE };

struct ae_test_run_keyboard
{
	int row, column; /* the cursor: a wide key's column is its first */
	int symbols;     /* the symbols' page */
	int capitals;    /* the letters in capitals */
};

/* the cursor on the letter row's first key, the letters' page in lower case */
void ae_test_run_keyboard_open(struct ae_test_run_keyboard *keyboard);
/* the cursor moved by rows and columns, wrapping; on the last row it goes a wide key at a time */
void ae_test_run_keyboard_move(struct ae_test_run_keyboard *keyboard, int rows_down, int columns_right);
/* the character a key types (capitals, and the page, as shown); 0 on the last row or off the keys */
char ae_test_run_keyboard_character(const struct ae_test_run_keyboard *keyboard, int row, int column);
/* the last row's key at a column (a wide key's column is its first); -1 above the last row */
int ae_test_run_keyboard_special(int row, int column);
/* what the last row's key is called now: "abc" / "ABC" for the case key, "123" / "abc" for the page key, and so on */
const char *ae_test_run_keyboard_special_label(const struct ae_test_run_keyboard *keyboard, int special);
/* the pad's presses (AE_TEST_RUN_PAD_*) applied to the keyboard and to note: 0 still open, 1 saved (START, or the DONE key),
-1 left (B): the note is then to be dropped */
int ae_test_run_keyboard_poll(struct ae_test_run_keyboard *keyboard, unsigned edges, struct ae_test_run_edit *note);

#endif
