/*
AE_LOBBY_ROSTER.C

The roster's order and names (ae_lobby_roster.h). Pure.
*/

#include <string.h>

#include "ae_lobby_roster.h"

/* host first, then by machine; equal ones keep their order (the game's local order) */
static int before(struct ae_lobby_player const *a, struct ae_lobby_player const *b)
{
	if (a->host != b->host)
		return a->host != 0;
	return a->machine < b->machine;
}

void ae_lobby_roster_order(struct ae_lobby_roster *roster)
{
	short index, count;

	if (!roster)
		return;
	count = roster->count;
	if (count < 0)
		count = 0;
	if (count > AE_LOBBY_MAXIMUM_PLAYERS)
		count = AE_LOBBY_MAXIMUM_PLAYERS;
	roster->count = count;
	/* (insertion sort: stable, at most 128) */
	for (index = 1; index < count; index++)
	{
		struct ae_lobby_player moving = roster->players[index];
		short place = index;

		while (place > 0 && before(&moving, &roster->players[place - 1]))
		{
			roster->players[place] = roster->players[place - 1];
			place--;
		}
		roster->players[place] = moving;
	}
	for (index = 0; index < count; index++)
		roster->players[index].slot = (short)(index + 1);
}

/* a UTF-8 sequence's length from its first byte (1 for a stray continuation byte) */
static int sequence_length(unsigned char lead)
{
	if (lead >= 0xF0)
		return 4;
	if (lead >= 0xE0)
		return 3;
	if (lead >= 0xC0)
		return 2;
	return 1;
}

int ae_lobby_copy_utf8(char *out, int size, const char *text)
{
	int length = 0;

	if (!out || size <= 0)
		return 0;
	if (text)
	{
		while (text[length])
		{
			int step = sequence_length((unsigned char)text[length]), index;

			/* (a sequence cut short by the text's end: stop) */
			for (index = 1; index < step; index++)
				if (!text[length + index])
					step = 0;
			if (step == 0 || length + step > size - 1)
				break;
			length += step;
		}
		memcpy(out, text, (size_t)length);
	}
	out[length] = 0;
	return length;
}

int ae_lobby_utf16_to_utf8(char *out, int size, const unsigned short *text, int count)
{
	int length = 0, index;

	if (!out || size <= 0)
		return 0;
	for (index = 0; text && index < count && text[index]; index++)
	{
		unsigned long code = text[index];
		unsigned char bytes[4];
		int step;

		/* (a surrogate pair: one character; a lone surrogate: U+FFFD) */
		if (code >= 0xD800 && code <= 0xDBFF && index + 1 < count && text[index + 1] >= 0xDC00 &&
			text[index + 1] <= 0xDFFF)
		{
			code = 0x10000 + ((code - 0xD800) << 10) + (text[index + 1] - 0xDC00);
			index++;
		}
		else if (code >= 0xD800 && code <= 0xDFFF)
			code = 0xFFFD;
		if (code < 0x80)
		{
			bytes[0] = (unsigned char)code;
			step = 1;
		}
		else if (code < 0x800)
		{
			bytes[0] = (unsigned char)(0xC0 | (code >> 6));
			bytes[1] = (unsigned char)(0x80 | (code & 0x3F));
			step = 2;
		}
		else if (code < 0x10000)
		{
			bytes[0] = (unsigned char)(0xE0 | (code >> 12));
			bytes[1] = (unsigned char)(0x80 | ((code >> 6) & 0x3F));
			bytes[2] = (unsigned char)(0x80 | (code & 0x3F));
			step = 3;
		}
		else
		{
			bytes[0] = (unsigned char)(0xF0 | (code >> 18));
			bytes[1] = (unsigned char)(0x80 | ((code >> 12) & 0x3F));
			bytes[2] = (unsigned char)(0x80 | ((code >> 6) & 0x3F));
			bytes[3] = (unsigned char)(0x80 | (code & 0x3F));
			step = 4;
		}
		if (length + step > size - 1)
			break;
		memcpy(out + length, bytes, (size_t)step);
		length += step;
	}
	out[length] = 0;
	return length;
}
