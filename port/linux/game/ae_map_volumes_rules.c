/*
AE_MAP_VOLUMES_RULES.C

MAP VOLUMES' names (ae_map_volumes_rules.h).
*/

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "ae_map_volumes_rules.h"

static int same_text(char const *a, char const *b)
{
	for (; *a && *b; a++, b++)
	{
		if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
			return 0;
	}
	return *a == *b;
}

int ae_map_volume_kind(char const *name)
{
	static char const *const kills[] = { "hell", "lava", "kill", "killzone", "kill_zone", "death", "deathzone",
		"death_zone", "deadzone" };
	size_t index;
	int letter;

	if (!name)
		return AE_MAP_VOLUME_NONE;
	for (index = 0; index < sizeof(kills) / sizeof(kills[0]); index++)
	{
		if (same_text(name, kills[index]))
			return AE_MAP_VOLUME_KILL;
	}
	if (strlen(name) != 3)
		return AE_MAP_VOLUME_NONE;
	letter = toupper((unsigned char)name[2]);
	if (toupper((unsigned char)name[0]) == 'T' && toupper((unsigned char)name[1]) == 'P' && letter >= 'A' &&
		letter <= 'Y' && (letter - 'A') % 2 == 0)
	{
		return AE_MAP_VOLUME_LIFT;
	}
	return AE_MAP_VOLUME_NONE;
}

int ae_map_volume_lift_exit(char const *name, char *out, int out_size)
{
	if (ae_map_volume_kind(name) != AE_MAP_VOLUME_LIFT || !out || out_size < 4)
		return 0;
	snprintf(out, (size_t)out_size, "%c%c%c", name[0], name[1], (char)(toupper((unsigned char)name[2]) + 1));
	return 1;
}

int ae_map_volume_same_name(char const *a, char const *b)
{
	return a && b && same_text(a, b);
}
