/*
AE_PACK_BUILD.C

The weapon pack builder (ae_pack_build.h; docs/weapon-packs.md, "Building
a pack"), compiled only into tools/ae_pack_build.c.

A DONOR map holds the weapon. Its tags are read where the game reads a
map's (the tool puts them at the tag cache's address) and checked by the
tag validator exactly as a map's are (tag_validate_tags): after that every
block, data and tag reference the schema names can be trusted. From the
weapon (and any other roots), each tag is copied through its group's
schema (tag_schema.h) into one run of bytes, its blocks and data after its
root, and each pointer, tag reference and tag index is written down as a
FIXUP rather than kept: the bytes hold no address of the donor's. What the
schema does not say is taken by hand: a model part's vertices and indices
(out of the donor's buffers: PARTS), a bitmap's pixels and a sound's
samples (out of the donor's file: RESOURCES), a sound permutation's own
tag index.

A tag a map has itself (its group and name in the external names, the
stock maps' tags) is EXTERNAL: named only, nothing below it walked; it
is required unless its group is one a map may lack (the table below).
Every other tag is OWN: renamed under the prefix (decision 9), its donor
name kept as its source path.

The pack goes out through ae_pack_write, which checks it as the reader
does: tags in the order the walk meets them (breadth-first from the
roots), so the same donor and options give the same bytes.

Nothing here trusts the donor beyond what the validator checked, and every
copy is bounded again (the spike's pack_build, ae/br-prototype b2b489ac
ae_weapon_pack.c, moved out of the game).
*/

/* ---------- headers */

#include "cseries.h"
#include "tag_files/tag_groups.h"
#include "bitmaps/bitmap_group.h"
#include "rasterizer/rasterizer_geometry.h"
#include "sound/sound_definitions.h"
#include "tag_schema.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ae_pack_build.h"

/* (a host tool's unit: the C library's allocator, not the game's debug one that cseries.h names them) */
#undef malloc
#undef free
#undef realloc

/* ---------- constants */

enum
{
	PACK_TAG_HEADER_SIGNATURE = 0x74616773, /* 'tags' */
	PACK_BUFFER_SIZE = 12, /* a D3DVertexBuffer or D3DIndexBuffer in the tags */
	/* everything the pack holds (its file's limit) */
	MAXIMUM_PACK_BYTES = AE_PACK_MAXIMUM_FILE_SIZE,
	MAXIMUM_PACK_DEPTH = 16,

	/* tag layout 1's vertex sizes of a model part's two types (docs/weapon-packs.md, PARTS) */
	MODEL_UNCOMPRESSED_VERTEX_SIZE = 68,
	MODEL_COMPRESSED_VERTEX_SIZE = 32,

	/* the bitmap values tag_schema_render.c names (the game's units keep them to themselves) */
	PACK_BITMAP_TYPE_CUBE_MAP = 2,
	PACK_BITMAP_FORMATS = 18,
	PACK_BITMAP_COMPRESSED_BIT = 1,
};

#define PACK_WEAPON_GROUP 0x77656170 /* 'weap' */
#define PACK_ITEM_COLLECTION_GROUP 0x69746D63 /* 'itmc' */
#define PACK_BITMAP_GROUP 0x6269746D /* 'bitm' */
#define PACK_STRUCTURE_BSP_GROUP 0x73627370 /* 'sbsp' */
#define PACK_SCENARIO_GROUP 0x73636E72 /* 'scnr' */

/* the groups a map may lack a pack's external tag of: such a tag is not required (its references resolve to NONE
on a map without it, the engine's weapon pack loader). Checked for each group: every field of tag_schema*.c naming
it is a reference or tag index the schema lets be NONE (TAG_SCHEMA_REFERENCE, _REFERENCE_ARRAY, _TAG_INDEX and the
dialogue's vocalizations all carry _tag_schema_none_bit, so the validator itself makes any of them NONE when it is
not a tag of the group), the checks that look at them (effect_part_check, predicted_resource_check, decal_check)
take NONE first, and tag_validate.c names none of them (tools/test_weapon_pack_build.py holds this table to that).
What reads them draws, plays or spawns nothing for NONE. */
static struct
{
	unsigned long group_tag;
	char const *why;
} const optional_external_groups[] =
{
	/* effects: weapons' firing / ready / reload effects, projectiles' and material responses' effects, damage
	regions', items' detonations, objects' creation effects and attachments, particles' collision effects */
	{ 'effe', "effect" },
	/* sounds: the same references where they may be sounds, the weapon's pickup and zoom sounds, projectiles'
	flyby, animation graphs' sounds, effect parts, damage effects, material effects, predicted resources */
	{ 'snd!', "sound" },
	/* decals: effect parts and the next decal in a chain */
	{ 'deca', "decal" },
	/* particles: effects' particles and breakable surfaces' */
	{ 'part', "particle" },
	/* lights: effect parts and objects' attachments */
	{ 'ligh', "light" },
};

/* bits a pixel of each format (tag_schema_render.c's bitmap_format_bits_per_pixel) */
static char const bitmap_format_bits_per_pixel[PACK_BITMAP_FORMATS] =
{
	8, 8, 8, 16, 0, 0, 16, 0, 16, 16, 32, 32, 0, 0, 4, 8, 8, 8
};

/* ---------- structures */

/* cache_files.c's struct cache_file_tag_instance */
struct pack_tag_instance
{
	unsigned long group_tag;
	unsigned long parent_group_tags[2];
	long tag_index;
	char *name;
	void *base_address;
	unsigned long unused[2];
};

/* cache_files.c's struct cache_file_tag_header */
struct pack_tag_header
{
	struct pack_tag_instance *instances;
	long scenario_tag_index;
	unsigned long checksum;
	long tag_count;
	long vertex_buffer_count;
	byte *vertex_buffers;
	long index_buffer_count;
	byte *index_buffers;
	unsigned long signature;
};

/* models.c's struct model_geometry_part (tag_schema_models.c) */
struct pack_model_part
{
	unsigned long flags;
	short shader_index;
	char previous_part_index;
	char next_part_index;
	short centroid_primary_node_index;
	short centroid_secondary_node_index;
	real centroid_primary_node_weight;
	real centroid_secondary_node_weight;
	real_point3d centroid;
	struct tag_block uncompressed_vertices;
	struct tag_block compressed_vertices;
	struct tag_block triangles;
	struct triangle_buffer triangle_buffer;
	struct vertex_buffer vertex_buffer;
};

typedef char verify_pack_tag_instance_size[sizeof(struct pack_tag_instance) == 0x20 ? 1 : -1];
typedef char verify_pack_tag_header_size[sizeof(struct pack_tag_header) == 0x24 ? 1 : -1];
typedef char verify_pack_model_part_size[sizeof(struct pack_model_part) == 0x68 ? 1 : -1];

/* a pack tag as it is built */
struct builder_tag
{
	unsigned long group_tag;
	unsigned long parent_group_tags[2];
	char *name; /* the pack's */
	char *source_path; /* the donor's name (own tags), "" (externals) */
	long donor_index; /* its absolute index in the donor */
	unsigned char kind;
	unsigned char required;
	unsigned char flags;
	/* own tags: its root, then its blocks and data */
	byte *bytes;
	unsigned long size;
	unsigned long capacity;
	struct ae_pack_fixup_record *fixups;
	unsigned long fixup_count;
	unsigned long fixup_capacity;
	struct ae_pack_build_part *parts;
	unsigned long part_count;
	unsigned long part_capacity;
	struct ae_pack_build_resource *resources;
	unsigned long resource_count;
	unsigned long resource_capacity;
	unsigned long resource_bytes;
};

struct builder
{
	struct ae_pack_builder_options const *options;
	struct ae_pack_problem *problem;
	boolean failed;
	/* the donor's tags (validated) and its file */
	byte *tags;
	unsigned long tags_size;
	struct pack_tag_instance *instances;
	long instance_count;
	byte *vertex_buffers;
	long vertex_buffer_count;
	byte *index_buffers;
	long index_buffer_count;
	/* each donor tag's pack tag, or NONE */
	long *pack_indices;
	/* the pack's tags */
	struct builder_tag *pack_tags;
	long tag_count;
	/* everything the pack holds, against MAXIMUM_PACK_BYTES */
	unsigned long bytes;
	/* the tag being copied: its donor root */
	byte const *current_root;
	/* the summary's tags */
	struct ae_pack_builder_tag *summary_tags;
};

/* ---------- private code */

static void builder_refuse(
	struct builder *builder,
	char const *format,
	...)
{
	va_list arguments;

	if (builder->failed)
		return;
	builder->failed = TRUE;
	va_start(arguments, format);
	vsnprintf(builder->problem->text, sizeof(builder->problem->text), format, arguments);
	va_end(arguments);
}

static int ascii_lower(
	int character)
{
	return character >= 'A' && character <= 'Z' ? character - 'A' + 'a' : character;
}

/* whether the names are the same but for ASCII case (as the engine's tag_loaded compares them) */
static boolean names_equal(
	char const *a,
	char const *b)
{
	while (*a && ascii_lower((unsigned char)*a) == ascii_lower((unsigned char)*b))
	{
		a++;
		b++;
	}
	return ascii_lower((unsigned char)*a) == ascii_lower((unsigned char)*b);
}

static char *group_text(
	unsigned long group_tag,
	char *text)
{
	short index;

	for (index = 0; index < 4; index++)
	{
		char character = (char)(group_tag >> (24 - 8 * index));

		text[index] = character >= ' ' && character < 0x7f ? character : '?';
	}
	text[4] = 0;

	return text;
}

/* whether size bytes at address lie in the size bytes at region */
static boolean region_contains(
	void const *region,
	unsigned long region_size,
	void const *address,
	unsigned long size)
{
	unsigned long start = (unsigned long)region;
	unsigned long at = (unsigned long)address;

	return address && at >= start && at - start <= region_size && size <= region_size - (at - start);
}

static boolean donor_contains(
	struct builder const *builder,
	void const *address,
	unsigned long size)
{
	return region_contains(builder->tags, builder->tags_size, address, size);
}

/* the length of a tag's name in the tags (size bytes at tags): NONE if it is not in them, is longer than a pack's
names or not printable ASCII (a pack's names are), or is empty */
static long tag_name_length(
	byte const *tags,
	unsigned long tags_size,
	char const *name)
{
	unsigned long start = (unsigned long)tags;
	unsigned long at = (unsigned long)name;
	unsigned long room;
	char const *end;
	long index;

	if (!name || at < start || at - start >= tags_size)
		return NONE;
	room = tags_size - (at - start);
	if (room > AE_PACK_MAXIMUM_NAME + 1)
		room = AE_PACK_MAXIMUM_NAME + 1;
	end = memchr(name, 0, room);
	if (!end || end == name)
		return NONE;
	for (index = 0; name + index < end; index++)
	{
		if ((unsigned char)name[index] < 0x20 || (unsigned char)name[index] > 0x7e)
			return NONE;
	}

	return (long)(end - name);
}

static struct tag_schema_group const *schema_group_get(
	unsigned long group_tag)
{
	struct tag_schema_group const *const *list;

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

static boolean group_optional(
	unsigned long group_tag)
{
	short index;

	for (index = 0; index < (short)NUMBEROF(optional_external_groups); index++)
	{
		if (optional_external_groups[index].group_tag == group_tag)
			return TRUE;
	}

	return FALSE;
}

/* whether the map has a tag of the group and name (the external names) */
static boolean name_external(
	struct builder const *builder,
	unsigned long group_tag,
	char const *name)
{
	unsigned long index;

	for (index = 0; index < builder->options->external_name_count; index++)
	{
		struct ae_pack_builder_name const *external = &builder->options->external_names[index];

		if (external->group == group_tag && names_equal(external->name, name))
			return TRUE;
	}

	return FALSE;
}

static char *string_copy(
	struct builder *builder,
	char const *first,
	char const *second)
{
	unsigned long first_length = (unsigned long)strlen(first);
	unsigned long second_length = second ? (unsigned long)strlen(second) : 0;
	char *copy = malloc(first_length + second_length + 1);

	if (!copy)
	{
		builder_refuse(builder, "out of memory");
		return NULL;
	}
	memcpy(copy, first, first_length);
	if (second_length)
		memcpy(copy + first_length, second, second_length);
	copy[first_length + second_length] = 0;

	return copy;
}

/* an own tag's pack name: the prefix and its donor name, a leading "cmt\" or "ae_pack\" dropped (the art's makers'
folders: the rest, "_smooth" suffixes too, stays a name only) */
static char const *own_name_rest(
	char const *donor_name)
{
	static char const *const dropped[] = { "cmt\\", "ae_pack\\" };
	short index;

	for (index = 0; index < (short)NUMBEROF(dropped); index++)
	{
		unsigned long length = (unsigned long)strlen(dropped[index]);
		unsigned long character;

		for (character = 0; character < length; character++)
		{
			if (ascii_lower((unsigned char)donor_name[character]) != dropped[index][character])
				break;
		}
		if (character == length && donor_name[length])
			return donor_name + length;
	}

	return donor_name;
}

static void *array_grow(
	struct builder *builder,
	void *array,
	unsigned long *capacity,
	unsigned long count,
	unsigned long element_size)
{
	unsigned long new_capacity;
	void *grown;

	if (count < *capacity)
		return array;
	new_capacity = *capacity ? *capacity * 2 : 16;
	if (new_capacity > 0x7FFFFFFFUL / element_size)
	{
		builder_refuse(builder, "out of memory");
		return NULL;
	}
	grown = realloc(array, new_capacity * element_size);
	if (!grown)
	{
		builder_refuse(builder, "out of memory");
		return NULL;
	}
	*capacity = new_capacity;

	return grown;
}

/* size more bytes at the end of the tag's (zeroed, on a 4-byte boundary): their offset, or NONE */
static long tag_bytes_append(
	struct builder *builder,
	struct builder_tag *tag,
	void const *source,
	unsigned long size)
{
	unsigned long offset = (tag->size + 3) & ~3UL;

	if (builder->failed)
		return NONE;
	if (size > AE_PACK_MAXIMUM_TAG_SIZE || offset > AE_PACK_MAXIMUM_TAG_SIZE - size)
	{
		builder_refuse(builder, "%s is larger than a pack's tags (4 MiB)", tag->source_path);
		return NONE;
	}
	if (builder->bytes > MAXIMUM_PACK_BYTES - size)
	{
		builder_refuse(builder, "the pack would be larger than its limit (64 MiB)");
		return NONE;
	}
	if (offset + size > tag->capacity)
	{
		unsigned long capacity = tag->capacity ? tag->capacity : 1024;
		byte *bytes;

		while (capacity < offset + size)
			capacity *= 2;
		bytes = realloc(tag->bytes, capacity);
		if (!bytes)
		{
			builder_refuse(builder, "out of memory");
			return NONE;
		}
		memset(bytes + tag->capacity, 0, capacity - tag->capacity);
		tag->bytes = bytes;
		tag->capacity = capacity;
	}
	if (source)
		memcpy(tag->bytes + offset, source, size);
	tag->size = offset + size;
	builder->bytes += size;

	return (long)offset;
}

static void tag_fixup_add(
	struct builder *builder,
	struct builder_tag *tag,
	unsigned short kind,
	unsigned long offset,
	unsigned long value)
{
	struct ae_pack_fixup_record *fixup;

	if (builder->failed)
		return;
	if (tag->fixup_count >= AE_PACK_MAXIMUM_FIXUPS)
	{
		builder_refuse(builder, "%s has more fixups than a pack holds", tag->source_path);
		return;
	}
	fixup = array_grow(builder, tag->fixups, &tag->fixup_capacity, tag->fixup_count, sizeof(*fixup));
	if (!fixup)
		return;
	tag->fixups = fixup;
	fixup = &tag->fixups[tag->fixup_count++];
	fixup->offset = (unsigned int)offset;
	fixup->kind = kind;
	fixup->pad = 0;
	fixup->value = (unsigned int)value;
}

/* the donor's file's size bytes at offset kept for the tag (a bitmap's pixels, a sound's samples), once */
static void tag_resource_add(
	struct builder *builder,
	struct builder_tag *tag,
	unsigned long offset,
	unsigned long size)
{
	struct ae_pack_build_resource *resource;
	unsigned long index;

	if (builder->failed || !size)
		return;
	if (offset > builder->options->file_size || size > builder->options->file_size - offset)
	{
		builder_refuse(builder, "pixels or samples outside the donor's file, of %s", tag->source_path);
		return;
	}
	if (size > AE_PACK_MAXIMUM_RESOURCE_SIZE)
	{
		builder_refuse(builder, "%s has pixels or samples larger than a pack's (8 MiB)", tag->source_path);
		return;
	}
	for (index = 0; index < tag->resource_count; index++)
	{
		if (tag->resources[index].map_offset == offset && tag->resources[index].size == size)
			return;
	}
	if (builder->bytes > MAXIMUM_PACK_BYTES - size)
	{
		builder_refuse(builder, "the pack would be larger than its limit (64 MiB)");
		return;
	}
	resource = array_grow(builder, tag->resources, &tag->resource_capacity, tag->resource_count, sizeof(*resource));
	if (!resource)
		return;
	tag->resources = resource;
	resource = &tag->resources[tag->resource_count++];
	resource->map_offset = (unsigned int)offset;
	resource->data = builder->options->file + offset;
	resource->size = (unsigned int)size;
	tag->resource_bytes += size;
	builder->bytes += size;
}

/* the pack tag of a donor tag, added to the pack if it is new (an own tag's bytes are copied later, from
ae_pack_builder_build's loop): NONE for no tag, or once the build has failed */
static long pack_tag_of(
	struct builder *builder,
	long donor_tag_index)
{
	long absolute_index = donor_tag_index & 0xffff;
	struct pack_tag_instance const *instance;
	struct builder_tag *tag;
	long length;
	char text[5];

	if (donor_tag_index == NONE || builder->failed)
		return NONE;
	if (absolute_index >= builder->instance_count ||
		builder->instances[absolute_index].tag_index != donor_tag_index)
	{
		/* (the validator made every reference a tag or NONE) */
		builder_refuse(builder, "a reference that is not a tag of the donor's");
		return NONE;
	}
	if (builder->pack_indices[absolute_index] != NONE)
		return builder->pack_indices[absolute_index];

	instance = &builder->instances[absolute_index];
	length = tag_name_length(builder->tags, builder->tags_size, instance->name);
	if (length == NONE)
	{
		builder_refuse(builder, "a tag of group %s has no name a pack can carry",
			group_text(instance->group_tag, text));
		return NONE;
	}
	if (instance->group_tag == PACK_STRUCTURE_BSP_GROUP || instance->group_tag == PACK_SCENARIO_GROUP)
	{
		builder_refuse(builder, "the weapon leads to a scenario or structure bsp: %s", instance->name);
		return NONE;
	}
	if (builder->tag_count >= AE_PACK_MAXIMUM_TAGS)
	{
		builder_refuse(builder, "the weapon leads to more tags than a pack holds (%d)", AE_PACK_MAXIMUM_TAGS);
		return NONE;
	}

	tag = &builder->pack_tags[builder->tag_count];
	memset(tag, 0, sizeof(*tag));
	tag->group_tag = instance->group_tag;
	tag->parent_group_tags[0] = instance->parent_group_tags[0];
	tag->parent_group_tags[1] = instance->parent_group_tags[1];
	tag->donor_index = absolute_index;
	if (name_external(builder, instance->group_tag, instance->name))
	{
		tag->kind = AE_PACK_TAG_EXTERNAL;
		tag->required = group_optional(instance->group_tag) ? 0 : 1;
		tag->name = string_copy(builder, instance->name, NULL);
		tag->source_path = string_copy(builder, "", NULL);
	}
	else
	{
		tag->kind = AE_PACK_TAG_OWN;
		tag->name = string_copy(builder, builder->options->prefix, own_name_rest(instance->name));
		tag->source_path = string_copy(builder, instance->name, NULL);
		if (tag->name && strlen(tag->name) > AE_PACK_MAXIMUM_NAME)
		{
			builder_refuse(builder, "%s's pack name is longer than %d characters", instance->name,
				AE_PACK_MAXIMUM_NAME);
		}
	}
	if (builder->failed)
	{
		free(tag->name);
		free(tag->source_path);
		return NONE;
	}
	builder->pack_indices[absolute_index] = builder->tag_count;

	return builder->tag_count++;
}

/* the bytes of pixels the game reads for a bitmap (tag_schema_render.c's bitmap_read_pixel_data_size: every
mipmap, compressed sides rounded to 4, a cube map's six faces), or NONE */
static long bitmap_read_size(
	struct bitmap_data const *bitmap)
{
	boolean compressed = TEST_FLAG(bitmap->flags, PACK_BITMAP_COMPRESSED_BIT) != 0;
	long bits_per_pixel;
	unsigned long pixel_count = 0;
	short mipmap_index;

	if (bitmap->format < 0 || bitmap->format >= PACK_BITMAP_FORMATS || bitmap->mipmap_count < 0 ||
		bitmap->mipmap_count > 16)
	{
		return NONE;
	}
	bits_per_pixel = bitmap_format_bits_per_pixel[bitmap->format];
	for (mipmap_index = 0; mipmap_index <= bitmap->mipmap_count; mipmap_index++)
	{
		unsigned long width = MAX(bitmap->width >> mipmap_index, 1);
		unsigned long height = MAX(bitmap->height >> mipmap_index, 1);
		unsigned long depth = MAX(bitmap->depth >> mipmap_index, 1);

		if (compressed)
		{
			width += (-width) & 3;
			height += (-height) & 3;
		}
		pixel_count += width * height * depth * (bitmap->type == PACK_BITMAP_TYPE_CUBE_MAP ? 6 : 1);
	}
	if (!bits_per_pixel || pixel_count > (unsigned long)AE_PACK_MAXIMUM_RESOURCE_SIZE * 8 / bits_per_pixel)
		return NONE;

	return (long)(pixel_count * bits_per_pixel / 8);
}

/* a model part: its vertices and indices out of the donor's buffers (as model_geometry_part_check found them:
tag_schema_models.c), and no buffers in the copy (the engine makes its own from the PARTS record) */
static void pack_model_part(
	struct builder *builder,
	struct builder_tag *tag,
	struct pack_model_part const *source,
	unsigned long offset)
{
	struct pack_model_part *copy = (struct pack_model_part *)(tag->bytes + offset);
	byte const *vertex_buffer = (byte const *)source->vertex_buffer.hardware_format;
	byte const *index_buffer = (byte const *)source->triangle_buffer.hardware_format;
	struct ae_pack_build_part *part;
	unsigned long vertex_offset, index_offset;
	byte const *vertices;
	byte const *indices;
	unsigned long vertex_size, index_count;

	memset(&copy->vertex_buffer, 0, sizeof(copy->vertex_buffer));
	memset(&copy->triangle_buffer, 0, sizeof(copy->triangle_buffer));
	/* (a part the validator left none is not drawn) */
	if (!vertex_buffer || !index_buffer)
		return;
	vertex_offset = (unsigned long)(vertex_buffer - builder->vertex_buffers);
	index_offset = (unsigned long)(index_buffer - builder->index_buffers);
	if (vertex_buffer < builder->vertex_buffers || vertex_offset % PACK_BUFFER_SIZE ||
		vertex_offset / PACK_BUFFER_SIZE >= (unsigned long)builder->vertex_buffer_count ||
		index_buffer < builder->index_buffers || index_offset % PACK_BUFFER_SIZE ||
		index_offset / PACK_BUFFER_SIZE >= (unsigned long)builder->index_buffer_count)
	{
		return;
	}
	if (source->vertex_buffer.count <= 0 || source->triangle_buffer.count <= 0)
		return;
	switch (source->vertex_buffer.type)
	{
	case AE_PACK_VERTEX_MODEL_UNCOMPRESSED:
		vertex_size = MODEL_UNCOMPRESSED_VERTEX_SIZE;
		break;
	case AE_PACK_VERTEX_MODEL_COMPRESSED:
		vertex_size = MODEL_COMPRESSED_VERTEX_SIZE;
		break;
	default:
		builder_refuse(builder, "%s has a model part of vertex type %d (a pack's are 4 or 5)", tag->source_path,
			source->vertex_buffer.type);
		return;
	}
	if (source->vertex_buffer.count > AE_PACK_MAXIMUM_PART_COUNT ||
		source->triangle_buffer.count > AE_PACK_MAXIMUM_PART_COUNT)
	{
		builder_refuse(builder, "%s has a model part larger than a pack's", tag->source_path);
		return;
	}
	switch (source->triangle_buffer.type)
	{
	case _triangle_buffer_type_triangles:
		index_count = 3 * (unsigned long)source->triangle_buffer.count;
		break;
	case _triangle_buffer_type_precompiled_strip:
		index_count = (unsigned long)source->triangle_buffer.count + 2;
		break;
	default:
		return;
	}
	/* (a buffer's Data: its bytes' address in the tags, as the donor's were never registered) */
	vertices = *(byte const *const *)(vertex_buffer + 4);
	indices = *(byte const *const *)(index_buffer + 4);
	if (!donor_contains(builder, vertices, (unsigned long)source->vertex_buffer.count * vertex_size) ||
		!donor_contains(builder, indices, index_count * sizeof(word)))
	{
		return;
	}
	if (builder->bytes > MAXIMUM_PACK_BYTES - ((unsigned long)source->vertex_buffer.count * vertex_size +
		index_count * sizeof(word)))
	{
		builder_refuse(builder, "the pack would be larger than its limit (64 MiB)");
		return;
	}

	part = array_grow(builder, tag->parts, &tag->part_capacity, tag->part_count, sizeof(*part));
	if (!part)
		return;
	tag->parts = part;
	part = &tag->parts[tag->part_count++];
	memset(part, 0, sizeof(*part));
	part->offset = (unsigned int)offset;
	part->vertex_type = (unsigned short)source->vertex_buffer.type;
	part->triangle_type = (unsigned short)source->triangle_buffer.type;
	part->vertex_count = (unsigned int)source->vertex_buffer.count;
	part->triangle_count = (unsigned int)source->triangle_buffer.count;
	part->vertices = vertices;
	part->vertices_size = (unsigned int)(source->vertex_buffer.count * vertex_size);
	/* (rasterizer_triangle_buffer_new reads 2 * count + 4 bytes of a strip, 6 * count of a list: index_count
	words) */
	part->indices = indices;
	part->indices_size = (unsigned int)(index_count * sizeof(word));
	builder->bytes += part->vertices_size + part->indices_size;
}

/* an element of the donor's (source), copied at offset in the tag's bytes: its fields through the schema, its
blocks' elements after it */
static void pack_element(
	struct builder *builder,
	long tag_pack_index,
	byte const *source,
	unsigned long offset,
	struct tag_schema_definition const *definition,
	short depth)
{
	struct tag_schema_field const *field;

	if (depth >= MAXIMUM_PACK_DEPTH)
	{
		builder_refuse(builder, "%s is deeper than a pack takes", builder->pack_tags[tag_pack_index].source_path);
		return;
	}
	for (field = definition->fields; field->type != _tag_schema_terminator && !builder->failed; field++)
	{
		short index;

		for (index = 0; index < field->count && !builder->failed; index++)
		{
			unsigned long field_offset = (unsigned long)field->offset + (unsigned long)(index * field->size);
			byte const *address = source + field_offset;
			/* (the tag's bytes move as they grow: taken again each time) */
			struct builder_tag *tag = &builder->pack_tags[tag_pack_index];

			switch (field->type)
			{
			case _tag_schema_struct:
				pack_element(builder, tag_pack_index, address, offset + field_offset, field->definition, depth + 1);
				break;

			case _tag_schema_block:
			{
				struct tag_block const *block = (struct tag_block const *)address;
				struct tag_schema_definition const *element_definition = field->definition;
				struct tag_block *copy;
				long copy_offset = NONE;

				if (block->count > 0)
				{
					unsigned long element_size = (unsigned long)element_definition->size;
					unsigned long size = (unsigned long)block->count * element_size;
					byte const *elements = (byte const *)block->address;
					long element_index;

					if ((unsigned long)block->count > MAXIMUM_PACK_BYTES / element_size ||
						!donor_contains(builder, elements, size))
					{
						builder_refuse(builder, "a block outside the donor's tags, in %s", tag->source_path);
						break;
					}
					copy_offset = tag_bytes_append(builder, tag, elements, size);
					if (copy_offset == NONE)
						break;
					tag_fixup_add(builder, tag, AE_PACK_FIXUP_POINTER,
						offset + field_offset + offsetof(struct tag_block, address), (unsigned long)copy_offset);
					for (element_index = 0; element_index < block->count && !builder->failed; element_index++)
					{
						pack_element(builder, tag_pack_index, elements + element_index * element_size,
							(unsigned long)copy_offset + element_index * element_size, element_definition,
							depth + 1);
					}
				}
				tag = &builder->pack_tags[tag_pack_index];
				copy = (struct tag_block *)(tag->bytes + offset + field_offset);
				copy->address = XBOX_NULL;
				copy->definition = XBOX_NULL;
				if (copy_offset == NONE)
					copy->count = 0;
				break;
			}

			case _tag_schema_data:
			{
				struct tag_data const *data = (struct tag_data const *)address;
				struct tag_data *copy;
				long copy_offset = NONE;

				if (data->size > 0)
				{
					byte const *bytes = (byte const *)data->address;

					if (!donor_contains(builder, bytes, (unsigned long)data->size))
					{
						builder_refuse(builder, "data outside the donor's tags, in %s", tag->source_path);
						break;
					}
					copy_offset = tag_bytes_append(builder, tag, bytes, (unsigned long)data->size);
					if (copy_offset == NONE)
						break;
					tag_fixup_add(builder, tag, AE_PACK_FIXUP_POINTER,
						offset + field_offset + offsetof(struct tag_data, address), (unsigned long)copy_offset);
				}
				tag = &builder->pack_tags[tag_pack_index];
				copy = (struct tag_data *)(tag->bytes + offset + field_offset);
				copy->address = XBOX_NULL;
				copy->definition = XBOX_NULL;
				if (copy_offset == NONE)
					copy->size = 0;
				break;
			}

			case _tag_schema_file_data:
			{
				struct tag_data const *data = (struct tag_data const *)address;
				struct tag_data *copy = (struct tag_data *)(tag->bytes + offset + field_offset);

				copy->address = XBOX_NULL;
				copy->definition = XBOX_NULL;
				if (data->size > 0 && data->file_offset >= 0)
					tag_resource_add(builder, tag, (unsigned long)data->file_offset, (unsigned long)data->size);
				else
					copy->size = 0;
				break;
			}

			case _tag_schema_reference:
			{
				struct tag_reference const *reference = (struct tag_reference const *)address;
				long referenced = pack_tag_of(builder, reference->index);
				struct tag_reference *copy;

				/* (every reference, a tag's or none's: its name and index are the donor's, which are not
				kept) */
				tag = &builder->pack_tags[tag_pack_index];
				copy = (struct tag_reference *)(tag->bytes + offset + field_offset);
				copy->name = XBOX_NULL;
				copy->index = NONE;
				tag_fixup_add(builder, tag, AE_PACK_FIXUP_REFERENCE, offset + field_offset,
					referenced == NONE ? AE_PACK_NONE : (unsigned long)referenced);
				break;
			}

			case _tag_schema_tag_index:
			{
				long referenced = pack_tag_of(builder, *(long const *)address);

				tag = &builder->pack_tags[tag_pack_index];
				*(long *)(tag->bytes + offset + field_offset) = NONE;
				if (referenced != NONE)
				{
					tag_fixup_add(builder, tag, AE_PACK_FIXUP_TAG_INDEX, offset + field_offset,
						(unsigned long)referenced);
				}
				break;
			}

			default:
				break;
			}
		}
	}
	if (builder->failed)
		return;

	/* what the schema does not say */
	{
		struct builder_tag *tag = &builder->pack_tags[tag_pack_index];

		if (!strcmp(definition->name, "model_geometry_part") &&
			definition->size == (long)sizeof(struct pack_model_part))
		{
			pack_model_part(builder, tag, (struct pack_model_part const *)source, offset);
		}
		else if (!strcmp(definition->name, "sound_permutation") &&
			definition->size == (long)sizeof(struct sound_permutation))
		{
			/* (the sound's own tag index, which the sound cache reads the samples by: xbox_sound_cache.c's
			cache_tag_index, and the runtime's) */
			*(long *)(tag->bytes + offset + offsetof(struct sound_permutation, unknown2)) = NONE;
			*(long *)(tag->bytes + offset + offsetof(struct sound_permutation, unknown3)) = NONE;
			tag_fixup_add(builder, tag, AE_PACK_FIXUP_TAG_INDEX, offset + offsetof(struct sound_permutation, unknown2),
				(unsigned long)tag_pack_index);
			tag_fixup_add(builder, tag, AE_PACK_FIXUP_TAG_INDEX, offset + offsetof(struct sound_permutation, unknown3),
				(unsigned long)tag_pack_index);
		}
		else if (!strcmp(definition->name, "bitmap_data") && definition->size == (long)sizeof(struct bitmap_data) &&
			tag->group_tag == PACK_BITMAP_GROUP)
		{
			/* (its pixels, where the texture cache reads them: texture_cache_bitmap_new adds the group's offset to
			the bitmap's, and reads what its size, format and mipmaps make) */
			struct bitmap_data const *bitmap = (struct bitmap_data const *)source;
			struct bitmap_group const *group = (struct bitmap_group const *)builder->current_root;
			long size = bitmap_read_size(bitmap);

			if (bitmap->pixels_size > size)
				size = bitmap->pixels_size;
			if (size > 0 && group->pixel_data.file_offset >= 0 && bitmap->pixels_offset >= 0)
			{
				tag_resource_add(builder, tag,
					(unsigned long)group->pixel_data.file_offset + (unsigned long)bitmap->pixels_offset,
					(unsigned long)size);
			}
		}
	}
}

/* the donor's tags checked by the validator, and its tag header read */
static boolean donor_tags_read(
	struct builder *builder,
	void *tag_data,
	unsigned long tag_data_size,
	unsigned long file_size,
	char const *name,
	boolean shared_bytes)
{
	struct pack_tag_header *header = tag_data;
	boolean valid;

	if ((unsigned long)tag_data != AE_PACK_BUILDER_TAG_CACHE_ADDRESS || tag_data_size < sizeof(*header) ||
		tag_data_size > AE_PACK_BUILDER_TAG_CACHE_SIZE)
	{
		builder_refuse(builder, "%s's tags are not where the game reads them, or do not fit", name);
		return FALSE;
	}
	tag_validate_allow_shared_bytes(shared_bytes);
	valid = tag_validate_tags(tag_data, (long)tag_data_size, (long)file_size, name);
	tag_validate_allow_shared_bytes(FALSE);
	if (!valid)
	{
		builder_refuse(builder, "%s's tags did not pass the map check", name);
		return FALSE;
	}
	builder->tags = tag_data;
	builder->tags_size = tag_data_size;
	builder->instances = header->instances;
	builder->instance_count = header->tag_count;
	builder->vertex_buffers = header->vertex_buffers;
	builder->vertex_buffer_count = header->vertex_buffer_count;
	builder->index_buffers = header->index_buffers;
	builder->index_buffer_count = header->index_buffer_count;
	/* (what the validator checked, checked again: nothing below reads outside the tags) */
	if (header->signature != PACK_TAG_HEADER_SIGNATURE || builder->instance_count <= 0 ||
		builder->instance_count > 0xffff ||
		!donor_contains(builder, builder->instances,
			(unsigned long)builder->instance_count * sizeof(struct pack_tag_instance)) ||
		builder->vertex_buffer_count < 0 || builder->index_buffer_count < 0 ||
		(builder->vertex_buffer_count && !donor_contains(builder, builder->vertex_buffers,
			(unsigned long)builder->vertex_buffer_count * PACK_BUFFER_SIZE)) ||
		(builder->index_buffer_count && !donor_contains(builder, builder->index_buffers,
			(unsigned long)builder->index_buffer_count * PACK_BUFFER_SIZE)))
	{
		builder_refuse(builder, "%s's tag header is damaged", name);
		return FALSE;
	}

	return TRUE;
}

/* the donor's tag of the group and name (ASCII case aside): its tag index, or NONE */
static long donor_tag_find(
	struct builder *builder,
	unsigned long group_tag,
	char const *name)
{
	long index;

	for (index = 0; index < builder->instance_count; index++)
	{
		struct pack_tag_instance const *instance = &builder->instances[index];

		if (instance->group_tag == group_tag &&
			tag_name_length(builder->tags, builder->tags_size, instance->name) != NONE &&
			names_equal(instance->name, name))
		{
			return instance->tag_index;
		}
	}

	return NONE;
}

/* the pack's own bitmap of the donor's name, or NONE (what --smooth and --icon name) */
static long own_bitmap_find(
	struct builder *builder,
	char const *donor_name,
	char const *what)
{
	long index;

	for (index = 0; index < builder->tag_count; index++)
	{
		struct builder_tag const *tag = &builder->pack_tags[index];

		if (tag->group_tag == PACK_BITMAP_GROUP && names_equal(builder->instances[tag->donor_index].name, donor_name))
		{
			if (tag->kind != AE_PACK_TAG_OWN)
			{
				builder_refuse(builder, "%s: %s is a map's own bitmap (external), not the pack's", what, donor_name);
				return NONE;
			}
			return index;
		}
	}
	builder_refuse(builder, "%s: the pack has no bitmap %s", what, donor_name);

	return NONE;
}

static void builder_free(
	struct builder *builder)
{
	long index;

	for (index = 0; builder->pack_tags && index < builder->tag_count; index++)
	{
		struct builder_tag *tag = &builder->pack_tags[index];

		free(tag->name);
		free(tag->source_path);
		free(tag->bytes);
		free(tag->fixups);
		free(tag->parts);
		free(tag->resources);
	}
	free(builder->pack_tags);
	free(builder->pack_indices);
	free(builder->summary_tags);
	free(builder);
}

/* the parts in offset order (the writer takes them so; the walk meets them so, but this does not depend on it) */
static void parts_sort(
	struct builder_tag *tag)
{
	unsigned long index;

	for (index = 1; index < tag->part_count; index++)
	{
		struct ae_pack_build_part part = tag->parts[index];
		unsigned long other = index;

		while (other > 0 && tag->parts[other - 1].offset > part.offset)
		{
			tag->parts[other] = tag->parts[other - 1];
			other--;
		}
		tag->parts[other] = part;
	}
}

static void summary_fill(
	struct builder *builder,
	struct ae_pack_builder_summary *summary)
{
	long index;

	summary->tag_count = 0;
	summary->tags = NULL;
	builder->summary_tags = calloc((unsigned long)(builder->tag_count ? builder->tag_count : 1),
		sizeof(*builder->summary_tags));
	if (!builder->summary_tags)
		return;
	summary->tags = builder->summary_tags;
	for (index = 0; index < builder->tag_count; index++)
	{
		struct builder_tag const *tag = &builder->pack_tags[index];
		struct ae_pack_builder_tag *listed = &summary->tags[index];
		unsigned long part_index;

		listed->group = (unsigned int)tag->group_tag;
		listed->name = tag->name;
		listed->source_path = tag->source_path;
		listed->kind = tag->kind;
		listed->required = tag->required;
		listed->flags = tag->flags;
		listed->bytes = (unsigned int)tag->size;
		listed->fixups = (unsigned int)tag->fixup_count;
		listed->parts = (unsigned int)tag->part_count;
		listed->resources = (unsigned int)tag->resource_count;
		listed->resource_bytes = (unsigned int)tag->resource_bytes;
		if (tag->kind == AE_PACK_TAG_OWN)
			summary->own_count++;
		else
			summary->external_count++;
		summary->required_count += tag->required;
		summary->fixup_count += (unsigned int)tag->fixup_count;
		summary->part_count += (unsigned int)tag->part_count;
		summary->resource_count += (unsigned int)tag->resource_count;
		summary->tag_bytes += (unsigned int)tag->size;
		summary->resource_bytes += (unsigned int)tag->resource_bytes;
		for (part_index = 0; part_index < tag->part_count; part_index++)
			summary->geometry_bytes += tag->parts[part_index].vertices_size + tag->parts[part_index].indices_size;
		summary->tag_count++;
	}
}

/* ---------- public code */

int ae_pack_builder_build(
	struct ae_pack_builder_options const *options,
	char const *path,
	struct ae_pack_builder_summary *summary,
	struct ae_pack_problem *problem)
{
	struct builder *builder = calloc(1, sizeof(*builder));
	struct ae_pack_build_tag *build_tags = NULL;
	struct ae_pack_build_data data;
	struct ae_pack_build_weapon weapon;
	long weapon_tag;
	long index;
	int written = 0;
	char text[5];

	memset(summary, 0, sizeof(*summary));
	summary->item_collection_tag = AE_PACK_NONE;
	summary->icon_tag = AE_PACK_NONE;
	problem->text[0] = 0;
	if (!builder)
	{
		snprintf(problem->text, sizeof(problem->text), "out of memory");
		return 0;
	}
	builder->options = options;
	builder->problem = problem;
	summary->memory = builder;

	if (!donor_tags_read(builder, options->tag_data, options->tag_data_size, options->file_size,
		options->donor_name, TRUE))
	{
		goto done;
	}
	summary->corrections = (unsigned int)tag_validate_corrections();
	builder->pack_indices = malloc((unsigned long)builder->instance_count * sizeof(long));
	builder->pack_tags = calloc(AE_PACK_MAXIMUM_TAGS, sizeof(struct builder_tag));
	if (!builder->pack_indices || !builder->pack_tags)
	{
		builder_refuse(builder, "out of memory");
		goto done;
	}
	for (index = 0; index < builder->instance_count; index++)
		builder->pack_indices[index] = NONE;

	/* the roots: the weapon, then the others, in order (no weapon: a pack of tags only, its roots') */
	if (!options->weapon_name && !options->root_count)
	{
		builder_refuse(builder, "a pack of tags only needs a root");
		goto done;
	}
	if (options->weapon_name)
	{
		weapon_tag = donor_tag_find(builder, PACK_WEAPON_GROUP, options->weapon_name);
		if (weapon_tag == NONE)
		{
			builder_refuse(builder, "the donor has no weapon named %s", options->weapon_name);
			goto done;
		}
		if (pack_tag_of(builder, weapon_tag) != 0 || builder->pack_tags[0].kind != AE_PACK_TAG_OWN)
		{
			builder_refuse(builder, "the weapon %s is a map's own tag (in the external names)", options->weapon_name);
			goto done;
		}
	}
	for (index = 0; index < (long)options->root_count && !builder->failed; index++)
	{
		struct ae_pack_builder_name const *root = &options->roots[index];
		long root_tag = donor_tag_find(builder, root->group, root->name);
		long pack_index;

		if (root_tag == NONE)
		{
			builder_refuse(builder, "the donor has no %s %s", group_text(root->group, text), root->name);
			break;
		}
		pack_index = pack_tag_of(builder, root_tag);
		if (pack_index == NONE)
			break;
		if (builder->pack_tags[pack_index].kind != AE_PACK_TAG_OWN)
		{
			builder_refuse(builder, "the root %s is a map's own tag (in the external names)", root->name);
			break;
		}
		if (root->group == PACK_ITEM_COLLECTION_GROUP && summary->item_collection_tag == AE_PACK_NONE)
			summary->item_collection_tag = (unsigned int)pack_index;
	}

	/* each own tag the roots lead to (the list grows as they are copied) */
	for (index = 0; index < builder->tag_count && !builder->failed; index++)
	{
		struct builder_tag *tag = &builder->pack_tags[index];
		struct pack_tag_instance const *instance = &builder->instances[tag->donor_index];
		struct tag_schema_group const *group;
		byte const *root;

		if (tag->kind != AE_PACK_TAG_OWN)
			continue;
		group = schema_group_get(tag->group_tag);
		root = instance->base_address;
		if (!group || !group->definition)
		{
			builder_refuse(builder, "the roots lead to a tag of a group the pack cannot copy (no schema): %s %s",
				group_text(tag->group_tag, text), tag->source_path);
			break;
		}
		if (!donor_contains(builder, root, (unsigned long)group->definition->size))
		{
			builder_refuse(builder, "a tag outside the donor's tags: %s", tag->source_path);
			break;
		}
		builder->current_root = root;
		if (tag_bytes_append(builder, tag, root, (unsigned long)group->definition->size) != 0)
		{
			builder_refuse(builder, "a tag could not be copied: %s", tag->source_path);
			break;
		}
		pack_element(builder, index, root, 0, group->definition, 0);
		tag = &builder->pack_tags[index];
		/* (a bitmap's pixel data is in the file, never in the tags: its address is none) */
		if (!builder->failed && tag->group_tag == PACK_BITMAP_GROUP)
		{
			struct bitmap_group *copy = (struct bitmap_group *)tag->bytes;

			copy->pixel_data.address = XBOX_NULL;
			copy->pixel_data.definition = XBOX_NULL;
		}
		parts_sort(tag);
	}
	if (builder->failed)
		goto done;

	/* no two tags of a group named alike (the engine finds tags by name, ASCII case aside) */
	for (index = 0; index < builder->tag_count && !builder->failed; index++)
	{
		long other;

		for (other = index + 1; other < builder->tag_count; other++)
		{
			if (builder->pack_tags[index].group_tag == builder->pack_tags[other].group_tag &&
				names_equal(builder->pack_tags[index].name, builder->pack_tags[other].name))
			{
				builder_refuse(builder, "%s and %s would have the same name in the pack (%s)",
					builder->instances[builder->pack_tags[index].donor_index].name,
					builder->instances[builder->pack_tags[other].donor_index].name, builder->pack_tags[index].name);
				break;
			}
		}
	}

	/* R9: the named own bitmaps' flags */
	for (index = 0; index < (long)options->flag_count && !builder->failed; index++)
	{
		long bitmap = own_bitmap_find(builder, options->flags[index].name, "--smooth");

		if (bitmap != NONE)
			builder->pack_tags[bitmap].flags |= (unsigned char)options->flags[index].flags;
	}

	/* R10: the icon, a sequence with sprites (a sheet drawn whole, the counter's, would cover the screen) */
	if (options->icon_name && !builder->failed)
	{
		long bitmap = own_bitmap_find(builder, options->icon_name, "--icon");

		if (bitmap != NONE)
		{
			struct bitmap_group const *group =
				builder->instances[builder->pack_tags[bitmap].donor_index].base_address;
			struct bitmap_group_sequence const *sequences = (struct bitmap_group_sequence const *)group->sequences.address;

			if (group->sequences.count <= 0 || options->icon_sequence >= (unsigned long)group->sequences.count ||
				!donor_contains(builder, sequences, (unsigned long)group->sequences.count * sizeof(*sequences)))
			{
				builder_refuse(builder, "--icon: %s has no sequence %u", options->icon_name, options->icon_sequence);
			}
			else if (sequences[options->icon_sequence].sprites.count <= 0)
			{
				builder_refuse(builder, "--icon: sequence %u of %s has no sprites (an icon is a sprite of a sheet)",
					options->icon_sequence, options->icon_name);
			}
			else
			{
				summary->icon_tag = (unsigned int)bitmap;
			}
		}
	}
	if (builder->failed)
		goto done;

	/* the pack, through the writer (which checks it as the reader does) */
	build_tags = calloc((unsigned long)builder->tag_count, sizeof(*build_tags));
	if (!build_tags)
	{
		builder_refuse(builder, "out of memory");
		goto done;
	}
	for (index = 0; index < builder->tag_count; index++)
	{
		struct builder_tag const *tag = &builder->pack_tags[index];
		struct ae_pack_build_tag *build_tag = &build_tags[index];

		build_tag->group = (unsigned int)tag->group_tag;
		build_tag->parent_groups[0] = (unsigned int)tag->parent_group_tags[0];
		build_tag->parent_groups[1] = (unsigned int)tag->parent_group_tags[1];
		build_tag->name = tag->name;
		build_tag->source_path = tag->source_path;
		build_tag->kind = tag->kind;
		build_tag->required = tag->required;
		build_tag->flags = tag->flags;
		build_tag->bytes = tag->bytes;
		build_tag->bytes_size = (unsigned int)tag->size;
		build_tag->fixups = tag->fixups;
		build_tag->fixup_count = (unsigned int)tag->fixup_count;
		build_tag->parts = tag->parts;
		build_tag->part_count = (unsigned int)tag->part_count;
		build_tag->resources = tag->resources;
		build_tag->resource_count = (unsigned int)tag->resource_count;
	}
	memset(&weapon, 0, sizeof(weapon));
	weapon.key = options->weapon_key;
	weapon.root_tag = 0;
	weapon.item_collection_tag = summary->item_collection_tag;
	weapon.behaviour = options->behaviour;
	weapon.label = options->label;
	weapon.fallback_labels[0] = options->fallback_labels[0];
	weapon.fallback_labels[1] = options->fallback_labels[1];
	weapon.menu_name = options->menu_name;
	weapon.weapon_pickup_text = options->weapon_pickup_text;
	weapon.ammo_pickup_text = options->ammo_pickup_text;
	weapon.icon_bitmap_tag = summary->icon_tag;
	weapon.icon_sequence = summary->icon_tag == AE_PACK_NONE ? 0 : options->icon_sequence;
	weapon.replaces = options->replaces;
	weapon.power = options->power;
	memset(&data, 0, sizeof(data));
	data.pack_id = options->pack_id;
	data.pack_revision = options->pack_revision;
	data.display_name = options->display_name;
	data.tags = build_tags;
	data.tag_count = (unsigned int)builder->tag_count;
	data.weapons = options->weapon_name ? &weapon : NULL;
	data.weapon_count = options->weapon_name ? 1 : 0;
	data.credits = options->credits;
	written = ae_pack_write(path, &data, problem);
	if (!written)
		builder->failed = TRUE;

done:
	free(build_tags);
	summary_fill(builder, summary);

	return written;
}

void ae_pack_builder_summary_free(
	struct ae_pack_builder_summary *summary)
{
	if (summary->memory)
		builder_free(summary->memory);
	memset(summary, 0, sizeof(*summary));
}

int ae_pack_builder_names(
	void *tag_data,
	unsigned int tag_data_size,
	unsigned int file_size,
	char const *map_name,
	void (*name)(void *context, unsigned int group, char const *name),
	void *context,
	struct ae_pack_problem *problem)
{
	struct builder builder;
	long index;

	memset(&builder, 0, sizeof(builder));
	builder.problem = problem;
	problem->text[0] = 0;
	if (!donor_tags_read(&builder, tag_data, tag_data_size, file_size, map_name, FALSE))
		return 0;
	for (index = 0; index < builder.instance_count; index++)
	{
		struct pack_tag_instance const *instance = &builder.instances[index];

		if (tag_name_length(builder.tags, builder.tags_size, instance->name) == NONE)
		{
			char text[5];

			builder_refuse(&builder, "%s: tag %u of group %s has no name a pack can carry", map_name, (unsigned int)index,
				group_text(instance->group_tag, text));
			return 0;
		}
		name(context, (unsigned int)instance->group_tag, instance->name);
	}

	return 1;
}
