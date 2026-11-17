/*
AE_TAG_NAME_INDEX.C

(ae_tag_name_index.h) open addressing over 2^17 slots, each a tag's index plus one (0: empty), keyed by the group and
the name's lower-case FNV-1a hash; filled in tag order, a name met again keeps its first (lowest) tag, as the walk did.
*/

#include <stddef.h>
#include "ae_tag_name_index.h"

#define SLOT_BITS 17
#define SLOTS (1L << SLOT_BITS)
#define NONE_INDEX (-1L)

static struct
{
	void const *table;
	long count;
	unsigned long generation;
	int valid;
	unsigned long builds;
	unsigned int slots[SLOTS];
	unsigned int hashes[SLOTS];
} index_state;

static unsigned char lower(unsigned char c)
{
	return (c >= 'A' && c <= 'Z') ? (unsigned char)(c - 'A' + 'a') : c;
}

static int same_name(char const *a, char const *b)
{
	while (*a && lower((unsigned char)*a) == lower((unsigned char)*b))
	{
		a++;
		b++;
	}
	return lower((unsigned char)*a) == lower((unsigned char)*b);
}

static unsigned int hash_of(long group, char const *name)
{
	unsigned int hash = 2166136261u ^ (unsigned int)group;

	hash *= 16777619u;
	while (*name)
	{
		hash ^= lower((unsigned char)*name++);
		hash *= 16777619u;
	}
	return hash ? hash : 1u;
}

static void build(void const *table, long count, unsigned long generation, ae_tag_name_index_get get, void *context)
{
	long tag;

	for (tag = 0; tag < SLOTS; tag++)
		index_state.slots[tag] = 0;
	for (tag = 0; tag < count; tag++)
	{
		long group = 0, other_group = 0;
		char const *name = NULL, *other = NULL;
		unsigned int hash, slot;

		get(context, tag, &group, &name);
		if (!name)
			continue;
		hash = hash_of(group, name);
		for (slot = hash & (SLOTS - 1); index_state.slots[slot]; slot = (slot + 1) & (SLOTS - 1))
		{
			if (index_state.hashes[slot] != hash)
				continue;
			get(context, (long)index_state.slots[slot] - 1, &other_group, &other);
			if (other_group == group && other && same_name(other, name))
				break;
		}
		if (!index_state.slots[slot])
		{
			index_state.slots[slot] = (unsigned int)(tag + 1);
			index_state.hashes[slot] = hash;
		}
	}
	index_state.table = table;
	index_state.count = count;
	index_state.generation = generation;
	index_state.valid = 1;
	index_state.builds++;
}

long ae_tag_name_index_find(long group, char const *name, void const *table, long count, unsigned long generation,
	ae_tag_name_index_get get, void *context)
{
	unsigned int hash, slot;

	if (count < 0 || count > AE_TAG_NAME_INDEX_MAXIMUM_TAGS || !get)
		return AE_TAG_NAME_INDEX_UNUSABLE;
	if (!name)
		return NONE_INDEX;
	if (!index_state.valid || index_state.table != table || index_state.count != count ||
		index_state.generation != generation)
	{
		build(table, count, generation, get, context);
	}
	hash = hash_of(group, name);
	for (slot = hash & (SLOTS - 1); index_state.slots[slot]; slot = (slot + 1) & (SLOTS - 1))
	{
		long tag = (long)index_state.slots[slot] - 1;
		long found_group = 0;
		char const *found = NULL;

		if (index_state.hashes[slot] != hash)
			continue;
		get(context, tag, &found_group, &found);
		if (found_group == group && found && same_name(found, name))
			return tag;
	}
	return NONE_INDEX;
}

unsigned long ae_tag_name_index_builds(void)
{
	return index_state.builds;
}
