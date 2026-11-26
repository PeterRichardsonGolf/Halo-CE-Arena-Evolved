/*
AE_RANDOMIZER_LOADOUT.C

PLAYER LOADOUT's weapons of AE's (game.randomizer_loadout_primary / _secondary "ae:<registry id>") for randomizer.c's loadout code,
which asks these at three places (see ae_randomizer_glue.h): the weapon a SET value names, whether a missing primary sends the
game back to the level's own loadout, and the weapons RANDOM may also draw. An AE weapon is on a level when its weapon pack was
added to it (ae_weapon_id_available); a level without it is logged and the level's own weapon is what the player gets.
*/

#include <stdio.h>
#include <string.h>
#include "cseries.h"
#include "../src/port_config.h"
#include "ae_weapon_ids.h"
#include "ae_randomizer_menus_rules.h"
#include "ae_randomizer_glue.h"

void platform_log(char const *format, ...);

int ae_randomizer_loadout_weapon(char const *value, long *found)
{
	unsigned int id = ae_rnd_loadout_ae_id(value);

	if (ae_rnd_loadout_value_kind(value) != AE_RND_LOADOUT_VALUE_AE)
		return 0;
	*found = NONE;
	if (!id)
	{
		platform_log("randomizer: the loadout's weapon %s is not one of AE's", value);
		return 1;
	}
	if (!ae_weapon_id_available(id))
	{
		platform_log("randomizer: the loadout's AE weapon %u is not on this level (its weapon pack was not added): the level's own weapon "
			"is used", id);
		return 1;
	}
	*found = ae_weapon_id_definition(id);
	return 1;
}

int ae_randomizer_loadout_primary_missing(char const *requested, long found_definition)
{
	int missing = ae_rnd_loadout_primary_missing(requested, found_definition != NONE);

	if (missing)
		platform_log("randomizer: the loadout's primary weapon (%s) is not on this level: the level's own loadout is used", requested);
	return missing;
}

void ae_randomizer_loadout_add_ae(long *definitions, long *count, long maximum)
{
	int each;

	if (!config_boolean("game.campaign_ae_weapons"))
		return;
	for (each = 0; each < AE_RND_LOADOUT_AE_COUNT; each++)
	{
		unsigned int id = ae_rnd_loadout_ae_id_at(each);
		long definition, at;

		if (!id || !ae_weapon_id_available(id))
			continue;
		definition = ae_weapon_id_definition(id);
		for (at = 0; at < *count && definitions[at] != definition; at++)
			;
		if (definition != NONE && at == *count && *count < maximum)
			definitions[(*count)++] = definition;
	}
}
