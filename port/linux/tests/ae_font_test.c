#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/ae_font.h"

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
