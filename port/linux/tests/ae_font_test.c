#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/ae_font.h"

/* stb's own answers, uncached, for comparing ae_font's cached ones with (a second static copy, as ae_font.c has) */
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include "../../third_party/stb/stb_truetype.h"
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static int near(float a, float b, float tolerance)
{
	return fabsf(a - b) <= tolerance;
}

/* a file read whole (cwd is the repo root); NULL when it can't be */
static unsigned char *read_file(const char *path, int *size)
{
	FILE *file = fopen(path, "rb");
	unsigned char *data;
	long length;

	*size = 0;
	if (!file)
		return NULL;
	fseek(file, 0, SEEK_END);
	length = ftell(file);
	fseek(file, 0, SEEK_SET);
	data = length > 0 ? malloc((size_t)length) : NULL;
	if (data && fread(data, 1, (size_t)length, file) != (size_t)length)
	{
		free(data);
		data = NULL;
	}
	fclose(file);
	if (data)
		*size = (int)length;
	return data;
}

/* a code point as stb takes it (as ae_font.c does) */
static int reference_codepoint(unsigned int codepoint)
{
	return codepoint <= 0x10FFFF ? (int)codepoint : 0xFFFD;
}

/* ae_font_advance's answer by stb's code point calls, one at a time */
static float reference_advance(stbtt_fontinfo const *info, float em, unsigned int codepoint, unsigned int previous)
{
	int advance = 0, bearing = 0;

	stbtt_GetCodepointHMetrics(info, reference_codepoint(codepoint), &advance, &bearing);
	if (previous)
		advance += stbtt_GetCodepointKernAdvance(info, reference_codepoint(previous), reference_codepoint(codepoint));
	return (float)advance * stbtt_ScaleForMappingEmToPixels(info, em);
}

/* ae_font_measure's answer by stb's code point calls, one at a time */
static float reference_measure(stbtt_fontinfo const *info, float em, float tracking_em, const char *utf8, float *top,
	float *bottom)
{
	const char *cursor = utf8;
	unsigned int codepoint, previous = 0;
	int units = 0, count = 0, ink_top, ink_bottom = 0, advance, bearing, x0, y0, x1, y1;
	float scale = stbtt_ScaleForMappingEmToPixels(info, em);

	stbtt_GetCodepointBox(info, 'H', &x0, &y0, &x1, &ink_top);
	while ((codepoint = ae_font_utf8_next(&cursor)) != 0)
	{
		stbtt_GetCodepointHMetrics(info, reference_codepoint(codepoint), &advance, &bearing);
		units += advance;
		if (previous)
			units += stbtt_GetCodepointKernAdvance(info, reference_codepoint(previous), reference_codepoint(codepoint));
		if (stbtt_GetCodepointBox(info, reference_codepoint(codepoint), &x0, &y0, &x1, &y1))
		{
			if (y1 > ink_top)
				ink_top = y1;
			if (-y0 > ink_bottom)
				ink_bottom = -y0;
		}
		previous = codepoint;
		count++;
	}
	*top = (float)ink_top * scale;
	*bottom = (float)ink_bottom * scale;
	return (float)units * scale + (count > 1 ? tracking_em * em * (float)(count - 1) : 0.0f);
}

/* every face's measures and advances equal stb's uncached ones exactly, the first time and again (the cache) */
static void check_cached_measures(int face, unsigned char const *data)
{
	static const char *const texts[] =
	{
		"AVAWAY To Wa Yo Te LT", "SIGN IN WITH STEAM",
		"Halo: The Master Chief Collection is in Steam on this PC, but Arena Evolved isn't using its files.",
		"\xC3\x9Cn\xC3\xAF" "c\xC3\xB6" "d\xC3\xA9 \xC3\x80\xC3\x89\xC3\x8E \xC3\x97\xC3\xB7 \xC3\x9F",
		"\xCE\xA9 \xE2\x80\x94 \xE2\x80\xA6 \xE2\x82\xAC \xE2\x98\x85 \xE6\x97\xA5", "\xF0\x9F\x8E\xAE A",
		"\xC3 bad \xFF\x80 bytes", "x", "",
	};
	static const unsigned int extra[] = { 0xA0, 0xE9, 0xFF, 0x100, 0x2014, 0x2026, 0xFFFD, 0x1F3AE, 0x80000000u };
	stbtt_fontinfo info;
	unsigned int codepoints[95 + sizeof(extra) / sizeof(extra[0])];
	int count = 0, pass, text, a, b;

	CHECK(stbtt_InitFont(&info, data, 0));
	for (a = 32; a < 127; a++)
		codepoints[count++] = (unsigned int)a;
	for (a = 0; a < (int)(sizeof(extra) / sizeof(extra[0])); a++)
		codepoints[count++] = extra[a];
	for (pass = 0; pass < 2; pass++)
	{
		for (text = 0; text < (int)(sizeof(texts) / sizeof(texts[0])); text++)
		{
			float top, bottom, want_top, want_bottom;
			float width = ae_font_measure(face, 23.0f, 0.02f, texts[text], &top, &bottom);
			float want = reference_measure(&info, 23.0f, 0.02f, texts[text], &want_top, &want_bottom);

			CHECK(width == want && top == want_top && bottom == want_bottom);
		}
		for (a = 0; a < count; a++)
		{
			CHECK(ae_font_advance(face, 31.0f, codepoints[a], 0) == reference_advance(&info, 31.0f, codepoints[a], 0));
			for (b = 0; b < count; b++)
				CHECK(ae_font_advance(face, 31.0f, codepoints[a], codepoints[b]) ==
					reference_advance(&info, 31.0f, codepoints[a], codepoints[b]));
		}
	}
}

int main(void)
{
	static const unsigned int widget_glyphs[] = { 0x2026, 0x2022, 0x25B2, 0x25BC, 0x25C0, 0x25B6, 0x2039, 0x203A };
	static const char *const files[AE_NUMBER_OF_FACES] =
	{
		"port/assets/fonts/OpenCE-Regular.ttf", "port/assets/fonts/Overpass-900.ttf",
		"port/assets/fonts/Overpass-750.ttf",
	};
	unsigned char *data[AE_NUMBER_OF_FACES];
	int sizes[AE_NUMBER_OF_FACES];
	unsigned char garbage[64];
	unsigned char *bitmap, *copy;
	float width, top, bottom, plain;
	int face, index, w, h, xo, yo;
	const char *c;

	/* before anything is loaded: every face unready, every call 0 / NULL */
	CHECK(!ae_font_ready(AE_FACE_OPENCE) && !ae_font_has(AE_FACE_OPENCE, 'A'));
	CHECK(ae_font_measure(AE_FACE_OPENCE, 60.0f, 0.0f, "X", &top, &bottom) == 0.0f);
	CHECK(!ae_font_ready(-1) && !ae_font_ready(AE_NUMBER_OF_FACES) && ae_font_cap_height(99, 10.0f) == 0.0f);
	CHECK(ae_font_glyph(AE_FACE_OPENCE, 60.0f, 'S', &w, &h, &xo, &yo) == NULL && w == 0 && h == 0);

	for (face = 0; face < AE_NUMBER_OF_FACES; face++)
	{
		data[face] = read_file(files[face], &sizes[face]);
		CHECK(data[face] != NULL);
		if (!data[face])
			return 1;
		CHECK(ae_font_load(face, data[face], sizes[face]));
		CHECK(ae_font_ready(face));
	}

	/* cap heights are em-based: Overpass caps 1400/2000 em, OpenCE's 719/1000 */
	CHECK(near(ae_font_cap_height(AE_FACE_OVERPASS_900, 24.0f), 16.8f, 0.05f));
	CHECK(near(ae_font_cap_height(AE_FACE_OVERPASS_750, 24.0f), 16.8f, 0.05f));
	CHECK(near(ae_font_cap_height(AE_FACE_OPENCE, 60.0f), 43.14f, 0.05f));

	/* the title box rule: "SETTINGS" in OpenCE at 60 holds every glyph bitmap's bottom */
	width = ae_font_measure(AE_FACE_OPENCE, 60.0f, 0.0f, "SETTINGS", &top, &bottom);
	CHECK(width > 0.0f && top >= 43.0f && bottom >= 0.0f);
	for (c = "SETTINGS"; *c; c++)
	{
		bitmap = ae_font_glyph(AE_FACE_OPENCE, 60.0f, (unsigned char)*c, &w, &h, &xo, &yo);
		CHECK(bitmap != NULL && w > 0 && h > 0);
		/* bitmap bottom (y down from the baseline) inside the box */
		CHECK(yo + h <= bottom + 1.0f);
		CHECK(-yo <= top + 1.0f);
		ae_font_free(bitmap);
	}
	/* descenders count */
	ae_font_measure(AE_FACE_OVERPASS_750, 21.0f, 0.0f, "gjpq", &top, &bottom);
	CHECK(bottom > 3.0f);
	/* ... and ink above the capitals raises the top */
	ae_font_measure(AE_FACE_OVERPASS_900, 24.0f, 0.0f, "\xC3\x89", &top, &bottom);   /* E acute */
	CHECK(top > ae_font_cap_height(AE_FACE_OVERPASS_900, 24.0f) + 1.0f);
	/* an empty string: no width, the cap height, nothing below */
	CHECK(ae_font_measure(AE_FACE_OVERPASS_900, 24.0f, 0.1f, "", &top, &bottom) == 0.0f);
	CHECK(near(top, 16.8f, 0.05f) && bottom == 0.0f);

	/* tracking: .09 em after every glyph but the last */
	CHECK(near(ae_font_measure(AE_FACE_OVERPASS_750, 18, 0.09f, "GAME", &top, &bottom),
		ae_font_measure(AE_FACE_OVERPASS_750, 18, 0.0f, "GAME", &top, &bottom) + 3 * 0.09f * 18, 0.01f));
	CHECK(near(ae_font_measure(AE_FACE_OVERPASS_750, 18, 0.09f, "G", &top, &bottom),
		ae_font_advance(AE_FACE_OVERPASS_750, 18, 'G', 0), 0.001f));
	/* the width is the advances with kerning */
	plain = ae_font_advance(AE_FACE_OVERPASS_900, 24.0f, 'A', 0) + ae_font_advance(AE_FACE_OVERPASS_900, 24.0f, 'V', 'A');
	CHECK(near(ae_font_measure(AE_FACE_OVERPASS_900, 24.0f, 0.0f, "AV", &top, &bottom), plain, 0.001f));
	/* sizes scale linearly */
	CHECK(near(ae_font_measure(AE_FACE_OPENCE, 120.0f, 0.0f, "SETTINGS", &top, &bottom), 2.0f * width, 0.01f));

	/* the glyphs the widgets draw from Overpass exist (... bullet, triangles, single guillemets) */
	for (index = 0; index < (int)(sizeof(widget_glyphs) / sizeof(widget_glyphs[0])); index++)
	{
		CHECK(ae_font_has(AE_FACE_OVERPASS_900, widget_glyphs[index]));
		CHECK(ae_font_has(AE_FACE_OVERPASS_750, widget_glyphs[index]));
	}
	CHECK(!ae_font_has(AE_FACE_OVERPASS_900, 0x4E00));
	/* the spec's U+00B7 finding: Overpass marks it as a combining mark (GDEF class 3), so shaping renderers draw it
	with no advance while stb gives it 510/2000 em: AE never draws it (ae_strings_test checks the strings) */
	CHECK(ae_font_advance(AE_FACE_OVERPASS_900, 20.0f, 0xB7, 0) > 4.0f);

	/* invalid UTF-8 bytes count as U+FFFD; a sequence cut short too */
	CHECK(near(ae_font_measure(AE_FACE_OVERPASS_900, 20.0f, 0.0f, "\xFF", &top, &bottom),
		ae_font_advance(AE_FACE_OVERPASS_900, 20.0f, 0xFFFD, 0), 0.001f));
	CHECK(near(ae_font_measure(AE_FACE_OVERPASS_900, 20.0f, 0.0f, "\xE2\x80" "A", &top, &bottom),
		ae_font_advance(AE_FACE_OVERPASS_900, 20.0f, 0xFFFD, 0) + ae_font_advance(AE_FACE_OVERPASS_900, 20.0f, 'A', 0xFFFD),
		0.001f));
	c = "\xE2\x80\xA6" "\xC0\xAF" "Z";
	CHECK(ae_font_utf8_next(&c) == 0x2026);
	CHECK(ae_font_utf8_next(&c) == 0xFFFD && ae_font_utf8_next(&c) == 0xFFFD);   /* overlong: each byte */
	CHECK(ae_font_utf8_next(&c) == 'Z' && ae_font_utf8_next(&c) == 0 && *c == 0);
	/* a surrogate (ED A0 80), an overlong 3-byte form (E0 80 80), past U+10FFFF (F4 90 80 80, F5): U+FFFD for each
	byte, as the lead's second byte already rules them out; a valid 4-byte sequence (U+1F600) whole */
	for (c = "\xED\xA0\x80" "\xE0\x80\x80" "\xF4\x90\x80\x80" "\xF5", index = 0; *c; index++)
		CHECK(ae_font_utf8_next(&c) == 0xFFFD);
	CHECK(index == 11);
	c = "\xF0\x9F\x98\x80" "\xF4\x8F\xBF\xBF" "\xEF\xBF\xBD" "\xDF\xBF";
	CHECK(ae_font_utf8_next(&c) == 0x1F600 && ae_font_utf8_next(&c) == 0x10FFFF);
	CHECK(ae_font_utf8_next(&c) == 0xFFFD && ae_font_utf8_next(&c) == 0x7FF && *c == 0);
	/* (a code point past U+10FFFF handed in directly reads as U+FFFD, never as a negative int) */
	CHECK(near(ae_font_advance(AE_FACE_OVERPASS_900, 20.0f, 0x80000000u, 0),
		ae_font_advance(AE_FACE_OVERPASS_900, 20.0f, 0xFFFD, 0), 0.001f));
	CHECK(near(ae_font_advance(AE_FACE_OVERPASS_900, 20.0f, 'A', 0xFFFFFFFFu),
		ae_font_advance(AE_FACE_OVERPASS_900, 20.0f, 'A', 0xFFFD), 0.001f));
	CHECK(!ae_font_has(AE_FACE_OVERPASS_900, 0x110000));
	bitmap = ae_font_glyph(AE_FACE_OVERPASS_900, 20.0f, 0xFFFFFFFFu, &w, &h, &xo, &yo);
	ae_font_free(bitmap);

	/* the cache answers as stb does; a face loaded again with other data forgets the old data's answers */
	for (face = 0; face < AE_NUMBER_OF_FACES; face++)
		check_cached_measures(face, data[face]);
	CHECK(ae_font_load(AE_FACE_OPENCE, data[AE_FACE_OVERPASS_900], sizes[AE_FACE_OVERPASS_900]));
	check_cached_measures(AE_FACE_OPENCE, data[AE_FACE_OVERPASS_900]);
	CHECK(ae_font_load(AE_FACE_OPENCE, data[AE_FACE_OPENCE], sizes[AE_FACE_OPENCE]));
	check_cached_measures(AE_FACE_OPENCE, data[AE_FACE_OPENCE]);

	/* garbage and truncated data: refused, nothing crashes (built with the sanitizers) */
	memset(garbage, 0xFF, sizeof(garbage));
	CHECK(!ae_font_load(AE_FACE_OPENCE, garbage, 64));
	CHECK(!ae_font_ready(AE_FACE_OPENCE));
	CHECK(ae_font_measure(AE_FACE_OPENCE, 60, 0, "X", &top, &bottom) == 0.0f);
	CHECK(ae_font_advance(AE_FACE_OPENCE, 60, 'X', 0) == 0.0f && ae_font_cap_height(AE_FACE_OPENCE, 60) == 0.0f);
	CHECK(!ae_font_load(AE_FACE_OPENCE, data[AE_FACE_OPENCE], sizes[AE_FACE_OPENCE] / 3));
	CHECK(!ae_font_load(AE_FACE_OPENCE, data[AE_FACE_OPENCE], 11));
	CHECK(!ae_font_load(AE_FACE_OPENCE, NULL, 1000));
	CHECK(!ae_font_load(AE_NUMBER_OF_FACES, data[AE_FACE_OPENCE], sizes[AE_FACE_OPENCE]));
	/* (each truncation of the table directory and of the tables, in a copy just as long, so ASan sees any overread) */
	for (index = 1; index < sizes[AE_FACE_OPENCE]; index += index < 1024 ? 1 : 997)
	{
		copy = malloc((size_t)index);
		memcpy(copy, data[AE_FACE_OPENCE], (size_t)index);
		CHECK(!ae_font_load(AE_FACE_OVERPASS_750, copy, index));
		free(copy);
	}
	CHECK(!ae_font_ready(AE_FACE_OVERPASS_750) && ae_font_ready(AE_FACE_OVERPASS_900));
	/* and loaded again, it works again */
	CHECK(ae_font_load(AE_FACE_OPENCE, data[AE_FACE_OPENCE], sizes[AE_FACE_OPENCE]));
	CHECK(near(ae_font_cap_height(AE_FACE_OPENCE, 60.0f), 43.14f, 0.05f));

	for (face = 0; face < AE_NUMBER_OF_FACES; face++)
		free(data[face]);
	if (failures)
		printf("%d failures\n", failures);
	return failures ? 1 : 0;
}
