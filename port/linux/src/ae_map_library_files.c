/*
AE_MAP_LIBRARY_FILES.C

The installed maps library's host file calls (ae_map_library_files.h), made through posix.h's helpers (posix_files.c
on Linux, Android and macOS, win32_files.c on Windows), whose structures have the same layout on both sides of the
platform layer's ABI; a real path is the C library's (realpath, or _fullpath on Windows) into a buffer of ours.
*/

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "posix.h"
#include "../game/ae_map_library_rules.h"
#include "ae_map_library_files.h"

#ifdef _WIN32
#define REAL_PATH_SIZE 1024
#else
#define REAL_PATH_SIZE 4096
char *realpath(const char *path, char *resolved);
int link(const char *from, const char *to);
int unlink(const char *path);
#endif

/* a path's real one: on Windows the file's final path (junctions and links on its way resolved, as realpath does),
its \\?\ prefix taken off */
static int resolve(char const *path, char *resolved)
{
#ifdef _WIN32
	char final[REAL_PATH_SIZE];
	HANDLE handle = CreateFileA(path, FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
		OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
	DWORD length;
	char const *start = final;

	if (handle == INVALID_HANDLE_VALUE)
		return 0;
	length = GetFinalPathNameByHandleA(handle, final, sizeof(final), FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
	CloseHandle(handle);
	if (!length || length >= sizeof(final))
		return 0;
	if (!strncmp(final, "\\\\?\\UNC\\", 8))
	{
		snprintf(resolved, REAL_PATH_SIZE, "\\\\%s", final + 8);
		return 1;
	}
	if (!strncmp(final, "\\\\?\\", 4))
		start = final + 4;
	snprintf(resolved, REAL_PATH_SIZE, "%s", start);
	return 1;
#else
	return realpath(path, resolved) != NULL;
#endif
}

/* whether a path is itself a link: on Windows a symbolic link or a junction only (a cloud or deduplicated file's
reparse point is a plain file) */
static int is_link(char const *path)
{
#ifdef _WIN32
	WIN32_FIND_DATAA data;
	HANDLE find;
	int linked;

	if (!(GetFileAttributesA(path) & FILE_ATTRIBUTE_REPARSE_POINT) || GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES)
		return 0;
	find = FindFirstFileA(path, &data);
	if (find == INVALID_HANDLE_VALUE)
		return 1;
	FindClose(find);
	linked = data.dwReserved0 == IO_REPARSE_TAG_SYMLINK || data.dwReserved0 == IO_REPARSE_TAG_MOUNT_POINT;
	return linked;
#else
	return posix_is_link(path);
#endif
}

int ae_map_files_is_link(char const *path)
{
	return path && path[0] && is_link(path);
}

int ae_map_files_real_path(char const *path, char *real, int size)
{
	char resolved[REAL_PATH_SIZE];

	if (!path || !path[0] || !resolve(path, resolved))
		return 0;
	if (size <= 0 || strlen(resolved) >= (size_t)size)
		return 0;
	memcpy(real, resolved, strlen(resolved) + 1);
	return 1;
}

/* whether a real path is under a real root (at a separator: maps_ce2 is not under maps_ce) */
static int inside(char const *path, char const *root)
{
	size_t length = strlen(root);

	while (length > 1 && (root[length - 1] == '/' || root[length - 1] == '\\'))
		length--;
#ifdef _WIN32
	if (!length || _strnicmp(path, root, length))
		return 0;
	return path[length] == '\\' || path[length] == '/';
#else
	if (!length || strncmp(path, root, length))
		return 0;
	return path[length] == '/' || (length == 1 && root[0] == '/');
#endif
}

int ae_map_files_path_check(char const *path, char const *const *real_roots, int root_count, char *real, int size)
{
	struct posix_file_information information;
	char resolved[REAL_PATH_SIZE];
	int index;

	if (!path || !path[0])
		return AE_MAP_REFUSED_UNKNOWN_MAP;
	/* (the file itself not a link; the folders on its way resolved below, so a linked folder that leads out of the
	roots is caught there) */
	if (is_link(path))
		return AE_MAP_REFUSED_LINK;
	if (posix_stat(path, &information))
		return AE_MAP_REFUSED_UNKNOWN_MAP;
	if (information.flags & _posix_file_is_directory)
		return AE_MAP_REFUSED_NOT_A_FILE;
	if (!resolve(path, resolved))
		return AE_MAP_REFUSED_UNKNOWN_MAP;
	if (size <= 0 || strlen(resolved) >= (size_t)size)
		return AE_MAP_REFUSED_PATH_TOO_LONG;
	for (index = 0; real_roots && index < root_count; index++)
	{
		/* (a file is never a root itself: it is under one) */
		if (real_roots[index] && inside(resolved, real_roots[index]) && strlen(resolved) > strlen(real_roots[index]))
		{
			memcpy(real, resolved, strlen(resolved) + 1);
			return AE_MAP_CHANGE_ALLOWED;
		}
	}
	return AE_MAP_REFUSED_OUTSIDE_ROOTS;
}

int ae_map_files_is_file(char const *path)
{
	struct posix_file_information information;

	return path && path[0] && !posix_stat(path, &information) && !(information.flags & _posix_file_is_directory);
}

int ae_map_files_exists(char const *path)
{
	struct posix_file_information information;

	return path && path[0] && (is_link(path) || !posix_stat(path, &information));
}

int ae_map_files_size(char const *path, unsigned long long *size)
{
	struct posix_file_information information;

	if (!path || !path[0] || posix_stat(path, &information) || (information.flags & _posix_file_is_directory))
		return 0;
	*size = (unsigned long long)information.size_high << 32 | (unsigned long long)information.size_low;
	return 1;
}

int ae_map_files_read(char const *path, unsigned char *bytes, int size)
{
	FILE *file;
	size_t read;

	if (!path || size <= 0)
		return -1;
	file = fopen(path, "rb");
	if (!file)
		return -1;
	read = fread(bytes, 1, (size_t)size, file);
	fclose(file);
	return (int)read;
}

int ae_map_files_delete(char const *path)
{
	return path && path[0] && !remove(path);
}

int ae_map_files_rename_new(char const *from, char const *to)
{
	if (!from || !to || ae_map_files_exists(to))
		return 0;
#ifndef _WIN32
	/* (a hard link is made only where nothing is, in one step, so nothing is ever replaced; then the old name goes.
	Across file systems no link is made: posix_rename_directory's look then rename) */
	if (!link(from, to))
	{
		if (!unlink(from))
			return 1;
		unlink(to);
		return 0;
	}
	if (errno == EEXIST)
		return 0;
#endif
	/* (posix_rename_directory moves a file as well, and never onto anything that is there) */
	return !posix_rename_directory(from, to);
}

int ae_map_files_make_folder(char const *path)
{
	struct posix_file_information information;

	if (!path || !path[0])
		return 0;
	if (posix_stat(path, &information))
		posix_make_directory(path);
	return !posix_stat(path, &information) && (information.flags & _posix_file_is_directory);
}

/* ---------- a folder's entries */

struct map_folder
{
	void *directory;
	char path[AE_MAP_LIBRARY_PATH_SIZE];
	int used;
};

static struct map_folder map_folders[2];

void *ae_map_files_folder_open(char const *path)
{
	unsigned int index;

	if (!path || !path[0] || strlen(path) >= sizeof(map_folders[0].path))
		return NULL;
	for (index = 0; index < sizeof(map_folders) / sizeof(map_folders[0]); index++)
	{
		struct map_folder *folder = &map_folders[index];

		if (folder->used)
			continue;
		folder->directory = posix_directory_open(path);
		if (!folder->directory)
			return NULL;
		memcpy(folder->path, path, strlen(path) + 1);
		folder->used = 1;
		return folder;
	}
	return NULL;
}

int ae_map_files_folder_next(void *opened, char *name, int size, int *is_folder, unsigned long long *file_size)
{
	struct map_folder *folder = (struct map_folder *)opened;
	char entry[AE_MAP_LIBRARY_PATH_SIZE];

	if (!folder || !folder->used || size <= 0)
		return 0;
	while (posix_directory_next(folder->directory, entry, sizeof(entry)))
	{
		struct posix_file_information information;
		char path[AE_MAP_LIBRARY_PATH_SIZE];
		size_t length = strlen(folder->path);
		char const *join = strchr(folder->path, '\\') ? "\\" : "/";
		int written;

		if (strlen(entry) >= (size_t)size)
			continue;
		written = snprintf(path, sizeof(path), "%s%s%s", folder->path,
			length && folder->path[length - 1] == join[0] ? "" : join, entry);
		if (written <= 0 || (size_t)written >= sizeof(path) || posix_stat(path, &information))
			continue;
		memcpy(name, entry, strlen(entry) + 1);
		*is_folder = (information.flags & _posix_file_is_directory) != 0;
		*file_size = (unsigned long long)information.size_high << 32 | (unsigned long long)information.size_low;
		return 1;
	}
	return 0;
}

void ae_map_files_folder_close(void *opened)
{
	struct map_folder *folder = (struct map_folder *)opened;

	if (!folder || !folder->used)
		return;
	posix_directory_close(folder->directory);
	folder->directory = NULL;
	folder->used = 0;
}
