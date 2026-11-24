/*
AE_CREDITS_RULES.C

See ae_credits_rules.h.
*/

#include <string.h>

#include "ae_credits_rules.h"

static void append(struct ae_credits_block *block, char const *text, size_t length)
{
	size_t used = strlen(block->text);

	if (used && used + 1 < sizeof(block->text))
		block->text[used++] = ' ';
	if (used + length >= sizeof(block->text))
		length = sizeof(block->text) - 1 - used;
	memcpy(block->text + used, text, length);
	block->text[used + length] = 0;
}

int ae_credits_parse(const char *text, struct ae_credits_block *out, int maximum)
{
	int count = 0, open = 0;

	if (!text || !out || maximum < 1)
		return 0;
	while (*text)
	{
		char const *end = strchr(text, '\n'), *line = text;
		size_t length = end ? (size_t)(end - text) : strlen(text);
		short kind = AE_CREDITS_PARAGRAPH;

		text = end ? end + 1 : text + length;
		while (length && (line[length - 1] == '\r' || line[length - 1] == ' ' || line[length - 1] == '\t'))
			length--;
		while (length && (*line == ' ' || *line == '\t'))
		{
			line++;
			length--;
		}
		if (!length)
		{
			open = 0;
			continue;
		}
		if (length >= 2 && line[0] == '#' && line[1] == ' ')
		{
			kind = AE_CREDITS_HEADING;
			line += 2;
			length -= 2;
		}
		else if (length >= 2 && line[0] == '-' && line[1] == ' ')
		{
			kind = AE_CREDITS_BULLET;
			line += 2;
			length -= 2;
		}
		else if (open && count > 0 && out[count - 1].kind != AE_CREDITS_HEADING)
		{
			append(&out[count - 1], line, length);
			continue;
		}
		if (count == maximum)
			break;
		memset(&out[count], 0, sizeof(out[count]));
		out[count].kind = kind;
		append(&out[count], line, length);
		count++;
		/* (a heading is one line: the text after it starts a block of its own) */
		open = kind != AE_CREDITS_HEADING;
	}
	return count;
}

const char *ae_credits_fallback(void)
{
	return
		"# ARENA EVOLVED\n"
		"Halo CE: Arena Evolved is made by the Arena Evolved contributors, on OpenCE (AGPL-3.0) and ChupathingyCE (GPL-3.0). "
		"It is free software under the GNU Affero General Public License 3.0 or later, and a non-commercial fan project.\n"
		"# YOU NEED YOUR OWN HALO FILES\n"
		"Arena Evolved includes none of the game's maps, sounds or art. It plays from your own copy of Halo: Combat Evolved.\n"
		"# MORE\n"
		"The full list of credits is in CREDITS.md and README.md, beside the game or in its source.\n";
}
