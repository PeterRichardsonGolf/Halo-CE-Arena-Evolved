/* The career insignia of the player's MCC install (see the header). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/ae_mcc.h"
#include "../src/ae_draw.h"
#include "ae_mcc_career.h"
#include "ae_mcc_career_rules.h"
#include "ae_pak_rules.h"

void platform_log(char const *format, ...);
FILE *ae_mcc_install_open(char const *path);

#ifndef _WIN32
/* (the build is strict ANSI: the two 64 bit file calls are declared here; the 64 bit targets' off_t holds the pak's 11 GB) */
#include <sys/types.h>
int fseeko(FILE *file, off_t offset, int whence);
off_t ftello(FILE *file);
#endif
#ifdef _WIN32
#define SEEK64(file, offset) _fseeki64(file, (__int64)(offset), SEEK_SET)
#define SEEK_END64(file) _fseeki64(file, 0, SEEK_END)
#define TELL64(file) ((long long)_ftelli64(file))
#else
#define SEEK64(file, offset) fseeko(file, (off_t)(offset), SEEK_SET)
#define SEEK_END64(file) fseeko(file, 0, SEEK_END)
#define TELL64(file) ((long long)ftello(file))
#endif

#define PAK_PATH "MCC/Content/Paks/MCC-WindowsNoEditor.pak"
#define DATA_ASSET "MCC/Content/UI/Features/Nameplate/DA_AccountProgression"
#define MENU_SIDE 128

static struct
{
	int tried, usable;
	char pak[AE_MCC_PATH_SIZE + 64];
	struct ae_pak_entry rank[AE_MCC_CAREER_RANKS], tour[AE_MCC_CAREER_TOURS];
	int image[2][AE_MCC_CAREER_RANKS + 1];
	int image_failed[2][AE_MCC_CAREER_RANKS + 1];
} career;

/* a block of the pak's file: bytes at offset into buffer; 1 when read whole */
static int block_read(FILE *file, unsigned long long offset, void *buffer, size_t size)
{
	return SEEK64(file, offset) == 0 && fread(buffer, 1, size, file) == size;
}

/* an entry's data read whole (at most most bytes) into a made buffer, the record at its start checked; NULL for none */
static unsigned char *entry_read(FILE *file, struct ae_pak_entry const *entry, size_t most)
{
	unsigned char record[AE_PAK_RECORD], *data;

	if (entry->size == 0 || entry->size > most || !block_read(file, entry->data_offset - AE_PAK_RECORD, record, sizeof(record)) ||
		!ae_pak_record_ok(record, sizeof(record), entry->size))
		return NULL;
	if (!(data = malloc((size_t)entry->size)))
		return NULL;
	if (!block_read(file, entry->data_offset, data, (size_t)entry->size))
	{
		free(data);
		return NULL;
	}
	return data;
}

static void files_read(void)
{
	char const *root, *why = "";
	FILE *file = NULL;
	unsigned char footer[AE_PAK_FOOTER], *index = NULL, *uasset = NULL, *uexp = NULL;
	unsigned long long file_size = 0, index_offset, index_size;
	static char ranks[AE_MCC_CAREER_RANKS][AE_MCC_CAREER_PATH], tours[AE_MCC_CAREER_TOURS][AE_MCC_CAREER_PATH];
	struct ae_pak_entry asset_a, asset_x;
	int each;

	if (career.tried)
		return;
	career.tried = 1;
	/* (game.mcc_use no: the player has asked for none of MCC's files to be read) */
	if (ae_mcc_use() == AE_MCC_USE_NO || !(root = ae_mcc_root()))
	{
		platform_log("mcc career: no MCC folder to take the insignia from: ranks are shown by name");
		return;
	}
	snprintf(career.pak, sizeof(career.pak), "%s/%s", root, PAK_PATH);
	if (!(file = ae_mcc_install_open(career.pak)))
	{
		why = "no pak";
		goto failed;
	}
	if (SEEK_END64(file) != 0 || (file_size = (unsigned long long)TELL64(file)) < AE_PAK_FOOTER ||
		!block_read(file, file_size - AE_PAK_FOOTER, footer, sizeof(footer)) ||
		!ae_pak_footer(footer, sizeof(footer), file_size, &index_offset, &index_size))
	{
		why = "not a version 7 pak";
		goto failed;
	}
	if (!(index = malloc((size_t)index_size)) || !block_read(file, index_offset, index, (size_t)index_size))
	{
		why = "index not read";
		goto failed;
	}
	if (!ae_pak_index_find(index, (size_t)index_size, file_size, DATA_ASSET ".uasset", &asset_a) ||
		!ae_pak_index_find(index, (size_t)index_size, file_size, DATA_ASSET ".uexp", &asset_x) ||
		!(uasset = entry_read(file, &asset_a, AE_MCC_CAREER_MOST_ASSET)) ||
		!(uexp = entry_read(file, &asset_x, AE_MCC_CAREER_MOST_ASSET)))
	{
		why = "data asset not found";
		goto failed;
	}
	if (!ae_mcc_career_order(uasset, (size_t)asset_a.size, uexp, (size_t)asset_x.size, ranks, tours))
	{
		why = "order not found in the data asset";
		goto failed;
	}
	for (each = 0; each < AE_MCC_CAREER_RANKS; each++)
		if (!ae_pak_index_find(index, (size_t)index_size, file_size, ranks[each], &career.rank[each]))
		{
			why = "a rank picture is missing";
			goto failed;
		}
	for (each = 0; each < AE_MCC_CAREER_TOURS; each++)
		if (!ae_pak_index_find(index, (size_t)index_size, file_size, tours[each], &career.tour[each]))
		{
			why = "a tour picture is missing";
			goto failed;
		}
	career.usable = 1;
	platform_log("mcc career: insignia of %d ranks and %d tours in %s; order read from the data asset", AE_MCC_CAREER_RANKS,
		AE_MCC_CAREER_TOURS, career.pak);
	goto done;
failed:
	platform_log("mcc career: the insignia could not be read from %s (%s): ranks are shown by name", root, why);
done:
	if (file)
		fclose(file);
	if (index)
		free(index);
	if (uasset)
		free(uasset);
	if (uexp)
		free(uexp);
}

int ae_mcc_career_available(void)
{
	files_read();
	return career.usable;
}

int ae_mcc_career_rgba(int kind, int number, unsigned int side, unsigned char *rgba, size_t size)
{
	struct ae_pak_entry const *entry;
	unsigned char *data;
	unsigned int picture_side;
	size_t pixels;
	FILE *file;
	int done = 0;

	if (!ae_mcc_career_available() || !rgba || size < (size_t)side * side * 4)
		return 0;
	if (kind == 0 && number >= 1 && number <= AE_MCC_CAREER_RANKS)
		entry = &career.rank[number - 1];
	else if (kind == 1 && number >= 1 && number <= AE_MCC_CAREER_TOURS)
		entry = &career.tour[number - 1];
	else
		return 0;
	if (!(file = ae_mcc_install_open(career.pak)))
		return 0;
	data = entry_read(file, entry, 2 * 1024 * 1024);
	fclose(file);
	if (!data)
		return 0;
	if (ae_mcc_career_texture(data, (size_t)entry->size, &picture_side, &pixels))
		done = ae_mcc_career_scale(data + pixels, picture_side, rgba, side);
	free(data);
	return done;
}

int ae_mcc_career_menu_image(int kind, int number)
{
	unsigned char *rgba;
	int id;

	kind = kind ? 1 : 0;
	if (number < 1 || number > AE_MCC_CAREER_RANKS || career.image_failed[kind][number] || !ae_draw_available() ||
		!ae_mcc_career_available())
		return 0;
	if (career.image[kind][number] && ae_draw_image_live(career.image[kind][number]))
		return career.image[kind][number];
	if (!(rgba = malloc((size_t)MENU_SIDE * MENU_SIDE * 4)))
		return 0;
	if (!ae_mcc_career_rgba(kind, number, MENU_SIDE, rgba, (size_t)MENU_SIDE * MENU_SIDE * 4))
	{
		free(rgba);
		career.image_failed[kind][number] = 1;
		return 0;
	}
	id = ae_draw_image_load(rgba, MENU_SIDE, MENU_SIDE);
	free(rgba);
	if (!id)
		career.image_failed[kind][number] = 1;
	career.image[kind][number] = id;
	return id;
}
