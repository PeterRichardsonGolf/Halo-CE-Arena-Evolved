/*
AE_EXTRAS_RULES.C

See ae_extras_rules.h.
*/

#include "ae_extras_rules.h"

#include <stdio.h>
#include <ctype.h>
#include <string.h>

const char *const ae_extras_folders[AE_EXTRAS_FOLDER_COUNT] = { "packs", "voices", "sounds" };

int ae_extras_app_places(const char *executable, char *resources, size_t resources_size, char *beside,
	size_t beside_size)
{
	static const char tail[] = ".app/Contents/MacOS/";
	const char *found = NULL, *scan, *start;
	int written;

	if (resources_size)
		resources[0] = '\0';
	if (beside_size)
		beside[0] = '\0';
	if (!executable || !resources_size || !beside_size)
		return 0;
	/* (the last one: the application's own, not a folder of that name above it) */
	for (scan = executable; (scan = strstr(scan, tail)) != NULL; scan++)
		found = scan;
	if (!found || !found[sizeof(tail) - 1])
		return 0;
	/* (the application's name starts after the separator before it) */
	for (start = found; start > executable && start[-1] != '/'; start--)
		;
	written = snprintf(resources, resources_size, "%.*s/Contents/Resources", (int)(found - executable) + 4, executable);
	if (written < 0 || (size_t)written >= resources_size)
		goto fail;
	if (start == executable)
		written = snprintf(beside, beside_size, ".");
	else if (start - executable == 1)
		written = snprintf(beside, beside_size, "/");
	else
		written = snprintf(beside, beside_size, "%.*s", (int)(start - executable) - 1, executable);
	if (written < 0 || (size_t)written >= beside_size)
		goto fail;
	return 1;
fail:
	resources[0] = '\0';
	beside[0] = '\0';
	return 0;
}

int ae_extras_translocated(const char *executable)
{
	return executable && strstr(executable, "/AppTranslocation/") != NULL;
}

int ae_extras_download_name_matches(const char *name)
{
	static const char words[] = "arenaevolved";
	const char *want = words;

	if (!name)
		return 0;
	for (; *name && *want; name++)
	{
		if (*name == '-' || *name == '_' || *name == ' ')
			continue;
		if (tolower((unsigned char)*name) != *want)
			return 0;
		want++;
	}
	return *want == '\0';
}

int ae_extras_name_allowed(const char *name)
{
	return name && *name && name[0] != '.' && !strchr(name, '/') && !strchr(name, '\\');
}

int ae_extras_should_copy(int destination_exists, unsigned long source_seconds, unsigned long destination_seconds)
{
	return !destination_exists || source_seconds > destination_seconds;
}
