/* Unreal pak (version 7) reading rules (see the header). Pure code. */
#include <string.h>

#include "ae_pak_rules.h"

static unsigned int read32(unsigned char const *at)
{
	return (unsigned int)at[0] | ((unsigned int)at[1] << 8) | ((unsigned int)at[2] << 16) | ((unsigned int)at[3] << 24);
}

static unsigned long long read64(unsigned char const *at)
{
	return (unsigned long long)read32(at) | ((unsigned long long)read32(at + 4) << 32);
}

int ae_pak_footer(unsigned char const *footer, size_t footer_size, unsigned long long file_size, unsigned long long *index_offset,
	unsigned long long *index_size)
{
	unsigned long long offset, size;

	if (!footer || footer_size < AE_PAK_FOOTER || !index_offset || !index_size)
		return 0;
	/* (guid 16, encrypted 1, magic 4, version 4, index offset 8, index size 8, hash 20) */
	if (footer[16] != 0 || read32(footer + 17) != 0x5A6F12E1u || read32(footer + 21) != AE_PAK_VERSION)
		return 0;
	offset = read64(footer + 25);
	size = read64(footer + 33);
	if (size == 0 || size > AE_PAK_MOST_INDEX || offset > file_size || size > file_size - offset)
		return 0;
	*index_offset = offset;
	*index_size = size;
	return 1;
}

/* a string at *at (an int32 length, positive: that many bytes with the end; negative: 16 bit characters): where its text is and its
   length in characters, *at moved past it; 0 when it does not fit */
static int string_at(unsigned char const *index, size_t size, size_t *at, unsigned char const **text, long *length, int *wide)
{
	long n;

	if (*at + 4 > size)
		return 0;
	n = (long)(int)read32(index + *at);
	*at += 4;
	if (n > 0)
	{
		if (n > 4096 || *at + (size_t)n > size)
			return 0;
		*text = index + *at;
		*length = n - 1;
		*wide = 0;
		*at += (size_t)n;
		return 1;
	}
	if (n < 0)
	{
		if (n < -4096 || *at + (size_t)(-n) * 2 > size)
			return 0;
		*text = index + *at;
		*length = -n - 1;
		*wide = 1;
		*at += (size_t)(-n) * 2;
		return 1;
	}
	*text = index + *at;
	*length = 0;
	*wide = 0;
	return 1;
}

int ae_pak_index_find(unsigned char const *index, size_t index_size, unsigned long long file_size, char const *path,
	struct ae_pak_entry *entry)
{
	size_t at = 0, path_length;
	unsigned char const *text;
	long length, count, each;
	int wide;

	if (!index || !path || !entry || !(path_length = strlen(path)))
		return 0;
	/* (the mount point, then the entries' count) */
	if (!string_at(index, index_size, &at, &text, &length, &wide) || at + 4 > index_size)
		return 0;
	count = (long)(int)read32(index + at);
	at += 4;
	if (count < 0 || count > AE_PAK_MOST_ENTRIES)
		return 0;
	for (each = 0; each < count; each++)
	{
		unsigned long long offset, size, uncompressed;
		unsigned int compression, flags;
		int match;

		if (!string_at(index, index_size, &at, &text, &length, &wide))
			return 0;
		match = !wide && (size_t)length == path_length && !memcmp(text, path, path_length);
		/* (offset 8, size 8, uncompressed size 8, compression 4, hash 20) */
		if (at + 48 > index_size)
			return 0;
		offset = read64(index + at);
		size = read64(index + at + 8);
		uncompressed = read64(index + at + 16);
		compression = read32(index + at + 24);
		at += 48;
		if (compression != 0)
		{
			unsigned int blocks;

			if (at + 4 > index_size)
				return 0;
			blocks = read32(index + at);
			if (blocks > 1000000u || at + 4 + (size_t)blocks * 16 > index_size)
				return 0;
			at += 4 + (size_t)blocks * 16;
		}
		/* (flags 1, block size 4) */
		if (at + 5 > index_size)
			return 0;
		flags = index[at];
		at += 5;
		if (!match)
			continue;
		if (compression != 0 || (flags & 1u) || size != uncompressed || offset > file_size ||
			size > file_size - offset || offset + AE_PAK_RECORD > file_size - size)
			return 0;
		entry->data_offset = offset + AE_PAK_RECORD;
		entry->size = size;
		return 1;
	}
	return 0;
}

int ae_pak_record_ok(unsigned char const *record, size_t record_size, unsigned long long entry_size)
{
	if (!record || record_size < AE_PAK_RECORD)
		return 0;
	/* (offset 8, size 8, uncompressed size 8, compression 4: 0) */
	return read64(record + 8) == entry_size && read64(record + 16) == entry_size && read32(record + 24) == 0;
}
