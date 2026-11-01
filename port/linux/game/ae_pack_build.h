/*
AE_PACK_BUILD.H

The weapon pack builder (docs/weapon-packs.md, "Building a pack"): a
weapon's tag graph taken out of a DONOR map (an Xbox map whose tags the
tag validator passes) into a pack, through ae_pack_write. Compiled only
into the host tool tools/ae_pack_build.c (ninja linux), never into the
game: the donor's tags must lie where the game reads a map's (the tag
cache's address), which only that 32-bit tool arranges.

Every type here is the same in the game's ABI and the platform layer's
(unsigned ints, pointers, no doubles, no structs returned by value), as
ae_pack_format.h's: the tool calls these functions across the two.
*/

#ifndef AE_PACK_BUILD_H
#define AE_PACK_BUILD_H

#include "ae_pack_format.h"

enum
{
	/* the tags a donor's tag data is read to (physical_memory_map.h: where the game reads a map's tags, and how
	many bytes there are room for) */
	AE_PACK_BUILDER_TAG_CACHE_ADDRESS = 0x803A6000,
	AE_PACK_BUILDER_TAG_CACHE_SIZE = 0x1600000,
	AE_PACK_BUILDER_MAXIMUM_ROOTS = 64,
	AE_PACK_BUILDER_MAXIMUM_SMOOTH = 32,
};

/* a tag named by group and name */
struct ae_pack_builder_name
{
	unsigned int group;
	char const *name;
};

/* an own bitmap's R9 flags, by its donor name */
struct ae_pack_builder_flags
{
	char const *name;
	unsigned int flags; /* AE_PACK_TAG_FLAG_* */
};

struct ae_pack_builder_options
{
	/* the donor: its tag data (tag_data_size bytes at AE_PACK_BUILDER_TAG_CACHE_ADDRESS, checked here by the tag
	validator, corrected in place) and its whole file, inflated (the bitmaps' pixels and sounds' samples) */
	void *tag_data;
	unsigned int tag_data_size;
	unsigned char const *file;
	unsigned int file_size;
	char const *donor_name; /* for the validator's messages */

	/* what the pack holds: the weapon (a 'weap' tag of the donor's, by name) and every tag it leads to, then each
	other root's; the first 'itmc' root is the weapon's item collection. No weapon (NULL): a pack of tags only
	(AE_PACK_FLAG_NO_WEAPONS), its roots' tags, and the weapon entry's options below are not used */
	char const *weapon_name;
	struct ae_pack_builder_name const *roots;
	unsigned int root_count;
	/* the tags a map has itself (the stock maps' names, ae_pack_build --list-names): those met are EXTERNAL */
	struct ae_pack_builder_name const *external_names;
	unsigned int external_name_count;
	/* own tags' names: prefix + the donor's name (a leading "cmt\" or "ae_pack\" dropped) */
	char const *prefix;
	struct ae_pack_builder_flags const *flags;
	unsigned int flag_count;

	/* the header and the weapon entry */
	char const *pack_id;
	unsigned int pack_revision;
	char const *display_name;
	char const *weapon_key;
	unsigned int behaviour;
	char const *label;
	char const *fallback_labels[2]; /* NULL: none */
	char const *menu_name;
	char const *weapon_pickup_text; /* NULL: the engine's */
	char const *ammo_pickup_text; /* NULL: the engine's */
	char const *icon_name; /* an own bitmap's donor name, or NULL */
	unsigned int icon_sequence; /* (that sequence must have sprites: R10) */
	unsigned int replaces;
	unsigned int power;
	char const *credits; /* NULL: none */
};

/* a pack tag, as written (for the tool's --verbose list) */
struct ae_pack_builder_tag
{
	unsigned int group;
	char const *name; /* in the pack */
	char const *source_path; /* the donor's name ("" for externals) */
	unsigned int kind; /* AE_PACK_TAG_OWN, AE_PACK_TAG_EXTERNAL */
	unsigned int required;
	unsigned int flags;
	unsigned int bytes;
	unsigned int fixups;
	unsigned int parts;
	unsigned int resources;
	unsigned int resource_bytes;
};

/* what a build made (ae_pack_builder_summary_free it) */
struct ae_pack_builder_summary
{
	struct ae_pack_builder_tag *tags;
	unsigned int tag_count;
	unsigned int own_count;
	unsigned int external_count;
	unsigned int required_count;
	unsigned int fixup_count;
	unsigned int part_count;
	unsigned int resource_count;
	unsigned int tag_bytes;
	unsigned int geometry_bytes;
	unsigned int resource_bytes;
	/* the tag validator's corrections of the donor's tags (an approved donor needs none) */
	unsigned int corrections;
	/* pack tag indices, or AE_PACK_NONE */
	unsigned int item_collection_tag;
	unsigned int icon_tag;
	/* (the build's own memory) */
	void *memory;
};

/* the pack built from the donor and written to path (ae_pack_write: deterministic, every section stored): nonzero
if written, zero and the reason in problem if not (nothing is written then). summary is filled either way as far as
the build got */
int ae_pack_builder_build(struct ae_pack_builder_options const *options, char const *path,
	struct ae_pack_builder_summary *summary, struct ae_pack_problem *problem);

void ae_pack_builder_summary_free(struct ae_pack_builder_summary *summary);

/* a map's tags (tag_data_size bytes at AE_PACK_BUILDER_TAG_CACHE_ADDRESS, of a map file of file_size bytes) checked
by the tag validator, then name called with each tag's group and name: nonzero if they passed */
int ae_pack_builder_names(void *tag_data, unsigned int tag_data_size, unsigned int file_size, char const *map_name,
	void (*name)(void *context, unsigned int group, char const *name), void *context,
	struct ae_pack_problem *problem);

#endif
