/*
AE_BUTTONS_TAGS.C

Arena Evolved: PlayStation button icons, the game's side (port/linux/src/
ae_buttons.h; from DamnationCE v0.3.41, CC0). Which bitmaps of the loaded map
the PlayStation textures stand for (ae_button_glyphs[]), found as its tags load
(hud_hires_tags.c's AE hook): a whole icon by its pixels' address
(ae_buttons_whole_at, which the texture cache asks as it uploads them), and a
sheet's sprites through a placeholder bitmap of AE's, of the sheet's size and
format (so the same coordinates), made when first needed and deleted with the
map, which the texture cache draws the PlayStation texture for. Upstream's own
placeholders (hud_hires_tags.c, display.high_res_text) are left as they are:
AE's is handed out only while a PlayStation controller is the last used.
*/

#include "cseries.h"
#include "bitmaps/bitmap_group.h"
#include "bitmaps/bitmap_group_lookup.h"
#include "bitmaps/bitmaps.h"
#include "rasterizer/xbox/rasterizer_xbox_hardware_bitmaps.h"
#include "tag_files/tag_groups.h"
#include "ae_buttons_tags.h"

#include "../src/ae_buttons.h"

/* the platform layer's */
void platform_log(char const *format, ...);

/* ---------- constants */

enum
{
	MAXIMUM_GLYPHS = 32,
};

/* ---------- globals */

/* each glyph's bitmap in the loaded map (NULL: not in it); a sheet's
placeholder, and whether its pixels at checked_address are the ones the
texture was drawn for */
static struct
{
	struct bitmap_data *bitmap;
	struct bitmap_data *placeholder;
	boolean placeholder_failed;
	unsigned long checked_address;
	boolean pixels_match;
} loaded[MAXIMUM_GLYPHS];

/* ---------- private code */

static long glyph_count(
	void)
{
	return ae_button_glyph_count < MAXIMUM_GLYPHS ? (long)ae_button_glyph_count : MAXIMUM_GLYPHS;
}

/* ---------- public code */

void ae_buttons_tags_unloaded(
	void)
{
	long glyph;

	for (glyph = 0; glyph < MAXIMUM_GLYPHS; glyph++)
	{
		if (loaded[glyph].placeholder)
		{
			ae_buttons_register_placeholder(glyph, NULL);
			bitmap_delete(loaded[glyph].placeholder);
		}
	}
	csmemset(loaded, 0, sizeof(loaded));

	return;
}

void ae_buttons_tags_loaded(
	void)
{
	long glyph;
	long found = 0;

	ae_buttons_tags_unloaded();
	for (glyph = 0; glyph < glyph_count(); glyph++)
	{
		const struct ae_button_glyph *embedded = &ae_button_glyphs[glyph];
		long group_index = tag_loaded(BITMAP_GROUP_TAG, embedded->tag);
		struct bitmap_data *bitmap = group_index == NONE ? NULL :
			bitmap_group_try_and_get_bitmap(group_index, (short)embedded->bitmap);

		/* (the map's bitmap of the size the texture was drawn for) */
		if (bitmap && (unsigned int)bitmap->width == embedded->width && (unsigned int)bitmap->height == embedded->height)
		{
			loaded[glyph].bitmap = bitmap;
			found++;
		}
	}
	if (found)
		platform_log("PlayStation buttons: %ld of %ld icons in this map", found, glyph_count());

	return;
}

long ae_buttons_whole_at(
	unsigned long address,
	unsigned long width,
	unsigned long height)
{
	long glyph;

	for (glyph = 0; glyph < glyph_count(); glyph++)
	{
		struct bitmap_data *bitmap = loaded[glyph].bitmap;

		if (bitmap && !ae_button_glyphs[glyph].sprites && bitmap->cache_block_index != NONE &&
			(unsigned long)bitmap->base_address == address && (unsigned long)bitmap->width == width &&
			(unsigned long)bitmap->height == height)
		{
			return glyph;
		}
	}
	return -1;
}

struct bitmap_data const *ae_buttons_sprite_bitmap(
	struct bitmap_data const *bitmap,
	short sequence_index)
{
	long glyph;

	if (!bitmap || !ae_buttons_playstation_now())
		return NULL;
	for (glyph = 0; glyph < glyph_count(); glyph++)
	{
		struct bitmap_data *sheet = loaded[glyph].bitmap;

		if (sheet != bitmap || !ae_button_glyphs[glyph].sprites)
			continue;
		/* (checked on its pixels in the texture cache, once they are loaded) */
		if (sheet->cache_block_index == NONE || !sheet->base_address)
			return NULL;
		if (loaded[glyph].checked_address != (unsigned long)sheet->base_address)
		{
			loaded[glyph].checked_address = (unsigned long)sheet->base_address;
			loaded[glyph].pixels_match = ae_buttons_pixels_match(glyph, (unsigned long)sheet->base_address,
				(unsigned long)bitmap_mipmap_get_pixel_data_size(sheet, 0)) != 0;
		}
		if (!ae_buttons_sprite_wanted(1, ae_button_glyphs[glyph].sprites, sequence_index,
			loaded[glyph].pixels_match) || !ae_buttons_glyph_ready(glyph))
		{
			return NULL;
		}
		if (!loaded[glyph].placeholder && !loaded[glyph].placeholder_failed)
		{
			struct bitmap_data *placeholder = bitmap_2d_new(sheet->width, sheet->height, 0, sheet->format);

			if (placeholder && !rasterizer_bitmap_new(placeholder))
			{
				bitmap_delete(placeholder);
				placeholder = NULL;
			}
			if (placeholder)
			{
				ae_buttons_register_placeholder(glyph,
					(unsigned long const *)xbox_pointer(placeholder->hardware_format));
			}
			else
			{
				platform_log("PlayStation buttons: no placeholder for %s's sprites: drawn as they are",
					ae_button_glyphs[glyph].tag);
				loaded[glyph].placeholder_failed = TRUE;
			}
			loaded[glyph].placeholder = placeholder;
		}
		return loaded[glyph].placeholder;
	}
	return NULL;
}
