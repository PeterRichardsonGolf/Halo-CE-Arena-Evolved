/*
AE_CONSOLE_PASTE.H

Ctrl+V in the developer console (source/main/console.c): the clipboard's first line pasted where the cursor is.
*/

#ifndef __AE_CONSOLE_PASTE_H
#define __AE_CONSOLE_PASTE_H

struct edit_text;
struct key_stroke;

/* a V key in the console: with Ctrl held, the "v" the edit took as typed comes out and the clipboard's first line
(printable characters only, as much as fits) goes in at the cursor */
void ae_console_paste(struct edit_text *edit, struct key_stroke const *key);
/* the text a paste puts in (pure, for the test): the clipboard's first line, printable ASCII only, at most room
characters; how many */
int ae_console_paste_text(char const *clipboard, char *out, int room);

#endif
