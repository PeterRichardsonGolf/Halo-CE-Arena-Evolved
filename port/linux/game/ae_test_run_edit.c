/* ae_test_run_edit.c: TEST RUN's note editing and text fitting (ae_test_run_edit.h). */

#include "ae_test_run_edit.h"

#include <string.h>

static const char ELLIPSIS[] = "\xE2\x80\xA6";

/* the length of the whole character at text (1 to 4), or 0 when its lead byte or its continuation bytes are wrong or run past the end (overlong and surrogate forms are not
checked: this keeps a character whole, it does not validate) */
static int character_length(const unsigned char *text, int available)
{
	int length, index;

	if (available <= 0)
		return 0;
	if (text[0] < 0x80)
		length = 1;
	else if (text[0] >= 0xC2 && text[0] <= 0xDF)
		length = 2;
	else if (text[0] >= 0xE0 && text[0] <= 0xEF)
		length = 3;
	else if (text[0] >= 0xF0 && text[0] <= 0xF4)
		length = 4;
	else
		return 0;
	if (length > available)
		return 0;
	for (index = 1; index < length; index++)
	{
		if ((text[index] & 0xC0) != 0x80)
			return 0;
	}
	return length;
}

/* the start of the character that ends at offset */
static int previous_start(const char *text, int offset)
{
	while (offset > 0)
	{
		offset--;
		if (((unsigned char)text[offset] & 0xC0) != 0x80)
			break;
	}
	return offset;
}

void ae_test_run_edit_init(struct ae_test_run_edit *edit, char *text, int size)
{
	int at = 0, length;

	edit->text = text;
	edit->size = size;
	text[size - 1] = 0;
	/* (keeps the whole characters; cuts at the first invalid or torn one) */
	while (text[at] && (length = character_length((const unsigned char *)text + at, (int)strlen(text + at))) > 0)
		at += length;
	text[at] = 0;
	edit->cursor = at;
}

int ae_test_run_edit_insert(struct ae_test_run_edit *edit, const char *utf8, int length)
{
	int used = (int)strlen(edit->text), typed = 0, at = 0;

	while (at < length)
	{
		int count = character_length((const unsigned char *)utf8 + at, length - at);

		if (!count)
		{
			at++;
			continue;
		}
		if (((unsigned char)utf8[at] < 0x20) || (unsigned char)utf8[at] == 0x7F)
		{
			at++;
			continue;
		}
		if (used + count > edit->size - 1)
			break;
		memmove(edit->text + edit->cursor + count, edit->text + edit->cursor, (size_t)(used - edit->cursor + 1));
		memcpy(edit->text + edit->cursor, utf8 + at, (size_t)count);
		edit->cursor += count;
		used += count;
		typed += count;
		at += count;
	}
	return typed;
}

int ae_test_run_edit_paste(struct ae_test_run_edit *edit, const char *clipboard)
{
	int length = 0;

	if (!clipboard)
		return 0;
	while (clipboard[length] && clipboard[length] != '\r' && clipboard[length] != '\n')
		length++;
	return ae_test_run_edit_insert(edit, clipboard, length);
}

void ae_test_run_edit_backspace(struct ae_test_run_edit *edit)
{
	int start = previous_start(edit->text, edit->cursor);

	if (start == edit->cursor)
		return;
	memmove(edit->text + start, edit->text + edit->cursor, strlen(edit->text + edit->cursor) + 1);
	edit->cursor = start;
}

void ae_test_run_edit_delete(struct ae_test_run_edit *edit)
{
	int length = character_length((const unsigned char *)edit->text + edit->cursor, (int)strlen(edit->text + edit->cursor));

	if (!length)
		return;
	memmove(edit->text + edit->cursor, edit->text + edit->cursor + length, strlen(edit->text + edit->cursor + length) + 1);
}

void ae_test_run_edit_home(struct ae_test_run_edit *edit)
{
	edit->cursor = 0;
}

void ae_test_run_edit_end(struct ae_test_run_edit *edit)
{
	edit->cursor = (int)strlen(edit->text);
}

void ae_test_run_edit_left(struct ae_test_run_edit *edit)
{
	edit->cursor = previous_start(edit->text, edit->cursor);
}

void ae_test_run_edit_right(struct ae_test_run_edit *edit)
{
	int length = character_length((const unsigned char *)edit->text + edit->cursor, (int)strlen(edit->text + edit->cursor));

	edit->cursor += length;
}

/* ---------- the card's text */

/* line + ELLIPSIS cut to fit width: characters come off the end (and spaces before the "…") until it fits */
static void with_ellipsis(char *line, int line_size, float width, ae_test_run_measure measure, void *context)
{
	int length = (int)strlen(line);

	for (;;)
	{
		char probe[256];

		while (length > 0 && line[length - 1] == ' ')
			length--;
		if (length + 4 > line_size || length + 4 > (int)sizeof(probe))
		{
			length = previous_start(line, length - 1 >= 0 ? length - 1 : 0);
			continue;
		}
		memcpy(probe, line, (size_t)length);
		memcpy(probe + length, ELLIPSIS, sizeof(ELLIPSIS));
		if (length == 0 || measure(probe, context) <= width)
		{
			memcpy(line, probe, (size_t)length + sizeof(ELLIPSIS));
			return;
		}
		length = previous_start(line, length);
	}
}

int ae_test_run_card_wrap(const char *text, float width, int max_lines, ae_test_run_measure measure, void *context,
	char *lines, int line_size, int *truncated)
{
	int count = 0, at = 0, total = (int)strlen(text);

	*truncated = 0;
	if (max_lines < 1 || line_size < 8)
		return 0;
	while (at < total && text[at] == ' ')
		at++;
	while (at < total && count < max_lines)
	{
		char *line = lines + count * line_size;
		char probe[256];
		int fit = 0, last_space = -1, take, length;

		/* fit: the most bytes (whole characters, at least one) that measure within width */
		while (at + fit < total)
		{
			int step = character_length((const unsigned char *)text + at + fit, total - at - fit);

			if (!step)
				step = 1;
			if (fit + step >= line_size || fit + step >= (int)sizeof(probe))
				break;
			memcpy(probe, text + at, (size_t)(fit + step));
			probe[fit + step] = 0;
			if (fit > 0 && measure(probe, context) > width)
				break;
			if (fit > 0 && text[at + fit] == ' ')
				last_space = fit;
			fit += step;
		}
		if (at + fit >= total || text[at + fit] == ' ')
			take = fit;                       /* (the rest, or a break right at a space) */
		else if (last_space > 0)
			take = last_space;                /* (back to the word before) */
		else
			take = fit;                       /* (one word wider than a line: cut) */
		memcpy(line, text + at, (size_t)take);
		length = take;
		while (length > 0 && line[length - 1] == ' ')
			length--;
		line[length] = 0;
		at += take;
		while (at < total && text[at] == ' ')
			at++;
		count++;
	}
	if (at < total && count > 0)
	{
		with_ellipsis(lines + (count - 1) * line_size, line_size, width, measure, context);
		*truncated = 1;
	}
	return count;
}
