/*
AE_MAP_WEAPONS.H

Which of the weapon list's weapons a map keeps (ae_map_weapons.c).
*/

#ifndef __AE_MAP_WEAPONS_H
#define __AE_MAP_WEAPONS_H

#include "cseries.h"

/* whether the loaded map keeps its flamethrower: a Halo PC, Custom Edition or
MCC map, whose flamethrower is a finished multiplayer weapon (the owner's
11-01 rule: every weapon a map has). An original Xbox map's is unfinished:
game_engine_remap_weapon makes it a rocket launcher, as upstream does */
boolean ae_map_keeps_flamethrower(void);

/* whether the loaded map keeps its gravity rifle slot: community
ports (CE format) put the battle rifle there; only an original Xbox map's
unfinished cut weapon becomes a rocket launcher */
boolean ae_map_keeps_gravity_rifle(void);

/* (game_engine_loadout_weapon_definition, AE hook) a custom loadout's FUEL ROD or FLAMETHROWER (game_engine.h's
_loadout_weapon_fuel_rod, _flamethrower: upstream's numbers before it took them out) on this map: the map's fuel rod
gun or its kept flamethrower (given here as the weapon list's), else its rocket launcher (rocket_launcher), as an
original Xbox map has neither; NONE for any other value */
long ae_map_loadout_weapon(byte weapon, long flamethrower, long rocket_launcher);
/* (game_engine_log_loadout_weapon, AE hook) their names in the log, "?" for any other value */
char const *ae_map_loadout_weapon_name(byte weapon);

#endif // __AE_MAP_WEAPONS_H
