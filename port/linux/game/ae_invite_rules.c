/*
AE_INVITE_RULES.C

An invite's code in a player's text (ae_invite_rules.h). Pure.
*/

#include <string.h>

#include "ae_invite_rules.h"

static int blank(char c)
{
	return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static int hex_digit(char c)
{
	return (c >= '0' && c <= '9') || ((c | 0x20) >= 'a' && (c | 0x20) <= 'f');
}

int ae_invite_code(const char *text, char *code, size_t size)
{
	static const char prefix[] = "halo://join/";
	const char *start, *end, *search, *after;
	size_t digits;

	if (!text || !code || size <= AE_INVITE_CODE_DIGITS)
		return 0;
	while (blank(*text))
		text++;
	for (end = text + strlen(text); end > text && blank(end[-1]); end--)
		;
	/* (a link anywhere in the text: its code follows the prefix; else the text is the code) */
	start = text;
	for (search = text; search < end; search++)
	{
		size_t length;

		for (length = 0; prefix[length] && search + length < end && (search[length] | 0x20) == prefix[length]; length++)
			;
		if (!prefix[length])
		{
			start = search + (sizeof(prefix) - 1);
			break;
		}
	}
	for (digits = 0; start + digits < end && hex_digit(start[digits]); digits++)
		;
	after = start + digits;
	/* (a bare code is the whole text; a link's code ends at the text's end or at a character no code has) */
	if (digits != AE_INVITE_CODE_DIGITS || (start == text && after != end))
		return 0;
	for (search = start; search < after; search++)
		*code++ = (char)(*search >= 'A' && *search <= 'F' ? *search | 0x20 : *search);
	*code = 0;
	return 1;
}
