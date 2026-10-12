/*
AE_WEAPON_RULES.H

Arena Evolved's small weapon rules, pure (port/linux/tests/ae_weapon_rules_test.c
runs them with fake triggers): included by source/items/weapons.c's AE hooks.
*/

#ifndef __AE_WEAPON_RULES_H
#define __AE_WEAPON_RULES_H

/* the magazine a charged trigger's ammunition check reads (weapons.c's
_trigger_charged state): the trigger's own when the weapon has it, else -1,
no magazine. A trigger with none (-1) has no rounds to run out of, as the
idle state already reads it. The Xbox game read the slot before the
weapon's magazines there (the second trigger's last bytes, out of the
array's bounds by one): whether a fully charged shot was let go at once hung
on that unrelated value. No stock multiplayer weapon has a charging trigger
without a magazine (the plasma pistol's has one); a map's own weapon can */
static inline short ae_weapon_charged_magazine_index(short magazine_index, short magazine_count)
{
	return magazine_index >= 0 && magazine_index < magazine_count ? magazine_index : (short)-1;
}

#endif
