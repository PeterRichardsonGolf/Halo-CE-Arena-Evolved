/* same-width digits in the high-res text (ae_text_digits.c), with the font the game draws with: every digit gets the
widest digit's advance, its ink is moved to the middle of that width, and nothing else changes */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include "../../third_party/stb/stb_truetype.h"
#include "ae_text_digits.h"

static int failures;
static int checks;
#define CHECK(c) do { checks++; if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static unsigned char *read_file(const char *path)
{
	FILE *file = fopen(path, "rb");
	unsigned char *data;
	long size;

	if (!file)
		return NULL;
	fseek(file, 0, SEEK_END);
	size = ftell(file);
	fseek(file, 0, SEEK_SET);
	data = malloc((size_t)size);
	if (data && fread(data, 1, (size_t)size, file) != (size_t)size)
	{
		free(data);
		data = NULL;
	}
	fclose(file);
	return data;
}

int main(void)
{
	static const char *const fonts[] = { AE_FONT_DIR "/Overpass-900.ttf", AE_FONT_DIR "/Overpass-750.ttf" };
	int font;

	/* (the pure shift: half the difference, rounded; never to the left) */
	CHECK(ae_text_digit_shift(1336, 836, 0.01f) == 3);
	CHECK(ae_text_digit_shift(1336, 1336, 0.01f) == 0);
	CHECK(ae_text_digit_shift(1000, 1200, 0.01f) == 0);
	for (font = 0; font < 2; font++)
	{
		unsigned char *data = read_file(fonts[font]);
		stbtt_fontinfo info;
		float units_scale = 0.02f, pixels_scale = 0.05f;
		float first_advance = -1.0f;
		unsigned long code;

		CHECK(data != NULL);
		if (!data || !stbtt_InitFont(&info, data, stbtt_GetFontOffsetForIndex(data, 0)))
		{
			free(data);
			continue;
		}
		for (code = '0'; code <= '9'; code++)
		{
			int own = 0, bearing = 0;
			float advance;
			short left = 7;

			stbtt_GetGlyphHMetrics(&info, stbtt_FindGlyphIndex(&info, (int)code), &own, &bearing);
			advance = own * units_scale;
			ae_text_tabular_digit((struct stbtt_fontinfo const *)&info, code, units_scale, pixels_scale, &advance, &left);
			if (first_advance < 0.0f)
				first_advance = advance;
			/* (one width for all ten) */
			CHECK(fabsf(advance - first_advance) < 0.0001f);
			CHECK(advance >= own * units_scale);
			/* (the ink moved right by half the space it gained) */
			CHECK(abs(left - 7 - (int)lroundf((advance / units_scale - own) * 0.5f * pixels_scale)) <= 0);
		}
		{
			/* (a letter is left alone) */
			int own = 0, bearing = 0;
			float advance;
			short left = 7;

			stbtt_GetGlyphHMetrics(&info, stbtt_FindGlyphIndex(&info, 'A'), &own, &bearing);
			advance = own * units_scale;
			ae_text_tabular_digit((struct stbtt_fontinfo const *)&info, 'A', units_scale, pixels_scale, &advance, &left);
			CHECK(advance == own * units_scale && left == 7);
		}
		free(data);
	}
	printf("%s: %d checks, %d failed\n", __FILE__, checks, failures);
	return failures ? 1 : 0;
}
