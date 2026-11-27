/*
AE_DEV_AMMO_RULES.C

See ae_dev_ammo_rules.h.
*/

#include "ae_dev_ammo_rules.h"

int ae_dev_ammo_applies(int dev_mode_on, int setting_on, int held_by_player, int held_by_bot)
{
	return dev_mode_on && setting_on && held_by_player && !held_by_bot;
}
