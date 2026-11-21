/*
AE_STEAM_GLYPHS.C

The Steam Controller's glyphs as drawn images (ae_steam_glyphs.h).
*/

#include <stdlib.h>

#include "ae_draw.h"
#include "ae_steam_glyphs.h"

enum { SIZE = 64 };

static int images[AE_STEAM_GLYPH_COUNT];
static int failed[AE_STEAM_GLYPH_COUNT];

int ae_steam_glyph_image(
	int glyph)
{
	unsigned char *rgba;
	int index;

	if (glyph < 0 || glyph >= AE_STEAM_GLYPH_COUNT || failed[glyph] || !ae_draw_available())
		return 0;
	/* (an image the drawing freed for a newer one is made again) */
	if (images[glyph] && ae_draw_image_live(images[glyph]))
		return images[glyph];
	rgba = malloc(SIZE * SIZE * 4);
	if (!rgba)
		return 0;
	for (index = 0; index < SIZE * SIZE; index++)
	{
		rgba[index * 4] = rgba[index * 4 + 1] = rgba[index * 4 + 2] = 255;
		rgba[index * 4 + 3] = ae_steam_glyph_alpha[glyph][index];
	}
	images[glyph] = ae_draw_image_load(rgba, SIZE, SIZE);
	free(rgba);
	if (!images[glyph])
		failed[glyph] = 1;
	return images[glyph];
}
