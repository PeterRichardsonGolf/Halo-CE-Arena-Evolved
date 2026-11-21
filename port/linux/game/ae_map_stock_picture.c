/*
AE_MAP_STOCK_PICTURE.C

A stock map's picture for AE's own screens (ae_map_stock_picture.h).
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "cseries/cseries.h"
#include "bitmaps/bitmaps.h"
#include "bitmaps/bitmap_group.h"
#include "cache/texture_cache.h"
#include "tag_files/tag_groups.h"
#include "halo_ui_map_list.h"
#include "halo_map_families.h"
#include "ae_map_auto_picture_rules.h"
#include "ae_map_stock_picture.h"

#ifdef HALO_MAP_LIST
/* Unmodified iOrange/bcdec, MIT option (see third_party/bcdec/LICENSE). */
#define BCDEC_STATIC
#define BCDEC_IMPLEMENTATION
#include "../../third_party/bcdec/bcdec.h"

enum
{
	/* the frame numbers of halo_ui_map_list.h: the loaded ui.map's own frames below, Halo PC's pictures from here */
	CE_PICTURE_BASE = 0x4000,
	/* the picture's box in its frame (the picture fills the frame's top left; the rest is margin) */
	ART_WIDTH = 140,
	ART_HEIGHT = 114,
	/* bitmap formats of bitmap_group.c (private there) */
	FORMAT_X8R8G8B8 = 10,
	FORMAT_A8R8G8B8 = 11,
	FORMAT_DXT1 = 14,
	FORMAT_DXT3 = 15,
	FORMAT_DXT5 = 16
};

/* the menus' list row of a map as a screen names it: its list name (a stock Xbox map's is its scenario's, levels\test\x\x; Halo
PC's, x@ce), else the plain file's name tried in those forms */
static long find_row(char const *map_name)
{
	char name[128];
	long row = ui_map_list_lookup(map_name);

	if (row != NONE || strchr(map_name, '@') || strchr(map_name, '\\') || strlen(map_name) > 40)
		return row;
	snprintf(name, sizeof(name), "levels\\test\\%s\\%s", map_name, map_name);
	if ((row = ui_map_list_lookup(name)) != NONE)
		return row;
	snprintf(name, sizeof(name), "%s@ce", map_name);
	return ui_map_list_lookup(name);
}

/* Halo PC's own picture of one of its maps by its file's name ("x", "x@ce"), else NULL */
static struct bitmap_data *pc_picture(char const *map_name)
{
	char file[128];
	char *at;

	snprintf(file, sizeof(file), "%s", map_name);
	if ((at = strchr(file, '@')) != NULL)
		*at = 0;
	return ui_map_list_family_picture(_map_family_custom_edition, file);
}

int ae_map_stock_picture_rgba(char const *list_name, unsigned char **rgba, int *width, int *height, int *retry)
{
	struct bitmap_data *bitmap = NULL;
	unsigned char *out;
	unsigned char const *in;
	long row, frame, tag;
	int x, y, w, h;
	short format;
	char map_name[128];

	*retry = 0;
	/* (an MCC stock map's row is "<file>@mcc": its picture is the file's, the Xbox maps' or Halo PC's: 0.9.3 it asked for the
	name as it was and none matched, so no MCC stock map had one) */
	if (!list_name || !list_name[0] || !ae_map_stock_picture_file(list_name, map_name, sizeof(map_name)) || !map_name[0])
		return 0;
	row = find_row(map_name);
	/* (a row not found: the Xbox maps' by their name in the loaded ui.map, as the menus' rows give theirs) */
	frame = row != NONE ? ui_map_list_picture_index(row) : 0;
	if (row == NONE)
	{
		boolean own = FALSE;

		frame = ui_map_list_xbox_picture(map_name, &own);
		if (!own)
			bitmap = pc_picture(map_name);
		if (!own && !bitmap)
			return 0;
	}
	if (bitmap)
		;
	else if (frame >= CE_PICTURE_BASE)
	{
		/* (Halo PC's own pictures: its 19 maps' and the unknown level's, which is no picture; AE's own files are past these) */
		if (!ae_map_auto_picture_stock_frame((int)frame))
			return 0;
		bitmap = ui_map_list_picture((short)frame);
	}
	else
	{
		boolean own = FALSE;

		/* (the Xbox maps': by the row's name in the loaded ui.map, not the unknown level's; Halo PC's six others, whose row
		stands an Xbox map's picture in when its ui.map is not read, by their own picture) */
		ui_map_list_xbox_picture(map_name, &own);
		if (!own)
		{
			bitmap = pc_picture(map_name);
			if (!bitmap)
				return 0;
		}
		else
		{
			tag = tag_loaded('bitm', "ui\\shell\\bitmaps\\mp_map_grafix");
			if (tag == NONE)
			{
				*retry = 1;
				return 0;
			}
			bitmap = bitmap_group_get_bitmap_from_sequence(tag, 0, (short)frame);
		}
	}
	if (!bitmap)
	{
		*retry = 1;
		return 0;
	}
	/* (the pixels read in, as the menus' drawing asks for them) */
	if (!_texture_cache_bitmap_get_hardware_format(bitmap, TRUE, TRUE) || !bitmap->base_address)
	{
		*retry = 1;
		return 0;
	}
	if (bitmap->type != 0 /* 2D */ || TEST_FLAG(bitmap->flags, 1u << 3 /* swizzled */))
		return 0;
	format = bitmap->format;
	w = bitmap->width < ART_WIDTH ? bitmap->width : ART_WIDTH;
	h = bitmap->height < ART_HEIGHT ? bitmap->height : ART_HEIGHT;
	if (w < 1 || h < 1)
		return 0;
	in = (unsigned char const *)xbox_pointer(bitmap->base_address);
	out = malloc((size_t)w * h * 4);
	if (!out)
		return 0;
	if (format == FORMAT_X8R8G8B8 || format == FORMAT_A8R8G8B8)
	{
		for (y = 0; y < h; y++)
			for (x = 0; x < w; x++)
			{
				unsigned char const *p = in + ((size_t)y * bitmap->width + x) * 4;
				unsigned char *q = out + ((size_t)y * w + x) * 4;

				q[0] = p[2];
				q[1] = p[1];
				q[2] = p[0];
				q[3] = format == FORMAT_X8R8G8B8 ? 255 : p[3];
			}
	}
	else if (format == FORMAT_DXT1 || format == FORMAT_DXT3 || format == FORMAT_DXT5)
	{
		int blocks_wide = (bitmap->width + 3) / 4, step = format == FORMAT_DXT1 ? 8 : 16;

		for (y = 0; y < h; y += 4)
			for (x = 0; x < w; x += 4)
			{
				unsigned char block[64];
				unsigned char const *source = in + ((size_t)(y / 4) * blocks_wide + x / 4) * step;
				int dx, dy;

				if (format == FORMAT_DXT1)
					bcdec_bc1(source, block, 16);
				else if (format == FORMAT_DXT3)
					bcdec_bc2(source, block, 16);
				else
					bcdec_bc3(source, block, 16);
				for (dy = 0; dy < 4 && y + dy < h; dy++)
					for (dx = 0; dx < 4 && x + dx < w; dx++)
						memcpy(out + ((size_t)(y + dy) * w + x + dx) * 4, block + (dy * 4 + dx) * 4, 4);
			}
	}
	else
	{
		free(out);
		return 0;
	}
	*rgba = out;
	*width = w;
	*height = h;
	return 1;
}
#else
/* (a build without the port's map list, the web build, has no rows: no stock picture there) */
int ae_map_stock_picture_rgba(char const *list_name, unsigned char **rgba, int *width, int *height, int *retry)
{
	(void)list_name;
	(void)rgba;
	(void)width;
	(void)height;
	*retry = 0;
	return 0;
}
#endif
