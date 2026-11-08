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
	unsigned long looked_at;
	boolean bitmap_failed;
	struct bitmap_data bitmap;
} pictures[MAXIMUM_PICTURE_MAPS];
static short picture_count;

/* the last PNG read for the art (ae_map_picture_png) */
static unsigned char *png_data;

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
	if (map_file_path(map_name, file, sizeof(file), map_path, sizeof(map_path)) &&
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
	if (png_data)
		free(png_data);
	png_data = NULL;
	*size = 0;
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
