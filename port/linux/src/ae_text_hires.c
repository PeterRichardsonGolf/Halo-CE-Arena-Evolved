/*
AE_TEXT_HIRES.C

The high-res text's fonts as AE lays its text out by them (ae_text_hires.h).
*/

/* (its own static copy, as text_hires.c and ae_font.c have: the font info text_hires.c passes is the same struct) */
#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include "../../third_party/stb/stb_truetype.h"

#include "ae_text_hires.h"

/* the GSUB table: its bytes and their count; every read is held inside them */
struct table
{
	unsigned char const *data;
	unsigned long length;
};

/* (0 for a read past the table's end) */
static unsigned int read16(struct table const *table, unsigned long at)
{
	if (at > table->length || table->length - at < 2)
		return 0;
	return (unsigned int)table->data[at] << 8 | table->data[at + 1];
}

static unsigned long read32(struct table const *table, unsigned long at)
{
	return (unsigned long)read16(table, at) << 16 | read16(table, at + 2);
}

/* the glyph's place in the coverage table at coverage, -1 if it is not in it */
static long coverage_index(struct table const *table, unsigned long coverage, int glyph)
{
	unsigned int count = read16(table, coverage + 2);
	unsigned int index;

	if (read16(table, coverage) == 1)
	{
		for (index = 0; index < count; index++)
		{
			if ((int)read16(table, coverage + 4 + 2 * index) == glyph)
				return (long)index;
		}
	}
	else if (read16(table, coverage) == 2)
	{
		for (index = 0; index < count; index++)
		{
			unsigned long range = coverage + 4 + 6 * (unsigned long)index;

			if (glyph >= (int)read16(table, range) && glyph <= (int)read16(table, range + 2))
				return (long)read16(table, range + 4) + glyph - (long)read16(table, range);
		}
	}
	return -1;
}

int ae_text_tabular_glyph_in(unsigned char const *gsub, unsigned long length, int glyph)
{
	struct table table;
	unsigned long features, lookups;
	unsigned int feature;

	if (!gsub || length < 10 || glyph <= 0)
		return glyph;
	table.data = gsub;
	table.length = length;
	features = read16(&table, 6);
	lookups = read16(&table, 8);
	for (feature = 0; feature < read16(&table, features); feature++)
	{
		unsigned long record = features + 2 + 6 * (unsigned long)feature;
		unsigned long list;
		unsigned int lookup;

		if (read32(&table, record) != 0x746E756DUL) /* (tnum) */
			continue;
		list = features + read16(&table, record + 4);
		for (lookup = 0; lookup < read16(&table, list + 2); lookup++)
		{
			unsigned long entry = lookups + read16(&table, lookups + 2 + 2 * (unsigned long)read16(&table, list + 4 + 2 * (unsigned long)lookup));
			unsigned int type = read16(&table, entry);
			unsigned int subtable;

			for (subtable = 0; subtable < read16(&table, entry + 4); subtable++)
			{
				unsigned long single = entry + read16(&table, entry + 6 + 2 * (unsigned long)subtable);
				unsigned int single_type = type;
				long index;

				if (type == 7)
				{
					single_type = read16(&table, single + 2);
					single += read32(&table, single + 4);
				}
				if (single_type != 1)
					continue;
				index = coverage_index(&table, single + read16(&table, single + 2), glyph);
				if (index < 0)
					continue;
				if (read16(&table, single) == 1)
					return (int)((glyph + (int)(short)read16(&table, single + 4)) & 0xFFFF);
				if (read16(&table, single) == 2 && index < (long)read16(&table, single + 4))
					return (int)read16(&table, single + 6 + 2 * (unsigned long)index);
			}
		}
		/* (the first tnum feature record: a font has one per script, all with the same lookups) */
		break;
	}
	return glyph;
}

/* the font's table of a tag: its offset in the font's data and its length, from the font's table records (as
stbtt__find_table reads them, with the length too); 0 if it has none */
static unsigned long font_table(unsigned char const *data, unsigned long fontstart, char const *tag,
	unsigned long *length)
{
	unsigned int count = (unsigned int)data[fontstart + 4] << 8 | data[fontstart + 5];
	unsigned int index;

	for (index = 0; index < count; index++)
	{
		unsigned char const *record = data + fontstart + 12 + 16 * (unsigned long)index;

		if (record[0] == (unsigned char)tag[0] && record[1] == (unsigned char)tag[1] &&
			record[2] == (unsigned char)tag[2] && record[3] == (unsigned char)tag[3])
		{
			*length = (unsigned long)record[12] << 24 | (unsigned long)record[13] << 16 |
				(unsigned long)record[14] << 8 | record[15];
			return (unsigned long)record[8] << 24 | (unsigned long)record[9] << 16 |
				(unsigned long)record[10] << 8 | record[11];
		}
	}
	*length = 0;
	return 0;
}

int ae_text_tabular_glyph(struct stbtt_fontinfo const *info, unsigned long code)
{
	/* (the GSUB tables of the fonts seen: two Overpass weights draw the game's text) */
	static struct
	{
		unsigned char const *data;
		unsigned long gsub, length;
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
			known[slot].gsub = font_table(info->data, (unsigned long)info->fontstart, "GSUB", &known[slot].length);
			break;
		}
	}
	if (slot == sizeof(known) / sizeof(known[0]))
		return glyph;
	if (!known[slot].gsub)
		return glyph;
	return ae_text_tabular_glyph_in(info->data + known[slot].gsub, known[slot].length, glyph);
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
