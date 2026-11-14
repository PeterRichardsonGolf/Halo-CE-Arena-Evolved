/*
AE_CROSSHAIR_RULES.C

CROSSHAIR v3 as pure rules (ae_crosshair_rules.h): the shape library, a crosshair's defaults and ranges, its share
code, the profile's strings, and the pieces it is drawn from. Kenney's Crosshair Pack (CC0) is where the shapes come
from; tools/ae_crosshair_shapes.py is their table.
*/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ae_crosshair_rules.h"

/* a Kenney unit in pixels at 1080p (its 64 unit canvas is 48 pixels) */
#define K 0.75f

enum { AE_XH_PRIM_RING, AE_XH_PRIM_ARC, AE_XH_PRIM_SEG, AE_XH_PRIM_POLY, AE_XH_PRIM_WEDGE, AE_XH_PRIM_SQUARE,
	AE_XH_PRIM_BRACKETS };
enum { LAYOUT_CROSS, LAYOUT_TEE, LAYOUT_X, LAYOUT_SIDES, LAYOUT_UPDOWN, LAYOUT_TRIPOD };

struct ae_xh_prim
{
	unsigned char kind, closed, count;
	short first;
	float a, b, c;
};

struct ae_xh_set { int layout; float offset, length, thickness; };

struct ae_xh_shape
{
	const char *name;
	int family, centre;
	float centre_a, centre_b;
	struct ae_xh_set inner, outer;
	int first, count;
	float frame_thickness;
};

#include "ae_crosshair_shapes.inc"

static const char *const family_names[4] = { "LINES", "CHEVRONS", "RINGS", "SHAPES" };

int ae_xh_shape_count(void)
{
	return AE_XH_SHAPE_COUNT;
}

static const struct ae_xh_shape *shape_of(int shape)
{
	return shape >= 1 && shape <= AE_XH_SHAPE_COUNT ? &shape_table[shape - 1] : NULL;
}

const char *ae_xh_shape_name(int shape)
{
	const struct ae_xh_shape *row = shape_of(shape);

	return row ? row->name : "";
}

int ae_xh_shape_family(int shape)
{
	const struct ae_xh_shape *row = shape_of(shape);

	return row ? row->family : 0;
}

const char *ae_xh_family_name(int family)
{
	return family >= 0 && family < 4 ? family_names[family] : "";
}

int ae_xh_shape_of_name(const char *name)
{
	int index;

	if (name)
		for (index = 0; index < AE_XH_SHAPE_COUNT; index++)
			if (!strcmp(shape_table[index].name, name))
				return index + 1;
	return 0;
}

int ae_xh_shape_has_frame(int shape)
{
	const struct ae_xh_shape *row = shape_of(shape);

	return row && row->count > 0;
}

/* ---------- the palette */

static const struct { const char *name; unsigned long rgb; } palette[] =
{
	{ "WHITE", 0xF5F8FAul }, { "ICE", 0x96D2FFul }, { "CYAN", 0x40E8F0ul }, { "PINK", 0xFF82C8ul },
	{ "YELLOW", 0xFFE040ul }, { "ORANGE", 0xFF9A40ul }, { "VIOLET", 0xB48CFFul }, { "BLUE", 0x4098FFul },
	{ "GRAY", 0xA0A8B0ul }, { "BLACK", 0x101418ul }
};
#define PALETTE ((int)(sizeof(palette) / sizeof(palette[0])))

int ae_xh_palette_count(void)
{
	return PALETTE;
}

const char *ae_xh_palette_name(int index)
{
	return index >= 0 && index < PALETTE ? palette[index].name : "";
}

unsigned long ae_xh_palette_rgb(int index)
{
	return index >= 0 && index < PALETTE ? palette[index].rgb : palette[0].rgb;
}

void ae_xh_color_set(struct ae_crosshair *xh, unsigned long rgb)
{
	xh->color[0] = (unsigned char)(rgb >> 16);
	xh->color[1] = (unsigned char)(rgb >> 8);
	xh->color[2] = (unsigned char)rgb;
}

int ae_xh_palette_of(const struct ae_crosshair *xh)
{
	unsigned long rgb = ((unsigned long)xh->color[0] << 16) | ((unsigned long)xh->color[1] << 8) | xh->color[2];
	int index;

	for (index = 0; index < PALETTE; index++)
		if (palette[index].rgb == rgb)
			return index;
	return -1;
}

/* ---------- ranges */

static unsigned char clampb(long value, long low, long high)
{
	return (unsigned char)(value < low ? low : value > high ? high : value);
}

static long half_pixels(float units)
{
	return (long)floorf(units * K * 2.0f + 0.5f);
}

int ae_xh_name_clean(const char *text, char *name, size_t size)
{
	size_t length = 0;

	if (!name || size == 0)
		return 0;
	name[0] = 0;
	for (; text && *text && length < AE_XH_NAME && length + 1 < size; text++)
	{
		char c = *text;

		if (c >= 'a' && c <= 'z')
			c = (char)(c - 'a' + 'A');
		if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ' || c == '-' || c == '.'))
			continue;
		if (c == ' ' && length == 0)
			continue;
		name[length++] = c;
	}
	while (length > 0 && name[length - 1] == ' ')
		length--;
	name[length] = 0;
	return (int)length;
}

static void lines_clamp(struct ae_xh_lines *lines)
{
	lines->on = lines->on ? 1 : 0;
	lines->length = clampb(lines->length, 0, AE_XH_LENGTH_MAX);
	lines->thickness = clampb(lines->thickness, AE_XH_THICKNESS_MIN, AE_XH_THICKNESS_MAX);
	lines->offset = clampb(lines->offset, 0, AE_XH_OFFSET_MAX);
	lines->opacity = clampb(lines->opacity, 0, 100);
}

void ae_xh_clamp(struct ae_crosshair *xh)
{
	char name[AE_XH_NAME + 1], raw[AE_XH_NAME + 1];

	/* (the name may not end: only its own bytes are read) */
	memcpy(raw, xh->name, AE_XH_NAME);
	raw[AE_XH_NAME] = 0;
	ae_xh_name_clean(raw, name, sizeof(name));
	memset(xh->name, 0, sizeof(xh->name));
	memcpy(xh->name, name, strlen(name));
	if (xh->shape > AE_XH_SHAPE_COUNT)
		xh->shape = 0;
	if (xh->centre > AE_XH_CENTRE_PLUS)
		xh->centre = AE_XH_CENTRE_NONE;
	xh->centre_size = clampb(xh->centre_size, 1, AE_XH_CENTRE_SIZE_MAX);
	xh->opacity = clampb(xh->opacity, AE_XH_OPACITY_MIN, 100);
	lines_clamp(&xh->inner);
	lines_clamp(&xh->outer);
	xh->frame_on = xh->frame_on ? 1 : 0;
	xh->frame_size = clampb(xh->frame_size, AE_XH_FRAME_SIZE_MIN, AE_XH_FRAME_SIZE_MAX);
	xh->frame_thickness = clampb(xh->frame_thickness, AE_XH_THICKNESS_MIN, AE_XH_THICKNESS_MAX);
	xh->frame_opacity = clampb(xh->frame_opacity, 0, 100);
	xh->outline_on = xh->outline_on ? 1 : 0;
	xh->outline_thickness = clampb(xh->outline_thickness, 1, AE_XH_OUTLINE_THICKNESS_MAX);
	xh->outline_opacity = clampb(xh->outline_opacity, 0, 100);
	xh->movement = xh->movement ? 1 : 0;
	xh->firing = xh->firing ? 1 : 0;
}

static void lines_of_set(struct ae_xh_lines *lines, const struct ae_xh_set *set)
{
	if (set->layout < 0)
	{
		/* (a set the shape has none of: a modest one, off) */
		lines->on = 0;
		lines->length = 12;
		lines->thickness = 4;
		lines->offset = 12;
		lines->opacity = 100;
		return;
	}
	lines->on = 1;
	lines->length = (unsigned char)half_pixels(set->length);
	lines->thickness = (unsigned char)(half_pixels(set->thickness) < 1 ? 1 : half_pixels(set->thickness));
	lines->offset = (unsigned char)half_pixels(set->offset);
	lines->opacity = 100;
}

void ae_xh_apply_shape(struct ae_crosshair *xh, int shape)
{
	const struct ae_xh_shape *row = shape_of(shape);

	if (!row)
	{
		xh->shape = 0;
		return;
	}
	xh->shape = (unsigned char)shape;
	if (row->centre == 1)
	{
		xh->centre = AE_XH_CENTRE_DOT;
		xh->centre_size = (unsigned char)half_pixels(row->centre_a);
	}
	else if (row->centre == 2)
	{
		xh->centre = AE_XH_CENTRE_PLUS;
		xh->centre_size = (unsigned char)half_pixels(row->centre_b);
	}
	else
	{
		xh->centre = AE_XH_CENTRE_NONE;
		xh->centre_size = 4;
	}
	lines_of_set(&xh->inner, &row->inner);
	lines_of_set(&xh->outer, &row->outer);
	xh->frame_on = row->count > 0;
	xh->frame_size = 100;
	xh->frame_thickness = (unsigned char)(half_pixels(row->frame_thickness) < 1 ? 1 : half_pixels(row->frame_thickness));
	xh->frame_opacity = 100;
	ae_xh_clamp(xh);
}

void ae_xh_default(struct ae_crosshair *xh)
{
	memset(xh, 0, sizeof(*xh));
	ae_xh_color_set(xh, palette[0].rgb);
	xh->opacity = 100;
	xh->outline_on = 1;
	xh->outline_thickness = 2;
	xh->outline_opacity = 80;
	xh->firing = 1;
	snprintf(xh->name, sizeof(xh->name), "PRECISE");
	ae_xh_apply_shape(xh, ae_xh_shape_of_name("TWIN CROSS"));
	xh->outer.opacity = 60;
}

/* ---------- (0.7.7) the starters */

int ae_xh_starter_count(void)
{
	return 3;
}

static void lines_set(struct ae_xh_lines *lines, int on, int length, int thickness, int offset, int opacity)
{
	lines->on = (unsigned char)on;
	lines->length = (unsigned char)length;
	lines->thickness = (unsigned char)thickness;
	lines->offset = (unsigned char)offset;
	lines->opacity = (unsigned char)opacity;
}

int ae_xh_starter(int index, struct ae_crosshair *xh)
{
	ae_xh_default(xh);
	switch (index)
	{
	case 0:
		/* PRECISE: a 2 pixel dot, short inner lines and faint outer ones (half pixels below) */
		snprintf(xh->name, sizeof(xh->name), "PRECISE");
		xh->centre = AE_XH_CENTRE_DOT;
		xh->centre_size = 4;
		lines_set(&xh->inner, 1, 8, 4, 6, 100);
		lines_set(&xh->outer, 1, 6, 4, 16, 60);
		break;
	case 1:
		/* SNAP: a dot in a thin ring */
		ae_xh_apply_shape(xh, ae_xh_shape_of_name("RING"));
		snprintf(xh->name, sizeof(xh->name), "SNAP");
		xh->centre = AE_XH_CENTRE_DOT;
		xh->centre_size = 3;
		xh->inner.on = xh->outer.on = 0;
		xh->frame_on = 1;
		xh->frame_size = 56;
		xh->frame_thickness = 4;
		xh->outline_opacity = 60;
		break;
	case 2:
		/* OCTA: a dot in an octagon, no outline */
		ae_xh_apply_shape(xh, ae_xh_shape_of_name("OCTAGON"));
		snprintf(xh->name, sizeof(xh->name), "OCTA");
		xh->centre = AE_XH_CENTRE_DOT;
		xh->centre_size = 3;
		xh->inner.on = xh->outer.on = 0;
		xh->frame_on = 1;
		xh->frame_size = 115;
		xh->frame_thickness = 5;
		xh->outline_on = 0;
		break;
	default:
		return 0;
	}
	ae_xh_clamp(xh);
	return 1;
}

/* ---------- (0.7.7) CUSTOM COLOR */

static int hex_digit(char c)
{
	return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
}

int ae_xh_hex_parse(const char *text, unsigned long *rgb)
{
	unsigned long value = 0;
	int index, digits = 0;

	if (!text)
		return 0;
	while (*text == ' ')
		text++;
	if (*text == '#')
		text++;
	for (index = 0; index < 6; index++)
	{
		int digit = hex_digit(text[index]);

		if (digit < 0)
			return 0;
		value = (value << 4) | (unsigned long)digit;
		digits++;
	}
	for (text += 6; *text == ' '; text++)
		;
	if (*text || digits != 6)
		return 0;
	if (rgb)
		*rgb = value;
	return 1;
}

void ae_xh_hex_format(unsigned long rgb, char *text, size_t size)
{
	if (text && size)
		snprintf(text, size, "#%06lX", rgb & 0xFFFFFFul);
}

/* ---------- the share code */

static unsigned crc16(const unsigned char *data, size_t length)
{
	unsigned crc = 0xFFFF;
	size_t index;
	int bit;

	for (index = 0; index < length; index++)
	{
		crc ^= (unsigned)data[index] << 8;
		for (bit = 0; bit < 8; bit++)
			crc = (crc & 0x8000) ? ((crc << 1) ^ 0x1021) & 0xFFFF : (crc << 1) & 0xFFFF;
	}
	return crc;
}

static const char crockford[] = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";
enum { CODE_BYTES = 23, CODE_CHARS = 37 };

static void code_bytes(const struct ae_crosshair *xh, unsigned char *b)
{
	unsigned crc;

	b[0] = 1;
	b[1] = xh->shape;
	b[2] = (unsigned char)((xh->inner.on ? 1 : 0) | (xh->outer.on ? 2 : 0) | (xh->frame_on ? 4 : 0) |
		(xh->outline_on ? 8 : 0) | (xh->movement ? 16 : 0) | (xh->firing ? 32 : 0) | (xh->centre << 6));
	b[3] = xh->color[0];
	b[4] = xh->color[1];
	b[5] = xh->color[2];
	b[6] = xh->opacity;
	b[7] = xh->inner.length;
	b[8] = xh->inner.thickness;
	b[9] = xh->inner.offset;
	b[10] = xh->inner.opacity;
	b[11] = xh->outer.length;
	b[12] = xh->outer.thickness;
	b[13] = xh->outer.offset;
	b[14] = xh->outer.opacity;
	b[15] = xh->frame_size;
	b[16] = xh->frame_thickness;
	b[17] = xh->frame_opacity;
	b[18] = xh->centre_size;
	b[19] = xh->outline_thickness;
	b[20] = xh->outline_opacity;
	crc = crc16(b, 21);
	b[21] = (unsigned char)(crc >> 8);
	b[22] = (unsigned char)(crc & 0xFF);
}

void ae_xh_code_encode(const struct ae_crosshair *xh, char *text, size_t size)
{
	unsigned char b[CODE_BYTES];
	char body[CODE_CHARS + 1];
	char out[AE_XH_CODE_TEXT];
	int index, pos = 0;

	if (!text || size == 0)
		return;
	code_bytes(xh, b);
	for (index = 0; index < CODE_CHARS; index++)
	{
		int bit = index * 5, value = 0, k;

		for (k = 0; k < 5; k++)
		{
			int at = bit + k;

			value <<= 1;
			if (at < CODE_BYTES * 8)
				value |= (b[at >> 3] >> (7 - (at & 7))) & 1;
		}
		body[index] = crockford[value];
	}
	body[CODE_CHARS] = 0;
	pos += snprintf(out, sizeof(out), "AE1-");
	for (index = 0; index < CODE_CHARS; index++)
	{
		if (index > 0 && index % 4 == 0)
			out[pos++] = '-';
		out[pos++] = body[index];
	}
	out[pos] = 0;
	snprintf(text, size, "%s", out);
}

static int crockford_value(char c)
{
	const char *at;

	if (c >= 'a' && c <= 'z')
		c = (char)(c - 'a' + 'A');
	if (c == 'O')
		c = '0';
	else if (c == 'I' || c == 'L')
		c = '1';
	at = c ? strchr(crockford, c) : NULL;
	return at ? (int)(at - crockford) : -1;
}

int ae_xh_code_decode(const char *text, struct ae_crosshair *xh, int *unknown_shape)
{
	unsigned char b[CODE_BYTES];
	int values[CODE_CHARS + 1], count = 0, index, prefix = 0, version = 0;
	size_t length;
	struct ae_crosshair made;
	unsigned crc;

	if (unknown_shape)
		*unknown_shape = 0;
	if (!text)
		return AE_XH_CODE_NOT_A_CODE;
	for (length = 0; text[length]; length++)
		if (length >= AE_XH_CODE_INPUT_MAX)
			return AE_XH_CODE_NOT_A_CODE;
	/* (the prefix: AE and a version digit; dashes, spaces and line ends are ignored everywhere) */
	for (index = 0; text[index]; index++)
	{
		char c = text[index];

		if (c == '-' || c == ' ' || c == '\t' || c == '\r' || c == '\n')
			continue;
		if (prefix == 0)
		{
			if (c != 'A' && c != 'a')
				return AE_XH_CODE_NOT_A_CODE;
			prefix = 1;
		}
		else if (prefix == 1)
		{
			if (c != 'E' && c != 'e')
				return AE_XH_CODE_NOT_A_CODE;
			prefix = 2;
		}
		else if (prefix == 2)
		{
			if (c < '0' || c > '9')
				return AE_XH_CODE_NOT_A_CODE;
			version = c - '0';
			prefix = 3;
			if (version != 1)
				return version > 1 ? AE_XH_CODE_NEWER : AE_XH_CODE_NOT_A_CODE;
		}
		else
		{
			int value = crockford_value(c);

			if (value < 0)
				return AE_XH_CODE_NOT_A_CODE;
			if (count >= CODE_CHARS)
				return AE_XH_CODE_DAMAGED;
			values[count++] = value;
		}
	}
	if (prefix < 3)
		return AE_XH_CODE_NOT_A_CODE;
	if (count != CODE_CHARS)
		return AE_XH_CODE_DAMAGED;
	memset(b, 0, sizeof(b));
	for (index = 0; index < CODE_CHARS; index++)
	{
		int k;

		for (k = 0; k < 5; k++)
		{
			int at = index * 5 + k, bit = (values[index] >> (4 - k)) & 1;

			if (at < CODE_BYTES * 8)
				b[at >> 3] = (unsigned char)(b[at >> 3] | (bit << (7 - (at & 7))));
			else if (bit)
				return AE_XH_CODE_DAMAGED; /* (the padding bit is zero) */
		}
	}
	crc = crc16(b, 21);
	if (crc != (((unsigned)b[21] << 8) | b[22]))
		return AE_XH_CODE_DAMAGED;
	if (b[0] != 1)
		return b[0] > 1 ? AE_XH_CODE_NEWER : AE_XH_CODE_DAMAGED;
	memset(&made, 0, sizeof(made));
	made.shape = b[1];
	if (made.shape > AE_XH_SHAPE_COUNT)
	{
		made.shape = 0;
		if (unknown_shape)
			*unknown_shape = 1;
	}
	made.inner.on = b[2] & 1;
	made.outer.on = (b[2] >> 1) & 1;
	made.frame_on = (b[2] >> 2) & 1;
	made.outline_on = (b[2] >> 3) & 1;
	made.movement = (b[2] >> 4) & 1;
	made.firing = (b[2] >> 5) & 1;
	made.centre = (unsigned char)(b[2] >> 6);
	made.color[0] = b[3];
	made.color[1] = b[4];
	made.color[2] = b[5];
	made.opacity = b[6];
	made.inner.length = b[7];
	made.inner.thickness = b[8];
	made.inner.offset = b[9];
	made.inner.opacity = b[10];
	made.outer.length = b[11];
	made.outer.thickness = b[12];
	made.outer.offset = b[13];
	made.outer.opacity = b[14];
	made.frame_size = b[15];
	made.frame_thickness = b[16];
	made.frame_opacity = b[17];
	made.centre_size = b[18];
	made.outline_thickness = b[19];
	made.outline_opacity = b[20];
	ae_xh_clamp(&made);
	if (xh)
	{
		char name[AE_XH_NAME + 1];

		memcpy(name, xh->name, sizeof(name));
		*xh = made;
		memcpy(xh->name, name, sizeof(name));
		xh->name[AE_XH_NAME] = 0;
	}
	return AE_XH_CODE_OK;
}

const char *ae_xh_code_why(int result)
{
	switch (result)
	{
	case AE_XH_CODE_OK: return "";
	case AE_XH_CODE_NOT_A_CODE: return "not a crosshair code";
	case AE_XH_CODE_DAMAGED: return "damaged code: a character is wrong";
	case AE_XH_CODE_NEWER: return "from a newer Arena Evolved";
	}
	return "not a crosshair code";
}

/* ---------- the profile's strings */

int ae_xh_slot_parse(const char *text, struct ae_crosshair *xh)
{
	char name_text[AE_XH_NAME * 4 + 4];
	const char *bar;
	size_t length;
	struct ae_crosshair made;

	memset(&made, 0, sizeof(made));
	if (!text || !text[0] || strlen(text) > 160)
		return 0;
	/* (0.7.7) a profile's starter, as the slot's default says it: "starter:1" to "starter:3" */
	if (!strncmp(text, "starter:", 8) && text[8] >= '1' && text[8] <= '0' + ae_xh_starter_count() && !text[9])
	{
		ae_xh_starter(text[8] - '1', &made);
		if (xh)
			*xh = made;
		return 1;
	}
	bar = strchr(text, '|');
	if (!bar)
		return 0;
	length = (size_t)(bar - text);
	if (length >= sizeof(name_text))
		length = sizeof(name_text) - 1;
	memcpy(name_text, text, length);
	name_text[length] = 0;
	ae_xh_name_clean(name_text, made.name, sizeof(made.name));
	if (ae_xh_code_decode(bar + 1, &made, NULL) != AE_XH_CODE_OK)
		return 0;
	if (xh)
		*xh = made;
	return 1;
}

void ae_xh_slot_format(const struct ae_crosshair *xh, char *text, size_t size)
{
	char code[AE_XH_CODE_TEXT];

	ae_xh_code_encode(xh, code, sizeof(code));
	snprintf(text, size, "%s|%s", xh->name, code);
}

static int key_ok(const char *key)
{
	size_t length = key ? strlen(key) : 0, index;

	if (length == 0 || length >= AE_XH_WEAPON_KEY)
		return 0;
	for (index = 0; index < length; index++)
		if (!((key[index] >= 'a' && key[index] <= 'z') || (key[index] >= '0' && key[index] <= '9') || key[index] == '_'))
			return 0;
	return 1;
}

void ae_xh_weapons_parse(const char *text, struct ae_xh_weapons *table)
{
	const char *at = text;

	memset(table, 0, sizeof(*table));
	while (at && *at && table->count < AE_XH_WEAPON_ENTRIES)
	{
		const char *end = strchr(at, ';'), *eq;
		size_t length = end ? (size_t)(end - at) : strlen(at);
		char key[AE_XH_WEAPON_KEY + 4], value[8];

		eq = memchr(at, '=', length);
		if (eq && (size_t)(eq - at) < sizeof(key) && length - (size_t)(eq - at) - 1 < sizeof(value))
		{
			int choice = AE_XH_CHOICE_MAIN;
			size_t key_length = (size_t)(eq - at), value_length = length - key_length - 1;

			memcpy(key, at, key_length);
			key[key_length] = 0;
			memcpy(value, eq + 1, value_length);
			value[value_length] = 0;
			if (!strcmp(value, "own"))
				choice = AE_XH_CHOICE_OWN;
			else if (value[0] >= '1' && value[0] <= '8' && !value[1])
				choice = value[0] - '1';
			if (choice != AE_XH_CHOICE_MAIN && key_ok(key) && ae_xh_weapons_get(table, key) == AE_XH_CHOICE_MAIN)
			{
				memcpy(table->entry[table->count].key, key, strlen(key) + 1);
				table->entry[table->count].choice = (signed char)choice;
				table->count++;
			}
		}
		at = end ? end + 1 : NULL;
	}
}

void ae_xh_weapons_format(const struct ae_xh_weapons *table, char *text, size_t size)
{
	size_t used = 0;
	int index;

	if (!text || size == 0)
		return;
	text[0] = 0;
	for (index = 0; index < table->count; index++)
	{
		char one[AE_XH_WEAPON_KEY + 8];
		int written = table->entry[index].choice == AE_XH_CHOICE_OWN ?
			snprintf(one, sizeof(one), "%s=own", table->entry[index].key) :
			snprintf(one, sizeof(one), "%s=%d", table->entry[index].key, table->entry[index].choice + 1);

		if (used + (size_t)written + 2 > size)
			break;
		memcpy(text + used, one, (size_t)written);
		used += (size_t)written;
		text[used++] = ';';
		text[used] = 0;
	}
	if (used > 0)
		text[used - 1] = 0;
}

int ae_xh_weapons_get(const struct ae_xh_weapons *table, const char *key)
{
	int index;

	for (index = 0; key && index < table->count; index++)
		if (!strcmp(table->entry[index].key, key))
			return table->entry[index].choice;
	return AE_XH_CHOICE_MAIN;
}

int ae_xh_weapons_set(struct ae_xh_weapons *table, const char *key, int choice)
{
	int index;

	if (!key_ok(key))
		return 0;
	for (index = 0; index < table->count; index++)
		if (!strcmp(table->entry[index].key, key))
		{
			if (choice == AE_XH_CHOICE_MAIN)
			{
				memmove(&table->entry[index], &table->entry[index + 1],
					sizeof(table->entry[0]) * (size_t)(table->count - index - 1));
				table->count--;
			}
			else
				table->entry[index].choice = (signed char)choice;
			return 1;
		}
	if (choice == AE_XH_CHOICE_MAIN)
		return 1;
	if (table->count >= AE_XH_WEAPON_ENTRIES)
		return 0;
	memcpy(table->entry[table->count].key, key, strlen(key) + 1);
	table->entry[table->count].choice = (signed char)choice;
	table->count++;
	return 1;
}

void ae_xh_weapons_summary(const struct ae_xh_weapons *table, int *own, int *custom)
{
	int index;

	*own = *custom = 0;
	for (index = 0; index < table->count; index++)
	{
		if (table->entry[index].choice == AE_XH_CHOICE_OWN)
			(*own)++;
		else
			(*custom)++;
	}
}

void ae_xh_weapon_key_of_tag(const char *tag_name, char *key, size_t size)
{
	const char *base = tag_name ? tag_name : "", *at;
	size_t length = 0;
	int pending = 0;

	for (at = base; *at; at++)
		if (*at == '\\' || *at == '/')
			base = at + 1;
	if (!key || size == 0)
		return;
	for (at = base; *at && length + 1 < size && length + 1 < AE_XH_WEAPON_KEY; at++)
	{
		char c = *at;

		if (c >= 'A' && c <= 'Z')
			c = (char)(c - 'A' + 'a');
		if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
		{
			if (pending && length > 0)
				key[length++] = '_';
			pending = 0;
			if (length + 1 < size && length + 1 < AE_XH_WEAPON_KEY)
				key[length++] = c;
		}
		else
			pending = 1;
	}
	key[length] = 0;
	/* (a multiplayer copy is the same gun: "mp_ghost_gun", "mp_gun_turret_gun"; the stationary gun's campaign tag too) */
	if (!strncmp(key, "mp_", 3))
		memmove(key, key + 3, strlen(key + 3) + 1);
	if (!strcmp(key, "c_gun_turret_gun"))
		snprintf(key, size, "gun_turret_gun");
}

int ae_xh_main_of(const char *text)
{
	if (text && text[0] >= '1' && text[0] <= '8' && !text[1])
		return text[0] - '1';
	return AE_XH_CHOICE_OWN;
}

/* ---------- the stock reticle's rows */

int ae_xh_reticle_size_clamp(long percent)
{
	return percent < AE_XH_RETICLE_SIZE_MIN ? AE_XH_RETICLE_SIZE_MIN :
		percent > AE_XH_RETICLE_SIZE_MAX ? AE_XH_RETICLE_SIZE_MAX : (int)percent;
}

int ae_xh_reticle_scaling_of(const char *text)
{
	return text && !strcmp(text, "fixed") ? AE_XH_SCALING_FIXED : AE_XH_SCALING_FOV;
}

int ae_xh_fov_base_of(const char *text)
{
	return text && !strcmp(text, "78") ? AE_XH_FOV_BASE_78 : AE_XH_FOV_BASE_70;
}

float ae_xh_fov_base_factor(int base)
{
	/* (degrees across a 4:3 view: the game's own view is 70; tan(39 deg) / tan(35 deg) for 78) */
	return base == AE_XH_FOV_BASE_70 ? 1.0f : tanf(39.0f * 3.14159265f / 180.0f) / tanf(35.0f * 3.14159265f / 180.0f);
}

int ae_xh_stock_motion_of(const char *text)
{
	return text && !strcmp(text, "all") ? AE_XH_STOCK_MOTION_ALL : text && !strcmp(text, "off") ?
		AE_XH_STOCK_MOTION_OFF : AE_XH_STOCK_MOTION_RIFLES;
}

void ae_xh_motion_class_of(float rate, float lower_degrees, float upper_degrees, struct ae_xh_motion_class *out)
{
	float span = upper_degrees - lower_degrees;

	out->has_spread = 0;
	out->gain = 0.0f;
	out->growth = 0.0f;
	out->ms = 0;
	if (span > 0.05f && span < 90.0f)
	{
		out->has_spread = 1;
		out->gain = 0.10f + 0.25f * (span >= 3.0f ? 1.0f : span / 3.0f);
		return;
	}
	if (!(rate > 0.0f) || rate > 1000.0f)
	{
		out->growth = 0.15f;
		out->ms = 300;
		return;
	}
	out->growth = rate >= 6.0f ? 0.15f : rate >= 2.5f ? 0.18f : 0.22f;
	out->ms = (int)(700.0f / rate);
	out->ms = out->ms < 200 ? 200 : out->ms > 500 ? 500 : out->ms;
}

float ae_xh_stock_motion_scale(struct ae_xh_motion_class const *cls, float kick, float error, float spread)
{
	float scale = 1.0f;

	if (!cls)
		return scale;
	if (cls->has_spread)
	{
		float grown = error > 0.0f ? cls->gain * (error > 1.0f ? 1.0f : error) : 0.0f;
		float cone = spread > 0.0f && spread < 10000.0f ? spread / 24.0f : 0.0f;

		scale += grown < cone ? grown : cone;
	}
	else if (kick > 0.0f)
		scale += cls->growth * (kick > 1.0f ? 1.0f : kick);
	return scale > 1.35f ? 1.35f : scale;
}

static const char *const tint_texts[AE_XH_TINTS] = { "original", "hud", "white", "ice", "cyan", "pink", "yellow" };

int ae_xh_tint_of(const char *text)
{
	int index;

	for (index = 0; text && index < AE_XH_TINTS; index++)
		if (!strcmp(text, tint_texts[index]))
			return index;
	return AE_XH_TINT_ORIGINAL;
}

const char *ae_xh_tint_text(int tint)
{
	return tint >= 0 && tint < AE_XH_TINTS ? tint_texts[tint] : tint_texts[0];
}

unsigned long ae_xh_tint_rgb(int tint, unsigned long base)
{
	switch (tint)
	{
	case AE_XH_TINT_WHITE: return 0xF5F8FAul;
	case AE_XH_TINT_ICE: return 0x96D2FFul;
	case AE_XH_TINT_CYAN: return 0x40E8F0ul;
	case AE_XH_TINT_PINK: return 0xFF82C8ul;
	case AE_XH_TINT_YELLOW: return 0xFFE040ul;
	}
	return base;
}

static const char *const state_color_texts[AE_XH_STATE_COLORS] =
	{ "red", "orange", "yellow", "green", "cyan", "blue", "magenta", "white" };
static const unsigned long state_color_rgbs[AE_XH_STATE_COLORS] =
	{ 0xFF3030ul, 0xFF8C1Aul, 0xFFE040ul, 0x00FF00ul, 0x40E8F0ul, 0x3A7DFFul, 0xFF40FFul, 0xF5F8FAul };
static int state_enemy = AE_XH_STATE_RED, state_ally = AE_XH_STATE_GREEN;

int ae_xh_state_color_of(const char *text, int fallback)
{
	int index;

	for (index = 0; text && index < AE_XH_STATE_COLORS; index++)
		if (!strcmp(text, state_color_texts[index]))
			return index;
	return fallback;
}

unsigned long ae_xh_state_color_rgb(int color)
{
	return color >= 0 && color < AE_XH_STATE_COLORS ? state_color_rgbs[color] : state_color_rgbs[0];
}

void ae_xh_state_colors_set(int enemy, int ally)
{
	state_enemy = enemy >= 0 && enemy < AE_XH_STATE_COLORS ? enemy : AE_XH_STATE_RED;
	state_ally = ally >= 0 && ally < AE_XH_STATE_COLORS ? ally : AE_XH_STATE_GREEN;
}

unsigned long ae_xh_state_rgb(unsigned long chosen, int enemy, int ally)
{
	return enemy ? state_color_rgbs[state_enemy] : ally ? state_color_rgbs[state_ally] : (chosen & 0xFFFFFFul);
}

int ae_xh_allowed_of(unsigned char ball_melee)
{
	return (ball_melee & 0x04) == 0;
}

unsigned char ae_xh_allowed_set(unsigned char ball_melee, int allowed)
{
	return (unsigned char)(allowed ? (ball_melee & ~0x04) : (ball_melee | 0x04));
}

/* ---------- drawing */

struct pass
{
	struct ae_xh_piece *pieces;
	int count, maximum;
	float scale, grow;
	unsigned long rgb;
};

static unsigned int rgba_of(const struct pass *pass, int percent)
{
	int alpha = (percent * 255 + 50) / 100;

	return (unsigned int)((pass->rgb & 0xFFFFFFul) << 8) | (unsigned int)(alpha < 0 ? 0 : alpha > 255 ? 255 : alpha);
}

static struct ae_xh_piece *piece_new(struct pass *pass)
{
	return pass->count < pass->maximum ? &pass->pieces[pass->count++] : NULL;
}

/* an axis aligned stroke on the pixel grid: c its centre line, a0 to a1 along it, w thick */
static void axis_stroke(struct pass *pass, int horizontal, float c, float a0, float a1, float w, int percent)
{
	struct ae_xh_piece *p = piece_new(pass);
	float thick, low, high, lo, hi, t;

	if (!p)
		return;
	t = floorf(w + 0.5f);
	if (t < 1.0f)
		t = 1.0f;
	t += 2.0f * floorf(pass->grow + 0.5f);
	/* (an odd thickness is centred on a pixel's middle, an even one on a corner, so its edges are whole pixels) */
	c = ((int)t & 1) ? floorf(c) + 0.5f : floorf(c + 0.5f);
	thick = t;
	low = c - thick / 2.0f;
	high = c + thick / 2.0f;
	lo = floorf(a0 + 0.5f) - floorf(pass->grow + 0.5f);
	hi = floorf(a1 + 0.5f) + floorf(pass->grow + 0.5f);
	if (hi <= lo)
		hi = lo + 1.0f;
	p->kind = AE_XH_PIECE_RECT;
	p->x0 = horizontal ? lo : low;
	p->y0 = horizontal ? low : lo;
	p->x1 = horizontal ? hi : high;
	p->y1 = horizontal ? high : hi;
	p->size = 0.0f;
	p->rgba = rgba_of(pass, percent);
}

/* any stroke; round ends. extend: the ends of an axis aligned one reach half a thickness further (corners fill) */
static void stroke(struct pass *pass, float x0, float y0, float x1, float y1, float w, int percent, int extend)
{
	float dx = x1 - x0, dy = y1 - y0;

	if (fabsf(dy) < 0.01f)
	{
		float a0 = dx < 0 ? x1 : x0, a1 = dx < 0 ? x0 : x1, e = extend ? floorf(w + 0.5f) / 2.0f : 0.0f;

		axis_stroke(pass, 1, y0, a0 - e, a1 + e, w, percent);
	}
	else if (fabsf(dx) < 0.01f)
	{
		float a0 = dy < 0 ? y1 : y0, a1 = dy < 0 ? y0 : y1, e = extend ? floorf(w + 0.5f) / 2.0f : 0.0f;

		axis_stroke(pass, 0, x0, a0 - e, a1 + e, w, percent);
	}
	else
	{
		struct ae_xh_piece *p = piece_new(pass);

		if (!p)
			return;
		p->kind = AE_XH_PIECE_SEGMENT;
		p->x0 = x0;
		p->y0 = y0;
		p->x1 = x1;
		p->y1 = y1;
		p->size = (w < 1.0f ? 1.0f : w) + 2.0f * pass->grow;
		p->rgba = rgba_of(pass, percent);
	}
}

static void disc(struct pass *pass, float x, float y, float radius, int percent)
{
	struct ae_xh_piece *p = piece_new(pass);

	if (!p)
		return;
	p->kind = AE_XH_PIECE_DISC;
	p->x0 = x;
	p->y0 = y;
	p->x1 = p->y1 = 0.0f;
	p->size = (radius < 0.75f ? 0.75f : radius) + pass->grow;
	p->rgba = rgba_of(pass, percent);
}

static void polyline(struct pass *pass, const float *xy, int count, int closed, float w, int percent)
{
	int index;

	for (index = 0; index + 1 < count; index++)
		stroke(pass, xy[2 * index], xy[2 * index + 1], xy[2 * index + 2], xy[2 * index + 3], w, percent, 1);
	if (closed && count > 2)
		stroke(pass, xy[2 * (count - 1)], xy[2 * (count - 1) + 1], xy[0], xy[1], w, percent, 1);
}

static void arc(struct pass *pass, float radius, float a0, float a1, float w, int percent)
{
	float length = fabsf(a1 - a0) * 3.14159265f / 180.0f * radius;
	int steps = (int)ceilf(length / 3.0f), index;
	float px = 0, py = 0;

	if (steps < 4)
		steps = 4;
	if (steps > 160)
		steps = 160;
	for (index = 0; index <= steps; index++)
	{
		float angle = (a0 + (a1 - a0) * (float)index / (float)steps) * 3.14159265f / 180.0f;
		float x = radius * cosf(angle), y = radius * sinf(angle);

		if (index > 0)
			stroke(pass, px, py, x, y, w, percent, 0);
		px = x;
		py = y;
	}
}

static void wedge(struct pass *pass, const float *xy, int count, int percent)
{
	float ymin = xy[1], ymax = xy[1], y;
	int index;

	for (index = 1; index < count; index++)
	{
		if (xy[2 * index + 1] < ymin)
			ymin = xy[2 * index + 1];
		if (xy[2 * index + 1] > ymax)
			ymax = xy[2 * index + 1];
	}
	for (y = floorf(ymin); y < ceilf(ymax); y += 1.0f)
	{
		float yc = y + 0.5f, hits[16];
		int n = 0, a, b;

		for (a = 0; a < count && n < 15; a++)
		{
			float x0, y0, x1, y1;

			b = (a + 1) % count;
			x0 = xy[2 * a];
			y0 = xy[2 * a + 1];
			x1 = xy[2 * b];
			y1 = xy[2 * b + 1];
			if ((y0 <= yc && y1 > yc) || (y1 <= yc && y0 > yc))
				hits[n++] = x0 + (yc - y0) / (y1 - y0) * (x1 - x0);
		}
		for (a = 0; a + 1 < n; a++)
			for (b = a + 1; b < n; b++)
				if (hits[b] < hits[a])
				{
					float t = hits[a];

					hits[a] = hits[b];
					hits[b] = t;
				}
		for (a = 0; a + 1 < n; a += 2)
		{
			struct ae_xh_piece *p = piece_new(pass);

			if (!p)
				return;
			p->kind = AE_XH_PIECE_RECT;
			p->x0 = floorf(hits[a] + 0.5f) - floorf(pass->grow + 0.5f);
			p->x1 = floorf(hits[a + 1] + 0.5f) + floorf(pass->grow + 0.5f);
			p->y0 = y - floorf(pass->grow + 0.5f);
			p->y1 = y + 1.0f + floorf(pass->grow + 0.5f);
			p->size = 0.0f;
			p->rgba = rgba_of(pass, percent);
			if (p->x1 <= p->x0)
				p->x1 = p->x0 + 1.0f;
		}
	}
}

static const float layout_angles[6][4] =
{
	{ 0, 90, 180, 270 }, { 0, 90, 180, 0 }, { 45, 135, 225, 315 }, { 0, 180, 0, 0 }, { 90, 270, 0, 0 }, { 270, 35, 145, 0 }
};
static const int layout_counts[6] = { 4, 3, 4, 2, 2, 3 };

static void lines_pass(struct pass *pass, const struct ae_xh_lines *lines, int layout, float shift, int base_percent)
{
	int index;
	float s = pass->scale;

	if (!lines->on || lines->length == 0)
		return;
	for (index = 0; index < layout_counts[layout]; index++)
	{
		float angle = layout_angles[layout][index] * 3.14159265f / 180.0f;
		float r0 = ((float)lines->offset * 0.5f + shift) * s, r1 = r0 + (float)lines->length * 0.5f * s;

		stroke(pass, r0 * cosf(angle), r0 * sinf(angle), r1 * cosf(angle), r1 * sinf(angle),
			(float)lines->thickness * 0.5f * s, lines->opacity * base_percent / 100, 0);
	}
}

static void frame_pass(struct pass *pass, const struct ae_crosshair *xh, const struct ae_xh_shape *shape, float grow_scale,
	int base_percent)
{
	float k = K * (float)xh->frame_size / 100.0f * grow_scale * pass->scale, w = (float)xh->frame_thickness * 0.5f * pass->scale;
	int index, percent = xh->frame_opacity * base_percent / 100;

	for (index = 0; index < shape->count; index++)
	{
		const struct ae_xh_prim *prim = &shape_prims[shape->first + index];
		/* (zeroed: a primitive of no points reads none, and gcc 16's -O1 sees no path that leaves it unset) */
		float xy[32] = { 0.0f };
		int n, j;

		switch (prim->kind)
		{
		case AE_XH_PRIM_RING:
			arc(pass, prim->a * k, 0.0f, 360.0f, w, percent);
			break;
		case AE_XH_PRIM_ARC:
			arc(pass, prim->a * k, prim->b, prim->c, w, percent);
			break;
		case AE_XH_PRIM_SEG:
		case AE_XH_PRIM_POLY:
		case AE_XH_PRIM_WEDGE:
			n = prim->count > 16 ? 16 : prim->count;
			for (j = 0; j < n; j++)
			{
				xy[2 * j] = shape_points[2 * (prim->first + j)] * k;
				xy[2 * j + 1] = shape_points[2 * (prim->first + j) + 1] * k;
			}
			if (prim->kind == AE_XH_PRIM_WEDGE)
				wedge(pass, xy, n, percent);
			else
				polyline(pass, xy, n, prim->closed, w, percent);
			break;
		case AE_XH_PRIM_SQUARE:
			xy[0] = -prim->a * k; xy[1] = -prim->a * k; xy[2] = prim->a * k; xy[3] = -prim->a * k;
			xy[4] = prim->a * k; xy[5] = prim->a * k; xy[6] = -prim->a * k; xy[7] = prim->a * k;
			polyline(pass, xy, 4, 1, w, percent);
			break;
		case AE_XH_PRIM_BRACKETS:
		{
			int sx, sy;
			float h = prim->a * k, arm = prim->b * k;

			for (sx = -1; sx <= 1; sx += 2)
				for (sy = -1; sy <= 1; sy += 2)
				{
					xy[0] = (float)sx * h; xy[1] = (float)sy * (h - arm);
					xy[2] = (float)sx * h; xy[3] = (float)sy * h;
					xy[4] = (float)sx * (h - arm); xy[5] = (float)sy * h;
					polyline(pass, xy, 3, 0, w, percent);
				}
			break;
		}
		}
	}
}

static void centre_pass(struct pass *pass, const struct ae_crosshair *xh, int base_percent)
{
	float r = (float)xh->centre_size * 0.5f * pass->scale;

	if (xh->centre == AE_XH_CENTRE_DOT)
		disc(pass, 0.0f, 0.0f, r, base_percent);
	else if (xh->centre == AE_XH_CENTRE_PLUS)
	{
		axis_stroke(pass, 1, 0.0f, -2.0f * r, 2.0f * r, r, base_percent);
		axis_stroke(pass, 0, 0.0f, -2.0f * r, 2.0f * r, r, base_percent);
	}
}

static int layout_of(const struct ae_xh_set *own, const struct ae_xh_set *other)
{
	return own->layout >= 0 ? own->layout : other && other->layout >= 0 ? other->layout : LAYOUT_CROSS;
}

int ae_xh_build(const struct ae_crosshair *xh, const struct ae_xh_motion *motion, unsigned long rgb, float scale,
	struct ae_xh_piece *pieces, int maximum)
{
	struct pass pass;
	const struct ae_xh_shape *shape = shape_of(xh->shape);
	float fire = xh->firing && motion ? motion->spread + motion->kick : 0.0f;
	float move = xh->movement && motion ? motion->move : 0.0f;
	int inner_layout = shape ? layout_of(&shape->inner, NULL) : LAYOUT_CROSS;
	int outer_layout = shape ? layout_of(&shape->outer, &shape->inner) : LAYOUT_CROSS;
	int mode;

	if (move > 3.0f)
		move = 3.0f;
	if (fire < 0.0f)
		fire = 0.0f;
	memset(&pass, 0, sizeof(pass));
	pass.pieces = pieces;
	pass.maximum = maximum;
	pass.scale = scale > 0.0f ? scale : 1.0f;
	for (mode = xh->outline_on ? 0 : 1; mode < 2; mode++)
	{
		int percent = xh->opacity;
		unsigned long colour = rgb;

		if (mode == 0)
		{
			pass.rgb = 0x000000ul;
			pass.grow = (float)xh->outline_thickness * 0.5f * pass.scale;
			percent = percent * xh->outline_opacity / 100;
		}
		else
		{
			pass.rgb = colour;
			pass.grow = 0.0f;
		}
		if (shape && xh->frame_on)
			frame_pass(&pass, xh, shape, 1.0f + fire / 20.0f, percent);
		lines_pass(&pass, &xh->outer, outer_layout, fire + move, percent);
		lines_pass(&pass, &xh->inner, inner_layout, fire + move, percent);
		centre_pass(&pass, xh, percent);
	}
	return pass.count;
}

float ae_xh_extent(const struct ae_crosshair *xh)
{
	struct ae_xh_piece pieces[AE_XH_PIECES];
	int count = ae_xh_build(xh, NULL, 0xFFFFFFul, 1.0f, pieces, AE_XH_PIECES), index;
	float extent = 0.0f;

	for (index = 0; index < count; index++)
	{
		float values[4], reach;
		int k;

		values[0] = fabsf(pieces[index].x0);
		values[1] = fabsf(pieces[index].y0);
		values[2] = fabsf(pieces[index].x1);
		values[3] = fabsf(pieces[index].y1);
		reach = pieces[index].kind == AE_XH_PIECE_DISC ? pieces[index].size : pieces[index].kind == AE_XH_PIECE_SEGMENT ?
			pieces[index].size / 2.0f : 0.0f;
		for (k = 0; k < 4; k++)
			if (values[k] + reach > extent)
				extent = values[k] + reach;
	}
	return extent;
}

float ae_xh_spread_pixels(float error_angle, float vertical_fov)
{
	if (!(error_angle > 0.0f) || !(vertical_fov > 0.01f) || vertical_fov > 3.0f)
		return 0.0f;
	return tanf(error_angle) / tanf(vertical_fov * 0.5f) * 540.0f;
}
