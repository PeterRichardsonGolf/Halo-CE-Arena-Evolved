/*
AE_MEDAL_PACK.C

The medals' pictures and names from the player's MCC install (ae_medal_pack.h). The files' formats are read by
ae_medal_pack_rules.c, which checks every length and offset against the file: a file that is cut short, changed by an update or
not what it should be gives no picture and the medal is shown by its name.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cseries/cseries.h"
#include "math/real_math.h"
#include "bitmaps/bitmaps.h"
#include "bitmaps/bitmap_group.h"
#include "cache/texture_cache.h"
#include "rasterizer/rasterizer.h"
#include "rasterizer/xbox/rasterizer_xbox_hardware_bitmaps.h"

#include "../src/ae_mcc.h"
#include "../src/ae_medal_art.h"

#include "ae_medal_draw_rules.h"
#include "ae_medal_pack.h"
#include "ae_medal_pack_rules.h"
#include "ae_medals.h"

void platform_log(char const *format, ...);

/* (bitmaps.c's _bitmap_format_a8r8g8b8, which no header has: hud_item_timers.c's glyphs the same) */
#define MEDAL_BITMAP_FORMAT 11

/* the most each file may be (they are 137 KB, 1.8 MB and 340 KB in MCC): a file larger than this is not the one */
#define MAXIMUM_INDEX_BYTES (4 * 1024 * 1024)
#define MAXIMUM_PIXELS_BYTES (32 * 1024 * 1024)
#define MAXIMUM_CAREER_BYTES (16 * 1024 * 1024)

enum { PICTURE_UNTRIED, PICTURE_HAS, PICTURE_NONE };

static struct
{
	boolean tried;
	unsigned char *index;
	size_t index_size;
	unsigned char *pixels;
	size_t pixels_size;
	char *career;
	size_t career_size;
	/* each medal: whether it has a picture in the files, the record it is, the texture made for it */
	unsigned char state[AE_MEDAL_COUNT];
	float caption[AE_MEDAL_COUNT][3];
	struct ae_medal_record record[AE_MEDAL_COUNT];
	/* the medals MCC has no picture for, where AE has its own (port/assets/medals): the table's place plus one, else 0 */
	unsigned char art[AE_MEDAL_COUNT];
	struct bitmap_data *bitmap[AE_MEDAL_COUNT];
	boolean bitmap_failed[AE_MEDAL_COUNT];
} pack;

/* a whole file, no larger than most bytes; NULL for none (missing, empty, too large, not read to its end) */
FILE *ae_mcc_install_open(char const *path);

static void *read_file(char const *path, size_t most, size_t *size)
{
	/* (a path of the host, not one of the game's drives) */
	FILE *file = ae_mcc_install_open(path);
	unsigned char *data = NULL;
	long length;

	*size = 0;
	if (!file)
		return NULL;
	if (fseek(file, 0, SEEK_END) == 0 && (length = ftell(file)) > 0 && (size_t)length <= most && fseek(file, 0, SEEK_SET) == 0)
	{
		data = malloc((size_t)length);
		if (data && fread(data, 1, (size_t)length, file) == (size_t)length)
			*size = (size_t)length;
		else
		{
			if (data)
				free(data);
			data = NULL;
		}
	}
	fclose(file);
	return data;
}

int halo_server_mode(void);

static void files_read(void)
{
	char const *root;
	char path[AE_MCC_PATH_SIZE + 64];

	if (pack.tried)
		return;
	pack.tried = TRUE;
	/* (game.mcc_use no: the player has asked for none of MCC's files to be read) */
	if (ae_mcc_use() == AE_MCC_USE_NO || !(root = ae_mcc_root()))
	{
		platform_log("medals: no MCC folder to take the pictures from: the medals are shown by name");
		return;
	}
	snprintf(path, sizeof(path), "%s/Data/UI/texturepacks/medalstexturepack.perm.bin", root);
	pack.index = read_file(path, MAXIMUM_INDEX_BYTES, &pack.index_size);
	snprintf(path, sizeof(path), "%s/Data/UI/texturepacks/medalstexturepack.temp.bin", root);
	pack.pixels = read_file(path, MAXIMUM_PIXELS_BYTES, &pack.pixels_size);
	snprintf(path, sizeof(path), "%s/Data/careerdb/careerdb.xml", root);
	pack.career = read_file(path, MAXIMUM_CAREER_BYTES, &pack.career_size);
	if (!pack.index || !pack.pixels || !pack.career || ae_medal_index_count(pack.index, pack.index_size) < 1 ||
		!ae_medal_pixels_valid(pack.pixels, pack.pixels_size))
	{
		platform_log("medals: MCC's medal pictures could not be read from %s: the medals are shown by name", root);
		/* (the game's free halts on a null) */
		if (pack.index)
			free(pack.index);
		if (pack.pixels)
			free(pack.pixels);
		if (pack.career)
			free(pack.career);
		pack.index = NULL;
		pack.pixels = NULL;
		pack.career = NULL;
		return;
	}
	platform_log("medals: %d pictures in MCC's folder %s", ae_medal_index_count(pack.index, pack.index_size), root);
}

/* AE's own picture of a medal, which MCC's files have none of: the table's place plus one, and the record of its size; 0 for none */
static unsigned char own_art_find(int medal, struct ae_medal_record *record)
{
	char name[AE_MEDAL_NAME_SIZE];
	unsigned int index;

	if (ae_medal_art_name_from_key(ae_medal_key(medal), name, sizeof(name)) < 1)
		return 0;
	for (index = 0; index < ae_medal_art_count && index < 255; index++)
	{
		struct ae_medal_art const *art = &ae_medal_art[index];

		if (art->name && !strcmp(art->name, name) && art->rgba && art->width > 0 && art->width <= AE_MEDAL_PICTURE_MOST &&
			art->height > 0 && art->height <= AE_MEDAL_PICTURE_MOST)
		{
			memset(record, 0, sizeof(*record));
			record->width = art->width;
			record->height = art->height;
			return (unsigned char)(index + 1);
		}
	}
	return 0;
}

boolean ae_medal_pack_has_picture(int medal)
{
	char icon[AE_MEDAL_NAME_SIZE];
	char const *key;
	int found;

	if (medal < 0 || medal >= AE_MEDAL_COUNT)
		return FALSE;
	if (pack.state[medal] != PICTURE_UNTRIED)
		return (boolean)(pack.state[medal] == PICTURE_HAS);
	files_read();
	pack.state[medal] = PICTURE_NONE;
	key = ae_medal_key(medal);
	if (!key || !pack.index || !pack.pixels || !pack.career)
		return FALSE;
	if (!ae_medal_career_icon(pack.career, pack.career_size, key, icon, sizeof(icon)) ||
		(found = ae_medal_index_find(pack.index, pack.index_size, icon)) < 0 ||
		!ae_medal_index_record(pack.index, pack.index_size, found, &pack.record[medal]) ||
		!ae_medal_record_fits(&pack.record[medal], pack.pixels_size))
	{
		/* (MCC's files are read and have none for it: AE's own picture, where it has one) */
		if ((pack.art[medal] = own_art_find(medal, &pack.record[medal])) != 0)
		{
			pack.state[medal] = PICTURE_HAS;
			return TRUE;
		}
		platform_log("medals: no picture for %s", key);
		return FALSE;
	}
	pack.state[medal] = PICTURE_HAS;
	return TRUE;
}

/* the texture: the picture in the corner of a 128 square (a power of two, as the game's bitmaps are), premultiplied by its alpha
as the HUD's blend takes it. NULL when it cannot be made */
static struct bitmap_data *bitmap_make(int medal)
{
	struct ae_medal_record const *record = &pack.record[medal];
	unsigned char *rgba = malloc((size_t)record->width * record->height * 4);
	struct bitmap_data *bitmap = NULL;

	if (!rgba)
		return NULL;
	if (pack.art[medal])
	{
		/* (AE's own: raw pixels, as the generated table holds them) */
		memcpy(rgba, ae_medal_art[pack.art[medal] - 1].rgba, (size_t)record->width * record->height * 4);
	}
	else if (!ae_medal_decode_bc3(pack.pixels + record->offset, record->size, record->width, record->height, rgba))
	{
		free(rgba);
		return NULL;
	}
	ae_medal_caption_color(rgba, record->width, record->height, pack.caption[medal]);
	bitmap = bitmap_2d_new(AE_MEDAL_PICTURE_MOST, AE_MEDAL_PICTURE_MOST, 0, MEDAL_BITMAP_FORMAT);
	if (bitmap && (!bitmap->base_address || !rasterizer_bitmap_new(bitmap)))
	{
		bitmap_delete(bitmap);
		bitmap = NULL;
	}
	if (bitmap)
	{
		unsigned int x, y;

		for (y = 0; y < AE_MEDAL_PICTURE_MOST; y++)
		{
			pixel32 *row = (pixel32 *)bitmap_2d_address(bitmap, 0, (short)y, 0);

			for (x = 0; x < AE_MEDAL_PICTURE_MOST; x++)
			{
				if (x < record->width && y < record->height)
				{
					unsigned char const *in = rgba + ((size_t)y * record->width + x) * 4;
					unsigned int alpha = in[3];

					row[x] = (pixel32)((alpha << 24) | (((unsigned int)in[0] * alpha / 255u) << 16) |
						(((unsigned int)in[1] * alpha / 255u) << 8) | ((unsigned int)in[2] * alpha / 255u));
				}
				else
					row[x] = 0;
			}
		}
		rasterizer_bitmap_changed(bitmap);
	}
	free(rgba);
	return bitmap;
}

struct bitmap_data const *ae_medal_pack_picture(int medal, real_rectangle2d *clip)
{
	if (!ae_medal_pack_has_picture(medal))
		return NULL;
	if (!pack.bitmap[medal] && !pack.bitmap_failed[medal])
	{
		pack.bitmap[medal] = bitmap_make(medal);
		if (!pack.bitmap[medal])
		{
			pack.bitmap_failed[medal] = TRUE;
			platform_log("medals: no texture for %s: its name is shown", ae_medal_key(medal));
		}
	}
	/* (none without a renderer: debug.null_renderer) */
	if (!pack.bitmap[medal] || !_texture_cache_bitmap_get_hardware_format(pack.bitmap[medal], FALSE, TRUE))
		return NULL;
	clip->x0 = 0.0f;
	clip->y0 = 0.0f;
	clip->x1 = (real)pack.record[medal].width / (real)AE_MEDAL_PICTURE_MOST;
	clip->y1 = (real)pack.record[medal].height / (real)AE_MEDAL_PICTURE_MOST;
	return pack.bitmap[medal];
}

void ae_medal_pack_caption(int medal, real_argb_color *color)
{
	color->red = medal >= 0 && medal < AE_MEDAL_COUNT ? pack.caption[medal][0] : 1.0f;
	color->green = medal >= 0 && medal < AE_MEDAL_COUNT ? pack.caption[medal][1] : 1.0f;
	color->blue = medal >= 0 && medal < AE_MEDAL_COUNT ? pack.caption[medal][2] : 1.0f;
}

void ae_medal_pack_preload(void)
{
	int medal;

	if (halo_server_mode())
		return;
	for (medal = 0; medal < AE_MEDAL_COUNT; medal++)
	{
		real_rectangle2d clip;

		if (ae_medal_pack_has_picture(medal))
			ae_medal_pack_picture(medal, &clip);
	}
}

void ae_medal_pack_name(int medal, wchar_t *name, int size, boolean upper)
{
	char text[AE_MEDAL_NAME_SIZE * 2];
	int index, length;

	if (!name || size < 1)
		return;
	name[0] = 0;
	if (medal < 0 || medal >= AE_MEDAL_COUNT)
		return;
	length = ae_medal_name_from_key(ae_medal_key(medal), text, sizeof(text), upper);
	for (index = 0; index < length && index < size - 1; index++)
		name[index] = (wchar_t)(unsigned char)text[index];
	name[index] = 0;
}
