/*
AE_BUTTONS.C

Arena Evolved: PlayStation button icons, the platform's side (ae_buttons.h).
From DamnationCE v0.3.41 (CC0), in AE's own files.
*/

#include "ae_buttons.h"
#include "hud_hires.h"
#include "platform.h"
#include "xgpu.h"

#include "zlib_prefixed.h"

#include <string.h>

#ifdef HALO_GAME_BROWSER
/* xinput_sdl.c's (with the game browser, every build but --no-game-browser's) */
int platform_input_scheme(void);
#endif

enum
{
	MAXIMUM_GLYPHS = 32,
	/* the whole icons' textures in the texture cache at once (a few of each
	size: the menus' and the dialogs') */
	MAXIMUM_UPLOADS = 32,
};

static struct
{
	unsigned int texture;
	unsigned long levels;
	int failed;
	/* a sheet's placeholder's D3D Data (ae_buttons_tags.c), or 0 */
	unsigned long placeholder_data;
} glyphs[MAXIMUM_GLYPHS];

static struct
{
	unsigned long data;
	long glyph;
} uploads[MAXIMUM_UPLOADS];
static long upload_count;

static long glyph_count(void)
{
	return ae_button_glyph_count < MAXIMUM_GLYPHS ? (long)ae_button_glyph_count : MAXIMUM_GLYPHS;
}

int ae_buttons_family_now(void)
{
#ifdef HALO_GAME_BROWSER
	return ae_buttons_scheme_family(platform_input_scheme());
#else
	return 0;
#endif
}

int ae_buttons_playstation_now(void)
{
	return ae_buttons_family_now() != AE_BUTTON_FAMILY_OWN;
}

/* the picture drawn for a PlayStation glyph (the one the maps' bitmap is found as) with the pad now: its own, or the
Nintendo glyph of the same bitmap (tag, bitmap and sprites) while a Nintendo pad is the last used; -1: none */
static long drawn_glyph(long glyph)
{
	long index;
	int family = ae_buttons_family_now();

	if (glyph < 0 || glyph >= glyph_count() || ae_button_glyphs[glyph].family != AE_BUTTON_FAMILY_PLAYSTATION)
		return glyph;
	if (ae_buttons_glyph_is_drawn(AE_BUTTON_FAMILY_PLAYSTATION, family))
		return glyph;
	for (index = 0; index < glyph_count(); index++)
	{
		const struct ae_button_glyph *other = &ae_button_glyphs[index];

		if (ae_buttons_glyph_is_drawn(other->family, family) && other->bitmap == ae_button_glyphs[glyph].bitmap &&
			other->sprites == ae_button_glyphs[glyph].sprites && !strcmp(other->tag, ae_button_glyphs[glyph].tag))
		{
			return index;
		}
	}
	return -1;
}

int ae_buttons_pixels_match(long glyph, unsigned long address, unsigned long level0_size)
{
	if (glyph < 0 || glyph >= glyph_count() || !address || !level0_size)
		return 0;
#ifdef HALO_64BIT
	/* (an Xbox address: the pixels in the contiguous memory) */
	return crc32(0L, (const Bytef *)xbox_pointer(address), (uInt)level0_size) == ae_button_glyphs[glyph].crc;
#else
	return crc32(0L, (const Bytef *)address, (uInt)level0_size) == ae_button_glyphs[glyph].crc;
#endif
}

int ae_buttons_glyph_ready(long glyph)
{
	const struct ae_button_glyph *embedded;

	/* (a PlayStation glyph asked for while a Nintendo pad is used: the Nintendo one of the bitmap) */
	glyph = drawn_glyph(glyph);
	if (glyph < 0 || glyph >= glyph_count() || glyphs[glyph].failed)
		return 0;
	if (glyphs[glyph].texture)
		return 1;
	/* (decoded only once a PlayStation controller is used: a keyboard or
	Xbox player never pays for it; AE's own pictures when first drawn, or
	as a map loads: ae_buttons_own_preload) */
	embedded = &ae_button_glyphs[glyph];
	glyphs[glyph].texture = hud_hires_png_texture(embedded->png, embedded->png_size, &glyphs[glyph].levels);
	if (!glyphs[glyph].texture)
	{
		if (embedded->own)
			platform_log("AE pictures: could not decode %s", embedded->tag);
		else if (embedded->family == AE_BUTTON_FAMILY_NINTENDO)
			platform_log("Nintendo buttons: could not decode the texture for %s bitmap %d", embedded->tag,
				embedded->bitmap);
		else
			platform_log("PlayStation buttons: could not decode the texture for %s bitmap %d", embedded->tag,
				embedded->bitmap);
		glyphs[glyph].failed = 1;
		return 0;
	}
	return 1;
}

void ae_buttons_uploaded(unsigned long data, unsigned long address, unsigned long width, unsigned long height,
	unsigned long level0_size)
{
	long glyph = ae_buttons_whole_at(address, width, height);
	long index;

	if (glyph >= 0 && !ae_buttons_pixels_match(glyph, address, level0_size))
		glyph = -1;
	for (index = 0; index < upload_count && uploads[index].data != data; index++)
		;
	if (glyph < 0)
	{
		/* (forgotten: another bitmap's pixels now) */
		if (index < upload_count)
		{
			memmove(&uploads[index], &uploads[index + 1], (size_t)(upload_count - index - 1) * sizeof(uploads[0]));
			upload_count--;
		}
		return;
	}
	if (index == upload_count)
	{
		if (upload_count >= MAXIMUM_UPLOADS)
		{
			/* (full: the oldest goes. The table is in upload order, and
			earlier maps' textures that nothing reused would otherwise fill
			it for good) */
			memmove(&uploads[0], &uploads[1], (size_t)(MAXIMUM_UPLOADS - 1) * sizeof(uploads[0]));
			index = --upload_count;
		}
		upload_count++;
	}
	uploads[index].data = data;
	uploads[index].glyph = glyph;
}

void ae_buttons_register_placeholder(long glyph, const unsigned long *texture)
{
	if (glyph >= 0 && glyph < glyph_count())
		glyphs[glyph].placeholder_data = texture ? texture[1] : 0;
}

/* what the texture whose D3D Data is data is to AE, and its glyph */
static int entry_kind(unsigned long data, long *glyph)
{
	long index;

	if (!data)
		return AE_BUTTONS_NONE;
	for (index = 0; index < glyph_count(); index++)
	{
		if (glyphs[index].placeholder_data == data)
		{
			*glyph = index;
			return AE_BUTTONS_PLACEHOLDER;
		}
	}
	for (index = 0; index < upload_count; index++)
	{
		if (uploads[index].data == data)
		{
			*glyph = uploads[index].glyph;
			return AE_BUTTONS_WHOLE;
		}
	}
	return AE_BUTTONS_NONE;
}

int ae_buttons_drawn(unsigned long data)
{
	long glyph = -1;
	int kind = entry_kind(data, &glyph);

	if (kind == AE_BUTTONS_NONE)
		return 0;
	return ae_buttons_draw_playstation(kind, ae_buttons_playstation_now()) && ae_buttons_glyph_ready(drawn_glyph(glyph));
}

unsigned int ae_buttons_entry_texture(unsigned long data, struct xgpu_texture_description *description)
{
	long glyph = -1;

	if (entry_kind(data, &glyph) == AE_BUTTONS_NONE)
		return 0;
	glyph = drawn_glyph(glyph);
	if (!ae_buttons_glyph_ready(glyph))
		return 0;
	description->levels = glyphs[glyph].levels;
	description->hires = TRUE;
	return glyphs[glyph].texture;
}
