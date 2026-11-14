/*
AE_MAP_WEAPONS.C

Which of the weapon list's weapons a map keeps (ae_map_weapons.h). Upstream
turns the flamethrower into a rocket launcher on every map, as the Xbox's
maps have it unfinished; a Halo PC, Custom Edition or MCC map has it as a
finished multiplayer weapon and keeps it.
*/

#include "cseries.h"
#include "game/game_engine.h"
#include "items/weapon_definitions.h"
#include "tag_files/tag_groups.h"

#include "ae_map_weapons.h"
#include "mcc_cache.h"

boolean cache_file_tags_are_ce(void);

boolean ae_map_keeps_flamethrower(void)
{
	return cache_file_tags_are_ce() || mcc_cache_tags_loaded();
}

boolean ae_map_keeps_gravity_rifle(void)
{
	/* a Halo PC, Custom Edition or MCC map may put a finished weapon (the
	community's ports: the battle rifle) in the gravity rifle's
	place; only the Xbox's maps carry the unfinished cut weapon */
	return cache_file_tags_are_ce() || mcc_cache_tags_loaded();
}

long ae_map_loadout_weapon(byte weapon, long flamethrower, long rocket_launcher)
{
	long definition_index = NONE;

	if (weapon == _loadout_weapon_fuel_rod)
		definition_index = tag_loaded(WEAPON_DEFINITION_TAG, "weapons\\plasma_cannon\\plasma_cannon");
	else if (weapon == _loadout_weapon_flamethrower)
		definition_index = ae_map_keeps_flamethrower() ? flamethrower : NONE;
	else
		return NONE;
	return definition_index != NONE ? definition_index : rocket_launcher;
}

char const *ae_map_loadout_weapon_name(byte weapon)
{
	return weapon == _loadout_weapon_fuel_rod ? "fuel rod" : weapon == _loadout_weapon_flamethrower ? "flamethrower" :
		"?";
}
