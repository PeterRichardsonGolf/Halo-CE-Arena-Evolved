/* ae_test_run_card_test.c: TEST RUN's note editing and card text fitting (ae_test_run_edit.h): the whole character on
Backspace and Delete, paste's first line, the buffer's end, the wrap's "…". */

#include <stdio.h>
#include <string.h>

#include "ae_test_run_edit.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

/* every byte that starts a character is 10 wide */
static float measure(const char *text, void *context)
{
	float width = 0;

	(void)context;
	for (; *text; text++)
	{
		if (((unsigned char)*text & 0xC0) != 0x80)
			width += 10;
	}
	return width;
}

/* a tracked text: every character 10 wide and 3 more after each (the card's header is drawn tracked, so it is measured so) */
static float measure_tracked(const char *text, void *context)
{
	return measure(text, context) * 1.3f;
}

int main(void)
{
	struct ae_test_run_edit edit;
	char buffer[16];
	char lines[3][32];
	int truncated, count;

	/* typing, the cursor, backspace, delete */
	buffer[0] = 0;
	ae_test_run_edit_init(&edit, buffer, sizeof(buffer));
	CHECK(ae_test_run_edit_insert(&edit, "abc", 3) == 3);
	CHECK(!strcmp(buffer, "abc") && edit.cursor == 3);
	ae_test_run_edit_left(&edit);
	ae_test_run_edit_insert(&edit, "X", 1);
	CHECK(!strcmp(buffer, "abXc") && edit.cursor == 3);
	ae_test_run_edit_home(&edit);
	ae_test_run_edit_backspace(&edit);
	CHECK(!strcmp(buffer, "abXc") && edit.cursor == 0);
	ae_test_run_edit_delete(&edit);
	CHECK(!strcmp(buffer, "bXc"));
	ae_test_run_edit_end(&edit);
	CHECK(edit.cursor == 3);
	ae_test_run_edit_right(&edit);
	CHECK(edit.cursor == 3);
	ae_test_run_edit_backspace(&edit);
	CHECK(!strcmp(buffer, "bX"));

	/* a multi-byte character goes whole on Backspace, Delete and the arrows; control characters are dropped */
	buffer[0] = 0;
	ae_test_run_edit_init(&edit, buffer, sizeof(buffer));
	ae_test_run_edit_insert(&edit, "a\xC3\xA9\xE2\x82\xAC" "\t" "b\x7F", 9);
	CHECK(!strcmp(buffer, "a\xC3\xA9\xE2\x82\xAC" "b"));
	ae_test_run_edit_left(&edit);
	ae_test_run_edit_left(&edit);
	CHECK(edit.cursor == 3);
	ae_test_run_edit_backspace(&edit);
	CHECK(!strcmp(buffer, "a\xE2\x82\xAC" "b") && edit.cursor == 1);
	ae_test_run_edit_delete(&edit);
	CHECK(!strcmp(buffer, "ab"));
	/* an invalid byte is dropped, a torn character is not typed */
	ae_test_run_edit_insert(&edit, "\xFF\xE2\x82", 3);
	CHECK(!strcmp(buffer, "ab"));

	/* the buffer's end: 15 bytes and the 0; a character that does not fit whole is not typed */
	buffer[0] = 0;
	ae_test_run_edit_init(&edit, buffer, sizeof(buffer));
	CHECK(ae_test_run_edit_insert(&edit, "0123456789012345678", 19) == 15);
	CHECK(strlen(buffer) == 15);
	ae_test_run_edit_backspace(&edit);
	CHECK(ae_test_run_edit_insert(&edit, "\xC3\xA9", 2) == 0);
	CHECK(strlen(buffer) == 14);
	CHECK(ae_test_run_edit_insert(&edit, "\xC3\xA9", 2) == 0);
	ae_test_run_edit_backspace(&edit);
	CHECK(ae_test_run_edit_insert(&edit, "\xC3\xA9", 2) == 2);
	CHECK(strlen(buffer) == 15);

	/* paste: the first line only, truncated safely */
	buffer[0] = 0;
	ae_test_run_edit_init(&edit, buffer, sizeof(buffer));
	CHECK(ae_test_run_edit_paste(&edit, "one\r\ntwo") == 3);
	CHECK(!strcmp(buffer, "one"));
	CHECK(ae_test_run_edit_paste(&edit, "\n\nlater") == 0);
	CHECK(ae_test_run_edit_paste(&edit, NULL) == 0);
	CHECK(ae_test_run_edit_paste(&edit, "12345678901\xC3\xA9") == 11);
	CHECK(strlen(buffer) == 14);
	/* init keeps a buffer's whole characters and cuts a torn one */
	memcpy(buffer, "ab\xE2\x82", 5);
	ae_test_run_edit_init(&edit, buffer, sizeof(buffer));
	CHECK(!strcmp(buffer, "ab") && edit.cursor == 2);

	/* wrap: fits, wraps at a word, last line truncated with "…" */
	count = ae_test_run_card_wrap("short text", 200, 2, measure, NULL, &lines[0][0], 32, &truncated);
	CHECK(count == 1 && !truncated && !strcmp(lines[0], "short text"));
	count = ae_test_run_card_wrap("alpha beta gamma delta", 100, 3, measure, NULL, &lines[0][0], 32, &truncated);
	CHECK(count == 3 && !truncated);
	CHECK(!strcmp(lines[0], "alpha beta") && !strcmp(lines[1], "gamma") && !strcmp(lines[2], "delta"));
	count = ae_test_run_card_wrap("alpha beta gamma delta epsilon", 100, 2, measure, NULL, &lines[0][0], 32, &truncated);
	CHECK(count == 2 && truncated);
	CHECK(!strcmp(lines[0], "alpha beta"));
	CHECK(!strcmp(lines[1], "gamma de\xE2\x80\xA6") || !strcmp(lines[1], "gamma\xE2\x80\xA6") ||
		(measure(lines[1], NULL) <= 100 && strstr(lines[1], "\xE2\x80\xA6")));
	CHECK(measure(lines[1], NULL) <= 100);
	/* one line: the whole text cut with "…" to the width */
	count = ae_test_run_card_wrap("abcdefghijklmnop", 50, 1, measure, NULL, &lines[0][0], 32, &truncated);
	CHECK(count == 1 && truncated && !strcmp(lines[0], "abcd\xE2\x80\xA6"));
	/* a multi-byte text is never cut inside a character */
	count = ae_test_run_card_wrap("\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9", 40, 1, measure, NULL, &lines[0][0], 32, &truncated);
	CHECK(count == 1 && truncated && !strcmp(lines[0], "\xC3\xA9\xC3\xA9\xC3\xA9\xE2\x80\xA6"));
	CHECK(ae_test_run_card_wrap("", 100, 2, measure, NULL, &lines[0][0], 32, &truncated) == 0);
	/* a word wider than the line is cut at a character */
	count = ae_test_run_card_wrap("abcdefghijklmno", 50, 3, measure, NULL, &lines[0][0], 32, &truncated);
	CHECK(count == 3 && !truncated && !strcmp(lines[0], "abcde") && !strcmp(lines[2], "klmno"));

	/* the callback's measure is the one used: tracked, "abcd…" no longer fits 50 and "abc…" does not either */
	count = ae_test_run_card_wrap("abcdefghijklmnop", 50, 1, measure_tracked, NULL, &lines[0][0], 32, &truncated);
	CHECK(count == 1 && truncated && !strcmp(lines[0], "ab\xE2\x80\xA6"));
	CHECK(measure_tracked(lines[0], NULL) <= 50);

	if (!failures)
		printf("ae_test_run_card_test: ok\n");
	return failures ? 1 : 0;
}
