/*
ae_bitmap_dump.c: a test aid. debug.dump_bitmap names a bitmap tag (a part of its name, for example "burning flame");
the first time that bitmap is ready to draw, its decoded pixels are written to dump_bitmap.<tag index>.<ce or mcc>.raw in the current folder
and one line in the log gives its size, format, flags and a checksum, so the same tag read from two kinds of map file
can be compared byte for byte. Off (empty) in a normal game.
*/

#include "cseries.h"
#include "cseries/errors.h"
#include "tag_files/tag_files.h"
const char *config_string(const char *name); /* port/linux/src/port_config.c */
#include "bitmaps/bitmap_group.h"
#include "tag_files/tag_groups.h"
#include "ae_bitmap_dump.h"

#include <stdio.h>
#include <string.h>

#define MAXIMUM_DUMPS 8

void ae_bitmap_dump(
	struct bitmap_data const *bitmap,
	void const *pixels,
	unsigned long bytes,
	char const *source)
{
	static long done[MAXIMUM_DUMPS];
	static long done_count = 0;
	char const *wanted = config_string("debug.dump_bitmap");
	char const *name;
	char file[256];
	unsigned long checksum = 2166136261UL;
	unsigned long index;
	FILE *out;
	long slot;

	if (!wanted || !wanted[0] || !bitmap || !pixels || !bytes)
		return;
	name = tag_get_name(bitmap->tag_index);
	if (!name || !strstr(name, wanted))
		return;
	for (slot = 0; slot < done_count; slot++)
		if (done[slot] == bitmap->tag_index * 4 + (source[0] == 'm' ? 1 : (source[strlen(source) - 1] == 'd' ? 2 : 0)))
			return;
	if (done_count >= MAXIMUM_DUMPS)
		return;
	done[done_count++] = bitmap->tag_index * 4 + (source[0] == 'm' ? 1 : (source[strlen(source) - 1] == 'd' ? 2 : 0));
	for (index = 0; index < bytes; index++)
		checksum = (checksum ^ ((unsigned char const *)pixels)[index]) * 16777619UL;
	snprintf(file, sizeof(file), "dump_bitmap.%ld.%s.raw", (long)bitmap->tag_index, source);
	out = fopen(file, "wb");
	if (out)
	{
		fwrite(pixels, 1, bytes, out);
		fclose(out);
	}
	error(_error_silent, "bitmap dump (%s): %s %dx%d format %d flags 0x%x type %d bytes %lu fnv %08lx -> %s",
		source, name, (int)bitmap->width, (int)bitmap->height, (int)bitmap->format, (unsigned)bitmap->flags,
		(int)bitmap->type, bytes, checksum, file);
}
