/*
AE_FONT.C

The AE menus' own faces (ae_font.h): TrueType data measured and rasterised
with stb_truetype (port/third_party/stb), included as text_hires.c includes
it (static, so it never meets the overlay's or text_hires.c's copy). Built
in every build and in the unit tests: no GL, no game; the data come from
ae_font_embedded.c (the game) or the test.

stb_truetype trusts its data: it reads the table directory and the tables
where they say they are. ae_font_load first checks that the directory and
every table it lists lie inside the data, that the tables stb reads are
long enough for the glyph count, and that every glyph's outline lies inside
'glyf', so a garbage or truncated font is refused instead of read past its
end. (A font whose tables are in bounds but whose contents lie, such as a
cmap pointing nowhere, is still trusted: AE's fonts are its own.)
*/

#include "ae_font.h"

#include <math.h>
#include <stddef.h>
#include <stdlib.h>

/* (stb's many static functions AE does not call would warn as unused: -Wall -Wextra -Werror in the unit tests) */
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

/* the largest em drawn (a bitmap of at most about 4 MB) */
#define MAXIMUM_EM 2048.0f

static struct
{
	int ready;
	stbtt_fontinfo info;
	/* the top of 'H' in font units */
	int cap;
} faces[AE_NUMBER_OF_FACES];

/* ---------- checking the data */

static unsigned int be16(const unsigned char *bytes)
{
	return (unsigned int)bytes[0] << 8 | bytes[1];
}

static unsigned int be32(const unsigned char *bytes)
{
	return (unsigned int)bytes[0] << 24 | (unsigned int)bytes[1] << 16 | (unsigned int)bytes[2] << 8 | bytes[3];
}

static int tag_is(const unsigned char *bytes, const char *tag)
{
	return bytes[0] == (unsigned char)tag[0] && bytes[1] == (unsigned char)tag[1] &&
		bytes[2] == (unsigned char)tag[2] && bytes[3] == (unsigned char)tag[3];
}

/* whether a TrueType font's directory and tables lie inside its size bytes (see the file's comment) */
static int data_fits(const unsigned char *data, unsigned int size)
{
	enum { CMAP, HEAD, HHEA, HMTX, MAXP, LOCA, GLYF, REQUIRED };
	static const char *const required[REQUIRED] = { "cmap", "head", "hhea", "hmtx", "maxp", "loca", "glyf" };
	static const unsigned int shortest[REQUIRED] = { 4, 54, 36, 4, 6, 2, 0 };
	unsigned int offsets[REQUIRED] = { 0 }, lengths[REQUIRED] = { 0 };
	int found[REQUIRED] = { 0 };
	unsigned int version, tables, index, glyphs, long_metrics, long_offsets, previous;
	int kind;

	if (!data || size < 12)
		return 0;
	/* (TrueType outlines in a single font: no CFF, no collection) */
	version = be32(data);
	if (version != 0x00010000u && version != 0x74727565u /* 'true' */)
		return 0;
	tables = be16(data + 4);
	if (!tables || size < 12u + 16u * tables)
		return 0;
	for (index = 0; index < tables; index++)
	{
		const unsigned char *record = data + 12 + 16 * index;
		unsigned int offset = be32(record + 8), length = be32(record + 12);

		if (offset > size || length > size - offset)
			return 0;
		for (kind = 0; kind < REQUIRED; kind++)
		{
			if (tag_is(record, required[kind]) && !found[kind])
			{
				found[kind] = 1;
				offsets[kind] = offset;
				lengths[kind] = length;
			}
		}
	}
	for (kind = 0; kind < REQUIRED; kind++)
	{
		if (!found[kind] || lengths[kind] < shortest[kind])
			return 0;
	}
	/* the per-glyph tables stb indexes by glyph: long enough for maxp's count */
	glyphs = be16(data + offsets[MAXP] + 4);
	long_metrics = be16(data + offsets[HHEA] + 34);
	long_offsets = be16(data + offsets[HEAD] + 50);
	if (!glyphs || !long_metrics || long_metrics > glyphs || long_offsets > 1)
		return 0;
	if (lengths[HMTX] < 4u * long_metrics + 2u * (glyphs - long_metrics))
		return 0;
	if (lengths[LOCA] < (glyphs + 1u) * (long_offsets ? 4u : 2u))
		return 0;
	/* every glyph's outline inside glyf (loca's offsets never go back) */
	previous = 0;
	for (index = 0; index <= glyphs; index++)
	{
		const unsigned char *entry = data + offsets[LOCA] + index * (long_offsets ? 4u : 2u);
		unsigned int at = long_offsets ? be32(entry) : be16(entry) * 2u;

		if (at < previous || at > lengths[GLYF])
			return 0;
		previous = at;
	}
	return 1;
}

/* ---------- faces */

static stbtt_fontinfo *face_info(int face)
{
	return face >= 0 && face < AE_NUMBER_OF_FACES && faces[face].ready ? &faces[face].info : NULL;
}

int ae_font_load(int face, const unsigned char *data, int size)
{
	int x0, y0, x1, y1;

	if (face < 0 || face >= AE_NUMBER_OF_FACES)
		return 0;
	faces[face].ready = 0;
	if (size <= 0 || !data_fits(data, (unsigned int)size))
		return 0;
	if (!stbtt_InitFont(&faces[face].info, data, 0))
		return 0;
	/* (the capitals' height places every line: a face without an 'H' is no use to AE) */
	if (!stbtt_GetCodepointBox(&faces[face].info, 'H', &x0, &y0, &x1, &y1) || y1 <= 0)
		return 0;
	faces[face].cap = y1;
	faces[face].ready = 1;
	return 1;
}

int ae_font_ready(int face)
{
	return face_info(face) != NULL;
}

int ae_font_has(int face, unsigned int codepoint)
{
	stbtt_fontinfo *info = face_info(face);

	return info && codepoint <= 0x10FFFF && stbtt_FindGlyphIndex(info, (int)codepoint) != 0;
}

/* a code point as stb takes it (an int): past U+10FFFF, U+FFFD (a huge unsigned would turn negative) */
static int stb_codepoint(unsigned int codepoint)
{
	return codepoint <= 0x10FFFF ? (int)codepoint : 0xFFFD;
}

/* font units to pixels at an em size */
static float scale_of(stbtt_fontinfo const *info, float em)
{
	return stbtt_ScaleForMappingEmToPixels(info, em);
}

float ae_font_cap_height(int face, float em)
{
	stbtt_fontinfo *info = face_info(face);

	return info ? (float)faces[face].cap * scale_of(info, em) : 0.0f;
}

/* a code point's advance in font units, with the kerning after the previous (0: none) */
static int advance_units(stbtt_fontinfo const *info, unsigned int codepoint, unsigned int previous)
{
	int advance = 0, bearing = 0;

	stbtt_GetCodepointHMetrics(info, stb_codepoint(codepoint), &advance, &bearing);
	if (previous)
		advance += stbtt_GetCodepointKernAdvance(info, stb_codepoint(previous), stb_codepoint(codepoint));
	return advance;
}

float ae_font_advance(int face, float em, unsigned int codepoint, unsigned int previous)
{
	stbtt_fontinfo *info = face_info(face);

	return info ? (float)advance_units(info, codepoint, previous) * scale_of(info, em) : 0.0f;
}

unsigned int ae_font_utf8_next(const char **cursor)
{
	const unsigned char *bytes;
	unsigned int codepoint;
	int extra, index;

	if (!cursor || !*cursor || !**cursor)
		return 0;
	bytes = (const unsigned char *)*cursor;
	if (bytes[0] < 0x80)
	{
		*cursor += 1;
		return bytes[0];
	}
	if (bytes[0] >= 0xC2 && bytes[0] <= 0xDF) { codepoint = bytes[0] & 0x1Fu; extra = 1; }
	else if (bytes[0] >= 0xE0 && bytes[0] <= 0xEF) { codepoint = bytes[0] & 0x0Fu; extra = 2; }
	else if (bytes[0] >= 0xF0 && bytes[0] <= 0xF4) { codepoint = bytes[0] & 0x07u; extra = 3; }
	else
	{
		/* (a continuation byte alone, C0/C1, F5..FF) */
		*cursor += 1;
		return 0xFFFD;
	}
	for (index = 1; index <= extra; index++)
	{
		/* (the end of the string stops it too: 0 is no continuation byte) */
		if ((bytes[index] & 0xC0) != 0x80)
		{
			*cursor += index;
			return 0xFFFD;
		}
		/* (the second byte rules out overlong forms, surrogates and code points past U+10FFFF) */
		if (index == 1 && ((bytes[0] == 0xE0 && bytes[1] < 0xA0) || (bytes[0] == 0xED && bytes[1] > 0x9F) ||
			(bytes[0] == 0xF0 && bytes[1] < 0x90) || (bytes[0] == 0xF4 && bytes[1] > 0x8F)))
		{
			*cursor += 1;
			return 0xFFFD;
		}
		codepoint = codepoint << 6 | (bytes[index] & 0x3Fu);
	}
	*cursor += extra + 1;
	return codepoint;
}

float ae_font_measure(int face, float em, float tracking_em, const char *utf8, float *top, float *bottom)
{
	stbtt_fontinfo *info = face_info(face);
	const char *cursor = utf8;
	unsigned int codepoint, previous = 0;
	int units = 0, count = 0, ink_top, ink_bottom = 0, x0, y0, x1, y1;
	float scale;

	if (top)
		*top = 0.0f;
	if (bottom)
		*bottom = 0.0f;
	if (!info)
		return 0.0f;
	ink_top = faces[face].cap;
	while (utf8 && (codepoint = ae_font_utf8_next(&cursor)) != 0)
	{
		units += advance_units(info, codepoint, previous);
		if (stbtt_GetCodepointBox(info, stb_codepoint(codepoint), &x0, &y0, &x1, &y1))
		{
			if (y1 > ink_top)
				ink_top = y1;
			if (-y0 > ink_bottom)
				ink_bottom = -y0;
		}
		previous = codepoint;
		count++;
	}
	scale = scale_of(info, em);
	if (top)
		*top = (float)ink_top * scale;
	if (bottom)
		*bottom = (float)ink_bottom * scale;
	return (float)units * scale + (count > 1 ? tracking_em * em * (float)(count - 1) : 0.0f);
}

unsigned char *ae_font_glyph(int face, float em, unsigned int codepoint, int *width, int *height,
	int *x_offset, int *y_offset)
{
	stbtt_fontinfo *info = face_info(face);
	float scale;

	*width = *height = *x_offset = *y_offset = 0;
	if (!info || !(em > 0.0f) || em > MAXIMUM_EM)
		return NULL;
	scale = scale_of(info, em);
	return stbtt_GetCodepointBitmap(info, scale, scale, stb_codepoint(codepoint), width, height, x_offset, y_offset);
}

void ae_font_free(unsigned char *bitmap)
{
	stbtt_FreeBitmap(bitmap, NULL);
}
