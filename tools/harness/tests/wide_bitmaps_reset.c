#include "harness.h"
/* ae_wide_bitmaps.c's table of the wide bitmaps it made narrower (16 at most): a map's tags unloaded forget them, so
the next Custom Edition map's wide bitmaps are made narrower too (before, the table filled over several maps and
later maps' wide bitmaps were left too wide: drawn flat) */

struct bitmap_data { short width, height, depth, type, format, mipmap_count; long pixels_offset, pixels_size, tag_index; };
static boolean cache_file_tags_are_ce(void) { return TRUE; }
static void platform_log(char const *format, ...) { }
static char const *tag_get_name(long tag_index) { return "bitmap"; }
static int ae_wide_levels_to_skip(long width, long height, long mipmaps, long maximum) { return 0; }
static long ae_wide_level_bytes(int format, long width, long height) { return width * height * 4; }
#include "under_test.inc"

static struct bitmap_data maps[3][8];

static void load(struct bitmap_data *bitmaps, long first_tag)
{
	int index;

	for (index = 0; index < 8; index++)
	{
		struct bitmap_data *bitmap = &bitmaps[index];

		memset(bitmap, 0, sizeof(*bitmap));
		bitmap->width = 15360;
		bitmap->height = 256;
		bitmap->depth = 1;
		bitmap->format = 11;
		bitmap->pixels_size = 15360L * 256 * 4;
		bitmap->tag_index = first_tag + index;
		ae_wide_bitmap_prepare(bitmap);
	}
}

int main(int argc, char **argv)
{
	char const *case_name = argv[1];
	int map, index;

	CASE("maps-in-turn")
	{
		for (map = 0; map < 3; map++)
		{
			load(maps[map], map * 100);
			for (index = 0; index < 8; index++)
				CHECK(maps[map][index].width <= 4096, "map %d's bitmap %d left %d wide", map, index, maps[map][index].width);
			ae_wide_bitmaps_tags_unloaded();
		}
		return 0;
	}
	fprintf(stderr, "unknown case %s\n", case_name);
	return 2;
}
