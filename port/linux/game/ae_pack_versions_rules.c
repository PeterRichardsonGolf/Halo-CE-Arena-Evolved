/*
AE_PACK_VERSIONS_RULES.C

A pack map of another version: the pure rules (ae_pack_versions_rules.h). No engine includes.
*/

#include "ae_pack_versions_rules.h"

#include <stdio.h>
#include <string.h>

static int plain_name(char const *name)
{
	return name && name[0] && name[0] != '.' && !strpbrk(name, "\\/:@*?\"<>|");
}

int ae_pack_versions_folder(char *out, int size, char const *pack, unsigned long version)
{
	int length;

	if (!out || size <= 0)
		return 0;
	out[0] = 0;
	if (!plain_name(pack) || !version)
		return 0;
	length = snprintf(out, (size_t)size, "%s%s\\%08lx\\", AE_PACK_VERSIONS_ROOT, pack, version & 0xFFFFFFFFUL);
	if (length < 0 || length >= size)
	{
		out[0] = 0;
		return 0;
	}
	return 1;
}

int ae_pack_versions_path(char *out, int size, char const *pack, char const *file, unsigned long version)
{
	size_t length;

	if (!plain_name(file) || !ae_pack_versions_folder(out, size, pack, version))
		return 0;
	length = strlen(out);
	if ((int)(length + strlen(file) + 4) >= size)
	{
		out[0] = 0;
		return 0;
	}
	snprintf(out + length, (size_t)size - length, "%s.map", file);
	return 1;
}

int ae_pack_versions_parse_folder(char const *name, unsigned long *version)
{
	unsigned long value = 0;
	int index;

	if (!name)
		return 0;
	for (index = 0; index < 8; index++)
	{
		char c = name[index];
		int digit = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 :
			c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;

		if (digit < 0)
			return 0;
		value = (value << 4) | (unsigned long)digit;
	}
	if (name[8] || !value)
		return 0;
	if (version)
		*version = value;
	return 1;
}

int ae_pack_versions_evict(int count, unsigned long const *versions, unsigned long long const *last_used,
	unsigned long in_use, int keep, int *remove)
{
	char taken[AE_PACK_VERSIONS_LISTED];
	int removed = 0;
	int left = count;

	if (!versions || !last_used || !remove || count <= 0)
		return 0;
	if (count > AE_PACK_VERSIONS_LISTED)
		count = left = AE_PACK_VERSIONS_LISTED;
	if (keep < 1)
		keep = 1;
	memset(taken, 0, sizeof(taken));
	while (left > keep)
	{
		int oldest = -1;
		int index;

		for (index = 0; index < count; index++)
		{
			if (taken[index] || (in_use && versions[index] == in_use))
				continue;
			if (oldest < 0 || last_used[index] < last_used[oldest])
				oldest = index;
		}
		if (oldest < 0)
			break;
		taken[oldest] = 1;
		remove[removed++] = oldest;
		left--;
	}
	return removed;
}
