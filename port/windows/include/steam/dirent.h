/*
DIRENT.H

opendir, readdir and closedir on the Windows C runtime's _findfirst, for the Steam client's package scans
(port/android/host/host_mcc_hash.c): the entry's name only (d_name), "." and ".." included as POSIX gives them.
*/

#ifndef __HALO_WINDOWS_STEAM_DIRENT_H
#define __HALO_WINDOWS_STEAM_DIRENT_H

#include <io.h>
#include <stdlib.h>
#include <string.h>

struct dirent
{
	char d_name[260];
};

typedef struct
{
	intptr_t handle;
	int started;
	struct _finddata_t data;
	struct dirent entry;
} DIR;

static inline DIR *opendir(char const *path)
{
	char pattern[1024];
	DIR *directory;

	if (strlen(path) + 3 > sizeof(pattern))
		return NULL;
	directory = (DIR *)calloc(1, sizeof(*directory));
	if (!directory)
		return NULL;
	strcpy(pattern, path);
	strcat(pattern, "\\*");
	directory->handle = _findfirst(pattern, &directory->data);
	if (directory->handle == -1)
	{
		free(directory);
		return NULL;
	}
	return directory;
}

static inline struct dirent *readdir(DIR *directory)
{
	if (directory->started)
	{
		if (_findnext(directory->handle, &directory->data) != 0)
			return NULL;
	}
	directory->started = 1;
	strncpy(directory->entry.d_name, directory->data.name, sizeof(directory->entry.d_name) - 1);
	directory->entry.d_name[sizeof(directory->entry.d_name) - 1] = 0;
	return &directory->entry;
}

static inline int closedir(DIR *directory)
{
	_findclose(directory->handle);
	free(directory);
	return 0;
}

#endif
