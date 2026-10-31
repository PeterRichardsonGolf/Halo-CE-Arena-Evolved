/*
AE_FONT_EMBEDDED.C

The AE menus' faces from the game's own data (ae_font.h,
ae_font_embedded_load): OpenCE from AE's copy (ae_font_data.c, written by
tools/ae_font_data.py: fonts.json does not list it, so upstream does not
embed it), Overpass 900 and 750 from upstream's embedded fonts
(text_hires_embedded[], tools/embed_assets.py, by file name). Only in game
builds with the game browser, as ae_draw.c.
*/

#ifdef HALO_GAME_BROWSER

#include "ae_font.h"
#include "platform.h"
#include "text_hires.h"

#include <string.h>

/* (ae_font_data.c: the font as little-endian words) */
extern const unsigned int ae_font_opence[];
extern const unsigned int ae_font_opence_size;

/* an upstream embedded font by its file name (NULL: none) */
static const struct text_hires_embedded *embedded_file(const char *file)
{
	unsigned int index;

	for (index = 0; index < text_hires_embedded_count; index++)
	{
		if (text_hires_embedded[index].file && !strcmp(text_hires_embedded[index].file, file))
			return &text_hires_embedded[index];
	}
	return NULL;
}

void ae_font_embedded_load(void)
{
	static const char *const files[AE_NUMBER_OF_FACES] =
	{
		"OpenCE-Regular.ttf", "Overpass-900.ttf", "Overpass-750.ttf",
	};
	static int loaded;
	int face;

	if (loaded)
		return;
	loaded = 1;
	for (face = 0; face < AE_NUMBER_OF_FACES; face++)
	{
		const unsigned char *data = NULL;
		int size = 0;

		if (face == AE_FACE_OPENCE)
		{
			data = (const unsigned char *)ae_font_opence;
			size = (int)ae_font_opence_size;
		}
		else
		{
			const struct text_hires_embedded *embedded = embedded_file(files[face]);

			if (embedded)
			{
				data = (const unsigned char *)embedded->data;
				size = (int)embedded->size;
			}
		}
		if (!data || !ae_font_load(face, data, size))
			platform_log("ae draw: %s not found: drawn with Noto", files[face]);
	}
}

#endif
