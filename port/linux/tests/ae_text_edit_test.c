#include <stdio.h>
#include <string.h>
#include "ae_text_edit.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	struct ae_text t;
	char paste[201];
	short from, to;
	int index;

	/* insert to the limit: 1; one more: 0, the text unchanged */
	ae_text_init(&t, "", 11, 0);
	CHECK(t.length == 0 && t.caret == 0 && t.limit == 11);
	CHECK(ae_text_insert(&t, "Slayer Pro1") == 1 && t.length == 11 && !strcmp(t.text, "Slayer Pro1"));
	CHECK(ae_text_insert(&t, "x") == 0 && !strcmp(t.text, "Slayer Pro1") && t.length == 11);
	/* newlines and tabs dropped: "AB\nC\tD" -> "ABCD", 0 */
	ae_text_init(&t, "", 20, 0);
	CHECK(ae_text_insert(&t, "AB\nC\tD") == 0 && !strcmp(t.text, "ABCD") && t.caret == 4);
	/* non-ASCII dropped whole: "é" (C3 A9) -> nothing, 0 */
	CHECK(ae_text_insert(&t, "\xC3\xA9") == 0 && !strcmp(t.text, "ABCD"));
	CHECK(ae_text_insert(&t, "e\xC3\xA9z") == 0 && !strcmp(t.text, "ABCDez"));
	/* select all + insert "X" -> "X" */
	ae_text_select_all(&t);
	CHECK(ae_text_selection(&t, &from, &to) && from == 0 && to == 6);
	CHECK(ae_text_insert(&t, "X") == 1 && !strcmp(t.text, "X") && t.caret == 1 && !ae_text_selection(&t, NULL, NULL));
	/* backspace at 0 and delete at the end: no-ops */
	ae_text_home(&t, 0);
	ae_text_backspace(&t);
	CHECK(!strcmp(t.text, "X") && t.caret == 0);
	ae_text_end(&t, 0);
	ae_text_delete(&t);
	CHECK(!strcmp(t.text, "X") && t.caret == 1);
	/* home / end with select */
	ae_text_init(&t, "GAME TYPE", 20, 0);
	ae_text_move(&t, -4, 0);
	CHECK(t.caret == 5);
	ae_text_home(&t, 1);
	CHECK(ae_text_selection(&t, &from, &to) && from == 0 && to == 5);
	ae_text_end(&t, 1);
	CHECK(ae_text_selection(&t, &from, &to) && from == 5 && to == 9);
	ae_text_backspace(&t);
	CHECK(!strcmp(t.text, "GAME ") && t.caret == 5);
	/* a paste of 200 characters into limit 11 keeps 11 */
	for (index = 0; index < 200; index++)
		paste[index] = (char)('a' + index % 26);
	paste[200] = 0;
	ae_text_init(&t, "", 11, 0);
	CHECK(ae_text_insert(&t, paste) == 0 && t.length == 11 && !strncmp(t.text, paste, 11) && t.text[11] == 0);
	/* insertion in the middle, the caret after it; delete and backspace in the middle */
	ae_text_init(&t, "ACE", 20, 0);
	ae_text_move(&t, -2, 0);
	CHECK(ae_text_insert(&t, "B") && !strcmp(t.text, "ABCE") && t.caret == 2);
	ae_text_move(&t, 1, 0);
	CHECK(ae_text_insert(&t, "D") && !strcmp(t.text, "ABCDE"));
	ae_text_delete(&t);
	CHECK(!strcmp(t.text, "ABCD"));
	ae_text_backspace(&t);
	CHECK(!strcmp(t.text, "ABC") && t.caret == 3);
	/* moves clamp; a move from a selection lands on its edge */
	ae_text_move(&t, 10, 0);
	CHECK(t.caret == 3);
	ae_text_move(&t, -10, 0);
	CHECK(t.caret == 0);
	ae_text_move(&t, 2, 1);
	CHECK(ae_text_selection(&t, &from, &to) && from == 0 && to == 2);
	ae_text_move(&t, -1, 0);
	CHECK(t.caret == 0 && !ae_text_selection(&t, NULL, NULL));
	/* an uppercase field upper-cases what is typed (P17): a, b -> "AB" */
	ae_text_init(&t, "", 11, 1);
	CHECK(ae_text_insert(&t, "a") && ae_text_insert(&t, "b") && !strcmp(t.text, "AB"));
	ae_text_init(&t, "slayer", 11, 1);
	CHECK(!strcmp(t.text, "SLAYER"));
	/* the limit is at most AE_TEXT_MAXIMUM - 1; init cuts at the limit */
	ae_text_init(&t, paste, 999, 0);
	CHECK(t.limit == AE_TEXT_MAXIMUM - 1 && t.length == AE_TEXT_MAXIMUM - 1 && t.text[AE_TEXT_MAXIMUM - 1] == 0);
	ae_text_init(&t, "abcdef", 3, 0);
	CHECK(!strcmp(t.text, "abc") && t.caret == 3);
	CHECK(ae_text_insert(&t, NULL) == 1);
	/* every caret position: an insert, a backspace and a delete there, against the same edit done by hand */
	for (index = 0; index <= 6; index++)
	{
		char expected[16];

		ae_text_init(&t, "ABCDEF", 20, 0);
		ae_text_home(&t, 0);
		ae_text_move(&t, (short)index, 0);
		CHECK(t.caret == index);
		ae_text_insert(&t, "x");
		snprintf(expected, sizeof(expected), "%.*sx%s", index, "ABCDEF", "ABCDEF" + index);
		CHECK(!strcmp(t.text, expected) && t.caret == index + 1 && t.length == 7);
		ae_text_init(&t, "ABCDEF", 20, 0);
		ae_text_home(&t, 0);
		ae_text_move(&t, (short)index, 0);
		ae_text_backspace(&t);
		if (index > 0)
			snprintf(expected, sizeof(expected), "%.*s%s", index - 1, "ABCDEF", "ABCDEF" + index);
		else
			snprintf(expected, sizeof(expected), "ABCDEF");
		CHECK(!strcmp(t.text, expected) && t.caret == (index > 0 ? index - 1 : 0));
		ae_text_init(&t, "ABCDEF", 20, 0);
		ae_text_home(&t, 0);
		ae_text_move(&t, (short)index, 0);
		ae_text_delete(&t);
		if (index < 6)
			snprintf(expected, sizeof(expected), "%.*s%s", index, "ABCDEF", "ABCDEF" + index + 1);
		else
			snprintf(expected, sizeof(expected), "ABCDEF");
		CHECK(!strcmp(t.text, expected) && t.caret == index && (short)strlen(t.text) == t.length);
	}
	/* a selection in the middle (BCD): insert replaces it, backspace and delete remove it */
	ae_text_init(&t, "ABCDEF", 20, 0);
	ae_text_home(&t, 0);
	ae_text_move(&t, 1, 0);
	ae_text_move(&t, 3, 1);
	CHECK(ae_text_selection(&t, &from, &to) && from == 1 && to == 4);
	CHECK(ae_text_insert(&t, "xy") && !strcmp(t.text, "AxyEF") && t.caret == 3);
	ae_text_init(&t, "ABCDEF", 20, 0);
	ae_text_move(&t, -2, 0);
	ae_text_move(&t, -3, 1);
	CHECK(ae_text_selection(&t, &from, &to) && from == 1 && to == 4);
	ae_text_backspace(&t);
	CHECK(!strcmp(t.text, "AEF") && t.caret == 1);
	ae_text_init(&t, "ABCDEF", 20, 0);
	ae_text_move(&t, -5, 0);
	ae_text_move(&t, 3, 1);
	ae_text_delete(&t);
	CHECK(!strcmp(t.text, "AEF") && t.caret == 1);
	/* a selection with a full field: the insert fills only what the selection frees */
	ae_text_init(&t, "ABCDEF", 6, 0);
	ae_text_home(&t, 0);
	ae_text_move(&t, 2, 1);
	CHECK(ae_text_insert(&t, "xyz") == 0 && !strcmp(t.text, "xyCDEF"));
	if (failures)
		printf("%d failures\n", failures);
	return failures ? 1 : 0;
}
