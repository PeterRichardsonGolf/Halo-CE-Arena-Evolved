/*
AE_FORGE_MAPS.C

See ae_forge_maps.h.
*/

#include "cseries.h"
#include "cseries/cseries_windows.h"
#include "../src/ae_draw.h"
#include "ae_forge_maps.h"
#include <xtl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void platform_log(char const *format, ...);

enum
{
	HEADER_BYTES = 0x800,
	TEXT_BYTES = 2048,
	PICTURE_MAX = 140 * 114 * 4 + 1024,
	FORGE_SIGNATURE = 0x464C4731
};

static struct ae_forge_map maps[AE_FORGE_MAPS_MAX];
static int map_count, download_count;
static int view[AE_FORGE_MAPS_MAX], view_count;

/* the folders the saves are in, then the downloads' (a path is a game path: d:\...) */
static char const *const made_folders[] = { "d:\\custom_maps\\" };
static char const *const download_folders[] = { "d:\\maps_shared\\", "d:\\maps_ce\\" };

static unsigned long little32(unsigned char const *p)
{
	return (unsigned long)p[0] | (unsigned long)p[1] << 8 | (unsigned long)p[2] << 16 | (unsigned long)p[3] << 24;
}

/* up to `size` bytes of a file from `offset`; the bytes read (0 when it does not open) */
static long read_part(char const *path, long offset, void *buffer, long size)
{
	HANDLE file = CreateFileA(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
	DWORD got = 0;

	if (file == INVALID_HANDLE_VALUE)
		return 0;
	if ((offset && SetFilePointer(file, offset, NULL, FILE_BEGIN) == (DWORD)-1) || !ReadFile(file, buffer, (DWORD)size, &got, NULL))
		got = 0;
	CloseHandle(file);
	return (long)got;
}

static int file_exists(char const *path)
{
	HANDLE file = CreateFileA(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);

	if (file == INVALID_HANDLE_VALUE)
		return 0;
	CloseHandle(file);
	return 1;
}

static unsigned long file_seconds(FILETIME const *time)
{
	unsigned long long ticks = ((unsigned long long)time->dwHighDateTime << 32) | time->dwLowDateTime;
	unsigned long long seconds = ticks / 10000000ULL;

	return seconds > 11644473600ULL ? (unsigned long)(seconds - 11644473600ULL) : 0;
}

static void upper_copy(char *to, int size, char const *from)
{
	int at;

	for (at = 0; at < size - 1 && from[at]; at++)
		to[at] = from[at] >= 'a' && from[at] <= 'z' ? (char)(from[at] - 32) : from[at];
	to[at] = 0;
}

/* one file as a saved map into maps[]; 0 when it is none (no Forge trailer) */
static int map_read(char const *folder, WIN32_FIND_DATAA const *data, int downloaded)
{
	struct ae_forge_map *map = &maps[map_count];
	unsigned char header[HEADER_BYTES];
	char text[TEXT_BYTES + 1];
	struct ae_forge_maps_facts facts;
	unsigned long state_offset, state_size;
	long got, length;

	if (map_count >= AE_FORGE_MAPS_MAX || (data->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
		return 0;
	length = (long)strlen(data->cFileName);
	if (length < 5 || length - 4 >= (long)sizeof(map->file) || _stricmp(data->cFileName + length - 4, ".map") ||
		snprintf(map->path, sizeof(map->path), "%s%s", folder, data->cFileName) >= (int)sizeof(map->path))
		return 0;
	if (read_part(map->path, 0, header, HEADER_BYTES) != HEADER_BYTES || little32(header + 0x1FC) != FORGE_SIGNATURE)
		return 0;
	memset(map, 0, sizeof(*map));
	snprintf(map->path, sizeof(map->path), "%s%s", folder, data->cFileName);
	memcpy(map->file, data->cFileName, (size_t)(length - 4));
	map->file[length - 4] = 0;
	map->downloaded = downloaded;
	map->kind = ae_forge_maps_kind((long)little32(header + 4));
	map->size = (long)data->nFileSizeLow;
	map->saved = file_seconds(&data->ftLastWriteTime);
	state_offset = little32(header + 0x1E4);
	state_size = little32(header + 0x1E8);
	map->picture_offset = (long)little32(header + 0x1EC);
	map->picture_size = (long)little32(header + 0x1F0);
	if (map->picture_size > PICTURE_MAX || map->picture_offset < HEADER_BYTES)
		map->picture_size = 0;
	snprintf(map->name, sizeof(map->name), "%s", map->file);
	got = state_offset >= HEADER_BYTES && state_size ? read_part(map->path, (long)state_offset, text,
		state_size < TEXT_BYTES ? (long)state_size : TEXT_BYTES) : 0;
	text[got] = 0;
	if (got && ae_forge_maps_facts_read(text, &facts))
	{
		map->readable = map->kind != AE_FORGE_MAP_UNREADABLE;
		map->objects = facts.objects;
		map->squads = facts.squads;
		if (facts.name[0])
			snprintf(map->name, sizeof(map->name), "%s", facts.name);
		if (facts.base[0])
		{
			char base[64];
			char *dot;

			upper_copy(base, sizeof(base), facts.base);
			dot = strrchr(base, '.');
			if (dot && !strcmp(dot, ".MAP"))
				*dot = 0;
			snprintf(map->base, sizeof(map->base), "%s", base);
		}
	}
	ae_forge_maps_level_name(map->kind, map->file, map->level, sizeof(map->level));
	if (!map->readable)
		map->level[0] = 0;
	map_count++;
	return 1;
}

static void folder_scan(char const *folder, int downloaded)
{
	WIN32_FIND_DATAA data;
	char pattern[300];
	HANDLE find;

	snprintf(pattern, sizeof(pattern), "%s*.map", folder);
	find = FindFirstFileA(pattern, &data);
	if (find == INVALID_HANDLE_VALUE)
		return;
	do
	{
		if (map_read(folder, &data, downloaded) && downloaded)
			download_count++;
	}
	while (FindNextFileA(find, &data));
	CloseHandle(find);
}

int ae_forge_maps_scan(void)
{
	int each;

	for (each = 0; each < map_count; each++)
	{
		if (maps[each].picture)
			ae_draw_image_release(maps[each].picture);
	}
	map_count = download_count = view_count = 0;
	for (each = 0; each < (int)(sizeof(made_folders) / sizeof(made_folders[0])); each++)
		folder_scan(made_folders[each], 0);
	for (each = 0; each < (int)(sizeof(download_folders) / sizeof(download_folders[0])); each++)
		folder_scan(download_folders[each], 1);
	return map_count;
}

struct ae_forge_map *ae_forge_maps_get(int index)
{
	return index >= 0 && index < map_count ? &maps[index] : NULL;
}

int ae_forge_maps_total(void)
{
	return map_count;
}

int ae_forge_maps_downloaded(void)
{
	return download_count;
}

static int view_show, view_sort;

static int view_compare(void const *a, void const *b)
{
	struct ae_forge_map const *x = &maps[*(int const *)a], *y = &maps[*(int const *)b];

	return ae_forge_maps_compare(view_sort, x->name, x->base, x->saved, y->name, y->base, y->saved);
}

int ae_forge_maps_view(int show, int sort)
{
	int each;

	view_show = show;
	view_sort = sort;
	view_count = 0;
	for (each = 0; each < map_count; each++)
	{
		if (ae_forge_maps_shown(show, maps[each].downloaded))
			view[view_count++] = each;
	}
	qsort(view, (size_t)view_count, sizeof(view[0]), view_compare);
	return view_count;
}

struct ae_forge_map *ae_forge_maps_at(int place)
{
	return place >= 0 && place < view_count ? &maps[view[place]] : NULL;
}

int ae_forge_maps_picture(struct ae_forge_map *map)
{
	static unsigned char bytes[PICTURE_MAX], rgba[140 * 114 * 4];
	int width = 0, height = 0;

	if (!map || map->picture || map->picture_size <= 0)
		return map ? map->picture : 0;
	if (read_part(map->path, map->picture_offset, bytes, map->picture_size) == map->picture_size &&
		ae_forge_maps_bmp_rgba(bytes, map->picture_size, rgba, sizeof(rgba), &width, &height))
		map->picture = ae_draw_image_load(rgba, width, height);
	else
		map->picture_size = 0;
	return map->picture;
}

int ae_forge_maps_find(char const *file)
{
	int each;

	for (each = 0; each < map_count; each++)
	{
		if (!_stricmp(maps[each].file, file))
			return each;
	}
	return -1;
}

/* the files a name is saved in, in the order Forge's loader tries them */
static char const *const path_forms[] =
{
	"d:\\custom_maps\\%s.map",
	"d:\\forge_maps\\%s\\%s.map",
	"d:\\Faber_data\\forge_maps\\%s\\%s.map",
};

int ae_forge_maps_path(char const *name, char *path, int size)
{
	int each;

	if (!name || !ae_forge_maps_name_valid(name))
		return 0;
	for (each = 0; each < (int)(sizeof(path_forms) / sizeof(path_forms[0])); each++)
	{
		char candidate[300];

		if (snprintf(candidate, sizeof(candidate), path_forms[each], name, name) < (int)sizeof(candidate) && file_exists(candidate))
		{
			if ((int)strlen(candidate) >= size)
				return 0;
			memcpy(path, candidate, strlen(candidate) + 1);
			return 1;
		}
	}
	return 0;
}

unsigned long ae_forge_maps_saved_time(char const *name)
{
	WIN32_FIND_DATAA data;
	char path[300];
	HANDLE find;
	unsigned long seconds;

	if (!ae_forge_maps_path(name, path, (int)sizeof(path)))
		return 0;
	find = FindFirstFileA(path, &data);
	if (find == INVALID_HANDLE_VALUE)
		return 0;
	seconds = file_seconds(&data.ftLastWriteTime);
	CloseHandle(find);
	return seconds;
}

int ae_forge_maps_level_path(char const *level, char *path, int size)
{
	char file[72];
	char const *last = level ? strrchr(level, '\\') : NULL;
	unsigned char header[HEADER_BYTES];

	if (!level || strchr(level, '@'))
		return 0;
	snprintf(file, sizeof(file), "%s", last ? last + 1 : level);
	if (!ae_forge_maps_path(file, path, size) || read_part(path, 0, header, HEADER_BYTES) != HEADER_BYTES ||
		little32(header + 0x1FC) != FORGE_SIGNATURE || ae_forge_maps_kind((long)little32(header + 4)) != AE_FORGE_MAP_XBOX)
		return 0;
	return 1;
}

void ae_forge_maps_xbox_path(char const *level, char *path, long size)
{
	char found[300];

	if (!path[0] || file_exists(path) || !ae_forge_maps_level_path(level, found, (int)sizeof(found)) || (long)strlen(found) >= size)
		return;
	memcpy(path, found, strlen(found) + 1);
}

int ae_forge_maps_xbox_count(void)
{
	int each, count = 0;

	ae_forge_maps_scan();
	for (each = 0; each < map_count; each++)
		count += maps[each].readable && maps[each].kind == AE_FORGE_MAP_XBOX && !maps[each].downloaded;
	return count;
}

int ae_forge_maps_xbox_entry(int index, char *level, int level_size, char *title, int title_size)
{
	int each;

	for (each = 0; each < map_count; each++)
	{
		if (!maps[each].readable || maps[each].kind != AE_FORGE_MAP_XBOX || maps[each].downloaded || index--)
			continue;
		snprintf(level, (size_t)level_size, "%s", maps[each].level);
		upper_copy(title, title_size, maps[each].name);
		return 1;
	}
	return 0;
}

int ae_forge_maps_is_level(char const *level)
{
	int each;

	for (each = 0; level && each < map_count; each++)
	{
		if (maps[each].level[0] && !_stricmp(maps[each].level, level))
			return 1;
	}
	return 0;
}

/* a name this PC's maps do not have, and Forge's save box could make */
static int name_free(char const *name, char *text, int size)
{
	char path[300];

	if (!ae_forge_maps_name_valid(name))
		snprintf(text, (size_t)size, "A name is a-z, 0-9 and _, up to %d letters.", AE_FORGE_MAP_NAME_MAX);
	else if (ae_forge_maps_name_is_stock(name))
		snprintf(text, (size_t)size, "A map of the game has this name.");
	else if (ae_forge_maps_path(name, path, (int)sizeof(path)) || ae_forge_maps_find(name) >= 0)
		snprintf(text, (size_t)size, "A map has this name.");
	else
		return 1;
	return 0;
}

/* the header's name of a file made the new file's (the loader reads the map by it) */
static int name_patch(char const *path, char const *name)
{
	HANDLE file = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
	char field[32];
	DWORD put = 0;
	int ok;

	if (file == INVALID_HANDLE_VALUE)
		return 0;
	memset(field, 0, sizeof(field));
	snprintf(field, sizeof(field), "%s", name);
	ok = SetFilePointer(file, 0x20, NULL, FILE_BEGIN) != (DWORD)-1 && WriteFile(file, field, sizeof(field), &put, NULL) &&
		put == sizeof(field);
	CloseHandle(file);
	return ok;
}

static int copy_as(struct ae_forge_map const *from, char const *name, char *to, int size)
{
	if (snprintf(to, (size_t)size, "d:\\custom_maps\\%s.map", name) >= size || !CopyFileA(from->path, to, TRUE))
		return 0;
	if (!name_patch(to, name))
	{
		DeleteFileA(to);
		return 0;
	}
	return 1;
}

int ae_forge_maps_copy(char const *file, char const *name, char *text, int size)
{
	int index = ae_forge_maps_find(file);
	char to[300];

	if (index < 0)
		snprintf(text, (size_t)size, "The map is not there any more.");
	else if (name_free(name, text, size))
	{
		if (copy_as(&maps[index], name, to, (int)sizeof(to)))
		{
			platform_log("ae forge: %s copied as %s", file, name);
			return 1;
		}
		snprintf(text, (size_t)size, "The copy could not be written.");
	}
	return 0;
}

int ae_forge_maps_rename(char const *file, char const *name, char *text, int size)
{
	int index = ae_forge_maps_find(file);
	char to[300];

	if (index < 0)
		snprintf(text, (size_t)size, "The map is not there any more.");
	else if (maps[index].downloaded)
		snprintf(text, (size_t)size, "A downloaded map keeps its name.");
	else if (name_free(name, text, size))
	{
		if (copy_as(&maps[index], name, to, (int)sizeof(to)))
		{
			DeleteFileA(maps[index].path);
			platform_log("ae forge: %s renamed %s", file, name);
			return 1;
		}
		snprintf(text, (size_t)size, "The map could not be renamed.");
	}
	return 0;
}

int ae_forge_maps_delete(char const *file, char *text, int size)
{
	int index = ae_forge_maps_find(file);

	if (index < 0)
		snprintf(text, (size_t)size, "The map is not there any more.");
	else if (!DeleteFileA(maps[index].path))
		snprintf(text, (size_t)size, "The map's file could not be deleted.");
	else
	{
		platform_log("ae forge: %s deleted", file);
		return 1;
	}
	return 0;
}
