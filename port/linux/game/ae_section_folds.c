/*
AE_SECTION_FOLDS.C

The folded sections of CHANGE MAP's folders. See ae_section_folds.h.
*/

#include <stdio.h>
#include <string.h>

#include "ae_section_folds.h"

void ae_section_folds_init(struct ae_section_folds *folds)
{
	memset(folds, 0, sizeof(*folds));
}

static int find(struct ae_section_folds const *folds, const char *picker, const char *key)
{
	int index;

	for (index = 0; index < folds->count; index++)
	{
		if (!strcmp(folds->picker[index], picker) && !strcmp(folds->key[index], key))
			return index;
	}
	return -1;
}

int ae_section_folds_get(struct ae_section_folds const *folds, const char *picker, const char *key)
{
	return folds && picker && key && find(folds, picker, key) >= 0;
}

int ae_section_folds_set(struct ae_section_folds *folds, const char *picker, const char *key, int folded)
{
	int at;

	if (!folds || !picker || !key || !picker[0] || !key[0] || strlen(picker) >= AE_SECTION_FOLDS_PICKER ||
		strlen(key) >= AE_SECTION_FOLDS_KEY)
		return 0;
	at = find(folds, picker, key);
	if (!folded)
	{
		if (at < 0)
			return 0;
		/* (the last one into its place) */
		folds->count--;
		memcpy(folds->picker[at], folds->picker[folds->count], sizeof(folds->picker[0]));
		memcpy(folds->key[at], folds->key[folds->count], sizeof(folds->key[0]));
		return 1;
	}
	if (at >= 0 || folds->count >= AE_SECTION_FOLDS_MAXIMUM)
		return 0;
	snprintf(folds->picker[folds->count], sizeof(folds->picker[0]), "%s", picker);
	snprintf(folds->key[folds->count], sizeof(folds->key[0]), "%s", key);
	folds->count++;
	return 1;
}

int ae_section_folds_read(struct ae_section_folds *folds, const char *text)
{
	const char *line = text;

	if (!folds || !text)
		return 0;
	ae_section_folds_init(folds);
	while (*line)
	{
		const char *end = strchr(line, '\n'), *tab;
		size_t length = end ? (size_t)(end - line) : strlen(line);
		char picker[AE_SECTION_FOLDS_PICKER], key[AE_SECTION_FOLDS_KEY];

		if (length && line[length - 1] == '\r')
			length--;
		tab = memchr(line, '\t', length);
		if (line[0] != '#' && tab && tab > line && (size_t)(tab - line) < sizeof(picker) &&
			length - (size_t)(tab - line) - 1 > 0 && length - (size_t)(tab - line) - 1 < sizeof(key))
		{
			memcpy(picker, line, (size_t)(tab - line));
			picker[tab - line] = 0;
			memcpy(key, tab + 1, length - (size_t)(tab - line) - 1);
			key[length - (size_t)(tab - line) - 1] = 0;
			ae_section_folds_set(folds, picker, key, 1);
		}
		line = end ? end + 1 : line + length;
	}
	return 1;
}

long ae_section_folds_write(struct ae_section_folds const *folds, char *buffer, long size)
{
	long length;
	int index;

	if (!folds || !buffer || size <= 0)
		return -1;
	length = snprintf(buffer, (size_t)size, "# AE section folds\n");
	for (index = 0; index < folds->count && length >= 0 && length < size; index++)
		length += snprintf(buffer + length, (size_t)(size - length), "%s\t%s\n", folds->picker[index], folds->key[index]);
	return length < 0 || length >= size ? -1 : length;
}

void ae_section_folds_key(char *key, int size, const char *folder, const char *section)
{
	if (size > 0)
		snprintf(key, (size_t)size, "%s/%s", folder ? folder : "", section ? section : "");
}
