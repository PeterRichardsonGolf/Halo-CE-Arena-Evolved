/* the high-res text laid out by its own fonts (ae_text_layout.c, ae_text_hires.c): the game's fonts' tabular digits
(one advance for all ten, letters left alone), the pen keeping its fraction from one character to the next, a line
starting where its exact width puts it, and the tag's layout kept for a string the high-res text does not draw */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include "../../third_party/stb/stb_truetype.h"
#include "ae_text_hires.h"
#include "ae_text_layout.h"

static int failures;
static int checks;
#define CHECK(c) do { checks++; if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
#define NEAR(a, b) (fabsf((float)(a) - (float)(b)) < 0.001f)

/* (stand-ins for the game: one high-res font, 0, for font tag 7; it has every character but '~'; an 'a' is 3.4
units wide, anything else 5) */
long ae_text_layout_hires_font(long font_index)
{
	return font_index == 7 || font_index == -497614621 ? 0 : -1;
}

int text_hires_covers(long font, unsigned long code)
{
	return font == 0 && code != '~';
}

float text_hires_advance(long font, unsigned long code)
{
	if (font != 0)
		return -1.0f;
	return code == 'a' ? 3.4f : 5.0f;
}

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

static void test_fonts(void)
{
	static const char *const fonts[] = { AE_FONT_DIR "/Overpass-900.ttf", AE_FONT_DIR "/Overpass-750.ttf" };
	int font;

	for (font = 0; font < 2; font++)
	{
		unsigned char *data = read_file(fonts[font]);
		stbtt_fontinfo info;
		int zero_advance = -1;
		unsigned long digit;
		int proportional_one = 0, bearing = 0;

		CHECK(data != NULL);
		if (!data || !stbtt_InitFont(&info, data, stbtt_GetFontOffsetForIndex(data, 0)))
		{
			CHECK(!"font read");
			free(data);
			continue;
		}
		for (digit = '0'; digit <= '9'; digit++)
		{
			int glyph = ae_text_tabular_glyph(&info, digit);

			/* (a tabular glyph of its own, not the proportional one) */
			CHECK(glyph != 0);
			CHECK(glyph != stbtt_FindGlyphIndex(&info, (int)digit));
			if (zero_advance < 0)
				zero_advance = ae_text_font_advance(&info, digit);
			CHECK(ae_text_font_advance(&info, digit) == zero_advance);
		}
		stbtt_GetGlyphHMetrics(&info, stbtt_FindGlyphIndex(&info, '1'), &proportional_one, &bearing);
		CHECK(zero_advance > proportional_one);
		/* (letters and the space are their own glyphs) */
		CHECK(ae_text_tabular_glyph(&info, 'A') == stbtt_FindGlyphIndex(&info, 'A'));
		CHECK(ae_text_tabular_glyph(&info, 'l') == stbtt_FindGlyphIndex(&info, 'l'));
		CHECK(ae_text_tabular_glyph(&info, ' ') == stbtt_FindGlyphIndex(&info, ' '));
		/* (none for a character the font has not) */
		CHECK(ae_text_tabular_glyph(&info, 0xE000) == 0);
		CHECK(ae_text_font_advance(&info, 0xE000) == -1);
		/* (no GSUB table, or one cut short: the glyph as it is, nothing read past its end) */
		CHECK(ae_text_tabular_glyph_in(NULL, 0, 42) == 42);
		{
			int one = stbtt_FindGlyphIndex(&info, '1');
			unsigned char *cut = malloc(12);

			CHECK(cut != NULL);
			if (cut)
			{
				memcpy(cut, data + stbtt__find_table(data, 0, "GSUB"), 12);
				CHECK(ae_text_tabular_glyph_in(cut, 12, one) == one);
				free(cut);
			}
		}
		free(data);
	}
}

static void test_pen(void)
{
	float exact = 0.0f, offset = 1.0f;
	short last = 0;
	int valid = 0;

	/* (3.4 units a character from 10: the pen rounds, the fraction is kept) */
	CHECK(ae_text_pen_advance(10, 3.4f, &exact, &last, &valid, &offset) == 13);
	CHECK(NEAR(offset, 0.0f));
	CHECK(ae_text_pen_advance(13, 3.4f, &exact, &last, &valid, &offset) == 17);
	CHECK(NEAR(offset, 0.4f));
	CHECK(ae_text_pen_advance(17, 3.4f, &exact, &last, &valid, &offset) == 20);
	CHECK(NEAR(offset, -0.2f));
	CHECK(NEAR(exact, 20.2f));
	/* (a pen moved elsewhere, as to a tab stop, starts over there) */
	CHECK(ae_text_pen_advance(50, 3.4f, &exact, &last, &valid, &offset) == 53);
	CHECK(NEAR(offset, 0.0f));

	CHECK(NEAR(ae_text_line_start(1, 100, 200, 2, 50.4f, 247), 247.6f));
	CHECK(NEAR(ae_text_line_start(2, 100, 200, 2, 50.4f, 174), 174.8f));
	CHECK(NEAR(ae_text_line_start(0, 100, 200, 2, 50.4f, 102), 102.0f));
}

static void test_layout(void)
{
	short width = 0;
	short pen;

	/* (a string the font draws: measured and drawn by its advances) */
	ae_text_layout_begin(7, "aaa");
	width = ae_text_layout_advance(0, 'a', 6, width);
	width = ae_text_layout_advance(0, 'a', 6, width);
	width = ae_text_layout_advance(0, 'a', 6, width);
	CHECK(width == 10);
	/* (right-aligned in 0 to 100: the game puts the pen at 90, the exact width 10.2 starts it at 89.8) */
	ae_text_layout_line(1, 0, 100, 0, width, 90);
	pen = ae_text_layout_advance(1, 'a', 6, 90);
	CHECK(pen == 93);
	CHECK(NEAR(ae_text_layout_glyph_offset(6, 3.4f), -0.2f));
	pen = ae_text_layout_advance(1, 'a', 6, pen);
	pen = ae_text_layout_advance(1, 'a', 6, pen);
	CHECK(pen == 100);
	CHECK(NEAR(ae_text_layout_glyph_offset(6, 3.4f), -0.4f));

	/* (the bounds of the last character: its pen, 96.6, to the next, 100; a cut one keeps the tag's) */
	{
		short x0 = 90, x1 = 95;

		ae_text_layout_character_bounds(0, 4, 4, &x0, &x1);
		CHECK(x0 == 96 && x1 == 100);
		x0 = 90;
		x1 = 95;
		ae_text_layout_character_bounds(1, 3, 4, &x0, &x1);
		CHECK(x0 == 90 && x1 == 95);
	}

	/* (a character the font has not: the whole string keeps the tag's widths, the glyph centred as before) */
	ae_text_layout_begin(7, "a~a");
	CHECK(ae_text_layout_advance(0, 'a', 6, 0) == 6);
	ae_text_layout_line(1, 0, 100, 0, 6, 94);
	CHECK(ae_text_layout_advance(1, 'a', 6, 94) == 100);
	CHECK(NEAR(ae_text_layout_glyph_offset(6, 3.4f), 1.3f));
	/* (a font tag's index is a datum, negative as a long) */
	ae_text_layout_begin(-497614621, "aa");
	CHECK(ae_text_layout_advance(1, 'a', 6, 0) == 3);
	/* (a font tag without a high-res font, and the unicode strings') */
	ae_text_layout_begin(3, "aaa");
	CHECK(ae_text_layout_advance(1, 'a', 6, 0) == 6);
	ae_text_layout_begin_unicode(7, L"aa");
	CHECK(ae_text_layout_advance(1, 'a', 6, 0) == 3);
	ae_text_layout_begin_unicode(7, L"a~");
	CHECK(ae_text_layout_advance(1, 'a', 6, 0) == 6);
}

static void test_caret(void)
{
	/* (the Xbox keyboard's walk, as virtual_keyboard.c's hook lines do it: "aab", the caret before the b) */
	static wchar_t const text[] = L"aab";
	short width = 0, offset = 0;
	int index;

	ae_text_layout_begin_unicode(7, text);
	for (index = 0; text[index]; index++)
	{
		width = ae_text_layout_advance(0, (unsigned short)text[index], 6, width);
		if (index < 2) offset = width;
	}
	/* (3.4 + 3.4 + 5: 11.8 wide, the caret at 6.8) */
	CHECK(offset == 7);
	CHECK(width == 12);
}

int main(void)
{
	test_fonts();
	test_pen();
	test_layout();
	test_caret();
	printf("%d checks, %d failures\n", checks, failures);
	return failures ? 1 : 0;
}
