/* ae_text_edit.c: Arena Evolved menus, editing a field's text (see ae_text_edit.h). No engine includes. */

#include <string.h>
#include "ae_text_edit.h"

static int printable(char character)
{
	return character >= ' ' && character <= '~';
}

static short clamp(short value, short low, short high)
{
	return value < low ? low : value > high ? high : value;
}

void ae_text_init(struct ae_text *text, const char *initial, short limit, int uppercase)
{
	memset(text, 0, sizeof(*text));
	text->limit = clamp(limit, 0, AE_TEXT_MAXIMUM - 1);
	text->uppercase = uppercase != 0;
	if (initial)
		ae_text_insert(text, initial);
	text->anchor = text->caret = text->length;
}

int ae_text_selection(struct ae_text const *text, short *from, short *to)
{
	short low = text->anchor < text->caret ? text->anchor : text->caret;
	short high = text->anchor < text->caret ? text->caret : text->anchor;

	if (from)
		*from = low;
	if (to)
		*to = high;
	return high > low;
}

/* the selection removed (caret at its start); nothing when empty */
static void remove_selection(struct ae_text *text)
{
	short from, to;

	if (!ae_text_selection(text, &from, &to))
		return;
	memmove(text->text + from, text->text + to, (size_t)(text->length - to + 1));
	text->length = (short)(text->length - (to - from));
	text->caret = text->anchor = from;
}

int ae_text_insert(struct ae_text *text, const char *utf8)
{
	char characters[AE_TEXT_MAXIMUM];
	short count = 0, room;
	int whole = 1;
	const char *cursor;

	if (!utf8)
		return 1;
	remove_selection(text);
	room = (short)(text->limit - text->length);
	/* (printable ASCII only, upper-cased for an uppercase field; anything else dropped, whole sequences included;
	what passes the limit cut) */
	for (cursor = utf8; *cursor; cursor++)
	{
		char character = *cursor;

		if (!printable(character))
		{
			whole = 0;
			continue;
		}
		if (count >= room)
		{
			whole = 0;
			break;
		}
		if (text->uppercase && character >= 'a' && character <= 'z')
			character = (char)(character - 'a' + 'A');
		characters[count++] = character;
	}
	if (count > 0)
	{
		memmove(text->text + text->caret + count, text->text + text->caret, (size_t)(text->length - text->caret + 1));
		memcpy(text->text + text->caret, characters, (size_t)count);
		text->length = (short)(text->length + count);
		text->caret = (short)(text->caret + count);
	}
	text->anchor = text->caret;
	return whole;
}

void ae_text_backspace(struct ae_text *text)
{
	if (ae_text_selection(text, NULL, NULL))
	{
		remove_selection(text);
		return;
	}
	if (text->caret <= 0)
		return;
	memmove(text->text + text->caret - 1, text->text + text->caret, (size_t)(text->length - text->caret + 1));
	text->length--;
	text->caret--;
	text->anchor = text->caret;
}

void ae_text_delete(struct ae_text *text)
{
	if (ae_text_selection(text, NULL, NULL))
	{
		remove_selection(text);
		return;
	}
	if (text->caret >= text->length)
		return;
	memmove(text->text + text->caret, text->text + text->caret + 1, (size_t)(text->length - text->caret));
	text->length--;
	text->anchor = text->caret;
}

void ae_text_move(struct ae_text *text, short delta, int select)
{
	short from, to;

	/* (a move without select from a selection lands on its edge) */
	if (!select && ae_text_selection(text, &from, &to) && delta)
		text->caret = delta < 0 ? from : to;
	else
		text->caret = clamp((short)(text->caret + delta), 0, text->length);
	if (!select)
		text->anchor = text->caret;
}

void ae_text_home(struct ae_text *text, int select)
{
	text->caret = 0;
	if (!select)
		text->anchor = text->caret;
}

void ae_text_end(struct ae_text *text, int select)
{
	text->caret = text->length;
	if (!select)
		text->anchor = text->caret;
}

void ae_text_select_all(struct ae_text *text)
{
	text->anchor = 0;
	text->caret = text->length;
}
