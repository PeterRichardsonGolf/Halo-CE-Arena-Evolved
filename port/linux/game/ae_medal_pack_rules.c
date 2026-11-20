/*
AE_MEDAL_PACK_RULES.C

The medal pictures' files read as pure rules (ae_medal_pack_rules.h). No engine headers: the unit tests build it alone, under the
sanitizers, as the files are the player's and may be cut short or changed by an update.
*/

#include "ae_medal_pack_rules.h"

#include <ctype.h>
#include <string.h>

/* ---------- little endian reads (the caller has checked the bytes are there) */

static unsigned int read16(unsigned char const *bytes)
{
	return (unsigned int)bytes[0] | ((unsigned int)bytes[1] << 8);
}

static unsigned int read32(unsigned char const *bytes)
{
	return read16(bytes) | (read16(bytes + 2) << 16);
}

/* ---------- the index */

/* the offsets inside a record */
enum { RECORD_NAME = 0x04, RECORD_SIZE = 0x34, RECORD_DATA_SIZE = 0x48, RECORD_DATA_OFFSET = 0x50 };

static int is_index(unsigned char const *index, size_t index_size)
{
	return index && index_size >= AE_MEDAL_INDEX_FIRST && read32(index) == AE_MEDAL_INDEX_MAGIC;
}

/* whether the part of record i that is read lies inside the file (64 bits: i is not trusted to be small) */
static int record_inside(size_t index_size, int i)
{
	unsigned long long end;

	if (i < 0 || i >= AE_MEDAL_INDEX_MOST)
		return 0;
	end = (unsigned long long)AE_MEDAL_INDEX_FIRST + (unsigned long long)i * AE_MEDAL_INDEX_STRIDE + AE_MEDAL_INDEX_USED;
	return end <= (unsigned long long)index_size;
}

static int has_medal_prefix(unsigned char const *name)
{
	return !strncmp((char const *)name, "MEDAL_", 6);
}

int ae_medal_index_count(unsigned char const *index, size_t index_size)
{
	int count = 0;

	if (!is_index(index, index_size))
		return 0;
	while (record_inside(index_size, count) &&
		has_medal_prefix(index + AE_MEDAL_INDEX_FIRST + (size_t)count * AE_MEDAL_INDEX_STRIDE + RECORD_NAME))
	{
		count++;
	}
	return count;
}

int ae_medal_index_record(unsigned char const *index, size_t index_size, int i, struct ae_medal_record *record)
{
	unsigned char const *bytes;
	int length;

	if (!record || !is_index(index, index_size) || !record_inside(index_size, i))
		return 0;
	bytes = index + AE_MEDAL_INDEX_FIRST + (size_t)i * AE_MEDAL_INDEX_STRIDE;
	if (!has_medal_prefix(bytes + RECORD_NAME))
		return 0;
	/* (the name field is 48 bytes up to the size: a name that is not ended in it, or has a character that is not printable
	ASCII, is not one) */
	for (length = 0; length < AE_MEDAL_NAME_SIZE && bytes[RECORD_NAME + length]; length++)
	{
		if (bytes[RECORD_NAME + length] < 0x20 || bytes[RECORD_NAME + length] > 0x7E)
			return 0;
	}
	if (length >= AE_MEDAL_NAME_SIZE)
		return 0;
	memset(record, 0, sizeof(*record));
	memcpy(record->name, bytes + RECORD_NAME, (size_t)length);
	record->width = read16(bytes + RECORD_SIZE);
	record->height = read16(bytes + RECORD_SIZE + 2);
	record->size = read32(bytes + RECORD_DATA_SIZE);
	record->offset = (unsigned long)read32(bytes + RECORD_DATA_OFFSET);
	return 1;
}

/* a name for matching: lower case, the underscores left out, with the MEDAL start if it has none; 0 if it does not fit */
static int match_form(char const *text, char *out, size_t out_size)
{
	size_t length = 0;

	for (; *text; text++)
	{
		if (*text == '_')
			continue;
		if (length + 1 >= out_size)
			return 0;
		out[length++] = (char)tolower((unsigned char)*text);
	}
	out[length] = 0;
	if (strncmp(out, "medal", 5))
	{
		if (length + 5 >= out_size)
			return 0;
		memmove(out + 5, out, length + 1);
		memcpy(out, "medal", 5);
	}
	return 1;
}

int ae_medal_index_find(unsigned char const *index, size_t index_size, char const *icon)
{
	char wanted[AE_MEDAL_NAME_SIZE * 2];
	int count = ae_medal_index_count(index, index_size);
	int i;

	if (!icon || !match_form(icon, wanted, sizeof(wanted)))
		return -1;
	for (i = 0; i < count; i++)
	{
		struct ae_medal_record record;
		char form[AE_MEDAL_NAME_SIZE * 2];

		if (ae_medal_index_record(index, index_size, i, &record) && match_form(record.name, form, sizeof(form)) &&
			!strcmp(form, wanted))
		{
			return i;
		}
	}
	return -1;
}

/* ---------- the pixels */

int ae_medal_pixels_valid(unsigned char const *pixels, size_t pixels_size)
{
	return pixels && pixels_size >= AE_MEDAL_PIXELS_HEADER && read32(pixels) == AE_MEDAL_PIXELS_MAGIC;
}

int ae_medal_record_fits(struct ae_medal_record const *record, size_t pixels_size)
{
	unsigned long long end;

	if (!record || record->width < 4 || record->height < 4 || record->width > AE_MEDAL_PICTURE_MOST ||
		record->height > AE_MEDAL_PICTURE_MOST || (record->width & 3u) || (record->height & 3u))
	{
		return 0;
	}
	if (record->size != record->width * record->height || record->offset < AE_MEDAL_PIXELS_HEADER)
		return 0;
	end = (unsigned long long)record->offset + record->size;
	return end <= (unsigned long long)pixels_size;
}

/* a 5-6-5 colour as red, green, blue */
static void colour_565(unsigned int value, unsigned int rgb[3])
{
	unsigned int red = (value >> 11) & 31u, green = (value >> 5) & 63u, blue = value & 31u;

	rgb[0] = (red << 3) | (red >> 2);
	rgb[1] = (green << 2) | (green >> 4);
	rgb[2] = (blue << 3) | (blue >> 2);
}

int ae_medal_decode_bc3(unsigned char const *blocks, size_t size, unsigned int width, unsigned int height,
	unsigned char *rgba)
{
	unsigned int block_x, block_y;

	if (!blocks || !rgba || width < 4 || height < 4 || (width & 3u) || (height & 3u) || width > 4096 || height > 4096 ||
		size != (size_t)width * height)
	{
		return 0;
	}
	for (block_y = 0; block_y < height / 4; block_y++)
	{
		for (block_x = 0; block_x < width / 4; block_x++)
		{
			unsigned char const *block = blocks + ((size_t)block_y * (width / 4) + block_x) * 16;
			unsigned int alpha[8], palette[4][3], alpha_bits_low, alpha_bits_high, colour_bits, c0, c1, pixel;
			unsigned long long alpha_bits = 0;
			int shift;

			/* (the alpha: two ends and six or four between them, with 0 and 255 after the four) */
			alpha[0] = block[0];
			alpha[1] = block[1];
			if (alpha[0] > alpha[1])
			{
				for (pixel = 1; pixel < 7; pixel++)
					alpha[1 + pixel] = ((7 - pixel) * alpha[0] + pixel * alpha[1]) / 7u;
			}
			else
			{
				for (pixel = 1; pixel < 5; pixel++)
					alpha[1 + pixel] = ((5 - pixel) * alpha[0] + pixel * alpha[1]) / 5u;
				alpha[6] = 0;
				alpha[7] = 255;
			}
			alpha_bits_low = read32(block + 2);
			alpha_bits_high = read16(block + 6);
			alpha_bits = (unsigned long long)alpha_bits_low | ((unsigned long long)alpha_bits_high << 32);
			/* (the colour: this kind of block is always four colours, whichever end is the larger) */
			c0 = read16(block + 8);
			c1 = read16(block + 10);
			colour_565(c0, palette[0]);
			colour_565(c1, palette[1]);
			for (shift = 0; shift < 3; shift++)
			{
				palette[2][shift] = (2u * palette[0][shift] + palette[1][shift]) / 3u;
				palette[3][shift] = (palette[0][shift] + 2u * palette[1][shift]) / 3u;
			}
			colour_bits = read32(block + 12);
			for (pixel = 0; pixel < 16; pixel++)
			{
				unsigned int x = block_x * 4 + (pixel & 3u), y = block_y * 4 + (pixel >> 2);
				unsigned int choice = (colour_bits >> (2 * pixel)) & 3u;
				unsigned char *out = rgba + ((size_t)y * width + x) * 4;

				out[0] = (unsigned char)palette[choice][0];
				out[1] = (unsigned char)palette[choice][1];
				out[2] = (unsigned char)palette[choice][2];
				out[3] = (unsigned char)alpha[(alpha_bits >> (3 * pixel)) & 7u];
			}
		}
	}
	return 1;
}

/* ---------- the career list */

static int blank(char c)
{
	return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

/* one attribute's name and value from the tag at *at (the tag's text is text up to end): moves *at past it. 1 for an attribute, 0
at the tag's close or at text that is not one */
static int next_attribute(char const *text, size_t end, size_t *at, size_t *name, size_t *name_length, size_t *value,
	size_t *value_length)
{
	size_t i = *at;
	char quote;

	while (i < end && blank(text[i]))
		i++;
	if (i >= end || text[i] == '>' || text[i] == '/')
		return 0;
	*name = i;
	while (i < end && !blank(text[i]) && text[i] != '=' && text[i] != '>' && text[i] != '/')
		i++;
	*name_length = i - *name;
	while (i < end && blank(text[i]))
		i++;
	if (*name_length == 0 || i >= end || text[i] != '=')
		return 0;
	i++;
	while (i < end && blank(text[i]))
		i++;
	if (i >= end || (text[i] != '"' && text[i] != '\''))
		return 0;
	quote = text[i++];
	*value = i;
	while (i < end && text[i] != quote)
		i++;
	if (i >= end)
		return 0;
	*value_length = i - *value;
	*at = i + 1;
	return 1;
}

static int same_text_ignoring_case(char const *a, size_t a_length, char const *b)
{
	size_t i;

	if (strlen(b) != a_length)
		return 0;
	for (i = 0; i < a_length; i++)
	{
		if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i]))
			return 0;
	}
	return 1;
}

int ae_medal_career_icon(char const *xml, size_t xml_size, char const *id, char *icon, size_t icon_size)
{
	size_t i = 0;

	if (!xml || !id || !id[0] || !icon || icon_size < 2)
		return 0;
	icon[0] = 0;
	while (i + 7 < xml_size)
	{
		size_t at, name, name_length, value, value_length;
		int has_id = 0;
		size_t icon_at = 0, icon_length = 0;
		int has_icon = 0;

		/* (a medal row: "<Medal" and a blank; <MPMedals> and <MissionMedals> are other tags) */
		if (xml[i] != '<' || memcmp(xml + i + 1, "Medal", 5) || !blank(xml[i + 6]))
		{
			i++;
			continue;
		}
		at = i + 6;
		while (next_attribute(xml, xml_size, &at, &name, &name_length, &value, &value_length))
		{
			if (name_length == 2 && !memcmp(xml + name, "id", 2) && same_text_ignoring_case(xml + value, value_length, id))
				has_id = 1;
			else if (name_length == 9 && !memcmp(xml + name, "smallIcon", 9))
			{
				has_icon = 1;
				icon_at = value;
				icon_length = value_length;
			}
		}
		if (has_id)
		{
			if (!has_icon || icon_length == 0 || icon_length >= icon_size)
				return 0;
			memcpy(icon, xml + icon_at, icon_length);
			icon[icon_length] = 0;
			return 1;
		}
		/* (on past this tag's name at least) */
		i += 6;
	}
	return 0;
}

/* a value's text with the five XML entities read (&amp; &lt; &gt; &quot; &apos;); another "&" is kept as it is. 0 when it does
not fit whole in out */
static int entities_read(char const *value, size_t length, char *out, size_t out_size)
{
	static struct { char const *name; size_t length; char c; } const entities[] =
	{
		{ "&amp;", 5, '&' }, { "&lt;", 4, '<' }, { "&gt;", 4, '>' }, { "&quot;", 6, '"' }, { "&apos;", 6, '\'' },
	};
	size_t at = 0, i = 0, e;

	while (i < length)
	{
		char c = value[i++];

		if (c == '&')
		{
			for (e = 0; e < sizeof(entities) / sizeof(entities[0]); e++)
			{
				if (i - 1 + entities[e].length <= length && !memcmp(value + i - 1, entities[e].name, entities[e].length))
				{
					c = entities[e].c;
					i += entities[e].length - 1;
					break;
				}
			}
		}
		/* (a control character, a line break in the value, reads as a space) */
		if ((unsigned char)c < 0x20)
			c = ' ';
		if (at + 1 >= out_size)
			return 0;
		out[at++] = c;
	}
	out[at] = 0;
	return 1;
}

int ae_medal_career_desc(char const *xml, size_t xml_size, char const *id, char *desc, size_t desc_size)
{
	size_t i = 0;

	if (!desc || desc_size < 2)
		return 0;
	desc[0] = 0;
	if (!xml || !id || !id[0])
		return 0;
	while (i + 7 < xml_size)
	{
		size_t at, name, name_length, value, value_length, desc_at = 0, desc_length = 0;
		int has_id = 0, has_desc = 0;

		if (xml[i] != '<' || memcmp(xml + i + 1, "Medal", 5) || !blank(xml[i + 6]))
		{
			i++;
			continue;
		}
		at = i + 6;
		while (next_attribute(xml, xml_size, &at, &name, &name_length, &value, &value_length))
		{
			if (name_length == 2 && !memcmp(xml + name, "id", 2) && same_text_ignoring_case(xml + value, value_length, id))
				has_id = 1;
			else if (name_length == 4 && !memcmp(xml + name, "desc", 4))
			{
				has_desc = 1;
				desc_at = value;
				desc_length = value_length;
			}
		}
		if (has_id)
		{
			/* (a "$" value names a line of the install's string tables, which this does not read: no text) */
			if (!has_desc || desc_length == 0 || xml[desc_at] == '$' ||
				!entities_read(xml + desc_at, desc_length, desc, desc_size))
			{
				desc[0] = 0;
				return 0;
			}
			return 1;
		}
		i += 6;
	}
	return 0;
}

/* ---------- names */

int ae_medal_name_from_key(char const *key, char *out, size_t out_size, int upper)
{
	size_t length = 0;
	int word_start = 1;

	if (!out || out_size < 1)
		return 0;
	out[0] = 0;
	if (!key)
		return 0;
	if (!strncmp(key, "medal_", 6))
		key += 6;
	/* (keys cut short or joined in the career list: the name players know) */
	{
		static char const *const names[][2] =
		{
			{ "triple_of_each_kill", "triple_kill" },
			{ "untouchable_row_wit_untouchable", "untouchable" },
			{ "splatter_with_a__splatter", "splatter" },
			{ "sniper_rifle_w_spree", "sniper_spree" },
			{ "sharpshooter_rifle_w_sharpshooter", "sharpshooter" },
			{ "shotgun_shotgun_spree", "shotgun_spree" },
			{ "splatter_row_wit_spree", "splatter_spree" },
			{ "vehicle_row_wit_manslaughter", "vehicular_manslaughter" },
			{ "wheelman_kill_an_wheelman", "wheelman" },
			{ "unfriggenbelievable_row_wit_unfriggenbelievable", "unfriggenbelievable" },
			{ "supercombine_using_a_supercombine", "supercombine" },
			{ "vehicle_a_vehic_kill", "vehicle_kill" },
			{ "first_poiint", "first_point" },
			{ "smooth_with_th_moves", "smooth_moves" },
			{ "wingman_5_assis_wingman", "wingman" },
		};
		size_t index;

		for (index = 0; index < sizeof(names) / sizeof(names[0]); index++)
		{
			if (!strcmp(key, names[index][0]))
			{
				key = names[index][1];
				break;
			}
		}
	}
	for (; *key && length + 1 < out_size; key++)
	{
		unsigned char c = (unsigned char)*key;

		if (c == '_')
		{
			/* (a run of underscores is one space; none at the start or the end) */
			if (!word_start)
				out[length++] = ' ';
			word_start = 1;
			continue;
		}
		out[length++] = (char)((upper || word_start) ? toupper(c) : tolower(c));
		word_start = 0;
	}
	while (length > 0 && out[length - 1] == ' ')
		length--;
	out[length] = 0;
	return (int)length;
}

int ae_medal_art_name_from_key(char const *key, char *out, size_t out_size)
{
	size_t length;

	if (!out || out_size < 1)
		return 0;
	out[0] = 0;
	if (!key || strncmp(key, "medal_", 6) || !key[6])
		return 0;
	key += 6;
	/* (a file's name cannot be a bare number to a C table's reader: three_sixty) */
	if (!strcmp(key, "360"))
		key = "three_sixty";
	length = strlen(key);
	if (length + 1 > out_size)
		return 0;
	memcpy(out, key, length + 1);
	return (int)length;
}
