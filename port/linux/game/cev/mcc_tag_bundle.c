/*
MCC_TAG_BUNDLE.C

(port) An MCC map's tags opened for the Tag Injector (mcc_tag_bundle.h,
tag_injector.c) to take out as tag bundles, which any level then plays as it
plays a Custom Edition map's (docs/tag_injector.md, "MCC maps").

A Halo 1 MCC map's tags are Halo PC's, as Custom Edition's are (Invader
builds both from the same definitions), with a few of MCC's own besides:
BC7 bitmaps, pixels and samples kept in MCC's shared resource maps by name,
HUDs laid out on a 960-line canvas, cached blocks shared between tags. A
bundle holds tags as a Custom Edition map's are once loaded and converted
(custom_edition_cache_bundle_open): its models gbxmodels ('mod2'), its
bitmaps Halo PC's, laid out as the tag stores them, its sounds Xbox ADPCM.
So the map is opened as the MCC loader opens a level (mcc_cache.c), in the
same tag window, its tags at the base they were written for (or elsewhere,
their addresses moved), and made so:

- its metadata made this build's as a level's is (mcc_hud_bitmaps_prepare,
  mcc_tags_prepare: shader types, chicago extended shaders, weapon
  functions, HUD placements and meters; Gearbox meters' opacity none, as
  mcc_bitmaps.c's mcc_meter_usage has it), its scenario's scripts, recorded
  animations and cutscene cameras left out (a bundle never holds a
  scenario, and those are a level's to prepare: mcc_scripts.c);
- its models not converted (mcc_geometry.c makes a level's this build's
  models): gbxmodels, their vertices and strips in the map's model data, as
  the bundle writer reads a Custom Edition map's;
- each bitmap's pixels given an offset of the bundle's own, past the map's
  file, and read from the map or MCC's bitmaps.map (by its tag's name and
  index, as mcc_bitmaps.c reads them) as the writer asks for them: as they
  are, BC7's as DXT5 (block for block, as mcc_bitmaps.c keeps those drawn as
  they are), and those whose sides are not powers of two, or BC7 that cannot
  be, decoded to linear A8R8G8B8 (their first level, as mcc_bitmaps.c makes
  them); their descriptors' MCC flags (external, environment, lightmap)
  cleared, as a Custom Edition map's are prepared (cache_file_formats.c's
  bitmaps_prepare);
- the sounds of the tags wanted decoded (PCM, ADPCM, Ogg Vorbis, from the
  map or MCC's sounds.map, as mcc_audio.c reads them) and encoded as Xbox
  ADPCM at this build's rates in memory, at offsets of the bundle's own
  after the pixels'; every other sound's samples none;
- then checked by the MCC validator (mcc_tags_validate_bundle), which
  separates shared blocks, takes gbxmodels by Custom Edition's schema and
  Halo PC bitmaps by the schemas' own checks, with every pixel and sample in
  the bundle's offsets.

Its tag header is then laid out as a Custom Edition map's (the table, and
where its models' vertices and strips are), which the Tag Injector's menu,
tag_names.c and the bundle writer read. What a level's MCC dispatch does
when it draws (a weapon HUD's child anchors, Gearbox meters' coverage, the
safe frame) is the MCC level's alone: a bundle's tags are drawn as a Custom
Edition map's are. A model shader of MCC's flag 6 and a HUD meter of its
flag 5 keep Xbox channel orders, which a Custom Edition map's never do: the
bundles say they are MCC's (_tag_bundle_mcc_bit), and the level adding them
leaves those channels as they are (tag_injector.c).

A bundle of a map that is not a Steam Workshop item's (MCC's own maps, or
any copied out of an installation), or whose pixels or samples are MCC's
shared resource maps', holds MCC's own content, which may not be sent to
another machine (as mcc_share_session.c's mcc_share_admit has it): it is
marked so (_tag_bundle_mcc_own_content_bit, mcc_tag_bundle_own_content),
and is never in a network game's share or a Forge map's bake
(port_tag_profiles.c).
*/

#include "cseries.h"
#include "errors.h"
#include "bitmaps/bitmap_group.h"
#include "interface/unit_hud_interface_definition.h"
#include "scenario/scenario_definitions.h"
#include "sound/sound_definitions.h"
#include "tag_files/tag_groups.h"

#include "mcc_cache.h"
#include "mcc_cache_format.h"
#include "mcc_identity.h"
#include "mcc_package.h"
#include "mcc_resources.h"
#include "mcc_runtime.h"
#include "mcc_tag_bundle.h"
#include "mcc_tag_validate.h"
#include "tag_bundle.h"
#include "tag_schema.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- constants */

/* the tag header, laid out as a Custom Edition map's (tag_injector.c's
struct injector_map_header) */
#define BUNDLE_HEADER_INSTANCES_OFFSET 0x00
#define BUNDLE_HEADER_TAG_COUNT_OFFSET 0x0C
#define BUNDLE_HEADER_MODEL_PART_COUNT_OFFSET 0x10
#define BUNDLE_HEADER_MODEL_DATA_OFFSET_OFFSET 0x14
#define BUNDLE_HEADER_MODEL_PART_COUNT_AGAIN_OFFSET 0x18
#define BUNDLE_HEADER_MODEL_INDEX_DATA_OFFSET_OFFSET 0x1C
#define BUNDLE_HEADER_MODEL_DATA_SIZE_OFFSET 0x20
#define BUNDLE_HEADER_SIGNATURE_OFFSET 0x24
#define BUNDLE_HEADER_BYTES 0x28
#define BUNDLE_TAG_ENTRY_BYTES 0x20

/* the bundle's offsets: the map's file, then the pixels, then the samples,
each from a multiple of this; all of them a long's (a bitmap's
pixels_offset), and room kept past the pixels for the samples */
#define BUNDLE_SPACE_ALIGNMENT 0x10000u
#define BUNDLE_OFFSET_LIMIT 0x7FFF0000u
#define BUNDLE_SAMPLES_RESERVE 0x10000000u
/* the most bytes of one bitmap's pixels read, or made (mcc_bitmaps.c's) */
#define BUNDLE_BITMAP_LIMIT 0x08000000u
#define BUNDLE_TEXTURE_SIZE_LIMIT 16384

/* an MCC bitmap's flags (mcc_bitmaps.c's): its pixels in bitmaps.map, an
environment's (metadata), a lightmap's; and the descriptor's own */
#define MCC_BITMAP_EXTERNAL_BIT 0x100
#define MCC_BITMAP_ENVIRONMENT_BIT 0x200
#define MCC_BITMAP_LIGHTMAP_BIT 0x1000
#define BITMAP_POWER_OF_TWO_BIT 0x01
#define BITMAP_COMPRESSED_BIT 0x02
#define BITMAP_PALETTIZED_BIT 0x04
#define BITMAP_SWIZZLED_BIT 0x08
#define BITMAP_LINEAR_BIT 0x10

enum
{
	_bitmap_format_dxt1 = 14,
	_bitmap_format_dxt5 = 16,
	_bitmap_format_p8 = 17,
	_bitmap_format_bc7 = 18,
	_bitmap_format_a8r8g8b8 = 11,

	_bitmap_type_2d = 0,
	_bitmap_type_cube_map = 2,
};

/* how a bitmap's pixels are read */
enum
{
	_pixels_as_they_are,
	/* BC7 made DXT5, block for block */
	_pixels_transcoded,
	/* the first level decoded to A8R8G8B8 */
	_pixels_decoded,
};

/* ---------- structures */

/* a bitmap's pixels as the bundle has them: its offset and size there, and
where they are read from (the map's file, or a resource of bitmaps.map, by
its tag's name and index) */
struct bundle_bitmap
{
	uint32_t offset;
	uint32_t size;
	uint32_t raw_offset;
	uint32_t raw_size;
	long tag;
	short index;
	short width;
	short height;
	short format;
	unsigned char external;
	unsigned char transform;
};

/* a sound permutation's samples, made: their offset and size, and bytes */
struct bundle_samples
{
	uint32_t offset;
	uint32_t size;
	unsigned char *bytes;
};

/* ---------- globals */

/* (mcc_memory.c's, as mcc_cache.c has them) */
void *mcc_memory_reserve(uint32_t address, uint32_t size);
void *mcc_memory_reserve_elsewhere(uint32_t linked, uint32_t size);
void mcc_memory_release(void *allocation, uint32_t size);
extern unsigned long platform_mcc_shift;
extern unsigned long platform_mcc_linked_base;
int config_boolean(char const *name);
/* (mcc_tags.c's, mcc_bitmaps.c's, mcc_audio.c's) */
int mcc_tags_prepare(struct mcc_runtime *runtime);
void mcc_bitmaps_bc7_transcode(unsigned char *blocks, uint32_t bytes);
int mcc_bitmaps_decode_texels(void const *input, uint32_t input_bytes, uint32_t *output,
	unsigned width, unsigned height, short format);
unsigned char *mcc_audio_bundle_samples(unsigned char const *data, uint32_t size, int compression,
	int channels, uint32_t rate, uint32_t *bytes, int *kept);

static struct
{
	int open;
	void *file;
	struct mcc_runtime runtime;
	char level_name[64];
	/* a Steam Workshop item's map (its package a Workshop id) */
	int workshop;
	/* of each tag: its pixels or samples are of MCC's shared resource maps;
	and the name it had in the map, which they are found by there */
	uint32_t tag_count;
	unsigned char *external;
	char **resource_names;
	struct bundle_bitmap *bitmaps;
	uint32_t bitmap_count;
	struct bundle_samples *samples;
	uint32_t sample_count;
	uint32_t sample_capacity;
	uint32_t bitmaps_base;
	uint32_t bitmaps_end;
	uint32_t samples_base;
	uint32_t samples_end;
	struct tag_validate_file_range ranges[3];
	/* the bitmap last made whole (transcoded or decoded), which the writer
	reads a piece at a time */
	long cached;
	unsigned char *cache;
	uint32_t cache_size;
	/* pixels left out for want of room (said once) */
	int out_of_room;
} mcc_bundle = { 0 };

/* ---------- private code */

static uint32_t bundle_u32(void const *p)
{
	uint32_t value;

	memcpy(&value, p, sizeof(value));
	return value;
}

static void bundle_put_u32(void *p, uint32_t value)
{
	memcpy(p, &value, sizeof(value));
}

static uint32_t bundle_align(uint32_t value, uint32_t alignment)
{
	return value > UINT32_MAX - (alignment - 1) ? UINT32_MAX : (value + alignment - 1) & ~(alignment - 1);
}

static int bundle_power_of_two(long value)
{
	return value > 0 && !(value & (value - 1));
}

/* a block's elements in the tags, or NULL (an empty block's: the tags) */
static void *bundle_block(struct tag_block const *block, uint32_t stride)
{
	struct mcc_runtime *runtime = &mcc_bundle.runtime;

	if (block->count < 0 || (uint32_t)block->count > UINT32_MAX / stride)
		return NULL;
	if (!block->count)
		return runtime->tags;
	return mcc_runtime_pointer(runtime, (uint32_t)(uintptr_t)block->address, (uint32_t)block->count * stride);
}

/* the index entry of a tag, and its root of `bytes` (NULL when it lies
outside the tags) */
static unsigned char *bundle_entry(uint32_t index)
{
	return mcc_bundle.runtime.tag_index + index * BUNDLE_TAG_ENTRY_BYTES;
}

static void *bundle_root(uint32_t index, uint32_t bytes)
{
	return mcc_runtime_pointer(&mcc_bundle.runtime, bundle_u32(bundle_entry(index) + 0x14), bytes);
}

/* a tag's name as the map has it (NULL when it is not in the tags) */
static char const *bundle_map_name(uint32_t index)
{
	struct mcc_runtime *runtime = &mcc_bundle.runtime;
	char const *name = mcc_runtime_pointer(runtime, bundle_u32(bundle_entry(index) + 0x10), 1);
	uint32_t left;

	if (!name)
		return NULL;
	left = runtime->used - (uint32_t)((unsigned char const *)name - runtime->tags);
	return memchr(name, 0, MIN(left, 256u)) ? name : NULL;
}

/* a tag whose pixels or samples are in MCC's shared resource maps: so
marked, and its name kept for finding them there (tag_names.c may give it
another as the menu lists it) */
static int bundle_tag_external(uint32_t index)
{
	char const *name = bundle_map_name(index);
	size_t length;

	mcc_bundle.external[index] = TRUE;
	if (mcc_bundle.resource_names[index])
		return TRUE;
	if (!name)
		return FALSE;
	length = strlen(name);
	mcc_bundle.resource_names[index] = malloc(length + 1);
	if (!mcc_bundle.resource_names[index])
		return FALSE;
	memcpy(mcc_bundle.resource_names[index], name, length + 1);
	return TRUE;
}

/* ---------- the scenario */

/* the scenario's scripts, recorded animations and cutscene cameras left
out: a bundle never holds it, and those are prepared for a level
(mcc_scripts.c, mcc_tags.c's recordings and cameras), whose checks would
otherwise refuse the map */
static void bundle_scenario_strip(void)
{
	struct mcc_runtime *runtime = &mcc_bundle.runtime;
	uint32_t index = runtime->report.scenario_handle & 0xFFFFu;
	struct scenario *scenario;
	unsigned char *bytes;

	if (index >= runtime->report.tag_count || bundle_u32(bundle_entry(index)) != 'scnr')
		return;
	scenario = bundle_root(index, sizeof(*scenario));
	if (!scenario)
		return;
	memset(&scenario->hs_syntax_data, 0, sizeof(scenario->hs_syntax_data));
	memset(&scenario->hs_string_constants, 0, sizeof(scenario->hs_string_constants));
	scenario->hs_scripts.count = 0;
	scenario->hs_globals.count = 0;
	scenario->hs_references.count = 0;
	scenario->hs_source_files.count = 0;
	/* (its recorded animations and cutscene camera points: mcc_tags.c's
	offsets) */
	bytes = (unsigned char *)scenario;
	bundle_put_u32(bytes + 0x36C, 0);
	bundle_put_u32(bytes + 0x4F0, 0);
}

/* ---------- the HUDs */

/* (mcc_bitmaps.c's mcc_meter_usage) a Gearbox meter (MCC's flag 5 clear:
its channels as Halo PC keeps them) never had the Xbox's RGB opacity tint,
which would draw an authored opacity (Descent's 1) black */
static void bundle_meter_prepare(struct meter_hud_element_definition *meter)
{
	if (!(meter->meter_flags & 0x20))
		meter->opacity = 0.0f;
}

static void bundle_meters_prepare(void)
{
	struct mcc_runtime *runtime = &mcc_bundle.runtime;
	uint32_t index;

	for (index = 0; index < runtime->report.tag_count; index++)
	{
		uint32_t group_tag = bundle_u32(bundle_entry(index));

		if (group_tag == UNIT_HUD_INTERFACE_DEFINITION_TAG)
		{
			struct unit_hud_interface_definition *hud = bundle_root(index, sizeof(*hud));
			struct auxilary_meter_definition *meters;
			long meter_index;

			if (!hud)
				continue;
			bundle_meter_prepare(&hud->shield_meter.meter);
			bundle_meter_prepare(&hud->health_meter.meter);
			meters = bundle_block(&hud->auxilary_meters, sizeof(*meters));
			for (meter_index = 0; meters && meter_index < hud->auxilary_meters.count; meter_index++)
				bundle_meter_prepare(&meters[meter_index].panel.meter);
		}
		else if (group_tag == 'wphi')
		{
			/* (its meters: 0xB4-byte elements, each's meter at 0x24) */
			unsigned char *hud = bundle_root(index, 0x78);
			unsigned char *elements;
			long meter_index;

			if (!hud)
				continue;
			elements = bundle_block((struct tag_block *)(hud + 0x6C), 0xB4);
			for (meter_index = 0; elements && meter_index < ((struct tag_block *)(hud + 0x6C))->count; meter_index++)
				bundle_meter_prepare((struct meter_hud_element_definition *)(elements + meter_index * 0xB4 + 0x24));
		}
	}
}

/* ---------- the bitmaps */

static int bundle_format_bytes(short format)
{
	switch (format)
	{
	case 0: case 1: case 2: case _bitmap_format_p8: return 1;
	case 3: case 6: case 8: case 9: return 2;
	case 10: case _bitmap_format_a8r8g8b8: return 4;
	default: return 0;
	}
}

/* (mcc_bitmaps.c's mcc_bitmap_complete_mips: a DXT1 chain whose tail below
4x4 is sized as width*height/2 keeps its whole-block levels) */
static void bundle_bitmap_complete_mips(struct bitmap_data *bitmap)
{
	uint64_t packed = 0;
	unsigned level;
	unsigned maximum;
	unsigned complete = 0;

	if (bitmap->type != _bitmap_type_2d || bitmap->format != _bitmap_format_dxt1 || bitmap->flags != 0x81 ||
		bitmap->depth != 1 || bitmap->width < 4 || bitmap->height < 4 ||
		!bundle_power_of_two(bitmap->width) || !bundle_power_of_two(bitmap->height))
		return;
	for (maximum = 0; (1L << (maximum + 1)) <= MAX(bitmap->width, bitmap->height); maximum++)
	{
	}
	if (bitmap->mipmap_count != (short)maximum)
		return;
	for (level = 0; level <= maximum; level++)
	{
		unsigned width = MAX((unsigned)bitmap->width >> level, 1u);
		unsigned height = MAX((unsigned)bitmap->height >> level, 1u);

		packed += (uint64_t)width * height / 2;
		if (width >= 4 && height >= 4)
			complete = level;
	}
	if (packed == (uint32_t)bitmap->pixels_size)
		bitmap->mipmap_count = (short)complete;
}

/* the bytes of a bitmap's levels as it lays them out, or 0 when it is not
one this reads */
static uint64_t bundle_bitmap_raw_size(struct bitmap_data const *bitmap)
{
	int bytes_per_pixel = bundle_format_bytes(bitmap->format);
	unsigned faces = bitmap->type == _bitmap_type_cube_map ? 6 : 1;
	uint64_t size = 0;
	unsigned level;

	if (!bytes_per_pixel && (bitmap->format < _bitmap_format_dxt1 || bitmap->format > _bitmap_format_bc7 ||
		bitmap->format == _bitmap_format_p8))
		return 0;
	for (level = 0; level <= (unsigned)bitmap->mipmap_count; level++)
	{
		unsigned width = MAX((unsigned)bitmap->width >> level, 1u);
		unsigned height = MAX((unsigned)bitmap->height >> level, 1u);
		unsigned depth = MAX((unsigned)bitmap->depth >> level, 1u);
		uint64_t slice = bytes_per_pixel ? (uint64_t)width * height * (unsigned)bytes_per_pixel :
			(uint64_t)((width + 3) / 4) * ((height + 3) / 4) * (bitmap->format == _bitmap_format_dxt1 ? 8 : 16);

		size += slice * depth * faces;
	}
	return size;
}

/* a bitmap with no pixels (one not wanted, or not one this can read): the
level adding its tag leaves the tag out (custom_edition_bitmap_drawable) */
static void bundle_bitmap_empty(struct bitmap_data *bitmap, long handle)
{
	bitmap->flags &= (unsigned short)~(MCC_BITMAP_EXTERNAL_BIT | MCC_BITMAP_ENVIRONMENT_BIT | MCC_BITMAP_LIGHTMAP_BIT |
		BITMAP_SWIZZLED_BIT);
	if (bitmap->format == _bitmap_format_bc7)
	{
		bitmap->format = _bitmap_format_dxt5;
		bitmap->flags |= BITMAP_COMPRESSED_BIT;
	}
	bitmap->pixels_offset = 0;
	bitmap->pixels_size = 0;
	bitmap->tag_index = handle;
	bitmap->cache_block_index = NONE;
	bitmap->hardware_format = XBOX_NULL;
	bitmap->base_address = XBOX_NULL;
}

/* a bitmap's pixels given their offset in the bundle, and its descriptor
made Halo PC's (planned: its tag is wanted) */
static void bundle_bitmap_plan(uint32_t tag_index, long handle, short bitmap_index, struct bitmap_data *bitmap,
	int planned)
{
	struct bundle_bitmap *plan = &mcc_bundle.bitmaps[mcc_bundle.bitmap_count];
	uint32_t limit = BUNDLE_OFFSET_LIMIT - BUNDLE_SAMPLES_RESERVE;
	uint64_t raw_size;
	uint64_t size;
	unsigned char transform = _pixels_as_they_are;
	short format = bitmap->format;
	int external = (bitmap->flags & MCC_BITMAP_EXTERNAL_BIT) != 0;
	int non_power_of_two;

	if (external && !bundle_tag_external(tag_index))
		planned = FALSE;
	if (!planned || bitmap->width <= 0 || bitmap->height <= 0 || bitmap->depth <= 0 ||
		bitmap->width > BUNDLE_TEXTURE_SIZE_LIMIT || bitmap->height > BUNDLE_TEXTURE_SIZE_LIMIT ||
		bitmap->type < _bitmap_type_2d || bitmap->type > _bitmap_type_cube_map ||
		bitmap->mipmap_count < 0 || bitmap->mipmap_count > 13 || bitmap->pixels_size <= 0 || bitmap->pixels_offset < 0)
	{
		bundle_bitmap_empty(bitmap, handle);
		return;
	}
	bundle_bitmap_complete_mips(bitmap);
	raw_size = bundle_bitmap_raw_size(bitmap);
	if (!raw_size || raw_size > (uint32_t)bitmap->pixels_size || raw_size > BUNDLE_BITMAP_LIMIT)
	{
		error(_error_silent, "mcc tag bundle: bitmap %d of '%s' is not one this can read (format %d, %dx%dx%d, "
			"%d mipmaps, %ld bytes)", bitmap_index, bundle_map_name(tag_index) ? bundle_map_name(tag_index) : "?",
			bitmap->format, bitmap->width, bitmap->height, bitmap->depth, bitmap->mipmap_count, bitmap->pixels_size);
		bundle_bitmap_empty(bitmap, handle);
		return;
	}
	size = raw_size;
	non_power_of_two = bitmap->type == _bitmap_type_2d &&
		(!bundle_power_of_two(bitmap->width) || !bundle_power_of_two(bitmap->height));
	if (format == _bitmap_format_bc7 && !non_power_of_two && !(bitmap->flags & BITMAP_LINEAR_BIT) &&
		bitmap->width >= 4 && bitmap->height >= 4)
	{
		transform = _pixels_transcoded;
		bitmap->format = _bitmap_format_dxt5;
	}
	else if (non_power_of_two || format == _bitmap_format_bc7)
	{
		/* (its first level, linear: a 2D bitmap alone) */
		size = (uint64_t)(unsigned)bitmap->width * (unsigned)bitmap->height * 4;
		if (bitmap->type != _bitmap_type_2d || bitmap->depth != 1 || size > BUNDLE_BITMAP_LIMIT)
		{
			error(_error_silent, "mcc tag bundle: bitmap %d of '%s' (format %d, type %d, %dx%d) cannot be drawn",
				bitmap_index, bundle_map_name(tag_index) ? bundle_map_name(tag_index) : "?", format, bitmap->type,
				bitmap->width, bitmap->height);
			bundle_bitmap_empty(bitmap, handle);
			return;
		}
		/* (and its power-of-two flag as its sides say, as cache_file_formats.c's
		bitmap_npot_make_linear leaves a Custom Edition map's) */
		transform = _pixels_decoded;
		bitmap->format = _bitmap_format_a8r8g8b8;
		bitmap->mipmap_count = 0;
		bitmap->flags = (unsigned short)((bitmap->flags & ~(BITMAP_COMPRESSED_BIT | BITMAP_PALETTIZED_BIT |
			BITMAP_POWER_OF_TWO_BIT)) | BITMAP_LINEAR_BIT | (non_power_of_two ? 0 : BITMAP_POWER_OF_TWO_BIT));
	}
	if (size > limit - MIN(limit, mcc_bundle.bitmaps_end) ||
		bundle_align(mcc_bundle.bitmaps_end + (uint32_t)size, 16) > limit)
	{
		if (!mcc_bundle.out_of_room)
			error(_error_silent, "mcc tag bundle: the map's bitmaps are past the %lu MB a bundle's offsets hold: "
				"those after '%s' have no pixels", (unsigned long)(limit >> 20),
				bundle_map_name(tag_index) ? bundle_map_name(tag_index) : "?");
		mcc_bundle.out_of_room = TRUE;
		bitmap->format = format;
		bundle_bitmap_empty(bitmap, handle);
		return;
	}
	memset(plan, 0, sizeof(*plan));
	plan->offset = mcc_bundle.bitmaps_end;
	plan->size = (uint32_t)size;
	plan->raw_offset = (uint32_t)bitmap->pixels_offset;
	plan->raw_size = (uint32_t)raw_size;
	plan->tag = (long)tag_index;
	plan->index = bitmap_index;
	plan->width = bitmap->width;
	plan->height = bitmap->height;
	plan->format = format;
	plan->external = (unsigned char)external;
	plan->transform = transform;
	mcc_bundle.bitmap_count++;
	mcc_bundle.bitmaps_end = bundle_align(mcc_bundle.bitmaps_end + (uint32_t)size, 16);

	/* (Halo PC's descriptor, as a Custom Edition map's is prepared:
	cache_file_formats.c's bitmaps_prepare; its compression and palette by
	its format, MCC's flags of where its pixels were gone) */
	bitmap->flags &= (unsigned short)~(MCC_BITMAP_EXTERNAL_BIT | MCC_BITMAP_ENVIRONMENT_BIT | MCC_BITMAP_LIGHTMAP_BIT |
		BITMAP_SWIZZLED_BIT | BITMAP_COMPRESSED_BIT | BITMAP_PALETTIZED_BIT);
	if (bitmap->format >= _bitmap_format_dxt1 && bitmap->format <= _bitmap_format_dxt5)
		bitmap->flags |= BITMAP_COMPRESSED_BIT;
	if (bitmap->format == _bitmap_format_p8)
		bitmap->flags |= BITMAP_PALETTIZED_BIT;
	bitmap->pixels_offset = (long)plan->offset;
	bitmap->pixels_size = (long)plan->size;
	bitmap->tag_index = handle;
	bitmap->cache_block_index = NONE;
	bitmap->hardware_format = XBOX_NULL;
	bitmap->base_address = XBOX_NULL;
}

/* every bitmap's pixels given offsets, the wanted tags' first (the menu goes
on previewing the others' after taking a tag out), while there is room;
FALSE when there is no memory for them */
static int bundle_bitmaps_plan(unsigned char const *wanted, long wanted_count)
{
	struct mcc_runtime *runtime = &mcc_bundle.runtime;
	uint32_t total = 0;
	uint32_t index;
	int pass;

	for (index = 0; index < runtime->report.tag_count; index++)
	{
		struct bitmap_group *group;

		if (bundle_u32(bundle_entry(index)) != BITMAP_GROUP_TAG || !(group = bundle_root(index, sizeof(*group))))
			continue;
		if (group->bitmaps.count > 0 && (uint32_t)group->bitmaps.count < 0x10000u)
			total += (uint32_t)group->bitmaps.count;
	}
	mcc_bundle.bitmaps = malloc((total ? total : 1) * sizeof(*mcc_bundle.bitmaps));
	if (!mcc_bundle.bitmaps)
		return FALSE;
	mcc_bundle.bitmaps_base = bundle_align((uint32_t)MIN(runtime->source.size, (uint64_t)0x50000000u),
		BUNDLE_SPACE_ALIGNMENT);
	mcc_bundle.bitmaps_end = mcc_bundle.bitmaps_base;
	for (pass = 0; pass < 2; pass++)
	for (index = 0; index < runtime->report.tag_count; index++)
	{
		struct bitmap_group *group;
		struct bitmap_data *bitmaps;
		int first = wanted && (long)index < wanted_count && wanted[index];
		long handle = (long)bundle_u32(bundle_entry(index) + 0x0C);
		long bitmap_index;

		/* (the wanted first, then the rest; with none wanted, all at once) */
		if (wanted ? (pass == 0) != first : pass != 0)
			continue;
		if (bundle_u32(bundle_entry(index)) != BITMAP_GROUP_TAG || !(group = bundle_root(index, sizeof(*group))))
			continue;
		/* (editor data: the pixels are each bitmap's, in the bundle's
		offsets) */
		memset(&group->import_bitmap, 0, sizeof(group->import_bitmap));
		memset(&group->pixel_data, 0, sizeof(group->pixel_data));
		if (group->bitmaps.count <= 0 || (uint32_t)group->bitmaps.count >= 0x10000u ||
			!(bitmaps = bundle_block(&group->bitmaps, sizeof(*bitmaps))))
			continue;
		for (bitmap_index = 0; bitmap_index < group->bitmaps.count && mcc_bundle.bitmap_count < total; bitmap_index++)
			bundle_bitmap_plan(index, handle, (short)bitmap_index, &bitmaps[bitmap_index], TRUE);
	}
	return TRUE;
}

/* `bytes` of a bitmap's pixels as its map has them, at `offset` of them */
static int bundle_bitmap_raw_read(struct bundle_bitmap const *plan, uint32_t offset, uint32_t bytes, void *out)
{
	struct mcc_runtime *runtime = &mcc_bundle.runtime;

	if (offset > plan->raw_size || bytes > plan->raw_size - offset)
		return FALSE;
	if (plan->external)
	{
		char const *name = mcc_bundle.resource_names[plan->tag];
		char resource_name[288];
		char group_name[288];

		if (!name)
			return FALSE;
		snprintf(resource_name, sizeof(resource_name), "%s_%u", name, (unsigned)plan->index);
		snprintf(group_name, sizeof(group_name), "%s__pixels", name);
		return mcc_resources_read(runtime, 1, resource_name, plan->raw_offset + offset, bytes, out, group_name);
	}
	return plan->raw_offset + (uint64_t)offset + bytes <= runtime->source.size &&
		mcc_runtime_read(runtime, plan->raw_offset + offset, bytes, out);
}

/* a bitmap's pixels made whole (transcoded or decoded, or a resource of
bitmaps.map read whole), the one kept */
static unsigned char const *bundle_bitmap_made(long plan_index)
{
	struct bundle_bitmap const *plan = &mcc_bundle.bitmaps[plan_index];
	unsigned char *raw;

	if (mcc_bundle.cached == plan_index)
		return mcc_bundle.cache;
	if (mcc_bundle.cache)
		free(mcc_bundle.cache);
	mcc_bundle.cache = NULL;
	mcc_bundle.cached = NONE;
	raw = malloc(plan->raw_size);
	if (!raw || !bundle_bitmap_raw_read(plan, 0, plan->raw_size, raw))
	{
		if (raw)
			free(raw);
		return NULL;
	}
	if (plan->transform == _pixels_as_they_are)
		mcc_bundle.cache = raw;
	else if (plan->transform == _pixels_transcoded)
	{
		mcc_bitmaps_bc7_transcode(raw, plan->raw_size);
		mcc_bundle.cache = raw;
	}
	else
	{
		unsigned char *decoded = malloc(plan->size);

		if (decoded && !mcc_bitmaps_decode_texels(raw, plan->raw_size, (uint32_t *)decoded, (unsigned)plan->width,
			(unsigned)plan->height, plan->format))
		{
			free(decoded);
			decoded = NULL;
		}
		free(raw);
		mcc_bundle.cache = decoded;
	}
	if (mcc_bundle.cache)
		mcc_bundle.cached = plan_index;
	return mcc_bundle.cache;
}

/* the bitmap whose pixels hold `size` bytes at `offset`, or NULL */
static long bundle_bitmap_find(uint32_t offset, uint32_t size)
{
	long low = 0;
	long high = (long)mcc_bundle.bitmap_count - 1;

	while (low <= high)
	{
		long middle = (low + high) / 2;
		struct bundle_bitmap const *plan = &mcc_bundle.bitmaps[middle];

		if (offset < plan->offset)
			high = middle - 1;
		else if (offset - plan->offset >= MAX(plan->size, 1u))
			low = middle + 1;
		else
			return size <= plan->size - (offset - plan->offset) ? middle : NONE;
	}
	return NONE;
}

/* ---------- the sounds */

/* `size` bytes of samples kept at the next of the bundle's sample offsets:
that offset, or 0 when there is no room (the bytes are let go) */
static uint32_t bundle_samples_add(unsigned char *bytes, uint32_t size)
{
	uint32_t offset = mcc_bundle.samples_end;

	if (!size || size > BUNDLE_OFFSET_LIMIT - offset || bundle_align(offset + size, 16) > BUNDLE_OFFSET_LIMIT)
	{
		free(bytes);
		return 0;
	}
	if (mcc_bundle.sample_count == mcc_bundle.sample_capacity)
	{
		uint32_t capacity = mcc_bundle.sample_capacity ? mcc_bundle.sample_capacity * 2 : 64;
		struct bundle_samples *grown = malloc(capacity * sizeof(*grown));

		if (!grown)
		{
			free(bytes);
			return 0;
		}
		if (mcc_bundle.samples)
		{
			memcpy(grown, mcc_bundle.samples, mcc_bundle.sample_count * sizeof(*grown));
			free(mcc_bundle.samples);
		}
		mcc_bundle.samples = grown;
		mcc_bundle.sample_capacity = capacity;
	}
	mcc_bundle.samples[mcc_bundle.sample_count].offset = offset;
	mcc_bundle.samples[mcc_bundle.sample_count].size = size;
	mcc_bundle.samples[mcc_bundle.sample_count].bytes = bytes;
	mcc_bundle.sample_count++;
	mcc_bundle.samples_end = bundle_align(offset + size, 16);
	return offset;
}

/* a permutation's samples made Xbox ADPCM (`made`: of a sound wanted) and
given the bundle's offset, or none */
static void bundle_permutation_prepare(uint32_t tag_index, long handle, struct sound_definition const *sound,
	long range_index, long permutation_index, struct sound_permutation *permutation, int made)
{
	struct mcc_runtime *runtime = &mcc_bundle.runtime;
	int external = (permutation->samples.pad & 1) != 0;
	int channels = sound->encoding + 1;
	uint32_t rate = sound->sample_rate == 1 ? 44100 : 22050;
	uint32_t size = permutation->samples.size > 0 ? (uint32_t)permutation->samples.size : 0;
	unsigned char *data = NULL;
	unsigned char *samples = NULL;
	uint32_t samples_size = 0;
	uint32_t offset = 0;
	int read = FALSE;
	int kept = FALSE;

	if (external)
		bundle_tag_external(tag_index);
	permutation->unknown0 = NONE;
	permutation->unknown1 = 0;
	permutation->unknown2 = (unsigned long)handle;
	permutation->unknown3 = (unsigned long)handle;
	permutation->sample_buffer_size = 0;
	permutation->samples.address = XBOX_NULL;
	permutation->samples.definition = XBOX_NULL;
	if (made && size && size <= 0x4000000u && sound->sample_rate != NONE && (data = malloc(size)) != NULL)
	{
		if (external)
		{
			char const *name = mcc_bundle.resource_names[tag_index];
			char resource_name[288];
			char group_name[288];

			if (name)
			{
				snprintf(resource_name, sizeof(resource_name), "%s__%ld__%ld", name, range_index, permutation_index);
				snprintf(group_name, sizeof(group_name), "%s__permutations", name);
				read = mcc_resources_read(runtime, 2, resource_name, (uint32_t)permutation->samples.file_offset, size,
					data, group_name);
			}
		}
		else
		{
			read = mcc_runtime_read(runtime, (uint32_t)permutation->samples.file_offset, size, data);
		}
		samples = read ? mcc_audio_bundle_samples(data, size, permutation->compression, channels, rate, &samples_size,
			&kept) : NULL;
		if (kept)
		{
			samples = data;
			samples_size = size;
			data = NULL;
		}
		if (data)
			free(data);
		if (samples)
			offset = bundle_samples_add(samples, samples_size);
		if (!offset)
		{
			error(_error_silent, "mcc tag bundle: '%s', range %ld, permutation %ld (codec %d) cannot be made Xbox ADPCM: "
				"it is silent", bundle_map_name(tag_index) ? bundle_map_name(tag_index) : "?", range_index,
				permutation_index, permutation->compression);
			samples_size = 0;
		}
	}
	permutation->samples.pad &= ~1UL;
	permutation->samples.file_offset = offset ? (long)offset : 0;
	permutation->samples.size = offset ? (long)samples_size : 0;
	permutation->compression = 1;
}

/* every sound's permutations given the bundle's samples (made of the sounds
wanted, none of the others) */
static void bundle_sounds_prepare(unsigned char const *wanted, long wanted_count)
{
	struct mcc_runtime *runtime = &mcc_bundle.runtime;
	uint32_t index;
	uint32_t made_count = 0;

	mcc_bundle.samples_base = bundle_align(MAX(mcc_bundle.bitmaps_end, mcc_bundle.bitmaps_base + 16),
		BUNDLE_SPACE_ALIGNMENT);
	mcc_bundle.samples_end = mcc_bundle.samples_base;
	for (index = 0; index < runtime->report.tag_count; index++)
	{
		struct sound_definition *sound;
		struct sound_pitch_range *ranges;
		long handle = (long)bundle_u32(bundle_entry(index) + 0x0C);
		int made = wanted && (long)index < wanted_count && wanted[index];
		long range_index;
		uint32_t output_rate;

		if (bundle_u32(bundle_entry(index)) != SOUND_DEFINITION_TAG || !(sound = bundle_root(index, sizeof(*sound))))
			continue;
		if (sound->encoding < 0 || sound->encoding > 1 || sound->pitch_ranges.count < 0 ||
			!(ranges = bundle_block(&sound->pitch_ranges, sizeof(*ranges))))
		{
			sound->pitch_ranges.count = 0;
			continue;
		}
		for (range_index = 0; range_index < sound->pitch_ranges.count; range_index++)
		{
			struct sound_pitch_range *range = &ranges[range_index];
			struct sound_permutation *permutations = bundle_block(&range->permutations, sizeof(*permutations));
			long permutation_index;

			if (!permutations)
			{
				range->permutations.count = 0;
				continue;
			}
			for (permutation_index = 0; permutation_index < range->permutations.count; permutation_index++)
			{
				bundle_permutation_prepare(index, handle, sound, range_index, permutation_index,
					&permutations[permutation_index], made);
			}
		}
		/* (as this build plays them: mono at 22,050 Hz, stereo at its own) */
		output_rate = sound->encoding == 0 ? 22050 : sound->sample_rate == 1 ? 44100 : 22050;
		sound->compression = 1;
		sound->sample_rate = output_rate == 44100 ? 1 : 0;
		made_count += made;
	}
	if (made_count)
		error(_error_silent, "mcc tag bundle: %lu sounds made Xbox ADPCM, %lu bytes", (unsigned long)made_count,
			(unsigned long)(mcc_bundle.samples_end - mcc_bundle.samples_base));
}

/* ---------- reads */

static boolean bundle_read(void *context, unsigned long offset, unsigned long size, void *buffer)
{
	struct mcc_runtime *runtime = &mcc_bundle.runtime;
	uint32_t at = (uint32_t)offset;
	uint32_t bytes = (uint32_t)size;
	long found;

	(void)context;
	if (!mcc_bundle.open || offset > UINT32_MAX || size > UINT32_MAX)
		return FALSE;
	if (at < mcc_bundle.bitmaps_base)
		return (uint64_t)at + bytes <= runtime->source.size && mcc_runtime_read(runtime, at, bytes, buffer);
	if (at < mcc_bundle.samples_base)
	{
		struct bundle_bitmap const *plan;
		unsigned char const *made;

		found = bundle_bitmap_find(at, bytes);
		if (found == NONE)
			return FALSE;
		plan = &mcc_bundle.bitmaps[found];
		/* (a resource of bitmaps.map is read whole, from its start: its
		pieces from the bitmap made, mcc_resources.c's mcc_resource_read) */
		if (plan->transform == _pixels_as_they_are && !plan->external)
			return bundle_bitmap_raw_read(plan, at - plan->offset, bytes, buffer);
		made = bundle_bitmap_made(found);
		if (!made)
			return FALSE;
		memcpy(buffer, made + (at - plan->offset), bytes);
		return TRUE;
	}
	{
		long low = 0;
		long high = (long)mcc_bundle.sample_count - 1;

		while (low <= high)
		{
			long middle = (low + high) / 2;
			struct bundle_samples const *samples = &mcc_bundle.samples[middle];

			if (at < samples->offset)
				high = middle - 1;
			else if (at - samples->offset >= samples->size)
				low = middle + 1;
			else
			{
				if (bytes > samples->size - (at - samples->offset))
					return FALSE;
				memcpy(buffer, samples->bytes + (at - samples->offset), bytes);
				return TRUE;
			}
		}
	}
	return !bytes;
}

/* ---------- the tag window */

/* the tag window taken, at the base the tags were written for or elsewhere
(their addresses then moved there: halo_port_window.h), as mcc_cache.c takes
a level's */
static int bundle_window_reserve(void)
{
	struct mcc_runtime *runtime = &mcc_bundle.runtime;

	runtime->capacity = MCC_CACHE_TAG_CAPACITY;
	if (!config_boolean("debug.mcc_elsewhere"))
		runtime->tags = mcc_memory_reserve(runtime->report.tag_base, runtime->capacity);
	if (!runtime->tags)
	{
		runtime->tags = mcc_memory_reserve_elsewhere(runtime->report.tag_base, runtime->capacity);
		if (runtime->tags)
			platform_mcc_shift = (unsigned long)mcc_address(runtime->tags) - runtime->report.tag_base;
	}
	if (runtime->tags)
		platform_mcc_linked_base = runtime->report.tag_base;
	return runtime->tags != NULL;
}

static int bundle_workshop(char const *level_name)
{
	char const *package = strchr(level_name, '\\');
	char const *end = package ? strchr(package + 1, '\\') : NULL;
	char const *character;

	if (!end || end == package + 1)
		return FALSE;
	for (character = package + 1; character < end; character++)
	{
		if (*character < '0' || *character > '9')
			return FALSE;
	}
	return TRUE;
}

/* ---------- public code */

int mcc_tag_bundle_opened(void)
{
	return mcc_bundle.open;
}

void *mcc_tag_bundle_open(char const *level_name, unsigned char const *wanted, long wanted_count)
{
	struct mcc_runtime *runtime = &mcc_bundle.runtime;
	struct mcc_cache_identity identity;
	enum mcc_cache_status status;
	unsigned char *raw = NULL;
	unsigned char *header;
	char const *problem = "it cannot be opened";

	mcc_tag_bundle_close();
	if (mcc_cache_tags_loaded())
	{
		error(_error_silent, "mcc tag bundle: cannot take tags out of '%s' while an MCC map is played",
			level_name ? level_name : "");
		return NULL;
	}
	if (!level_name || !mcc_package_name(mcc_identity_local_name(level_name), mcc_bundle.level_name))
	{
		error(_error_silent, "mcc tag bundle: '%s' is not an MCC map's name", level_name ? level_name : "");
		return NULL;
	}
	memset(runtime, 0, sizeof(*runtime));
	mcc_bundle.cached = NONE;
	mcc_bundle.file = mcc_cache_file_open(level_name, &runtime->source, &identity);
	if (!mcc_bundle.file)
		goto failed;
	snprintf(runtime->level_name, sizeof(runtime->level_name), "%s", mcc_bundle.level_name);
	mcc_bundle.workshop = bundle_workshop(mcc_bundle.level_name);
	problem = "its file is larger than the MCC loader supports";
	if (identity.file_length >= 0x50000000u)
		goto failed;
	problem = "there is no memory to read its tags";
	raw = malloc(identity.tag_data_size ? identity.tag_data_size : 1);
	if (!raw)
		goto failed;
	status = mcc_cache_load(&runtime->source, raw, identity.tag_data_size, &runtime->report);
	if (status != MCC_CACHE_OK)
	{
		error(_error_silent, "mcc tag bundle: '%s': %s (tag %lu, location %08lx)", level_name,
			mcc_cache_status_string(status), (unsigned long)runtime->report.problem_tag,
			(unsigned long)runtime->report.problem_location);
		problem = "its tags cannot be read";
		goto failed;
	}
	problem = "its MCC resource maps cannot be found (debug.txt says which)";
	if (!mcc_resources_select(runtime, raw))
		goto failed;
	problem = "the MCC tag window is taken";
	if (!bundle_window_reserve())
		goto failed;
	memcpy(runtime->tags, raw, identity.tag_data_size);
	runtime->used = identity.tag_data_size;
	runtime->tag_index = runtime->tags + BUNDLE_HEADER_BYTES;
	free(raw);
	raw = NULL;
	mcc_bundle.tag_count = runtime->report.tag_count;
	mcc_bundle.external = malloc(mcc_bundle.tag_count ? mcc_bundle.tag_count : 1);
	mcc_bundle.resource_names = malloc((mcc_bundle.tag_count ? mcc_bundle.tag_count : 1) * sizeof(char *));
	problem = "there is no memory for its tags";
	if (!mcc_bundle.external || !mcc_bundle.resource_names)
		goto failed;
	memset(mcc_bundle.external, 0, mcc_bundle.tag_count ? mcc_bundle.tag_count : 1);
	memset(mcc_bundle.resource_names, 0, (mcc_bundle.tag_count ? mcc_bundle.tag_count : 1) * sizeof(char *));

	/* (its metadata as a level's is made, its scenario's level-only parts
	left out first) */
	bundle_scenario_strip();
	problem = "its tags cannot be converted (debug.txt says why)";
	if (!mcc_hud_bitmaps_prepare(runtime) || !mcc_tags_prepare(runtime))
		goto failed;
	bundle_meters_prepare();
	problem = "there is no memory for its bitmaps";
	if (!bundle_bitmaps_plan(wanted, wanted_count))
		goto failed;
	bundle_sounds_prepare(wanted, wanted_count);

	/* (checked with its header the MCC validator's: no buffers) */
	header = runtime->tags;
	memset(header + BUNDLE_HEADER_MODEL_PART_COUNT_OFFSET, 0, BUNDLE_HEADER_SIGNATURE_OFFSET -
		BUNDLE_HEADER_MODEL_PART_COUNT_OFFSET);
	bundle_put_u32(header + BUNDLE_HEADER_INSTANCES_OFFSET, mcc_address(runtime->tag_index));
	bundle_put_u32(header + BUNDLE_HEADER_TAG_COUNT_OFFSET, runtime->report.tag_count);
	bundle_put_u32(header + BUNDLE_HEADER_SIGNATURE_OFFSET, 'tags');
	/* (the validator reads the header as MCC's/Xbox's, whose signature is at
	0x20, where Custom Edition's has its model data size: written there too
	for the check, the size put in its place after it) */
	bundle_put_u32(header + BUNDLE_HEADER_MODEL_DATA_SIZE_OFFSET, 'tags');
	mcc_bundle.ranges[0].offset = 0;
	mcc_bundle.ranges[0].size = (unsigned long)MIN(runtime->source.size, (uint64_t)0x50000000u);
	mcc_bundle.ranges[1].offset = mcc_bundle.bitmaps_base;
	mcc_bundle.ranges[1].size = mcc_bundle.bitmaps_end - mcc_bundle.bitmaps_base;
	mcc_bundle.ranges[2].offset = mcc_bundle.samples_base;
	mcc_bundle.ranges[2].size = mcc_bundle.samples_end - mcc_bundle.samples_base;
	problem = "its tags are not valid (debug.txt says why)";
	if (!mcc_tags_validate_bundle(runtime, mcc_bundle.ranges, (short)NUMBEROF(mcc_bundle.ranges)))
		goto failed;
	/* (its ownership map no longer needed: the blocks it separated are the
	tags') */
	mcc_validation_dispose(runtime);

	/* (and laid out as a Custom Edition map's: where its models' vertices
	and strips are in its file) */
	bundle_put_u32(header + BUNDLE_HEADER_MODEL_DATA_OFFSET_OFFSET, runtime->report.model_file_offset);
	bundle_put_u32(header + BUNDLE_HEADER_MODEL_INDEX_DATA_OFFSET_OFFSET, runtime->report.model_vertex_bytes);
	bundle_put_u32(header + BUNDLE_HEADER_MODEL_DATA_SIZE_OFFSET, runtime->report.model_bytes);
	mcc_bundle.open = TRUE;
	error(_error_silent, "mcc tag bundle: %s open: %lu tags, %lu bitmaps with %lu MB of pixels%s", mcc_bundle.level_name,
		(unsigned long)runtime->report.tag_count, (unsigned long)mcc_bundle.bitmap_count,
		(unsigned long)((mcc_bundle.bitmaps_end - mcc_bundle.bitmaps_base) >> 20),
		mcc_bundle.workshop ? "" : " (MCC's own map: its tags stay on this machine)");
	return header;

failed:
	error(_error_silent, "mcc tag bundle: cannot take tags out of '%s': %s", level_name, problem);
	if (runtime->failure[0])
		error(_error_silent, "mcc tag bundle: %s", runtime->failure);
	if (raw)
		free(raw);
	mcc_tag_bundle_close();
	return NULL;
}

void mcc_tag_bundle_source(struct tag_bundle_source *source)
{
	memset(source, 0, sizeof(*source));
	if (!mcc_bundle.open)
		return;
	source->tag_header = mcc_bundle.runtime.tags;
	source->loaded_size = (long)mcc_bundle.runtime.used;
	source->read = bundle_read;
	source->context = NULL;
	source->map_name = mcc_bundle.runtime.report.identity.name;
	source->map_build = mcc_bundle.runtime.report.identity.build;
	source->flags = FLAG(_tag_bundle_mcc_bit);
}

int mcc_tag_bundle_own_content(long const *tags, long count)
{
	long index;

	if (!mcc_bundle.open || !mcc_bundle.workshop)
		return TRUE;
	for (index = 0; index < count; index++)
	{
		if (tags[index] < 0 || (uint32_t)tags[index] >= mcc_bundle.tag_count || mcc_bundle.external[tags[index]])
			return TRUE;
	}
	return FALSE;
}

void mcc_tag_bundle_close(void)
{
	struct mcc_runtime *runtime = &mcc_bundle.runtime;
	uint32_t index;

	if (!mcc_bundle.open && !mcc_bundle.file && !runtime->tags && !mcc_bundle.external && !mcc_bundle.bitmaps &&
		!mcc_bundle.samples)
		return;
	/* (the game's free does not take NULL) */
	if (mcc_bundle.cache)
		free(mcc_bundle.cache);
	for (index = 0; index < mcc_bundle.sample_count; index++)
	{
		if (mcc_bundle.samples[index].bytes)
			free(mcc_bundle.samples[index].bytes);
	}
	if (mcc_bundle.samples)
		free(mcc_bundle.samples);
	if (mcc_bundle.bitmaps)
		free(mcc_bundle.bitmaps);
	if (mcc_bundle.resource_names)
	{
		for (index = 0; index < mcc_bundle.tag_count; index++)
		{
			if (mcc_bundle.resource_names[index])
				free(mcc_bundle.resource_names[index]);
		}
		free(mcc_bundle.resource_names);
	}
	if (mcc_bundle.external)
		free(mcc_bundle.external);
	mcc_validation_dispose(runtime);
	mcc_resources_dispose(runtime);
	if (runtime->tags)
	{
		mcc_memory_release(runtime->tags, runtime->capacity);
		platform_mcc_shift = 0;
		platform_mcc_linked_base = 0;
	}
	if (mcc_bundle.file)
		mcc_cache_file_close(mcc_bundle.file);
	memset(&mcc_bundle, 0, sizeof(mcc_bundle));
	mcc_bundle.cached = NONE;
}
