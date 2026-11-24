/*
AE_PACK_VERSIONS.C

A pack map of another version: the host's copy beside the player's own (ae_pack_versions.h).
*/

#include "cseries.h"
#include "cseries/cseries_windows.h"
#include "cseries/errors.h"
#include "networking/network_game_globals.h"
#include "networking/network_client_manager.h"
#include "networking/network_server_manager.h"
#include "halo_map_families.h"
#include "halo_map_packs.h"
#include "ae_map_share.h"
#include "ae_pack_versions.h"
#include "ae_pack_versions_rules.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <xtl.h>

boolean config_boolean(const char *name);

enum
{
	LEVEL_TEXT = 96,
	PATH_TEXT = 256,
	/* (the fingerprints kept for the run, by the file's size and time) */
	FINGERPRINTS_KEPT = 16,
	/* (the map identity check waits this long at most for the host's copy) */
	FETCH_WAIT_MS = 10 * 60 * 1000
};

/* the network game's map and the host's version, noted at the join */
static struct
{
	char map[LEVEL_TEXT];
	unsigned long version;
	/* (the use of its copy logged and its last use written, once a join) */
	int used_said;
	/* (map sharing fetches the host's copy for this join: since when) */
	unsigned long fetching_since;
} game;

static struct
{
	char path[PATH_TEXT];
	unsigned long size, time_low, time_high, fingerprint;
} fingerprints[FINGERPRINTS_KEPT];
static int fingerprint_next;

/* (the bytes of every copy, counted again after a copy arrives or goes) */
static struct
{
	int counted;
	unsigned long long bytes;
} total;

static int handle_read(
	void *context,
	unsigned long offset,
	void *buffer,
	unsigned long size)
{
	HANDLE file = *(HANDLE *)context;
	unsigned long bytes_read = 0;

	if (SetFilePointer(file, (long)offset, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER)
		return 0;
	return ReadFile(file, buffer, size, &bytes_read, NULL) && bytes_read == size;
}

unsigned long ae_pack_versions_fingerprint(
	char const *path)
{
	FILETIME written;
	unsigned long size, fingerprint;
	HANDLE handle;
	int index;

	if (!path || !path[0] || strlen(path) >= PATH_TEXT)
		return 0;
	handle = CreateFileA(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (handle == INVALID_HANDLE_VALUE)
		return 0;
	size = GetFileSize(handle, NULL);
	memset(&written, 0, sizeof(written));
	GetFileTime(handle, NULL, NULL, &written);
	for (index = 0; index < FINGERPRINTS_KEPT; index++)
	{
		if (fingerprints[index].fingerprint && !_stricmp(fingerprints[index].path, path) &&
			fingerprints[index].size == size && fingerprints[index].time_low == (unsigned long)written.dwLowDateTime &&
			fingerprints[index].time_high == (unsigned long)written.dwHighDateTime)
		{
			CloseHandle(handle);
			return fingerprints[index].fingerprint;
		}
	}
	fingerprint = size == INVALID_FILE_SIZE ? 0 : map_pack_fingerprint_read(size, handle_read, &handle);
	CloseHandle(handle);
	if (!fingerprint)
		return 0;
	index = fingerprint_next++ % FINGERPRINTS_KEPT;
	snprintf(fingerprints[index].path, sizeof(fingerprints[index].path), "%s", path);
	fingerprints[index].size = size;
	fingerprints[index].time_low = (unsigned long)written.dwLowDateTime;
	fingerprints[index].time_high = (unsigned long)written.dwHighDateTime;
	fingerprints[index].fingerprint = fingerprint;
	return fingerprint;
}

/* the pack and file of a pack's map, <file>@<pack> */
static int pack_map(
	char const *map_name,
	char *file,
	long file_size,
	char *pack,
	long pack_size)
{
	return map_name && map_family_parse(map_name, file, file_size) == _map_family_pack &&
		map_family_pack_id(map_name, pack, pack_size) && file[0] && pack[0];
}

static int same_map(
	char const *a,
	char const *b)
{
	return a && b && a[0] && !_stricmp(a, b);
}

static int file_here(
	char const *path)
{
	HANDLE file = CreateFileA(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (file == INVALID_HANDLE_VALUE)
		return 0;
	CloseHandle(file);
	return 1;
}

/* the last use of a copy: a small file beside it holding the time, written when the copy is used */
static void last_use_path(
	char const *folder,
	char *path,
	long size)
{
	snprintf(path, (size_t)size, "%slast_use.txt", folder);
}

static void last_use_write(
	char const *folder)
{
	char path[PATH_TEXT], text[32];
	unsigned long written = 0;
	HANDLE file;
	int length;

	last_use_path(folder, path, sizeof(path));
	length = snprintf(text, sizeof(text), "%lld\n", (long long)time(NULL));
	file = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == INVALID_HANDLE_VALUE)
		return;
	WriteFile(file, text, (unsigned long)length, &written, NULL);
	CloseHandle(file);
}

static unsigned long long last_use_read(
	char const *folder)
{
	char path[PATH_TEXT], text[32];
	unsigned long bytes_read = 0;
	unsigned long long value = 0;
	HANDLE file;
	int index;

	last_use_path(folder, path, sizeof(path));
	file = CreateFileA(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == INVALID_HANDLE_VALUE)
		return 0;
	if (!ReadFile(file, text, sizeof(text) - 1, &bytes_read, NULL))
		bytes_read = 0;
	CloseHandle(file);
	text[bytes_read] = 0;
	for (index = 0; text[index] >= '0' && text[index] <= '9'; index++)
		value = value * 10 + (unsigned long long)(text[index] - '0');
	return value;
}

void ae_pack_versions_note(
	char const *map_name,
	unsigned long version)
{
	/* (each join's check: the copy's use is said and written again for this game) */
	game.used_said = 0;
	game.fetching_since = 0;
	snprintf(game.map, sizeof(game.map), "%s", map_name ? map_name : "");
	game.version = version;
}

/* the copy of the game's version of its map, when it is here and is that version */
static int game_copy(
	char *path,
	long size)
{
	char file[LEVEL_TEXT], pack[MAP_PACK_ID_LENGTH + 1];

	return game.version && pack_map(game.map, file, sizeof(file), pack, sizeof(pack)) &&
		ae_pack_versions_path(path, (int)size, pack, file, game.version) && file_here(path) &&
		ae_pack_versions_fingerprint(path) == game.version;
}

static void copy_used(
	char const *path,
	unsigned long mine)
{
	char folder[PATH_TEXT];
	char *last;

	if (game.used_said)
		return;
	game.used_said = 1;
	snprintf(folder, sizeof(folder), "%s", path);
	last = strrchr(folder, '\\');
	if (last)
	{
		last[1] = 0;
		last_use_write(folder);
	}
	error(_error_silent, "map sharing: %s is %08lx here, %08lx on the host: this game plays the host's copy %s", game.map,
		mine, game.version, path);
}

int ae_pack_versions_join(
	char const *map_name,
	unsigned long version,
	unsigned long mine,
	char *message,
	long size)
{
	char path[PATH_TEXT], file[LEVEL_TEXT], pack[MAP_PACK_ID_LENGTH + 1];
	size_t length;

	if (!version || !mine || mine == version || !pack_map(map_name, file, sizeof(file), pack, sizeof(pack)))
		return 0;
	ae_pack_versions_note(map_name, version);
	if (game_copy(path, sizeof(path)))
	{
		copy_used(path, mine);
		return 1;
	}
	if (config_boolean("network.map_download") && ae_map_share_played_pack_version(map_name))
	{
		ae_pack_versions_path(path, sizeof(path), pack, file, version);
		game.fetching_since = system_milliseconds() | 1;
		error(_error_silent, "map sharing: %s is %08lx here, %08lx on the host: the host's copy is fetched to %s, "
			"maps_%s\\%s.map left as it is", map_name, mine, version, path, pack, file);
		return 1;
	}
	error(_error_silent, "map sharing: %s is %08lx here, %08lx on the host: the host's copy is not fetched (%s)", map_name,
		mine, version, config_boolean("network.map_download") ? "the host does not share it" :
		"network.map_download is off");
	if (message && size > 0 && (length = strlen(message)) + 2 < (size_t)size)
	{
		snprintf(message + length, (size_t)size - length, "\r\n%s", config_boolean("network.map_download") ?
			"The host does not share its maps." : "Or turn on DOWNLOAD MAPS (SETTINGS > ONLINE) to get the host's copy.");
	}
	return 0;
}

int ae_pack_versions_fetching(
	char const *map_name)
{
	char copy[PATH_TEXT];

	/* (a fetch for this join of the game's map, its copy not here yet; at most FETCH_WAIT_MS, a download that went
	wrong ends the join on its own) */
	if (!game.fetching_since || !same_map(game.map, map_name) || !global_network_game_client_get() ||
		global_network_game_server_get() || system_milliseconds() - game.fetching_since > FETCH_WAIT_MS)
	{
		return 0;
	}
	if (game_copy(copy, sizeof(copy)))
	{
		game.fetching_since = 0;
		return 0;
	}
	return 1;
}

int ae_pack_versions_xbox_path(
	char const *map_name,
	char *path,
	long size)
{
	char copy[PATH_TEXT];
	unsigned long mine;

	/* (a joined guest of the game noted, its own file there and another version) */
	if (!path || !path[0] || !game.version || !same_map(game.map, map_name) || !global_network_game_client_get() ||
		global_network_game_server_get())
	{
		return 0;
	}
	mine = ae_pack_versions_fingerprint(path);
	if (!mine || mine == game.version || !game_copy(copy, sizeof(copy)) || (long)strlen(copy) >= size)
		return 0;
	copy_used(copy, mine);
	snprintf(path, (size_t)size, "%s", copy);
	return 1;
}

int ae_pack_versions_download_path(
	char const *level_name,
	unsigned long fingerprint,
	char *path,
	long size)
{
	char file[LEVEL_TEXT], pack[MAP_PACK_ID_LENGTH + 1], folder[PATH_TEXT];
	char *slash;

	if (!same_map(game.map, level_name) || !fingerprint || !pack_map(level_name, file, sizeof(file), pack, sizeof(pack)) ||
		!ae_pack_versions_folder(folder, sizeof(folder), pack, fingerprint) ||
		!ae_pack_versions_path(path, (int)size, pack, file, fingerprint))
	{
		return 0;
	}
	if (fingerprint != game.version)
	{
		error(_error_silent, "map sharing: the host's copy of %s is %08lx, though the host said %08lx: kept by its own "
			"version", level_name, fingerprint, game.version);
	}
	/* (maps_shared\<pack>\, then its version's folder) */
	slash = strrchr(folder, '\\');
	if (slash)
	{
		*slash = 0;
		slash = strrchr(folder, '\\');
		if (slash)
		{
			*slash = 0;
			CreateDirectoryA(folder, NULL);
			*slash = '\\';
		}
		CreateDirectoryA(folder, NULL);
	}
	return 1;
}

void ae_pack_versions_arrived(
	char const *level_name,
	unsigned long fingerprint)
{
	char file[LEVEL_TEXT], pack[MAP_PACK_ID_LENGTH + 1], pattern[PATH_TEXT];
	char folders[AE_PACK_VERSIONS_LISTED][PATH_TEXT];
	unsigned long versions[AE_PACK_VERSIONS_LISTED];
	unsigned long long last_used[AE_PACK_VERSIONS_LISTED];
	int remove[AE_PACK_VERSIONS_LISTED];
	int count = 0, removed, index;
	WIN32_FIND_DATAA data;
	HANDLE find;

	total.counted = 0;
	if (!pack_map(level_name, file, sizeof(file), pack, sizeof(pack)))
		return;
	error(_error_silent, "map sharing: the host's copy of %s (%08lx) is in maps_shared\\%s\\%08lx\\", level_name,
		fingerprint, pack, fingerprint);
	if (snprintf(pattern, sizeof(pattern), "%s%s\\*", AE_PACK_VERSIONS_ROOT, pack) >= (int)sizeof(pattern))
		return;
	find = FindFirstFileA(pattern, &data);
	if (find == INVALID_HANDLE_VALUE)
		return;
	do
	{
		unsigned long version;
		char path[PATH_TEXT];

		if (count == AE_PACK_VERSIONS_LISTED || !(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
			!ae_pack_versions_parse_folder(data.cFileName, &version) ||
			!ae_pack_versions_folder(folders[count], PATH_TEXT, pack, version) ||
			!ae_pack_versions_path(path, sizeof(path), pack, file, version) || !file_here(path))
		{
			continue;
		}
		versions[count] = version;
		last_used[count] = last_use_read(folders[count]);
		/* (the one just come: used now) */
		if (version == fingerprint)
			last_used[count] = (unsigned long long)time(NULL);
		count++;
	}
	while (FindNextFileA(find, &data));
	CloseHandle(find);
	removed = ae_pack_versions_evict(count, versions, last_used, game.version, AE_PACK_VERSIONS_KEPT, remove);
	for (index = 0; index < removed; index++)
	{
		char path[PATH_TEXT], side[PATH_TEXT];
		int which = remove[index];

		/* (never the copy just come or the game's: ae_pack_versions_evict keeps the game's) */
		if (versions[which] == fingerprint)
			continue;
		ae_pack_versions_path(path, sizeof(path), pack, file, versions[which]);
		last_use_path(folders[which], side, sizeof(side));
		if (DeleteFileA(path))
		{
			DeleteFileA(side);
			folders[which][strlen(folders[which]) - 1] = 0;
			RemoveDirectoryA(folders[which]);
			error(_error_silent, "map sharing: removed the oldest copy of %s, %s (%08lx), %d of the map are kept",
				level_name, path, versions[which], (int)AE_PACK_VERSIONS_KEPT);
		}
		else
			error(_error_silent, "map sharing: could not remove the old copy %s (error %lu)", path,
				(unsigned long)GetLastError());
	}
}

unsigned long long ae_pack_versions_total_bytes(
	void)
{
	WIN32_FIND_DATAA packs;
	HANDLE find_packs;

	if (total.counted)
		return total.bytes;
	total.counted = 1;
	total.bytes = 0;
	find_packs = FindFirstFileA(AE_PACK_VERSIONS_ROOT "*", &packs);
	if (find_packs == INVALID_HANDLE_VALUE)
		return 0;
	do
	{
		char pattern[PATH_TEXT];
		WIN32_FIND_DATAA versions;
		HANDLE find_versions;

		if (!(packs.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || packs.cFileName[0] == '.' ||
			snprintf(pattern, sizeof(pattern), "%s%s\\*", AE_PACK_VERSIONS_ROOT, packs.cFileName) >= (int)sizeof(pattern))
		{
			continue;
		}
		find_versions = FindFirstFileA(pattern, &versions);
		if (find_versions == INVALID_HANDLE_VALUE)
			continue;
		do
		{
			char maps[PATH_TEXT];
			WIN32_FIND_DATAA map;
			HANDLE find_map;

			if (!(versions.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
				!ae_pack_versions_parse_folder(versions.cFileName, NULL) ||
				snprintf(maps, sizeof(maps), "%s%s\\%s\\*.map", AE_PACK_VERSIONS_ROOT, packs.cFileName,
					versions.cFileName) >= (int)sizeof(maps))
			{
				continue;
			}
			find_map = FindFirstFileA(maps, &map);
			if (find_map == INVALID_HANDLE_VALUE)
				continue;
			do
			{
				if (!(map.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
					total.bytes += ((unsigned long long)map.nFileSizeHigh << 32) | map.nFileSizeLow;
			}
			while (FindNextFileA(find_map, &map));
			CloseHandle(find_map);
		}
		while (FindNextFileA(find_versions, &versions));
		CloseHandle(find_versions);
	}
	while (FindNextFileA(find_packs, &packs));
	CloseHandle(find_packs);
	return total.bytes;
}
