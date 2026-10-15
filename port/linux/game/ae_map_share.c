/*
AE_MAP_SHARE.C

Arena Evolved: OpenCE's map sharing on Arena Evolved's (ChupathingyCE's)
Custom Edition maps (ae_map_share.h).
*/

#include "cseries.h"
#include "cseries/cseries_windows.h"
#include "cseries/errors.h"
#include "cache/cache_files.h"
#include "halo_map_families.h"
#include "map_share.h"
#include "ae_map_share.h"
#include "ae_map_share_rules.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xtl.h>

/* (the C library's allocator: a tag index read and freed here) */
#undef malloc
#undef free

/* ---------- constants */

enum
{
	/* a Custom Edition cache's header: its tag data's offset and size */
	CE_HEADER_TAG_DATA_OFFSET = 0x10,
	CE_HEADER_TAG_DATA_SIZE = 0x14,
	CE_HEADER_BYTES = 0x18,
	/* its tag index (OpenCE's cache_file_formats.c): the instances'
	address, their count, the signature */
	CE_TAG_INDEX_BYTES = 0x28,
	CE_TAG_INDEX_INSTANCES = 0x00,
	CE_TAG_INDEX_COUNT = 0x0C,
	CE_TAG_INDEX_SIGNATURE = 0x24,
	/* each instance: its group, and whether it is held in a resource map */
	CE_TAG_INSTANCE_BYTES = 0x20,
	CE_TAG_INSTANCE_GROUP = 0x00,
	CE_TAG_INSTANCE_IN_RESOURCE_MAP = 0x18,
	CE_MAXIMUM_TAGS = 0x10000,
	/* the level names map_share_level_file hands out at once */
	LEVEL_FILE_BUFFERS = 4,
	LEVEL_FILE_BYTES = 64,
	/* a message's level name (map_share.c's LEVEL_NAME_BYTES) */
	WIRE_LEVEL_NAME_BYTES = 64,
};

/* where a Custom Edition map's tag data is linked (platform.h's
PLATFORM_CE_TAG_CACHE_BASE) */
#define CE_TAG_CACHE_ADDRESS 0x40440000UL

/* the folders whose maps are named for their hashes and shared */
static char const *const shared_folders[] =
{
	"d:\\maps_ce\\",
	"d:\\custom_maps\\",
};

static char const *const resource_names[NUMBER_OF_RESOURCE_MAP_TYPES] =
{
	"", "bitmaps", "sounds", "loc",
};

/* ---------- private code */

static unsigned long little_long(
	byte const *bytes)
{
	return (unsigned long)bytes[0] | (unsigned long)bytes[1] << 8 | (unsigned long)bytes[2] << 16 |
		(unsigned long)bytes[3] << 24;
}

static boolean file_exists(
	char const *path)
{
	HANDLE file = CreateFileA(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (file == INVALID_HANDLE_VALUE)
		return FALSE;
	CloseHandle(file);
	return TRUE;
}

/* ---------- public code (OpenCE's names) */

char const *resource_map_type_describe(
	enum resource_map_type type)
{
	return type > _resource_map_none && type < NUMBER_OF_RESOURCE_MAP_TYPES ? resource_names[type] : "";
}

boolean custom_edition_level_name(
	char const *level_name)
{
	/* (and a community map in the Xbox's format that is shared: ae_map_share_all.c) */
	return level_name && (map_family_parse(level_name, NULL, 0) == _map_family_custom_edition ||
		ae_map_share_xbox_level(level_name));
}

#ifdef HALO_CUSTOM_EDITION

boolean custom_edition_map_file_path(
	char const *level_name,
	char *path)
{
	char file[LEVEL_FILE_BYTES];

	if (ae_map_share_xbox_level(level_name))
		return ae_map_share_xbox_file_path(level_name, path);
	if (!custom_edition_level_name(level_name))
		return FALSE;
	map_family_parse(level_name, file, sizeof(file));
	return map_family_find(_map_family_custom_edition, file, path, MAP_SHARE_PATH_SIZE);
}

char const *ae_map_share_download_name(
	char const *level_name,
	char const *name,
	char const *digits,
	char *base,
	long size)
{
	size_t length = name ? strlen(name) : 0;

	if (!name || !level_name || ae_map_share_xbox_level(level_name) || !digits || !map_share_name_hashed(name) || length <= MAP_SHARE_NAME_DIGITS + 1 ||
		strcmp(name + length - MAP_SHARE_NAME_DIGITS, digits) || length - MAP_SHARE_NAME_DIGITS - 1 >= (size_t)size)
	{
		return name;
	}
	snprintf(base, (size_t)size, "%.*s", (int)(length - MAP_SHARE_NAME_DIGITS - 1), name);
	return base;
}

boolean custom_edition_map_file_named(
	char const *level_name,
	char *path)
{
	return custom_edition_map_find_version(level_name, 0, path);
}

boolean custom_edition_map_find_version(
	char const *level_name,
	unsigned long version,
	char *path)
{
	char file[LEVEL_FILE_BYTES];

	if (ae_map_share_xbox_level(level_name))
		return ae_map_share_xbox_file_path(level_name, path);
	if (!custom_edition_level_name(level_name))
		return FALSE;
	map_family_parse(level_name, file, sizeof(file));
	return map_family_find_version(_map_family_custom_edition, file, version, path, MAP_SHARE_PATH_SIZE);
}

boolean custom_edition_resource_map_file_path(
	short type,
	char *path)
{
	if (type <= _resource_map_none || type >= NUMBER_OF_RESOURCE_MAP_TYPES)
		return FALSE;
	/* (the game's own order: maps_ce, maps\ce, custom_maps, then MCC's
	folder read in place, game.mcc_use: what the game would load is what
	is shared) */
	return map_family_resource(resource_names[type], path, MAP_SHARE_PATH_SIZE);
}

unsigned long custom_edition_map_resource_maps_needed(
	char const *level_name)
{
	char path[MAP_SHARE_PATH_SIZE];
	byte header[CE_HEADER_BYTES];
	byte index[CE_TAG_INDEX_BYTES];
	byte *instances = NULL;
	unsigned long types = 0;
	unsigned long tag_data_offset, tag_data_size, address, count, tag;
	FILE *stream;

	/* (an Xbox map needs none) */
	if (ae_map_share_xbox_level(level_name) || !custom_edition_map_file_path(level_name, path) ||
		!(stream = fopen(path, "rb")))
	{
		return 0;
	}
	if (fread(header, 1, sizeof(header), stream) != sizeof(header))
		goto done;
	tag_data_offset = little_long(header + CE_HEADER_TAG_DATA_OFFSET);
	tag_data_size = little_long(header + CE_HEADER_TAG_DATA_SIZE);
	if (tag_data_size < CE_TAG_INDEX_BYTES || fseek(stream, (long)tag_data_offset, SEEK_SET) != 0 ||
		fread(index, 1, sizeof(index), stream) != sizeof(index) ||
		little_long(index + CE_TAG_INDEX_SIGNATURE) != 'tags')
	{
		goto done;
	}
	address = little_long(index + CE_TAG_INDEX_INSTANCES);
	count = little_long(index + CE_TAG_INDEX_COUNT);
	if (!count || count > CE_MAXIMUM_TAGS || address < CE_TAG_CACHE_ADDRESS ||
		address - CE_TAG_CACHE_ADDRESS > tag_data_size ||
		count * CE_TAG_INSTANCE_BYTES > tag_data_size - (address - CE_TAG_CACHE_ADDRESS))
	{
		goto done;
	}
	instances = malloc(count * CE_TAG_INSTANCE_BYTES);
	if (!instances || fseek(stream, (long)(tag_data_offset + address - CE_TAG_CACHE_ADDRESS), SEEK_SET) != 0 ||
		fread(instances, CE_TAG_INSTANCE_BYTES, count, stream) != count)
	{
		goto done;
	}
	for (tag = 0; tag < count; tag++)
	{
		byte const *instance = instances + tag * CE_TAG_INSTANCE_BYTES;

		if (!little_long(instance + CE_TAG_INSTANCE_IN_RESOURCE_MAP))
			continue;
		switch (little_long(instance + CE_TAG_INSTANCE_GROUP))
		{
		case 'bitm':
			types |= 1UL << _resource_map_bitmaps;
			break;
		case 'snd!':
			types |= 1UL << _resource_map_sounds;
			break;
		case 'font':
		case 'ustr':
		case 'hmt ':
			types |= 1UL << _resource_map_locale;
			break;
		}
	}
done:
	free(instances);
	fclose(stream);
	return types;
}

unsigned long custom_edition_map_checksum(
	char const *level_name)
{
	return custom_edition_level_name(level_name) && !ae_map_share_xbox_level(level_name) ?
		cache_files_map_version(level_name) : 0;
}

char const *custom_edition_cache_loaded_path(
	void)
{
	return cache_files_ce_map_loaded_path();
}

boolean map_share_release_map(
	char const *path)
{
	return cache_files_ce_map_release(path);
}

boolean custom_edition_cache_present(
	char const *level_name,
	unsigned long checksum,
	char *message,
	long message_size)
{
	char path[MAP_SHARE_PATH_SIZE];
	char const *file = map_share_level_file(level_name);
	unsigned long needed;
	short type;

	if (ae_map_share_xbox_level(level_name))
		return ae_map_share_xbox_present(level_name, message, message_size);
	if (!halo_custom_edition_tag_cache())
	{
		snprintf(message, (size_t)message_size, "Custom Edition maps cannot be played on this computer.");
		return FALSE;
	}
	if (!custom_edition_map_file_path(level_name, path))
	{
		snprintf(message, (size_t)message_size, "You don't have the Custom Edition map %.63s.", file);
		return FALSE;
	}
	if (checksum && custom_edition_map_checksum(level_name) != checksum)
	{
		snprintf(message, (size_t)message_size, "Your Custom Edition map %.63s is another version than the host's.",
			file);
		return FALSE;
	}
	needed = custom_edition_map_resource_maps_needed(level_name);
	for (type = _resource_map_bitmaps; type < NUMBER_OF_RESOURCE_MAP_TYPES; type++)
	{
		if ((needed & (1UL << type)) && !custom_edition_resource_map_file_path(type, path))
		{
			snprintf(message, (size_t)message_size,
				"The Custom Edition map %.63s needs Custom Edition's %s.map, which is not in maps_ce.", file,
				resource_names[type]);
			return FALSE;
		}
	}
	return TRUE;
}

void custom_edition_maps_look_again(
	void)
{
	/* (Arena Evolved's map lists read the folders each time they are
	shown: ui_map_list_refresh) */
}

boolean halo_custom_edition_tag_cache(
	void)
{
	extern int platform_ce_tag_cache_ready;

	return platform_ce_tag_cache_ready != 0;
}

boolean map_share_resource_maps_missing(
	char const *level_name)
{
	char path[MAP_SHARE_PATH_SIZE];
	unsigned long needed;
	short type;

	if (!custom_edition_map_file_path(level_name, path))
		return FALSE;
	needed = custom_edition_map_resource_maps_needed(level_name);
	for (type = _resource_map_bitmaps; type < NUMBER_OF_RESOURCE_MAP_TYPES; type++)
	{
		if ((needed & (1UL << type)) && !custom_edition_resource_map_file_path(type, path))
			return TRUE;
	}
	return FALSE;
}

void map_share_folder_make(
	void)
{
	/* (not before the player's yes: nothing is made for an offer nobody agreed to) */
	if (ae_map_share_download_may_write())
	{
		CreateDirectoryA("d:\\maps_ce\\", NULL);
		CreateDirectoryA(ae_map_share_xbox_folder(), NULL);
	}
}

#else

boolean custom_edition_map_file_path(char const *level_name, char *path) { (void)level_name; (void)path; return FALSE; }
boolean custom_edition_map_file_named(char const *level_name, char *path) { (void)level_name; (void)path; return FALSE; }
char const *ae_map_share_download_name(char const *level_name, char const *name, char const *digits, char *base,
	long size)
{
	(void)level_name;
	(void)digits;
	(void)base;
	(void)size;
	return name;
}
boolean custom_edition_map_find_version(char const *level_name, unsigned long version, char *path)
{
	(void)level_name;
	(void)version;
	(void)path;
	return FALSE;
}
boolean custom_edition_resource_map_file_path(short type, char *path) { (void)type; (void)path; return FALSE; }
unsigned long custom_edition_map_resource_maps_needed(char const *level_name) { (void)level_name; return 0; }
unsigned long custom_edition_map_checksum(char const *level_name) { (void)level_name; return 0; }
char const *custom_edition_cache_loaded_path(void) { return NULL; }
boolean custom_edition_cache_present(char const *level_name, unsigned long checksum, char *message, long message_size)
{
	(void)level_name;
	(void)checksum;
	snprintf(message, (size_t)message_size, "Custom Edition maps cannot be played on this computer.");
	return FALSE;
}
void custom_edition_maps_look_again(void) {}
boolean map_share_release_map(char const *path) { (void)path; return TRUE; }
boolean halo_custom_edition_tag_cache(void) { return FALSE; }
boolean map_share_resource_maps_missing(char const *level_name) { (void)level_name; return FALSE; }
void map_share_folder_make(void) {}

#endif

/* ---------- map sharing's safety rules (DamnationCE v0.3.41, CC0) */

boolean ae_map_share_source_allowed(
	boolean internet,
	unsigned long address,
	long download)
{
	/* (each address refused said once per download: the host sends its
	sources every few seconds) */
	static unsigned long logged[16];
	static long logged_count;
	static long logged_download;
	long index;

	if (internet || ae_map_share_address_local(address))
		return TRUE;
	if (download != logged_download)
	{
		logged_download = download;
		logged_count = 0;
	}
	for (index = 0; index < logged_count && logged[index] != address; index++)
		;
	if (index == logged_count && logged_count < (long)NUMBEROF(logged))
	{
		logged[logged_count++] = address;
		error(_error_silent, "map sharing: the host named a LAN source at %lu.%lu.%lu.%lu, not a local address: "
			"not reached (the map comes from the host and the local players)",
			(address >> 24) & 0xFF, (address >> 16) & 0xFF, (address >> 8) & 0xFF, address & 0xFF);
	}
	return FALSE;
}

static int discard_delete(
	char const *path,
	void *context)
{
	(void)context;
	/* (the game may hold it open from the check: let go first) */
	map_share_release_map(path);
	return DeleteFileA(path) != 0;
}

void ae_map_share_discard_failed(
	char const *path)
{
	char const *paths[1];
	/* (the downloads' folders: maps_ce, or an Xbox map's maps_shared) */
	char const *folder = ae_map_share_path_in_folder(path, ae_map_share_xbox_folder()) ? ae_map_share_xbox_folder() :
		"d:\\maps_ce\\";

	paths[0] = path;
	if (ae_map_share_discard(paths, 1, folder, discard_delete, NULL))
		error(_error_silent, "map sharing: removed %s, which failed its check", path);
	else if (!ae_map_share_path_in_folder(path, folder))
		error(_error_silent, "map sharing: did not remove %s: not a file of %s", path ? path : "", folder);
	else
		error(_error_silent, "map sharing: could not remove %s, which failed its check (error %lu)", path,
			(unsigned long)GetLastError());
}

char const *map_share_level_file(
	char const *level_name)
{
	static char buffers[LEVEL_FILE_BUFFERS][LEVEL_FILE_BYTES];
	static long next;
	char *buffer = buffers[next++ % LEVEL_FILE_BUFFERS];

	buffer[0] = 0;
	/* (an Xbox map's with its hash, as a Custom Edition map's file is named: map sharing checks the name's) */
	if (level_name && ae_map_share_xbox_level(level_name) &&
		ae_map_share_xbox_level_file(level_name, buffer, LEVEL_FILE_BYTES))
	{
		return buffer;
	}
	if (level_name)
		map_family_parse(level_name, buffer, LEVEL_FILE_BYTES);
	return buffer;
}

void map_share_level_to_wire(
	char *wire,
	char const *level_name)
{
	csmemset(wire, 0, WIRE_LEVEL_NAME_BYTES);
	map_family_wire_name(level_name, wire, WIRE_LEVEL_NAME_BYTES);
	wire[WIRE_LEVEL_NAME_BYTES - 1] = 0;
}

void map_share_level_from_wire(
	char *level_name)
{
	char name[WIRE_LEVEL_NAME_BYTES];

	map_family_from_wire_name(level_name, name, sizeof(name));
	csmemset(level_name, 0, WIRE_LEVEL_NAME_BYTES);
	csstrncpy(level_name, name, WIRE_LEVEL_NAME_BYTES - 1);
}

boolean map_share_folder_shared(
	char const *folder)
{
	long index;

	for (index = 0; index < (long)NUMBEROF(shared_folders); index++)
	{
		if (folder && !_stricmp(folder, shared_folders[index]))
			return TRUE;
	}
	return FALSE;
}
