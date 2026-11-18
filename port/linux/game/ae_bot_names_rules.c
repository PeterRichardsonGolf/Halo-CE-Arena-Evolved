#include "ae_bot_names_rules.h"

#include <stdio.h>
#include <string.h>

int ae_bot_names_parse(char const *text, char names[][AE_BOT_NAMES_LENGTH + 1], int maximum)
{
	int count = 0;

	if (!text || !names || maximum <= 0)
		return 0;
	while (*text && count < maximum)
	{
		int length = 0;
		char line[AE_BOT_NAMES_LENGTH + 1];
		int first = 1, comment = 0, trailing = 0;

		while (*text && *text != '\n')
		{
			unsigned char c = (unsigned char)*text++;

			if (c == '\r')
				continue;
			if (first && (c == ' ' || c == '\t'))
				continue;
			if (first && c == '#')
				comment = 1;
			first = 0;
			if (comment || c < 0x20 || c > 0x7E)
				continue;
			if (length < AE_BOT_NAMES_LENGTH)
			{
				line[length++] = (char)c;
				trailing = (c == ' ' || c == '\t') ? trailing + 1 : 0;
			}
		}
		if (*text == '\n')
			text++;
		length -= trailing;
		if (comment || length <= 0)
			continue;
		memcpy(names[count], line, (size_t)length);
		names[count][length] = 0;
		count++;
	}
	return count;
}

void ae_bot_names_pick(char const names[][AE_BOT_NAMES_LENGTH + 1], int count, long bot_index, char *name, int size)
{
	if (!name || size <= 0)
		return;
	name[0] = 0;
	if (!names || count <= 0 || bot_index < 0)
		return;
	if (bot_index < count)
		snprintf(name, (size_t)size, "%s", names[bot_index]);
	else
		snprintf(name, (size_t)size, "%.8s %ld", names[bot_index % count], bot_index / count + 1);
}
