/*
AE_TEXT_DIGITS.H

Same-width digits in the high-res text: the TrueType fonts it draws with (Overpass) have proportional digits (a 1 is
about two thirds of a 0), so a column of numbers, as the carnage report's, staggers by a few pixels row to row. The
fonts carry tabular digits as an OpenType feature, which stb_truetype does not apply, so text_hires.c's glyph cache
calls this (an AE hook line) for each glyph it rasterizes: a digit gets the widest digit's advance and its ink is
moved to the middle of that width. Include after stb_truetype.h.
*/

#ifndef AE_TEXT_DIGITS_H
#define AE_TEXT_DIGITS_H

/* a glyph's advance (in the 480 lines' units: font units x units_scale) and the left edge of its bitmap (pixels:
font units x pixels_scale), given its code: a digit's become the widest digit's advance and its bitmap moves right
by half the difference; anything else is left as it is */
void ae_text_tabular_digit(struct stbtt_fontinfo const *info, unsigned long code, float units_scale, float pixels_scale,
	float *advance, short *left);

/* the pixels a digit's bitmap moves right in a widest-digit cell (pure: the unit tests' too) */
int ae_text_digit_shift(int widest_advance, int own_advance, float pixels_scale);

#endif
