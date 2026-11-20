/*
AE_MAP_ALIAS_RULES.C

The old-name table of ae_map_alias_rules.h.
*/

#include <ctype.h>
#include <string.h>
#include "ae_map_alias_rules.h"

static struct { char const *old_name; char const *identity; } const pairs[] =
{
	{ "ascetic@smp", "ascetic mcc debug flycam pm@ws2899679058" },
	{ "blamitemines@smp", "blamitemines mcc flycam pm@ws2899709584" },
	{ "blood_covenant@smp", "blood_covenant_mcc flycam pm@ws2899741750" },
	{ "co2@smp", "co2 mcc debug flycam pm@ws2900166697" },
	{ "cold_shoulder@smp", "cold_shoulder mcc flycam pm@ws2899509722" },
	{ "delta_ruined@smp", "delta_ruined flycam mcc@ws2899747345" },
	{ "dredwerkz@smp", "dredwerkz mcc debug flycam pm@ws2900215828" },
	{ "fissurefall@smp", "fissurefall mcc dbug flycam pm@ws2899774877" },
	{ "fractured_peak@smp", "fractured peak mcc flycam pm@ws2900252105" },
	{ "frigid_keep@smp", "frigid_keep dbug flycam mcc pm@ws2899770689" },
	{ "gothra@smp", "gothra mcc debug flycam pm@ws2899688264" },
	{ "gridiron@smp", "gridiron mcc debug flycam pm@ws2900050996" },
	{ "gutter_nm@smp", "gutter mcc debug flycam pm nm@ws2900257741" },
	{ "hektik@smp", "hektik mcc debug flycam pm@ws2900177725" },
	{ "intensity_v2@smp", "intensity_v2 debug flycam pm@ws2900242317" },
	{ "nitra_v2@smp", "nitra_v2 debug flycam pm@ws2900199550" },
	{ "ruins_of_alph@smp", "ruins_of_alph mcc flycam pm@ws2905217149" },
	{ "sympathy@smp", "sympathy mcc debug flycam pm@ws2899751222" },
	{ "warren@smp", "warren_mcc debug flycam pm@ws2900072240" },
	{ "dreadnought@pro", "dreadnought@ws2899301014" },
	{ "pat_beavercreek@pro", "pat_beavercreek@ws2899342285" },
	{ "pat_chillout@pro", "pat_chillout@ws2899343509" },
	{ "pat_damnation@pro", "pat_damnation@ws2899341154" },
	{ "pat_downrush@pro", "pat_downrush@ws2899270824" },
	{ "pat_hangemhigh@pro", "pat_hangemhigh@ws2899339889" },
	{ "pat_longest@pro", "pat_longest@ws2899345854" },
	{ "pat_prisoner@pro", "pat_prisoner@ws2899340399" },
	{ "pat_ratrace@pro", "pat_ratrace@ws2899352017" },
	{ "pat_wizard@pro", "pat_wizard@ws2899666869" },
};

#define PAIR_COUNT ((int)(sizeof(pairs) / sizeof(pairs[0])))

static int same_text(char const *a, char const *b)
{
	for (; *a && *b; a++, b++)
		if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
			return 0;
	return *a == *b;
}

char const *ae_map_alias_old_name(char const *identity)
{
	int index;

	if (!identity)
		return NULL;
	for (index = 0; index < PAIR_COUNT; index++)
		if (same_text(pairs[index].identity, identity))
			return pairs[index].old_name;
	return NULL;
}

char const *ae_map_alias_new_name(char const *old_name)
{
	int index;

	if (!old_name)
		return NULL;
	for (index = 0; index < PAIR_COUNT; index++)
		if (same_text(pairs[index].old_name, old_name))
			return pairs[index].identity;
	return NULL;
}

int ae_map_alias_old_file(char const *identity, char *file, int size)
{
	char const *old_name = ae_map_alias_old_name(identity);
	char const *at;
	int length, index;

	if (!old_name || !file || size <= 0)
		return 0;
	at = strchr(old_name, '@');
	length = at ? (int)(at - old_name) : (int)strlen(old_name);
	if (length <= 0 || length >= size)
		return 0;
	for (index = 0; index < length; index++)
		file[index] = (char)tolower((unsigned char)old_name[index]);
	file[length] = 0;
	return 1;
}

int ae_map_alias_count(void)
{
	return PAIR_COUNT;
}

int ae_map_alias_get(int index, char const **old_name, char const **identity)
{
	if (index < 0 || index >= PAIR_COUNT)
		return 0;
	*old_name = pairs[index].old_name;
	*identity = pairs[index].identity;
	return 1;
}
