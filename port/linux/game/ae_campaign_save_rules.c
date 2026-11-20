/*
AE_CAMPAIGN_SAVE_RULES.C

A custom campaign's saved game file name. See ae_campaign_save_rules.h.
*/

#include <stdio.h>
#include <string.h>

#include "ae_campaign_save_rules.h"

static unsigned long name_hash(const char *text)
{
	unsigned long hash = 2166136261ul;

	for (; *text; text++)
		hash = ((hash ^ (unsigned char)*text) * 16777619ul) & 0xFFFFFFFFul;
	return hash;
}

int ae_campaign_save_file_name(const char *package, char *file, int size)
{
	static const char hex[] = "0123456789abcdef";
	char written[AE_CAMPAIGN_SAVE_NAME_KEPT + 24];
	const unsigned char *cursor;
	int length = 0, cut = 0;

	if (file && size > 0)
		file[0] = 0;
	if (!package || !package[0] || !file || size < AE_CAMPAIGN_SAVE_FILE_SIZE)
		return 0;
	for (cursor = (const unsigned char *)package; *cursor; cursor++)
	{
		unsigned char c = *cursor;
		int plain = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-';
		int need = plain ? 1 : 3;

		if (length + need > AE_CAMPAIGN_SAVE_NAME_KEPT)
		{
			cut = 1;
			break;
		}
		if (plain)
			written[length++] = (char)c;
		else
		{
			written[length++] = '_';
			written[length++] = hex[c >> 4];
			written[length++] = hex[c & 15];
		}
	}
	/* (a long name: its first part, then "_h" and the whole name's hash; 11 characters made room for) */
	if (cut)
	{
		while (length > AE_CAMPAIGN_SAVE_NAME_KEPT - 10)
		{
			/* (never half an escape: back to before its '_') */
			length--;
			if (length >= 2 && written[length - 2] == '_')
				length -= 2;
			else if (length >= 1 && written[length - 1] == '_')
				length -= 1;
		}
		length += snprintf(written + length, sizeof(written) - (size_t)length, "_h%08lx", name_hash(package));
	}
	written[length] = 0;
	snprintf(file, (size_t)size, "savegame-%s.bin", written);
	return 1;
}
