/* The downloaded maps' list's one writer (see the header). */
#include "ae_marks_file.h"

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#endif

void platform_translate_path(char const *xbox_path, char *host_path, unsigned long host_path_size);

/* the list may be this long: the game's table takes 65,536 names of at most 63 characters */
#define MARKS_MAXIMUM_BYTES (65536L * 64)

static pthread_mutex_t marks_lock = PTHREAD_MUTEX_INITIALIZER;

static int refuse(char *error, int error_size, char const *text)
{
	if (error && error_size > 0)
		snprintf(error, (size_t)error_size, "%s", text);
	return 0;
}

static int lower(int c)
{
	return c >= 'A' && c <= 'Z' ? c + 32 : c;
}

/* whether the list (text, size bytes) has a line that is name, in any case */
static int has_line(char const *text, long size, char const *name)
{
	size_t length = strlen(name);
	long at = 0;

	while (at < size)
	{
		long end = at;
		size_t i;

		while (end < size && text[end] != '\n' && text[end] != '\r')
			end++;
		if ((size_t)(end - at) == length)
		{
			for (i = 0; i < length && lower((unsigned char)text[at + (long)i]) == lower((unsigned char)name[i]); i++)
				;
			if (i == length)
				return 1;
		}
		at = end + 1;
	}
	return 0;
}

int ae_marks_append_host(char const *path, char const *name, char *error, int error_size)
{
	char temporary[1100];
	char *text = NULL;
	long size = 0;
	int good = 0;
	FILE *list;

	if (!path || !name || !name[0] || strlen(name) > 63 || strpbrk(name, "\r\n") || strlen(path) + 8 > sizeof(temporary))
		return refuse(error, error_size, "that is not a name for the list of downloaded maps");
	pthread_mutex_lock(&marks_lock);
	errno = 0;
	list = fopen(path, "rb");
	/* (a list that is there but cannot be opened is not an empty one: writing now would drop its names) */
	if (!list && errno != ENOENT && errno != ENOTDIR && errno != 0)
	{
		pthread_mutex_unlock(&marks_lock);
		return refuse(error, error_size, "the game's list of downloaded maps could not be read");
	}
	if (list)
	{
		if (fseek(list, 0, SEEK_END) == 0)
			size = ftell(list);
		if (size < 0 || size > MARKS_MAXIMUM_BYTES)
		{
			fclose(list);
			pthread_mutex_unlock(&marks_lock);
			return refuse(error, error_size, "the game's list of downloaded maps is full");
		}
		text = (char *)malloc((size_t)size + 1);
		if (!text || fseek(list, 0, SEEK_SET) != 0 || fread(text, 1, (size_t)size, list) != (size_t)size)
		{
			free(text);
			fclose(list);
			pthread_mutex_unlock(&marks_lock);
			return refuse(error, error_size, "the list of downloaded maps could not be read");
		}
		fclose(list);
		text[size] = 0;
	}
	/* (a name already there is not written again) */
	if (text && has_line(text, size, name))
	{
		free(text);
		pthread_mutex_unlock(&marks_lock);
		return 1;
	}
	/* (the list as it will be: its size with the name added, a line feed ended a last line a torn write left, is within the
	limit) */
	{
		long projected = size + (size > 0 && text[size - 1] != '\n' ? 1 : 0) + (long)strlen(name) + 1;

		if (projected > MARKS_MAXIMUM_BYTES)
		{
			free(text);
			pthread_mutex_unlock(&marks_lock);
			return refuse(error, error_size, "the game's list of downloaded maps is full");
		}
	}
	/* (the whole list written to a file beside it and put in its place in one step: no reader sees half of it, and a torn
	write leaves the old list) */
	snprintf(temporary, sizeof(temporary), "%s.part", path);
	remove(temporary);
	list = fopen(temporary, "wb");
	if (list)
	{
		good = 1;
		if (size > 0)
		{
			good = fwrite(text, 1, (size_t)size, list) == (size_t)size;
			/* (a last line with no line feed is ended, so the name is a line of its own) */
			if (good && text[size - 1] != '\n')
				good = fputc('\n', list) != EOF;
		}
		good = good && fprintf(list, "%s\n", name) > 0 && fflush(list) == 0;
#if !defined(_WIN32)
		good = good && fsync(fileno(list)) == 0;
#endif
		good = (fclose(list) == 0) && good;
		if (good)
		{
#if defined(_WIN32)
			good = MoveFileExA(temporary, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
			good = rename(temporary, path) == 0;
#endif
		}
		if (!good)
			remove(temporary);
	}
	free(text);
	pthread_mutex_unlock(&marks_lock);
	if (!good)
		return refuse(error, error_size, "the list of downloaded maps could not be written");
	return 1;
}

int ae_marks_append(char const *game_path, char const *name, char *error, int error_size)
{
	char path[1024];

	platform_translate_path(game_path, path, (unsigned long)sizeof(path));
	return ae_marks_append_host(path, name, error, error_size);
}
