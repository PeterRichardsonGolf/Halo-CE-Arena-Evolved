/*
AE_TEXT_EDIT.H

Editing a text field's text (ae_text_edit.c): insertion, deletion, the caret
and the selection. Pure C, no engine includes (unit test:
port/linux/tests/ae_text_edit_test.c, under the sanitizers).

Fields take printable ASCII only (' '..'~'), as upstream's PC text field does
(menu_functions.c text_field_insert): the game's names are 16-bit strings
drawn in the maps' fonts. Limits count characters. A field marked uppercase
(gametype names) upper-cases what is inserted.
*/

#ifndef __AE_TEXT_EDIT_H
#define __AE_TEXT_EDIT_H

enum { AE_TEXT_MAXIMUM = 128 };
struct ae_text { char text[AE_TEXT_MAXIMUM]; short length, caret, anchor; /* selection: anchor..caret */
	short limit; int uppercase; };
/* the text (printable ASCII of initial, cut at the limit), the caret at its end, nothing selected; limit at most
AE_TEXT_MAXIMUM - 1 */
void ae_text_init(struct ae_text *text, const char *initial, short limit, int uppercase);
/* inserts printable ASCII (' '..'~') replacing the selection; other bytes (newlines, tabs, non-ASCII) are dropped;
cut at the limit; returns 0 when anything was dropped or cut (the caller plays failure once) */
int ae_text_insert(struct ae_text *text, const char *utf8);
void ae_text_backspace(struct ae_text *text);
void ae_text_delete(struct ae_text *text);
void ae_text_move(struct ae_text *text, short delta, int select);
void ae_text_home(struct ae_text *text, int select);
void ae_text_end(struct ae_text *text, int select);
void ae_text_select_all(struct ae_text *text);
int ae_text_selection(struct ae_text const *text, short *from, short *to);   /* 0 when empty */

#endif
