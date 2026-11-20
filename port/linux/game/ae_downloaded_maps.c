/* The downloaded maps' list read into the table (see the header). */
#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "cseries/cseries_windows.h"
#include "ae_downloaded_maps.h"
#include "ae_downloaded_maps_rules.h"
#include "ae_map_share.h"
#include "halo_map_families.h"

void platform_log(char const *format, ...);

/* (share read and write: a download thread may be appending) */
#define SHARE_READ_WRITE 3

static unsigned long last_size;
static int overflow_logged;

void ae_downloaded_maps_mark(char const *map_name)
{
	if (!ae_downloaded_maps_add(map_name) && !overflow_logged)
	{
		overflow_logged = 1;
		platform_log("downloaded maps: more than %d names: every map is played by the downloaded maps' rules", (int)AE_DOWNLOADED_MAPS_MAXIMUM);
	}
}

void ae_downloaded_maps_read_file(char const *path)
{
	char piece[1024];
	struct ae_downloaded_maps_reader reader;
	unsigned long bytes_read = 0;
	HANDLE file = CreateFileA(path, GENERIC_READ, SHARE_READ_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (file == INVALID_HANDLE_VALUE)
		return;
	ae_downloaded_maps_reader_start(&reader);
	last_size = 0;
	while (ReadFile(file, piece, sizeof(piece), &bytes_read, NULL) && bytes_read)
	{
		last_size += bytes_read;
		ae_downloaded_maps_reader_feed(&reader, piece, (long)bytes_read);
	}
	ae_downloaded_maps_reader_end(&reader);
	CloseHandle(file);
	if (ae_downloaded_maps_overflowed() && !overflow_logged)
	{
		overflow_logged = 1;
		platform_log("downloaded maps: more than %d names: every map is played by the downloaded maps' rules", (int)AE_DOWNLOADED_MAPS_MAXIMUM);
	}
}

int ae_downloaded_maps_script_check(char const *requested)
{
	char name[AE_DOWNLOADED_MAPS_NAME];
	char const *opened = custom_edition_cache_loaded_path();

	if (map_is_downloaded(requested))
		return 1;
	if (!opened)
		return 0;
	/* (a Custom Edition map is loaded: when no name can be made of its file, it is played by the strict rules) */
	return !ae_downloaded_maps_file_name(opened, name, (int)sizeof(name)) || map_is_downloaded(name);
}

int ae_downloaded_maps_known(char const *map_name, char const *path)
{
#ifndef HALO_XBOX_CONSOLE
	if (!ae_downloaded_maps_has(map_name))
	{
		HANDLE file = CreateFileA(path, GENERIC_READ, SHARE_READ_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		unsigned long size;

		if (file == INVALID_HANDLE_VALUE)
			return 0;
		size = GetFileSize(file, NULL);
		CloseHandle(file);
		if (size != last_size && size != 0xffffffffUL)
			ae_downloaded_maps_read_file(path);
	}
#else
	(void)path;
#endif
	return ae_downloaded_maps_has(map_name);
}
