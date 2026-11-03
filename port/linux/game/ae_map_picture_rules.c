/*
AE_MAP_PICTURE_RULES.C

Where a map's own picture is, and which files are drawn (ae_map_picture_rules.h).
*/

#include <stdio.h>
#include <string.h>

#include "ae_map_picture_rules.h"

static int plain_name(char const *file)
{
	if (!file || !file[0] || !strcmp(file, ".") || !strcmp(file, ".."))
		return 0;
	return !strpbrk(file, "/\\:");
}

int ae_map_picture_path(char const *map_path, char const *file, char *out, unsigned int size)
{
	char const *slash, *backslash, *last;
	char separator;
	size_t folder;
	int written;

	if (!out || !size)
		return 0;
	out[0] = 0;
	if (!map_path || !plain_name(file))
		return 0;
	slash = strrchr(map_path, '/');
	backslash = strrchr(map_path, '\\');
	last = !slash ? backslash : !backslash ? slash : slash > backslash ? slash : backslash;
	if (!last)
		return 0;
	separator = *last;
	folder = (size_t)(last - map_path) + 1;
	written = snprintf(out, size, "%.*spreviews%c%s.png", (int)folder, map_path, separator, file);
	if (written < 0 || (unsigned int)written >= size)
	{
		out[0] = 0;
		return 0;
	}
	return 1;
}

static unsigned int big_endian(unsigned char const *bytes)
{
	return (unsigned int)bytes[0] << 24 | (unsigned int)bytes[1] << 16 | (unsigned int)bytes[2] << 8 | bytes[3];
}

int ae_map_picture_png_usable(unsigned char const *header, unsigned int header_size, unsigned int file_size)
{
	unsigned int width, height;

	if (!header || header_size < AE_MAP_PICTURE_HEADER_SIZE || file_size < AE_MAP_PICTURE_HEADER_SIZE ||
		file_size > AE_MAP_PICTURE_MAXIMUM_FILE)
	{
		return 0;
	}
	/* (the signature, then IHDR's length 13 and its name) */
	if (memcmp(header, "\x89PNG\r\n\x1a\n", 8) || big_endian(header + 8) != 13 || memcmp(header + 12, "IHDR", 4))
		return 0;
	width = big_endian(header + 16);
	height = big_endian(header + 20);
	/* (depth 8, colour type 6: RGBA, compression 0, filter 0, interlace 0) */
	return width >= 1 && height >= 1 && width <= AE_MAP_PICTURE_MAXIMUM_SIDE && height <= AE_MAP_PICTURE_MAXIMUM_SIDE &&
		header[24] == 8 && header[25] == 6 && header[26] == 0 && header[27] == 0 && header[28] == 0;
}
