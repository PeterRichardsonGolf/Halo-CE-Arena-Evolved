/*
AE_FIREFIGHT_VARIANTS_RULES.C

See ae_firefight_variants_rules.h. A line is "v1|<name>|<value>,<value>...|<skull word>,<skull word>|<item set>": decimal values,
the words and the set in hexadecimal.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ae_firefight_variants_rules.h"

#define VERSION "v1"

static int letter_up(int c)
{
	return c >= 'a' && c <= 'z' ? c - 32 : c;
}

int ae_ffv_name_clean(const char *typed, char *name, size_t size)
{
	size_t used = 0;
	int space = 0;

	if (!name || size == 0)
		return 0;
	name[0] = 0;
	if (!typed)
		return 0;
	for (; *typed && used < AE_FFV_NAME - 1 && used < size - 1; typed++)
	{
		unsigned char c = (unsigned char)*typed;

		if (c == ' ')
		{
			space = used > 0;
			continue;
		}
		if (c < 0x20 || c > 0x7E || c == '|' || c == ';' || c == '"')
			continue;
		if (space && used < AE_FFV_NAME - 2 && used < size - 2)
			name[used++] = ' ';
		space = 0;
		name[used++] = (char)c;
	}
	name[used] = 0;
	return used > 0;
}

int ae_ffv_name_same(const char *a, const char *b)
{
	for (; *a && *b; a++, b++)
	{
		if (letter_up((unsigned char)*a) != letter_up((unsigned char)*b))
			return 0;
	}
	return *a == *b;
}

int ae_ffv_format(struct ae_ffv_record const *record, char *text, size_t size)
{
	size_t used;
	int row;
	int written = snprintf(text, size, "%s|%s|", VERSION, record->name);

	if (written < 0 || (size_t)written >= size)
		return 0;
	used = (size_t)written;
	for (row = 0; row < AE_FFV_ROWS; row++)
	{
		written = snprintf(text + used, size - used, "%s%ld", row ? "," : "", record->raw[row]);
		if (written < 0 || (size_t)written >= size - used)
			return 0;
		used += (size_t)written;
	}
	written = snprintf(text + used, size - used, "|%lx,%lx|%lx", record->skulls[0], record->skulls[1], record->item_set);
	return written >= 0 && (size_t)written < size - used;
}

/* a field up to the next bar (or the end): its length, and the cursor past the bar; -1 when the line ends before it */
static int field(const char **cursor, char *out, size_t size, int last)
{
	const char *start = *cursor, *end = strchr(start, '|');
	size_t length;

	if (!end)
	{
		if (!last)
			return 0;
		end = start + strlen(start);
	}
	length = (size_t)(end - start);
	if (length >= size)
		return 0;
	memcpy(out, start, length);
	out[length] = 0;
	*cursor = *end ? end + 1 : end;
	return 1;
}

static int number(const char *text, int base, long *out)
{
	char *end;
	long value;

	if (!*text)
		return 0;
	value = strtol(text, &end, base);
	if (*end)
		return 0;
	*out = value;
	return 1;
}

int ae_ffv_parse(const char *line, struct ae_ffv_record *record)
{
	char version[8], name[AE_FFV_NAME + 8], values[AE_FFV_LINE], skulls[80], item[24], clean[AE_FFV_NAME], *at, *comma = NULL;
	const char *cursor = line;
	struct ae_ffv_record read;
	long number_read;
	int row;

	if (!line || !record)
		return 0;
	memset(&read, 0, sizeof(read));
	if (!field(&cursor, version, sizeof(version), 0) || strcmp(version, VERSION) || !field(&cursor, name, sizeof(name), 0) ||
		!field(&cursor, values, sizeof(values), 0) || !field(&cursor, skulls, sizeof(skulls), 0) ||
		!field(&cursor, item, sizeof(item), 1))
		return 0;
	if (!ae_ffv_name_clean(name, clean, sizeof(clean)) || strcmp(clean, name))
		return 0;
	memcpy(read.name, clean, sizeof(read.name));
	at = values;
	for (row = 0; row < AE_FFV_ROWS; row++)
	{
		comma = strchr(at, ',');
		if (comma)
			*comma = 0;
		else if (row != AE_FFV_ROWS - 1)
			return 0;
		if (!number(at, 10, &number_read) || number_read < -100000L || number_read > 100000L)
			return 0;
		read.raw[row] = number_read;
		at = comma ? comma + 1 : at + strlen(at);
	}
	if (comma)
		return 0;
	comma = strchr(skulls, ',');
	if (!comma)
		return 0;
	*comma = 0;
	if (!number(skulls, 16, &number_read))
		return 0;
	read.skulls[0] = (unsigned long)number_read & 0xFFFFFFFFUL;
	if (!number(comma + 1, 16, &number_read))
		return 0;
	read.skulls[1] = (unsigned long)number_read & 0xFFFFFFFFUL;
	if (!number(item, 16, &number_read))
		return 0;
	read.item_set = (unsigned long)number_read & 0xFFFFFFFFUL;
	*record = read;
	return 1;
}

int ae_ffv_file_read(const char *text, struct ae_ffv_record *records, int maximum)
{
	int count = 0;
	char line[AE_FFV_LINE + 128];

	while (text && *text && count < maximum)
	{
		size_t length = strcspn(text, "\r\n");

		if (length < sizeof(line))
		{
			memcpy(line, text, length);
			line[length] = 0;
			if (length && ae_ffv_parse(line, &records[count]))
			{
				int other, taken = 0;

				for (other = 0; other < count; other++)
					taken |= ae_ffv_name_same(records[other].name, records[count].name);
				if (!taken)
					count++;
			}
		}
		text += length;
		while (*text == '\r' || *text == '\n')
			text++;
	}
	return count;
}

int ae_ffv_file_write(struct ae_ffv_record const *records, int count, char *text, size_t size)
{
	size_t used = 0;
	int index;

	if (!text || size == 0)
		return 0;
	text[0] = 0;
	for (index = 0; index < count; index++)
	{
		char line[AE_FFV_LINE + 128];

		if (!ae_ffv_format(&records[index], line, sizeof(line)) || used + strlen(line) + 2 > size)
			return 0;
		used += (size_t)snprintf(text + used, size - used, "%s\n", line);
	}
	return 1;
}

int ae_ffv_list_add(struct ae_ffv_record *records, int count, struct ae_ffv_record const *record, int *dropped)
{
	int index;

	if (dropped)
		*dropped = 0;
	for (index = 0; index < count; index++)
	{
		if (ae_ffv_name_same(records[index].name, record->name))
		{
			records[index] = *record;
			return count;
		}
	}
	if (count >= AE_FFV_SAVED)
	{
		memmove(records, records + 1, sizeof(records[0]) * (size_t)(AE_FFV_SAVED - 1));
		count = AE_FFV_SAVED - 1;
		if (dropped)
			*dropped = 1;
	}
	records[count] = *record;
	return count + 1;
}

int ae_ffv_list_remove(struct ae_ffv_record *records, int count, int index)
{
	if (index < 0 || index >= count)
		return count;
	memmove(records + index, records + index + 1, sizeof(records[0]) * (size_t)(count - index - 1));
	return count - 1;
}
