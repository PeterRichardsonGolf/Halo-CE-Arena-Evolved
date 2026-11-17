/*
AE_DEV_AMMO.C

DEV MODE's INFINITE AMMO and BOTTOMLESS CLIP (ae_dev_ammo.h).
*/

#include "cseries.h"
#include "game/players.h"
#include "items/weapons.h"
#include "units/units.h"

#include "ae_dev_ammo.h"
#include "ae_dev_ammo_rules.h"
#include "ae_dev_mode.h"
#include "ae_objective_log.h"

int config_boolean(const char *name);

static int ae_dev_ammo_for(long weapon_index, const char *key)
{
	long unit_index;
	long player_index = NONE;

	/* (the cheap tests first: this runs for every shot and reload) */
	if (weapon_index == NONE || !ae_dev_mode_on() || !config_boolean(key))
		return FALSE;
	unit_index = weapon_get(weapon_index)->object.parent_object_index;
	if (unit_index != NONE)
		player_index = unit_get(unit_index)->unit.player_index;
	return ae_dev_ammo_applies(TRUE, TRUE, player_index != NONE, player_index != NONE && ae_obj_player_is_bot(player_index));
}

int ae_dev_ammo_infinite(long weapon_index)
{
	return ae_dev_ammo_for(weapon_index, "system.dev_infinite_ammo");
}

int ae_dev_ammo_bottomless(long weapon_index)
{
	return ae_dev_ammo_for(weapon_index, "system.dev_bottomless_clip");
}
