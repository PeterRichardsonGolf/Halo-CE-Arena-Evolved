/*
AE_TEST_RUN_EDIT.H
TEST RUN's note text, edited on a fixed buffer (pure: no engine calls; unit test port/linux/tests/ae_test_run_card_test.c),
and the card's text fitting. The text is UTF-8; an edit never splits a multi-byte character, and the buffer always
ends in a 0 (size includes it). The cursor is a byte offset on a character boundary.
*/
#ifndef AE_TEST_RUN_EDIT_H
#define AE_TEST_RUN_EDIT_H

struct ae_test_run_edit
{
	char *text;
	int size;   /* the buffer's bytes, with the 0 */
	int cursor; /* a byte offset, 0 to strlen(text) */
};

/* the cursor goes to the end of what the buffer holds (a buffer holding a torn character is cut back to a whole one) */
void ae_test_run_edit_init(struct ae_test_run_edit *edit, char *text, int size);
/* types utf8 (length bytes) at the cursor: control characters and invalid bytes are dropped, a character that does
not fit whole is not typed (the rest of the text is dropped with it); returns the bytes typed */
int ae_test_run_edit_insert(struct ae_test_run_edit *edit, const char *utf8, int length);
/* Ctrl+V: the clipboard's first line (up to a CR or LF), typed; returns the bytes typed */
int ae_test_run_edit_paste(struct ae_test_run_edit *edit, const char *clipboard);
void ae_test_run_edit_backspace(struct ae_test_run_edit *edit); /* the whole character before the cursor */
void ae_test_run_edit_delete(struct ae_test_run_edit *edit);    /* the whole character at the cursor */
void ae_test_run_edit_home(struct ae_test_run_edit *edit);
void ae_test_run_edit_end(struct ae_test_run_edit *edit);
void ae_test_run_edit_left(struct ae_test_run_edit *edit);
void ae_test_run_edit_right(struct ae_test_run_edit *edit);

/* the width of a text in layout units */
typedef float (*ae_test_run_measure)(const char *utf8, void *context);
/* word-wraps text into at most max_lines lines (each line_size bytes, at lines[0..]) of at most width each. When the
text does not fit, the last line ends in "…" (U+2026) and fits with it; a word wider than a line is cut at a character.
Returns the lines written (0 for an empty text); *truncated says whether "…" was added. */
int ae_test_run_card_wrap(const char *text, float width, int max_lines, ae_test_run_measure measure, void *context,
	char *lines, int line_size, int *truncated);

#endif
