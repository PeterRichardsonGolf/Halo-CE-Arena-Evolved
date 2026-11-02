/*
AE_FONT.H

The AE menus' own faces (ae_font.c): the look's TrueType fonts, measured and
rasterised with stb_truetype. OpenCE draws the screen titles, Overpass 900
the rows and tabs, Overpass 750 the body text (spec 3).

Sizes are em sizes in pixels (or in any unit: everything here is linear in
the em), the size Pillow and the mockups use: Overpass's capitals are 0.70
em, OpenCE's 0.719 em. Nothing here reads hhea or typo metrics (OpenCE's
descent has the wrong sign there): heights come from the glyphs' own boxes.

No GL, no game, plain types only (the platform's units and the game's both
include it; 64-bit builds rewrite the game's long), so the unit tests build
it alone (port/linux/tests/ae_font_test.c).
*/

#ifndef __AE_FONT_H
#define __AE_FONT_H

enum { AE_FACE_OPENCE, AE_FACE_OVERPASS_900, AE_FACE_OVERPASS_750, AE_NUMBER_OF_FACES };

/* gives a face its TrueType data (kept, not copied); 1 when stb_truetype reads it, else 0 and the face stays
unready (every call below then returns 0 / NULL). The table directory and every table are checked to lie inside the
data first: garbage and truncated fonts are refused, never read past their end */
int ae_font_load(int face, const unsigned char *data, int size);
int ae_font_ready(int face);
int ae_font_has(int face, unsigned int codepoint);
/* sizes are em sizes in pixels (stbtt_ScaleForMappingEmToPixels), never from hhea / typo metrics */
float ae_font_cap_height(int face, float em);   /* the top of 'H' above the baseline */
float ae_font_advance(int face, float em, unsigned int codepoint, unsigned int previous);   /* with kerning */
/* a UTF-8 string: returns its width (advances, kerning, and tracking_em x em after every glyph but the last);
*top = the ink's height above the baseline, at least the cap height; *bottom = the ink's depth below it, at least 0
(both from the glyphs' own boxes, stbtt_GetCodepointBox); invalid UTF-8 bytes count as U+FFFD. top and bottom may
be NULL */
float ae_font_measure(int face, float em, float tracking_em, const char *utf8, float *top, float *bottom);
/* a glyph's coverage (one byte a pixel, width x height) and its top left from the pen on the baseline (y down);
NULL for an empty glyph (a space) or an unready face; ae_font_free frees it */
unsigned char *ae_font_glyph(int face, float em, unsigned int codepoint, int *width, int *height,
	int *x_offset, int *y_offset);
void ae_font_free(unsigned char *bitmap);
/* the next code point of a UTF-8 string, *cursor moved past it; 0 at its end. A byte that starts no valid sequence,
or a sequence cut short, overlong, a surrogate or past U+10FFFF, is U+FFFD (one for each maximal invalid part, as
Unicode recommends): ae_draw lays text out with the same reading */
unsigned int ae_font_utf8_next(const char **cursor);

/* (ae_font_embedded.c, game builds with HALO_GAME_BROWSER) loads the three faces once; a face that fails is logged
("ae draw: <face> not found: drawn with Noto") and ae_draw falls back to the overlay's Noto for it */
void ae_font_embedded_load(void);

#endif
