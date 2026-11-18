/*
AE_MAP_PICTURES.C

A map's own picture beside its file, on the map select screen (ae_map_pictures.h).
*/

#include "cseries.h"
#include "cseries/cseries_windows.h"
#include "bitmaps/bitmap_group.h"
#include "cache/cache_files.h"
#include "rasterizer/xbox/rasterizer_xbox_hardware_bitmaps.h"

#include "halo_custom_maps.h"
#include "halo_map_families.h"
#include "halo_menus.h"

#include "ae_map_picture_rules.h"
#include "ae_map_pictures.h"
#include "ae_map_folders.h"
#include "ae_glue_setup_maps.h"
#include "../src/ae_mcc_workshop.h"

#include <xtl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void platform_log(char const *format, ...);

enum
{
	/* bitmap frames from here are these pictures (ui_map_list.c's Halo PC pictures are at 0x4000 and up, at most
	24 of them) */
	AE_MAP_PICTURE_BASE = 0x5000,
	/* the maps asked about in a run (ui_map_list.c lists at most 2048: frames up to 0x5800) */
	MAXIMUM_PICTURE_MAPS = 2048,
	MAP_NAME_SIZE = 64,
	PICTURE_PATH_SIZE = 300,
	/* the D3D texture a picture's PNG is drawn for (menu_tags.c's), and the size the game draws it in: the map
	select screen's and the lobby's picture box (port/assets/menus/ce, mp_map_right_pic and lobby_map_pic), which
	the whole PNG fills */
	PLACEHOLDER_SIZE = 4,
	PICTURE_WIDTH = 140,
	PICTURE_HEIGHT = 114,
	/* (the bitmap's type, format and flag, as the game's: ae_menus_swap.c's) */
	_bitmap_type_2d = 0,
	_bitmap_format_a8r8g8b8 = 11,
	_bitmap_has_power_of_two_dimensions_bit = 0
};

/* an art name of a picture: this, then its index here */
#define ART_PREFIX "ae-map-picture/"

static struct
{
	char map_name[MAP_NAME_SIZE];
	char path[PICTURE_PATH_SIZE];
	/* whether the path is a picture the menus draw (looked for again every few seconds while it is not: a picture
	put in the previews folder while the game runs shows) */
	boolean found;
	/* (a file there the menus cannot draw: said once, not looked for again) */
	boolean refused;
	/* (a Workshop map's: the path is a host path in its Steam item, read with the platform's stdio, never the game's
	translated paths; the PNG may be of any colour type, made the menus' kind when drawn) */
	boolean host;
	unsigned long looked_at;
	boolean bitmap_failed;
	struct bitmap_data bitmap;
} pictures[MAXIMUM_PICTURE_MAPS];
static short picture_count;

/* the last PNG read for the art (ae_map_picture_png) */
static unsigned char *png_data;
/* whether png_data is one of cache's */
static boolean png_cached;

/* (0.7.9) the Workshop pictures made the menus' kind (decoded, cropped, scaled): the last PICTURE_CACHE of them */
enum { PICTURE_CACHE = 8 };
static struct
{
	short index;
	unsigned char *bytes;
	unsigned int size;
} cache[PICTURE_CACHE];
static short cache_next;

/* a made picture kept (the oldest dropped when all are in use); png_data becomes the cache's */
static void remember_picture(short index, unsigned char *bytes, unsigned int size)
{
	if (cache[cache_next].bytes)
		free(cache[cache_next].bytes);
	cache[cache_next].index = index;
	cache[cache_next].bytes = bytes;
	cache[cache_next].size = size;
	cache_next = (short)((cache_next + 1) % PICTURE_CACHE);
	png_cached = TRUE;
}

/* the file of a map of the list (as the game opens it), or FALSE */
static boolean map_file_path(char const *map_name, char *file, unsigned int file_size, char *path, unsigned int size)
{
	short family = map_family_parse(map_name, file, (long)file_size);

	if (!file[0])
		return FALSE;
	if (family == _map_family_xbox)
	{
		cache_files_map_file_path(file, path, (long)size);
		return path[0] != 0;
	}
	return map_family_find(family, file, path, (long)size);
}

/* (0.7.9) a Workshop map's picture file (a host path), its bytes malloc'd (the game's) into data; 0 for none or a file
that cannot be a picture (any colour type is taken: ae_setup_map_picture_rgba_png makes it the menus' kind) */
static boolean host_picture_bytes(char const *path, unsigned char **data, unsigned long *size)
{
	FILE *file = ae_mcc_workshop_open(path);
	unsigned char header[AE_MAP_PICTURE_HEADER_SIZE];
	long long length;

	*data = NULL;
	*size = 0;
	if (!file)
		return FALSE;
	length = ae_mcc_workshop_file_size(file);
	fseek(file, 0, SEEK_SET);
	if (length < (long long)sizeof(header) || length > AE_MAP_PICTURE_MAXIMUM_FILE ||
		fread(header, 1, sizeof(header), file) != sizeof(header) ||
		!ae_map_picture_png_decodable(header, sizeof(header), (unsigned int)length) || !(*data = malloc((size_t)length)))
	{
		fclose(file);
		platform_log("map pictures: %s is not a PNG the menus can decode (not shown)", path);
		return FALSE;
	}
	memcpy(*data, header, sizeof(header));
	if (fread(*data + sizeof(header), 1, (size_t)length - sizeof(header), file) != (size_t)length - sizeof(header))
	{
		fclose(file);
		free(*data);
		*data = NULL;
		return FALSE;
	}
	fclose(file);
	*size = (unsigned long)length;
	return TRUE;
}

/* a Workshop map's ("<file>@ws<item>") picture from its item (the catalog the start's scan made): its Large, else its
Thumbnail. TRUE with the host path in path */
static boolean workshop_picture_path(char const *map_name, char *path, unsigned int size)
{
	char file[MAP_NAME_SIZE], item[24], large[PICTURE_PATH_SIZE], thumbnail[PICTURE_PATH_SIZE];
	char const *chosen;
	unsigned char *data;
	unsigned long length;

	if (!ae_map_picture_workshop_identity(map_name, file, sizeof(file), item, sizeof(item)) ||
		!ae_mcc_workshop_map_pictures(item, file, large, thumbnail, sizeof(large)) ||
		!(chosen = ae_map_picture_workshop_choice(large, thumbnail)) || strlen(chosen) >= size)
		return FALSE;
	if (!host_picture_bytes(chosen, &data, &length) && chosen == large && thumbnail[0] &&
		host_picture_bytes(thumbnail, &data, &length))
		chosen = thumbnail;
	else if (!data)
		return FALSE;
	free(data);
	strcpy(path, chosen);
	return TRUE;
}

/* whether a file is a picture the menus draw; *exists: whether there is a file at all */
static boolean picture_usable(char const *path, boolean *exists)
{
	HANDLE file = CreateFileA(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
	unsigned char header[AE_MAP_PICTURE_HEADER_SIZE];
	unsigned long file_size, file_size_high = 0, read = 0;
	boolean usable;

	*exists = file != INVALID_HANDLE_VALUE;
	if (file == INVALID_HANDLE_VALUE)
		return FALSE;
	file_size = GetFileSize(file, &file_size_high);
	usable = file_size != INVALID_FILE_SIZE && !file_size_high &&
		ReadFile(file, header, sizeof(header), &read, NULL) && read == sizeof(header) &&
		ae_map_picture_png_usable(header, (unsigned int)read, (unsigned int)file_size);
	CloseHandle(file);
	if (!usable)
		platform_log("map pictures: %s is not an 8-bit RGBA PNG of at most 2048 by 2048 (not shown)", path);
	return usable;
}

#define LOOK_AGAIN_MILLISECONDS 5000

/* a map's entry (made the first time it is asked about), or NONE when the list is full */
static short picture_entry(char const *map_name)
{
	char file[MAP_NAME_SIZE], map_path[PICTURE_PATH_SIZE];
	short index;

	for (index = 0; index < picture_count; index++)
	{
		if (strcmp(pictures[index].map_name, map_name))
			continue;
		if (pictures[index].found || pictures[index].refused ||
			system_milliseconds() - pictures[index].looked_at < LOOK_AGAIN_MILLISECONDS)
			return index;
		break;
	}
	if (index == picture_count)
	{
		if (picture_count == MAXIMUM_PICTURE_MAPS || strlen(map_name) >= MAP_NAME_SIZE)
			return NONE;
		index = picture_count++;
		memset(&pictures[index], 0, sizeof(pictures[index]));
		strcpy(pictures[index].map_name, map_name);
	}
	pictures[index].looked_at = system_milliseconds();
	pictures[index].path[0] = 0;
	file[0] = 0;
	pictures[index].host = FALSE;
	if (strstr(map_name, "@ws"))
	{
		/* (a Workshop map: its item's own picture, not one beside its file) */
		if (workshop_picture_path(map_name, pictures[index].path, sizeof(pictures[index].path)))
		{
			pictures[index].found = pictures[index].host = TRUE;
			platform_log("map pictures: %s shows %s", map_name, pictures[index].path);
		}
	}
	else if (map_file_path(map_name, file, sizeof(file), map_path, sizeof(map_path)) &&
		ae_map_picture_path(map_path, file, pictures[index].path, sizeof(pictures[index].path)))
	{
		boolean exists = FALSE;
		char root_path[PICTURE_PATH_SIZE];

		pictures[index].found = picture_usable(pictures[index].path, &exists);
		/* (a map in a folder of the maps folder with no picture in its folder's previews: the maps folder's,
		ae_map_folders.c) */
		if (!exists && ae_map_folders_root_path(map_path, root_path, sizeof(root_path)) &&
			ae_map_picture_path(root_path, file, pictures[index].path, sizeof(pictures[index].path)))
		{
			pictures[index].found = picture_usable(pictures[index].path, &exists);
		}
		pictures[index].refused = exists && !pictures[index].found;
		if (pictures[index].found)
			platform_log("map pictures: %s shows %s", map_name, pictures[index].path);
	}
	return index;
}

/* the art name of a picture */
static void art_name(short index, char *name, unsigned int size)
{
	snprintf(name, size, ART_PREFIX "%d", (int)index);
}

/* a picture's bitmap (made the first time it is drawn), or NULL */
static struct bitmap_data *picture_bitmap(short index)
{
	struct bitmap_data *bitmap = &pictures[index].bitmap;
	char name[32];

	if (pictures[index].bitmap_failed)
		return NULL;
	if (!bitmap->hardware_format)
	{
		/* (a small D3D texture, which the PNG is drawn for: menu_tags.c's bitmap_build) */
		memset(bitmap, 0, sizeof(*bitmap));
		bitmap->signature = BITMAP_GROUP_TAG;
		bitmap->width = PLACEHOLDER_SIZE;
		bitmap->height = PLACEHOLDER_SIZE;
		bitmap->depth = 1;
		bitmap->type = _bitmap_type_2d;
		bitmap->format = _bitmap_format_a8r8g8b8;
		bitmap->flags = FLAG(_bitmap_has_power_of_two_dimensions_bit);
		bitmap->tag_index = NONE;
		bitmap->cache_block_index = NONE;
		/* (none without a renderer: debug.null_renderer) */
		if (!rasterizer_bitmap_new(bitmap) || !bitmap->hardware_format)
		{
			pictures[index].bitmap_failed = TRUE;
			return NULL;
		}
		bitmap->width = PICTURE_WIDTH;
		bitmap->height = PICTURE_HEIGHT;
	}
	/* (again each time: the menus' art is forgotten when their tags are made again, menu_tags.c; the same name
	for the same texture costs a look through the list) */
	art_name(index, name, sizeof(name));
	halo_menus_art_register(xbox_pointer(bitmap->hardware_format), name);
	return bitmap;
}

/* ---------- public code */

short ae_map_picture_frame(char const *map_name)
{
	short index;

	/* (both menus' (owner 2026-10-16): the PC menus' map select and lobby, and the Xbox menus' through the map
	list's frame of a row, ui_map_list_picture_index) */
	/* (the stock maps: the loaded ui.map's own pictures) */
	if (!map_name || !map_name[0] || (!strchr(map_name, '@') && !native_map_is_custom(map_name)))
		return NONE;
	index = picture_entry(map_name);
	if (index == NONE || !pictures[index].found || !picture_bitmap(index))
		return NONE;
	return (short)(AE_MAP_PICTURE_BASE + index);
}

struct bitmap_data *ae_map_picture_bitmap(short frame_index)
{
	short index = (short)(frame_index - AE_MAP_PICTURE_BASE);

	if (frame_index < AE_MAP_PICTURE_BASE || index >= picture_count || !pictures[index].found)
		return NULL;
	return picture_bitmap(index);
}

unsigned char const *ae_map_picture_png(char const *art, unsigned int *size)
{
	size_t prefix = strlen(ART_PREFIX);
	HANDLE file;
	unsigned long file_size, file_size_high = 0, read = 0;
	char *end;
	long index;

	if (!art || strncmp(art, ART_PREFIX, prefix) || !art[prefix])
		return NULL;
	index = strtol(art + prefix, &end, 10);
	if (*end || index < 0 || index >= picture_count || !pictures[index].found)
		return NULL;
	/* (the game's free takes no NULL: debug_memory.c asserts) */
	/* (a picture kept in the cache is freed there, not here) */
	if (png_data && !png_cached)
		free(png_data);
	png_data = NULL;
	png_cached = FALSE;
	*size = 0;
	if (pictures[index].host)
	{
		unsigned char *original;
		short slot;
		unsigned long original_size, out_size = 0;

		/* (the pictures made before: a PNG decoded and scaled once, not at each draw; the last few kept in memory, the
		oldest dropped (simpler than a cache folder: nothing to age, nothing on disk) */
		for (slot = 0; slot < PICTURE_CACHE; slot++)
		{
			if (cache[slot].bytes && cache[slot].index == index)
			{
				png_data = cache[slot].bytes;
				*size = cache[slot].size;
				png_cached = TRUE;
				return png_data;
			}
		}
		if (!host_picture_bytes(pictures[index].path, &original, &original_size))
			return NULL;
		/* (a picture that is the menus' kind already is drawn as it is) */
		if (ae_map_picture_png_usable(original, (unsigned int)original_size, (unsigned int)original_size))
		{
			png_data = original;
			*size = (unsigned int)original_size;
			return png_data;
		}
		/* (cropped to the box's proportions: the menus' box fills with the whole PNG) */
		ae_setup_map_picture_rgba_png(original, original_size, PICTURE_WIDTH, PICTURE_HEIGHT, &png_data, &out_size);
		free(original);
		if (png_data)
		{
			*size = (unsigned int)out_size;
			remember_picture(index, png_data, *size);
		}
		return png_data;
	}
	file = CreateFileA(pictures[index].path, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (file == INVALID_HANDLE_VALUE)
		return NULL;
	file_size = GetFileSize(file, &file_size_high);
	if (file_size != INVALID_FILE_SIZE && !file_size_high && file_size <= AE_MAP_PICTURE_MAXIMUM_FILE)
		png_data = malloc(file_size ? file_size : 1);
	if (png_data && (!ReadFile(file, png_data, file_size, &read, NULL) || read != file_size))
	{
		free(png_data);
		png_data = NULL;
	}
	CloseHandle(file);
	if (png_data)
		*size = (unsigned int)file_size;
	return png_data;
}

int ae_map_picture_missing(char const *map_name, char *path, unsigned int size)
{
	char file[MAP_NAME_SIZE], map_path[PICTURE_PATH_SIZE];
	boolean exists = FALSE;
	short index;

	if (!path || !size)
		return FALSE;
	path[0] = 0;
	/* (as ae_map_picture_frame's: the stock maps keep the loaded ui.map's own pictures) */
	if (!map_name || !map_name[0] || (!strchr(map_name, '@') && !native_map_is_custom(map_name)))
		return FALSE;
	index = picture_entry(map_name);
	if (index == NONE || pictures[index].found || pictures[index].refused)
		return FALSE;
	file[0] = 0;
	if (!map_file_path(map_name, file, sizeof(file), map_path, sizeof(map_path)) ||
		!ae_map_picture_path(map_path, file, path, size))
	{
		path[0] = 0;
		return FALSE;
	}
	picture_usable(path, &exists);
	if (exists)
		path[0] = 0;
	return path[0] != 0;
}
