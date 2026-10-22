/*
AE_GLUE_SETUP_MAPS.C

The game setup spike's CHANGE MAP glue. See ae_glue_setup_maps.h.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cseries.h"
#include "cseries/cseries_windows.h"
#include "cache/cache_files.h"
#include "saved games/saved_game_files.h"
#include "halo_custom_maps.h"
#include "halo_map_families.h"
#include "halo_ui_map_list.h"
#include "../src/ae_discs.h"
#include "../src/ae_draw.h"
#include "ae_catalog.h"
#include "ae_glue_setup_maps.h"
#include "ae_map_folders.h"
#include "ae_map_folders_rules.h"
#include "ae_map_picture_rules.h"
#include "ae_map_warm_up.h"
#include "ae_mapinfo.h"
#include <xtl.h>

/* (PNG alone: a map's picture; static, so it never meets posix_ui_font.c's JPEG one) */
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include "../../third_party/stb/stb_image.h"
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

void platform_log(char const *format, ...);
/* ui_widget_event_handler_functions.c (declared as menu_functions.c does) */
short ui_widget_port_multiplayer_maps(char const *const **names, short *last_used);
void ui_widget_port_multiplayer_maps_refresh(void);
boolean ui_widget_port_multiplayer_map_choose(short level_index);
/* ae_maps_data.c (tools/ae_maps_data.py) */
extern const unsigned int ae_maps_data_words[];
extern const int ae_maps_data_size;

enum { PATH_SIZE = 512, PICTURES = 32, SMALL_EXTENT = 35, LARGE_EXTENT = 130 };
enum { GROUP_HALO_CE = 1, GROUP_WARM_UP, GROUP_COMP, GROUP_BTB, FIRST_FOLDER_GROUP };

static struct
{
	/* (about 300 KB: static) */
	struct ae_catalog catalog;
	int catalog_read;
	char text[8192];
	/* each map's source group (its badge), and the groups past the folders' */
	signed char source[AE_SETUP_MAPS_MAXIMUM];
	int disc_group, mod_group, ce_group, halomd_group;
	short facts_read, facts_unreadable;
	int facts_logged;
	/* the pictures loaded: the map's file, its image, its size (an image 0: none) */
	short picture_count;
	char picture_files[PICTURES][AE_SETUP_MAP_TEXT];
	int picture_images[PICTURES], picture_widths[PICTURES], picture_heights[PICTURES];
	short last_used;
} glue = { .last_used = -1 };

/* ---------- maps.txt */

static void catalog_read(void)
{
	int index;

	if (glue.catalog_read)
		return;
	glue.catalog_read = 1;
	if (ae_maps_data_size <= 0 || ae_maps_data_size >= (int)sizeof(glue.text))
		return;
	/* (the words' bytes little-endian, as tools/ae_maps_data.py packs them) */
	for (index = 0; index < ae_maps_data_size; index++)
		glue.text[index] = (char)(ae_maps_data_words[index / 4] >> (8 * (index % 4)));
	glue.text[ae_maps_data_size] = 0;
	ae_catalog_read_facts(&glue.catalog, glue.text);
}

static struct ae_map_facts const *facts_of(char const *file)
{
	short index;

	for (index = 0; index < glue.catalog.facts_count; index++)
	{
		if (!_stricmp(glue.catalog.facts[index].file, file))
			return &glue.catalog.facts[index];
	}
	return NULL;
}

static int has_tag(char const *tags, char const *tag)
{
	size_t length = strlen(tag);
	char const *at = tags;

	while ((at = strstr(at, tag)) != NULL)
	{
		if ((at == tags || at[-1] == ',') && (at[length] == 0 || at[length] == ','))
			return 1;
		at += length;
	}
	return 0;
}

/* ---------- loading */

static int group_add(struct ae_setup_maps *maps, char const *name)
{
	int group;

	for (group = 0; group < maps->group_count; group++)
	{
		if (!strcmp(maps->group_names[group], name))
			return group;
	}
	if (maps->group_count >= AE_SETUP_MAP_GROUPS)
		return -1;
	snprintf(maps->group_names[maps->group_count], AE_SETUP_MAP_GROUP_NAME, "%s", name);
	return maps->group_count++;
}

static void narrow(wchar_t const *wide, char *text, int size)
{
	int index;

	for (index = 0; index < size - 1 && wide[index]; index++)
		text[index] = wide[index] >= 0x20 && wide[index] < 0x7F ? (char)wide[index] : '?';
	text[index] = 0;
}

static int file_exists(char const *path)
{
	HANDLE file = CreateFileA(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);

	if (file == INVALID_HANDLE_VALUE)
		return 0;
	CloseHandle(file);
	return 1;
}

static int compare_names(void const *a, void const *b)
{
	return ae_map_folders_compare((char const *)a, (char const *)b);
}

void ae_setup_maps_load(struct ae_setup_maps *maps)
{
	static char pages[AE_SETUP_MAPS_MAXIMUM][AE_MAP_PAGE_NAME_SIZE];
	static char folders[AE_SETUP_MAP_GROUPS][AE_MAP_PAGE_NAME_SIZE];
	char const *const *names = NULL;
	char const *disc = cache_files_mod_started();
	char disc_name[AE_SETUP_MAP_GROUP_NAME];
	short count, index, last_used = 0;
	int folder_count = 0, folder, disc_is_nhe = 0;

	catalog_read();
	memset(maps, 0, sizeof(*maps));
	memset(glue.source, -1, sizeof(glue.source));
	glue.facts_read = glue.facts_unreadable = 0;
	glue.facts_logged = 0;
	ui_widget_port_multiplayer_maps_refresh();
	count = ui_widget_port_multiplayer_maps(&names, &last_used);
	glue.last_used = last_used;
	if (count > AE_SETUP_MAPS_MAXIMUM)
		count = AE_SETUP_MAPS_MAXIMUM;
	/* the folders' pages, A-Z */
	for (index = 0; index < count; index++)
	{
		pages[index][0] = 0;
		if (index >= HALO_STOCK_MULTIPLAYER_MAP_COUNT)
			ae_map_folders_row_page(names[index], pages[index], sizeof(pages[index]));
		for (folder = 0; pages[index][0] && folder < folder_count; folder++)
		{
			if (!strcmp(folders[folder], pages[index]))
				break;
		}
		if (pages[index][0] && folder == folder_count && folder_count < AE_SETUP_MAP_GROUPS - FIRST_FOLDER_GROUP - 4)
			snprintf(folders[folder_count++], AE_MAP_PAGE_NAME_SIZE, "%s", pages[index]);
	}
	qsort(folders, (size_t)folder_count, sizeof(folders[0]), compare_names);
	group_add(maps, "FAVOURITES");
	group_add(maps, "HALO CE");
	group_add(maps, "WARM UP");
	group_add(maps, "COMP");
	group_add(maps, "BTB");
	for (folder = 0; folder < folder_count; folder++)
		group_add(maps, folders[folder]);
	glue.disc_group = -1;
	if (disc && disc[0])
	{
		int at;

		for (at = 0; at < (int)sizeof(disc_name) - 1 && disc[at]; at++)
			disc_name[at] = disc[at] >= 'a' && disc[at] <= 'z' ? (char)(disc[at] - 32) : disc[at];
		disc_name[at] = 0;
		disc_is_nhe = strstr(disc_name, "NHE") != NULL;
		glue.disc_group = group_add(maps, disc_name);
	}
	glue.mod_group = group_add(maps, "MOD MAPS");
	glue.ce_group = group_add(maps, "CUSTOM EDITION");
	glue.halomd_group = group_add(maps, "HALOMD");
	for (index = 0; index < count; index++)
	{
		struct ae_setup_map *map = &maps->maps[maps->count];
		struct ae_map_facts const *facts;
		char file[AE_SETUP_MAP_TEXT], path[PATH_SIZE];
		wchar_t caption[AE_SETUP_MAP_TEXT];
		short family = map_family_parse(names[index], file, sizeof(file));
		int source = -1;

		snprintf(map->file, sizeof(map->file), "%s", names[index]);
		map->list_index = index;
		/* (maps.txt's key: a CE or MD map's list name, <file>@ce; a stock map's file, not its scenario path) */
		facts = facts_of(names[index]);
		if (!facts)
			facts = facts_of(file);
		if (facts && facts->name[0])
			snprintf(map->name, sizeof(map->name), "%s", facts->name);
		else
		{
			ui_map_list_caption(names[index], caption, AE_SETUP_MAP_TEXT);
			narrow(caption, map->name, sizeof(map->name));
		}
		if (facts)
		{
			snprintf(map->credits, sizeof(map->credits), "%s", facts->credits);
			map->players_min = facts->players_min;
			map->players_max = facts->players_max;
			map->size = facts->size;
			map->nhe = has_tag(facts->tags, "nhe-vehicles");
			if (has_tag(facts->tags, "competitive"))
				map->groups |= 1u << GROUP_COMP;
		}
		if (index < HALO_STOCK_MULTIPLAYER_MAP_COUNT)
			source = GROUP_HALO_CE;
		else if (pages[index][0])
		{
			for (folder = 0; folder < folder_count; folder++)
			{
				if (!strcmp(folders[folder], pages[index]))
					source = FIRST_FOLDER_GROUP + folder;
			}
		}
		else if (family == _map_family_xbox && glue.disc_group >= 0 &&
			snprintf(path, sizeof(path), "d:\\%s\\%s\\maps\\%s.map", ae_disc_folder(disc), disc, file) < (int)sizeof(path) &&
			file_exists(path))
		{
			source = glue.disc_group;
			map->nhe |= disc_is_nhe;
		}
		else if (family == _map_family_custom_edition)
			source = glue.ce_group;
		else if (family == _map_family_halomd)
			source = glue.halomd_group;
		else
			source = glue.mod_group;
		if (ae_map_warm_up(names[index]))
			map->groups |= 1u << GROUP_WARM_UP;
		if (map->size == 'l')
			map->groups |= 1u << GROUP_BTB;
		if (source >= 0)
			map->groups |= 1u << source;
		glue.source[maps->count] = (signed char)source;
		maps->count++;
	}
	{
		char line[512];
		int group, length = 0;

		line[0] = 0;
		for (group = 0; group < maps->group_count && length < (int)sizeof(line) - 40; group++)
			length += snprintf(line + length, sizeof(line) - (size_t)length, "%s%s %d", group ? ", " : "",
				maps->group_names[group], ae_setup_map_group_count(maps, group, AE_MF_ALL));
		platform_log("ae change map: maps %d, groups %s", maps->count, line);
	}
}

/* ---------- the facts */

static int file_reader(void *context, unsigned long offset, void *buffer, unsigned long size)
{
	HANDLE file = *(HANDLE *)context;
	unsigned long bytes_read = 0;

	if (SetFilePointer(file, (long)offset, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER)
		return 0;
	return ReadFile(file, buffer, size, &bytes_read, NULL) && bytes_read == size;
}

/* a map's file as the game opens it; 0 for none */
static int map_path(char const *list_name, char *path, long size)
{
	char file[AE_SETUP_MAP_TEXT];
	short family = map_family_parse(list_name, file, sizeof(file));

	if (!file[0])
		return 0;
	if (family == _map_family_xbox)
	{
		cache_files_map_file_path(file, path, size);
		return path[0] != 0;
	}
	return map_family_find(family, file, path, size) ? 1 : 0;
}

static void facts_read(struct ae_setup_map *map)
{
	char path[PATH_SIZE];
	struct ae_mapinfo info;
	HANDLE file;
	unsigned long high = 0, size;

	map->facts = AE_SETUP_FACTS_UNREADABLE;
	if (!map_path(map->file, path, sizeof(path)))
		return;
	file = CreateFileA(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (file == INVALID_HANDLE_VALUE)
		return;
	size = GetFileSize(file, &high);
	memset(&info, 0, sizeof(info));
	if (!high && ae_mapinfo_parse(file_reader, &file, size, &info) == AE_MAPINFO_OK)
	{
		map->facts = AE_SETUP_FACTS_READ;
		map->modes = info.modes;
		/* (maps.txt's size first; else the play area's, as maps.txt's sizes were made) */
		if (!map->size && info.play_extent > 0.0f)
			map->size = info.play_extent < SMALL_EXTENT ? 's' : info.play_extent <= LARGE_EXTENT ? 'm' : 'l';
		if (map->size == 'l')
			map->groups |= 1u << GROUP_BTB;
	}
	CloseHandle(file);
}

short ae_setup_maps_read_facts(struct ae_setup_maps *maps, short count)
{
	short index, left = 0;

	for (index = 0; index < maps->count; index++)
	{
		struct ae_setup_map *map = &maps->maps[index];

		if (map->facts != AE_SETUP_FACTS_NONE)
			continue;
		if (count > 0)
		{
			facts_read(map);
			if (map->facts == AE_SETUP_FACTS_READ)
				glue.facts_read++;
			else
				glue.facts_unreadable++;
			count--;
			continue;
		}
		left++;
	}
	if (!left && !glue.facts_logged && maps->count)
	{
		glue.facts_logged = 1;
		platform_log("ae change map: facts %d read (%d unreadable)", glue.facts_read, glue.facts_unreadable);
	}
	return left;
}

/* ---------- the panel's words */

int ae_setup_map_source(short index)
{
	return index >= 0 && index < AE_SETUP_MAPS_MAXIMUM ? glue.source[index] : -1;
}

const char *ae_setup_map_badge(struct ae_setup_maps const *maps, short index)
{
	int source = index >= 0 && index < maps->count ? glue.source[index] : -1;

	if (source < 0)
		return "";
	if (source == GROUP_HALO_CE)
		return "HALO CE";
	if (source == glue.mod_group)
		return "MOD MAP";
	return maps->group_names[source];
}

const char *ae_setup_map_sharing(struct ae_setup_maps const *maps, short index)
{
	int source = index >= 0 && index < maps->count ? glue.source[index] : -1;

	if (source == GROUP_HALO_CE)
		return "Every player has it.";
	if (source >= 0 && source == glue.disc_group)
		return "From the disc you started with: players need the same disc to join.";
	return "From your maps folder. Players who don't have it get it from you when they join (map sharing), with a % "
		"in the lobby.";
}

/* ---------- the picture */

static int picture_load(char const *list_name, int *width, int *height)
{
	char path[PATH_SIZE], picture[PATH_SIZE], file[AE_SETUP_MAP_TEXT];
	unsigned char header[AE_MAP_PICTURE_HEADER_SIZE], *data, *rgba;
	unsigned long high = 0, size, bytes_read = 0;
	int w = 0, h = 0, channels = 0, image;
	HANDLE handle;

	map_family_parse(list_name, file, sizeof(file));
	if (!map_path(list_name, path, sizeof(path)) || !ae_map_picture_path(path, file, picture, sizeof(picture)))
		return 0;
	handle = CreateFileA(picture, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (handle == INVALID_HANDLE_VALUE)
	{
		platform_log("ae change map: picture '%s' none: no %s", list_name, picture);
		return 0;
	}
	size = GetFileSize(handle, &high);
	if (high || !ReadFile(handle, header, sizeof(header), &bytes_read, NULL) || bytes_read != sizeof(header) ||
		!ae_map_picture_png_usable(header, sizeof(header), (unsigned int)size) || !(data = malloc(size)))
	{
		CloseHandle(handle);
		platform_log("ae change map: picture '%s' none: not a usable PNG", list_name);
		return 0;
	}
	memcpy(data, header, sizeof(header));
	bytes_read = 0;
	if (!ReadFile(handle, data + sizeof(header), size - sizeof(header), &bytes_read, NULL) ||
		bytes_read != size - sizeof(header))
	{
		CloseHandle(handle);
		free(data);
		platform_log("ae change map: picture '%s' none: short read", list_name);
		return 0;
	}
	CloseHandle(handle);
	rgba = stbi_load_from_memory(data, (int)size, &w, &h, &channels, 4);
	free(data);
	if (!rgba)
	{
		platform_log("ae change map: picture '%s' none: not decoded", list_name);
		return 0;
	}
	image = ae_draw_image_load(rgba, w, h);
	stbi_image_free(rgba);
	*width = w;
	*height = h;
	platform_log("ae change map: picture '%s' %dx%d", list_name, w, h);
	return image;
}

int ae_setup_map_picture(struct ae_setup_maps const *maps, short index, int *width, int *height)
{
	char const *file;
	short slot;

	*width = *height = 0;
	if (index < 0 || index >= maps->count)
		return 0;
	file = maps->maps[index].file;
	for (slot = 0; slot < glue.picture_count; slot++)
	{
		if (!strcmp(glue.picture_files[slot], file))
		{
			*width = glue.picture_widths[slot];
			*height = glue.picture_heights[slot];
			return glue.picture_images[slot];
		}
	}
	/* (the oldest forgotten when full: ae_draw frees images least recently drawn itself) */
	slot = glue.picture_count < PICTURES ? glue.picture_count++ : (short)(index % PICTURES);
	snprintf(glue.picture_files[slot], AE_SETUP_MAP_TEXT, "%s", file);
	glue.picture_images[slot] = picture_load(file, &glue.picture_widths[slot], &glue.picture_heights[slot]);
	*width = glue.picture_widths[slot];
	*height = glue.picture_heights[slot];
	return glue.picture_images[slot];
}

/* ---------- favourites, the pick */

void ae_setup_map_favourite(struct ae_setup_maps *maps, short index)
{
	struct ae_setup_map *map;

	if (index < 0 || index >= maps->count)
		return;
	map = &maps->maps[index];
	map->favourite = !map->favourite;
	if (map->favourite)
		map->groups |= 1u << AE_SETUP_GROUP_FAVOURITES;
	else
		map->groups &= ~(1u << AE_SETUP_GROUP_FAVOURITES);
	platform_log("ae change map: favourite '%s' %s", map->file, map->favourite ? "on" : "off");
}

int ae_setup_map_pick(struct ae_setup_maps const *maps, short index)
{
	if (index < 0 || index >= maps->count)
		return 0;
	platform_log("ae change map: pick '%s'", maps->maps[index].file);
	return ui_widget_port_multiplayer_map_choose(maps->maps[index].list_index) ? 1 : 0;
}

short ae_setup_maps_last_used(void)
{
	return glue.last_used;
}
