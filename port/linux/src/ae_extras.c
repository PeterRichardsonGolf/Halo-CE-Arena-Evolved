/*
AE_EXTRAS.C

See ae_extras.h.
*/

#include "ae_extras.h"
#include "ae_extras_rules.h"
#include "platform.h"
#include "posix.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

#define EXTRAS_MAX_DEPTH 3 /* voices/<voice>/<clip>.wav is two folders down */
#define EXTRAS_PLACES 3
#define EXTRAS_DOWNLOAD_PLACES 8

struct extras_count
{
	int copied;
	int present;
	int failed;
	char first_failure[1100];
};

static int path_is_directory(const char *path)
{
	struct posix_file_information information;

	return posix_stat(path, &information) == 0 && (information.flags & _posix_file_is_directory);
}

/* the file copied as <destination>.new, then put in place in one step, with the download's modification time (so the
next start finds it as new as the download); 1 when done */
static int copy_file(const char *source, const char *destination, unsigned long modified)
{
	char temporary[1100];
	char buffer[16384];
	FILE *from, *to;
	size_t count;
	int good = 1;

	if (snprintf(temporary, sizeof(temporary), "%s.new", destination) >= (int)sizeof(temporary))
		return 0;
	if (!(from = fopen(source, "rb")))
		return 0;
	if (!(to = fopen(temporary, "wb")))
	{
		fclose(from);
		return 0;
	}
	while ((count = fread(buffer, 1, sizeof(buffer), from)) > 0)
	{
		if (fwrite(buffer, 1, count, to) != count)
		{
			good = 0;
			break;
		}
	}
	if (ferror(from))
		good = 0;
	fclose(from);
	if (fclose(to))
		good = 0;
	if (good)
	{
		posix_set_file_times(temporary, 0, 0, (posix_ulong)modified, 0);
		good = posix_replace_file(temporary, destination) == 0;
	}
	if (!good)
		remove(temporary);
	return good;
}

static void copy_tree(const char *source, const char *destination, int depth, struct extras_count *count)
{
	void *directory;
	char name[256];

	if (depth > EXTRAS_MAX_DEPTH || !(directory = posix_directory_open(source)))
		return;
	posix_make_directory(destination);
	while (posix_directory_next(directory, name, sizeof(name)))
	{
		char from[1100], to[1100];
		struct posix_file_information information, existing;

		if (!ae_extras_name_allowed(name) ||
			snprintf(from, sizeof(from), "%s/%s", source, name) >= (int)sizeof(from) ||
			snprintf(to, sizeof(to), "%s/%s", destination, name) >= (int)sizeof(to) ||
			posix_is_link(from) || posix_stat(from, &information) != 0)
		{
			continue;
		}
		if (information.flags & _posix_file_is_directory)
		{
			copy_tree(from, to, depth + 1, count);
		}
		else if (ae_extras_should_copy(posix_stat(to, &existing) == 0, information.modification_seconds,
			existing.modification_seconds))
		{
			if (copy_file(from, to, information.modification_seconds))
			{
				count->copied++;
			}
			else
			{
				if (!count->failed++)
					snprintf(count->first_failure, sizeof(count->first_failure), "%s", from);
			}
		}
		else
		{
			count->present++;
		}
	}
	posix_directory_close(directory);
}

/* one place's folders copied; whether it held any */
static int copy_place(const char *place, const char *data_root)
{
	struct extras_count count;
	char found[64] = "";
	int index, any = 0;

	memset(&count, 0, sizeof(count));
	for (index = 0; index < AE_EXTRAS_FOLDER_COUNT; index++)
	{
		char from[1100], to[1100];

		if (snprintf(from, sizeof(from), "%s/%s", place, ae_extras_folders[index]) >= (int)sizeof(from) ||
			snprintf(to, sizeof(to), "%s/%s", data_root, ae_extras_folders[index]) >= (int)sizeof(to) ||
			!path_is_directory(from))
		{
			continue;
		}
		any = 1;
		if (strlen(found) + strlen(ae_extras_folders[index]) + 2 < sizeof(found))
			sprintf(found + strlen(found), "%s%s", found[0] ? ", " : "", ae_extras_folders[index]);
		copy_tree(from, to, 1, &count);
	}
	if (any)
	{
		platform_log("extras: %s found in %s: %d files copied into %s, %d already there", found, place, count.copied,
			data_root, count.present);
		if (count.failed)
			platform_log("extras: %d files could not be copied (the first: %s)", count.failed, count.first_failure);
	}
	return any;
}

#ifdef __APPLE__
/* the player's Downloads folder, and the unpacked release folders in it (the app may run from a temporary copy, or
have been moved to Applications, with the folders left where the download was unpacked) */
static int copy_from_downloads(const char *data_root)
{
	const char *home = getenv("HOME");
	char downloads[1024];
	char names[EXTRAS_DOWNLOAD_PLACES][256];
	void *directory;
	char name[256];
	int count = 0, index, found = 0;

	if (!home || !*home || snprintf(downloads, sizeof(downloads), "%s/Downloads", home) >= (int)sizeof(downloads))
		return 0;
	if (!(directory = posix_directory_open(downloads)))
	{
		platform_log("extras: could not read %s", downloads);
		return 0;
	}
	while (count < EXTRAS_DOWNLOAD_PLACES && posix_directory_next(directory, name, sizeof(name)))
	{
		char path[1100];

		if (ae_extras_name_allowed(name) && ae_extras_download_name_matches(name) &&
			snprintf(path, sizeof(path), "%s/%s", downloads, name) < (int)sizeof(path) && path_is_directory(path))
		{
			snprintf(names[count++], sizeof(names[0]), "%s", name);
		}
	}
	posix_directory_close(directory);
	platform_log("extras: looking in %s and %d unpacked release folder%s in it", downloads, count, count == 1 ? "" : "s");
	found |= copy_place(downloads, data_root);
	for (index = 0; index < count; index++)
	{
		char path[1100];

		if (snprintf(path, sizeof(path), "%s/%s", downloads, names[index]) < (int)sizeof(path))
			found |= copy_place(path, data_root);
	}
	return found;
}
#endif

void ae_extras_copy(const char *data_root)
{
	char places[EXTRAS_PLACES][1024];
	const char *environment = getenv("HALO_EXTRAS_ROOT");
	int count = 0, index, found = 0;
#ifdef __APPLE__
	char executable[1024];
	char resources[1024], beside[1024];
	unsigned int executable_size = sizeof(executable);
	int translocated = 0;
#endif

	if (!data_root || !*data_root)
		return;
	if (environment && *environment && strlen(environment) < sizeof(places[0]))
		snprintf(places[count++], sizeof(places[0]), "%s", environment);
#ifdef __APPLE__
	if (_NSGetExecutablePath(executable, &executable_size) == 0)
	{
		translocated = ae_extras_translocated(executable);
		if (ae_extras_app_places(executable, resources, sizeof(resources), beside, sizeof(beside)))
		{
			/* (the download's folder first: a newer unpack is the one beside the application) */
			snprintf(places[count++], sizeof(places[0]), "%s", beside);
			snprintf(places[count++], sizeof(places[0]), "%s", resources);
		}
	}
#endif
	for (index = 0; index < count; index++)
	{
		/* (the data root itself is never its own source) */
		if (!strcmp(places[index], data_root))
			continue;
		found |= copy_place(places[index], data_root);
	}
#ifdef __APPLE__
	if (translocated)
		platform_log("extras: macOS opened the application from a temporary read-only copy (App Translocation), so "
			"the folders beside it cannot be read. Move the application out of the download's folder once "
			"(drag it to Applications, or run " AE_EXTRAS_QUARANTINE_HINT " in Terminal) and start it again");
	if (!found)
	{
		char packs[1100];
		struct posix_file_information information;

		snprintf(packs, sizeof(packs), "%s/packs", data_root);
		if (posix_stat(packs, &information) != 0)
			found = copy_from_downloads(data_root);
	}
	if (!found)
	{
		char packs[1100];
		struct posix_file_information information;

		snprintf(packs, sizeof(packs), "%s/packs", data_root);
		if (posix_stat(packs, &information) != 0)
		{
			platform_log("extras: no packs, voices or sounds in %s, beside the application, in its Resources or in Downloads%s",
				data_root, translocated
					? " (macOS opened the application from a temporary copy, so nothing beside it can be read: "
					  "copy the download's packs, voices and sounds folders into the folder above yourself)"
					: "");
		}
	}
#else
	(void)found;
#endif
}
