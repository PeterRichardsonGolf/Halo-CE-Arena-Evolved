/*
AE_MAP_KIND.C

(Arena Evolved) a map's kind by its identity (ae_map_kind.h). Pure C: the units test it alone.
*/

#include "ae_map_kind.h"
#include "../src/ae_mcc.h"

#include <string.h>

/* (an MCC identity is a level name of GulchCE's loader, at most 63 characters: mcc_runtime.h level_name) */
enum { AE_MAP_KIND_NAME_SIZE = 0x80, AE_MAP_KIND_PACK_ID_LENGTH = 8, AE_MAP_KIND_WORKSHOP_DIGITS = 20,
	AE_MAP_KIND_MCC_NAME_SIZE = 64 };

static int lower(int character)
{
	return character >= 'A' && character <= 'Z' ? character - 'A' + 'a' : character;
}

/* whether text's first `length` characters are `word`, ignoring case */
static int starts(const char *text, const char *word)
{
	size_t index;

	for (index = 0; word[index]; index++)
	{
		if (lower((unsigned char)text[index]) != word[index])
			return 0;
	}
	return 1;
}

static int ends(const char *text, size_t length, const char *word)
{
	size_t word_length = strlen(word);

	return length > word_length && starts(text + length - word_length, word);
}

/* a file's name before its @: something, no folder, not "." or ".." */
static int file_name_valid(const char *name, size_t length)
{
	size_t index;

	if (!length || name[0] == '.')
		return 0;
	for (index = 0; index < length; index++)
	{
		if (name[index] == '\\' || name[index] == '/' || name[index] == ':' || (unsigned char)name[index] < 32)
			return 0;
	}
	return 1;
}

static int pack_id(const char *id)
{
	static const char *const reserved[] = { "ce", "md", "pc", "de", "fr", "es", "it", "mcc" };
	size_t length = strlen(id), index;

	if (!length || length > AE_MAP_KIND_PACK_ID_LENGTH)
		return 0;
	for (index = 0; index < length; index++)
	{
		int character = lower((unsigned char)id[index]);

		if (!((character >= 'a' && character <= 'z') || (character >= '0' && character <= '9')))
			return 0;
	}
	for (index = 0; index < sizeof(reserved) / sizeof(reserved[0]); index++)
	{
		if (strlen(reserved[index]) == length && starts(id, reserved[index]))
			return 0;
	}
	return 1;
}

int ae_map_kind_mcc_stock_file(const char *identity, char *file, unsigned long size)
{
	size_t length;
	char name[AE_MAP_KIND_NAME_SIZE];

	if (!identity || !file || !size)
		return 0;
	length = strlen(identity);
	if (length >= AE_MAP_KIND_NAME_SIZE || !ends(identity, length, "@mcc") || !file_name_valid(identity, length - 4))
		return 0;
	memcpy(name, identity, length - 4);
	name[length - 4] = 0;
	if (!ae_mcc_stock_map_name(name) || length - 4 >= size)
		return 0;
	/* (MCC's files are named in lower case, as the stock names are: a case-sensitive file system finds them) */
	for (length = 0; name[length]; length++)
		file[length] = (char)lower((unsigned char)name[length]);
	file[length] = 0;
	return 1;
}

int ae_map_kind_of(const char *identity)
{
	const char *at, *base, *cursor;
	size_t length;
	char file[AE_MAP_KIND_NAME_SIZE];

	if (!identity || !identity[0] || (length = strlen(identity)) >= AE_MAP_KIND_NAME_SIZE)
		return AE_MAP_KIND_INVALID;
	if (starts(identity, "mcc_maps\\") || starts(identity, "mcc_maps/"))
		return AE_MAP_KIND_MCC_WORKSHOP;
	if (starts(identity, "custom_maps\\") || starts(identity, "maps_md\\") || starts(identity, "maps_pc\\"))
		return AE_MAP_KIND_CE;
	at = strrchr(identity, '@');
	if (at && starts(at, "@mcc") && !at[4])
		return ae_map_kind_mcc_stock_file(identity, file, sizeof(file)) ? AE_MAP_KIND_MCC_STOCK : AE_MAP_KIND_INVALID;
	if (at && starts(at, "@ws"))
	{
		size_t digits = strlen(at + 3);

		if (!file_name_valid(identity, (size_t)(at - identity)) || !digits || digits > AE_MAP_KIND_WORKSHOP_DIGITS ||
			length >= AE_MAP_KIND_MCC_NAME_SIZE)
		{
			return AE_MAP_KIND_INVALID;
		}
		for (cursor = at + 3; *cursor; cursor++)
		{
			if (*cursor < '0' || *cursor > '9')
				return AE_MAP_KIND_INVALID;
		}
		return AE_MAP_KIND_MCC_WORKSHOP;
	}
	if (ends(identity, length, "@ce") || ends(identity, length, "@pc") || ends(identity, length, "@md"))
		return AE_MAP_KIND_CE;
	base = identity;
	for (cursor = identity; *cursor; cursor++)
	{
		if (*cursor == '\\' || *cursor == '/')
			base = cursor + 1;
	}
	if (at && at > base && pack_id(at + 1))
		return AE_MAP_KIND_PACK;
	return AE_MAP_KIND_XBOX;
}

int ae_map_kind_workshop_parts(const char *identity, char *name, unsigned long name_size, char *id, unsigned long id_size)
{
	const char *at;

	if (!name || !id || !name_size || !id_size || ae_map_kind_of(identity) != AE_MAP_KIND_MCC_WORKSHOP ||
		!(at = strrchr(identity, '@')) || !starts(at, "@ws"))
	{
		return 0;
	}
	if ((unsigned long)(at - identity) >= name_size || strlen(at + 3) >= id_size)
		return 0;
	memcpy(name, identity, (size_t)(at - identity));
	name[at - identity] = 0;
	memcpy(id, at + 3, strlen(at + 3) + 1);
	return 1;
}
