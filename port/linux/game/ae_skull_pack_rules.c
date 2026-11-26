/*
AE_SKULL_PACK_RULES.C

The skull pictures' index read as pure rules (ae_skull_pack_rules.h). No engine headers: the unit tests build it alone, under the
sanitizers, as the files are the player's and may be cut short or changed by an update.
*/

#include "ae_skull_pack_rules.h"

#include <ctype.h>
#include <string.h>

static unsigned int read16(unsigned char const *bytes)
{
	return (unsigned int)bytes[0] | ((unsigned int)bytes[1] << 8);
}

static unsigned int read32(unsigned char const *bytes)
{
	return read16(bytes) | (read16(bytes + 2) << 16);
}

/* the offsets inside a record: the medal pack's */
enum { RECORD_NAME = 0x04, RECORD_SIZE = 0x34, RECORD_DATA_SIZE = 0x48, RECORD_DATA_OFFSET = 0x50 };

static int is_index(unsigned char const *index, size_t index_size)
{
	return index && index_size >= AE_MEDAL_INDEX_FIRST && read32(index) == AE_MEDAL_INDEX_MAGIC;
}

static int record_inside(size_t index_size, int i)
{
	unsigned long long end;

	if (i < 0 || i >= AE_MEDAL_INDEX_MOST)
		return 0;
	end = (unsigned long long)AE_MEDAL_INDEX_FIRST + (unsigned long long)i * AE_MEDAL_INDEX_STRIDE + AE_MEDAL_INDEX_USED;
	return end <= (unsigned long long)index_size;
}

/* whether the name field holds a name: capitals and digits, ended inside the field */
static int name_ok(unsigned char const *name)
{
	int length;

	for (length = 0; length < AE_MEDAL_NAME_SIZE && name[length]; length++)
	{
		if (!((name[length] >= 'A' && name[length] <= 'Z') || (name[length] >= '0' && name[length] <= '9')))
			return 0;
	}
	return length > 0 && length < AE_MEDAL_NAME_SIZE;
}

int ae_skull_index_count(unsigned char const *index, size_t index_size)
{
	int count = 0;

	if (!is_index(index, index_size))
		return 0;
	while (record_inside(index_size, count) &&
		name_ok(index + AE_MEDAL_INDEX_FIRST + (size_t)count * AE_MEDAL_INDEX_STRIDE + RECORD_NAME))
	{
		count++;
	}
	return count;
}

int ae_skull_index_record(unsigned char const *index, size_t index_size, int i, struct ae_medal_record *record)
{
	unsigned char const *bytes;

	if (!record || !is_index(index, index_size) || !record_inside(index_size, i))
		return 0;
	bytes = index + AE_MEDAL_INDEX_FIRST + (size_t)i * AE_MEDAL_INDEX_STRIDE;
	if (!name_ok(bytes + RECORD_NAME))
		return 0;
	memset(record, 0, sizeof(*record));
	memcpy(record->name, bytes + RECORD_NAME, strlen((char const *)bytes + RECORD_NAME));
	record->width = read16(bytes + RECORD_SIZE);
	record->height = read16(bytes + RECORD_SIZE + 2);
	record->size = read32(bytes + RECORD_DATA_SIZE);
	record->offset = (unsigned long)read32(bytes + RECORD_DATA_OFFSET);
	return 1;
}

/* a key as a record names it: capitals, the underscores left out; 0 when it does not fit */
static int squash(char const *key, char *out, size_t size)
{
	size_t length = 0;

	for (; *key; key++)
	{
		if (*key == '_')
			continue;
		if (length + 1 >= size)
			return 0;
		out[length++] = (char)toupper((unsigned char)*key);
	}
	out[length] = 0;
	return length > 0;
}

int ae_skull_index_find(unsigned char const *index, size_t index_size, char const *key)
{
	/* (the names MCC spells another way: its record for the Content Evolved key) */
	static const char *const spelled[][2] = { { "BANDANNA", "BANDANA" } };
	char wanted[AE_MEDAL_NAME_SIZE];
	int count, each;
	size_t alias;

	if (!key || !squash(key, wanted, sizeof(wanted)))
		return -1;
	for (alias = 0; alias < sizeof(spelled) / sizeof(spelled[0]); alias++)
	{
		if (!strcmp(wanted, spelled[alias][0]))
		{
			memcpy(wanted, spelled[alias][1], strlen(spelled[alias][1]) + 1);
			break;
		}
	}
	count = ae_skull_index_count(index, index_size);
	for (each = 0; each < count; each++)
	{
		struct ae_medal_record record;

		if (ae_skull_index_record(index, index_size, each, &record) && !strcmp(record.name, wanted))
			return each;
	}
	return -1;
}

int ae_skull_rgba_trim(unsigned char *rgba, int width, int height, int *trimmed_width, int *trimmed_height)
{
	int left, top, right = -1, bottom = -1, x, y, out_width, out_height;

	if (!rgba || width <= 0 || height <= 0 || !trimmed_width || !trimmed_height)
		return 0;
	left = width;
	top = height;
	for (y = 0; y < height; y++)
	{
		for (x = 0; x < width; x++)
		{
			if (rgba[((size_t)y * (size_t)width + (size_t)x) * 4 + 3] <= 8)
				continue;
			if (x < left)
				left = x;
			if (x > right)
				right = x;
			if (y < top)
				top = y;
			if (y > bottom)
				bottom = y;
		}
	}
	if (right < left || bottom < top)
		return 0;
	out_width = right - left + 1;
	out_height = bottom - top + 1;
	for (y = 0; y < out_height; y++)
	{
		memmove(rgba + (size_t)y * (size_t)out_width * 4,
			rgba + ((size_t)(top + y) * (size_t)width + (size_t)left) * 4, (size_t)out_width * 4);
	}
	*trimmed_width = out_width;
	*trimmed_height = out_height;
	return 1;
}
