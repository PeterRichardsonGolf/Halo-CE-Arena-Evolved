/*
AE_DISCS.C

Arena Evolved: where a modded disc's folder is (ae_discs.h). The game's paths
are the Xbox's (d:\<folder>\<disc>\maps\), the menus' the data root's.
*/

#include "ae_discs.h"
#include "platform.h"

#include <stdio.h>
#include <string.h>

char const *ae_disc_folder(char const *disc)
{
	static char logged[64];
	char path[1200];

	if (!disc || !disc[0])
		return "discs";
	snprintf(path, sizeof(path), "%s/discs/%s/maps", platform_data_root(), disc);
	if (platform_folder_exists(path))
		return "discs";
	snprintf(path, sizeof(path), "%s/mods/%s/maps", platform_data_root(), disc);
	if (!platform_folder_exists(path))
		return "discs";
	/* (said once a disc: an earlier release's place, read as it is) */
	if (strcmp(logged, disc))
	{
		snprintf(logged, sizeof(logged), "%s", disc);
		platform_log("modded discs: %s is in mods/ (an earlier release's place), played from there; "
			"discs/%s is its place now", disc, disc);
	}
	return "mods";
}

int ae_discs_unique(char *names, int width, int count)
{
	int index, kept = 0;

	for (index = 0; index < count; index++)
	{
		if (kept && !strcmp(names + (kept - 1) * width, names + index * width))
			continue;
		if (kept != index)
			memmove(names + kept * width, names + index * width, (size_t)width);
		kept++;
	}
	return kept;
}
