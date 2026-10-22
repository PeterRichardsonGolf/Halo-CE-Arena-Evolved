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
5. THE CHECKS AGAINST THE TARGET MAP (each marked CHECK below; a failure is
   the refusal line with its reason): the map's build is one the engine
   plays ("build <b>"); the table has room ("tag table full"); no own tag's
   group and name is already the map's ("name collision"); every required
   external is there ("missing <group> <name>"; optional ones become no tag,
   one line); the pack's tags and geometry fit PLACED_MEMORY_MAXIMUM and
   their buffers are made ("no memory"); the map's player holds the weapon
   by one of its labels in every seat it holds weapons in ("the player
   cannot hold it"); the weapon's first-person animations move the map's
   hands and the weapon's model ("first-person arms do not match"). A map's
   own battle rifle (CE+ X's) is left alone and logged. A Custom Edition
   map (or Halo PC's, HaloMD's) never gets the pack (R34: its branch of
   scenario_tags_load returns before the add; ae_weapon_pack_tags_refused_ce
   says so).
6. Only then is the grown table made the game's.

THE FALLBACK (decision 10). A map the pack was not added to plays the stock
weapons: ae_weapon_pack_added() is 0 and ae_weapon_pack_weapon_definition()
NONE, so the loadout keeps its stock value (an assault rifle, a pistol) and
BATTLE RIFLE ON MAP replaces nothing. At game start every machine's console
says why (ae_weapon_pack_unavailable_line); the host's PC lobby says so
before the game where it already knows (ae_weapon_pack_lobby_line: the
map's family, no pack here, an earlier refusal for that map).

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
#include "interface/hud_definitions.h"
#include "interface/weapon_hud_interface_definition.h"
#include "models/model_animation_definitions.h"
#include "tag_schema.h"
#include "halo_map_families.h"

#include "ae_pack_format.h"
#include "ae_weapon_pack.h"
#include "ae_weapon_slots.h"
#include "ae_pack_spawns.h"

/* ---------- constants */

#define PACKS_FOLDER "d:\\packs"

/* the failed resource reads a map logs, each on its own line (the "let go" line counts them all) */
#define FAILED_READS_LOGGED 32

/* the item message strings of the packs' weapons (hud_get_item_string): slot s's weapon n's pickup text is this plus
ITEM_STRINGS_PER_SLOT (s - 1) plus 2n, its ammo text the one after: far past any map's own list (the stock maps'
hold a few dozen; the prototype's number) */
#define ITEM_STRING_INDEX_FIRST 0x7F00
#define ITEM_STRINGS_PER_SLOT (2 * AE_PACK_MAXIMUM_WEAPONS)

/* a weapon HUD interface tag's group (hud.c's weapon_hud_interface_definition_get) */
#define WEAPON_HUD_INTERFACE_TAG 0x77706869 /* 'wphi' */

/* each slot's file in the packs folder, and the pack id it takes (ae_weapon_slots.h's slots, in their order) */
static struct
{
	char const *path;
	char const *pack_id;
} const slot_files[AE_WEAPON_PACK_SLOTS] =
{
	{ PACKS_FOLDER "\\ae_battle_rifle.aepack", "ae_battle_rifle" },
	{ PACKS_FOLDER "\\ae_smg.aepack", "ae_smg" },
	{ PACKS_FOLDER "\\ae_smg_silenced.aepack", "ae_smg_silenced" },
	{ PACKS_FOLDER "\\ae_carbine.aepack", "ae_carbine" },
	{ PACKS_FOLDER "\\ae_ar_silenced.aepack", "ae_ar_silenced" },
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
	/* the memory a pack may take on a map: its own tags' bytes and its model parts' vertices and indices */
	PLACED_MEMORY_MAXIMUM = 8 * 1024 * 1024,
	/* maps an earlier refusal is remembered for (the lobby's line) */
	REFUSED_MAPS_MAXIMUM = 16,
	MAP_NAME_SIZE = 33,
	REASON_SIZE = 200,
};

/* what a Custom Edition map (and Halo PC's, HaloMD's) is told (R34) */
#define NOT_ON_CUSTOM_EDITION "not on Custom Edition maps yet"

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

/* first_person_weapons.c's (and model_animations.c's) struct animation_graph_node: an animation graph's node,
its name first (tag_schema: animation_graph_node, 0x40 bytes) */
struct placed_animation_graph_node
{
	char name[TAG_STRING_LENGTH + 1];
	byte unused[0x20];
};

typedef char verify_slots[AE_WEAPON_PACK_SLOTS == AE_WEAPON_SLOT_COUNT ? 1 : -1];
typedef char verify_placed_animation_graph_node_size[sizeof(struct placed_animation_graph_node) == 0x40 ? 1 : -1];
typedef char verify_placed_tag_instance_size[sizeof(struct placed_tag_instance) == 0x20 ? 1 : -1];
typedef char verify_placed_tag_header_size[sizeof(struct placed_tag_header) == 0x24 ? 1 : -1];
typedef char verify_placed_model_part_size[sizeof(struct placed_model_part) == AE_PACK_MODEL_PART_SIZE ? 1 : -1];
typedef char verify_placed_reference_size[sizeof(struct tag_reference) == AE_PACK_TAG_REFERENCE_SIZE ? 1 : -1];

/* ---------- prototypes */

/* the platform layer's (port/linux/src) */
char const *config_string(char const *name);
int config_boolean(const char *name);
void platform_log(char const *format, ...);
long config_integer(char const *name);
/* (platform.h, xbox_memory.c: guest memory in the Xbox contiguous window, and a write into it that the renderer's
page protection does not stop; platform.h's BOOL is an int) */
int platform_is_contiguous(const void *address);
void platform_contiguous_write(void *destination, const void *source, size_t size);
/* cache_files.c's: the loaded map's build's region if the engine plays multiplayer on its maps (NTSC 2276, NHE's
2300, PAL 2342 through pal_tags.c, ...), else NULL; build gets the build */
char const *cache_files_multiplayer_region(char build[0x20]);
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

/* a pack on the loaded map (one a slot: placements below; `placed` is the one being added or asked) */
struct placed_state
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
	char problem[REASON_SIZE];
	/* the look of each of the pack's weapons on this map: its pickup texts (empty: the engine's own) and the
	item message index its tag had; the messaging icon of its HUD and the icon its entry names for it (bitmap
	NONE: none, or refused) */
	struct
	{
		wchar_t texts[2][AE_PACK_MAXIMUM_PICKUP_TEXT + 1];
		short message_index;
		boolean texts_set;
		boolean texts_shown[2];
		struct icon_hud_element_definition const *messaging_icon;
		long icon_bitmap;
		short icon_sequence;
		/* (the entry's icon refused for this map: the HUD's own is not drawn in its place either, no icon is) */
		boolean icon_refused;
		boolean icon_shown;
	} looks[AE_PACK_MAXIMUM_WEAPONS];
	/* (each SMOOTH bitmap tag's first upload on this map is logged) */
	byte smooth_logged[AE_PACK_MAXIMUM_TAGS];
};

/* each slot's pack on the loaded map, added in slot order (each grows the table the one before left) and let go
in reverse; `placed` is the slot's being added, let go or asked (placed_select) */
static struct placed_state placements[AE_WEAPON_PACK_SLOTS];
static struct placed_state *placed_current = &placements[0];
#define placed (*placed_current)

/* the last map a game asked each slot's pack for, and why it did not get it (the fallback's line at game start) */
struct refusal_state
{
	boolean asked;
	char map_name[MAP_NAME_SIZE];
	char reason[REASON_SIZE];
};
static struct refusal_state refusals[AE_WEAPON_PACK_SLOTS];
static struct refusal_state *refusal_current = &refusals[0];
#define refusal (*refusal_current)

/* the maps each slot's pack was refused for since the start, and why (the lobby's line): the packs and the maps do
not change while the game runs, so a map refused once is refused again */
struct refused_map
{
	char map_name[MAP_NAME_SIZE];
	char reason[REASON_SIZE];
};
static struct refused_map refused_maps_of[AE_WEAPON_PACK_SLOTS][REFUSED_MAPS_MAXIMUM];
static short refused_maps_next_of[AE_WEAPON_PACK_SLOTS];
static struct refused_map *refused_maps = refused_maps_of[0];
static short *refused_maps_next_current = &refused_maps_next_of[0];
#define refused_maps_next (*refused_maps_next_current)

/* `placed`, `refusal` and the refused maps made the slot's (1 to AE_WEAPON_PACK_SLOTS) */
static void placed_select(
	short slot)
{
	short index = slot >= 1 && slot <= AE_WEAPON_PACK_SLOTS ? (short)(slot - 1) : 0;

	placed_current = &placements[index];
	refusal_current = &refusals[index];
	refused_maps = refused_maps_of[index];
	refused_maps_next_current = &refused_maps_next_of[index];
}

/* the slot `placed` is now (what a public query selects back as it returns: it may be asked while a slot's pack is
being added, as placed_looks_set asks ae_weapon_pack_tag_is_own) */
static short placed_selected(
	void)
{
	return (short)(placed_current - placements + 1);
}

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

	/* (R32: no packs folder is a game without packs, and silent; a packs folder without slot 1's file is a
	refusal, without a later slot's one line, below) */
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
		/* (R32's later-slot case, decided 2026-10-21: a packs folder without a later slot's file is a machine without
		that weapon yet, said once and not a refusal; slot 1's missing file stays one) */
		if (slot > 1 && !asked_for)
		{
			error(_error_silent, "weapon pack: slot %d (%s): none here; its weapon plays as its stock fallback", slot,
				path);
			return NULL;
		}
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
	struct ae_pack const *pack,
	unsigned long bytes_size)
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
	/* CHECK: the pack's own tags' bytes and its geometry within PLACED_MEMORY_MAXIMUM */
	if (size > PLACED_MEMORY_MAXIMUM || bytes_size > PLACED_MEMORY_MAXIMUM - size)
	{
		char detail[96];

		snprintf(detail, sizeof(detail), "%lu bytes of tags and geometry, at most %d", bytes_size + size,
			(int)PLACED_MEMORY_MAXIMUM);
		return placed_refuse("no memory: %s", detail);
	}
	placed.geometry = placed_allocate(size);
	if (!placed.geometry)
		return FALSE;

	return TRUE;
}

/* the model parts' buffers, from the pack's vertices and indices (a fresh one each: the tags carry none). The
last step before the table is the game's. CHECK: every part's buffers made, else refused "no memory" (those made
are let go with the rest, placed_free); a game with no device (a dedicated server) makes none and draws nothing */
static boolean placed_buffers_make(
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
	{
		char detail[64];

		snprintf(detail, sizeof(detail), "%d model parts got no vertex or index buffers", (int)undrawn);
		return placed_refuse("no memory: %s", detail);
	}

	return TRUE;
}

/* a tag's definition while the pack is being added (before the grown table is the game's): an own tag of the
pack its placed copy, any other the map's (tag_get, which asserts its group); NULL for NONE */
static void *placed_definition(
	struct ae_pack const *pack,
	unsigned long group_tag,
	long tag_index)
{
	unsigned int index;

	if (tag_index == NONE)
		return NULL;
	for (index = 0; index < pack->tag_count; index++)
	{
		if (placed.tag_data[index] && placed.tag_indices[index] == tag_index)
			return pack->tags[index].group == group_tag ? placed.tag_data[index] : NULL;
	}
	if ((tag_index & 0xffff) >= placed.original_count)
		return NULL;

	return tag_get((long)group_tag, tag_index);
}

/* the seats of the graph that hold weapons and have no weapon type of the label: how many, the first of them in
seat_label (its label), and in holding how many seats hold weapons at all. unit_set_or_test_seat_and_weapon_label
(units.c) looks for a weapon's label among the weapon types of the classes of the unit's seat. A seat HOLDS
WEAPONS when one of its weapon types has a label: a seat whose classes have only the unarmed type (the empty
label: the stock Spartan's 13 driver's and gunner's seats, where no weapon is held and the assault rifle's label
is not found either) holds none and is not counted */
static short graph_seats_without_weapon_label(
	struct animation_graph *graph,
	char const *label,
	char const **seat_label,
	short *holding)
{
	short seat_index, class_index, type_index;
	short missing = 0;

	*seat_label = NULL;
	*holding = 0;
	for (seat_index = 0; seat_index < graph->unit_seats.count; seat_index++)
	{
		struct animation_graph_unit_seat *seat =
			TAG_BLOCK_GET_ELEMENT(&graph->unit_seats, seat_index, struct animation_graph_unit_seat);
		boolean found = FALSE, holds = FALSE;

		for (class_index = 0; !found && class_index < seat->weapon_classes.count; class_index++)
		{
			struct animation_graph_weapon_class *weapon_class =
				TAG_BLOCK_GET_ELEMENT(&seat->weapon_classes, class_index, struct animation_graph_weapon_class);

			for (type_index = 0; !found && type_index < weapon_class->weapon_types.count; type_index++)
			{
				struct animation_graph_weapon_type *type =
					TAG_BLOCK_GET_ELEMENT(&weapon_class->weapon_types, type_index, struct animation_graph_weapon_type);

				if (type->label[0])
					holds = TRUE;
				found = !_stricmp(type->label, label);
			}
		}
		if (!holds && !found)
			continue;
		(*holding)++;
		if (!found)
		{
			if (!missing)
				*seat_label = seat->label;
			missing++;
		}
	}

	return missing;
}

/* the map's player: its globals' multiplayer unit's animation graph (globals\globals: multiplayer_information[0]
.unit; the unit's object.animation_graph), or NULL */
static struct animation_graph *map_player_graph(
	void)
{
	long globals_index = tag_loaded(GAME_GLOBALS_TAG, "globals\\globals");
	struct game_globals *globals = globals_index == NONE ? NULL : game_globals_definition_get(globals_index);
	struct game_globals_multiplayer_information *information = globals && globals->multiplayer_information.count > 0 ?
		TAG_BLOCK_GET_ELEMENT(&globals->multiplayer_information, 0, struct game_globals_multiplayer_information) : NULL;
	long unit_index = information ? information->unit.index : NONE;
	long graph_index = unit_index == NONE ? NONE :
		object_definition_get(unit_index)->object.animation_graph.index;

	return graph_index == NONE ? NULL : animation_graph_definition_get(graph_index);
}

/* CHECK: each of the pack's weapons held by a label the map's player can hold it by in EVERY seat it holds
weapons in (graph_seats_without_weapon_label; unit_add_weapon_to_inventory refuses a weapon whose label the
unit's seat lacks: a seat without it would leave the player unable to take the rifle there): the weapon entry's label, else its fallback labels in
order ("h_br", which only a map with that biped knows, then "ar", the assault rifle's), the first one the graph
has in every such seat, written into the weapon's copy (weapon.label). None: refused "the player cannot hold it",
and nothing of the map is changed yet */
static boolean placed_weapon_labels(
	struct ae_pack const *pack)
{
	struct animation_graph *graph = map_player_graph();
	unsigned int index;

	for (index = 0; index < pack->weapon_count; index++)
	{
		struct ae_pack_weapon_record const *entry = &pack->weapons[index];
		struct weapon_definition *weapon = (struct weapon_definition *)placed.tag_data[entry->root_tag];
		char const *key = ae_pack_string(pack, entry->key);
		char const *labels[3];
		char const *chosen = NULL;
		char const *seat_label = NULL;
		char tried[3 * (sizeof(weapon->weapon.label) + 2)];
		short label_index, missing = 0, holding = 0;

		labels[0] = ae_pack_string(pack, entry->label);
		labels[1] = ae_pack_string(pack, entry->fallback_labels[0]);
		labels[2] = ae_pack_string(pack, entry->fallback_labels[1]);
		tried[0] = 0;
		for (label_index = 0; graph && !chosen && label_index < 3; label_index++)
		{
			char const *seat = NULL;
			short seats, seats_holding;
			size_t used = strlen(tried);

			if (!labels[label_index] || !labels[label_index][0] ||
				strlen(labels[label_index]) >= sizeof(weapon->weapon.label))
			{
				continue;
			}
			snprintf(tried + used, sizeof(tried) - used, "%s%s", used ? ", " : "", labels[label_index]);
			seats = graph_seats_without_weapon_label(graph, labels[label_index], &seat, &seats_holding);
			holding = seats_holding;
			if (!seats && seats_holding)
				chosen = labels[label_index];
			else if (!seat_label || seats < missing)
			{
				/* (what the refusal tells: the label the fewest seats lack) */
				seat_label = seat;
				missing = seats;
			}
		}
		if (!chosen)
		{
			char detail[REASON_SIZE];

			if (!graph)
				snprintf(detail, sizeof(detail), "the map's player has no animation graph");
			else
			{
				snprintf(detail, sizeof(detail), "the map's player has none of %s's labels (%s) in every seat it holds "
					"weapons in: at best %d of %d lack one, '%.32s' first", key ? key : "?", tried[0] ? tried : "none",
					(int)missing, (int)holding, seat_label ? seat_label : "");
			}
			return placed_refuse("the player cannot hold it: %s", detail);
		}
		memset(weapon->weapon.label, 0, sizeof(weapon->weapon.label));
		memcpy(weapon->weapon.label, chosen, strlen(chosen));
		error(_error_silent, "weapon pack: %s is held by the label '%s'", key ? key : "?", chosen);
	}

	return TRUE;
}

/* whether every node of the model has a node of the same name in the graph, as first_person_weapons.c binds
a first-person model to a weapon's first-person animations (model_build_remapping_table_for_animation_graph: the
model's first MAXIMUM_NODES_PER_ANIMATION nodes, each found by name among as many of the graph's; a node not found
makes the weapon's first person never shown). FALSE and the first node not found in node_name */
static boolean model_nodes_in_graph(
	struct model *model,
	struct animation_graph *graph,
	char const **node_name)
{
	short model_node_index, node_index;

	for (model_node_index = 0; model_node_index < MIN(model->nodes.count, MAXIMUM_NODES_PER_ANIMATION); model_node_index++)
	{
		struct model_node *model_node = TAG_BLOCK_GET_ELEMENT(&model->nodes, model_node_index, struct model_node);
		boolean found = FALSE;

		for (node_index = 0; !found && node_index < MIN(graph->nodes.count, MAXIMUM_NODES_PER_ANIMATION); node_index++)
		{
			struct placed_animation_graph_node *graph_node =
				TAG_BLOCK_GET_ELEMENT(&graph->nodes, node_index, struct placed_animation_graph_node);

			found = !strncmp(model_node->name, graph_node->name, sizeof(model_node->name));
		}
		if (!found)
		{
			*node_name = model_node->name;
			return FALSE;
		}
	}

	return TRUE;
}

/* CHECK: each of the pack's weapons' first-person animations (weapon.interface_definition.first_person_animations)
move the map's first-person hands (globals' first_person_interface[0].hands) and the weapon's own first-person
model (first_person_model), node by node as first_person_weapons.c binds them (first_person_weapon_ready: both
remapping tables must be valid, else the weapon is never shown in first person): else refused "first-person arms
do not match" */
static boolean placed_first_person_check(
	struct ae_pack const *pack)
{
	long globals_index = tag_loaded(GAME_GLOBALS_TAG, "globals\\globals");
	struct game_globals *globals = globals_index == NONE ? NULL : game_globals_definition_get(globals_index);
	struct game_globals_first_person_interface *first_person = globals && globals->first_person_interface.count > 0 ?
		TAG_BLOCK_GET_ELEMENT(&globals->first_person_interface, 0, struct game_globals_first_person_interface) : NULL;
	struct model *hands = first_person ? placed_definition(pack, MODELS_GROUP_TAG, first_person->hands.index) : NULL;
	unsigned int index;

	for (index = 0; index < pack->weapon_count; index++)
	{
		struct weapon_definition *weapon = (struct weapon_definition *)placed.tag_data[pack->weapons[index].root_tag];
		struct weapon_interface_definition const *interface = &weapon->weapon.interface_definition;
		struct animation_graph *graph = placed_definition(pack, ANIMATION_GRAPH_TAG, interface->first_person_animations.index);
		struct model *model = placed_definition(pack, MODELS_GROUP_TAG, interface->first_person_model.index);
		char const *node_name = NULL;
		char detail[REASON_SIZE];

		if (!graph || !model)
		{
			snprintf(detail, sizeof(detail), "the weapon has no first-person %s", !graph ? "animations" : "model");
			return placed_refuse("first-person arms do not match: %s", detail);
		}
		if (!hands)
			return placed_refuse("first-person arms do not match: %s", "the map has no first-person hands");
		if (!model_nodes_in_graph(hands, graph, &node_name))
		{
			snprintf(detail, sizeof(detail), "the map's hands' node '%.32s' is not in the weapon's animations", node_name);
			return placed_refuse("first-person arms do not match: %s", detail);
		}
		if (!model_nodes_in_graph(model, graph, &node_name))
		{
			snprintf(detail, sizeof(detail), "the weapon's model's node '%.32s' is not in its animations", node_name);
			return placed_refuse("first-person arms do not match: %s", detail);
		}
	}

	return TRUE;
}

/* a map's own battle rifle (CE+ X's dw2\weapons\battle_rifle\battle_rifle): a weapon tag of the map whose name
says so. Left as the map made it (decision 5), the pack's added beside it: logged with both names */
static void placed_own_rifle_log(
	struct ae_pack const *pack,
	struct placed_tag_instance const *instances,
	long count,
	char const *map_name)
{
	long index;

	for (index = 0; index < count; index++)
	{
		char const *name = xbox_pointer(instances[index].name);
		char lowered[AE_PACK_MAXIMUM_NAME + 1];
		size_t length;

		if (instances[index].group_tag != AE_PACK_GROUP_WEAPON || !name)
			continue;
		for (length = 0; name[length] && length < AE_PACK_MAXIMUM_NAME; length++)
			lowered[length] = (char)(name[length] >= 'A' && name[length] <= 'Z' ? name[length] + 32 : name[length]);
		lowered[length] = 0;
		if (!strstr(lowered, "battle_rifle") && !strstr(lowered, "battle rifle"))
			continue;
		error(_error_silent, "weapon pack: map has its own battle rifle: left as the map made it (%s's %s; the pack's "
			"is %s)", map_name, name, pack->weapon_count ? placed.tag_names[pack->weapons[0].root_tag] : "?");
	}
}

/* the pack added to the map whose tags have just loaded: FALSE (the reason in placed.problem, nothing of the
map changed, everything let go by the caller) if it cannot be */
static boolean placed_add(
	struct ae_pack *pack,
	char const *map_name,
	short slot)
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
	unsigned int first_missing = 0;
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
	/* CHECK: the map's tags and the pack's own within PLACED_TABLE_TAGS_MAXIMUM (or the test-only
	debug.weapon_pack_table_limit, when above 0 and below it), the salts within PLACED_SALT_MAXIMUM (R16) */
	{
		long limit = config_integer("debug.weapon_pack_table_limit");

		if (limit <= 0 || limit > PLACED_TABLE_TAGS_MAXIMUM)
			limit = PLACED_TABLE_TAGS_MAXIMUM;
		if (existing + (long)pack->own_tag_count > limit || salt + (long)pack->own_tag_count > PLACED_SALT_MAXIMUM)
		{
			char detail[96];

			snprintf(detail, sizeof(detail), "%ld of the map's and %u of the pack's, at most %ld", existing,
				pack->own_tag_count, limit);
			return placed_refuse("tag table full: %s", detail);
		}
	}
	placed.original_count = existing;

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
			/* (an optional one: no tag; the first logged, with how many at the end of the walk) */
			if (!placed.missing_count++)
				first_missing = index;
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
			/* CHECK: no tag of the map has an own tag's group and name (tag_loaded finds a tag by both: the first of
			two would be found by every lookup by name) */
			if (tag_loaded((long)tag->group, name) != NONE)
			{
				snprintf(placed.problem, sizeof(placed.problem), "name collision: the map has %s %s",
					group_text(tag->group, text), name);
				return FALSE;
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
	if (placed.missing_count)
	{
		error(_error_silent, "weapon pack: %s lacks %d optional tags of the pack, first %s %s: they are no tag there",
			map_name, (int)placed.missing_count, group_text(pack->tags[first_missing].group, text),
			ae_pack_string(pack, pack->tags[first_missing].name));
	}

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
	if (!placed_geometry_copy(pack, bytes_size))
		return FALSE;
	/* the checks against what the map gives the pack's weapons: a player who can hold them, hands they fit */
	if (!placed_weapon_labels(pack) || !placed_first_person_check(pack))
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

	if (!placed_buffers_make(pack))
		return FALSE;
	if (slot == AE_WEAPON_SLOT_BATTLE_RIFLE)
		placed_own_rifle_log(pack, instances, existing, map_name);
	placed.original_instances = instances;
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

/* ---------- private code: the fallback */

/* the map's file name's last part (a scenario's name or a header's: "bloodgulch") */
static char const *map_base_name(
	char const *map_name)
{
	char const *base = map_name, *cursor;

	for (cursor = map_name; *cursor; cursor++)
	{
		if (*cursor == '\\' || *cursor == '/')
			base = cursor + 1;
	}

	return base;
}

static char const *refused_map_reason(
	char const *map_name)
{
	short index;

	map_name = map_base_name(map_name);
	for (index = 0; index < REFUSED_MAPS_MAXIMUM; index++)
	{
		if (refused_maps[index].map_name[0] && !_stricmp(refused_maps[index].map_name, map_name))
			return refused_maps[index].reason;
	}

	return NULL;
}

static void refused_map_forget(
	char const *map_name)
{
	short index;

	map_name = map_base_name(map_name);
	for (index = 0; index < REFUSED_MAPS_MAXIMUM; index++)
	{
		if (!_stricmp(refused_maps[index].map_name, map_name))
			memset(&refused_maps[index], 0, sizeof(refused_maps[index]));
	}
}

/* why the pack is not on this map: for the line at game start, and (remembered) for the lobby's */
static void refusal_note(
	char const *map_name,
	char const *reason,
	boolean remembered)
{
	snprintf(refusal.reason, sizeof(refusal.reason), "%s", reason);
	if (remembered)
	{
		refused_map_forget(map_name);
		snprintf(refused_maps[refused_maps_next].map_name, sizeof(refused_maps[0].map_name), "%s",
			map_base_name(map_name));
		snprintf(refused_maps[refused_maps_next].reason, sizeof(refused_maps[0].reason), "%s", reason);
		refused_maps_next = (short)((refused_maps_next + 1) % REFUSED_MAPS_MAXIMUM);
	}
}

/* the add stopped (placed.problem says why): everything let go, one line, the reason kept */
static void refusal_refused(
	struct ae_pack *pack,
	char const *map_name)
{
	char problem[sizeof(placed.problem)];

	snprintf(problem, sizeof(problem), "%s", placed.problem);
	placed_free(pack);
	error(_error_silent, "weapon pack: refused for %s: %s; the game plays without it", map_name, problem);
	refusal_note(map_name, problem, TRUE);
}

/* whether the game about to be played on the map whose tags have just loaded asks for packs (their slots' mask in
mask, ae_weapon_slots.h): the pending game variant's weapon_pack (game_variant_global, which a network game's host
and clients, the test host and a split-screen game all set just before the map loads; the menus' map and the
campaign have none), and a multiplayer scenario. Each map starts with no refusal, for every slot */
static boolean refusal_asked(
	char const *map_name,
	long scenario_tag_index,
	unsigned int *mask)
{
	struct game_variant const *variant = &game_variant_global;
	short slot;

	for (slot = 1; slot <= AE_WEAPON_PACK_SLOTS; slot++)
	{
		memset(&refusals[slot - 1], 0, sizeof(refusals[slot - 1]));
		snprintf(refusals[slot - 1].map_name, sizeof(refusals[slot - 1].map_name), "%s", map_name);
	}
	*mask = 0;
	if (!variant->game_engine_index)
		return FALSE;
	*mask = (unsigned int)game_variant_weapon_pack(variant->universal_variant.weapon_pack);
	if (!*mask)
		return FALSE;
	if (scenario_tag_index == NONE || scenario_definition_get(scenario_tag_index)->type != _scenario_type_multiplayer)
		return FALSE;
	for (slot = 1; slot <= AE_WEAPON_PACK_SLOTS; slot++)
		refusals[slot - 1].asked = (*mask & AE_WEAPON_SLOT_BIT(slot)) != 0;

	return TRUE;
}

/* whether any slot's pack is on the loaded map */
static boolean placed_any(
	void)
{
	short slot;

	for (slot = 1; slot <= AE_WEAPON_PACK_SLOTS; slot++)
		if (placements[slot - 1].added)
			return TRUE;
	return FALSE;
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
	/* (Settings > Game Options, WEAPON PACKS: OFF: none is read, as with an empty packs folder) */
	if (!config_boolean("game.weapon_packs"))
	{
		platform_log("weapon pack: none read (game.weapon_packs is false)");
		return;
	}
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
	for (slot = 1; slot <= AE_WEAPON_PACK_SLOTS; slot++)
	{
		if (placements[slot - 1].added)
			error(_error_silent, "weapon pack: the game ends with the pack still on %s: its tags were never let go",
				placements[slot - 1].map_name);
		placements[slot - 1].added = FALSE;
	}
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

/* ---------- private code: the look of the pack's weapons */

/* a pack string (UTF-8, checked by the reader) as the game's wide text, at most count - 1 characters: a character
past 16 bits, or a broken sequence, becomes '?' */
static void text_widen(
	wchar_t *text,
	size_t count,
	char const *utf8)
{
	unsigned char const *in = (unsigned char const *)utf8;
	size_t length = 0;

	while (in && *in && length + 1 < count)
	{
		unsigned long character = *in++;

		if (character >= 0xC0 && character < 0xE0 && (in[0] & 0xC0) == 0x80)
			character = ((character & 0x1F) << 6) | (in++[0] & 0x3F);
		else if (character >= 0xE0 && character < 0xF0 && (in[0] & 0xC0) == 0x80 && (in[1] & 0xC0) == 0x80)
		{
			character = ((character & 0x0F) << 12) | ((unsigned long)(in[0] & 0x3F) << 6) | (in[1] & 0x3F);
			in += 2;
		}
		else if (character >= 0x80)
		{
			character = '?';
			while ((*in & 0xC0) == 0x80)
				in++;
		}
		text[length++] = (wchar_t)character;
	}
	text[length] = 0;
}

/* the look of each of the pack's weapons on the map it was just added to (its grown table is the game's): the
pickup texts its entry names replace its item messages (its hud_message_index then points at the pack's strings,
ITEM_STRING_INDEX_FIRST); the icon its entry names stands for its HUD's messaging icon in the swap and pickup
prompts. An icon whose bitmap is none, or whose stored sequence holds no sprites (R10, R30: a sheet drawn whole
would cover the screen, round 6b's bug), is refused for the map with one line, and the prompt then draws no icon.
Only the pack's own weapon HUD is keyed on: a HUD tag shared with the map is its weapons' too (their icon stays) */
static void placed_looks_set(
	struct ae_pack const *pack)
{
	unsigned int index;

	for (index = 0; index < pack->weapon_count && index < AE_PACK_MAXIMUM_WEAPONS; index++)
	{
		struct ae_pack_weapon_record const *record = &pack->weapons[index];
		struct weapon_definition *weapon = (struct weapon_definition *)placed.tag_data[record->root_tag];
		char const *key = ae_pack_string(pack, record->key);
		char const *texts[2];
		char icon_text[160];
		long hud_index;
		short which;

		placed.looks[index].icon_bitmap = NONE;
		placed.looks[index].messaging_icon = NULL;
		if (!weapon)
			continue;
		texts[0] = ae_pack_string(pack, record->weapon_pickup_text);
		texts[1] = ae_pack_string(pack, record->ammo_pickup_text);
		for (which = 0; which < 2; which++)
		{
			if (texts[which] && texts[which][0])
			{
				text_widen(placed.looks[index].texts[which], NUMBEROF(placed.looks[index].texts[which]), texts[which]);
				placed.looks[index].texts_set = TRUE;
			}
		}
		if (placed.looks[index].texts_set)
		{
			placed.looks[index].message_index = weapon->item.hud_message_index;
			weapon->item.hud_message_index = (short)(ITEM_STRING_INDEX_FIRST + ITEM_STRINGS_PER_SLOT * (placed.slot - 1) +
			2 * index);
		}

		/* (only the pack's own HUD: a HUD tag the pack shares with the map is the map's weapons' too, and they keep
		their icon) */
		hud_index = weapon->weapon.interface_definition.hud_interface.index;
		if (hud_index != NONE && ae_weapon_pack_tag_is_own(hud_index))
		{
			placed.looks[index].messaging_icon = &((struct weapon_hud_interface_definition *)
				tag_get(WEAPON_HUD_INTERFACE_TAG, hud_index))->messaging_icon;
		}
		snprintf(icon_text, sizeof(icon_text), "none");
		if (record->icon_bitmap_tag != AE_PACK_NONE)
		{
			char const *map_name = placed.map_name[0] ? placed.map_name : "the map";
			long bitmap_index = record->icon_bitmap_tag < pack->tag_count ?
				placed.tag_indices[record->icon_bitmap_tag] : NONE;
			char const *bitmap_name = record->icon_bitmap_tag < pack->tag_count ?
				placed.tag_names[record->icon_bitmap_tag] : NULL;
			struct bitmap_group *group = bitmap_index != NONE &&
				pack->tags[record->icon_bitmap_tag].group == AE_PACK_GROUP_BITMAP ? bitmap_group_get(bitmap_index) : NULL;
			long sprites = group && record->icon_sequence < (unsigned int)group->sequences.count ?
				TAG_BLOCK_GET_ELEMENT(&group->sequences, record->icon_sequence, struct bitmap_group_sequence)->sprites.count :
				0;

			if (hud_index == NONE)
				error(_error_silent, "weapon pack: %s's message icon refused: the weapon has no HUD", key ? key : "?");
			else if (!placed.looks[index].messaging_icon)
			{
				error(_error_silent, "weapon pack: %s's message icon refused for %s: its HUD is the map's; the map's "
					"weapons keep their icon", key ? key : "?", map_name);
			}
			else if (!group)
			{
				error(_error_silent, "weapon pack: %s's message icon refused for %s: its bitmap is not on the map; no "
					"icon is drawn", key ? key : "?", map_name);
			}
			else if (sprites <= 0)
			{
				error(_error_silent, "weapon pack: %s's message icon refused for %s: sequence %u of %s has no sprites; "
					"no icon is drawn", key ? key : "?", map_name, record->icon_sequence, bitmap_name ? bitmap_name : "?");
			}
			else
			{
				placed.looks[index].icon_bitmap = bitmap_index;
				placed.looks[index].icon_sequence = (short)record->icon_sequence;
				snprintf(icon_text, sizeof(icon_text), "%s sequence %u (%ld sprites)", bitmap_name ? bitmap_name : "?",
					record->icon_sequence, sprites);
			}
			/* (refused with the pack's own HUD: its messaging icon is not drawn in the entry's place, which would be
			a sprite of the map's sheet that means something else) */
			placed.looks[index].icon_refused = placed.looks[index].messaging_icon && placed.looks[index].icon_bitmap == NONE;
		}
		error(_error_silent, "weapon pack: %s's look: pickup texts \"%s\" / \"%s\", message icon %s; its HUD's icon "
			"sequence %d", key ? key : "?", texts[0] ? texts[0] : "(the engine's)", texts[1] ? texts[1] : "(the engine's)",
			icon_text, placed.looks[index].messaging_icon ? (int)placed.looks[index].messaging_icon->sequence_index : -1);
	}
}

/* once a map's tags are read and checked (scenario_tags_load), before anything else adds to its tag table: the
pack the game about to be played asks for is added to a multiplayer map. A map loaded for no game (the menus',
the campaign: no pending game variant) gets none */
void ae_weapon_pack_tags_loaded(
	char const *map_name,
	long scenario_tag_index)
{
	/* (the map's header's name: 32 bytes, its terminator the header's to keep) */
	char name[MAP_NAME_SIZE];
	unsigned int mask;
	short slot;

	if (placed_any() || !map_name)
		return;
	snprintf(name, sizeof(name), "%.32s", map_name);
	map_name = name;
	ae_pack_spawns_new_map();
	if (!refusal_asked(map_name, scenario_tag_index, &mask))
		return;
	/* (each named slot's pack in slot order: each grows the table the one before left) */
	for (slot = 1; slot <= AE_WEAPON_PACK_SLOTS; slot++)
	{
		struct ae_pack *pack = slot_pack(slot);

		if (!(mask & AE_WEAPON_SLOT_BIT(slot)))
			continue;
		placed_select(slot);
		memset(&placed, 0, sizeof(placed));
		if (!pack)
		{
			char reason[64];

			error(_error_silent, "weapon pack: not added to %s: this game has no pack for slot %d", map_name, (int)slot);
			snprintf(reason, sizeof(reason), "this machine has no %s pack", ae_weapon_slots_name(slot));
			refusal_note(map_name, reason, FALSE);
			continue;
		}
		/* CHECK: the map's tags are laid out as the engine's: a build it plays multiplayer on (2276, NHE's 2300,
		PAL's through pal_tags.c: cache_files.c's list) */
		{
			char build[0x20];

			if (!cache_files_multiplayer_region(build))
			{
				snprintf(placed.problem, sizeof(placed.problem), "build %.31s", build);
				refusal_refused(pack, map_name);
				continue;
			}
		}
		if (!placed_add(pack, map_name, slot))
		{
			refusal_refused(pack, map_name);
			continue;
		}
		placed.added = TRUE;
		placed.slot = slot;
		snprintf(placed.map_name, sizeof(placed.map_name), "%s", map_name);
		refused_map_forget(map_name);
		placed_looks_set(pack);
	}
	placed_select(1);
}

/* the Custom Edition branch of scenario_tags_load: no pack on such a map (R34: the pack's tags would be read
through the Custom Edition tag cache's bounds, and its bitmaps uploaded in Halo PC's layout); said once a map */
void ae_weapon_pack_tags_refused_ce(
	char const *map_name,
	long scenario_tag_index)
{
	char name[MAP_NAME_SIZE];
	unsigned int mask;
	short slot;

	if (placed_any() || !map_name)
		return;
	snprintf(name, sizeof(name), "%.32s", map_name);
	if (!refusal_asked(name, scenario_tag_index, &mask))
		return;
	for (slot = 1; slot <= AE_WEAPON_PACK_SLOTS; slot++)
	{
		if (!(mask & AE_WEAPON_SLOT_BIT(slot)))
			continue;
		placed_select(slot);
		error(_error_silent, "weapon pack: refused for %s: %s; the game plays without it", name, NOT_ON_CUSTOM_EDITION);
		refusal_note(name, NOT_ON_CUSTOM_EDITION, TRUE);
	}
	placed_select(1);
}

boolean ae_weapon_pack_unavailable_line(
	short slot,
	char *line,
	long size)
{
	struct refusal_state const *state = slot >= 1 && slot <= AE_WEAPON_PACK_SLOTS ? &refusals[slot - 1] : NULL;

	if (!line || size <= 0 || !state || !state->asked || placements[slot - 1].added)
		return FALSE;
	if (!strcmp(state->reason, NOT_ON_CUSTOM_EDITION))
		snprintf(line, (size_t)size, "%s: %s", ae_weapon_slots_display(slot), NOT_ON_CUSTOM_EDITION);
	else
	{
		snprintf(line, (size_t)size, "%s unavailable on %s: %s", ae_weapon_slots_display(slot), state->map_name,
			state->reason[0] ? state->reason : "not added");
	}

	return TRUE;
}

void ae_weapon_pack_lobby_line(
	wchar_t *text,
	long count,
	char const *map_name,
	struct game_variant const *variant)
{
	short const selected = placed_selected();
	char file[MAP_NAME_SIZE + 8];
	char line[96];
	unsigned int mask;
	short slot, index;
	size_t length;

	if (!text || count <= 0 || !map_name || !map_name[0] || !variant)
		return;
	mask = (unsigned int)game_variant_weapon_pack(variant->universal_variant.weapon_pack);
	/* (a line a named slot whose pack this machine knows cannot be added) */
	for (slot = 1; slot <= AE_WEAPON_PACK_SLOTS; slot++)
	{
		if (!(mask & AE_WEAPON_SLOT_BIT(slot)))
			continue;
		placed_select(slot);
		/* (the pre-game check on the map's family, R34: before any map is loaded) */
		if (map_family_parse(map_name, file, sizeof(file)) != _map_family_xbox)
			snprintf(line, sizeof(line), "%s: %s", ae_weapon_slots_display(slot), NOT_ON_CUSTOM_EDITION);
		else if (!slot_pack(slot) || refused_map_reason(file))
			snprintf(line, sizeof(line), "%s unavailable on %.40s", ae_weapon_slots_display(slot), file);
		else
			continue;
		length = 0;
		while (length < (size_t)count && text[length])
			length++;
		if (length + 2 + strlen(line) + 1 > (size_t)count)
			break;
		text[length++] = '\r';
		text[length++] = '\n';
		for (index = 0; line[index]; index++)
			text[length++] = (wchar_t)(unsigned char)line[index];
		text[length] = 0;
	}
	placed_select(selected);
}

/* as a map's tags are let go (scenario_tags_unload: after the texture and sound caches closed and the menus'
tags went, so the table is the pack's grown one again; before the map's file closes) */
void ae_weapon_pack_tags_unloaded(
	void)
{
	short slot;

	/* (in reverse: each slot's table back to the one the slot before left) */
	for (slot = AE_WEAPON_PACK_SLOTS; slot >= 1; slot--)
	{
		placed_select(slot);
		if (!placed.added)
			continue;
		error(_error_silent, "weapon pack: let go from %s: %d tags; %d bitmap and %d sound reads (%u bytes) were "
			"served from the pack, %d could not be", placed.map_name, (int)placed.own_count, (int)placed.bitmap_reads,
			(int)placed.sound_reads, (unsigned int)placed.read_bytes, (int)placed.failed_reads);
		cache_files_set_tag_instances(placed.original_instances, placed.original_count);
		placed_free(slot_pack(placed.slot));
	}
	placed_select(1);
}

/* a read of the map file for tag tag_index (cache_file_read: a bitmap's pixels, a sound's samples): TRUE if the
tag is one of the pack's own on this map, and then buffer has its size bytes from the pack's file, read now (zeros
for any the pack does not hold or its file no longer gives) and the read is complete */
/* guest memory written as DMA writes it: the renderer write-protects the texture cache pages it has uploaded
(memory_watch.c), and this write marks such a texture stale so that it is uploaded again (xbox_files.c's
read_at does the same) */
static void guest_write(
	void *destination,
	void const *source,
	size_t size)
{
#ifdef HALO_64BIT
	platform_contiguous_write(destination, source, size);
#else
	memcpy(destination, source, size);
#endif
}

/* a pack resource read into the caller's buffer: TRUE, or FALSE and the buffer zeros. A buffer in guest memory
(the texture and sound caches') is never handed to the file read: the kernel fails a read() into a page the
renderer protects (EFAULT) instead of faulting into its watch, so the bytes go through a host buffer first, as
xbox_files.c's read_at does (no read is larger than a resource, AE_PACK_MAXIMUM_RESOURCE_SIZE) */
static boolean resource_read_into(
	struct ae_pack *pack,
	unsigned int tag,
	unsigned int offset,
	unsigned int size,
	void *buffer)
{
	boolean guest = platform_is_contiguous(buffer) || platform_is_contiguous((char *)buffer + size - 1);
	char *staging;
	boolean read;

	if (!guest)
	{
		read = ae_pack_read_resource(pack, tag, offset, size, buffer);
		if (!read)
			memset(buffer, 0, size);
		return read;
	}
	staging = size <= AE_PACK_MAXIMUM_RESOURCE_SIZE ? malloc(size) : NULL;
	read = staging && ae_pack_read_resource(pack, tag, offset, size, staging);
	if (staging)
	{
		if (!read)
			memset(staging, 0, size);
		guest_write(buffer, staging, size);
		free(staging);
	}
	else
	{
		static char const zeros[0x1000];
		unsigned int done;

		for (done = 0; done < size; done += sizeof(zeros))
			guest_write((char *)buffer + done, zeros, MIN(size - done, (unsigned int)sizeof(zeros)));
	}
	return read;
}

static boolean placed_read(
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
		if (size && !resource_read_into(pack, index, (unsigned int)offset, (unsigned int)size, buffer))
		{
			/* (each failure its own line, up to a bound a map) */
			if (placed.failed_reads++ < FAILED_READS_LOGGED)
			{
				error(_error_silent, "weapon pack: %s: %d bytes at %08x could not be read from the pack: zeros",
					placed.tag_names[index], (int)size, (unsigned int)offset);
			}
			else if (placed.failed_reads == FAILED_READS_LOGGED + 1)
			{
				error(_error_silent, "weapon pack: more reads could not be read from the pack; the count is in the "
					"\"let go\" line");
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

boolean ae_weapon_pack_read(
	long tag_index,
	long offset,
	long size,
	void *buffer,
	boolean *complete)
{
	short const selected = placed_selected();
	short slot;
	boolean read = FALSE;

	for (slot = 1; !read && slot <= AE_WEAPON_PACK_SLOTS; slot++)
	{
		placed_select(slot);
		read = placed_read(tag_index, offset, size, buffer, complete);
	}
	placed_select(selected);
	return read;
}

short ae_weapon_pack_added(
	void)
{
	short slot;

	for (slot = 1; slot <= AE_WEAPON_PACK_SLOTS; slot++)
		if (placements[slot - 1].added)
			return slot;
	return 0;
}

boolean ae_weapon_pack_slot_added(
	short slot)
{
	return slot >= 1 && slot <= AE_WEAPON_PACK_SLOTS && placements[slot - 1].added;
}

unsigned int ae_weapon_pack_added_mask(
	void)
{
	unsigned int mask = 0;
	short slot;

	for (slot = 1; slot <= AE_WEAPON_PACK_SLOTS; slot++)
		if (placements[slot - 1].added)
			mask |= AE_WEAPON_SLOT_BIT(slot);
	return mask;
}

boolean ae_weapon_pack_fingerprint(
	short slot,
	unsigned char *fingerprint)
{
	if (!ae_weapon_pack_slot_added(slot))
		return FALSE;
	memcpy(fingerprint, placements[slot - 1].fingerprint, PLACED_FINGERPRINT_SIZE);

	return TRUE;
}

static boolean placed_tag_is_own(
	long tag_index)
{
	struct ae_pack const *pack = slot_pack(placed.slot);
	long absolute_index = tag_index & 0xffff;
	unsigned int index;

	if (!placed.added || !pack || tag_index == NONE || absolute_index < placed.original_count ||
		absolute_index >= placed.original_count + placed.own_count)
	{
		return FALSE;
	}
	for (index = 0; index < pack->tag_count; index++)
	{
		if (placed.tag_data[index] && placed.tag_indices[index] == tag_index)
			return TRUE;
	}
	return FALSE;
}

boolean ae_weapon_pack_tag_is_own(
	long tag_index)
{
	short const selected = placed_selected();
	short slot;
	boolean own = FALSE;

	for (slot = 1; !own && slot <= AE_WEAPON_PACK_SLOTS; slot++)
	{
		placed_select(slot);
		own = placed_tag_is_own(tag_index);
	}
	placed_select(selected);
	return own;
}

long ae_weapon_pack_weapon_definition(
	short slot,
	short weapon)
{
	struct ae_pack const *pack = slot_pack(slot);
	struct placed_state const *state = slot >= 1 && slot <= AE_WEAPON_PACK_SLOTS ? &placements[slot - 1] : NULL;

	if (!state || !state->added || state->slot != slot || !pack || weapon < 0 ||
		(unsigned int)weapon >= pack->weapon_count)
	{
		return NONE;
	}

	return state->tag_indices[pack->weapons[weapon].root_tag];
}

short ae_weapon_pack_definition_slot(
	long definition_index)
{
	short slot;

	for (slot = 1; definition_index != NONE && slot <= AE_WEAPON_PACK_SLOTS; slot++)
	{
		short weapon;

		for (weapon = 0; weapon < ae_weapon_pack_weapon_count(slot); weapon++)
			if (ae_weapon_pack_weapon_definition(slot, weapon) == definition_index)
				return slot;
	}
	return 0;
}

static void placed_loadout_given(
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

void ae_weapon_pack_loadout_given(
	short loadout_slot,
	long definition_index)
{
	short const selected = placed_selected();
	short slot = ae_weapon_pack_definition_slot(definition_index);

	if (!slot)
		return;
	placed_select(slot);
	placed_loadout_given(loadout_slot, definition_index);
	placed_select(selected);
}

static boolean placed_item_string(
	long *string_index,
	wchar_t const **string)
{
	long relative = *string_index - ITEM_STRING_INDEX_FIRST - ITEM_STRINGS_PER_SLOT * (placed.slot - 1);
	short weapon, which;

	if (!placed.added || relative < 0 || relative >= 2 * AE_PACK_MAXIMUM_WEAPONS)
		return FALSE;
	weapon = (short)(relative / 2);
	which = (short)(relative % 2);
	if (!placed.looks[weapon].texts_set)
		return FALSE;
	if (!placed.looks[weapon].texts[which][0])
	{
		/* (a text the entry does not name: the weapon's own, as its tag had it) */
		*string_index = placed.looks[weapon].message_index + which;
		return FALSE;
	}
	if (!placed.looks[weapon].texts_shown[which])
	{
		struct ae_pack const *pack = slot_pack(placed.slot);
		char const *text = pack && (unsigned int)weapon < pack->weapon_count ? ae_pack_string(pack,
			which ? pack->weapons[weapon].ammo_pickup_text : pack->weapons[weapon].weapon_pickup_text) : NULL;

		placed.looks[weapon].texts_shown[which] = TRUE;
		error(_error_silent, "weapon pack: item message \"%s\" shown (its weapon entry's %s pickup text)",
			text ? text : "?", which ? "ammo" : "weapon");
	}
	*string = placed.looks[weapon].texts[which];
	return TRUE;
}

boolean ae_weapon_pack_item_string(
	long *string_index,
	wchar_t const **string)
{
	short const selected = placed_selected();
	long relative = *string_index - ITEM_STRING_INDEX_FIRST;
	short slot = relative >= 0 ? (short)(relative / ITEM_STRINGS_PER_SLOT + 1) : 0;
	boolean found;

	if (slot < 1 || slot > AE_WEAPON_PACK_SLOTS)
		return FALSE;
	placed_select(slot);
	found = placed_item_string(string_index, string);
	placed_select(selected);
	return found;
}

static struct icon_hud_element_definition const *placed_message_icon(
	struct icon_hud_element_definition const *icon,
	long *bitmap_group_index)
{
	/* (the prompt draws one icon at a time, and keeps nothing of it) */
	static struct icon_hud_element_definition substitute;
	short index;

	if (!placed.added || !icon)
		return icon;
	for (index = 0; index < AE_PACK_MAXIMUM_WEAPONS; index++)
	{
		if (!placed.looks[index].messaging_icon || placed.looks[index].messaging_icon != icon ||
			(placed.looks[index].icon_bitmap == NONE && !placed.looks[index].icon_refused))
		{
			continue;
		}
		substitute = *icon;
		/* (a refused icon: sequence NONE, which hud_retrieve_bitmap_and_bounding_rect skips: none is drawn) */
		substitute.sequence_index = placed.looks[index].icon_refused ? (short)NONE : placed.looks[index].icon_sequence;
		if (!placed.looks[index].icon_refused)
			*bitmap_group_index = placed.looks[index].icon_bitmap;
		/* (its first prompt on a map is logged) */
		if (!placed.looks[index].icon_shown)
		{
			struct ae_pack const *pack = slot_pack(placed.slot);
			char const *key = pack && (unsigned int)index < pack->weapon_count ?
				ae_pack_string(pack, pack->weapons[index].key) : NULL;

			placed.looks[index].icon_shown = TRUE;
			if (placed.looks[index].icon_refused)
				error(_error_silent, "weapon pack: %s's prompt drawn with no icon (its icon was refused)", key ? key : "?");
			else
			{
				error(_error_silent, "weapon pack: %s's message icon shown in a prompt (sequence %d of its entry's bitmap)",
					key ? key : "?", (int)placed.looks[index].icon_sequence);
			}
		}
		return &substitute;
	}
	return icon;
}

/* (the texture cache asks as it uploads a texture's pixels; the high-res HUD's textures are found the same way:
hud_hires_tags.c's hud_hires_asset_at) */
struct icon_hud_element_definition const *ae_weapon_pack_message_icon(
	struct icon_hud_element_definition const *icon,
	long *bitmap_group_index)
{
	short const selected = placed_selected();
	struct icon_hud_element_definition const *result = icon;
	short slot;

	for (slot = 1; result == icon && slot <= AE_WEAPON_PACK_SLOTS; slot++)
	{
		placed_select(slot);
		result = placed_message_icon(icon, bitmap_group_index);
	}
	placed_select(selected);
	return result;
}

static long placed_smooth_bitmap_at(
	unsigned long address,
	long width,
	long height)
{
	struct ae_pack const *pack = slot_pack(placed.slot);
	unsigned int index;
	long bitmap_index;

	if (!placed.added || !pack)
		return 0;
	for (index = 0; index < pack->tag_count; index++)
	{
		struct bitmap_group *group;

		if (!(pack->tags[index].flags & AE_PACK_TAG_FLAG_SMOOTH) || pack->tags[index].group != AE_PACK_GROUP_BITMAP ||
			!placed.tag_data[index])
		{
			continue;
		}
		group = (struct bitmap_group *)placed.tag_data[index];
		for (bitmap_index = 0; bitmap_index < group->bitmaps.count; bitmap_index++)
		{
			struct bitmap_data const *bitmap = TAG_BLOCK_GET_ELEMENT(&group->bitmaps, bitmap_index, struct bitmap_data);
			boolean coverage = (pack->tags[index].flags & AE_PACK_TAG_FLAG_METER_COVERAGE) != 0;

			if (bitmap->cache_block_index == NONE || (unsigned long)bitmap->base_address != address ||
				bitmap->width != width || bitmap->height != height)
			{
				continue;
			}
			if (index < AE_PACK_MAXIMUM_TAGS && !placed.smooth_logged[index])
			{
				placed.smooth_logged[index] = TRUE;
				error(_error_silent, "weapon pack: %s drawn smooth (%ldx%ld, linear with its mips%s)",
					placed.tag_names[index], width, height, coverage ? ", its green its coverage" : "");
			}
			return coverage ? 2 : 1;
		}
	}
	return 0;
}

long ae_weapon_pack_smooth_bitmap_at(
	unsigned long address,
	long width,
	long height)
{
	short const selected = placed_selected();
	long smooth = 0;
	short slot;

	for (slot = 1; !smooth && slot <= AE_WEAPON_PACK_SLOTS; slot++)
	{
		placed_select(slot);
		smooth = placed_smooth_bitmap_at(address, width, height);
	}
	placed_select(selected);
	return smooth;
}
