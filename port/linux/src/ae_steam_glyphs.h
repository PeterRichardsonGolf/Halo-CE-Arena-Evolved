/*
AE_STEAM_GLYPHS.H

The Steam Controller's grip and trackpad pictures for AE's menus (port/assets/steam_pad: Kenney's Input Prompts, CC0).
*/

#ifndef AE_STEAM_GLYPHS_H
#define AE_STEAM_GLYPHS_H

enum
{
	AE_STEAM_GLYPH_L4, AE_STEAM_GLYPH_L5, AE_STEAM_GLYPH_R4, AE_STEAM_GLYPH_R5,
	AE_STEAM_GLYPH_RIGHT_PAD_CLICK, AE_STEAM_GLYPH_QUICK,
	AE_STEAM_GLYPH_COUNT
};

/* each glyph's alpha, 64 by 64 (ae_steam_glyphs_data.c) */
extern const unsigned char *const ae_steam_glyph_alpha[AE_STEAM_GLYPH_COUNT];

/* the glyph as AE's drawing has it (ae_draw_image's id; 0: none, nothing to draw with); white, to be tinted */
int ae_steam_glyph_image(int glyph);

#endif
