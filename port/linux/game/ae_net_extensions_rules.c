/*
AE_NET_EXTENSIONS_RULES.C

See ae_net_extensions_rules.h.
*/

#include <string.h>

#include "ae_net_extensions_rules.h"

int ae_netext_key_valid(char const *key, long length)
{
	long index;

	if (!key || length < 1 || length > AE_NETEXT_MAXIMUM_KEY)
		return 0;
	for (index = 0; index < length; index++)
	{
		char c = key[index];

		if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '.'))
			return 0;
	}

	return 1;
}

long ae_netext_build(unsigned char kind, struct ae_netext_capability const *table, long table_count,
	unsigned long required_mask, unsigned char *out, long capacity)
{
	long size, index;

	if (!out || !table || table_count < 0 || table_count > AE_NETEXT_MAXIMUM_ENTRIES ||
		capacity < AE_NETEXT_MAGIC_LENGTH + 3 + 1)
		return -1;
	if (capacity > AE_NETEXT_MAXIMUM_BYTES + 1)
		capacity = AE_NETEXT_MAXIMUM_BYTES + 1;
	memcpy(out, AE_NETEXT_MAGIC, AE_NETEXT_MAGIC_LENGTH);
	size = AE_NETEXT_MAGIC_LENGTH;
	out[size++] = AE_NETEXT_FORMAT;
	out[size++] = kind;
	out[size++] = (unsigned char)table_count;
	for (index = 0; index < table_count; index++)
	{
		long key_length = (long)strlen(table[index].key);
		/* (the host's text for a machine without it; none in an answer) */
		char const *text = kind == AE_NETEXT_KIND_ANNOUNCEMENT && table[index].missing_notice ?
			table[index].missing_notice : "";
		long text_length = (long)strlen(text);

		if (!ae_netext_key_valid(table[index].key, key_length))
			return -1;
		if (text_length > AE_NETEXT_MAXIMUM_TEXT)
			text_length = AE_NETEXT_MAXIMUM_TEXT;
		if (size + 1 + key_length + 4 + text_length + 1 > capacity)
			return -1;
		out[size++] = (unsigned char)key_length;
		memcpy(out + size, table[index].key, (size_t)key_length);
		size += key_length;
		out[size++] = (unsigned char)(table[index].version & 0xFF);
		out[size++] = (unsigned char)(table[index].version >> 8);
		out[size++] = (unsigned char)(index < 32 && ((required_mask >> index) & 1) ? AE_NETEXT_FLAG_REQUIRED : 0);
		out[size++] = (unsigned char)text_length;
		memcpy(out + size, text, (size_t)text_length);
		size += text_length;
	}
	out[size++] = AE_NETEXT_PACKET_TYPE;

	return size;
}

int ae_netext_parse(void const *data, long size, unsigned char kind, struct ae_netext_capability const *table,
	long table_count, struct ae_netext_peer *peer)
{
	unsigned char const *bytes = data;
	long at, count, index, entry;

	if (!data || !peer || !table || table_count < 0)
		return AE_NETEXT_PARSE_MALFORMED;
	memset(peer, 0, sizeof(*peer));
	/* (the type byte that ends the packet is not the message's) */
	size--;
	if (size < AE_NETEXT_MAGIC_LENGTH + 3 || memcmp(bytes, AE_NETEXT_MAGIC, AE_NETEXT_MAGIC_LENGTH) ||
		bytes[AE_NETEXT_MAGIC_LENGTH] != AE_NETEXT_FORMAT || bytes[AE_NETEXT_MAGIC_LENGTH + 1] != kind)
		return size > AE_NETEXT_MAXIMUM_BYTES ? AE_NETEXT_PARSE_MALFORMED : AE_NETEXT_PARSE_OTHER;
	if (size > AE_NETEXT_MAXIMUM_BYTES)
		return AE_NETEXT_PARSE_MALFORMED;
	count = bytes[AE_NETEXT_MAGIC_LENGTH + 2];
	if (count > AE_NETEXT_MAXIMUM_ENTRIES)
		return AE_NETEXT_PARSE_MALFORMED;
	at = AE_NETEXT_MAGIC_LENGTH + 3;
	for (entry = 0; entry < count; entry++)
	{
		long length, text_length, known = -1;
		unsigned short version;
		unsigned char flags;

		if (at >= size)
			goto malformed;
		length = bytes[at++];
		/* (the key, version, flags and the text's length) */
		if (at + length + 4 > size || !ae_netext_key_valid((char const *)bytes + at, length))
			goto malformed;
		text_length = bytes[at + length + 3];
		if (text_length > AE_NETEXT_MAXIMUM_TEXT || at + length + 4 + text_length > size)
			goto malformed;
		for (index = 0; index < peer->count; index++)
		{
			if ((long)strlen(peer->names[index]) == length && !memcmp(peer->names[index], bytes + at, (size_t)length))
				goto malformed;
		}
		for (index = 0; index < table_count; index++)
		{
			if ((long)strlen(table[index].key) == length && !memcmp(table[index].key, bytes + at, (size_t)length))
				known = index;
		}
		version = (unsigned short)(bytes[at + length] | (bytes[at + length + 1] << 8));
		flags = bytes[at + length + 2];
		memcpy(peer->names[peer->count], bytes + at, (size_t)length);
		peer->names[peer->count][length] = 0;
		peer->versions[peer->count] = version;
		peer->flags[peer->count] = flags;
		peer->count++;
		/* (a capability the game requires that the reader has not: kept with its text, printable) */
		if ((flags & AE_NETEXT_FLAG_REQUIRED) && !peer->missing_required &&
			(known < 0 || table[known].version < version))
		{
			long character;

			peer->missing_required = 1;
			memcpy(peer->missing_key, bytes + at, (size_t)length);
			peer->missing_key[length] = 0;
			for (character = 0; character < text_length; character++)
			{
				unsigned char c = bytes[at + length + 4 + character];

				peer->missing_text[character] = c >= 32 && c <= 126 ? (char)c : '?';
			}
			peer->missing_text[text_length] = 0;
		}
		at += length + 4 + text_length;
	}
	if (at != size)
		goto malformed;

	return AE_NETEXT_PARSE_OK;

malformed:
	memset(peer, 0, sizeof(*peer));

	return AE_NETEXT_PARSE_MALFORMED;
}

long ae_netext_firefight_build(unsigned long const *values, unsigned char const *widths, int count, unsigned char *out,
	long capacity)
{
	long size = 0, need = AE_NETEXT_MAGIC_LENGTH + 4 + 1;
	int index;

	if (!values || !widths || !out || count < 1 || count > AE_NETEXT_FIREFIGHT_MAXIMUM_RULES)
		return -1;
	for (index = 0; index < count; index++)
	{
		if (widths[index] != 1 && widths[index] != 2 && widths[index] != 4)
			return -1;
		need += widths[index];
	}
	if (need > capacity || need - 1 > AE_NETEXT_MAXIMUM_BYTES)
		return -1;
	memcpy(out, AE_NETEXT_MAGIC, AE_NETEXT_MAGIC_LENGTH);
	size = AE_NETEXT_MAGIC_LENGTH;
	out[size++] = AE_NETEXT_FORMAT;
	out[size++] = AE_NETEXT_KIND_FIREFIGHT;
	out[size++] = AE_NETEXT_FIREFIGHT_VERSION;
	out[size++] = (unsigned char)count;
	for (index = 0; index < count; index++)
	{
		int byte;

		for (byte = 0; byte < widths[index]; byte++)
			out[size++] = (unsigned char)((values[index] >> (8 * byte)) & 0xFF);
	}
	out[size++] = AE_NETEXT_PACKET_TYPE;

	return size;
}

int ae_netext_firefight_parse(void const *data, long size, unsigned char const *widths, int count, unsigned long *values)
{
	unsigned char const *bytes = data;
	long at, need = AE_NETEXT_MAGIC_LENGTH + 4;
	int index;

	if (!data || !widths || !values || count < 1 || count > AE_NETEXT_FIREFIGHT_MAXIMUM_RULES)
		return AE_NETEXT_PARSE_MALFORMED;
	/* (the type byte that ends the packet is not the message's) */
	size--;
	if (size < AE_NETEXT_MAGIC_LENGTH + 2 || memcmp(bytes, AE_NETEXT_MAGIC, AE_NETEXT_MAGIC_LENGTH) ||
		bytes[AE_NETEXT_MAGIC_LENGTH] != AE_NETEXT_FORMAT || bytes[AE_NETEXT_MAGIC_LENGTH + 1] != AE_NETEXT_KIND_FIREFIGHT)
		return AE_NETEXT_PARSE_OTHER;
	if (size > AE_NETEXT_MAXIMUM_BYTES || size < need || bytes[AE_NETEXT_MAGIC_LENGTH + 2] != AE_NETEXT_FIREFIGHT_VERSION ||
		bytes[AE_NETEXT_MAGIC_LENGTH + 3] != count)
		return AE_NETEXT_PARSE_MALFORMED;
	for (index = 0; index < count; index++)
		need += widths[index];
	if (size != need)
		return AE_NETEXT_PARSE_MALFORMED;
	at = AE_NETEXT_MAGIC_LENGTH + 4;
	for (index = 0; index < count; index++)
	{
		unsigned long value = 0;
		int byte;

		for (byte = 0; byte < widths[index]; byte++)
			value |= (unsigned long)bytes[at++] << (8 * byte);
		values[index] = value;
	}

	return AE_NETEXT_PARSE_OK;
}

int ae_netext_firefight_send(int firefight_game, int machine_has_ae, int changed)
{
	return firefight_game && machine_has_ae && changed;
}

unsigned short ae_netext_peer_version(struct ae_netext_peer const *peer, char const *key)
{
	int index;

	if (!peer || !key)
		return 0;
	for (index = 0; index < peer->count && index < AE_NETEXT_MAXIMUM_ENTRIES; index++)
	{
		if (!strcmp(peer->names[index], key))
			return peer->versions[index];
	}

	return 0;
}

long ae_netext_first_missing(struct ae_netext_peer const *peer, struct ae_netext_capability const *table,
	long table_count, unsigned long required_mask)
{
	long index;

	for (index = 0; index < table_count && index < 32; index++)
	{
		if (((required_mask >> index) & 1) && ae_netext_peer_version(peer, table[index].key) < table[index].version)
			return index;
	}

	return -1;
}
