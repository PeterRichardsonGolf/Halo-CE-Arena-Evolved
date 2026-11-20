/*
AE_MAP_LIBRARY.C

The installed maps library in the game (ae_map_library.h): the folders the game plays maps from, turned into the
host's paths, read on a worker thread through the platform layer's folder calls (ae_map_library_files.h), or in
slices on the menus' thread when no worker starts; MCC's stock maps and the Workshop's maps added from the MCC install's
lists; the maps in use asked of the game and the lobby before a change; and a change's path checked (real path, no
link, inside a Custom Edition or downloads folder) before a file is deleted or renamed.

A scan fills the back list; when it ends, the back list becomes the one shown, so a list is always whole.
*/

#include "cseries.h"
#include "cache/cache_files.h"
#include "networking/network_client_manager.h"
#include "networking/network_game_manager.h"
#include "networking/network_game_globals.h"
#include "networking/network_server_manager_internal.h"
#include "halo_map_families.h"
#include "ae_map_share.h"
#include "ae_glue_setup_maps.h"
#include "ae_map_library.h"
#include "../src/ae_mcc.h"
#include "../src/ae_mcc_workshop.h"
#include "../src/ae_map_library_files.h"
#include "../src/ae_worker.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xtl.h>

void platform_translate_path(const char *xbox_path, char *host_path, unsigned long host_path_size);
void platform_log(const char *format, ...);
unsigned long system_milliseconds(void);

/* a slice's share of a frame: map headers, and time */
#define SLICE_HEADERS 16
#define SLICE_MICROSECONDS 2000ull
/* the Workshop maps added at most */
#define WORKSHOP_MAPS 256

/* ---------- the lists */

/* the list shown and the one a scan fills */
static struct ae_map_entry lists[2][AE_MAP_LIBRARY_MAXIMUM];
static int shown_list;
static int shown_count;
static int ever_read;
static int scanning;
static struct ae_map_scan scan;
/* the Workshop's maps, copied from the MCC install's list at the end of a scan */
static struct ae_map_workshop_copy
{
	struct ae_mcc_workshop_map maps[WORKSHOP_MAPS];
} workshop;

/* ---------- the host's folders, for the scan */

/* the size of the entry read last (a scan reads a header right after its entry; one scan runs at a time) */
static unsigned long long entry_size;

static void *folder_open(void *context, char const *path)
{
	(void)context;
	return ae_map_files_folder_open(path);
}

static int folder_next(void *context, void *folder, char *name, unsigned int size, int *is_folder)
{
	(void)context;
	return ae_map_files_folder_next(folder, name, (int)size, is_folder, &entry_size);
}

static void folder_close(void *context, void *folder)
{
	(void)context;
	ae_map_files_folder_close(folder);
}

static int header_read(void *context, char const *path, unsigned char *header, unsigned int size,
	unsigned long long *file_size)
{
	(void)context;
	*file_size = entry_size;
	return ae_map_files_read(path, header, (int)size);
}

static unsigned long long now_us(void *context)
{
	(void)context;
	/* (the game's clock counts milliseconds: a slice's time is checked to the millisecond, its headers exactly) */
	return (unsigned long long)system_milliseconds() * 1000ull;
}

static struct ae_map_scan_io const host_io = { NULL, folder_open, folder_next, folder_close, header_read, now_us };

/* ---------- the roots */

static void root_add(struct ae_map_root *roots, int *count, char const *path, int source, char const *pack,
	int folders)
{
	struct ae_map_root *root;

	if (*count >= AE_MAP_LIBRARY_ROOTS || !path || !path[0] || strlen(path) >= sizeof(roots[0].path))
		return;
	root = &roots[(*count)++];
	memset(root, 0, sizeof(*root));
	snprintf(root->path, sizeof(root->path), "%s", path);
	root->source = source;
	snprintf(root->pack, sizeof(root->pack), "%s", pack ? pack : "");
	root->folders = folders;
}

/* the folders the game plays maps from, as map_families.c, map_packs.c, mcc_maps.c and the downloads find them */
static int roots_make(struct ae_map_root *roots)
{
	static char const *const not_packs[] = { "ce", "shared", "de", "fr", "es", "it" };
	char const *map_directory = cache_files_map_directory();
	char path[AE_MAP_LIBRARY_PATH_SIZE];
	WIN32_FIND_DATAA data;
	HANDLE find;
	int count = 0;

	root_add(roots, &count, map_directory, AE_MAP_SOURCE_STOCK, "", TRUE);
	root_add(roots, &count, "d:\\maps_ce\\", AE_MAP_SOURCE_CUSTOM_EDITION, "", TRUE);
	if (map_directory && strlen(map_directory) + 4 < sizeof(path))
	{
		snprintf(path, sizeof(path), "%sce\\", map_directory);
		root_add(roots, &count, path, AE_MAP_SOURCE_CUSTOM_EDITION, "", TRUE);
	}
	root_add(roots, &count, "d:\\custom_maps\\", AE_MAP_SOURCE_CUSTOM_EDITION, "", TRUE);
	root_add(roots, &count, ae_map_share_xbox_folder(), AE_MAP_SOURCE_DOWNLOADED, "", FALSE);
	root_add(roots, &count, "d:\\mcc_maps\\", AE_MAP_SOURCE_MCC, "", TRUE);
	/* (the packs: each maps_<pack> folder of the data folder but the ones above and the languages' maps folders) */
	find = FindFirstFileA("d:\\maps_*", &data);
	if (find != INVALID_HANDLE_VALUE)
	{
		do
		{
			char const *pack = data.cFileName + 5;
			int skipped = !(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || _strnicmp(data.cFileName, "maps_", 5) ||
				!pack[0] || strpbrk(pack, "\\/:@") || strlen(pack) >= AE_MAP_LIBRARY_PACK_SIZE;
			unsigned int index;

			for (index = 0; !skipped && index < NUMBEROF(not_packs); index++)
				skipped = !_stricmp(pack, not_packs[index]);
			if (skipped)
				continue;
			snprintf(path, sizeof(path), "d:\\%s\\", data.cFileName);
			root_add(roots, &count, path, AE_MAP_SOURCE_MAP_PACK, pack, FALSE);
		}
		while (FindNextFileA(find, &data));
		CloseHandle(find);
	}
	return count;
}

/* an entry for a map given by host path (MCC's, the Workshop's), its size and header read from the file */
static void listed_add(struct ae_map_entry *list, int *count, char const *name, char const *title, char const *pack,
	char const *host_path, int source)
{
	unsigned char header[AE_MAP_HEADER_SIZE];
	struct ae_map_header_info info;
	struct ae_map_entry *entry;
	unsigned long long size;
	int read;

	if (*count >= AE_MAP_LIBRARY_MAXIMUM || !name || !name[0] || !host_path || !host_path[0] ||
		!ae_map_files_size(host_path, &size))
	{
		return;
	}
	read = ae_map_files_read(host_path, header, (int)sizeof(header));
	entry = &list[(*count)++];
	memset(entry, 0, sizeof(*entry));
	snprintf(entry->name, sizeof(entry->name), "%s", name);
	if (title && title[0])
		snprintf(entry->title, sizeof(entry->title), "%s", title);
	else
		ae_map_library_title(name, entry->title, sizeof(entry->title));
	snprintf(entry->pack, sizeof(entry->pack), "%s", pack ? pack : "");
	snprintf(entry->path, sizeof(entry->path), "%s", host_path);
	entry->size = size;
	entry->source = source;
	entry->root = -1;
	entry->picture = -1;
	if (read == (int)sizeof(header) && ae_map_library_header_read(header, sizeof(header), &info))
	{
		entry->version = info.version;
		entry->serves = ae_map_library_serves(info.type);
	}
	else
		entry->serves = AE_MAP_SERVES_MULTIPLAYER;
}

/* MCC's stock maps in its install, and the Workshop's maps: added after the folders are read */
static void listed_maps_add(struct ae_map_entry *list, int *count)
{
	char root[AE_MAP_LIBRARY_PATH_SIZE];
	int index;
	int maps;

	if (ae_mcc_copy_root(root, sizeof(root)) && root[0])
	{
		for (index = 0; index < ae_mcc_stock_map_count(); index++)
		{
			char const *name = ae_mcc_stock_map(index);
			char path[AE_MAP_LIBRARY_PATH_SIZE];

			if (name && (size_t)snprintf(path, sizeof(path), "%s/halo1/maps/%s.map", root, name) < sizeof(path))
				listed_add(list, count, name, NULL, "mcc", path, AE_MAP_SOURCE_MCC);
		}
	}
	maps = ae_mcc_workshop_catalog_copy(workshop.maps, WORKSHOP_MAPS);
	for (index = 0; index < maps; index++)
	{
		struct ae_mcc_workshop_map const *map = &workshop.maps[index];

		int found;

		if (map->status != AE_MCC_WORKSHOP_PLAYABLE)
			continue;
		/* (an item the game's Steam page downloaded lies in mcc_maps\<item id>, which the folders' read has listed as a
		file of MCC's: that entry is the Workshop map, once) */
		for (found = 0; found < *count; found++)
		{
			struct ae_map_entry *entry = &list[found];

			if (entry->source == AE_MAP_SOURCE_MCC && entry->root >= 0 && !strcmp(entry->folder, map->item_id) &&
				!_stricmp(entry->name, map->name))
			{
				snprintf(entry->title, sizeof(entry->title), "%s", map->title[0] ? map->title : map->item_title);
				snprintf(entry->pack, sizeof(entry->pack), "%s", map->item_id);
				entry->source = AE_MAP_SOURCE_WORKSHOP;
				break;
			}
		}
		if (found == *count)
			listed_add(list, count, map->name, map->title[0] ? map->title : map->item_title, map->item_id, map->path,
				AE_MAP_SOURCE_WORKSHOP);
	}
}

/* the maps the game downloaded told from the player's own: a map a host sent lands at the top of maps_ce, one the map archive
installed at the top of custom_maps (the downloads' list and game.downloaded_maps name them), so only the maps there are asked about */
static void downloads_mark(struct ae_map_entry *list, int count, struct ae_map_root const *roots, int root_count)
{
	int index;

	for (index = 0; index < count; index++)
	{
		struct ae_map_entry *entry = &list[index];

		if (entry->source == AE_MAP_SOURCE_CUSTOM_EDITION && !entry->folder[0] && entry->root >= 0 &&
			entry->root < root_count && (!_stricmp(roots[entry->root].path, "d:\\maps_ce\\") ||
			!_stricmp(roots[entry->root].path, "d:\\custom_maps\\")) &&
			map_is_downloaded(entry->name))
		{
			entry->source = AE_MAP_SOURCE_DOWNLOADED;
		}
	}
}

/* ---------- the scan */

/* the roots of the scan under way (and the list shown's), as the game names them: a map's game path is made from its
root's, for the loaded-map checks */
static struct ae_map_root game_roots[AE_MAP_LIBRARY_ROOTS];
static int game_root_count;
/* ... and the list shown's (its entries' root indexes are into these) */
static struct ae_map_root shown_roots[AE_MAP_LIBRARY_ROOTS];
static int shown_root_count;
/* the worker: its lock, whether it runs, whether it is done, whether a new scan was asked for while it ran */
static struct ae_worker_lock *worker_lock;
static int worker_running;
static int worker_done;
static int worker_again;
/* whether the scan under way runs on the worker */
static int scan_on_worker;
/* the time a scan's end took on the menus' thread (MCC's and the Workshop's maps, the downloads marked) */
static unsigned int finish_us;

static void *scan_worker(void *argument)
{
	(void)argument;
	while (ae_map_scan_step(&scan, &host_io, 1 << 30, ~0ull))
	{
	}
	ae_worker_lock_take(worker_lock);
	worker_done = TRUE;
	ae_worker_lock_give(worker_lock);
	return NULL;
}

static void scan_finished(void)
{
	unsigned long long start = now_us(NULL);
	int back = !shown_list;
	int count = scan.count;

	unsigned long long listed;

	listed_maps_add(lists[back], &count);
	listed = now_us(NULL);
	downloads_mark(lists[back], count, game_roots, game_root_count);
	memcpy(shown_roots, game_roots, sizeof(game_roots[0]) * (size_t)game_root_count);
	shown_root_count = game_root_count;
	shown_list = back;
	shown_count = count;
	scanning = FALSE;
	ever_read = TRUE;
	finish_us = (unsigned int)(now_us(NULL) - start);
	platform_log("[maps] library: %d maps (%d not maps, %d over the limit, %d folders not opened) in %d steps on %s, "
		"longest %u us, all %u us, its end %u us on the menus' thread (MCC and Workshop %u us, downloads marked %u us)",
		shown_count, scan.not_maps, scan.dropped, scan.unopened, scan.steps, scan_on_worker ? "a worker" :
		"the menus' thread", (unsigned int)scan.longest_step_us, (unsigned int)scan.total_us, finish_us,
		(unsigned int)(listed - start), (unsigned int)(now_us(NULL) - listed));
}

/* a scan begun over the roots (the host's paths of the game's folders); on the worker when one starts */
static void scan_begin(void)
{
	struct ae_map_root roots[AE_MAP_LIBRARY_ROOTS];
	int index;

	game_root_count = roots_make(game_roots);
	for (index = 0; index < game_root_count; index++)
	{
		roots[index] = game_roots[index];
		platform_translate_path(game_roots[index].path, roots[index].path, sizeof(roots[index].path));
	}
	ae_map_scan_begin(&scan, roots, game_root_count, lists[!shown_list], AE_MAP_LIBRARY_MAXIMUM);
	scanning = TRUE;
	if (!worker_lock)
		worker_lock = ae_worker_lock_new();
	worker_done = FALSE;
	worker_running = worker_lock != NULL;
	if (worker_running && !ae_worker_start(scan_worker, NULL))
		worker_running = FALSE;
	scan_on_worker = worker_running;
}

void ae_map_library_refresh(void)
{
	/* (a worker is never stopped: the scan after it begins when it ends) */
	if (worker_running)
	{
		worker_again = TRUE;
		return;
	}
	if (scanning)
		ae_map_scan_end(&scan, &host_io);
	scan_begin();
}

/* the worker's scan ended (TRUE), its list taken or a new scan begun */
static int worker_finished(void)
{
	int done;

	ae_worker_lock_take(worker_lock);
	done = worker_done;
	ae_worker_lock_give(worker_lock);
	if (!done)
		return FALSE;
	worker_running = FALSE;
	if (worker_again)
	{
		worker_again = FALSE;
		scan_begin();
		return FALSE;
	}
	scan_finished();
	return TRUE;
}

void ae_map_library_update(void)
{
	if (!scanning && !ever_read)
		ae_map_library_refresh();
	if (!scanning)
		return;
	if (worker_running)
		worker_finished();
	else if (!ae_map_scan_step(&scan, &host_io, SLICE_HEADERS, SLICE_MICROSECONDS))
		scan_finished();
}

unsigned long long ae_map_library_finish(void)
{
	unsigned long long start = now_us(NULL);

	if (!scanning && !ever_read)
		ae_map_library_refresh();
	/* (a worker's scan waited for, ten seconds at most: still running then, the scan is left to it) */
	while (scanning && worker_running && now_us(NULL) - start < 10000000ull)
	{
		if (!worker_finished() && worker_running)
			ae_worker_sleep(1);
	}
	if (scanning && !worker_running)
	{
		while (ae_map_scan_step(&scan, &host_io, 1 << 30, ~0ull))
		{
		}
		scan_finished();
	}
	return now_us(NULL) - start;
}

int ae_map_library_scanning(void)
{
	return scanning;
}

int ae_map_library_ready(void)
{
	return ever_read;
}

int ae_map_library_count(void)
{
	return shown_count;
}

struct ae_map_entry const *ae_map_library_entry(int index)
{
	return index >= 0 && index < shown_count ? &lists[shown_list][index] : NULL;
}

int ae_map_library_entry_at_top_of(struct ae_map_entry const *entry, char const *game_folder)
{
	return entry && game_folder && !entry->folder[0] && entry->root >= 0 && entry->root < shown_root_count &&
		!_stricmp(shown_roots[entry->root].path, game_folder);
}

int ae_map_library_query(struct ae_map_filter const *filter, int sort, int *indexes, int maximum)
{
	return ae_map_library_select(lists[shown_list], shown_count, filter, sort, indexes, maximum);
}

int ae_map_library_find_named(char const *name, char const *pack)
{
	return ae_map_library_find(lists[shown_list], shown_count, name, pack);
}

void ae_map_library_list_name(struct ae_map_entry const *entry, char *name, unsigned int size)
{
	if (!size)
		return;
	name[0] = 0;
	if (!entry)
		return;
	switch (entry->source)
	{
	case AE_MAP_SOURCE_MAP_PACK:
		snprintf(name, size, "%s@%s", entry->name, entry->pack);
		break;
	case AE_MAP_SOURCE_MCC:
		snprintf(name, size, "%s@mcc", entry->name);
		break;
	case AE_MAP_SOURCE_WORKSHOP:
		snprintf(name, size, "%s@ws%s", entry->name, entry->pack);
		break;
	default:
		snprintf(name, size, "%s", entry->name);
		break;
	}
}

int ae_map_library_picture(int index, int *width, int *height)
{
	struct ae_map_entry *entry;
	char name[AE_MAP_LIBRARY_NAME_SIZE + AE_MAP_LIBRARY_PACK_SIZE + 8];
	int image;

	if (index < 0 || index >= shown_count)
		return 0;
	entry = &lists[shown_list][index];
	ae_map_library_list_name(entry, name, sizeof(name));
	/* (ae_setup_map_picture_named keeps each picture it loads: asking again costs nothing) */
	image = ae_setup_map_picture_named(name, width, height);
	entry->picture = image ? image : -1;
	return image;
}

/* ---------- required files */

static int host_file_exists(void *context, char const *path)
{
	char host[AE_MAP_LIBRARY_PATH_SIZE];

	(void)context;
	platform_translate_path(path, host, sizeof(host));
	return ae_map_files_is_file(host);
}

int ae_map_library_missing(int kind, struct ae_map_required_file *files, int maximum)
{
	if (kind == AE_MAP_REQUIRED_CUSTOM_EDITION)
	{
		/* (where map_family_resource looks: maps_ce, the maps folder's ce, custom_maps; MCC's Custom Edition folder) */
		char map_ce[AE_MAP_LIBRARY_PATH_SIZE];
		char mcc[AE_MAP_LIBRARY_PATH_SIZE];
		char root[AE_MAP_LIBRARY_PATH_SIZE];
		char const *folders[4];
		int count = 0;
		int listed;
		int index;

		folders[count++] = "d:\\maps_ce\\";
		snprintf(map_ce, sizeof(map_ce), "%sce\\", cache_files_map_directory());
		folders[count++] = map_ce;
		folders[count++] = "d:\\custom_maps\\";
		listed = ae_map_library_required(kind, folders, count, host_file_exists, NULL, files, maximum);
		/* (MCC's own Custom Edition files count too: a host path, checked as it is) */
		if (ae_mcc_copy_root(root, sizeof(root)) && root[0] &&
			(size_t)snprintf(mcc, sizeof(mcc), "%s/halo1/maps/custom_edition/", root) < sizeof(mcc))
		{
			for (index = 0; index < listed; index++)
			{
				char path[AE_MAP_LIBRARY_PATH_SIZE];

				if (!files[index].found && (size_t)snprintf(path, sizeof(path), "%s%s", mcc, files[index].file) <
					sizeof(path) && ae_map_files_is_file(path))
				{
					files[index].found = TRUE;
					snprintf(files[index].where, sizeof(files[index].where), "%s", mcc);
				}
			}
		}
		return listed;
	}
	if (kind == AE_MAP_REQUIRED_MCC)
	{
		char const *folders[1] = { "d:\\mcc_maps\\_shared\\" };

		return ae_map_library_required(kind, folders, 1, host_file_exists, NULL, files, maximum);
	}
	return 0;
}

/* ---------- delete and move */

/* a name a level or game map is known by, as the library's: its last path part, the part before any @ */
static void level_base_name(char const *level, char *name, unsigned int size)
{
	char const *base = level;
	char const *at;
	char const *p;
	size_t length;

	name[0] = 0;
	if (!level)
		return;
	for (p = level; *p; p++)
	{
		if (*p == '\\' || *p == '/')
			base = p + 1;
	}
	at = strchr(base, '@');
	length = at ? (size_t)(at - base) : strlen(base);
	if (length >= size)
		length = size - 1;
	memcpy(name, base, length);
	name[length] = 0;
	if (length > 4 && !_stricmp(name + length - 4, ".map"))
		name[length - 4] = 0;
}

/* the maps the game has in use: the loaded one, the Custom Edition one, the lobby's (a client's and a host's) */
static int in_use_names(char names[AE_MAP_LIBRARY_IN_USE][AE_MAP_LIBRARY_NAME_SIZE], char const **pointers)
{
	struct network_game_client *client = global_network_game_client_get();
	struct network_game_server *server = global_network_game_server_get();
	struct network_game *game;
	int count = 0;
	int index;

	level_base_name(cache_file_loaded_map_name(), names[count], AE_MAP_LIBRARY_NAME_SIZE);
	count++;
	level_base_name(custom_edition_cache_loaded_path(), names[count], AE_MAP_LIBRARY_NAME_SIZE);
	count++;
	game = client ? network_game_client_get_game(client) : NULL;
	if (game)
		level_base_name(game->map.name, names[count++], AE_MAP_LIBRARY_NAME_SIZE);
	game = server ? network_game_server_get_game(server) : NULL;
	if (game)
		level_base_name(game->map.name, names[count++], AE_MAP_LIBRARY_NAME_SIZE);
	for (index = 0; index < count; index++)
		pointers[index] = names[index];
	return count;
}

/* the real host paths of the folders a change may touch (the Custom Edition, downloads and maps folders), made by each
change's check */
static char change_roots[AE_MAP_LIBRARY_ROOTS][AE_MAP_LIBRARY_PATH_SIZE];
static char const *change_root_pointers[AE_MAP_LIBRARY_ROOTS];
static int change_root_count;

static void change_roots_make(void)
{
	struct ae_map_root roots[AE_MAP_LIBRARY_ROOTS];
	int count = roots_make(roots);
	int index;

	change_root_count = 0;
	for (index = 0; index < count; index++)
	{
		char host[AE_MAP_LIBRARY_PATH_SIZE];

		/* (the maps folder's too, for the Custom Edition maps kept there: its stock and Xbox maps are refused by their
		source before any path is looked at) */
		if (roots[index].source != AE_MAP_SOURCE_CUSTOM_EDITION && roots[index].source != AE_MAP_SOURCE_DOWNLOADED &&
			roots[index].source != AE_MAP_SOURCE_STOCK)
		{
			continue;
		}
		platform_translate_path(roots[index].path, host, sizeof(host));
		if (!ae_map_files_real_path(host, change_roots[change_root_count], AE_MAP_LIBRARY_PATH_SIZE))
			continue;
		change_root_pointers[change_root_count] = change_roots[change_root_count];
		change_root_count++;
	}
}

/* an entry's path as the game names it (its root's game path, its folder, its file), for the loaded-map checks */
static void game_path(struct ae_map_entry const *entry, char *path, unsigned int size)
{
	path[0] = 0;
	if (entry->root < 0 || entry->root >= shown_root_count)
		return;
	if (entry->folder[0])
		snprintf(path, size, "%s%s\\%s.map", shown_roots[entry->root].path, entry->folder, entry->name);
	else
		snprintf(path, size, "%s%s.map", shown_roots[entry->root].path, entry->name);
}

/* the checks a change makes before it: a list read, the source, the maps in use, the file's real path (into real) */
static int change_check(struct ae_map_entry const *entry, char *real, unsigned int size)
{
	char names[AE_MAP_LIBRARY_IN_USE][AE_MAP_LIBRARY_NAME_SIZE];
	char const *in_use[AE_MAP_LIBRARY_IN_USE];
	int answer;

	if (!ever_read)
		return AE_MAP_REFUSED_SCANNING;
	if (!entry)
		return AE_MAP_REFUSED_UNKNOWN_MAP;
	answer = ae_map_library_may_change(entry, in_use, in_use_names(names, in_use));
	if (answer != AE_MAP_CHANGE_ALLOWED)
		return answer;
	change_roots_make();
	return ae_map_files_path_check(entry->path, change_root_pointers, change_root_count, real, (int)size);
}

int ae_map_library_may_change_now(int index)
{
	char real[AE_MAP_LIBRARY_PATH_SIZE];

	return change_check(ae_map_library_entry(index), real, sizeof(real));
}

int ae_map_library_entry_in_use(int index)
{
	char names[AE_MAP_LIBRARY_IN_USE][AE_MAP_LIBRARY_NAME_SIZE];
	char const *in_use[AE_MAP_LIBRARY_IN_USE];
	struct ae_map_entry const *entry = ae_map_library_entry(index);

	if (!entry)
		return 0;
	if (entry->source != AE_MAP_SOURCE_WORKSHOP)
		return ae_map_library_may_change_now(index) == AE_MAP_REFUSED_IN_USE;
	return ae_map_library_name_in_use(entry->name, in_use, in_use_names(names, in_use));
}

int ae_map_library_workshop_item_maps(unsigned long long item, int *in_use)
{
	int each, count = ae_map_library_count(), maps = 0;

	if (in_use)
		*in_use = 0;
	for (each = 0; item && each < count; each++)
	{
		struct ae_map_entry const *entry = ae_map_library_entry(each);

		if (!entry || entry->source != AE_MAP_SOURCE_WORKSHOP || strtoull(entry->pack, NULL, 10) != item)
			continue;
		maps++;
		if (in_use && ae_map_library_entry_in_use(each))
			*in_use = 1;
	}
	return maps;
}

/* the file beside a map of another extension (".bmp", ".txt"): its path with .map swapped, checked as the map was
(a plain file inside the change roots): 1 and checked filled */
static int beside(char const *map_path, char const *extension, char *checked, unsigned int size)
{
	char path[AE_MAP_LIBRARY_PATH_SIZE];
	size_t length = strlen(map_path);

	if (length < 4 || length - 4 + strlen(extension) >= sizeof(path))
		return 0;
	memcpy(path, map_path, length - 4);
	strcpy(path + length - 4, extension);
	return ae_map_files_exists(path) &&
		ae_map_files_path_check(path, change_root_pointers, change_root_count, checked, (int)size) ==
			AE_MAP_CHANGE_ALLOWED;
}

/* the change done: the entry gone from the list shown, the game's lists told, a new scan begun */
static void changed(int index)
{
	struct ae_map_entry *list = lists[shown_list];

	if (index >= 0 && index < shown_count)
	{
		memmove(&list[index], &list[index + 1], sizeof(list[0]) * (size_t)(shown_count - index - 1));
		shown_count--;
	}
	custom_edition_maps_look_again();
	ae_map_library_refresh();
}

static char const *const beside_extensions[] = { ".bmp", ".txt" };

int ae_map_library_delete(int index)
{
	struct ae_map_entry const *entry = ae_map_library_entry(index);
	char real[AE_MAP_LIBRARY_PATH_SIZE];
	char game[AE_MAP_LIBRARY_PATH_SIZE];
	char name[AE_MAP_LIBRARY_NAME_SIZE];
	unsigned int extra;
	int answer = change_check(entry, real, sizeof(real));

	if (answer != AE_MAP_CHANGE_ALLOWED)
		return answer;
	/* (an open cache of it let go first; FALSE: it is the loaded one) */
	game_path(entry, game, sizeof(game));
	if (game[0] && !map_share_release_map(game))
		return AE_MAP_REFUSED_IN_USE;
	if (!ae_map_files_delete(real))
		return AE_MAP_REFUSED_FAILED;
	for (extra = 0; extra < NUMBEROF(beside_extensions); extra++)
	{
		char path[AE_MAP_LIBRARY_PATH_SIZE];

		if (beside(real, beside_extensions[extra], path, sizeof(path)))
			ae_map_files_delete(path);
	}
	snprintf(name, sizeof(name), "%s", entry->name);
	platform_log("[maps] library: deleted %s (%s)", name, real);
	changed(index);
	return AE_MAP_CHANGE_ALLOWED;
}

int ae_map_library_move(int index, char const *folder)
{
	struct ae_map_entry const *entry = ae_map_library_entry(index);
	char real[AE_MAP_LIBRARY_PATH_SIZE];
	char game[AE_MAP_LIBRARY_PATH_SIZE];
	char name[AE_MAP_LIBRARY_NAME_SIZE];
	char host_root[AE_MAP_LIBRARY_PATH_SIZE];
	char real_root[AE_MAP_LIBRARY_PATH_SIZE];
	char target[AE_MAP_LIBRARY_PATH_SIZE];
	unsigned int extra;
	int answer;

	if (!folder)
		folder = "";
	if (!ae_map_library_folder_name_ok(folder))
		return AE_MAP_REFUSED_FOLDER_NAME;
	answer = change_check(entry, real, sizeof(real));
	if (answer != AE_MAP_CHANGE_ALLOWED)
		return answer;
	/* (Custom Edition maps move within maps_ce, where the game finds its folders' maps; Xbox ones into the maps
	folder's, where they are the player's own) */
	platform_translate_path(ae_map_library_move_root_source(entry->version) == AE_MAP_SOURCE_CUSTOM_EDITION ?
		"d:\\maps_ce\\" : cache_files_map_directory(), host_root, sizeof(host_root));
	/* (the root's real path, so the folder the map is in already is seen as its own) */
	if (!ae_map_files_real_path(host_root, real_root, (int)sizeof(real_root)))
		return AE_MAP_REFUSED_FAILED;
	if (!ae_map_library_move_target(real_root, folder, entry->name, target, sizeof(target)))
		return AE_MAP_REFUSED_PATH_TOO_LONG;
	answer = ae_map_library_move_answer(real, target, ae_map_files_exists(target));
	if (answer != AE_MAP_CHANGE_ALLOWED)
		return answer;
	/* (an open cache of it let go first, before anything is made; FALSE: it is the loaded one) */
	game_path(entry, game, sizeof(game));
	if (game[0] && !map_share_release_map(game))
		return AE_MAP_REFUSED_IN_USE;
	if (folder[0])
	{
		char made[AE_MAP_LIBRARY_PATH_SIZE];
		char made_real[AE_MAP_LIBRARY_PATH_SIZE];
		size_t length = strlen(target) - strlen(entry->name) - 5;

		memcpy(made, target, length);
		made[length] = 0;
		/* (the folder a real one where it is named: one that is a link, or leads out of the root, is refused) */
		if (ae_map_files_is_link(made))
			return AE_MAP_REFUSED_LINK;
		if (!ae_map_files_make_folder(made))
			return AE_MAP_REFUSED_FAILED;
		if (!ae_map_files_real_path(made, made_real, (int)sizeof(made_real)) || ae_map_library_move_answer(made_real,
			made, FALSE) != AE_MAP_REFUSED_SAME_FOLDER)
		{
			return AE_MAP_REFUSED_OUTSIDE_ROOTS;
		}
	}
	if (!ae_map_files_rename_new(real, target))
		return ae_map_files_exists(target) ? AE_MAP_REFUSED_TARGET_TAKEN : AE_MAP_REFUSED_FAILED;
	for (extra = 0; extra < NUMBEROF(beside_extensions); extra++)
	{
		char from[AE_MAP_LIBRARY_PATH_SIZE];
		char to[AE_MAP_LIBRARY_PATH_SIZE];
		size_t length = strlen(target);

		/* (the map is gone from real now: its old name's files beside it, found by name) */
		if (length < 4 || length - 4 + strlen(beside_extensions[extra]) >= sizeof(to))
			continue;
		memcpy(to, target, length - 4);
		strcpy(to + length - 4, beside_extensions[extra]);
		if (beside(real, beside_extensions[extra], from, sizeof(from)))
			ae_map_files_rename_new(from, to);
	}
	snprintf(name, sizeof(name), "%s", entry->name);
	platform_log("[maps] library: moved %s to %s", name, target);
	changed(index);
	return AE_MAP_CHANGE_ALLOWED;
}

/* ---------- DEV MODE */

static int dev_entry(struct ae_map_command const *command, char *line, unsigned int size)
{
	int index = ae_map_library_find_named(command->text, command->pack);

	if (index == -1)
		snprintf(line, size, "maps: %s", ae_map_library_refusal_text(AE_MAP_REFUSED_UNKNOWN_MAP));
	else if (index == -2)
		snprintf(line, size, "maps: %s", ae_map_library_refusal_text(AE_MAP_REFUSED_TWO_OF_A_NAME));
	return index;
}

int ae_map_library_dev(char const *words, char *line, unsigned int size)
{
	static int indexes[AE_MAP_LIBRARY_MAXIMUM];
	struct ae_map_command command;
	unsigned long long took;
	int index;
	int answer;

	if (!line || !size)
		return 0;
	if (!ae_map_library_command_parse(words, &command))
	{
		snprintf(line, size, "usage: maps list [<source>:] [<text>] | missing | scan | delete <map>[@<pack>] | move "
			"<map>[@<pack>] <folder>|.");
		return 0;
	}
	if (command.verb == AE_MAP_COMMAND_SCAN)
		ae_map_library_refresh();
	took = ae_map_library_finish();
	/* (the worker still reading after the wait: its scan is not read here) */
	if (worker_running)
	{
		snprintf(line, size, "maps: %s", ae_map_library_refusal_text(AE_MAP_REFUSED_SCANNING));
		return 0;
	}
	switch (command.verb)
	{
	case AE_MAP_COMMAND_SCAN:
		snprintf(line, size, "maps: scanned %d maps in %u us, reading %u us, %u us on the menus' thread at its end (%d not "
			"maps, %d over the limit, %d folders not opened)", shown_count, (unsigned int)took,
			(unsigned int)scan.total_us, finish_us, scan.not_maps, scan.dropped, scan.unopened);
		return 1;
	case AE_MAP_COMMAND_LIST:
	{
		struct ae_map_filter filter;
		int kept;

		memset(&filter, 0, sizeof(filter));
		snprintf(filter.text, sizeof(filter.text), "%s", command.text);
		if (command.source >= 0)
			filter.sources = AE_MAP_SOURCE_BIT(command.source);
		kept = ae_map_library_query(&filter, AE_MAP_SORT_NAME, indexes, AE_MAP_LIBRARY_MAXIMUM);
		ae_map_library_list_line(lists[shown_list], shown_count, indexes, kept, line, size);
		return 1;
	}
	case AE_MAP_COMMAND_MISSING:
	{
		struct ae_map_required_file files[AE_MAP_REQUIRED_MAXIMUM];
		size_t used;
		int count;

		count = ae_map_library_missing(AE_MAP_REQUIRED_CUSTOM_EDITION, files, AE_MAP_REQUIRED_MAXIMUM);
		ae_map_library_missing_line(AE_MAP_REQUIRED_CUSTOM_EDITION, files, count, line, size);
		used = strlen(line);
		if (used + 3 < size)
		{
			line[used++] = ';';
			line[used++] = ' ';
			count = ae_map_library_missing(AE_MAP_REQUIRED_MCC, files, AE_MAP_REQUIRED_MAXIMUM);
			ae_map_library_missing_line(AE_MAP_REQUIRED_MCC, files, count, line + used, size - (unsigned int)used);
		}
		return 1;
	}
	case AE_MAP_COMMAND_DELETE:
	case AE_MAP_COMMAND_MOVE:
		index = dev_entry(&command, line, size);
		if (index < 0)
			return 0;
		answer = command.verb == AE_MAP_COMMAND_DELETE ? ae_map_library_delete(index) :
			ae_map_library_move(index, command.folder);
		if (answer == AE_MAP_CHANGE_ALLOWED)
		{
			snprintf(line, size, "maps: %s %s%s%s", command.verb == AE_MAP_COMMAND_DELETE ? "deleted" : "moved",
				command.text, command.verb == AE_MAP_COMMAND_MOVE ? " to " : "",
				command.verb == AE_MAP_COMMAND_MOVE ? (command.folder[0] ? command.folder : "the top folder") : "");
			return 1;
		}
		snprintf(line, size, "maps: %s refused: %s", command.text, ae_map_library_refusal_text(answer));
		return 0;
	default:
		return 0;
	}
}
