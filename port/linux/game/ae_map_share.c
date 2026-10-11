/*
AE_MAP_SHARE.C

Arena Evolved: OpenCE's map sharing on Arena Evolved's (ChupathingyCE's)
Custom Edition maps (ae_map_share.h).
*/

#include "cseries.h"
#include "cseries/cseries_windows.h"
#include "cache/cache_files.h"
#include "halo_map_families.h"
#include "map_share.h"
#include "ae_map_share.h"

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

/* the single <folder><file>_<hash>.map of each shared folder, the first
folder's first: FALSE if none, or more than one in the folder found first
(two versions: the name alone says neither) */
static boolean find_hashed(
	char const *file,
	char *path,
	long size)
{
	long folder;

	for (folder = 0; folder < (long)NUMBEROF(shared_folders); folder++)
	{
		char pattern[MAP_SHARE_PATH_SIZE];
		WIN32_FIND_DATAA data;
		HANDLE find;
		long found = 0;

		if (snprintf(pattern, sizeof(pattern), "%s%s_*.map", shared_folders[folder], file) >= (int)sizeof(pattern))
			continue;
		find = FindFirstFileA(pattern, &data);
		if (find == INVALID_HANDLE_VALUE)
			continue;
		do
		{
			char name[MAP_SHARE_PATH_SIZE];
			size_t length = strlen(data.cFileName);

			if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || length <= 4 || length - 4 >= sizeof(name))
				continue;
			snprintf(name, sizeof(name), "%.*s", (int)(length - 4), data.cFileName);
			if (!map_share_name_hashed(name) || map_share_name_base_length(name) != (short)strlen(file) ||
				_strnicmp(name, file, strlen(file)))
			{
				continue;
			}
			if (!found)
				snprintf(path, (size_t)size, "%s%s", shared_folders[folder], data.cFileName);
			found++;
		}
		while (FindNextFileA(find, &data));
		CloseHandle(find);
		if (found == 1)
			return TRUE;
		if (found > 1)
			break;
	}
	return FALSE;
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
	return level_name && map_family_parse(level_name, NULL, 0) == _map_family_custom_edition;
}

#ifdef HALO_CUSTOM_EDITION

boolean custom_edition_map_file_path(
	char const *level_name,
	char *path)
{
	char file[LEVEL_FILE_BYTES];

	if (!custom_edition_level_name(level_name))
		return FALSE;
	map_family_parse(level_name, file, sizeof(file));
	return map_family_find(_map_family_custom_edition, file, path, MAP_SHARE_PATH_SIZE);
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

	if (!custom_edition_map_file_path(level_name, path) || !(stream = fopen(path, "rb")))
		return 0;
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
	return custom_edition_level_name(level_name) ? cache_files_map_version(level_name) : 0;
}

char const *custom_edition_cache_loaded_path(
	void)
{
	return cache_files_ce_map_loaded_path();
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
	CreateDirectoryA(CUSTOM_EDITION_MAP_DIRECTORY, NULL);
}

boolean ae_map_share_find(
	short family,
	char const *file,
	char *path,
	long size)
{
	char base[LEVEL_FILE_BYTES];
	short base_length;
	long folder;

	if (family != _map_family_custom_edition || !file || !file[0] || strlen(file) >= sizeof(base))
		return FALSE;
	/* (a map named without its hash, renamed since: hugeass, now
	hugeass_1a2b3c4d) */
	if (!map_share_name_hashed(file))
		return find_hashed(file, path, size);
	/* (one named for its hash that this machine has under its own name,
	not named yet: named now, if its hash is that one) */
	base_length = map_share_name_base_length(file);
	snprintf(base, sizeof(base), "%.*s", (int)base_length, file);
	for (folder = 0; folder < (long)NUMBEROF(shared_folders); folder++)
	{
		char unnamed[MAP_SHARE_PATH_SIZE];

		snprintf(unnamed, sizeof(unnamed), "%s%s.map", shared_folders[folder], base);
		if (!file_exists(unnamed) || !map_share_name_now(shared_folders[folder], base))
			continue;
		snprintf(path, (size_t)size, "%s%s.map", shared_folders[folder], file);
		if (file_exists(path))
			return TRUE;
	}
	return FALSE;
}

#else

boolean custom_edition_map_file_path(char const *level_name, char *path) { (void)level_name; (void)path; return FALSE; }
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
boolean halo_custom_edition_tag_cache(void) { return FALSE; }
boolean map_share_resource_maps_missing(char const *level_name) { (void)level_name; return FALSE; }
void map_share_folder_make(void) {}
boolean ae_map_share_find(short family, char const *file, char *path, long size)
{
	(void)family;
	(void)file;
	(void)path;
	(void)size;
	return FALSE;
}

#endif

char const *map_share_level_file(
	char const *level_name)
{
	static char buffers[LEVEL_FILE_BUFFERS][LEVEL_FILE_BYTES];
	static long next;
	char *buffer = buffers[next++ % LEVEL_FILE_BUFFERS];

	buffer[0] = 0;
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
