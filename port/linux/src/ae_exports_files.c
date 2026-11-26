/*
AE_EXPORTS_FILES.C

The exports folder (ae_exports_files.h).
*/

#include "ae_exports_files.h"

#include <stdio.h>
#include <string.h>

#include "platform.h"
#include <SDL3/SDL.h>

/* the Android guest has no SDL file calls: there the exports folder is never there, and every call says so */
#if !defined(HALO_ANDROID)

enum { NAME_MAXIMUM = 80 };

static int name_usable(const char *name)
{
	size_t length = name ? strlen(name) : 0, index;

	if (!length || length > NAME_MAXIMUM || name[0] == '.')
		return 0;
	for (index = 0; index < length; index++)
	{
		char c = name[index];

		if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '_'))
			return 0;
	}
	return 1;
}

static int folder_path(int backup, char *path, size_t size)
{
	const char *root = platform_save_root();
	int length;

	if (!root || !*root)
		return 0;
	length = snprintf(path, size, "%s/exports%s", root, backup ? "/backup" : "");
	return length > 0 && (size_t)length < size;
}

static int file_path(int backup, const char *name, const char *suffix, char *path, size_t size)
{
	char folder[1024];
	int length;

	if (!name_usable(name) || !folder_path(backup, folder, sizeof(folder)))
		return 0;
	length = snprintf(path, size, "%s/%s%s", folder, name, suffix);
	return length > 0 && (size_t)length < size;
}

int ae_exports_folder(int backup, char *path, size_t size)
{
	SDL_PathInfo information;

	if (!folder_path(backup, path, size) || !SDL_CreateDirectory(path))
		return 0;
	return SDL_GetPathInfo(path, &information) && information.type == SDL_PATHTYPE_DIRECTORY;
}

long ae_exports_read(int backup, const char *name, char *text, long size)
{
	char path[1200];
	SDL_PathInfo information;
	void *data;
	size_t length;

	if (!file_path(backup, name, "", path, sizeof(path)) || !SDL_GetPathInfo(path, &information) ||
		information.type != SDL_PATHTYPE_FILE)
		return -1;
	if (information.size >= (Uint64)size)
		return -2;
	data = SDL_LoadFile(path, &length);
	if (!data)
		return -1;
	if ((long)length >= size)
	{
		SDL_free(data);
		return -2;
	}
	memcpy(text, data, length);
	text[length] = 0;
	SDL_free(data);
	return (long)length;
}

int ae_exports_write(int backup, const char *name, const char *text, long length)
{
	char folder[1024], path[1200], temporary[1210];

	if (length < 0 || !ae_exports_folder(backup, folder, sizeof(folder)) || !file_path(backup, name, "", path, sizeof(path)) ||
		!file_path(backup, name, ".new", temporary, sizeof(temporary)))
		return 0;
	if (!SDL_SaveFile(temporary, text, (size_t)length))
	{
		SDL_RemovePath(temporary);
		return 0;
	}
	if (!SDL_RenamePath(temporary, path))
	{
		SDL_RemovePath(temporary);
		return 0;
	}
	return 1;
}

int ae_exports_exists(int backup, const char *name)
{
	char path[1200];

	return file_path(backup, name, "", path, sizeof(path)) && SDL_GetPathInfo(path, NULL);
}

int ae_exports_remove(int backup, const char *name)
{
	char path[1200];

	return file_path(backup, name, "", path, sizeof(path)) && SDL_RemovePath(path);
}

struct listing
{
	const char *suffix;
	void (*visit)(const char *name, void *context);
	void *context;
	int count;
};

static SDL_EnumerationResult SDLCALL list_one(void *userdata, const char *directory, const char *name)
{
	struct listing *listing = userdata;
	size_t length = strlen(name), suffix = strlen(listing->suffix);

	(void)directory;
	if (name_usable(name) && length > suffix && !strcmp(name + length - suffix, listing->suffix))
	{
		listing->visit(name, listing->context);
		listing->count++;
	}
	return SDL_ENUM_CONTINUE;
}

int ae_exports_list(int backup, const char *suffix, void (*visit)(const char *name, void *context), void *context)
{
	char folder[1024];
	struct listing listing;

	if (!ae_exports_folder(backup, folder, sizeof(folder)))
		return -1;
	listing.suffix = suffix;
	listing.visit = visit;
	listing.context = context;
	listing.count = 0;
	return SDL_EnumerateDirectory(folder, list_one, &listing) ? listing.count : -1;
}

#else

int ae_exports_folder(int backup, char *path, size_t size)
{
	(void)backup;
	if (path && size)
		path[0] = 0;
	return 0;
}

long ae_exports_read(int backup, const char *name, char *text, long size)
{
	(void)backup; (void)name; (void)text; (void)size;
	return -1;
}

int ae_exports_write(int backup, const char *name, const char *text, long length)
{
	(void)backup; (void)name; (void)text; (void)length;
	return 0;
}

int ae_exports_exists(int backup, const char *name)
{
	(void)backup; (void)name;
	return 0;
}

int ae_exports_remove(int backup, const char *name)
{
	(void)backup; (void)name;
	return 0;
}

int ae_exports_list(int backup, const char *suffix, void (*visit)(const char *name, void *context), void *context)
{
	(void)backup; (void)suffix; (void)visit; (void)context;
	return -1;
}

#endif

int ae_exports_open_url(const char *url)
{
#ifdef HALO_SERVER
	(void)url;
	return 0;
#else
	return url && *url && SDL_OpenURL(url);
#endif
}
