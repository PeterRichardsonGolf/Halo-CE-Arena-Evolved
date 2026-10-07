/*
TAG_VALIDATE.C

A map's tags checked against their groups' schemas (tag_schema.h) as they
load, before anything else reads them (scenario_tags_load, and each
structure bsp as it loads: cache_files.c), so that the game can trust what
it reads in them as it trusted its own maps.

The game reads a map's tag data straight into the tag cache and uses it as
its structures: every pointer, count, index and enum in it is the map's,
and the game writes runtime values into tags as it runs. So each tag is
walked once through its group's schema:
- every block's and data's bytes must lie in the tags (or the bsp) and
  overlap no other's, which a map of the game's tools never does: a map
  whose tags overlap could have a runtime value the game writes to one tag
  change a pointer in another. Each one's bytes are marked as they are
  checked (a bit a byte, the claims below), so that the walk also never
  visits a byte twice: a map whose blocks point at each other cannot make
  it take longer than its size;
- counts are cut to what the game has room for, indices, enums and tags
  past what they name are corrected, runtime values reset (tag_schema.h);
- then, once every tag has been through it, the checks that look at other
  tags or at graphs (a bsp's nodes, a model's) run.
A map is refused if any pointer is wrong; anything else is corrected and
logged, and the map plays.

The validator is the game's only reader of a map that is not the game's own
data (a map made by other tools, downloaded, or converted from another
format): it reads nothing but the tags it is given, so tools/map_validate.c
runs it alone on a map file.

A Halo Custom Edition map's tags are checked the same way, once its loader
has read them into their own tag cache and made them this build's where only
their bytes differ (port/linux/game/cache_file_formats.c): its tag header has
no vertex or index buffers, its pixels and samples are in several files, and
its models are gbxmodels, whose parts keep their geometry in the map's model
data and which the game takes as models (tag_schema_custom_edition_groups).

port: in the 64-bit builds (HALO_64BIT) the tags' pointers are 32-bit Xbox
addresses (cseries/xbox_address.h): each one is turned into the host's
pointer as it is read (XBOX_POINTER) and back as it is written
(xbox_address), and pointers are compared as integers by POINTER_BITS.
*/

/* ---------- headers */

#include "cseries.h"
#include "cache/physical_memory_map.h"
#include "tag_schema.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* ---------- constants */

enum
{
	TAG_HEADER_SIGNATURE = 'tags',
	STRUCTURE_BSP_HEADER_SIGNATURE = 'sbsp',
	STRUCTURE_BSP_GROUP_TAG = 'sbsp',

	/* a vertex or index buffer in a header (a D3DResource: Common, Data,
	Lock) */
	BUFFER_SIZE = 12,

	/* how deep blocks go in a schema (the deepest are 6 or 7) */
	MAXIMUM_VALIDATION_DEPTH = 32,
	/* how many corrections are logged one by one, for each map or bsp */
	MAXIMUM_LOGGED_CORRECTIONS = 64,
	MAXIMUM_MESSAGE_LENGTH = 512,

	/* the claims: a bit for each byte of the largest tag cache */
	CLAIM_BITS = 32,
	CLAIM_WORDS = TAG_VALIDATE_MAXIMUM_TAG_CACHE_SIZE / CLAIM_BITS,

	/* port (Arena Evolved): the most claims kept by their first byte
	(extent_claim), for the tags and for a structure bsp (powers of two),
	and how full a table is let get */
	MAXIMUM_EXTENTS = 0x40000,
	MAXIMUM_STRUCTURE_BSP_EXTENTS = 0x20000,
	/* port (Arena Evolved): the most bytes that extents of different kinds
	may share in a map (or a bsp), kept to be found unchanged at the end
	(extent_claim) */
	MAXIMUM_SHARED_BYTES = 0x4000,
	MAXIMUM_SHARED_EXTENTS = 256,

	/* a Custom Edition map's models (cache_file_formats.c) */
	GBXMODEL_GROUP_TAG = 'mod2',
	MODEL_GROUP_TAG = 'mode',
	TRANSPARENT_CHICAGO_EXTENDED_GROUP_TAG = 'scex',
	TRANSPARENT_CHICAGO_GROUP_TAG = 'schi',
};

/* port (Arena Evolved): what an extent is (extent_claim) */
enum
{
	_extent_root,
	_extent_block,
	_extent_data,
};

/* the passes of the walk over a tag */
enum
{
	/* blocks' and data's counts and bytes */
	_pass_extents,
	/* indices, enums, tags, strings and runtime values */
	_pass_values,
	/* (into each block's elements, which take the same passes) */
	/* the checks (tag_schema_check_proc), after every tag's other passes */
	_pass_checks,
};

/* ---------- structures */

/* a tag in the header's table (cache_files.c's struct
cache_file_tag_instance) */
struct tag_validate_instance
{
	unsigned long group_tag;
	unsigned long parent_group_tags[2];
	long tag_index;
	XPTR(char) name;
	XPTR(void) base_address;
	unsigned long unused[2];
};

/* the tags' header (cache_files.c's struct cache_file_tag_header) */
struct tag_validate_header
{
	XPTR(struct tag_validate_instance) instances;
	long scenario_tag_index;
	unsigned long checksum;
	long tag_count;
	long vertex_buffer_count;
	XPTR(byte) vertex_buffers;
	long index_buffer_count;
	XPTR(byte) index_buffers;
	unsigned long signature;
};

/* a bsp's header (cache_files.c's struct cache_file_structure_bsp_header):
its second buffers are its lightmaps' vertex buffers
(structure_bsp_header_register_vertex_buffers), which the validator keeps
as a header's index buffers */
struct tag_validate_structure_bsp_header
{
	XPTR(void) base_address;
	long vertex_buffer_count;
	XPTR(byte) vertex_buffers;
	long index_buffer_count;
	XPTR(byte) index_buffers;
	unsigned long signature;
};

/* a Custom Edition map's tags' header: its model data is in the file, and it
has no buffers */
struct tag_validate_custom_edition_header
{
	XPTR(struct tag_validate_instance) instances;
	long scenario_tag_index;
	unsigned long checksum;
	long tag_count;
	long model_part_count;
	long model_vertex_data_offset;
	long model_part_count_again;
	long model_index_data_offset;
	long model_data_size;
	unsigned long signature;
};

typedef char verify_tag_validate_maximum_tag_cache_size[TAG_CACHE_SIZE <= TAG_VALIDATE_MAXIMUM_TAG_CACHE_SIZE ? 1 : -1];
typedef char verify_tag_validate_instance_size[sizeof(struct tag_validate_instance) == 0x20 ? 1 : -1];
typedef char verify_tag_validate_custom_edition_header_size[
	sizeof(struct tag_validate_custom_edition_header) == 0x28 ? 1 : -1];
typedef char verify_tag_validate_header_size[sizeof(struct tag_validate_header) == 0x24 ? 1 : -1];
typedef char verify_tag_validate_structure_bsp_header_size[
	sizeof(struct tag_validate_structure_bsp_header) == 0x18 ? 1 : -1];

/* an element being walked: its bytes, its definition, and where it is (for
messages) */
struct tag_validation_frame
{
	byte *base;
	struct tag_schema_definition const *definition;
	char const *field_name;
	long element_index;
};

struct tag_validation
{
	/* what the pointers being checked may point into */
	byte *region;
	unsigned long region_size;
	/* the header whose vertex and index buffers models (or the bsp) use */
	byte *vertex_buffers;
	long vertex_buffer_count;
	byte *index_buffers;
	long index_buffer_count;

	long tag_index;
	short pass;
	boolean refused;
	long corrections;
	/* port (Arena Evolved): how many of the elements being walked are
	another tag's too (extent_claim): their bytes may not be corrected */
	short shared_depth;

	struct tag_validation_frame frames[MAXIMUM_VALIDATION_DEPTH];
	short depth;
	/* the structure the field being checked is in (TAG_SCHEMA_STRUCTURE) */
	byte *structure;
	char const *field_name;
};

/* ---------- globals */

static struct
{
	/* the tags last checked (tag_validate_tags), in a tag cache of
	tag_cache_size bytes */
	struct tag_validate_header *header;
	long tag_data_size;
	unsigned long tag_cache_size;
	/* where their data in files may be: a map's file, or a Custom Edition
	map's files */
	struct tag_validate_file_range file_ranges[MAXIMUM_TAG_VALIDATE_FILE_RANGES];
	short file_range_count;
	boolean custom_edition;
	char map_name[64];
	long corrections;
} tag_validate_globals;

/* a bit for each byte of the tag cache that a tag's root, block or data
holds */
static unsigned long tag_validate_claims[CLAIM_WORDS];

#ifdef HALO_64BIT
/* (an Xbox address, in the tags' address space: tag_groups.c) */
#define tag_validate_empty_name ((char const *)tag_empty_string())
#else
static char const tag_validate_empty_name[] = "";
#endif

/* port (Arena Evolved): each root, block and data claimed, by its first
byte (extent_claim): what another at the same bytes may be the same one
of. The tags' are kept apart from the structure bsp's, which go as
another bsp loads; extent_table is the one in use */
struct tag_validate_extent
{
	byte const *address;
	/* its group's or block's schema, or its data's field */
	void const *kind;
	/* the instance, block or data that claimed it */
	void const *owner;
	unsigned long size;
	short kind_type;
};
/* port (Arena Evolved): whether the maps checked may share bytes at all
(tag_validate_allow_shared_bytes): only a mod's */
static boolean tag_validate_shared_bytes_allowed = FALSE;
static struct tag_validate_extent tag_validate_extents[MAXIMUM_EXTENTS];
static struct tag_validate_extent tag_validate_structure_bsp_extents[MAXIMUM_STRUCTURE_BSP_EXTENTS];
static struct
{
	struct tag_validate_extent *extents;
	unsigned long mask;
	long count;
	long maximum_count;
} extent_table;

/* port (Arena Evolved): the bytes that extents of different kinds share,
as they were when the second was claimed (extent_claim): they must be the
same when the map (or bsp) has been checked */
static struct
{
	long count;
	long byte_count;
	struct
	{
		byte const *address;
		long size;
		long first_byte;
	} extents[MAXIMUM_SHARED_EXTENTS];
	byte bytes[MAXIMUM_SHARED_BYTES];
} tag_validate_shared;

/* ---------- prototypes */

static struct tag_validate_instance *header_instances(struct tag_validate_header const *header);
static boolean extent_claim(void const *address, unsigned long size, void const *kind, short kind_type,
	void const *owner, boolean *shared);
static boolean extent_shared(void const *address, void const *owner);
static void validate_element(struct tag_validation *validation, byte *base,
	struct tag_schema_definition const *definition, char const *field_name, long element_index);
static void validate_fields(struct tag_validation *validation, byte *base,
	struct tag_schema_definition const *definition);

/* ---------- private code */

/* the header's tag table */
static struct tag_validate_instance *header_instances(
	struct tag_validate_header const *header)
{
	return XBOX_POINTER(struct tag_validate_instance, header->instances);
}

static struct tag_schema_group const *schema_group_get(
	unsigned long group_tag)
{
	struct tag_schema_group const *const *list;

	/* (a Custom Edition map's groups laid out otherwise come first) */
	if (tag_validate_globals.custom_edition)
	{
		struct tag_schema_group const *group;

		for (group = tag_schema_custom_edition_groups; group->group_tag; group++)
		{
			if (group->group_tag == group_tag)
				return group;
		}
	}

	for (list = tag_schema_group_lists; *list; list++)
	{
		struct tag_schema_group const *group;

		for (group = *list; group->group_tag; group++)
		{
			if (group->group_tag == group_tag)
				return group;
		}
	}

	return NULL;
}

static char *tag_to_text(
	unsigned long group_tag,
	char text[5])
{
	short index;

	for (index = 0; index < 4; index++)
	{
		char c = (char)(group_tag >> (8 * (3 - index)));

		text[index] = c >= ' ' && c < 0x7F ? c : '?';
	}
	text[4] = 0;

	return text;
}

/* the name of tag_index, if it has one in the tags */
static char const *tag_name(
	long tag_index)
{
	struct tag_validate_header *header = tag_validate_globals.header;
	short absolute_index = (short)tag_index;
	char const *name;
	unsigned long offset;

	if (!header || absolute_index < 0 || absolute_index >= header->tag_count)
		return "?";
	name = XBOX_POINTER(char const, header_instances(header)[absolute_index].name);
	if (name == tag_validate_empty_name)
		return name;
	offset = (unsigned long)(POINTER_BITS(name) - POINTER_BITS(header));
	if (POINTER_BITS(name) < POINTER_BITS(header) || offset >= (unsigned long)tag_validate_globals.tag_data_size ||
		!memchr(name, 0, tag_validate_globals.tag_data_size - offset))
	{
		return "?";
	}

	return name;
}

/* where the field being checked is: tag 'name' (grou): block[3].field */
static void validation_where(
	struct tag_validation *validation,
	char *where,
	size_t size)
{
	struct tag_validate_header *header = tag_validate_globals.header;
	short absolute_index = (short)validation->tag_index;
	char group[5] = "?";
	size_t length;
	short depth;

	if (header && absolute_index >= 0 && absolute_index < header->tag_count)
		tag_to_text(header_instances(header)[absolute_index].group_tag, group);
	snprintf(where, size, "tag '%s' (%s): ", tag_name(validation->tag_index), group);
	for (depth = 1; depth < validation->depth; depth++)
	{
		length = strlen(where);
		snprintf(where + length, size - length, "%s[%ld].",
			validation->frames[depth].field_name, validation->frames[depth].element_index);
	}
	length = strlen(where);
	snprintf(where + length, size - length, "%s", validation->field_name ? validation->field_name : "");
}

static void validation_message(
	struct tag_validation *validation,
	char const *what,
	char const *format,
	va_list arguments)
{
	char message[MAXIMUM_MESSAGE_LENGTH];
	size_t length;

	snprintf(message, sizeof(message), "the map '%s' %s: ", tag_validate_globals.map_name, what);
	length = strlen(message);
	validation_where(validation, message + length, sizeof(message) - length);
	length = strlen(message);
	snprintf(message + length, sizeof(message) - length, " ");
	length = strlen(message);
	vsnprintf(message + length, sizeof(message) - length, format, arguments);
	tag_validate_report(message);
}

/* whether size bytes at address lie in the region being checked */
static boolean region_contains(
	struct tag_validation *validation,
	void const *address,
	unsigned long size)
{
	unsigned long offset = (unsigned long)(POINTER_BITS(address) - POINTER_BITS(validation->region));

	return POINTER_BITS(address) >= POINTER_BITS(validation->region) &&
		offset <= validation->region_size &&
		size <= validation->region_size - offset;
}

/* marks size bytes at address (in the tag cache, as the region is) as a
tag's: FALSE if any of them already were */
static boolean claim(
	void const *address,
	unsigned long size)
{
	byte const *tag_cache = (byte const *)tag_validate_globals.header;
	unsigned long first = (unsigned long)((byte const *)address - tag_cache);
	unsigned long end = first + size;
	unsigned long bit;

	if (!size)
		return TRUE;
	if (first > tag_validate_globals.tag_cache_size || size > tag_validate_globals.tag_cache_size - first)
		return FALSE;

	/* (whole words where they can be) */
	bit = first;
	while (bit < end)
	{
		unsigned long *word = &tag_validate_claims[bit / CLAIM_BITS];

		if (bit % CLAIM_BITS == 0 && end - bit >= CLAIM_BITS)
		{
			if (*word)
				return FALSE;
			*word = 0xFFFFFFFFUL;
			bit += CLAIM_BITS;
		}
		else
		{
			unsigned long mask = 1UL << (bit % CLAIM_BITS);

			if (*word & mask)
				return FALSE;
			*word |= mask;
			bit++;
		}
	}

	return TRUE;
}

/* unclaims the size bytes at the tag cache's offset first (a bsp's, as
another loads in its place) */
static void unclaim(
	unsigned long first,
	unsigned long size)
{
	unsigned long end = first + size;
	unsigned long bit;

	if (first > tag_validate_globals.tag_cache_size || size > tag_validate_globals.tag_cache_size - first)
		return;
	for (bit = first; bit < end; )
	{
		if (bit % CLAIM_BITS == 0 && end - bit >= CLAIM_BITS)
		{
			tag_validate_claims[bit / CLAIM_BITS] = 0;
			bit += CLAIM_BITS;
		}
		else
		{
			tag_validate_claims[bit / CLAIM_BITS] &= ~(1UL << (bit % CLAIM_BITS));
			bit++;
		}
	}
}

/* whether a string ends in the region, or in the tags (where a bsp's tag
references' names are) */
static boolean string_valid(
	struct tag_validation *validation,
	char const *string)
{
	byte const *tags = (byte const *)tag_validate_globals.header;
	unsigned long offset = (unsigned long)(POINTER_BITS(string) - POINTER_BITS(validation->region));
	unsigned long tags_offset = (unsigned long)(POINTER_BITS(string) - POINTER_BITS(tags));

	/* (the name a nameless tag or reference is given) */
	if (string == tag_validate_empty_name)
		return TRUE;
	if (region_contains(validation, string, 1))
		return memchr(string, 0, validation->region_size - offset) != NULL;
	if (tags && POINTER_BITS(string) >= POINTER_BITS(tags) &&
		tags_offset < (unsigned long)tag_validate_globals.tag_data_size)
	{
		return memchr(string, 0, tag_validate_globals.tag_data_size - tags_offset) != NULL;
	}

	return FALSE;
}

/* the tag tag_index names, if it is one of the map's: NULL otherwise */
static struct tag_validate_instance *instance_get(
	long tag_index)
{
	struct tag_validate_header *header = tag_validate_globals.header;
	short absolute_index = (short)tag_index;
	struct tag_validate_instance *instance;

	if (!header || tag_index == NONE || absolute_index < 0 || absolute_index >= header->tag_count)
		return NULL;
	instance = &header_instances(header)[absolute_index];

	return instance->tag_index == tag_index ? instance : NULL;
}

/* whether a tag is of one of groups (a list ending with 0; NULL for any) */
static boolean instance_in_groups(
	struct tag_validate_instance const *instance,
	unsigned long const *groups)
{
	if (!groups)
		return TRUE;
	for (; *groups; groups++)
	{
		/* (a Custom Edition map's gbxmodels are the game's models) */
		if (tag_validate_globals.custom_edition && instance->group_tag == GBXMODEL_GROUP_TAG && *groups == MODEL_GROUP_TAG)
			return TRUE;
		if (instance->group_tag == *groups ||
			instance->parent_group_tags[0] == *groups ||
			instance->parent_group_tags[1] == *groups)
		{
			return TRUE;
		}
	}

	return FALSE;
}

static char const *groups_text(
	unsigned long const *groups,
	char *text,
	size_t size)
{
	char group[5];

	text[0] = 0;
	if (!groups)
		return "any";
	for (; *groups && strlen(text) + 6 < size; groups++)
	{
		if (text[0])
			strcat(text, ",");
		strcat(text, tag_to_text(*groups, group));
	}

	return text;
}

static long integer_get(
	byte const *address,
	short size,
	boolean is_unsigned)
{
	switch (size)
	{
	case 1:
		return is_unsigned ? (long)*address : (long)*(signed char const *)address;
	case 2:
		return is_unsigned ? (long)*(unsigned short const *)address : (long)*(short const *)address;
	default:
		return *(long const *)address;
	}
}

static void integer_set(
	byte *address,
	short size,
	long value)
{
	switch (size)
	{
	case 1:
		*address = (byte)value;
		break;
	case 2:
		*(short *)address = (short)value;
		break;
	default:
		*(long *)address = value;
		break;
	}
}

/* NONE in an integer of size bytes, as it reads (an unsigned byte's NONE is
0xFF) */
static long integer_none(
	short size,
	boolean is_unsigned)
{
	if (!is_unsigned)
		return NONE;

	return size == 1 ? 0xFF : size == 2 ? 0xFFFF : NONE;
}

/* the block a block index indexes */
static struct tag_block const *block_index_target(
	struct tag_validation *validation,
	struct tag_schema_field const *field)
{
	byte *base;

	if (field->target_level == TAG_SCHEMA_ROOT)
		base = validation->frames[0].base;
	else if (field->target_level == TAG_SCHEMA_STRUCTURE)
		base = validation->structure;
	else if (field->target_level >= 0 && field->target_level < validation->depth)
		base = validation->frames[validation->depth - 1 - field->target_level].base;
	else
		return NULL;

	return (struct tag_block const *)(base + field->target_offset);
}

static void validate_block_extent(
	struct tag_validation *validation,
	struct tag_block *block,
	struct tag_schema_field const *field)
{
	struct tag_schema_definition const *definition = field->definition;
	boolean shared;

	block->definition = XBOX_NULL;
	if (block->count < 0)
	{
		tag_validate_refuse(validation, "has %ld elements", block->count);
		return;
	}
	if (field->maximum > 0 && block->count > field->maximum)
	{
		tag_validate_correct(validation, "has %ld elements, more than the game's %ld: cut to %ld",
			block->count, field->maximum, field->maximum);
		block->count = field->maximum;
	}
	if (block->count &&
		((unsigned long)block->count > validation->region_size / (unsigned long)definition->size ||
			!region_contains(validation, XBOX_POINTER(void, block->address),
				(unsigned long)block->count * definition->size)))
	{
		tag_validate_refuse(validation, "has %ld elements of %ld bytes at %08lx, outside the tags",
			block->count, definition->size, (unsigned long)block->address);
		return;
	}
	if (!extent_claim(XBOX_POINTER(void, block->address), (unsigned long)block->count * definition->size,
		definition, _extent_block, block, &shared))
	{
		tag_validate_refuse(validation, "has %ld elements of %ld bytes at %08lx, which overlap another's",
			block->count, definition->size, (unsigned long)block->address);
		return;
	}
	if (!block->count)
		block->address = XBOX_NULL;

	return;
}

static void validate_data_extent(
	struct tag_validation *validation,
	struct tag_data *data,
	struct tag_schema_field const *field)
{
	boolean shared;

	data->definition = XBOX_NULL;
	if (data->size < 0)
	{
		tag_validate_refuse(validation, "has %ld bytes", data->size);
		return;
	}
	if (field->maximum > 0 && data->size > field->maximum)
	{
		tag_validate_correct(validation, "has %ld bytes, more than the game's %ld: cut to %ld",
			data->size, field->maximum, field->maximum);
		data->size = field->maximum;
	}
	if (field->type == _tag_schema_file_data)
	{
		if (!tag_validate_file_contains(validation, data->file_offset, data->size))
		{
			tag_validate_refuse(validation, "has %ld bytes at %08lx, outside the map's files",
				data->size, (unsigned long)data->file_offset);
		}
		return;
	}
	if (data->size &&
		(!region_contains(validation, XBOX_POINTER(void, data->address), data->size) ||
			!extent_claim(XBOX_POINTER(void, data->address), data->size, field, _extent_data, data, &shared)))
	{
		tag_validate_refuse(validation, "has %ld bytes at %08lx, outside the tags or overlapping another's",
			data->size, (unsigned long)data->address);
		return;
	}
	if (!data->size)
		data->address = XBOX_NULL;

	return;
}

static void validate_tag_index(
	struct tag_validation *validation,
	long *tag_index,
	unsigned long *group_tag,
	unsigned long const *groups)
{
	struct tag_validate_instance *instance;
	char text[96];

	if (*tag_index == NONE)
		return;
	instance = instance_get(*tag_index);
	if (!instance || !instance_in_groups(instance, groups))
	{
		tag_validate_correct(validation, "names %08lx, not a tag of %s: none",
			*tag_index, groups_text(groups, text, sizeof(text)));
		*tag_index = NONE;
		return;
	}
	/* (a Custom Edition map's transparent chicago extended shaders are its
	loader's transparent chicago shaders now, cache_file_formats.c: the
	references that say so are not wrong) */
	if (group_tag && tag_validate_globals.custom_edition &&
		*group_tag == TRANSPARENT_CHICAGO_EXTENDED_GROUP_TAG && instance->group_tag == TRANSPARENT_CHICAGO_GROUP_TAG)
	{
		*group_tag = instance->group_tag;
	}
	/* (what the game takes the reference's tag to be: the tag's group) */
	if (group_tag && *group_tag != instance->group_tag)
	{
		char said[5];
		char group[5];

		tag_validate_correct(validation, "says '%s' of a '%s' tag", tag_to_text(*group_tag, said),
			tag_to_text(instance->group_tag, group));
		*group_tag = instance->group_tag;
	}

	return;
}

static void validate_value(
	struct tag_validation *validation,
	byte *address,
	struct tag_schema_field const *field)
{
	boolean is_unsigned = TEST_FLAG(field->flags, _tag_schema_unsigned_bit);
	boolean none_allowed = TEST_FLAG(field->flags, _tag_schema_none_bit);

	switch (field->type)
	{
	case _tag_schema_reference:
	{
		struct tag_reference *reference = (struct tag_reference *)address;

		/* (a reference to no tag often has a name pointer that is not one,
		in the retail maps too: only one to a tag counts as a correction) */
		if (!string_valid(validation, XBOX_POINTER(char const, reference->name)))
		{
			if (reference->index != NONE)
				tag_validate_correct(validation, "has a name outside the tags: none");
			reference->name = xbox_address(tag_validate_empty_name);
		}
		validate_tag_index(validation, &reference->index, &reference->group_tag, field->definition);
		break;
	}
	case _tag_schema_tag_index:
		validate_tag_index(validation, (long *)address, NULL, field->definition);
		break;
	case _tag_schema_block_index:
	{
		struct tag_block const *target = block_index_target(validation, field);
		long value = integer_get(address, field->size, is_unsigned);
		long none = integer_none(field->size, is_unsigned);
		long count = target ? target->count : 0;

		/* (an empty block's index is none, as below, whether or not it
		may be otherwise) */
		if ((value >= 0 && value < count && value != none) || ((none_allowed || !count) && value == none))
			break;
		tag_validate_correct(validation, "is %ld, past its block's %ld: %s",
			value, count, none_allowed || !count ? "none" : "0");
		integer_set(address, field->size, none_allowed || !count ? NONE : 0);
		break;
	}
	case _tag_schema_enum:
	{
		long value = integer_get(address, field->size, is_unsigned);
		long none = integer_none(field->size, is_unsigned);

		if ((value >= 0 && value < field->maximum && value != none) || (none_allowed && value == none))
			break;
		tag_validate_correct(validation, "is %ld, past its %ld values: %s",
			value, field->maximum, none_allowed ? "none" : "0");
		integer_set(address, field->size, none_allowed ? NONE : 0);
		break;
	}
	case _tag_schema_string:
		if (!memchr(address, 0, field->size))
		{
			tag_validate_correct(validation, "is not terminated");
			address[field->size - 1] = 0;
		}
		break;
	case _tag_schema_reset:
		if (field->size <= 4)
			integer_set(address, field->size, field->target_offset);
		else
			memset(address, 0, field->size);
		break;
	}

	return;
}

/* one pass over an element's fields (those of the structures in it too) */
static void validate_fields(
	struct tag_validation *validation,
	byte *base,
	struct tag_schema_definition const *definition)
{
	struct tag_schema_field const *field;
	byte *structure = validation->structure;

	validation->structure = base;
	for (field = definition->fields; field->type != _tag_schema_terminator && !validation->refused; field++)
	{
		short index;

		validation->field_name = field->name;
		for (index = 0; index < field->count && !validation->refused; index++)
		{
			byte *address = base + field->offset + index * field->size;

			switch (field->type)
			{
			case _tag_schema_struct:
				validate_fields(validation, address, field->definition);
				validation->field_name = field->name;
				break;

			case _tag_schema_block:
				if (validation->pass == _pass_extents)
				{
					validate_block_extent(validation, (struct tag_block *)address, field);
				}
				else
				{
					struct tag_block *block = (struct tag_block *)address;
					boolean shared = extent_shared(XBOX_POINTER(void, block->address), block);
					long element_index;

					/* (the elements, through every pass; another tag's too
					not corrected: extent_claim) */
					validation->shared_depth += shared;
					for (element_index = 0;
						element_index < block->count && !validation->refused;
						element_index++)
					{
						validate_element(
							validation,
							XBOX_POINTER(byte, block->address) +
								element_index * ((struct tag_schema_definition const *)field->definition)->size,
							field->definition,
							field->name,
							element_index);
					}
					validation->shared_depth -= shared;
					validation->field_name = field->name;
				}
				break;

			case _tag_schema_data:
			case _tag_schema_file_data:
				if (validation->pass == _pass_extents)
					validate_data_extent(validation, (struct tag_data *)address, field);
				break;

			case _tag_schema_check:
				if (validation->pass == _pass_checks && !field->check(validation, base))
				{
					if (!validation->refused)
						tag_validate_refuse(validation, "failed its check");
				}
				break;

			default:
				if (validation->pass == _pass_values)
					validate_value(validation, address, field);
				break;
			}
		}
	}
	validation->structure = structure;

	return;
}

/* an element, and the elements of its blocks: in the first walk, its
extents then its values then its blocks' elements (so that an index's
block is checked before the index); in the checks' walk, its checks then
its blocks' elements */
static void validate_element(
	struct tag_validation *validation,
	byte *base,
	struct tag_schema_definition const *definition,
	char const *field_name,
	long element_index)
{
	struct tag_validation_frame *frame;
	short pass = validation->pass;

	if (validation->depth >= MAXIMUM_VALIDATION_DEPTH)
	{
		tag_validate_refuse(validation, "is too deep");
		return;
	}
	frame = &validation->frames[validation->depth++];
	frame->base = base;
	frame->definition = definition;
	frame->field_name = field_name;
	frame->element_index = element_index;

	if (pass == _pass_checks)
	{
		/* (its checks, then its blocks': validate_fields does both) */
		validate_fields(validation, base, definition);
	}
	else
	{
		validation->pass = _pass_extents;
		validate_fields(validation, base, definition);
		if (!validation->refused)
		{
			validation->pass = _pass_values;
			validate_fields(validation, base, definition);
		}
		validation->pass = pass;
	}
	validation->depth--;

	return;
}

/* the blocks' elements are walked in the values pass (validate_fields); the
first walk starts each tag there */
static void validate_tag(
	struct tag_validation *validation,
	long tag_index,
	void *root,
	struct tag_schema_definition const *definition,
	short pass)
{
	validation->tag_index = tag_index;
	validation->depth = 0;
	validation->structure = root;
	validation->field_name = NULL;
	validation->pass = pass;
	validate_element(validation, root, definition, NULL, NONE);

	return;
}

/* whether a definition's fields (and those of the structures in it) all
lie within it, as the walk assumes: a field past its definition's size
would be read and written in bytes that another tag may hold. A mistake
in a schema, reported (once) and the map refused, never trusted */
static boolean definition_fits(
	struct tag_schema_definition const *definition,
	short depth)
{
	struct tag_schema_field const *field;

	if (depth > MAXIMUM_VALIDATION_DEPTH)
		return FALSE;
	for (field = definition->fields; field->type != _tag_schema_terminator; field++)
	{
		/* (a structure's bytes are its definition's: the last of an array
		ends there) */
		long end = field->type == _tag_schema_struct && field->count >= 1 ?
			field->offset + (long)field->size * (field->count - 1) +
				((struct tag_schema_definition const *)field->definition)->size :
			field->offset + (long)field->size * field->count;

		if (field->type != _tag_schema_check &&
			(field->offset < 0 || field->count < 1 || end > definition->size))
		{
			char message[MAXIMUM_MESSAGE_LENGTH];

			snprintf(message, sizeof(message), "the tag schema '%s' has its field '%s' at %ld..%ld, past its %ld bytes",
				definition->name, field->name, field->offset, end, definition->size);
			tag_validate_report(message);
			return FALSE;
		}
		if ((field->type == _tag_schema_struct || field->type == _tag_schema_block) &&
			!definition_fits(field->definition, depth + 1))
		{
			return FALSE;
		}
		if (field->type == _tag_schema_struct &&
			((struct tag_schema_definition const *)field->definition)->size > field->size)
		{
			char message[MAXIMUM_MESSAGE_LENGTH];

			snprintf(message, sizeof(message), "the tag schema '%s' has its structure '%s' of %ld bytes in %d",
				definition->name, field->name, ((struct tag_schema_definition const *)field->definition)->size,
				field->size);
			tag_validate_report(message);
			return FALSE;
		}
	}

	return TRUE;
}

/* whether every group's schema fits (definition_fits), found once */
static boolean schemas_fit(
	void)
{
	static short fit = NONE;

	if (fit == NONE)
	{
		struct tag_schema_group const *const *list;

		fit = TRUE;
		for (list = tag_schema_group_lists; *list && fit; list++)
		{
			struct tag_schema_group const *group;

			for (group = *list; group->group_tag && fit; group++)
			{
				if (group->definition && !definition_fits(group->definition, 0))
					fit = FALSE;
			}
		}
	}

	return (boolean)fit;
}

static unsigned long extent_slot(
	void const *address)
{
	return (unsigned long)((POINTER_BITS(address) >> 2) * 2654435761UL) & extent_table.mask;
}

/* the table's extents of a map, or of a structure bsp, emptied */
static void extent_table_new(
	struct tag_validate_extent *extents,
	long size)
{
	memset(extents, 0, (size_t)size * sizeof(*extents));
	extent_table.extents = extents;
	extent_table.mask = (unsigned long)size - 1;
	extent_table.count = 0;
	extent_table.maximum_count = size / 4 * 3;
	tag_validate_shared.count = 0;
	tag_validate_shared.byte_count = 0;

	return;
}

/* the next extent at address after *slot's (start with extent_slot's): NULL
when there are no more */
static struct tag_validate_extent *extent_next(
	void const *address,
	unsigned long *slot)
{
	while (extent_table.extents && extent_table.extents[*slot].address)
	{
		struct tag_validate_extent *extent = &extent_table.extents[*slot];

		*slot = (*slot + 1) & extent_table.mask;
		if (extent->address == address)
			return extent;
	}

	return NULL;
}

/* (kept; FALSE if the table is full, which the claim is not shared with any
other then) */
static boolean extent_add(
	void const *address,
	unsigned long size,
	void const *kind,
	short kind_type,
	void const *owner)
{
	unsigned long slot = extent_slot(address);
	struct tag_validate_extent *extent;

	if (!size || !extent_table.extents || extent_table.count >= extent_table.maximum_count)
		return FALSE;
	while (extent_table.extents[slot].address)
		slot = (slot + 1) & extent_table.mask;
	extent = &extent_table.extents[slot];
	extent->address = address;
	extent->size = size;
	extent->kind = kind;
	extent->kind_type = kind_type;
	extent->owner = owner;
	extent_table.count++;

	return TRUE;
}

/* whether a definition has a runtime value (a field the game writes as it
runs: _tag_schema_reset) in its bytes from..to, in its structures too */
static boolean definition_resets_in(
	struct tag_schema_definition const *definition,
	long from,
	long to,
	short depth)
{
	struct tag_schema_field const *field;

	if (depth > MAXIMUM_VALIDATION_DEPTH)
		return TRUE;
	for (field = definition->fields; field->type != _tag_schema_terminator; field++)
	{
		short index;

		for (index = 0; index < field->count; index++)
		{
			long start = field->offset + (long)index * field->size;
			long end = start + (field->type == _tag_schema_struct ?
				((struct tag_schema_definition const *)field->definition)->size : field->size);

			if (end <= from || start >= to)
				continue;
			if (field->type == _tag_schema_reset ||
				(field->type == _tag_schema_struct &&
					definition_resets_in(field->definition, from - start, to - start, depth + 1)))
			{
				return TRUE;
			}
		}
	}

	return FALSE;
}

/* whether an extent of a kind may have bytes the game writes as it runs in
its first size bytes: a root's or block's runtime values, or the scripts'
data (hs_syntax_data's nodes, hs_string_constants', which the console's
strings are added to).

(Arena Evolved) The shared-bytes rule (extent_claim) is as safe as two
premises of the schemas, which upstream's own checks rest on too:
- every field of a root or block that the game writes after the map has
  loaded is a _tag_schema_reset field (tag_schema.h);
- no tag data is written as the game runs but the scripts' (hs_*).
A runtime write that the schemas do not mark would land in the other
owner's bytes after they were checked. tools/test_linux_port.py's
test_shared_bytes_* hold the rule to these premises on crafted maps */
static boolean kind_written_in(
	void const *kind,
	short kind_type,
	unsigned long size)
{
	struct tag_schema_definition const *definition;
	long element;

	switch (kind_type)
	{
	case _extent_root:
		definition = ((struct tag_schema_group const *)kind)->definition;
		break;
	case _extent_block:
		definition = kind;
		break;
	default:
	{
		char const *name = ((struct tag_schema_field const *)kind)->name;

		return name[0] == 'h' && name[1] == 's' && name[2] == '_';
	}
	}
	for (element = 0; element * definition->size < (long)size; element++)
	{
		if (definition_resets_in(definition, 0, (long)size - element * definition->size, 0))
			return TRUE;
	}

	return FALSE;
}

/* port (Arena Evolved): the claim of an extent at address (claim), kept by
its first byte: TRUE if it is claimed, or if its bytes are, from the same
first byte, other extents' that it may share them with (*shared is then
set; any bytes past theirs must be no other's).

Some maps hold bytes that are the same once, for every structure that has
them: CE+ X's ui.map (built by a tool that shares bytes that are the same)
names a shader, string lists and text widgets under two or more names,
shares bitmaps' sequences, strings and event handlers between tags, and a
few bytes of one kind with another's (a color table's empty root is the
start of a model's geometry; a string's bytes are the start of another
string list's strings).

- The same kind (the same group's root, the same block's elements, the same
  field's data) is the same structure at the same offsets: whatever is
  written through one owner lands in the same field of the other.
- Another kind may share bytes that hold no runtime value of either (no
  field the game writes as it runs: tag_schema.h), and that the check leaves
  as they were (shared_bytes_unchanged): nothing written through one lands
  in the other.

Each owner's walk checks the shared bytes in its own place, but may not
correct them (tag_validate_correct): what is right for one owner must be
right for each. Bytes shared from another first byte are still refused */
static boolean extent_claim(
	void const *address,
	unsigned long size,
	void const *kind,
	short kind_type,
	void const *owner,
	boolean *shared)
{
	struct tag_validate_extent *extent;
	unsigned long slot = extent_slot(address);
	unsigned long covered = 0;
	unsigned long shared_size = 0;
	boolean found = FALSE;

	*shared = FALSE;
	if (claim(address, size))
	{
		extent_add(address, size, kind, kind_type, owner);
		return TRUE;
	}
	/* (only a mod's maps may share bytes: tag_validate_allow_shared_bytes;
	any other map is refused for an overlap, as upstream's check does) */
	if (!tag_validate_shared_bytes_allowed)
		return FALSE;
	while ((extent = extent_next(address, &slot)) != NULL)
	{
		unsigned long overlap = MIN(size, extent->size);

		found = TRUE;
		covered = MAX(covered, extent->size);
		if (extent->kind == kind)
			continue;
		if (kind_written_in(kind, kind_type, overlap) ||
			kind_written_in(extent->kind, extent->kind_type, overlap))
		{
			char message[MAXIMUM_MESSAGE_LENGTH];

			snprintf(message, sizeof(message), "the map '%s' has bytes at %08lx that structures of different kinds "
				"share, with a value the game writes as it runs", tag_validate_globals.map_name,
				(unsigned long)xbox_address(address));
			tag_validate_report(message);
			return FALSE;
		}
		shared_size = MAX(shared_size, overlap);
	}
	if (!found ||
		(shared_size &&
			(tag_validate_shared.count >= MAXIMUM_SHARED_EXTENTS ||
				(long)shared_size > MAXIMUM_SHARED_BYTES - tag_validate_shared.byte_count)) ||
		(size > covered && !claim((byte const *)address + covered, size - covered)))
	{
		return FALSE;
	}
	if (shared_size)
	{
		tag_validate_shared.extents[tag_validate_shared.count].address = address;
		tag_validate_shared.extents[tag_validate_shared.count].size = (long)shared_size;
		tag_validate_shared.extents[tag_validate_shared.count].first_byte = tag_validate_shared.byte_count;
		memcpy(tag_validate_shared.bytes + tag_validate_shared.byte_count, address, shared_size);
		tag_validate_shared.count++;
		tag_validate_shared.byte_count += (long)shared_size;
	}
	/* (so that one after it shares with it too) */
	if (size > covered)
		extent_add(address, size, kind, kind_type, owner);
	*shared = TRUE;

	return TRUE;
}

/* whether the bytes extents of different kinds share are as they were
(extent_claim); the record is emptied for the next map or bsp */
static boolean shared_bytes_unchanged(
	void)
{
	long index;
	boolean unchanged = TRUE;

	for (index = 0; index < tag_validate_shared.count; index++)
	{
		byte const *now = tag_validate_shared.extents[index].address;
		byte const *then = tag_validate_shared.bytes + tag_validate_shared.extents[index].first_byte;
		long offset;

		for (offset = 0; offset < tag_validate_shared.extents[index].size; offset++)
		{
			if (now[offset] != then[offset])
				unchanged = FALSE;
		}
	}
	tag_validate_shared.count = 0;
	tag_validate_shared.byte_count = 0;

	return unchanged;
}

/* whether the extent at address that owner names is another owner's too
(extent_claim): the first extent kept at that address, which the walk
reaches first, is its own */
static boolean extent_shared(
	void const *address,
	void const *owner)
{
	unsigned long slot;
	struct tag_validate_extent const *extent;

	if (!address)
		return FALSE;
	slot = extent_slot(address);
	extent = extent_next(address, &slot);

	return extent && extent->owner != owner;
}

/* a tag of the table through a pass of its group's schema, if it has one */
static void validate_instance(
	struct tag_validation *validation,
	struct tag_validate_instance *instance,
	short pass)
{
	struct tag_schema_group const *group = schema_group_get(instance->group_tag);

	if (group && group->definition && instance->base_address)
	{
		/* (another tag's root under its name: extent_claim) */
		boolean shared = extent_shared(XBOX_POINTER(void, instance->base_address), instance);

		validation->shared_depth += shared;
		validate_tag(validation, instance->tag_index, XBOX_POINTER(void, instance->base_address), group->definition,
			pass);
		validation->shared_depth -= shared;
	}

	return;
}

static void validation_new(
	struct tag_validation *validation,
	void *region,
	unsigned long region_size)
{
	memset(validation, 0, sizeof(*validation));
	validation->region = region;
	validation->region_size = region_size;
	validation->tag_index = NONE;

	return;
}

/* a header's vertex and index buffers: in the region and claimed, their
data in the region */
static boolean validate_buffers(
	struct tag_validation *validation,
	byte *vertex_buffers,
	long vertex_buffer_count,
	byte *index_buffers,
	long index_buffer_count)
{
	long index;

	validation->vertex_buffers = vertex_buffers;
	validation->vertex_buffer_count = vertex_buffer_count;
	validation->index_buffers = index_buffers;
	validation->index_buffer_count = index_buffer_count;
	if (vertex_buffer_count < 0 || index_buffer_count < 0 ||
		(unsigned long)vertex_buffer_count > validation->region_size / BUFFER_SIZE ||
		(unsigned long)index_buffer_count > validation->region_size / BUFFER_SIZE ||
		(vertex_buffer_count && !region_contains(validation, vertex_buffers, vertex_buffer_count * BUFFER_SIZE)) ||
		(index_buffer_count && !region_contains(validation, index_buffers, index_buffer_count * BUFFER_SIZE)) ||
		!claim(vertex_buffers, vertex_buffer_count * BUFFER_SIZE) ||
		!claim(index_buffers, index_buffer_count * BUFFER_SIZE))
	{
		tag_validate_refuse(validation, "has its vertex or index buffers outside the tags");
		return FALSE;
	}
	for (index = 0; index < vertex_buffer_count + index_buffer_count; index++)
	{
		byte *buffer = index < vertex_buffer_count ?
			vertex_buffers + index * BUFFER_SIZE :
			index_buffers + (index - vertex_buffer_count) * BUFFER_SIZE;
		/* (the buffer's Data, its bytes' address before it is registered) */
		void *data = XBOX_POINTER(void, *(XPTR(void) const *)(buffer + 4));

		if (!region_contains(validation, data, 1))
		{
			tag_validate_refuse(validation, "has a vertex or index buffer's data at %08lx, outside the tags",
				(unsigned long)*(XPTR(void) const *)(buffer + 4));
			return FALSE;
		}
	}

	return TRUE;
}

/* ---------- public code */

/* port (Arena Evolved): (tag_schema.h) */
void tag_validate_allow_shared_bytes(
	boolean allow)
{
	tag_validate_shared_bytes_allowed = allow ? TRUE : FALSE;
}

/* the tags at tag_header (tag_data_size bytes of a tag cache), whose header
the caller has checked and claimed: each tag's table entry, then every tag
through its schema, then every tag's checks */
static boolean validate_tag_table(
	struct tag_validation *validation,
	struct tag_validate_instance *instances,
	long tag_count)
{
	long absolute_index;

	/* the tag table: each tag's index, name, groups and root */
	for (absolute_index = 0; absolute_index < tag_count && !validation->refused; absolute_index++)
	{
		struct tag_validate_instance *instance = &instances[absolute_index];
		struct tag_schema_group const *group = schema_group_get(instance->group_tag);
		unsigned long parent_group_tags[2];

		validation->tag_index = instance->tag_index;
		if ((short)instance->tag_index != absolute_index)
		{
			tag_validate_refuse(validation, "has tag %08lx in the table's place %ld", instance->tag_index,
				absolute_index);
			break;
		}
		if (!string_valid(validation, XBOX_POINTER(char const, instance->name)))
		{
			instance->name = xbox_address(tag_validate_empty_name);
			tag_validate_correct(validation, "has no name");
		}
		/* (a group's parents are what the game asks a tag's group by
		(tag_get): they are the group's, not the map's) */
		parent_group_tags[0] = group ? group->parent_group_tags[0] : NONE;
		parent_group_tags[1] = group ? group->parent_group_tags[1] : NONE;
		if (instance->parent_group_tags[0] != parent_group_tags[0] ||
			instance->parent_group_tags[1] != parent_group_tags[1])
		{
			char text[5];

			tag_validate_correct(validation, "has the wrong parent groups for '%s'",
				tag_to_text(instance->group_tag, text));
			instance->parent_group_tags[0] = parent_group_tags[0];
			instance->parent_group_tags[1] = parent_group_tags[1];
		}
		/* (a structure bsp's root is where it loads, set as it does) */
		if (instance->group_tag == STRUCTURE_BSP_GROUP_TAG)
		{
			instance->base_address = XBOX_NULL;
			continue;
		}
		if (!instance->base_address)
		{
			tag_validate_refuse(validation, "has no data");
			break;
		}
		if (group && group->definition)
		{
			boolean shared;

			if (!region_contains(validation, XBOX_POINTER(void, instance->base_address), group->definition->size) ||
				!extent_claim(XBOX_POINTER(void, instance->base_address), group->definition->size, group, _extent_root,
					instance, &shared))
			{
				tag_validate_refuse(validation, "has its data at %08lx, outside the tags or overlapping another's",
					(unsigned long)instance->base_address);
				break;
			}
			if (shared)
			{
				unsigned long slot = extent_slot(XBOX_POINTER(void, instance->base_address));
				struct tag_validate_extent const *extent =
					extent_next(XBOX_POINTER(void, instance->base_address), &slot);

				tag_validate_correct(validation, "is tag '%s' of its group under another name: checked as that one",
					tag_name(((struct tag_validate_instance const *)extent->owner)->tag_index));
			}
		}
	}

	/* every tag through its schema, then every tag's checks */
	for (absolute_index = 0; absolute_index < tag_count && !validation->refused; absolute_index++)
		validate_instance(validation, &instances[absolute_index], _pass_values);
	for (absolute_index = 0; absolute_index < tag_count && !validation->refused; absolute_index++)
		validate_instance(validation, &instances[absolute_index], _pass_checks);
	/* port (Arena Evolved): (extent_claim) */
	if (!shared_bytes_unchanged() && !validation->refused)
	{
		validation->tag_index = NONE;
		tag_validate_refuse(validation, "has bytes that tags of different kinds share, which its check changed");
	}

	tag_validate_globals.corrections = validation->corrections;

	return !validation->refused;
}

/* the globals for checking tags in a tag cache of tag_cache_size bytes, and
the validation of the tag_data_size bytes at tag_header */
static void validation_begin(
	struct tag_validation *validation,
	void *tag_header,
	long tag_data_size,
	unsigned long tag_cache_size,
	boolean custom_edition,
	char const *map_name)
{
	memset(&tag_validate_globals, 0, sizeof(tag_validate_globals));
	tag_validate_globals.header = tag_header;
	tag_validate_globals.tag_data_size = tag_data_size;
	tag_validate_globals.tag_cache_size = tag_cache_size;
	tag_validate_globals.custom_edition = custom_edition;
	snprintf(tag_validate_globals.map_name, sizeof(tag_validate_globals.map_name), "%s", map_name);
	memset(tag_validate_claims, 0, sizeof(tag_validate_claims));
	extent_table_new(tag_validate_extents, MAXIMUM_EXTENTS);
	validation_new(validation, tag_header, (unsigned long)tag_data_size);

	return;
}

boolean tag_validate_tags(
	void *tag_header,
	long tag_data_size,
	long file_length,
	char const *map_name)
{
	struct tag_validate_header *header = tag_header;
	struct tag_validation validation;

	validation_begin(&validation, tag_header, tag_data_size, TAG_CACHE_SIZE, FALSE, map_name);
	tag_validate_globals.file_ranges[0].offset = 0;
	tag_validate_globals.file_ranges[0].size = file_length < 0 ? 0 : (unsigned long)file_length;
	tag_validate_globals.file_range_count = 1;

	if (!schemas_fit())
	{
		tag_validate_refuse(&validation, "cannot be checked: a tag schema is wrong");
		return FALSE;
	}
	if (tag_data_size < (long)sizeof(*header) || tag_data_size > TAG_CACHE_SIZE ||
		header->signature != TAG_HEADER_SIGNATURE ||
		header->tag_count <= 0 || header->tag_count > UNSIGNED_SHORT_MAX ||
		!region_contains(&validation, header_instances(header), header->tag_count * sizeof(struct tag_validate_instance)))
	{
		tag_validate_refuse(&validation, "has a damaged tag header");
		return FALSE;
	}
	if (!claim(header, sizeof(*header)) ||
		!claim(header_instances(header), header->tag_count * sizeof(struct tag_validate_instance)) ||
		!validate_buffers(&validation, XBOX_POINTER(byte, header->vertex_buffers), header->vertex_buffer_count,
			XBOX_POINTER(byte, header->index_buffers), header->index_buffer_count))
	{
		if (!validation.refused)
			tag_validate_refuse(&validation, "has a damaged tag header");
		return FALSE;
	}

	return validate_tag_table(&validation, header_instances(header), header->tag_count);
}

boolean tag_validate_custom_edition_tags(
	void *tag_header,
	long loaded_size,
	unsigned long tag_cache_size,
	struct tag_validate_file_range const *file_ranges,
	short file_range_count,
	char const *map_name)
{
	struct tag_validate_custom_edition_header *header = tag_header;
	struct tag_validation validation;
	short range_index;

	validation_begin(&validation, tag_header, loaded_size, tag_cache_size, TRUE, map_name);
	for (range_index = 0; range_index < file_range_count && range_index < MAXIMUM_TAG_VALIDATE_FILE_RANGES; range_index++)
		tag_validate_globals.file_ranges[range_index] = file_ranges[range_index];
	tag_validate_globals.file_range_count = range_index;

	if (!schemas_fit())
	{
		tag_validate_refuse(&validation, "cannot be checked: a tag schema is wrong");
		return FALSE;
	}
	if (tag_cache_size > TAG_VALIDATE_MAXIMUM_TAG_CACHE_SIZE ||
		loaded_size < (long)sizeof(*header) || (unsigned long)loaded_size > tag_cache_size ||
		header->signature != TAG_HEADER_SIGNATURE ||
		header->tag_count <= 0 || header->tag_count > UNSIGNED_SHORT_MAX ||
		!region_contains(&validation, XBOX_POINTER(struct tag_validate_instance, header->instances), header->tag_count * sizeof(struct tag_validate_instance)) ||
		!claim(header, sizeof(*header)) ||
		!claim(XBOX_POINTER(struct tag_validate_instance, header->instances), header->tag_count * sizeof(struct tag_validate_instance)))
	{
		tag_validate_refuse(&validation, "has a damaged tag header");
		return FALSE;
	}
	/* (no buffers: its models and bsps are given theirs as they are made
	this build's, custom_edition_geometry.c) */
	validate_buffers(&validation, NULL, 0, NULL, 0);

	return validate_tag_table(&validation, header_instances(header), header->tag_count);
}

boolean tag_validate_structure_bsp(
	long tag_index,
	void *base,
	long size)
{
	struct tag_validate_header *header = tag_validate_globals.header;
	struct tag_validate_structure_bsp_header *bsp_header = base;
	struct tag_validate_instance *instance = instance_get(tag_index);
	struct tag_schema_group const *group = schema_group_get(STRUCTURE_BSP_GROUP_TAG);
	struct tag_validation validation;
	unsigned long tag_cache_offset = (unsigned long)((byte *)base - (byte *)header);

	validation_new(&validation, base, (unsigned long)size);
	validation.tag_index = tag_index;
	if (!header || !instance || instance->group_tag != STRUCTURE_BSP_GROUP_TAG ||
		size < (long)sizeof(*bsp_header) ||
		tag_cache_offset < (unsigned long)tag_validate_globals.tag_data_size ||
		tag_cache_offset > tag_validate_globals.tag_cache_size ||
		(unsigned long)size > tag_validate_globals.tag_cache_size - tag_cache_offset)
	{
		tag_validate_refuse(&validation, "has a structure bsp outside the tag cache");
		return FALSE;
	}
	/* (another bsp may have been where this one is) */
	unclaim(tag_validate_globals.tag_data_size, tag_validate_globals.tag_cache_size - tag_validate_globals.tag_data_size);
	extent_table_new(tag_validate_structure_bsp_extents, MAXIMUM_STRUCTURE_BSP_EXTENTS);

	if (bsp_header->signature != STRUCTURE_BSP_HEADER_SIGNATURE ||
		!claim(bsp_header, sizeof(*bsp_header)) ||
		!validate_buffers(&validation, XBOX_POINTER(byte, bsp_header->vertex_buffers), bsp_header->vertex_buffer_count,
			XBOX_POINTER(byte, bsp_header->index_buffers), bsp_header->index_buffer_count))
	{
		if (!validation.refused)
			tag_validate_refuse(&validation, "has a damaged structure bsp header");
		return FALSE;
	}
	if (group && group->definition)
	{
		if (!region_contains(&validation, XBOX_POINTER(void, bsp_header->base_address), group->definition->size) ||
			!claim(XBOX_POINTER(void, bsp_header->base_address), group->definition->size))
		{
			tag_validate_refuse(&validation, "has its structure bsp at %08lx, outside it",
				(unsigned long)bsp_header->base_address);
			return FALSE;
		}
		validate_tag(&validation, tag_index, XBOX_POINTER(void, bsp_header->base_address), group->definition,
			_pass_values);
		if (!validation.refused)
		{
			validate_tag(&validation, tag_index, XBOX_POINTER(void, bsp_header->base_address), group->definition,
				_pass_checks);
		}
	}
	/* port (Arena Evolved): (extent_claim) */
	if (!shared_bytes_unchanged() && !validation.refused)
		tag_validate_refuse(&validation, "has bytes that structures of different kinds share, which its check changed");
	tag_validate_globals.corrections += validation.corrections;

	return !validation.refused;
}

long tag_validate_corrections(
	void)
{
	return tag_validate_globals.corrections;
}

/* (for tools/map_validate.c's fuzzing: which bytes the last validation
found to be a tag's root, block or data) */
boolean tag_validate_claimed(
	void const *address)
{
	unsigned long offset = (unsigned long)(POINTER_BITS(address) - POINTER_BITS(tag_validate_globals.header));

	if (!tag_validate_globals.header || POINTER_BITS(address) < POINTER_BITS(tag_validate_globals.header) ||
		offset >= tag_validate_globals.tag_cache_size)
	{
		return FALSE;
	}

	return (tag_validate_claims[offset / CLAIM_BITS] & (1UL << (offset % CLAIM_BITS))) != 0;
}

/* whether any of the size bytes at address were found to be a tag's root,
block or data (for checks: a buffer's data the game draws from must not be
bytes it writes to as it runs) */
boolean tag_validate_any_claimed(
	void const *address,
	unsigned long size)
{
	unsigned long offset = (unsigned long)address - (unsigned long)tag_validate_globals.header;
	unsigned long bit;

	if (!tag_validate_globals.header || (unsigned long)address < (unsigned long)tag_validate_globals.header ||
		offset > tag_validate_globals.tag_cache_size || size > tag_validate_globals.tag_cache_size - offset)
	{
		return TRUE;
	}
	for (bit = offset; bit < offset + size; )
	{
		unsigned long word = tag_validate_claims[bit / CLAIM_BITS];

		if (bit % CLAIM_BITS == 0 && offset + size - bit >= CLAIM_BITS)
		{
			if (word)
				return TRUE;
			bit += CLAIM_BITS;
		}
		else
		{
			if (word & (1UL << (bit % CLAIM_BITS)))
				return TRUE;
			bit++;
		}
	}

	return FALSE;
}

void tag_validate_refuse(
	struct tag_validation *validation,
	char const *format,
	...)
{
	va_list arguments;

	validation->refused = TRUE;
	va_start(arguments, format);
	validation_message(validation, "cannot be played", format, arguments);
	va_end(arguments);

	return;
}

void tag_validate_correct(
	struct tag_validation *validation,
	char const *format,
	...)
{
	va_list arguments;

	/* port (Arena Evolved): bytes that another tag's walk reads too are
	not corrected for this one (extent_claim) */
	if (validation->shared_depth > 0)
	{
		char message[MAXIMUM_MESSAGE_LENGTH];

		va_start(arguments, format);
		vsnprintf(message, sizeof(message), format, arguments);
		va_end(arguments);
		tag_validate_refuse(validation, "shares its data with another tag, and would need correcting here: %s", message);
		return;
	}
	validation->corrections++;
	if (validation->corrections <= MAXIMUM_LOGGED_CORRECTIONS)
	{
		va_start(arguments, format);
		validation_message(validation, "is corrected", format, arguments);
		va_end(arguments);
	}
	else if (validation->corrections == MAXIMUM_LOGGED_CORRECTIONS + 1)
	{
		char message[MAXIMUM_MESSAGE_LENGTH];

		snprintf(message, sizeof(message), "the map '%s' has more corrections, not logged",
			tag_validate_globals.map_name);
		tag_validate_report(message);
	}

	return;
}

boolean tag_validate_file_contains(
	struct tag_validation *validation,
	long offset,
	long size)
{
	short range_index;

	(void)validation;
	if (offset < 0 || size < 0)
		return FALSE;
	for (range_index = 0; range_index < tag_validate_globals.file_range_count; range_index++)
	{
		struct tag_validate_file_range const *range = &tag_validate_globals.file_ranges[range_index];

		if ((unsigned long)offset >= range->offset && (unsigned long)offset - range->offset <= range->size &&
			(unsigned long)size <= range->size - ((unsigned long)offset - range->offset))
		{
			return TRUE;
		}
	}

	return FALSE;
}

void *tag_validate_root(
	struct tag_validation *validation)
{
	return validation->depth > 0 ? validation->frames[0].base : NULL;
}

boolean tag_validate_custom_edition(
	struct tag_validation *validation)
{
	(void)validation;

	return tag_validate_globals.custom_edition;
}

boolean tag_validate_contains(
	struct tag_validation *validation,
	void const *address,
	unsigned long size)
{
	return region_contains(validation, address, size);
}

void *tag_validate_tag_get(
	struct tag_validation *validation,
	long tag_index,
	unsigned long group_tag)
{
	struct tag_validate_instance *instance = instance_get(tag_index);
	unsigned long groups[2];

	(void)validation;
	groups[0] = group_tag;
	groups[1] = 0;
	if (!instance || !instance->base_address || !instance_in_groups(instance, groups))
		return NULL;

	return XBOX_POINTER(void, instance->base_address);
}

static void *buffer_data(
	byte *buffers,
	long count,
	void const *buffer)
{
	unsigned long offset = (unsigned long)(POINTER_BITS(buffer) - POINTER_BITS(buffers));

	if (POINTER_BITS(buffer) < POINTER_BITS(buffers) || offset % BUFFER_SIZE ||
		offset / BUFFER_SIZE >= (unsigned long)count)
	{
		return NULL;
	}

	return XBOX_POINTER(void, *(XPTR(void) const *)((byte const *)buffer + 4));
}

void *tag_validate_vertex_buffer_data(
	struct tag_validation *validation,
	void const *buffer)
{
	return buffer_data(validation->vertex_buffers, validation->vertex_buffer_count, buffer);
}

void *tag_validate_index_buffer_data(
	struct tag_validation *validation,
	void const *buffer)
{
	return buffer_data(validation->index_buffers, validation->index_buffer_count, buffer);
}
