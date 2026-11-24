/*
AE_MAP_DUPES_RULES.C

The same-map rules of ae_map_dupes_rules.h.
*/

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "ae_map_alias_rules.h"
#include "ae_map_dupes_rules.h"

enum { ENTRIES_MAXIMUM = 4096 };

static char const *pack_of(char const *list_name)
{
	char const *at = list_name ? strchr(list_name, '@') : NULL;

	return at ? at + 1 : "";
}

static int same_pack(char const *list_name, char const *pack)
{
	char const *found = pack_of(list_name);
	size_t length = strlen(pack);

	if (strlen(found) != length)
		return 0;
	for (; *pack; pack++, found++)
		if (tolower((unsigned char)*pack) != tolower((unsigned char)*found))
			return 0;
	return 1;
}

int ae_map_dupe_kind(char const *list_name, int stock)
{
	char const *pack = pack_of(list_name);

	if ((pack[0] == 'w' || pack[0] == 'W') && (pack[1] == 's' || pack[1] == 'S') && isdigit((unsigned char)pack[2]))
		return AE_MAP_DUPE_WORKSHOP;
	if (same_pack(list_name, "smp") || same_pack(list_name, "pro"))
		return AE_MAP_DUPE_CONVERTED;
	if (same_pack(list_name, "cex"))
		return AE_MAP_DUPE_CEX;
	if (same_pack(list_name, "mcc"))
		return AE_MAP_DUPE_MCC;
	if (same_pack(list_name, "ce") || same_pack(list_name, "pc"))
		return AE_MAP_DUPE_CUSTOM_EDITION;
	if (same_pack(list_name, "md"))
		return AE_MAP_DUPE_HALOMD;
	if (pack[0])
		return AE_MAP_DUPE_PACK;
	return stock ? AE_MAP_DUPE_STOCK : AE_MAP_DUPE_COMMUNITY;
}

char const *ae_map_dupe_chip(int kind)
{
	static char const *const chips[AE_MAP_DUPE_KINDS] = { "WORKSHOP", "STOCK", "COMMUNITY", "CONVERTED", "CE+ X", "MCC", "CE MAP",
		"HALOMD", "MAP PACK" };

	return kind >= 0 && kind < AE_MAP_DUPE_KINDS ? chips[kind] : "";
}

char const *ae_map_dupe_help(int kind)
{
	switch (kind)
	{
	case AE_MAP_DUPE_WORKSHOP:
		return "Your Steam Workshop copy, played as MCC has it.";
	case AE_MAP_DUPE_STOCK:
		return "The stock map every player has.";
	case AE_MAP_DUPE_COMMUNITY:
		return "The community's own map file from your maps folder.";
	case AE_MAP_DUPE_CONVERTED:
		return "Our converted copy, for PCs without the Workshop item.";
	case AE_MAP_DUPE_CEX:
		return "The copy from the CE+ X map pack.";
	case AE_MAP_DUPE_MCC:
		return "A stock map from your MCC install, played as MCC has it.";
	case AE_MAP_DUPE_CUSTOM_EDITION:
		return "A Custom Edition map from your maps folders.";
	case AE_MAP_DUPE_HALOMD:
		return "A HaloMD map.";
	case AE_MAP_DUPE_PACK:
		return "A map from a map pack.";
	default:
		return "";
	}
}

int ae_map_dupe_rank(int kind)
{
	switch (kind)
	{
	case AE_MAP_DUPE_WORKSHOP:
		return 0;
	case AE_MAP_DUPE_STOCK:
	case AE_MAP_DUPE_COMMUNITY:
	case AE_MAP_DUPE_CUSTOM_EDITION:
		return 1;
	case AE_MAP_DUPE_CONVERTED:
		return 2;
	default:
		return 3;
	}
}

/* the name's letters and digits in lower case */
static void name_key(char const *name, char *key, int size)
{
	int length = 0;

	for (; name && *name && length < size - 1; name++)
		if (isalnum((unsigned char)*name))
			key[length++] = (char)tolower((unsigned char)*name);
	key[length] = 0;
}

/* the kinds a file stem is read for */
static int stem_kind(int kind)
{
	return kind == AE_MAP_DUPE_WORKSHOP || kind == AE_MAP_DUPE_CONVERTED || kind == AE_MAP_DUPE_COMMUNITY ||
		kind == AE_MAP_DUPE_CUSTOM_EDITION;
}

/* whether two kinds may be one map: one of them is a Workshop item or a converted copy */
static int may_pair(int a, int b)
{
	return stem_kind(a) && stem_kind(b) && (a == AE_MAP_DUPE_WORKSHOP || a == AE_MAP_DUPE_CONVERTED ||
		b == AE_MAP_DUPE_WORKSHOP || b == AE_MAP_DUPE_CONVERTED);
}

/* the file stem a copy is matched by, "" when its kind has none */
static void stem_key(struct ae_map_dupe_entry const *entry, char *key, int size)
{
	char file[AE_MAP_DUPE_NAME_SIZE];
	int length = 0;

	key[0] = 0;
	if (entry->kind == AE_MAP_DUPE_WORKSHOP)
	{
		if (ae_map_alias_old_file(entry->list_name, file, sizeof(file)))
			name_key(file, key, size);
		return;
	}
	if (!stem_kind(entry->kind))
		return;
	for (; entry->list_name && entry->list_name[length] && entry->list_name[length] != '@' && length < (int)sizeof(file) - 1;
		length++)
		file[length] = entry->list_name[length];
	file[length] = 0;
	name_key(file, key, size);
}

static int root_of(short *parent, int index)
{
	while (parent[index] != index)
	{
		parent[index] = parent[parent[index]];
		index = parent[index];
	}
	return index;
}

void ae_map_dupes_group(struct ae_map_dupe_entry const *entries, int count, short *head, short *order, short *versions)
{
	static char stems[ENTRIES_MAXIMUM][AE_MAP_DUPE_NAME_SIZE];
	static short parent[ENTRIES_MAXIMUM];
	int index, other;

	if (count > ENTRIES_MAXIMUM)
		count = ENTRIES_MAXIMUM;
	for (index = 0; index < count; index++)
	{
		parent[index] = (short)index;
		stem_key(&entries[index], stems[index], AE_MAP_DUPE_NAME_SIZE);
	}
	for (index = 0; index < count; index++)
	{
		if (!entries[index].groupable)
			continue;
		for (other = 0; other < index; other++)
		{
			if (!entries[other].groupable)
				continue;
			if (stems[index][0] && !strcmp(stems[index], stems[other]) &&
				may_pair(entries[index].kind, entries[other].kind))
				parent[root_of(parent, index)] = (short)root_of(parent, other);
		}
	}
	for (index = 0; index < count; index++)
	{
		int best = index, place = 0, size = 0;

		for (other = 0; other < count; other++)
		{
			int better;

			if (root_of(parent, other) != root_of(parent, index))
				continue;
			size++;
			better = ae_map_dupe_rank(entries[other].kind) < ae_map_dupe_rank(entries[best].kind) ||
				(ae_map_dupe_rank(entries[other].kind) == ae_map_dupe_rank(entries[best].kind) && other < best);
			if (better)
				best = other;
		}
		for (other = 0; other < count; other++)
		{
			int ahead;

			if (other == index || root_of(parent, other) != root_of(parent, index))
				continue;
			ahead = ae_map_dupe_rank(entries[other].kind) < ae_map_dupe_rank(entries[index].kind) ||
				(ae_map_dupe_rank(entries[other].kind) == ae_map_dupe_rank(entries[index].kind) && other < index);
			place += ahead;
		}
		head[index] = (short)best;
		order[index] = (short)place;
		versions[index] = (short)size;
	}
}

int ae_map_facts_key(char const *list_name, int attempt, char *key, int size)
{
	size_t base;

	if (!list_name || !key || size <= 0)
		return 0;
	if (attempt == 0)
		return snprintf(key, (size_t)size, "%s", list_name) < size;
	if (attempt > 2 || !(same_pack(list_name, "mcc") || same_pack(list_name, "pc")))
		return 0;
	base = (size_t)(pack_of(list_name) - list_name - 1);
	if ((int)base + 4 >= size)
		return 0;
	memcpy(key, list_name, base);
	key[base] = 0;
	if (attempt == 1)
		strcat(key, "@ce");
	return 1;
}

int ae_map_btb_rank(char const *list_name)
{
	if (!list_name || !list_name[0])
		return 4;
	if (!strchr(list_name, '@'))
		return strchr(list_name, ' ') ? 4 : 1;
	if (same_pack(list_name, "ce"))
		return 0;
	if (same_pack(list_name, "mcc"))
		return 2;
	if (same_pack(list_name, "pc"))
		return 3;
	return 4;
}

int ae_map_same_base(char const *a, char const *b)
{
	for (; *a && *a != '@' && *b && *b != '@'; a++, b++)
		if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
			return 0;
	return (!*a || *a == '@') && (!*b || *b == '@');
}
