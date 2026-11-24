/*
AE_MODS_LIST_RULES.C

The mods list's pure rules (ae_mods_list_rules.h).
*/

#include "ae_mods_list_rules.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* the length of the UTF-8 character at `text`, 0 when it is broken (a stray continuation byte, a cut sequence, an
over-long form, a surrogate or a value above U+10FFFF) */
static int utf8_length(const unsigned char *text)
{
	unsigned long value;
	int length, index;

	if (text[0] < 0x80)
		return 1;
	if (text[0] >= 0xC2 && text[0] <= 0xDF)
		length = 2, value = text[0] & 0x1F;
	else if (text[0] >= 0xE0 && text[0] <= 0xEF)
		length = 3, value = text[0] & 0x0F;
	else if (text[0] >= 0xF0 && text[0] <= 0xF4)
		length = 4, value = text[0] & 0x07;
	else
		return 0;
	for (index = 1; index < length; index++)
	{
		if ((text[index] & 0xC0) != 0x80)
			return 0;
		value = (value << 6) | (text[index] & 0x3F);
	}
	if ((length == 3 && value < 0x800) || (length == 4 && (value < 0x10000 || value > 0x10FFFF)) ||
		(value >= 0xD800 && value <= 0xDFFF))
	{
		return 0;
	}
	return length;
}

size_t ae_mods_text_clean(char *destination, size_t size, const char *source, int characters, int lines)
{
	const unsigned char *text = (const unsigned char *)source;
	size_t length = 0;
	int count = 0, space = 0, cut = 0;

	if (!destination || !size)
		return 0;
	destination[0] = 0;
	if (!text || characters <= 0)
		return 0;
	while (*text)
	{
		int step = utf8_length(text);
		int line_break = lines && *text == '\n';

		/* (a control character or a broken one: a space; a line break kept as one when lines) */
		if (!step || (*text < 0x20 && !line_break) || *text == 0x7F)
		{
			space = length > 0;
			text += step ? step : 1;
			continue;
		}
		if (*text == ' ')
		{
			space = length > 0;
			text++;
			continue;
		}
		if (line_break)
		{
			if (length && destination[length - 1] != '\n')
			{
				if (count + 1 > characters || length + 1 >= size)
				{
					cut = 1;
					break;
				}
				destination[length++] = '\n';
				count++;
			}
			space = 0;
			text++;
			continue;
		}
		if (space && destination[length - 1] != '\n')
		{
			if (count + 1 > characters || length + 1 >= size)
			{
				cut = 1;
				break;
			}
			destination[length++] = ' ';
			count++;
		}
		space = 0;
		if (count + 1 > characters || length + (size_t)step >= size)
		{
			cut = 1;
			break;
		}
		memcpy(destination + length, text, (size_t)step);
		length += (size_t)step;
		count++;
		text += step;
	}
	/* (cut: room made for "..." within the characters and the bytes, whole characters taken back) */
	if (cut)
	{
		while (length && (count + 3 > characters || length + 4 > size))
		{
			do
				length--;
			while (length && ((unsigned char)destination[length] & 0xC0) == 0x80);
			count--;
		}
		while (length && (destination[length - 1] == ' ' || destination[length - 1] == '\n'))
		{
			length--;
			count--;
		}
		if (length + 4 <= size && count + 3 <= characters)
		{
			memcpy(destination + length, "...", 3);
			length += 3;
		}
	}
	while (length && (destination[length - 1] == ' ' || destination[length - 1] == '\n'))
		length--;
	destination[length] = 0;
	return length;
}

/* whether `key` ends with `extension`, any case */
static int ends_with(const char *key, const char *extension)
{
	size_t key_length = strlen(key), extension_length = strlen(extension), index;

	if (key_length <= extension_length)
		return 0;
	for (index = 0; index < extension_length; index++)
	{
		if (tolower((unsigned char)key[key_length - extension_length + index]) != extension[index])
			return 0;
	}
	return 1;
}

int ae_mods_group_of(const char *key)
{
	if (!key)
		return AE_MODS_GROUP_OTHER;
	if (ends_with(key, ".sound"))
		return AE_MODS_GROUP_SOUND;
	if (ends_with(key, ".bitmap"))
		return AE_MODS_GROUP_BITMAP;
	if (ends_with(key, ".unicode_string_list") || ends_with(key, ".string_list"))
		return AE_MODS_GROUP_TEXT;
	if (ends_with(key, ".gbxmodel") || ends_with(key, ".model"))
		return AE_MODS_GROUP_MODEL;
	return AE_MODS_GROUP_OTHER;
}

int ae_mods_state_of(int refused, int off, int played, int active)
{
	if (refused)
		return AE_MODS_STATE_REFUSED;
	if (off)
		return AE_MODS_STATE_OFF;
	return played && active ? AE_MODS_STATE_ON : AE_MODS_STATE_WAITS;
}

long ae_mods_priority_places(long count, long position, long priority)
{
	if (count <= 0 || position < 0 || position >= count)
		return 0;
	if (priority < 1)
		priority = 1;
	if (priority > count)
		priority = count;
	return (count - priority) - position;
}

long ae_mods_priority_of(long count, long position)
{
	return position >= 0 && position < count ? count - position : 0;
}

static int claim_compare(const void *first, const void *second)
{
	const struct ae_mods_claim *one = first, *other = second;
	int order = strcmp(one->key, other->key);

	if (order)
		return order;
	return one->position < other->position ? -1 : one->position > other->position;
}

/* the pair of (first, second) in overlaps, added when there is room; how many pairs there are */
static int overlap_add(struct ae_mods_overlap *overlaps, int count, int most, const struct ae_mods_claim *earlier,
	const struct ae_mods_claim *later)
{
	int index;

	for (index = 0; index < count && index < most; index++)
	{
		if (overlaps[index].first == earlier->mod && overlaps[index].second == later->mod)
		{
			overlaps[index].files++;
			return count;
		}
	}
	if (count < most)
	{
		overlaps[count].first = earlier->mod;
		overlaps[count].second = later->mod;
		overlaps[count].files = 1;
		overlaps[count].winner = later->mod;
	}
	return count + 1;
}

int ae_mods_overlaps(struct ae_mods_claim *claims, long count, struct ae_mods_overlap *overlaps, int most)
{
	long start, end, one, other;
	int pairs = 0;

	if (!claims || count < 2)
		return 0;
	qsort(claims, (size_t)count, sizeof(*claims), claim_compare);
	for (start = 0; start < count; start = end)
	{
		for (end = start + 1; end < count && !strcmp(claims[end].key, claims[start].key); end++)
			;
		/* (each pair of the key's mods, the earlier one first: the later one's file plays) */
		for (one = start; one < end; one++)
		{
			for (other = one + 1; other < end; other++)
			{
				if (claims[one].mod != claims[other].mod)
					pairs = overlap_add(overlaps, pairs, most, &claims[one], &claims[other]);
			}
		}
	}
	return pairs;
}
