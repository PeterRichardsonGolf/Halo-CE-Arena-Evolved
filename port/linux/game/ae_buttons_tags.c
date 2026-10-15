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

Custom Edition maps (Halo PC's tags): upstream's ce_hud.c gives the map's
message icons sheet (ui\hud\bitmaps\combined\hud_msg_icons) the Xbox's
sequences and bitmaps, read from ui.map, after the map's own, and points the
HUD globals' button icons at them; it tells AE where they start
(ae_buttons_ce_sheet, its AE hook). Two things follow here:
- the PlayStation sprites of that sheet are drawn over the Xbox's copy (its
  bitmaps after the map's, its sequences past first_sequence; the pixels are
  ui.map's own, so not checked as a map's are);
- the menus' and the carnage report's text icons (ui_widget.c's
  draw_string_and_hack_in_icons), which on the Xbox come from the globals'
  hud_msg_icons_sm, come from that sheet instead (Halo PC's hud_msg_icons_sm
  has none of the Xbox's sequences the button icons name: no icon was drawn),
  at the small sheet's size (ae_buttons_ce_menu_icons, its AE hooks).
*/

#include "cseries.h"
#include "bitmaps/bitmap_group.h"
#include "bitmaps/bitmap_group_lookup.h"
#include "bitmaps/bitmaps.h"
#include "rasterizer/xbox/rasterizer_xbox_hardware_bitmaps.h"
#include "tag_files/tag_groups.h"
#include "interface/hud.h"
#include "interface/hud_definitions.h"
#include "ae_buttons_tags.h"

#include "../src/ae_buttons.h"

/* the platform layer's */
void platform_log(char const *format, ...);
/* cache_files.c's: the loaded map's tags are a Custom Edition map's */
boolean cache_file_tags_are_ce(void);

/* ---------- constants */

enum
{
	MAXIMUM_GLYPHS = 32,
	/* the most message icons sheets ce_hud.c gives the Xbox's icons (its own most) */
	MAXIMUM_CE_SHEETS = 4,
};

/* ui.map's text icons (hud_msg_icons_sm, 16x20) are two thirds of its
message icons' (combined\hud_msg_icons, 24x30): a Custom Edition map's
menus and carnage report draw the latter at the former's size */
#define CE_MENU_ICON_SCALE (2.0f / 3.0f)

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
	/* (a Custom Edition map: the sheet is ce_hud.c's copy of ui.map's, its
	Xbox sequences from ce_first_sequence) */
	boolean ce_copy;
	long ce_first_sequence;
} loaded[MAXIMUM_GLYPHS];

/* the sheets the loaded Custom Edition map's ce_hud.c gave the Xbox's icons:
the group, the first of the Xbox's sequences and of its bitmaps there */
static struct
{
	void const *group;
	long first_sequence;
	long first_bitmap;
} ce_sheets[MAXIMUM_CE_SHEETS];
static long ce_sheet_count;
/* (said once a map) */
static boolean ce_menu_icons_logged;

/* ---------- private code */

static long glyph_count(
	void)
{
	return ae_button_glyph_count < MAXIMUM_GLYPHS ? (long)ae_button_glyph_count : MAXIMUM_GLYPHS;
}

/* the Custom Edition sheet record of the bitmap group, or NULL */
static long ce_sheet_of(
	long group_index)
{
	void const *group;
	long index;

	if (group_index == NONE || !ce_sheet_count || !cache_file_tags_are_ce())
		return NONE;
	group = tag_get(BITMAP_GROUP_TAG, group_index);
	for (index = 0; index < ce_sheet_count; index++)
	{
		if (ce_sheets[index].group == group)
			return index;
	}
	return NONE;
}

static void glyphs_forget(
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

/* ---------- public code */

void ae_buttons_tags_unloaded(
	void)
{
	glyphs_forget();
	/* (the map going: its sheets with it; ce_hud.c tells the next one's after this) */
	ce_sheet_count = 0;
	ce_menu_icons_logged = FALSE;

	return;
}

void ae_buttons_ce_sheet(
	void const *group,
	long first_sequence,
	long first_bitmap)
{
	if (!group || first_sequence < 0 || first_bitmap < 0 || ce_sheet_count >= MAXIMUM_CE_SHEETS)
		return;
	ce_sheets[ce_sheet_count].group = group;
	ce_sheets[ce_sheet_count].first_sequence = first_sequence;
	ce_sheets[ce_sheet_count].first_bitmap = first_bitmap;
	ce_sheet_count++;

	return;
}

long ae_buttons_ce_menu_icons(
	long bitmap_group_index)
{
	long icons = hud_globals ? hud_globals->messaging.messaging_icons.index : NONE;

	if (ce_sheet_of(icons) == NONE)
		return bitmap_group_index;
	if (!ce_menu_icons_logged)
	{
		ce_menu_icons_logged = TRUE;
		platform_log("Custom Edition map: text icons (menus, carnage report) from the message icons' Xbox copy");
	}
	return icons;
}

float ae_buttons_ce_menu_icon_scale(
	long bitmap_group_index)
{
	long icons = hud_globals ? hud_globals->messaging.messaging_icons.index : NONE;

	return bitmap_group_index == icons && ce_sheet_of(icons) != NONE ? CE_MENU_ICON_SCALE : 1.0f;
}

void ae_buttons_tags_loaded(
	void)
{
	long glyph;
	long found = 0;
	long ce_found = 0;

	glyphs_forget();
	for (glyph = 0; glyph < glyph_count(); glyph++)
	{
		const struct ae_button_glyph *embedded = &ae_button_glyphs[glyph];
		long group_index = tag_loaded(BITMAP_GROUP_TAG, embedded->tag);
		long ce_sheet = embedded->sprites ? ce_sheet_of(group_index) : NONE;
		struct bitmap_data *bitmap;

		/* (a Custom Edition map's sheet with the Xbox's copy: the copy's bitmap) */
		if (ce_sheet != NONE)
		{
			bitmap = bitmap_group_try_and_get_bitmap(group_index,
				(short)(ce_sheets[ce_sheet].first_bitmap + embedded->bitmap));
			if (bitmap && (unsigned int)bitmap->width == embedded->width &&
				(unsigned int)bitmap->height == embedded->height)
			{
				loaded[glyph].bitmap = bitmap;
				loaded[glyph].ce_copy = TRUE;
				loaded[glyph].ce_first_sequence = ce_sheets[ce_sheet].first_sequence;
				found++;
				ce_found++;
			}
			continue;
		}
		bitmap = group_index == NONE ? NULL : bitmap_group_try_and_get_bitmap(group_index, (short)embedded->bitmap);

		/* (the map's bitmap of the size the texture was drawn for) */
		if (bitmap && (unsigned int)bitmap->width == embedded->width && (unsigned int)bitmap->height == embedded->height)
		{
			loaded[glyph].bitmap = bitmap;
			found++;
		}
	}
	if (found)
		platform_log("PlayStation buttons: %ld of %ld icons in this map", found, glyph_count());
	if (ce_found)
		platform_log("PlayStation buttons: %ld sheets of the Xbox's icons ui.map gave this Custom Edition map", ce_found);

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
		int sequence = sequence_index;

		if (sheet != bitmap || !ae_button_glyphs[glyph].sprites)
			continue;
		/* (a Custom Edition map's copy of ui.map's sheet: ui.map's own pixels, in
		a texture of ce_hud.c's; its Xbox sequences after the map's) */
		if (loaded[glyph].ce_copy)
		{
			sequence = ae_buttons_ce_sequence(sequence_index, (int)loaded[glyph].ce_first_sequence);
			loaded[glyph].pixels_match = TRUE;
		}
		/* (checked on its pixels in the texture cache, once they are loaded) */
		else if (sheet->cache_block_index == NONE || !sheet->base_address)
			return NULL;
		else if (loaded[glyph].checked_address != (unsigned long)sheet->base_address)
		{
			loaded[glyph].checked_address = (unsigned long)sheet->base_address;
			loaded[glyph].pixels_match = ae_buttons_pixels_match(glyph, (unsigned long)sheet->base_address,
				(unsigned long)bitmap_mipmap_get_pixel_data_size(sheet, 0)) != 0;
		}
		if (!ae_buttons_sprite_wanted(1, ae_button_glyphs[glyph].sprites, sequence,
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
