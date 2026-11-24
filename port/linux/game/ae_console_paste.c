/*
AE_CONSOLE_PASTE.C

Ctrl+V in the developer console (ae_console_paste.h).
*/

#include <string.h>

#include "cseries.h"
#include "dialogs/edit_text.h"
#include "input/input.h"

#include "ae_console_paste.h"

int platform_clipboard_get(char *text, int size);

/* input_xbox.c's key modifier bits: shift 0, control 1 */
#define CONTROL_BIT 1

int ae_console_paste_text(char const *clipboard, char *out, int room)
{
	int count = 0;

	for (; clipboard && *clipboard && *clipboard != '\n' && *clipboard != '\r' && count < room; clipboard++)
		if (*clipboard >= ' ' && *clipboard <= '~')
			out[count++] = *clipboard;
		else if (*clipboard == '\t' && count < room)
			out[count++] = ' ';
	out[count] = 0;
	return count;
}

void ae_console_paste(struct edit_text *edit, struct key_stroke const *key)
{
	char clipboard[1024], text[300];
	int length, room, count;

	if (!edit || !edit->buffer || !TEST_FLAG(key->modifier_flags, CONTROL_BIT))
		return;
	/* (the "v" the edit took as typed, just before the cursor) */
	length = (int)strlen(edit->buffer);
	if (edit->insertion_point_index > 0 && edit->insertion_point_index <= length &&
		(edit->buffer[edit->insertion_point_index - 1] == 'v' || edit->buffer[edit->insertion_point_index - 1] == 'V'))
	{
		memmove(&edit->buffer[edit->insertion_point_index - 1], &edit->buffer[edit->insertion_point_index],
			(size_t)(length - edit->insertion_point_index + 1));
		edit->insertion_point_index--;
		length--;
	}
	if (!platform_clipboard_get(clipboard, sizeof clipboard))
		return;
	room = edit->maximum_length - length;
	if (room <= 0)
		return;
	if (room > (int)sizeof text - 1)
		room = (int)sizeof text - 1;
	count = ae_console_paste_text(clipboard, text, room);
	if (!count)
		return;
	memmove(&edit->buffer[edit->insertion_point_index + count], &edit->buffer[edit->insertion_point_index],
		(size_t)(length - edit->insertion_point_index + 1));
	memcpy(&edit->buffer[edit->insertion_point_index], text, (size_t)count);
	edit->insertion_point_index = (short)(edit->insertion_point_index + count);
	edit->selection_start_index = NONE;
}
