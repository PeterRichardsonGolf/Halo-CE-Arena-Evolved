/*
AE_TEXT_DIGITS.C

Same-width digits in the high-res text (ae_text_digits.h).
*/

/* (its own static copy, as text_hires.c and ae_font.c have: the font info text_hires.c passes is the same struct) */
#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include "../../third_party/stb/stb_truetype.h"

#include "ae_text_digits.h"

#include <math.h>

int ae_text_digit_shift(int widest_advance, int own_advance, float pixels_scale)
{
	if (widest_advance <= own_advance)
		return 0;
	return (int)lroundf((float)(widest_advance - own_advance) * 0.5f * pixels_scale);
}

void ae_text_tabular_digit(struct stbtt_fontinfo const *info, unsigned long code, float units_scale, float pixels_scale,
	float *advance, short *left)
{
	int widest = 0, own = 0;
	unsigned long digit;

	if (code < '0' || code > '9' || !info)
		return;
	for (digit = '0'; digit <= '9'; digit++)
	{
		int index = stbtt_FindGlyphIndex(info, (int)digit);
		int digit_advance = 0, bearing = 0;

		if (!index)
			return; /* (a font without all ten digits: left proportional) */
		stbtt_GetGlyphHMetrics(info, index, &digit_advance, &bearing);
		if (digit_advance > widest)
			widest = digit_advance;
		if (digit == code)
			own = digit_advance;
	}
	*advance = (float)widest * units_scale;
	*left = (short)(*left + ae_text_digit_shift(widest, own, pixels_scale));
}
