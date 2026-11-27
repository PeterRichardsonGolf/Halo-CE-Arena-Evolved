/* ae_forge_maps_rules.h */

#include <stdio.h>
#include <string.h>

#include "ae_forge_maps_rules.h"

int ae_forge_maps_name_valid(char const *name)
{
	int length = 0;

	if (!name)
		return 0;
	for (; name[length]; length++)
	{
		char c = name[length];

		if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_') || length >= AE_FORGE_MAP_NAME_MAX)
			return 0;
	}
	return length > 0;
}

int ae_forge_maps_name_clean(char const *typed, char *name, int size)
{
	int at = 0;

	if (!name || size <= 0)
		return 0;
	for (; typed && *typed && at < AE_FORGE_MAP_NAME_MAX && at < size - 1; typed++)
	{
		char c = *typed;

		if (c >= 'A' && c <= 'Z')
			c = (char)(c + 32);
		else if (c == ' ' || c == '-')
			c = '_';
		if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')
			name[at++] = c;
	}
	name[at] = 0;
	return at;
}

/* the text after a line's label, up to its end of line, into out */
static int line_value(char const *line, char const *label, char *out, int size)
{
	size_t length = strlen(label);
	int at = 0;

	if (strncmp(line, label, length))
		return 0;
	line += length;
	while (*line == ' ')
		line++;
	for (; *line && *line != '\r' && *line != '\n' && at < size - 1; line++)
		out[at++] = *line;
	out[at] = 0;
	return 1;
}

static int whole_number(char const *text)
{
	int value = 0;

	for (; *text >= '0' && *text <= '9' && value < 100000000; text++)
		value = value * 10 + (*text - '0');
	return value;
}

int ae_forge_maps_facts_read(char const *text, struct ae_forge_maps_facts *facts)
{
	static char const header[] = "[OpenCE Forge Map]";
	char value[64];
	char const *line = text;

	memset(facts, 0, sizeof(*facts));
	if (!text || strncmp(text, header, sizeof(header) - 1))
		return 0;
	while (*line)
	{
		if (!strncmp(line, "[ForgeState]", 12))
			break;
		if (line_value(line, "Map Name:", facts->name, (int)sizeof(facts->name)) ||
			line_value(line, "Base Map:", facts->base, (int)sizeof(facts->base)))
		{
		}
		else if (line_value(line, "Objects Placed:", value, (int)sizeof(value)))
			facts->objects = whole_number(value);
		else if (line_value(line, "AI Squads:", value, (int)sizeof(value)))
			facts->squads = whole_number(value);
		while (*line && *line != '\n')
			line++;
		if (*line)
			line++;
	}
	return 1;
}

int ae_forge_maps_kind(long version)
{
	return version == 5 ? AE_FORGE_MAP_XBOX : version == 609 ? AE_FORGE_MAP_CE : AE_FORGE_MAP_UNREADABLE;
}

void ae_forge_maps_level_name(int kind, char const *file, char *level, int size)
{
	size_t length = file ? strlen(file) : 0;

	if (size <= 0)
		return;
	level[0] = 0;
	if (!length || kind == AE_FORGE_MAP_UNREADABLE || (int)length + 4 >= size)
		return;
	memcpy(level, file, length);
	if (kind == AE_FORGE_MAP_CE)
		memcpy(level + length, "@ce", 4);
	else
		level[length] = 0;
}

int ae_forge_maps_shown(int show, int downloaded)
{
	return show == AE_FORGE_SHOW_ALL || (show == AE_FORGE_SHOW_DOWNLOADED) == (downloaded != 0);
}

static int compare_text(char const *a, char const *b)
{
	for (; *a && *b; a++, b++)
	{
		int x = *a >= 'A' && *a <= 'Z' ? *a + 32 : *a, y = *b >= 'A' && *b <= 'Z' ? *b + 32 : *b;

		if (x != y)
			return x < y ? -1 : 1;
	}
	return *a == *b ? 0 : *a ? 1 : -1;
}

int ae_forge_maps_compare(int sort, char const *name_a, char const *base_a, unsigned long saved_a, char const *name_b,
	char const *base_b, unsigned long saved_b)
{
	int order = 0;

	if (sort == AE_FORGE_SORT_SAVED && saved_a != saved_b)
		return saved_a > saved_b ? -1 : 1;
	if (sort == AE_FORGE_SORT_BASE)
		order = compare_text(base_a, base_b);
	return order ? order : compare_text(name_a, name_b);
}

int ae_forge_maps_name_is_stock(char const *name)
{
	static char const *const stock[] = { "a10", "a30", "a50", "b30", "b40", "c10", "c20", "c40", "d20", "d40", "bloodgulch",
		"beavercreek", "boardingaction", "carousel", "chillout", "damnation", "dangercanyon", "deathisland", "gephyrophobia",
		"hangemhigh", "icefields", "infinity", "longest", "prisoner", "putput", "ratrace", "sidewinder", "timberland",
		"wizard", "ui" };
	int each;

	for (each = 0; name && each < (int)(sizeof(stock) / sizeof(stock[0])); each++)
	{
		if (!compare_text(name, stock[each]))
			return 1;
	}
	return 0;
}

static unsigned long little(unsigned char const *at, int bytes)
{
	unsigned long value = 0;
	int each;

	for (each = bytes - 1; each >= 0; each--)
		value = value << 8 | at[each];
	return value;
}

int ae_forge_maps_bmp_rgba(unsigned char const *bmp, long size, unsigned char *rgba, long capacity, int *width, int *height)
{
	long data, row_bytes, x, y, w, h;
	int bits, top_down;

	if (!bmp || size < 54 || bmp[0] != 'B' || bmp[1] != 'M' || little(bmp + 14, 4) < 40)
		return 0;
	w = (long)(int)little(bmp + 18, 4);
	h = (long)(int)little(bmp + 22, 4);
	/* (a height the format's top-down sign cannot be taken off of, or past the pictures this reads: refused before the sign
	is taken off) */
	if (h < -1024 || h > 1024)
		return 0;
	top_down = h < 0;
	if (top_down)
		h = -h;
	bits = (int)little(bmp + 28, 2);
	data = (long)little(bmp + 10, 4);
	if (w <= 0 || h <= 0 || w > 1024 || h > 1024 || little(bmp + 26, 2) != 1 || (bits != 24 && bits != 32) ||
		little(bmp + 30, 4) != 0 || w * h * 4 > capacity)
		return 0;
	row_bytes = (w * (bits / 8) + 3) / 4 * 4;
	if (data < 54 || data + row_bytes * h > size)
		return 0;
	for (y = 0; y < h; y++)
	{
		unsigned char const *from = bmp + data + row_bytes * (top_down ? y : h - 1 - y);
		unsigned char *to = rgba + y * w * 4;

		for (x = 0; x < w; x++, from += bits / 8, to += 4)
		{
			to[0] = from[2];
			to[1] = from[1];
			to[2] = from[0];
			to[3] = 255;
		}
	}
	*width = (int)w;
	*height = (int)h;
	return 1;
}

void ae_forge_maps_duration_text(long seconds, char *text, int size)
{
	long minutes = seconds > 0 ? seconds / 60 : 0, hours = minutes / 60;

	if (size <= 0)
		return;
	if (hours == 0)
		snprintf(text, (size_t)size, "%ld MIN", minutes);
	else if (minutes % 60 == 0)
		snprintf(text, (size_t)size, "%ld H", hours);
	else
		snprintf(text, (size_t)size, "%ld H %ld MIN", hours, minutes % 60);
}
