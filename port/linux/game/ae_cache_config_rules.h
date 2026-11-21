/*
AE_CACHE_CONFIG_RULES.H

Which config view of a weapon a game uses, pure (tests/ae_cache_config_rules_test.c). A variant is (weapon id, config id), config
0 the weapon itself (weapon cache design 3.5). The gametype's AECW block (v3) names configs ONLY in its two START pairs; a weapon
SET (casual 0, comp 1, BTB 2; weapon spawners design 3.3) names the config of a pool entry ("battle_rifle:br_mag24"). The rule:

  1. a LOADOUT spawn (loadout slot 0 PRIMARY or 1 SECONDARY) of a weapon that START names with a config uses that config;
  2. else the weapon the game's SET gives a config (ae_cache_set_configs below) uses it, on map copies, layouts and loadouts alike;
  3. else config 0.

[MY CALL] The cache registry has no default-config column and no set ships a config yet (the comp bundle's "battle_rifle:
br_mag24" is the weapon lane's to define), so the table is EMPTY today and rule 2 changes nothing; the first set row to name a
config is one line here and one line in the test. A config the cache or the map lacks is the engine's check (a view must
exist for that weapon), not this file's: it falls back to config 0 there.
*/

#ifndef AE_CACHE_CONFIG_RULES_H
#define AE_CACHE_CONFIG_RULES_H

#include "ae_weapon_selection.h"

struct ae_cache_set_config
{
	unsigned int set_id, weapon_id, config_id;
};

/* the weapon sets' configs (set ids: ae_weapon_selection.h): none yet */
static struct ae_cache_set_config const ae_cache_set_configs[] =
{
	{ 0, 0, 0 } /* (terminator: weapon id 0) */
};

enum
{
	AE_CACHE_CONFIG_MAP_COPY = -1,
	AE_CACHE_CONFIG_PRIMARY = 0,
	AE_CACHE_CONFIG_SECONDARY = 1
};

static inline unsigned int ae_cache_config_pick(
	struct ae_weapon_selection const *selection,
	unsigned int weapon_id,
	int loadout_slot,
	struct ae_cache_set_config const *table)
{
	unsigned int index;

	if (!selection || !weapon_id)
		return 0;
	if (loadout_slot == AE_CACHE_CONFIG_PRIMARY || loadout_slot == AE_CACHE_CONFIG_SECONDARY)
	{
		struct ae_weapon_start const *start = &selection->start[loadout_slot];

		if (start->weapon == weapon_id && start->config)
			return start->config;
	}
	/* (the set only counts when the toggle is on: set_id is 0 whenever it is off, which is the casual set's row) */
	for (index = 0; table && table[index].weapon_id; index++)
	{
		if (table[index].set_id == selection->set_id && table[index].weapon_id == weapon_id)
			return table[index].config_id;
	}
	return 0;
}

#endif /* AE_CACHE_CONFIG_RULES_H */
