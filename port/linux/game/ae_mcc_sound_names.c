/*
AE_MCC_SOUND_NAMES.C

(Arena Evolved) an MCC stock map's sounds by name in Custom Edition's sounds.map (ae_mcc_sound_names.h).
*/

#include "ae_mcc_sound_names.h"

#include <string.h>

static unsigned long read_long(const unsigned char *at)
{
	return (unsigned long)at[0] | (unsigned long)at[1] << 8 | (unsigned long)at[2] << 16 | (unsigned long)at[3] << 24;
}

/* the decimal number of text up to end (1), digits only */
static int number(const char *text, const char *end, unsigned long *value)
{
	unsigned long result = 0;

	if (text >= end || end - text > 9)
		return 0;
	for (; text < end; text++)
	{
		if (*text < '0' || *text > '9')
			return 0;
		result = result * 10 + (unsigned long)(*text - '0');
	}
	*value = result;
	return 1;
}

int ae_mcc_sound_name_split(const char *name, char *tag, unsigned long tag_size, unsigned long *range,
	unsigned long *permutation)
{
	const char *last, *second, *cursor;
	size_t length;

	if (!name || !tag || !tag_size || !range || !permutation)
		return 0;
	length = strlen(name);
	last = NULL;
	second = NULL;
	/* (the last two "__": a tag's own path may hold more) */
	for (cursor = name + length; cursor >= name + 2; cursor--)
	{
		if (cursor[-1] == '_' && cursor[-2] == '_')
		{
			if (!last)
				last = cursor - 2;
			else
			{
				second = cursor - 2;
				break;
			}
			cursor--;
		}
	}
	if (!last || !second || second == name || !number(second + 2, last, range) ||
		!number(last + 2, name + length, permutation) || (unsigned long)(second - name) >= tag_size)
	{
		return 0;
	}
	memcpy(tag, name, (size_t)(second - name));
	tag[second - name] = 0;
	return 1;
}

int ae_mcc_sound_permutation(const unsigned char *item, unsigned long size, unsigned long range,
	unsigned long permutation, unsigned long *offset, unsigned long *bytes)
{
	unsigned long range_count, cursor, index, at;

	if (!item || !offset || !bytes || size < AE_MCC_SOUND_HEADER_SIZE)
		return 0;
	range_count = read_long(item + AE_MCC_SOUND_PITCH_RANGES_OFFSET);
	if (range >= range_count || range_count > (size - AE_MCC_SOUND_HEADER_SIZE) / AE_MCC_SOUND_PITCH_RANGE_SIZE)
		return 0;
	/* (the permutations of the ranges before this one, then this one's) */
	cursor = AE_MCC_SOUND_HEADER_SIZE + range_count * AE_MCC_SOUND_PITCH_RANGE_SIZE;
	for (index = 0; index <= range; index++)
	{
		unsigned long count = read_long(item + AE_MCC_SOUND_HEADER_SIZE + index * AE_MCC_SOUND_PITCH_RANGE_SIZE +
			AE_MCC_PITCH_RANGE_PERMUTATIONS_OFFSET);

		if (count > (size - cursor) / AE_MCC_SOUND_PERMUTATION_SIZE)
			return 0;
		if (index == range)
		{
			if (permutation >= count)
				return 0;
			at = cursor + permutation * AE_MCC_SOUND_PERMUTATION_SIZE + AE_MCC_PERMUTATION_SAMPLES_OFFSET;
			*bytes = read_long(item + at);
			*offset = read_long(item + at + 8);
			return 1;
		}
		cursor += count * AE_MCC_SOUND_PERMUTATION_SIZE;
	}
	return 0;
}
