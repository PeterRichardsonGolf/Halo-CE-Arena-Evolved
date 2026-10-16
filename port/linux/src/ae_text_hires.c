/*
AE_TEXT_HIRES.C

The high-res text's fonts as AE lays its text out by them (ae_text_hires.h).
*/

/* (its own static copy, as text_hires.c and ae_font.c have: the font info text_hires.c passes is the same struct) */
#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include "../../third_party/stb/stb_truetype.h"

#include "ae_text_hires.h"

static unsigned int read16(unsigned char const *at)
{
	return (unsigned int)at[0] << 8 | at[1];
}

static unsigned long read32(unsigned char const *at)
{
	return (unsigned long)at[0] << 24 | (unsigned long)at[1] << 16 | (unsigned long)at[2] << 8 | at[3];
}

/* the glyph's place in a coverage table, -1 if it is not in it */
static long coverage_index(unsigned char const *coverage, int glyph)
{
	unsigned int count = read16(coverage + 2);
	unsigned int index;

	if (read16(coverage) == 1)
	{
		for (index = 0; index < count; index++)
		{
			if ((int)read16(coverage + 4 + 2 * index) == glyph)
				return (long)index;
		}
	}
	else if (read16(coverage) == 2)
	{
		for (index = 0; index < count; index++)
		{
			unsigned char const *range = coverage + 4 + 6 * index;

			if (glyph >= (int)read16(range) && glyph <= (int)read16(range + 2))
				return (long)read16(range + 4) + glyph - (long)read16(range);
		}
	}
	return -1;
}

int ae_text_tabular_glyph_in(unsigned char const *data, unsigned long gsub, int glyph)
{
	unsigned char const *table;
	unsigned char const *features;
	unsigned char const *lookups;
	unsigned int feature;

	if (!data || !gsub || glyph <= 0)
		return glyph;
	table = data + gsub;
	features = table + read16(table + 6);
	lookups = table + read16(table + 8);
	for (feature = 0; feature < read16(features); feature++)
	{
		unsigned char const *record = features + 2 + 6 * feature;
		unsigned char const *list;
		unsigned int lookup;

		if (record[0] != 't' || record[1] != 'n' || record[2] != 'u' || record[3] != 'm')
			continue;
		list = features + read16(record + 4);
		for (lookup = 0; lookup < read16(list + 2); lookup++)
		{
			unsigned char const *entry = lookups + read16(lookups + 2 + 2 * read16(list + 4 + 2 * lookup));
			unsigned int type = read16(entry);
			unsigned int subtable;

			for (subtable = 0; subtable < read16(entry + 4); subtable++)
			{
				unsigned char const *single = entry + read16(entry + 6 + 2 * subtable);
				unsigned int single_type = type;
				long index;

				if (type == 7)
				{
					single_type = read16(single + 2);
					single += read32(single + 4);
				}
				if (single_type != 1)
					continue;
				index = coverage_index(single + read16(single + 2), glyph);
				if (index < 0)
					continue;
				if (read16(single) == 1)
					return (int)((glyph + (int)(short)read16(single + 4)) & 0xFFFF);
				if (read16(single) == 2 && index < (long)read16(single + 4))
					return (int)read16(single + 6 + 2 * index);
			}
		}
		/* (the first tnum feature record: a font has one per script, all with the same lookups) */
		break;
	}
	return glyph;
}

int ae_text_tabular_glyph(struct stbtt_fontinfo const *info, unsigned long code)
{
	/* (the GSUB tables of the fonts seen: two Overpass weights draw the game's text) */
	static struct
	{
		unsigned char const *data;
		unsigned long gsub;
	} known[8];
	unsigned int slot;
	int glyph;

	if (!info)
		return 0;
	glyph = stbtt_FindGlyphIndex(info, (int)code);
	if (!glyph)
		return 0;
	for (slot = 0; slot < sizeof(known) / sizeof(known[0]); slot++)
	{
		if (known[slot].data == info->data)
			break;
		if (!known[slot].data)
		{
			known[slot].data = info->data;
			known[slot].gsub = stbtt__find_table(info->data, (stbtt_uint32)info->fontstart, "GSUB");
			break;
		}
	}
	if (slot == sizeof(known) / sizeof(known[0]))
		return glyph;
	return ae_text_tabular_glyph_in(info->data, known[slot].gsub, glyph);
}

int ae_text_font_advance(struct stbtt_fontinfo const *info, unsigned long code)
{
	int glyph = ae_text_tabular_glyph(info, code);
	int advance = 0, bearing = 0;

	if (!glyph)
		return -1;
	stbtt_GetGlyphHMetrics(info, glyph, &advance, &bearing);
	return advance;
}
