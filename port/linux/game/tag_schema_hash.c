/*
TAG_SCHEMA_HASH.C

The hash of AE's compiled tag schema (tag_schema.h) that the weapon cache is stamped with (ae_weapon_cache_format.h,
design section 3.6): 64-bit FNV-1a over a canonical walk of every group (its tag, parents), every definition (name, size)
and every field (type, size, count, flags, offset, maximum, block index targets, name, referenced groups), the groups in
tag order and each definition's fields in the schema's own order, and the tag layout number. A definition met a second
time is hashed by its name alone. The check procedures are hashed by their names. When the schema, a structure's layout or
the tag layout changes, the hash changes, and a cache made for the old one is refused.

The same file is linked into the game and into tools/ae_cache_check, so the linker and the game compute it alike.
*/

#include "cseries.h"
#include "tag_schema.h"

#include "ae_weapon_cache.h"

enum
{
	VISITED_MAXIMUM = 8192,
	GROUPS_MAXIMUM = 512
};

struct hash_state
{
	unsigned long long value;
	struct tag_schema_definition const *visited[VISITED_MAXIMUM];
	unsigned long visited_count;
};

/* the groups a cache never carries (a pack builder refuses them: a scenario, a structure bsp, the globals) are left out of
the walk: their schemas include runtime pointers whose size is the game's ABI's, which would make the 64-bit game and the
32-bit linker tool disagree about a cache that holds none of them */
static int group_is_carried(unsigned long group_tag)
{
	return group_tag != 0x73627370ul /* 'sbsp' */ && group_tag != 0x73637672ul /* 'scnr' */ &&
		group_tag != 0x6D617467ul /* 'matg' */;
}

static void hash_bytes(struct hash_state *state, void const *data, unsigned long size)
{
	unsigned char const *bytes = (unsigned char const *)data;
	unsigned long index;

	for (index = 0; index < size; index++)
	{
		state->value ^= bytes[index];
		state->value *= 0x100000001B3ull;
	}
}

static void hash_number(struct hash_state *state, long number)
{
	unsigned char bytes[4];

	bytes[0] = (unsigned char)(number & 0xFF);
	bytes[1] = (unsigned char)((number >> 8) & 0xFF);
	bytes[2] = (unsigned char)((number >> 16) & 0xFF);
	bytes[3] = (unsigned char)((number >> 24) & 0xFF);
	hash_bytes(state, bytes, 4);
}

static void hash_text(struct hash_state *state, char const *text)
{
	unsigned long length = 0;

	if (text)
		while (text[length])
			length++;
	hash_number(state, (long)length);
	hash_bytes(state, text ? text : "", length);
}

static void hash_definition(struct hash_state *state, struct tag_schema_definition const *definition)
{
	struct tag_schema_field const *field;
	unsigned long index;

	if (!definition)
	{
		hash_number(state, -1);
		return;
	}
	for (index = 0; index < state->visited_count; index++)
	{
		if (state->visited[index] == definition)
		{
			hash_text(state, "R");
			hash_text(state, definition->name);
			return;
		}
	}
	if (state->visited_count < VISITED_MAXIMUM)
		state->visited[state->visited_count++] = definition;
	hash_text(state, "D");
	hash_text(state, definition->name);
	hash_number(state, definition->size);
	for (field = definition->fields; field->type != _tag_schema_terminator; field++)
	{
		hash_number(state, field->type);
		hash_number(state, field->size);
		hash_number(state, field->count);
		hash_number(state, field->flags);
		hash_number(state, field->offset);
		hash_number(state, field->maximum);
		hash_number(state, field->target_level);
		hash_number(state, field->target_offset);
		hash_text(state, field->name);
		switch (field->type)
		{
		case _tag_schema_block:
		case _tag_schema_struct:
			hash_definition(state, (struct tag_schema_definition const *)field->definition);
			break;
		case _tag_schema_reference:
		case _tag_schema_tag_index:
		{
			unsigned long const *groups = (unsigned long const *)field->definition;

			while (groups && *groups)
				hash_number(state, (long)*groups++);
			hash_number(state, 0);
			break;
		}
		default:
			break;
		}
	}
	hash_text(state, "E");
}

void tag_schema_hash(
	unsigned int *low,
	unsigned int *high)
{
	static struct hash_state state;
	static struct tag_schema_group const *groups[GROUPS_MAXIMUM];
	unsigned long count = 0, list, index, other;

	state.value = 0xCBF29CE484222325ull;
	state.visited_count = 0;
	for (list = 0; tag_schema_group_lists[list]; list++)
	{
		struct tag_schema_group const *group;

		for (group = tag_schema_group_lists[list]; group->group_tag && count < GROUPS_MAXIMUM; group++)
			if (group_is_carried(group->group_tag))
				groups[count++] = group;
	}
	/* (in tag order, whatever order the lists are in) */
	for (index = 1; index < count; index++)
	{
		struct tag_schema_group const *held = groups[index];

		for (other = index; other > 0 && groups[other - 1]->group_tag > held->group_tag; other--)
			groups[other] = groups[other - 1];
		groups[other] = held;
	}
	hash_number(&state, AE_CACHE_TAG_LAYOUT_XBOX);
	for (index = 0; index < count; index++)
	{
		hash_number(&state, (long)groups[index]->group_tag);
		hash_number(&state, (long)groups[index]->parent_group_tags[0]);
		hash_number(&state, (long)groups[index]->parent_group_tags[1]);
		hash_definition(&state, groups[index]->definition);
	}
	*low = (unsigned int)(state.value & 0xFFFFFFFFull);
	*high = (unsigned int)(state.value >> 32);
}

/* each group's own hash (a fresh walk of it alone), in tag order: for finding where two builds' schemas differ (the game's
64-bit objects and the 32-bit tool's must agree: ae_cache_check --schema-groups, and the game's HALO_SCHEMA_TRACE) */
void tag_schema_group_hashes(
	void (*report)(unsigned int group_tag, char const *definition_name, unsigned int low, unsigned int high))
{
	static struct hash_state state;
	static struct tag_schema_group const *groups[GROUPS_MAXIMUM];
	unsigned long count = 0, list, index, other;

	for (list = 0; tag_schema_group_lists[list]; list++)
	{
		struct tag_schema_group const *group;

		for (group = tag_schema_group_lists[list]; group->group_tag && count < GROUPS_MAXIMUM; group++)
			if (group_is_carried(group->group_tag))
				groups[count++] = group;
	}
	for (index = 1; index < count; index++)
	{
		struct tag_schema_group const *held = groups[index];

		for (other = index; other > 0 && groups[other - 1]->group_tag > held->group_tag; other--)
			groups[other] = groups[other - 1];
		groups[other] = held;
	}
	/* (a probe of the arithmetic alone, for the first line: "0" group, "probe") */
	state.value = 0xCBF29CE484222325ull;
	hash_text(&state, "abc");
	hash_number(&state, 0x12345678);
	hash_number(&state, -2);
	report(0x50524F42u, "probe", (unsigned int)(state.value & 0xFFFFFFFFull), (unsigned int)(state.value >> 32));
	for (index = 0; index < count; index++)
	{
		state.value = 0xCBF29CE484222325ull;
		state.visited_count = 0;
		hash_definition(&state, groups[index]->definition);
		report((unsigned int)groups[index]->group_tag, groups[index]->definition ? groups[index]->definition->name : "-",
			(unsigned int)(state.value & 0xFFFFFFFFull), (unsigned int)(state.value >> 32));
	}
}
