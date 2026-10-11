/*
AE_WEAPON_PACK.C

port: weapon pack. The packs (ae_weapon_pack.h), read at start through the
format's reader (ae_pack_format.c), which checks the file's containers and
its SHA-256, then every resource read once (ae_pack_verify_resources), so a
pack whose resources lie is refused here and not on a map. What a pack's
tags hold is checked once they are placed on a map (tag_validate.c), not
here: nothing is placed yet.

Slot 1's pack is packs\ae_battle_rifle.aepack in the data root (d:\), or the
file debug.weapon_pack_file names (R31: an Xbox path under d:\, never a
host path: a test pack, read in its place; its hash travels like any
pack's). What is at the path is asked before it is opened: only a file at
least a header long is. A slot takes only its own pack:
another pack id in its file is refused.

The spike's DONOR code (a map's weapon taken out of another map file at
each load: game.weapon_pack) is not carried over: packs are built by
the host tool (tools/ae_pack_build) and only read here.

THE PACK ON A MAP. When a multiplayer map's tags have loaded and been
checked (scenario_tags_load), and the game about to be played asks for a
pack (the pending game variant's weapon_pack: game_variant_global, which a
network game's host and clients, the test host and a split-screen game all
set just before the map loads; the menus' map and the campaign have none),
the slot's pack is ADDED to the map (ae_weapon_pack_tags_loaded):

1. Each EXTERNAL tag of the pack is found on the map by its group and name.
   A required one the map lacks refuses the pack for this map; an optional
   one becomes no tag.
2. Each OWN tag gets a new place past the end of the map's tag table (never
   the map's tag of the same name), in a grown copy of the table, as the
   menus' tags do (menu_tags.c). Its bytes are copied into memory of the
   pack's own, laid out as a map's tags are (a tag header, the table, the
   names, the tags), and its fixups applied: pointers to where the copy
   lies, references and tag indices to the tags they resolved to. Every
   offset and value is checked against the tag's bytes and the pack again
   here, whatever the reader checked.
3. THE TAGS ARE CHECKED AS A MAP'S ARE (ae_tag_validate.inc, the tag
   validator's own walk): every block, data, reference, index and enum of
   every own tag, and the checks between tags, against the real table (the
   map's tags and the pack's). A tag that fails refuses the pack for this
   map: nothing was added yet, and the game plays as with no pack.
4. What the schemas do not say is checked by hand: every model part the
   pack gives geometry for is a part of a model of the pack, and every run
   of pixels and samples its bitmaps and sounds read lies in the pack's
   resources. Model parts then get vertex and index buffers of their own.
5. Only then is the grown table made the game's.

A bitmap's pixels and a sound's samples are read from the pack's file when
the texture and sound caches ask for them (ae_weapon_pack_read, from
cache_file_read), bounded by the pack's resource records. They are not
hashed again after the start: a file changed under the game gives wrong
pixels or samples (or none: zeros), never a read outside the buffer.

Everything is let go as the map's tags are (ae_weapon_pack_tags_unloaded).
A refusal is one "weapon pack: refused for <map>: <reason>" line.
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "cseries_windows.h"
#include "errors.h"
#include "tag_files/tag_groups.h"
#include "bitmaps/bitmap_group.h"
#include "bitmaps/bitmaps.h"
#include "models/model_definitions.h"
#include "rasterizer/rasterizer_geometry.h"
#include "sound/sound_definitions.h"
#include "scenario/scenario_definitions.h"
#include "game/game_engine.h"
#include "game/game_globals.h"
#include "objects/object_definitions.h"
#include "items/weapon_definitions.h"
#include "models/model_animation_definitions.h"
#include "tag_schema.h"

#include "ae_pack_format.h"
#include "ae_weapon_pack.h"

/* ---------- constants */

#define PACKS_FOLDER "d:\\packs"

/* each slot's file in the packs folder, and the pack id it takes */
static struct
{
	char const *path;
	char const *pack_id;
} const slot_files[AE_WEAPON_PACK_SLOTS] =
{
	{ PACKS_FOLDER "\\ae_battle_rifle.aepack", "ae_battle_rifle" },
};

/* the pack on a map */
enum
{
	PLACED_TAG_HEADER_SIGNATURE = 0x74616773, /* 'tags' */
	/* a map's tags and the pack's own may be this many: a tag index's absolute index is 16 bits, short of
	NONE's, and the menus' tags come after these and need their room (menu_tags.c, instances_grow, refuses past
	UNSIGNED_SHORT_MAX) */
	PLACED_TABLE_TAGS_MAXIMUM = 0xC000,
	PLACED_SALT_MAXIMUM = 0xfffe,
	PLACED_ALIGNMENT = 16,
	PLACED_TABLE_OFFSET = 0x40,
	/* (schemas are 6 or 7 blocks deep; the validator walks 32) */
	PLACED_MAXIMUM_DEPTH = 32,
	/* what the validator takes for the "map file" the pack's bitmaps and sounds read: any offset a long holds;
	what they really read is held to the pack's resource records (resource_covers) */
	PLACED_FILE_LENGTH = 0x7fffffff,
	PLACED_FINGERPRINT_SIZE = AE_WEAPON_PACK_FINGERPRINT_SIZE,
	PLACED_STRUCTURE_BSP_GROUP = 0x73627370, /* 'sbsp' */
};

/* ---------- structures */

/* cache_files.c's struct cache_file_tag_instance */
struct placed_tag_instance
{
	unsigned long group_tag;
	unsigned long parent_group_tags[2];
	long tag_index;
	XPTR(char) name;
	XPTR(void) base_address;
	unsigned long unused[2];
};

/* cache_files.c's struct cache_file_tag_header */
struct placed_tag_header
{
	XPTR(struct placed_tag_instance) instances;
	long scenario_tag_index;
	unsigned long checksum;
	long tag_count;
	long vertex_buffer_count;
	XPTR(byte) vertex_buffers;
	long index_buffer_count;
	XPTR(byte) index_buffers;
	unsigned long signature;
};

/* models.c's struct model_geometry_part */
struct placed_model_part
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

typedef char verify_placed_tag_instance_size[sizeof(struct placed_tag_instance) == 0x20 ? 1 : -1];
typedef char verify_placed_tag_header_size[sizeof(struct placed_tag_header) == 0x24 ? 1 : -1];
typedef char verify_placed_model_part_size[sizeof(struct placed_model_part) == AE_PACK_MODEL_PART_SIZE ? 1 : -1];
typedef char verify_placed_reference_size[sizeof(struct tag_reference) == AE_PACK_TAG_REFERENCE_SIZE ? 1 : -1];

/* ---------- prototypes */

/* the platform layer's (port/linux/src) */
char const *config_string(char const *name);
/* cache_files.c's (port: the loaded tags' table, which a copy with more tags replaces, as the menus' do) */
void *cache_files_tag_instances(long *count);
void cache_files_set_tag_instances(void *instances, long count);
/* tag_validate.c's (ae_tag_validate.inc, which is part of it) */
long ae_tag_validate_claim_words(long region_size);
boolean ae_tag_validate_added_tags(void *region, long region_size, long first_index, long file_length,
	char const *name, unsigned long *claims_scratch, long *corrections);
/* game.c's: the variant of the game about to be played (game_set_game_variant), all zero for none */
extern struct game_variant game_variant_global;
/* rasterizer_xbox.c's: no device, no buffers (a dedicated server) */
extern void *global_d3d_device;
/* rasterizer_xbox_hardware_geometry.c's (in no header) */
boolean rasterizer_vertex_buffer_new(struct vertex_buffer *vertex_buffer, long vertex_type, long count,
	void const *vertices, long buffer_size);
void rasterizer_vertex_buffer_delete(struct vertex_buffer *vertex_buffer);
boolean rasterizer_triangle_buffer_new(struct triangle_buffer *triangle_buffer, short triangle_type, long count,
	void const *triangles);
void rasterizer_triangle_buffer_delete(struct triangle_buffer *triangle_buffer);

/* ---------- globals */

static struct
{
	boolean initialized;
	struct ae_pack *packs[AE_WEAPON_PACK_SLOTS];
} weapon_pack_globals;

/* the pack on the loaded map */
static struct
{
	boolean added;
	short slot;
	char map_name[64];
	/* the pack's own tags as a map's are laid out: a tag header, the grown table, the names, the tags */
	byte *arena;
	long arena_size;
	/* the model parts' vertices and indices (what their buffers were made from, and name as their data) */
	byte *geometry;
	/* each pack tag's tag on the map (NONE: an optional external tag the map lacks), its name there and, for
	an own tag, its bytes */
	long *tag_indices;
	char **tag_names;
	byte **tag_data;
	/* each PARTS record: whether it is a model part of its tag (model_part_found) */
	byte *part_found;
	boolean buffers_made;
	struct placed_tag_instance *original_instances;
	long original_count;
	long own_count;
	long missing_count;
	unsigned char fingerprint[PLACED_FINGERPRINT_SIZE];
	/* what cache_file_read was given from the pack, and what it could not be */
	long bitmap_reads;
	long sound_reads;
	long failed_reads;
	unsigned long read_bytes;
	boolean loadout_logged[2];
	char problem[200];
} placed;

/* ---------- private code */

static struct ae_pack *slot_pack(
	short slot)
{
	if (slot < 1 || slot > AE_WEAPON_PACK_SLOTS)
		return NULL;
	return weapon_pack_globals.packs[slot - 1];
}

enum
{
	_path_missing,
	_path_pack_file,
	_path_other,
};

/* what is at the path, asked without opening it: opening a named pipe would wait for a writer for ever, and a
pack is a download's name away from being anything. A folder, or anything smaller than a pack's header (a pipe,
a socket and a device all report no size), is no pack file */
static short path_kind(
	char const *path)
{
	WIN32_FILE_ATTRIBUTE_DATA data;

	if (!GetFileAttributesExA(path, GetFileExInfoStandard, &data))
		return _path_missing;
	if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
		(!data.nFileSizeHigh && data.nFileSizeLow < AE_PACK_HEADER_SIZE))
	{
		return _path_other;
	}
	return _path_pack_file;
}

/* whether a path names another drive than d: (the data root). Another letter is a folder of the save root, which
asking about makes (the platform layer's path translation): a test pack is never looked for there */
static boolean path_leaves_data_root(
	char const *path)
{
	return path[0] && path[1] == ':' && path[0] != 'd' && path[0] != 'D';
}

/* "weapon pack: 'ae_battle_rifle' revision N (sha256 0123456789abcdef...), <own> own tags, <ext> external,
<res> resources (<MB>), weapons: battle_rifle" */
static void pack_log(
	struct ae_pack const *pack)
{
	char hash[17];
	char weapons[AE_PACK_MAXIMUM_WEAPONS * (AE_PACK_MAXIMUM_KEY + 2) + 1];
	unsigned int index, tenths;

	for (index = 0; index < 8; index++)
		snprintf(hash + index * 2, 3, "%02x", pack->header.sha256[index]);
	weapons[0] = 0;
	for (index = 0; index < pack->weapon_count; index++)
	{
		char const *key = ae_pack_string(pack, pack->weapons[index].key);
		size_t used = strlen(weapons);

		snprintf(weapons + used, sizeof(weapons) - used, "%s%s", index ? ", " : "", key ? key : "?");
	}
	/* (at most 48 MiB: times ten stays well inside 32 bits) */
	tenths = (pack->resources_size * 10u + 524288u) / 1048576u;
	error(_error_silent, "weapon pack: '%s' revision %u (sha256 %s...), %u own tags, %u external, %u resources "
		"(%u.%u MB), weapons: %s",
		pack->header.pack_id, pack->header.pack_revision, hash, pack->own_tag_count,
		pack->tag_count - pack->own_tag_count, pack->resource_count, tenths / 10, tenths % 10, weapons);
}

static void pack_refused(
	short slot,
	char const *path,
	char const *reason)
{
	error(_error_silent, "weapon pack: slot %d (%s) refused: %s; the game plays without it", slot, path, reason);
}

/* the slot's pack read, checked and logged, or refused and logged; a slot with no file in a game with no packs
folder logs nothing */
static struct ae_pack *slot_read(
	short slot,
	char const *path,
	boolean asked_for)
{
	struct ae_pack *pack = NULL;
	struct ae_pack_problem problem;
	char const *pack_id = slot_files[slot - 1].pack_id;

	/* (R32: no packs folder is a game without packs, and silent; a packs folder without the slot's file is a
	refusal. With a second slot, a folder holding only some slots' files would log one each start: decide then) */
	if (!asked_for && path_kind(PACKS_FOLDER) == _path_missing)
		return NULL;
	if (asked_for && path_leaves_data_root(path))
	{
		pack_refused(slot, path, "a test pack's path starts d:\\");
		return NULL;
	}
	switch (path_kind(path))
	{
	case _path_missing:
		pack_refused(slot, path, "there is no such file");
		return NULL;
	case _path_other:
		pack_refused(slot, path, "it is no pack file (a folder, or smaller than a pack's header)");
		return NULL;
	}
	if (!ae_pack_open(path, &pack, &problem))
	{
		pack_refused(slot, path, problem.text);
		return NULL;
	}
	if (strcmp(pack->header.pack_id, pack_id) != 0)
	{
		char reason[96];

		snprintf(reason, sizeof(reason), "it is pack '%s', the slot takes '%s'", pack->header.pack_id, pack_id);
		pack_refused(slot, path, reason);
		ae_pack_close(pack);
		return NULL;
	}
	if (!ae_pack_verify_resources(pack, &problem))
	{
		pack_refused(slot, path, problem.text);
		ae_pack_close(pack);
		return NULL;
	}
	pack_log(pack);
	return pack;
}

/* ---------- private code: the pack on a map */

static char *group_text(
	unsigned int group_tag,
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

/* (the first reason is kept: the add stops there) */
static boolean placed_refuse(
	char const *format,
	char const *detail)
{
	if (!placed.problem[0])
		snprintf(placed.problem, sizeof(placed.problem), format, detail ? detail : "");

	return FALSE;
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

static void *placed_allocate(
	unsigned long size)
{
	void *block = malloc(size ? size : 1);

	if (block)
		memset(block, 0, size ? size : 1);
	else
		placed_refuse("no memory", NULL);

	return block;
}

/* the model parts' buffers let go, and everything the pack holds on the map */
static void placed_free(
	struct ae_pack const *pack)
{
	unsigned int index, part_index;

	if (placed.buffers_made && pack && placed.tag_data)
	{
		for (index = 0; index < pack->tag_count; index++)
		{
			struct ae_pack_tag_record const *tag = &pack->tags[index];

			if (!placed.tag_data[index])
				continue;
			for (part_index = 0; part_index < tag->parts_count; part_index++)
			{
				struct placed_model_part *part = (struct placed_model_part *)(placed.tag_data[index] +
					pack->parts[tag->parts_first + part_index].offset);

				rasterizer_triangle_buffer_delete(&part->triangle_buffer);
				rasterizer_vertex_buffer_delete(&part->vertex_buffer);
			}
		}
	}
	if (placed.arena)
		free(placed.arena);
	if (placed.geometry)
		free(placed.geometry);
	if (placed.tag_indices)
		free(placed.tag_indices);
	if (placed.tag_names)
		free(placed.tag_names);
	if (placed.tag_data)
		free(placed.tag_data);
	if (placed.part_found)
		free(placed.part_found);
	memset(&placed, 0, sizeof(placed));
}

/* whether size bytes at offset of the "map file" lie in one of the tag's resources: what a read of them is
given from (ae_pack_read_resource takes the same ranges) */
static boolean resource_covers(
	struct ae_pack const *pack,
	unsigned int pack_index,
	unsigned long long offset,
	unsigned long size)
{
	struct ae_pack_tag_record const *tag = &pack->tags[pack_index];
	unsigned int index;

	for (index = 0; index < tag->resources_count; index++)
	{
		struct ae_pack_resource_record const *resource = &pack->resources[tag->resources_first + index];

		if (offset >= resource->map_offset && offset - resource->map_offset < resource->size &&
			size <= resource->size - (offset - resource->map_offset))
		{
			return TRUE;
		}
	}

	return FALSE;
}

/* an element of an own tag (CHECKED by the validator: its blocks' counts and addresses can be trusted) and the
elements of its blocks, for what the schemas do not say:
- a model part: the PARTS record at its offset is found (the others are no parts, and get no buffers);
- data in the "map file" (a sound's samples) and a bitmap's pixels, where the texture cache reads them
  (texture_cache_bitmap_new: the group's offset added to the bitmap's, as many bytes as its size, format and
  mipmaps make): each must lie in one of the tag's resources.
FALSE (and the reason kept) if the pack cannot be added */
static boolean placed_element_check(
	struct ae_pack const *pack,
	unsigned int pack_index,
	byte *base,
	struct tag_schema_definition const *definition,
	short depth)
{
	struct ae_pack_tag_record const *tag = &pack->tags[pack_index];
	byte *root = placed.tag_data[pack_index];
	struct tag_schema_field const *field;

	if (depth >= PLACED_MAXIMUM_DEPTH)
		return placed_refuse("%s is deeper than a tag is", placed.tag_names[pack_index]);
	for (field = definition->fields; field->type != _tag_schema_terminator; field++)
	{
		short index;

		for (index = 0; index < field->count; index++)
		{
			byte *address = base + field->offset + index * field->size;

			switch (field->type)
			{
			case _tag_schema_struct:
				if (!placed_element_check(pack, pack_index, address, field->definition, depth + 1))
					return FALSE;
				break;

			case _tag_schema_block:
			{
				struct tag_block const *block = (struct tag_block const *)address;
				struct tag_schema_definition const *element_definition = field->definition;
				long element_index;

				for (element_index = 0; element_index < block->count; element_index++)
				{
					if (!placed_element_check(pack, pack_index,
						(byte *)xbox_pointer(block->address) + element_index * element_definition->size,
						element_definition, depth + 1))
					{
						return FALSE;
					}
				}
				break;
			}

			case _tag_schema_file_data:
			{
				struct tag_data const *data = (struct tag_data const *)address;

				if (data->size > 0 && (data->file_offset < 0 ||
					!resource_covers(pack, pack_index, (unsigned long long)data->file_offset, (unsigned long)data->size)))
				{
					return placed_refuse("%s reads bytes the pack does not hold", placed.tag_names[pack_index]);
				}
				break;
			}

			default:
				break;
			}
		}
	}

	if (!strcmp(definition->name, "model_geometry_part") && definition->size == (long)sizeof(struct placed_model_part))
	{
		unsigned int offset = (unsigned int)(base - root);
		unsigned int part_index;

		for (part_index = 0; part_index < tag->parts_count; part_index++)
		{
			if (pack->parts[tag->parts_first + part_index].offset == offset)
				placed.part_found[tag->parts_first + part_index] = TRUE;
		}
	}
	else if (!strcmp(definition->name, "bitmap_data") && definition->size == (long)sizeof(struct bitmap_data) &&
		tag->group == BITMAP_GROUP_TAG)
	{
		struct bitmap_data *bitmap = (struct bitmap_data *)base;
		struct bitmap_group const *group = (struct bitmap_group const *)root;
		long size;

		/* (what the game asks of every bitmap before it reads one: its signature, type, format, dimensions and
		mipmaps. The tag validator leaves the signature to the game, and bitmap_get_pixel_data_size asserts
		this) */
		if (!bitmap_verify(bitmap, FALSE))
			return placed_refuse("%s has a bitmap the game cannot read", placed.tag_names[pack_index]);
		size = bitmap_get_pixel_data_size(bitmap);
		if (bitmap->pixels_size > size)
			size = bitmap->pixels_size;
		if (size > 0 && (group->pixel_data.file_offset < 0 || bitmap->pixels_offset < 0 ||
			!resource_covers(pack, pack_index,
				(unsigned long long)group->pixel_data.file_offset + (unsigned long long)bitmap->pixels_offset,
				(unsigned long)size)))
		{
			return placed_refuse("%s reads pixels the pack does not hold", placed.tag_names[pack_index]);
		}
	}

	return TRUE;
}

/* where a string of length bytes (and its terminator) goes in the arena, copied */
static char *arena_string(
	unsigned long *cursor,
	char const *text)
{
	char *copy = (char *)placed.arena + *cursor;
	size_t length = strlen(text);

	memcpy(copy, text, length + 1);
	*cursor += (unsigned long)length + 1;

	return copy;
}

/* an own tag's fixups applied to its copy (each offset and value against the tag's bytes and the pack's tags
again): pointers to where the copy lies, references and tag indices to the tags they resolved to */
static boolean placed_fixups_apply(
	struct ae_pack const *pack,
	unsigned int pack_index)
{
	struct ae_pack_tag_record const *tag = &pack->tags[pack_index];
	byte *data = placed.tag_data[pack_index];
	unsigned int fixup_index;

	for (fixup_index = 0; fixup_index < tag->fixups_count; fixup_index++)
	{
		struct ae_pack_fixup_record const *fixup = &pack->fixups[tag->fixups_first + fixup_index];
		unsigned int width = fixup->kind == AE_PACK_FIXUP_REFERENCE ? AE_PACK_TAG_REFERENCE_SIZE : 4;
		byte *address = data + fixup->offset;

		if (tag->bytes_size < width || fixup->offset > tag->bytes_size - width)
			return placed_refuse("a fixup of %s lies past its tag", placed.tag_names[pack_index]);
		switch (fixup->kind)
		{
		case AE_PACK_FIXUP_POINTER:
			if (fixup->value >= tag->bytes_size)
				return placed_refuse("a pointer of %s leaves its tag", placed.tag_names[pack_index]);
			*(XPTR(void) *)address = XBOX_ADDRESS(data + fixup->value);
			break;

		case AE_PACK_FIXUP_REFERENCE:
		{
			struct tag_reference *reference = (struct tag_reference *)address;

			if (fixup->value != AE_PACK_NONE && fixup->value >= pack->tag_count)
				return placed_refuse("a reference of %s names no tag of the pack", placed.tag_names[pack_index]);
			/* (no tag: no name; the validator gives such a reference its empty one) */
			reference->index = fixup->value == AE_PACK_NONE ? NONE : placed.tag_indices[fixup->value];
			reference->name = reference->index == NONE ? XBOX_NULL : XBOX_ADDRESS(placed.tag_names[fixup->value]);
			reference->name_length = 0;
			break;
		}

		case AE_PACK_FIXUP_TAG_INDEX:
			if (fixup->value >= pack->tag_count)
				return placed_refuse("a tag index of %s names no tag of the pack", placed.tag_names[pack_index]);
			*(long *)address = placed.tag_indices[fixup->value];
			break;

		default:
			return placed_refuse("a fixup of %s is of no kind", placed.tag_names[pack_index]);
		}
	}

	return TRUE;
}

/* the model parts' vertices and indices copied (the buffers name them as their data), every PARTS record a
model part of its tag, of the size the rasterizer takes its type's vertices to be */
static boolean placed_geometry_copy(
	struct ae_pack const *pack)
{
	unsigned long size = 0;
	unsigned int index, part_index;

	for (index = 0; index < pack->tag_count; index++)
	{
		struct ae_pack_tag_record const *tag = &pack->tags[index];

		for (part_index = 0; part_index < tag->parts_count; part_index++)
		{
			struct ae_pack_part_record const *part = &pack->parts[tag->parts_first + part_index];

			if (!placed.tag_data[index] || tag->group != MODELS_GROUP_TAG || !placed.part_found[tag->parts_first + part_index])
				return placed_refuse("%s has geometry for what is no model part", placed.tag_names[index]);
			if (part->vertices_offset > pack->geometry_size || part->vertices_size > pack->geometry_size - part->vertices_offset ||
				part->indices_offset > pack->geometry_size || part->indices_size > pack->geometry_size - part->indices_offset ||
				(unsigned long)rasterizer_geometry_get_vertex_size((short)part->vertex_type) * part->vertex_count != part->vertices_size ||
				part->vertex_count < 1 || part->vertex_count > AE_PACK_MAXIMUM_PART_COUNT ||
				part->triangle_count < 1 || part->triangle_count > AE_PACK_MAXIMUM_PART_COUNT ||
				(part->triangle_type == AE_PACK_TRIANGLES ? 6 * part->triangle_count : 2 * part->triangle_count + 4) != part->indices_size ||
				(part->triangle_type != AE_PACK_TRIANGLES && part->triangle_type != AE_PACK_TRIANGLE_STRIP))
			{
				return placed_refuse("%s has a model part of sizes the game does not draw", placed.tag_names[index]);
			}
			size += ((unsigned long)part->vertices_size + PLACED_ALIGNMENT - 1) & ~(unsigned long)(PLACED_ALIGNMENT - 1);
			size += ((unsigned long)part->indices_size + PLACED_ALIGNMENT - 1) & ~(unsigned long)(PLACED_ALIGNMENT - 1);
			if (size > 2 * (unsigned long)AE_PACK_MAXIMUM_GEOMETRY)
				return placed_refuse("%s: the pack's geometry is more than a pack holds", placed.tag_names[index]);
		}
	}
	placed.geometry = placed_allocate(size);
	if (!placed.geometry)
		return FALSE;

	return TRUE;
}

/* the model parts' buffers, from the pack's vertices and indices (a fresh one each: the tags carry none). The
last step: nothing fails after it. A part whose buffers cannot be made is not drawn (the draw takes none); a
game with no device (a dedicated server) makes none */
static void placed_buffers_make(
	struct ae_pack const *pack)
{
	unsigned long cursor = 0;
	unsigned int index, part_index;
	long undrawn = 0;

	placed.buffers_made = TRUE;
	for (index = 0; index < pack->tag_count; index++)
	{
		struct ae_pack_tag_record const *tag = &pack->tags[index];

		for (part_index = 0; part_index < tag->parts_count; part_index++)
		{
			struct ae_pack_part_record const *source = &pack->parts[tag->parts_first + part_index];
			struct placed_model_part *part = (struct placed_model_part *)(placed.tag_data[index] + source->offset);
			byte *vertices = placed.geometry + cursor;
			byte *indices;

			cursor += ((unsigned long)source->vertices_size + PLACED_ALIGNMENT - 1) & ~(unsigned long)(PLACED_ALIGNMENT - 1);
			indices = placed.geometry + cursor;
			cursor += ((unsigned long)source->indices_size + PLACED_ALIGNMENT - 1) & ~(unsigned long)(PLACED_ALIGNMENT - 1);
			memcpy(vertices, pack->geometry + source->vertices_offset, source->vertices_size);
			memcpy(indices, pack->geometry + source->indices_offset, source->indices_size);
			memset(&part->vertex_buffer, 0, sizeof(part->vertex_buffer));
			memset(&part->triangle_buffer, 0, sizeof(part->triangle_buffer));
			if (!global_d3d_device)
				continue;
			if (!rasterizer_vertex_buffer_new(&part->vertex_buffer, (long)source->vertex_type, (long)source->vertex_count,
					vertices, (long)source->vertices_size) ||
				!rasterizer_triangle_buffer_new(&part->triangle_buffer, (short)source->triangle_type,
					(long)source->triangle_count, indices))
			{
				rasterizer_vertex_buffer_delete(&part->vertex_buffer);
				memset(&part->vertex_buffer, 0, sizeof(part->vertex_buffer));
				memset(&part->triangle_buffer, 0, sizeof(part->triangle_buffer));
				undrawn++;
			}
		}
	}
	if (undrawn)
		error(_error_silent, "weapon pack: %d model parts have no buffers: not drawn", (int)undrawn);
}

/* whether the graph has a weapon type of the label, in any seat and class (units.c,
unit_set_or_test_seat_and_weapon_label, which a unit holds a weapon by) */
static boolean graph_has_weapon_label(
	struct animation_graph *graph,
	char const *label)
{
	short seat_index, class_index, type_index;

	for (seat_index = 0; seat_index < graph->unit_seats.count; seat_index++)
	{
		struct animation_graph_unit_seat *seat =
			TAG_BLOCK_GET_ELEMENT(&graph->unit_seats, seat_index, struct animation_graph_unit_seat);

		for (class_index = 0; class_index < seat->weapon_classes.count; class_index++)
		{
			struct animation_graph_weapon_class *weapon_class =
				TAG_BLOCK_GET_ELEMENT(&seat->weapon_classes, class_index, struct animation_graph_weapon_class);

			for (type_index = 0; type_index < weapon_class->weapon_types.count; type_index++)
			{
				struct animation_graph_weapon_type *type =
					TAG_BLOCK_GET_ELEMENT(&weapon_class->weapon_types, type_index, struct animation_graph_weapon_type);

				if (!_stricmp(type->label, label))
					return TRUE;
			}
		}
	}

	return FALSE;
}

/* each of the pack's weapons held by a label the map's player has animations for, or no player could be given
it (unit_add_weapon_to_inventory refuses a weapon whose label the unit's graph lacks): the weapon entry's label,
else its fallback labels in order ("h_br", which only a map with that biped knows, then "ar", the assault
rifle's, which the stock Spartan holds a rifle by), written into the weapon's copy. The player is the map's
globals' multiplayer unit. Where none fits, the label stays as the pack has it and a line says so: what such a
map gets is the target-map checks' to decide (they come with the fallback rules), not this function's */
static void placed_weapon_labels(
	struct ae_pack const *pack)
{
	long globals_index = tag_loaded(GAME_GLOBALS_TAG, "globals\\globals");
	struct game_globals *globals = globals_index == NONE ? NULL : game_globals_definition_get(globals_index);
	struct game_globals_multiplayer_information *information = globals && globals->multiplayer_information.count > 0 ?
		TAG_BLOCK_GET_ELEMENT(&globals->multiplayer_information, 0, struct game_globals_multiplayer_information) : NULL;
	long unit_index = information ? information->unit.index : NONE;
	long graph_index = unit_index == NONE ? NONE :
		object_definition_get(unit_index)->object.animation_graph.index;
	struct animation_graph *graph = graph_index == NONE ? NULL : animation_graph_definition_get(graph_index);
	unsigned int index;

	for (index = 0; index < pack->weapon_count; index++)
	{
		struct ae_pack_weapon_record const *entry = &pack->weapons[index];
		struct weapon_definition *weapon = (struct weapon_definition *)placed.tag_data[entry->root_tag];
		char const *key = ae_pack_string(pack, entry->key);
		char const *labels[3];
		char const *chosen = NULL;
		short label_index;

		labels[0] = ae_pack_string(pack, entry->label);
		labels[1] = ae_pack_string(pack, entry->fallback_labels[0]);
		labels[2] = ae_pack_string(pack, entry->fallback_labels[1]);
		for (label_index = 0; graph && !chosen && label_index < 3; label_index++)
		{
			if (labels[label_index] && strlen(labels[label_index]) < sizeof(weapon->weapon.label) &&
				graph_has_weapon_label(graph, labels[label_index]))
			{
				chosen = labels[label_index];
			}
		}
		if (!chosen)
		{
			error(_error_silent, "weapon pack: the map's player has none of %s's labels: it cannot be held",
				key ? key : "?");
			continue;
		}
		memset(weapon->weapon.label, 0, sizeof(weapon->weapon.label));
		memcpy(weapon->weapon.label, chosen, strlen(chosen));
		error(_error_silent, "weapon pack: %s is held by the label '%s'", key ? key : "?", chosen);
	}
}

/* the pack added to the map whose tags have just loaded: FALSE (the reason in placed.problem, nothing of the
map changed, everything let go by the caller) if it cannot be */
static boolean placed_add(
	struct ae_pack *pack,
	char const *map_name)
{
	struct placed_tag_instance *instances, *table;
	struct placed_tag_header *header;
	long existing, salt = 0, next = 0, corrections = 0;
	unsigned long names_size = 0, bytes_size = 0, cursor, table_size;
	unsigned long *claims;
	unsigned int index;
	struct ae_pack_sha256 hash;
	unsigned char digest[AE_PACK_SHA256_SIZE];
	char check_name[64];
	char text[5];
	boolean valid;

	instances = cache_files_tag_instances(&existing);
	if (!instances || existing <= 0)
		return placed_refuse("the map has no tags", NULL);
	for (index = 0; index < (unsigned int)existing; index++)
	{
		long instance_salt = (long)((unsigned long)instances[index].tag_index >> 16);

		if (instance_salt > salt)
			salt = instance_salt;
	}
	if (existing + (long)pack->own_tag_count > PLACED_TABLE_TAGS_MAXIMUM ||
		salt + (long)pack->own_tag_count > PLACED_SALT_MAXIMUM)
	{
		return placed_refuse("tag table full", NULL);
	}

	placed.tag_indices = placed_allocate(pack->tag_count * sizeof(*placed.tag_indices));
	placed.tag_names = placed_allocate(pack->tag_count * sizeof(*placed.tag_names));
	placed.tag_data = placed_allocate(pack->tag_count * sizeof(*placed.tag_data));
	placed.part_found = placed_allocate(pack->part_count);
	if (!placed.tag_indices || !placed.tag_names || !placed.tag_data || !placed.part_found)
		return FALSE;

	/* each tag's tag on the map: an external one the map's of its group and name (a required one must be
	there), an own one a new place past the map's (never the map's tag of the same name) */
	for (index = 0; index < pack->tag_count; index++)
	{
		struct ae_pack_tag_record const *tag = &pack->tags[index];
		char const *name = ae_pack_string(pack, tag->name);

		if (!name || strlen(name) > AE_PACK_MAXIMUM_NAME)
			return placed_refuse("a tag of the pack has no name", NULL);
		names_size += (unsigned long)strlen(name) + 1;
		if (tag->kind == AE_PACK_TAG_EXTERNAL)
		{
			placed.tag_indices[index] = tag_loaded((long)tag->group, name);
			if (placed.tag_indices[index] != NONE)
				continue;
			if (tag->required)
			{
				snprintf(placed.problem, sizeof(placed.problem), "missing %s %s", group_text(tag->group, text), name);
				return FALSE;
			}
			placed.missing_count++;
		}
		else if (tag->kind == AE_PACK_TAG_OWN)
		{
			/* (a pack brings a weapon and what it leads to: never a scenario, its structure or the globals,
			which the game takes one of and a pack's builder refuses to carry) */
			if (tag->group == SCENARIO_TAG || tag->group == GAME_GLOBALS_TAG || tag->group == PLACED_STRUCTURE_BSP_GROUP)
				return placed_refuse("%s is a scenario's, a structure's or the globals' tag", name);
			if (tag->bytes_offset > pack->tag_bytes_size || tag->bytes_size > pack->tag_bytes_size - tag->bytes_offset ||
				tag->bytes_size > AE_PACK_MAXIMUM_TAG_SIZE)
			{
				return placed_refuse("%s lies past the pack's tags", name);
			}
			placed.tag_indices[index] = (long)(((unsigned long)(salt + 1 + next) << 16) | (unsigned long)(existing + next));
			next++;
			bytes_size += ((unsigned long)tag->bytes_size + PLACED_ALIGNMENT - 1) & ~(unsigned long)(PLACED_ALIGNMENT - 1);
		}
		else
			return placed_refuse("%s is of no kind", name);
	}
	if (!next || (unsigned int)next != pack->own_tag_count || bytes_size > AE_PACK_MAXIMUM_TAG_BYTES + PLACED_ALIGNMENT * (unsigned long)next)
		return placed_refuse("the pack's own tags are not as its header says", NULL);

	/* the pack's memory, laid out as a map's tags are: a tag header, the table (the map's tags, then the
	pack's), the names, the tags */
	table_size = (unsigned long)(existing + next) * sizeof(*table);
	names_size = (names_size + PLACED_ALIGNMENT - 1) & ~(unsigned long)(PLACED_ALIGNMENT - 1);
	placed.arena_size = (long)(PLACED_TABLE_OFFSET + table_size + names_size + bytes_size);
	placed.arena = placed_allocate((unsigned long)placed.arena_size);
	claims = placed_allocate((unsigned long)ae_tag_validate_claim_words(placed.arena_size) * sizeof(*claims));
	if (!placed.arena || !claims)
	{
		if (claims)
			free(claims);
		return FALSE;
	}
	header = (struct placed_tag_header *)placed.arena;
	table = (struct placed_tag_instance *)(placed.arena + PLACED_TABLE_OFFSET);
	header->instances = XBOX_ADDRESS(table);
	header->scenario_tag_index = NONE;
	header->tag_count = existing + next;
	header->signature = PLACED_TAG_HEADER_SIGNATURE;
	memcpy(table, instances, (size_t)existing * sizeof(*table));
	cursor = PLACED_TABLE_OFFSET + table_size;
	for (index = 0; index < pack->tag_count; index++)
		placed.tag_names[index] = arena_string(&cursor, ae_pack_string(pack, pack->tags[index].name));
	cursor = PLACED_TABLE_OFFSET + table_size + names_size;
	for (index = 0; index < pack->tag_count; index++)
	{
		struct ae_pack_tag_record const *tag = &pack->tags[index];
		struct placed_tag_instance *instance;

		if (tag->kind != AE_PACK_TAG_OWN)
			continue;
		placed.tag_data[index] = placed.arena + cursor;
		memcpy(placed.tag_data[index], pack->tag_bytes + tag->bytes_offset, tag->bytes_size);
		cursor += ((unsigned long)tag->bytes_size + PLACED_ALIGNMENT - 1) & ~(unsigned long)(PLACED_ALIGNMENT - 1);
		instance = &table[placed.tag_indices[index] & 0xffff];
		instance->group_tag = tag->group;
		instance->parent_group_tags[0] = tag->parent_groups[0];
		instance->parent_group_tags[1] = tag->parent_groups[1];
		instance->tag_index = placed.tag_indices[index];
		instance->name = XBOX_ADDRESS(placed.tag_names[index]);
		instance->base_address = XBOX_ADDRESS(placed.tag_data[index]);
	}
	valid = TRUE;
	for (index = 0; valid && index < pack->tag_count; index++)
	{
		if (placed.tag_data[index])
			valid = placed_fixups_apply(pack, index);
	}

	/* every own tag checked as a map's tags are, against the map's tags and the pack's, before the game (or
	anything below) reads one */
	if (valid)
	{
		snprintf(check_name, sizeof(check_name), "%.31s (weapon pack) on %.20s", pack->header.pack_id, map_name);
		valid = ae_tag_validate_added_tags(placed.arena, placed.arena_size, existing, PLACED_FILE_LENGTH, check_name,
			claims, &corrections);
		if (!valid)
			placed_refuse("its tags failed the tag check (the line before this one says which)", NULL);
	}
	free(claims);
	if (!valid)
		return FALSE;

	/* what the schemas do not say */
	for (index = 0; index < pack->tag_count; index++)
	{
		struct tag_schema_group const *group = schema_group_get(pack->tags[index].group);

		if (!placed.tag_data[index])
			continue;
		if (!group || !group->definition ||
			!placed_element_check(pack, index, placed.tag_data[index], group->definition, 0))
		{
			return placed_refuse("%s cannot be checked", placed.tag_names[index]);
		}
	}
	for (index = 0; index < pack->weapon_count; index++)
	{
		struct ae_pack_weapon_record const *weapon = &pack->weapons[index];

		if (weapon->root_tag >= pack->tag_count || !placed.tag_data[weapon->root_tag] ||
			pack->tags[weapon->root_tag].group != AE_PACK_GROUP_WEAPON)
		{
			return placed_refuse("a weapon of the pack is no weapon tag", NULL);
		}
	}
	if (!placed_geometry_copy(pack))
		return FALSE;

	/* what every machine must agree on to play this pack on this map: the pack, the map, and where each of
	the pack's tags is */
	ae_pack_sha256_begin(&hash);
	ae_pack_sha256_add(&hash, pack->header.sha256, AE_PACK_SHA256_SIZE);
	ae_pack_sha256_add(&hash, map_name, (unsigned int)strlen(map_name) + 1);
	{
		unsigned int count = (unsigned int)existing;

		ae_pack_sha256_add(&hash, &count, sizeof(count));
		for (index = 0; index < pack->tag_count; index++)
		{
			unsigned int tag_index = (unsigned int)placed.tag_indices[index];

			ae_pack_sha256_add(&hash, &tag_index, sizeof(tag_index));
		}
	}
	ae_pack_sha256_end(&hash, digest);
	memcpy(placed.fingerprint, digest, sizeof(placed.fingerprint));

	placed_weapon_labels(pack);
	placed_buffers_make(pack);
	placed.original_instances = instances;
	placed.original_count = existing;
	placed.own_count = next;
	cache_files_set_tag_instances(table, existing + next);
	{
		char fingerprint[2 * PLACED_FINGERPRINT_SIZE + 1];

		for (index = 0; index < PLACED_FINGERPRINT_SIZE; index++)
			snprintf(fingerprint + index * 2, 3, "%02x", placed.fingerprint[index]);
		error(_error_silent, "weapon pack: added to %s: %d tags new (%u bytes), %u the map's own, %d optional ones "
			"the map lacks, %d corrections; its tag table %d to %d; fingerprint %s",
			map_name, (int)next, (unsigned int)bytes_size,
			pack->tag_count - pack->own_tag_count - (unsigned int)placed.missing_count, (int)placed.missing_count,
			(int)corrections, (int)existing, (int)(existing + next), fingerprint);
	}

	return TRUE;
}

/* ---------- public code */

void ae_weapon_pack_initialize(
	void)
{
	char const *debug_file;
	short slot;

	if (weapon_pack_globals.initialized)
		return;
	weapon_pack_globals.initialized = TRUE;
	debug_file = config_string("debug.weapon_pack_file");
	for (slot = 1; slot <= AE_WEAPON_PACK_SLOTS; slot++)
	{
		boolean debug = slot == 1 && debug_file && debug_file[0];

		weapon_pack_globals.packs[slot - 1] = slot_read(slot, debug ? debug_file : slot_files[slot - 1].path, debug);
	}
}

void ae_weapon_pack_dispose(
	void)
{
	short slot;

	/* (a map's tags are let go before the game is: scenario_tags_unload. Were they not, the pack's tags stay
	as they are, in memory that is not given back, and no more is read from the pack: said, as it should not be) */
	if (placed.added)
		error(_error_silent, "weapon pack: the game ends with the pack still on %s: its tags were never let go",
			placed.map_name);
	placed.added = FALSE;
	for (slot = 1; slot <= AE_WEAPON_PACK_SLOTS; slot++)
	{
		ae_pack_close(weapon_pack_globals.packs[slot - 1]);
		weapon_pack_globals.packs[slot - 1] = NULL;
	}
	weapon_pack_globals.initialized = FALSE;
}

boolean ae_weapon_pack_slot_loaded(
	short slot)
{
	return slot_pack(slot) != NULL;
}

boolean ae_weapon_pack_identity(
	short slot,
	unsigned int *revision,
	unsigned char *sha256)
{
	struct ae_pack const *pack = slot_pack(slot);

	if (!pack)
		return FALSE;
	if (revision)
		*revision = pack->header.pack_revision;
	if (sha256)
		memcpy(sha256, pack->header.sha256, AE_WEAPON_PACK_SHA256_SIZE);
	return TRUE;
}

short ae_weapon_pack_weapon_count(
	short slot)
{
	struct ae_pack const *pack = slot_pack(slot);

	return pack ? (short)pack->weapon_count : 0;
}

struct ae_pack_weapon_record const *ae_weapon_pack_weapon_entry(
	short slot,
	short weapon)
{
	struct ae_pack const *pack = slot_pack(slot);

	if (!pack || weapon < 0 || (unsigned int)weapon >= pack->weapon_count)
		return NULL;
	return &pack->weapons[weapon];
}

char const *ae_weapon_pack_string(
	short slot,
	unsigned int offset)
{
	struct ae_pack const *pack = slot_pack(slot);

	return pack ? ae_pack_string(pack, offset) : NULL;
}

/* once a map's tags are read and checked (scenario_tags_load), before anything else adds to its tag table: the
pack the game about to be played asks for is added to a multiplayer map. A map loaded for no game (the menus',
the campaign: no pending game variant) gets none */
void ae_weapon_pack_tags_loaded(
	char const *map_name,
	long scenario_tag_index)
{
	struct game_variant const *variant = &game_variant_global;
	struct ae_pack *pack;
	/* (the map's header's name: 32 bytes, its terminator the header's to keep) */
	char name[33];
	short slot;

	if (placed.added || !variant->game_engine_index || !map_name)
		return;
	snprintf(name, sizeof(name), "%.32s", map_name);
	map_name = name;
	slot = game_variant_weapon_pack(variant->universal_variant.weapon_pack);
	if (slot == _weapon_pack_none)
		return;
	if (scenario_tag_index == NONE || scenario_definition_get(scenario_tag_index)->type != _scenario_type_multiplayer)
		return;
	pack = slot_pack(slot);
	memset(&placed, 0, sizeof(placed));
	if (!pack)
	{
		error(_error_silent, "weapon pack: not added to %s: this game has no pack for slot %d", map_name, (int)slot);
		return;
	}
	if (!placed_add(pack, map_name))
	{
		char problem[sizeof(placed.problem)];

		snprintf(problem, sizeof(problem), "%s", placed.problem);
		placed_free(pack);
		error(_error_silent, "weapon pack: refused for %s: %s; the game plays without it", map_name, problem);
		return;
	}
	placed.added = TRUE;
	placed.slot = slot;
	snprintf(placed.map_name, sizeof(placed.map_name), "%s", map_name);
}

/* as a map's tags are let go (scenario_tags_unload: after the texture and sound caches closed and the menus'
tags went, so the table is the pack's grown one again; before the map's file closes) */
void ae_weapon_pack_tags_unloaded(
	void)
{
	if (!placed.added)
		return;
	error(_error_silent, "weapon pack: let go from %s: %d tags; %d bitmap and %d sound reads (%u bytes) were served "
		"from the pack, %d could not be", placed.map_name, (int)placed.own_count, (int)placed.bitmap_reads,
		(int)placed.sound_reads, (unsigned int)placed.read_bytes, (int)placed.failed_reads);
	cache_files_set_tag_instances(placed.original_instances, placed.original_count);
	placed_free(slot_pack(placed.slot));
}

/* a read of the map file for tag tag_index (cache_file_read: a bitmap's pixels, a sound's samples): TRUE if the
tag is one of the pack's own on this map, and then buffer has its size bytes from the pack's file, read now (zeros
for any the pack does not hold or its file no longer gives) and the read is complete */
boolean ae_weapon_pack_read(
	long tag_index,
	long offset,
	long size,
	void *buffer,
	boolean *complete)
{
	struct ae_pack *pack;
	long absolute_index = tag_index & 0xffff;
	unsigned int index;

	if (!placed.added || tag_index == NONE || !buffer || !complete || size < 0 || offset < 0)
		return FALSE;
	/* (a pack tag's absolute index is past the map's own) */
	if (absolute_index < placed.original_count || absolute_index >= placed.original_count + placed.own_count)
		return FALSE;
	pack = slot_pack(placed.slot);
	for (index = 0; pack && index < pack->tag_count; index++)
	{
		if (!placed.tag_data[index] || placed.tag_indices[index] != tag_index)
			continue;
		if (size && !ae_pack_read_resource(pack, index, (unsigned int)offset, (unsigned int)size, buffer))
		{
			memset(buffer, 0, (size_t)size);
			if (!placed.failed_reads++)
			{
				error(_error_silent, "weapon pack: %s: %d bytes at %08x could not be read from the pack: zeros",
					placed.tag_names[index], (int)size, (unsigned int)offset);
			}
		}
		else
		{
			if (pack->tags[index].group == SOUND_DEFINITION_TAG)
				placed.sound_reads++;
			else
				placed.bitmap_reads++;
			placed.read_bytes += (unsigned long)size;
		}
		*complete = TRUE;
		return TRUE;
	}

	return FALSE;
}

short ae_weapon_pack_added(
	void)
{
	return placed.added ? placed.slot : 0;
}

boolean ae_weapon_pack_fingerprint(
	unsigned char *fingerprint)
{
	if (!placed.added)
		return FALSE;
	memcpy(fingerprint, placed.fingerprint, PLACED_FINGERPRINT_SIZE);

	return TRUE;
}

long ae_weapon_pack_weapon_definition(
	short slot,
	short weapon)
{
	struct ae_pack const *pack = slot_pack(slot);

	if (!placed.added || placed.slot != slot || !pack || weapon < 0 || (unsigned int)weapon >= pack->weapon_count)
		return NONE;

	return placed.tag_indices[pack->weapons[weapon].root_tag];
}

void ae_weapon_pack_loadout_given(
	short loadout_slot,
	long definition_index)
{
	struct ae_pack const *pack = slot_pack(placed.slot);
	char const *key;
	unsigned int index;

	if (!placed.added || !pack || loadout_slot < 0 || loadout_slot > 1 || placed.loadout_logged[loadout_slot])
		return;
	for (index = 0; index < pack->weapon_count; index++)
	{
		if (placed.tag_indices[pack->weapons[index].root_tag] != definition_index)
			continue;
		placed.loadout_logged[loadout_slot] = TRUE;
		key = ae_pack_string(pack, pack->weapons[index].key);
		error(_error_silent, "weapon pack: first spawn: the %s weapon is the pack's %s (%s, tag %08x)",
			loadout_slot ? "secondary" : "primary", key ? key : "?", placed.tag_names[pack->weapons[index].root_tag],
			(unsigned int)definition_index);
		return;
	}
}
