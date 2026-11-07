/*
AE_PRIVATE_FILES.C

AE's small text files in the save root's private folder (ae_private_files.h).
*/

#include "ae_private_files.h"
#include "port_folders.h"
#include "posix.h"

#include <stdio.h>
#include <string.h>

const char *platform_folder(enum port_folder folder);

/* a file's path in the private folder, with suffix after its name: 0 when it does not fit or the name is no bare
name */
static int file_path(const char *name, const char *suffix, char *path, size_t size)
{
	const char *folder = platform_folder(_port_folder_private);
	int length;

	if (!folder || !*folder || !name || !*name || strchr(name, '/') || strchr(name, '\\') || strstr(name, ".."))
		return 0;
	length = snprintf(path, size, "%s/%s%s", folder, name, suffix);
	return length > 0 && (size_t)length < size;
}

long ae_private_file_read(const char *name, char *text, long size)
{
	char path[1024];
	FILE *file;
	size_t count;

	if (!text || size <= 0)
		return -1;
	text[0] = 0;
	if (!file_path(name, "", path, sizeof(path)) || !(file = fopen(path, "rb")))
		return -1;
	count = fread(text, 1, (size_t)size, file);
	fclose(file);
	if (count >= (size_t)size)
	{
		text[0] = 0;
		return -2;
	}
	text[count] = 0;
	return (long)count;
}

long ae_save_root_file_read(const char *name, char *text, long size)
{
	char path[1024];
	const char *folder = platform_folder(_port_folder_private);
	size_t length;
	FILE *file;
	size_t count;

	if (!text || size <= 0 || !name || !*name || strchr(name, '/') || strchr(name, '\\') || strstr(name, "..") ||
		!folder || !*folder)
	{
		return -1;
	}
	text[0] = 0;
	length = strlen(folder);
	/* (the save root: the private folder's parent) */
	if (length > 8 && !strcmp(folder + length - 8, "/private") && length - 8 + strlen(name) + 2 < sizeof(path))
	{
		snprintf(path, sizeof(path), "%.*s/%s", (int)(length - 8), folder, name);
		if ((file = fopen(path, "rb")))
		{
			count = fread(text, 1, (size_t)size, file);
			fclose(file);
			if (count >= (size_t)size)
			{
				text[0] = 0;
				return -2;
			}
			text[count] = 0;
			return (long)count;
		}
	}
	return ae_private_file_read(name, text, size);
}

int ae_private_file_write(const char *name, const char *text, long length)
{
	char path[1024], temporary[1024];
	FILE *file;

	if (!text || length < 0 || !file_path(name, "", path, sizeof(path)) ||
		!file_path(name, ".new", temporary, sizeof(temporary)))
		return 0;
	posix_make_directory(platform_folder(_port_folder_private));
	if (!(file = fopen(temporary, "wb")))
		return 0;
	if (fwrite(text, 1, (size_t)length, file) != (size_t)length || fflush(file))
	{
		fclose(file);
		remove(temporary);
		return 0;
	}
	if (fclose(file) || posix_replace_file(temporary, path))
	{
		remove(temporary);
		return 0;
	}
	return 1;
}

int ae_private_file_set_aside(const char *name)
{
	char path[1024], bad[1024];

	if (!file_path(name, "", path, sizeof(path)) || !file_path(name, ".bad", bad, sizeof(bad)))
		return 0;
	return posix_replace_file(path, bad) == 0;
}
