/*
AE_WIDE_BITMAPS.C

Arena Evolved: a Custom Edition map's bitmap wider than the device's 4096 is made narrower as it loads.

The Xbox texture description holds a linear texture's width in 12 bits and its pitch in 8 steps of 64 bytes, so the
texture cache refuses a bitmap wider than 4096 (xbox_texture_cache.c: "can't be cached; it isn't drawn") and the
surface using it is drawn with no texture, a flat colour. Remastered map packs have them (Halo Flawless Cowboy
Edition's Danger Canyon: a refined teleporter shield 15360 by 256, 120 frames side by side, with 13 mipmap levels).
A bitmap with mipmaps is drawn from its first level that fits: the levels above are left out (its pixels start
after them; the shield's from 3840 by 64). A plain 32-bit bitmap without mipmaps has its rows averaged by a whole
factor as it is read. Either way the frames, and the U scale that steps through them as a fraction of the width,
are kept.
*/

#include "cseries.h"
#include "bitmaps/bitmap_group.h"
#include "cache/cache_files.h"
#include "tag_files/tag_files.h"
#include "ae_wide_bitmaps.h"
#include "ae_wide_rows.h"

#include <stdlib.h>
#include <string.h>

void platform_log(char const *format, ...);
boolean cache_file_tags_are_ce(void);

/* (bitmap_group.c's enums: private there) */
enum
{
	FORMAT_X8R8G8B8 = 10,
	FORMAT_A8R8G8B8 = 11,
	TYPE_2D = 0,
	MAXIMUM_WIDTH = 4096,
	MAXIMUM_REMEMBERED = 16,
};

/* (factor 0: its top levels left out, nothing to do as it is read) */
struct wide_bitmap
{
	struct bitmap_data *bitmap;
	long original_width, factor, pixels_offset, pixels_size, tag_index, width;
};
static struct wide_bitmap wide[MAXIMUM_REMEMBERED];
static int wide_count;

static struct wide_bitmap *remembered(struct bitmap_data const *bitmap)
{
	int at;

	for (at = 0; at < wide_count; at++)
		if (wide[at].bitmap == bitmap && wide[at].tag_index == bitmap->tag_index &&
			wide[at].pixels_offset == bitmap->pixels_offset && wide[at].pixels_size == bitmap->pixels_size &&
			bitmap->width == wide[at].width)
			return &wide[at];
	return NULL;
}

/* a bitmap with mipmaps: its top levels left out (FALSE: not one this can do) */
static boolean levels_left_out(struct bitmap_data *bitmap)
{
	long width = bitmap->width, height = bitmap->height, skipped_bytes = 0, level_bytes;
	int skip = ae_wide_levels_to_skip(bitmap->width, bitmap->height, bitmap->mipmap_count, MAXIMUM_WIDTH), level;

	if (skip <= 0 || wide_count >= MAXIMUM_REMEMBERED)
		return FALSE;
	for (level = 0; level < skip; level++)
	{
		level_bytes = ae_wide_level_bytes(bitmap->format, width, height);
		if (level_bytes <= 0)
			return FALSE;
		skipped_bytes += level_bytes;
		width = width / 2 < 1 ? 1 : width / 2;
		height = height / 2 < 1 ? 1 : height / 2;
	}
	if (skipped_bytes >= (long)bitmap->pixels_size)
		return FALSE;
	platform_log("wide bitmap: %s %dx%d drawn from its level %d, %ldx%ld", tag_get_name(bitmap->tag_index),
		bitmap->width, bitmap->height, skip, width, height);
	bitmap->width = (short)width;
	bitmap->height = (short)height;
	bitmap->mipmap_count = (short)(bitmap->mipmap_count - skip);
	bitmap->pixels_offset += skipped_bytes;
	bitmap->pixels_size -= skipped_bytes;
	wide[wide_count].bitmap = bitmap;
	wide[wide_count].original_width = width << skip;
	wide[wide_count].factor = 0;
	wide[wide_count].pixels_offset = bitmap->pixels_offset;
	wide[wide_count].pixels_size = bitmap->pixels_size;
	wide[wide_count].tag_index = bitmap->tag_index;
	wide[wide_count].width = bitmap->width;
	wide_count++;
	return TRUE;
}

void ae_wide_bitmap_prepare(struct bitmap_data *bitmap)
{
	long factor;
	boolean wide_format = bitmap->format == FORMAT_X8R8G8B8 || bitmap->format == FORMAT_A8R8G8B8;

	if ((bitmap->width > MAXIMUM_WIDTH || bitmap->height > MAXIMUM_WIDTH) && bitmap->mipmap_count > 0 &&
		bitmap->type == TYPE_2D && bitmap->depth == 1 && cache_file_tags_are_ce() && !remembered(bitmap))
	{
		levels_left_out(bitmap);
		return;
	}

	if (bitmap->width <= MAXIMUM_WIDTH || !wide_format || bitmap->type != TYPE_2D || bitmap->depth != 1 ||
		bitmap->mipmap_count != 0 || bitmap->height < 1 || bitmap->height > MAXIMUM_WIDTH || !cache_file_tags_are_ce() ||
		remembered(bitmap) || (long)bitmap->pixels_size < (long)bitmap->width * bitmap->height * 4)
		return;
	for (factor = 2; factor <= 16; factor++)
		if (bitmap->width % factor == 0 && bitmap->width / factor <= MAXIMUM_WIDTH && (bitmap->width / factor) * 4 % 64 == 0)
			break;
	if (factor > 16 || wide_count >= MAXIMUM_REMEMBERED)
		return;
	wide[wide_count].bitmap = bitmap;
	wide[wide_count].original_width = bitmap->width;
	wide[wide_count].factor = factor;
	wide[wide_count].pixels_offset = bitmap->pixels_offset;
	wide[wide_count].pixels_size = bitmap->pixels_size;
	wide[wide_count].tag_index = bitmap->tag_index;
	wide[wide_count].width = bitmap->width / factor;
	platform_log("wide bitmap: %s %dx%d made %ldx%d (rows averaged by %ld)", tag_get_name(bitmap->tag_index), bitmap->width,
		bitmap->height, bitmap->width / factor, bitmap->height, factor);
	bitmap->width = (short)(bitmap->width / factor);
	wide_count++;
}

boolean ae_wide_bitmap_read(struct bitmap_data *bitmap, void *buffer)
{
	struct wide_bitmap *entry = remembered(bitmap);
	boolean done = FALSE;
	long factor, out_width;

	if (!entry || !entry->factor)
		return FALSE;
	factor = entry->factor;
	out_width = bitmap->width;
	cache_file_read(bitmap->tag_index, bitmap->pixels_offset, entry->original_width * bitmap->height * 4, buffer, &done, TRUE);
	if (done != TRUE)
	{
		memset(buffer, 0, (size_t)(out_width * bitmap->height * 4));
		return TRUE;
	}
	ae_wide_rows_average(buffer, entry->original_width, bitmap->height, factor);
	return TRUE;
}
