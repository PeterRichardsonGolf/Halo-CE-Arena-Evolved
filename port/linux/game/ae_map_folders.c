/*
AE_MAP_FOLDERS.C

Map folders (ae_map_folders.h; owner 2026-10-17, queue 142). The maps folder's folders (one level) hold maps as the
maps folder does; a map is still known by its name alone (the network, gametypes, the last map saved, layouts,
pictures), so two machines that keep a map in different folders, or loose, play it together (GulchCE's rule for its
package folders: chestahh/GulchCE, CC0, docs/mcc_maps.md).

The first of a name wins, in the game's own order: the stock maps and the mod's, the maps folder's, then the folders
A-Z, then map sharing's downloads (maps_shared). A later one of a name is logged once and left out.
*/

#include "cseries.h"
#include "cseries/cseries_windows.h"
#include "cseries/errors.h"
#include "cache/cache_files.h"
#include "interface/ui_widget.h"
#include "interface/ui_widget_instance.h"
#include "halo_custom_maps.h"
#include "halo_map_families.h"
#include "ae_map_folders_rules.h"
#include "ae_map_folders.h"
#include "../src/ae_discs.h"
#include <xtl.h>
#include <stdio.h>
#include <string.h>

void platform_log(char const *format, ...);
/* (menu_tags.c: a widget definition's string list) */
long pc_menu_string_list(long definition_index);

enum
{
	/* the folders of a maps folder looked at */
	MAXIMUM_FOLDERS = 64,
	PATH_BYTES = 512,
	XBOX_HEADER_BYTES = 2048,
	/* a missed map looks at the folders again after this long (a map put in a folder while the game runs) */
	RESCAN_MILLISECONDS = 3000,
	/* a Custom Edition cache file's header, to its type (a short at 0x60: 1 multiplayer) */
	CE_HEADER_BYTES = 0x64,
	CE_TYPE_OFFSET = 0x60,
	CE_TYPE_MULTIPLAYER = 1,
	/* (menu_functions.c's MAP_KIND_SINGLEPLAYER and MAP_KIND_MULTIPLAYER) */
	KIND_SINGLEPLAYER = 0,
	KIND_MULTIPLAYER = 1
};

/* ---------- files */

static boolean file_exists(char const *path)
{
	HANDLE file = CreateFileA(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);

	if (file == INVALID_HANDLE_VALUE)
		return FALSE;
	CloseHandle(file);
	return TRUE;
}

static boolean read_header(char const *path, unsigned char *header, unsigned long size)
{
	HANDLE file = CreateFileA(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
	unsigned long read = 0;

	if (file == INVALID_HANDLE_VALUE)
		return FALSE;
	if (!ReadFile(file, header, size, &read, NULL))
		read = 0;
	CloseHandle(file);
	return read == size;
}

/* a mod's (modded disc's) main menu map too big for the main menu's cache file (cache_files_windows.c's
MAIN_MENU_CACHE_FILE_MAXIMUM_SIZE): its header's inflated size (offset 8) over it, of the main menu's type (offset 0x60:
2). The Halo 2 Mappack Collection's ui.map inflates to 51.6 MB, and asserted "no cache file can hold map 'ui'" at start
(item 140a): the stock one is used instead, said once in the log */
#define AE_MAIN_MENU_CACHE_MAXIMUM 0x02300000UL

boolean ae_map_folders_mod_map_refused(char const *path, char const *map_name)
{
	static char said[AE_MAP_NAME_SIZE];
	unsigned char header[0x64];
	unsigned long inflated, type;

	if (!read_header(path, header, sizeof(header)) || memcmp(header, "daeh", 4))
		return FALSE;
	inflated = header[8] | (header[9] << 8) | (header[10] << 16) | ((unsigned long)header[11] << 24);
	type = header[0x60] | (header[0x61] << 8);
	if (type != 2 || inflated <= AE_MAIN_MENU_CACHE_MAXIMUM)
		return FALSE;
	if (strncmp(said, map_name, sizeof(said)))
	{
		strncpy(said, map_name, sizeof(said) - 1);
		error(_error_silent, "mod: %s inflates to %lu bytes, more than the main menu's cache file holds (%lu): "
			"the game's own %s.map is used", path, inflated, AE_MAIN_MENU_CACHE_MAXIMUM, map_name);
	}
	return TRUE;
}

/* a name a map is found by: no folder in it */
static boolean plain_name(char const *name)
{
	return name && name[0] && strlen(name) < AE_MAP_NAME_SIZE && !strpbrk(name, "\\/:") && strcmp(name, "..");
}

/* a maps folder's page folders (root ends in its separator), A-Z */
static int page_folders(char const *root, char (*folders)[AE_MAP_FOLDER_NAME_SIZE], int maximum)
{
	WIN32_FIND_DATAA data;
	char pattern[PATH_BYTES];
	HANDLE find;
	int count = 0;

	if (snprintf(pattern, sizeof(pattern), "%s*", root) >= (int)sizeof(pattern))
		return 0;
	find = FindFirstFileA(pattern, &data);
	if (find == INVALID_HANDLE_VALUE)
		return 0;
	do
	{
		if (!(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || !ae_map_folders_folder_is_page(data.cFileName) ||
			strlen(root) + strlen(data.cFileName) + AE_MAP_NAME_SIZE + 8 >= PATH_BYTES)
		{
			continue;
		}
		if (count == maximum)
		{
			platform_log("[maps] %s has more than %d folders: %s and the ones after it are not read", root, maximum,
				data.cFileName);
			break;
		}
		snprintf(folders[count++], AE_MAP_FOLDER_NAME_SIZE, "%s", data.cFileName);
	}
	while (FindNextFileA(find, &data));
	CloseHandle(find);
	ae_map_folders_sort(folders, count);
	return count;
}

static unsigned long little_u32(unsigned char const *p)
{
	return (unsigned long)p[0] | (unsigned long)p[1] << 8 | (unsigned long)p[2] << 16 | (unsigned long)p[3] << 24;
}

/* ---------- Xbox maps: maps\<folder>\<name>.map */

static struct
{
	boolean scanned;
	unsigned long scanned_at;
	int folder_count;
	char folders[MAXIMUM_FOLDERS][AE_MAP_FOLDER_NAME_SIZE];
	/* each folder map's name and folder (the ones a mod's or the maps folder's file of the name does not hide) */
	struct ae_map_folder_index index;
} found_xbox;

static void xbox_scan(void)
{
	char const *root = cache_files_map_directory();
	char const *mod = cache_files_mod_started();
	/* (the duplicates are said at the first look, not at every one after) */
	boolean say = !found_xbox.scanned;
	int folder;

	if (mod && !mod[0])
		mod = NULL;
	found_xbox.scanned = TRUE;
	found_xbox.scanned_at = system_milliseconds();
	ae_map_folder_index_clear(&found_xbox.index);
	found_xbox.folder_count = page_folders(root, found_xbox.folders, MAXIMUM_FOLDERS);
	for (folder = 0; folder < found_xbox.folder_count; folder++)
	{
		WIN32_FIND_DATAA data;
		char pattern[PATH_BYTES];
		HANDLE find;

		snprintf(pattern, sizeof(pattern), "%s%s\\*.map", root, found_xbox.folders[folder]);
		find = FindFirstFileA(pattern, &data);
		if (find == INVALID_HANDLE_VALUE)
			continue;
		do
		{
			unsigned char header[XBOX_HEADER_BYTES];
			char path[PATH_BYTES], other[PATH_BYTES], name[HALO_CUSTOM_MAP_NAME_SIZE + 1];
			char const *first = NULL;

			if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
				snprintf(path, sizeof(path), "%s%s\\%s", root, found_xbox.folders[folder], data.cFileName) >= (int)sizeof(path) ||
				!read_header(path, header, sizeof(header)) || !native_map_header_valid(header, data.cFileName))
			{
				continue;
			}
			memcpy(name, header + 0x20, HALO_CUSTOM_MAP_NAME_SIZE);
			name[HALO_CUSTOM_MAP_NAME_SIZE] = 0;
			if (mod && snprintf(other, sizeof(other), "d:\\%s\\%s\\maps\\%s.map", ae_disc_folder(mod), mod, name) < (int)sizeof(other) &&
				file_exists(other))
			{
				first = "the mod's";
			}
			else if (snprintf(other, sizeof(other), "%s%s.map", root, name) < (int)sizeof(other) && file_exists(other))
				first = "the maps folder's";
			else if (!ae_map_folder_index_add(&found_xbox.index, name, found_xbox.folders[folder]))
				first = ae_map_folder_index_find(&found_xbox.index, name);
			if (first && say)
			{
				platform_log("[maps] %s\\%s is another map named %s: %s is played (the first of a name)",
					found_xbox.folders[folder], data.cFileName, name, first);
			}
		}
		while (FindNextFileA(find, &data));
		CloseHandle(find);
	}
}

/* the folder of an Xbox map in one, or NULL (the folders looked at again when it is not there, now and then) */
static char const *xbox_folder(char const *name)
{
	char const *folder;

	if (!found_xbox.scanned)
		xbox_scan();
	folder = ae_map_folder_index_find(&found_xbox.index, name);
	if (!folder && system_milliseconds() - found_xbox.scanned_at >= RESCAN_MILLISECONDS)
	{
		xbox_scan();
		folder = ae_map_folder_index_find(&found_xbox.index, name);
	}
	return folder;
}

void ae_map_folders_scan(void (*scan)(char const *directory))
{
	char const *root = cache_files_map_directory();
	int folder;

	if (!scan)
		return;
	xbox_scan();
	for (folder = 0; folder < found_xbox.folder_count; folder++)
	{
		char directory[PATH_BYTES];

		if (snprintf(directory, sizeof(directory), "%s%s\\", root, found_xbox.folders[folder]) < (int)sizeof(directory))
			scan(directory);
	}
}

void ae_map_folders_xbox_path(char const *map_name, char *path, long size)
{
	char found[PATH_BYTES];
	char const *folder;

	if (!plain_name(map_name) || !path || size <= 0 || !path[0] || file_exists(path))
		return;
	folder = xbox_folder(map_name);
	if (!folder || snprintf(found, sizeof(found), "%s%s\\%s.map", cache_files_map_directory(), folder, map_name) >=
		(int)sizeof(found) || (long)strlen(found) >= size || !file_exists(found))
	{
		return;
	}
	snprintf(path, (size_t)size, "%s", found);
}

/* ---------- Custom Edition maps: maps_ce\<folder>\<file>.map, maps\ce\<folder>\<file>.map */

static struct
{
	unsigned long read_at;
	boolean read;
	int folder_count;
	/* each folder's name (its page's) and path (ending in its separator), A-Z (maps_ce's first of a name) */
	char names[MAXIMUM_FOLDERS][AE_MAP_FOLDER_NAME_SIZE];
	char paths[MAXIMUM_FOLDERS][PATH_BYTES];
	/* the folders' maps listed (map_family_list), by file, and each one's folder */
	struct ae_map_folder_index index;
	boolean listed;
} found_ce;

static void ce_folders_read(void)
{
	static char found[MAXIMUM_FOLDERS][AE_MAP_FOLDER_NAME_SIZE];
	char roots[2][PATH_BYTES];
	int root, folder, count;

	snprintf(roots[0], sizeof(roots[0]), "d:\\maps_ce\\");
	snprintf(roots[1], sizeof(roots[1]), "%sce\\", cache_files_map_directory());
	found_ce.read = TRUE;
	found_ce.read_at = system_milliseconds();
	found_ce.folder_count = 0;
	for (root = 0; root < 2; root++)
	{
		count = page_folders(roots[root], found, MAXIMUM_FOLDERS);
		for (folder = 0; folder < count && found_ce.folder_count < MAXIMUM_FOLDERS; folder++)
		{
			int at = found_ce.folder_count++;

			/* (in among the others A-Z; maps_ce's before maps\ce's of the same name) */
			while (at > 0 && ae_map_folders_compare(found_ce.names[at - 1], found[folder]) > 0)
			{
				memcpy(found_ce.names[at], found_ce.names[at - 1], sizeof(found_ce.names[at]));
				memcpy(found_ce.paths[at], found_ce.paths[at - 1], sizeof(found_ce.paths[at]));
				at--;
			}
			snprintf(found_ce.names[at], sizeof(found_ce.names[at]), "%s", found[folder]);
			snprintf(found_ce.paths[at], sizeof(found_ce.paths[at]), "%s%s\\", roots[root], found[folder]);
		}
	}
}

/* whether a file is a Custom Edition cache file of a version (and a multiplayer map's, if asked) */
static boolean ce_cache_file(char const *path, long version, boolean multiplayer)
{
	unsigned char header[CE_HEADER_BYTES];

	if (!read_header(path, header, sizeof(header)) || memcmp(header, "daeh", 4) ||
		little_u32(header + 4) != (unsigned long)version)
	{
		return FALSE;
	}
	return !multiplayer || (header[CE_TYPE_OFFSET] | header[CE_TYPE_OFFSET + 1] << 8) == CE_TYPE_MULTIPLAYER;
}

boolean ae_map_folders_ce_find(char const *file, long version, char *path, long size)
{
	int folder;

	if (!plain_name(file) || !path || size <= 0)
		return FALSE;
	if (!found_ce.read || system_milliseconds() - found_ce.read_at >= RESCAN_MILLISECONDS)
		ce_folders_read();
	for (folder = 0; folder < found_ce.folder_count; folder++)
	{
		char candidate[PATH_BYTES];

		if (snprintf(candidate, sizeof(candidate), "%s%s.map", found_ce.paths[folder], file) >= (int)sizeof(candidate) ||
			(long)strlen(candidate) >= size || !ce_cache_file(candidate, version, FALSE))
		{
			continue;
		}
		snprintf(path, (size_t)size, "%s", candidate);
		return TRUE;
	}
	return FALSE;
}

void ae_map_folders_ce_list(short family, long version, void (*found)(char const *file, void *context), void *context,
	char *listed, long *listed_count, long maximum, long file_length)
{
	boolean say = !found_ce.listed;
	int folder;

	if (family != _map_family_custom_edition || !found || !listed || !listed_count || file_length <= 0)
		return;
	ce_folders_read();
	ae_map_folder_index_clear(&found_ce.index);
	found_ce.listed = TRUE;
	for (folder = 0; folder < found_ce.folder_count; folder++)
	{
		WIN32_FIND_DATAA data;
		char pattern[PATH_BYTES];
		HANDLE find;

		snprintf(pattern, sizeof(pattern), "%s*.map", found_ce.paths[folder]);
		find = FindFirstFileA(pattern, &data);
		if (find == INVALID_HANDLE_VALUE)
			continue;
		do
		{
			char file[AE_MAP_NAME_SIZE], path[PATH_BYTES];
			size_t length = strlen(data.cFileName);
			long other;

			if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || length <= 4 ||
				_stricmp(data.cFileName + length - 4, ".map") || length - 4 >= (size_t)file_length ||
				length - 4 >= sizeof(file))
			{
				continue;
			}
			snprintf(file, sizeof(file), "%.*s", (int)(length - 4), data.cFileName);
			/* (<file>.map only: a file named for a family, <file>@found_ce.map, is the maps folder's own place's) */
			if (map_family_parse(file, NULL, 0) != _map_family_xbox || !plain_name(file))
				continue;
			for (other = 0; other < *listed_count && _stricmp(listed + other * file_length, file); other++)
				;
			if (other < *listed_count)
			{
				if (say)
				{
					platform_log("[maps] %s%s is another Custom Edition map named %s: the first of a name is played",
						found_ce.paths[folder], data.cFileName, file);
				}
				continue;
			}
			if (*listed_count >= maximum ||
				snprintf(path, sizeof(path), "%s%s", found_ce.paths[folder], data.cFileName) >= (int)sizeof(path) ||
				!ce_cache_file(path, version, TRUE))
			{
				continue;
			}
			snprintf(listed + *listed_count * file_length, (size_t)file_length, "%s", file);
			(*listed_count)++;
			ae_map_folder_index_add(&found_ce.index, file, found_ce.names[folder]);
			found(file, context);
		}
		while (FindNextFileA(find, &data));
		CloseHandle(find);
	}
}

void ae_map_folders_ce_scan(void (*scan)(char const *directory))
{
	int folder;

	if (!scan)
		return;
	ce_folders_read();
	for (folder = 0; folder < found_ce.folder_count; folder++)
		scan(found_ce.paths[folder]);
}

/* ---------- the pages */

/* a list row's page name ("" for none: loose); the stock rows are the list's first */
static void row_page(char const *name, char *page, unsigned int size)
{
	char file[AE_MAP_NAME_SIZE];
	char const *folder = NULL;
	short family;

	page[0] = 0;
	if (!name)
		return;
	family = map_family_parse(name, file, sizeof(file));
	if (!file[0])
		return;
	if (family == _map_family_xbox)
		folder = xbox_folder(file);
	else if (family == _map_family_custom_edition)
		folder = ae_map_folder_index_find(&found_ce.index, file);
	if (folder)
		ae_map_folders_page_name(folder, page, size);
}

void ae_map_folders_row_page(char const *name, char *page, unsigned int size)
{
	row_page(name, page, size);
}

static void pages_of(char const *const *names, long count, struct ae_map_pages *pages)
{
	static unsigned char stock[AE_MAP_PAGES_MAPS];
	static char folder_names[AE_MAP_PAGES_MAPS][AE_MAP_PAGE_NAME_SIZE];
	static char const *folder_pages[AE_MAP_PAGES_MAPS];
	long index;

	if (count > AE_MAP_PAGES_MAPS)
		count = AE_MAP_PAGES_MAPS;
	if (count < 0 || !names)
		count = 0;
	for (index = 0; index < count; index++)
	{
		stock[index] = index < HALO_STOCK_MULTIPLAYER_MAP_COUNT;
		folder_names[index][0] = 0;
		if (!stock[index])
			row_page(names[index], folder_names[index], sizeof(folder_names[index]));
		folder_pages[index] = folder_names[index];
	}
	ae_map_pages_build(pages, (int)count, stock, folder_pages);
}

void ae_map_folders_log_pages(char const *const *names, long count)
{
	static struct ae_map_pages pages;
	static char logged[AE_MAP_PAGES_MAXIMUM * (AE_MAP_PAGE_NAME_SIZE + 8)];
	char now[sizeof(logged)];
	long length = 0;
	int page;

	pages_of(names, count, &pages);
	now[0] = 0;
	for (page = 0; page < pages.page_count; page++)
		length += snprintf(now + length, sizeof(now) - (size_t)length, "%s=%d;", pages.names[page], pages.count[page]);
	if (!strcmp(now, logged))
		return;
	strcpy(logged, now);
	for (page = 0; page < pages.page_count; page++)
		platform_log("[maps] page %s: %d map%s", pages.names[page], pages.count[page], pages.count[page] == 1 ? "" : "s");
}

boolean ae_map_folders_root_path(char const *map_path, char *path, long size)
{
	char const *file, *slash, *start;
	char folder[AE_MAP_FOLDER_NAME_SIZE], parent[PATH_BYTES], roots[3][PATH_BYTES];
	long folder_length, root;

	if (!map_path || !path || size <= 0)
		return FALSE;
	/* <parent>\<folder>\<file> */
	file = strrchr(map_path, '\\');
	slash = strrchr(map_path, '/');
	if (slash && (!file || slash > file))
		file = slash;
	if (!file || file == map_path)
		return FALSE;
	for (start = file - 1; start > map_path && start[-1] != '\\' && start[-1] != '/'; start--)
		;
	folder_length = (long)(file - start);
	if (start == map_path || folder_length <= 0 || folder_length >= (long)sizeof(folder) ||
		(long)(start - map_path) >= (long)sizeof(parent))
	{
		return FALSE;
	}
	snprintf(folder, sizeof(folder), "%.*s", (int)folder_length, start);
	snprintf(parent, sizeof(parent), "%.*s", (int)(start - map_path), map_path);
	if (!ae_map_folders_folder_is_page(folder))
		return FALSE;
	snprintf(roots[0], sizeof(roots[0]), "%s", cache_files_map_directory());
	snprintf(roots[1], sizeof(roots[1]), "d:\\maps_ce\\");
	snprintf(roots[2], sizeof(roots[2]), "%sce\\", cache_files_map_directory());
	for (root = 0; root < 3 && _stricmp(parent, roots[root]); root++)
		;
	if (root == 3 || snprintf(path, (size_t)size, "%s%s", parent, file + 1) >= size)
	{
		path[0] = 0;
		return FALSE;
	}
	return TRUE;
}

/* ---------- the PC menus' map lists */

static struct
{
	boolean active;
	struct widget_instance *spinner;
	long string_list;
	short list;
	boolean singleplayer;
	char const *const *names;
	short count;
	struct ae_map_pages pages;
	/* the page shown (-1: SINGLEPLAYER), and the one ae_map_pages_turned saw last */
	int page, turned_page;
	/* the chooser's values' names: SINGLEPLAYER, then the pages' */
	wchar_t text[AE_MAP_PAGES_MAXIMUM + 1][AE_MAP_PAGE_NAME_SIZE];
} lists;

/* each list's page and map shown last, this session */
static struct
{
	boolean valid;
	boolean singleplayer;
	char page[AE_MAP_PAGE_NAME_SIZE];
	char map[AE_MAP_NAME_SIZE];
} shown[NUMBER_OF_AE_MAP_PAGES_LISTS];

static void wide_text(wchar_t *text, char const *name)
{
	int index;

	for (index = 0; index + 1 < AE_MAP_PAGE_NAME_SIZE && name[index]; index++)
		text[index] = (wchar_t)(unsigned char)name[index];
	text[index] = 0;
}

/* the list index of a map's name, or -1 */
static int list_map(char const *name)
{
	short index;

	for (index = 0; name[0] && index < lists.count; index++)
	{
		if (lists.names[index] && !_stricmp(lists.names[index], name))
			return index;
	}
	return -1;
}

short ae_map_pages_begin(struct widget_instance *spinner, short list, char const *const *names, short count,
	boolean singleplayer, short last_used)
{
	boolean decided = FALSE;
	int page = -1, entry = 0, index;

	if (list < 0 || list >= NUMBER_OF_AE_MAP_PAGES_LISTS)
		list = AE_MAP_PAGES_MAP_SCREEN;
	lists.active = FALSE;
	if (spinner && names && count > 0)
		pages_of(names, count, &lists.pages);
	if (!spinner || !names || count <= 0 || !lists.pages.page_count)
		return list == AE_MAP_PAGES_MAP_SCREEN ? last_used : NONE;
	lists.active = TRUE;
	lists.spinner = spinner;
	lists.string_list = pc_menu_string_list(spinner->definition_tag_index);
	lists.list = list;
	lists.singleplayer = singleplayer;
	lists.names = names;
	lists.count = count;
	wide_text(lists.text[0], AE_MAP_PAGE_SINGLEPLAYER);
	for (index = 0; index < lists.pages.page_count; index++)
		wide_text(lists.text[index + 1], lists.pages.names[index]);
	/* the page: this session's last of the list's, else the map used last's (New Game: SINGLEPLAYER) */
	if (shown[list].valid)
	{
		if (shown[list].singleplayer)
			decided = singleplayer && list == AE_MAP_PAGES_NEW_GAME;
		else
			decided = (page = ae_map_pages_find(&lists.pages, shown[list].page)) >= 0;
	}
	if (!decided)
	{
		page = -1;
		if (list == AE_MAP_PAGES_MAP_SCREEN || !singleplayer)
			page = ae_map_pages_page_of(&lists.pages, last_used);
	}
	if (page < 0 && (!singleplayer || list == AE_MAP_PAGES_MAP_SCREEN))
		page = 0;
	/* the map on it: the one shown last there, else the map used last, else its first */
	if (page >= 0)
	{
		int map = decided && !shown[list].singleplayer ? list_map(shown[list].map) : -1;

		entry = ae_map_pages_entry_of(&lists.pages, page, map);
		if (entry < 0)
			entry = ae_map_pages_entry_of(&lists.pages, page, last_used);
		if (entry < 0)
			entry = 0;
	}
	lists.page = lists.turned_page = page;
	spinner->parameters.list.number_of_items = (word)ae_map_chooser_count(lists.pages.page_count, singleplayer);
	spinner->parameters.list.selected_index = (short)ae_map_chooser_value(page, lists.pages.page_count, singleplayer);
	return page < 0 ? NONE : (short)entry;
}

boolean ae_map_pages_on(struct widget_instance *spinner)
{
	return lists.active && spinner && spinner == lists.spinner;
}

short ae_map_pages_kind(struct widget_instance *spinner)
{
	int page;

	spinner->parameters.list.number_of_items = (word)ae_map_chooser_count(lists.pages.page_count, lists.singleplayer);
	page = ae_map_chooser_page(spinner->parameters.list.selected_index, lists.pages.page_count, lists.singleplayer);
	spinner->parameters.list.selected_index = (short)ae_map_chooser_value(page, lists.pages.page_count, lists.singleplayer);
	lists.page = page;
	shown[lists.list].valid = TRUE;
	shown[lists.list].singleplayer = page < 0;
	if (page >= 0)
		snprintf(shown[lists.list].page, sizeof(shown[lists.list].page), "%s", lists.pages.names[page]);
	return page < 0 ? KIND_SINGLEPLAYER : KIND_MULTIPLAYER;
}

void ae_map_pages_set(struct widget_instance *spinner, short kind)
{
	if (kind == KIND_SINGLEPLAYER && lists.singleplayer)
		lists.page = -1;
	else if (lists.page < 0)
		lists.page = 0;
	spinner->parameters.list.selected_index = (short)ae_map_chooser_value(lists.page, lists.pages.page_count,
		lists.singleplayer);
}

boolean ae_map_pages_turned(void)
{
	boolean turned;

	if (!lists.active)
		return FALSE;
	turned = lists.page != lists.turned_page;
	lists.turned_page = lists.page;
	return turned;
}

short ae_map_pages_count(short count)
{
	return lists.active && lists.page >= 0 ? (short)lists.pages.count[lists.page] : count;
}

short ae_map_pages_map(short entry)
{
	int map;

	if (!lists.active || lists.page < 0 || entry == NONE)
		return entry;
	map = ae_map_pages_map_at(&lists.pages, lists.page, entry);
	return map < 0 ? NONE : (short)map;
}

short ae_map_pages_shown_map(short entry)
{
	short map = ae_map_pages_map(entry);

	if (lists.active && lists.page >= 0 && map >= 0 && map < lists.count && lists.names[map])
		snprintf(shown[lists.list].map, sizeof(shown[lists.list].map), "%s", lists.names[map]);
	return map;
}

wchar_t const *ae_map_pages_spinner_string(long string_list_index, short string_index)
{
	int page;

	if (!lists.active || string_list_index == NONE || string_list_index != lists.string_list || string_index < 0 ||
		string_index >= ae_map_chooser_count(lists.pages.page_count, lists.singleplayer))
	{
		return NULL;
	}
	page = ae_map_chooser_page(string_index, lists.pages.page_count, lists.singleplayer);
	return lists.text[page + 1];
}
