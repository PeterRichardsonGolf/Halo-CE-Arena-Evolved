/* The rank pictures of the player's MCC install (see the header). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/ae_mcc.h"
#include "../src/ae_draw.h"
#include "ae_mcc_rank.h"
#include "ae_mcc_rank_rules.h"

void platform_log(char const *format, ...);
FILE *ae_mcc_install_open(char const *path);

/* the index file is 0.5 MB in MCC: a larger one is not the one */
#define MAXIMUM_INDEX_BYTES (4 * 1024 * 1024)
#define PACK_INDEX "ui/texturepacks/mainmenuandcampaigntexturepack.perm.bin"
#define PACK_PIXELS "ui/texturepacks/mainmenuandcampaigntexturepack.temp.bin"

static struct
{
	int tried, usable;
	unsigned char *index;
	size_t index_size;
	char pixels_path[AE_MCC_PATH_SIZE + 96];
	size_t pixels_size;
	int image[2][AE_MCC_RANK_LAST + 1];
	int image_failed[2][AE_MCC_RANK_LAST + 1];
	int image_width[2], image_height[2];
} pack;

/* a file under the install's data folder as Steam names it on Linux (data/ui, lower case), then as on Windows (Data/UI); its path
into path, the file open */
static FILE *install_open(char const *root, char const *name, char *path, size_t path_size)
{
	FILE *file;
	char upper[160];

	snprintf(path, path_size, "%s/data/%s", root, name);
	if ((file = ae_mcc_install_open(path)) != NULL)
		return file;
	snprintf(upper, sizeof(upper), "%s", name);
	if (!strncmp(upper, "ui/", 3))
		memcpy(upper, "UI/", 3);
	snprintf(path, path_size, "%s/Data/%s", root, upper);
	return ae_mcc_install_open(path);
}

/* the length of an open file, 0 for none */
static size_t file_length(FILE *file)
{
	long length;

	if (fseek(file, 0, SEEK_END) != 0 || (length = ftell(file)) <= 0)
		return 0;
	return (size_t)length;
}

static void files_read(void)
{
	char const *root;
	char path[AE_MCC_PATH_SIZE + 96];
	FILE *file;
	unsigned char header[AE_MEDAL_PIXELS_HEADER];

	if (pack.tried)
		return;
	pack.tried = 1;
	/* (game.mcc_use no: the player has asked for none of MCC's files to be read) */
	if (ae_mcc_use() == AE_MCC_USE_NO || !(root = ae_mcc_root()))
	{
		platform_log("mcc rank: no MCC folder to take the rank pictures from: ranks are shown as numbers");
		return;
	}
	file = install_open(root, PACK_INDEX, path, sizeof(path));
	if (file)
	{
		size_t length = file_length(file);

		if (length > 0 && length <= MAXIMUM_INDEX_BYTES && fseek(file, 0, SEEK_SET) == 0 && (pack.index = malloc(length)) != NULL)
		{
			if (fread(pack.index, 1, length, file) == length)
				pack.index_size = length;
			else
			{
				free(pack.index);
				pack.index = NULL;
			}
		}
		fclose(file);
	}
	file = install_open(root, PACK_PIXELS, pack.pixels_path, sizeof(pack.pixels_path));
	if (file)
	{
		/* (the pixels' file is 47 MB: only its header is read here, each picture's blocks when asked for) */
		pack.pixels_size = file_length(file);
		if (pack.pixels_size < sizeof(header) || fseek(file, 0, SEEK_SET) != 0 || fread(header, 1, sizeof(header), file) != sizeof(header) ||
			!ae_medal_pixels_valid(header, sizeof(header)))
			pack.pixels_size = 0;
		fclose(file);
	}
	if (pack.index)
	{
		struct ae_medal_record record;

		pack.usable = pack.pixels_size > 0 && ae_mcc_rank_record(pack.index, pack.index_size, AE_MCC_RANK_FIRST, 0, &record) &&
			ae_mcc_rank_record_fits(&record, pack.pixels_size) &&
			ae_mcc_rank_record(pack.index, pack.index_size, AE_MCC_RANK_LAST, 1, &record) &&
			ae_mcc_rank_record_fits(&record, pack.pixels_size);
	}
	if (!pack.usable)
	{
		platform_log("mcc rank: the rank pictures could not be read from %s: ranks are shown as numbers", root);
		if (pack.index)
			free(pack.index);
		pack.index = NULL;
		return;
	}
	platform_log("mcc rank: pictures of rank %d .. %d (small and large) in MCC's folder %s", AE_MCC_RANK_FIRST, AE_MCC_RANK_LAST, root);
}

int ae_mcc_rank_available(void)
{
	files_read();
	return pack.usable;
}

int ae_mcc_rank_rgba(int rank, int large, unsigned char *rgba, size_t size, int *width, int *height)
{
	struct ae_medal_record record;
	unsigned char *blocks;
	FILE *file;
	int done = 0;

	large = large ? 1 : 0;
	if (!ae_mcc_rank_available() || !rgba || !ae_mcc_rank_record(pack.index, pack.index_size, rank, large, &record) ||
		!ae_mcc_rank_record_fits(&record, pack.pixels_size) || size < (size_t)record.width * record.height * 4)
		return 0;
	if (!(blocks = malloc(record.size)))
		return 0;
	if ((file = ae_mcc_install_open(pack.pixels_path)) != NULL)
	{
		done = fseek(file, (long)record.offset, SEEK_SET) == 0 && fread(blocks, 1, record.size, file) == record.size &&
			ae_medal_decode_bc3(blocks, record.size, record.width, record.height, rgba);
		fclose(file);
	}
	free(blocks);
	if (done)
	{
		if (width)
			*width = (int)record.width;
		if (height)
			*height = (int)record.height;
	}
	return done;
}

int ae_mcc_rank_menu_image(int rank, int large, int *width, int *height)
{
	unsigned char *rgba;
	int w = 0, h = 0, id;

	large = large ? 1 : 0;
	if (rank < AE_MCC_RANK_FIRST || rank > AE_MCC_RANK_LAST || pack.image_failed[large][rank] || !ae_draw_available() ||
		!ae_mcc_rank_available())
		return 0;
	if (pack.image[large][rank] && ae_draw_image_live(pack.image[large][rank]))
	{
		if (width)
			*width = pack.image_width[large];
		if (height)
			*height = pack.image_height[large];
		return pack.image[large][rank];
	}
	rgba = malloc((size_t)AE_MCC_RANK_PICTURE_MOST * AE_MCC_RANK_PICTURE_MOST * 4);
	if (!rgba)
		return 0;
	if (!ae_mcc_rank_rgba(rank, large, rgba, (size_t)AE_MCC_RANK_PICTURE_MOST * AE_MCC_RANK_PICTURE_MOST * 4, &w, &h))
	{
		free(rgba);
		pack.image_failed[large][rank] = 1;
		return 0;
	}
	id = ae_draw_image_load(rgba, w, h);
	free(rgba);
	if (!id)
		pack.image_failed[large][rank] = 1;
	pack.image[large][rank] = id;
	pack.image_width[large] = w;
	pack.image_height[large] = h;
	if (width)
		*width = w;
	if (height)
		*height = h;
	return id;
}

void ae_mcc_rank_text(int rank, char *out, size_t size)
{
	if (!out || size < 1)
		return;
	snprintf(out, size, "%d", rank < AE_MCC_RANK_FIRST ? AE_MCC_RANK_FIRST : rank > AE_MCC_RANK_LAST ? AE_MCC_RANK_LAST : rank);
}
