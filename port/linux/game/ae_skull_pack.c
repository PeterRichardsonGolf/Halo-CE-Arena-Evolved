/*
AE_SKULL_PACK.C

The skulls' pictures from the player's MCC install (ae_skull_pack.h). The index's format is read by ae_skull_pack_rules.c and the
pixels' by ae_medal_pack_rules.c, which check every length and offset against the file: a file that is cut short, changed by an update
or not what it should be gives no picture and the skull keeps Content Evolved's.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cseries/cseries.h"

#include "../src/ae_draw.h"
#include "../src/ae_mcc.h"

#include "ae_medal_pack_rules.h"
#include "ae_skull_pack.h"
#include "ae_skull_pack_rules.h"
#include "halo_skulls.h"

void platform_log(char const *format, ...);
FILE *ae_mcc_install_open(char const *path);

/* the most each file may be (they are 20 KB and 686 KB in MCC): a file larger than this is not the one */
#define MAXIMUM_INDEX_BYTES (1024 * 1024)
#define MAXIMUM_PIXELS_BYTES (16 * 1024 * 1024)

enum { PICTURE_UNTRIED, PICTURE_HAS, PICTURE_NONE };

static struct
{
	boolean tried;
	unsigned char *index;
	size_t index_size;
	unsigned char *pixels;
	size_t pixels_size;
	unsigned char state[NUMBER_OF_SKULLS];
	struct ae_medal_record record[NUMBER_OF_SKULLS];
	int image[NUMBER_OF_SKULLS];
	unsigned short image_width[NUMBER_OF_SKULLS], image_height[NUMBER_OF_SKULLS];
	boolean image_bad[NUMBER_OF_SKULLS];
} pack;

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

/* a file under the install's data folder: as Steam names it on Linux (data/ui, lower case) first, then as on Windows (Data/UI) */
static void *read_install_file(char const *root, char const *lower, char const *upper, size_t most, size_t *size)
{
	char path[AE_MCC_PATH_SIZE + 64];
	void *data;

	snprintf(path, sizeof(path), "%s/data/%s", root, lower);
	if ((data = read_file(path, most, size)) != NULL)
		return data;
	snprintf(path, sizeof(path), "%s/Data/%s", root, upper);
	return read_file(path, most, size);
}

static void files_read(void)
{
	char const *root;

	if (pack.tried)
		return;
	pack.tried = TRUE;
	/* (game.mcc_use no: the player has asked for none of MCC's files to be read) */
	if (ae_mcc_use() == AE_MCC_USE_NO || !(root = ae_mcc_root()))
		return;
	pack.index = read_install_file(root, "ui/texturepacks/skullstexturepack.perm.bin", "UI/texturepacks/skullstexturepack.perm.bin",
		MAXIMUM_INDEX_BYTES, &pack.index_size);
	pack.pixels = read_install_file(root, "ui/texturepacks/skullstexturepack.temp.bin", "UI/texturepacks/skullstexturepack.temp.bin",
		MAXIMUM_PIXELS_BYTES, &pack.pixels_size);
	if (!pack.index || !pack.pixels || ae_skull_index_count(pack.index, pack.index_size) < 1 ||
		!ae_medal_pixels_valid(pack.pixels, pack.pixels_size))
	{
		platform_log("skulls: MCC's skull pictures could not be read from %s: Content Evolved's are used", root);
		/* (the game's free halts on a null) */
		if (pack.index)
			free(pack.index);
		if (pack.pixels)
			free(pack.pixels);
		pack.index = NULL;
		pack.pixels = NULL;
		return;
	}
	platform_log("skulls: %d pictures in MCC's folder %s", ae_skull_index_count(pack.index, pack.index_size), root);
}

int ae_skull_pack_has_picture(int skull)
{
	int found;

	if (skull < 0 || skull >= NUMBER_OF_SKULLS || pack.image_bad[skull])
		return 0;
	if (pack.state[skull] != PICTURE_UNTRIED)
		return pack.state[skull] == PICTURE_HAS;
	files_read();
	pack.state[skull] = PICTURE_NONE;
	if (!pack.index || !pack.pixels)
		return 0;
	if ((found = ae_skull_index_find(pack.index, pack.index_size, skull_descriptions[skull].key)) < 0 ||
		!ae_skull_index_record(pack.index, pack.index_size, found, &pack.record[skull]) ||
		!ae_medal_record_fits(&pack.record[skull], pack.pixels_size))
		return 0;
	pack.state[skull] = PICTURE_HAS;
	return 1;
}

int ae_skull_pack_menu_image(int skull, int *width, int *height)
{
	struct ae_medal_record const *record;
	unsigned char *rgba;
	int id, trimmed_width, trimmed_height;

	if (width)
		*width = 0;
	if (height)
		*height = 0;
	if (skull < 0 || skull >= NUMBER_OF_SKULLS || !ae_draw_available() || !ae_skull_pack_has_picture(skull))
		return 0;
	/* (an image the drawing freed for a newer one is made again) */
	if (pack.image[skull] && ae_draw_image_live(pack.image[skull]))
	{
		if (width)
			*width = pack.image_width[skull];
		if (height)
			*height = pack.image_height[skull];
		return pack.image[skull];
	}
	record = &pack.record[skull];
	rgba = malloc((size_t)record->width * record->height * 4);
	if (!rgba)
		return 0;
	if (!ae_medal_decode_bc3(pack.pixels + record->offset, record->size, record->width, record->height, rgba))
	{
		free(rgba);
		pack.image_bad[skull] = TRUE;
		return 0;
	}
	if (!ae_skull_rgba_trim(rgba, (int)record->width, (int)record->height, &trimmed_width, &trimmed_height))
	{
		free(rgba);
		pack.image_bad[skull] = TRUE;
		return 0;
	}
	id = ae_draw_image_load(rgba, trimmed_width, trimmed_height);
	free(rgba);
	if (id)
	{
		pack.image_width[skull] = (unsigned short)trimmed_width;
		pack.image_height[skull] = (unsigned short)trimmed_height;
		if (width)
			*width = trimmed_width;
		if (height)
			*height = trimmed_height;
	}
	pack.image[skull] = id;
	return id;
}
