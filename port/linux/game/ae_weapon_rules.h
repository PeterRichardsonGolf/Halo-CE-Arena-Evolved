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

/* INFINITE AMMO and BOTTOMLESS CLIP as a gametype's rules (seed revision 16, the OCTAGON drills; the cheats
cheat_infinite_ammo / cheat_bottomless_clip as before): bits 5 and 6 of the universal variant's pack_loadout byte
(game_engine.h; bits 0-4 the weapon pack's, bit 7 free), read whatever the weapon pack. The byte travels with the
variant (the saved file's and the network game's sizes are unchanged), so every machine in a game plays the host's;
every gametype from before has 0 there, and older builds ignore them. INFINITE AMMO: a reload takes nothing from
the reserve, an energy weapon never ages; BOTTOMLESS CLIP: a shot takes nothing from the magazine and adds no heat
(weapons.c's AE hooks, ae_gametype_ammo.c) */
#define AE_WEAPON_INFINITE_AMMO_BIT 0x20u
#define AE_WEAPON_BOTTOMLESS_CLIP_BIT 0x40u

static inline int ae_weapon_infinite_ammo(unsigned char pack_loadout)
{
	return (pack_loadout & AE_WEAPON_INFINITE_AMMO_BIT) != 0;
}

static inline int ae_weapon_bottomless_clip(unsigned char pack_loadout)
{
	return (pack_loadout & AE_WEAPON_BOTTOMLESS_CLIP_BIT) != 0;
}

/* the byte with them on or off (its other bits kept) */
static inline unsigned char ae_weapon_ammo_byte_set(unsigned char pack_loadout, int infinite_ammo, int bottomless_clip)
{
	pack_loadout &= (unsigned char)~(AE_WEAPON_INFINITE_AMMO_BIT | AE_WEAPON_BOTTOMLESS_CLIP_BIT);
	return (unsigned char)(pack_loadout | (infinite_ammo ? AE_WEAPON_INFINITE_AMMO_BIT : 0u) |
		(bottomless_clip ? AE_WEAPON_BOTTOMLESS_CLIP_BIT : 0u));
}

#endif
