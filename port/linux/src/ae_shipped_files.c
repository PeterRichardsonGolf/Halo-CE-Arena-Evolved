/*
AE_SHIPPED_FILES.C

See ae_shipped_files.h.
*/

#include <stdio.h>
#include <string.h>

#include <SDL3/SDL.h>

#include "ae_shipped_files.h"

void config_folder(char *path, size_t size);

static long read_path(const char *path, char *text, long size)
{
	FILE *file = fopen(path, "rb");
	size_t length;

	if (!file)
		return -1;
	length = fread(text, 1, (size_t)size - 1, file);
	fclose(file);
	text[length] = 0;
	return (long)length;
}

long ae_shipped_file_read(const char *name, char *text, long size)
{
	char folder[1024], path[1200];
	long length;

	if (!text || size < 2)
		return -1;
	text[0] = 0;
	config_folder(folder, sizeof(folder));
	snprintf(path, sizeof(path), "%s%s%s", folder, folder[0] && folder[strlen(folder) - 1] != '/' &&
		folder[strlen(folder) - 1] != '\\' ? "/" : "", name);
	length = read_path(path, text, size);
#if !defined(HALO_ANDROID) && !defined(HALO_WEB)
	if (length < 0)
	{
		const char *base = SDL_GetBasePath();

		if (base)
		{
			snprintf(path, sizeof(path), "%s%s", base, name);
			length = read_path(path, text, size);
		}
	}
#endif
	return length;
}
