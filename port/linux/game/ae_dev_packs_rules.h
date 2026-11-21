/*
AE_DEV_PACKS_RULES.H

The packs a game plays with. Weapon cache (design 6.5, T2-4): every weapon is on the map (ae_weapon_cache.h), so DEV MODE's
extra packs (owner 2026-11-02, added beside the gametype's for give and the wheel only) are gone; DEV MODE's give and wheel take any
weapon of the cache (ae_weapon_ids.h), and what the game plays with (the map's spawns, the loadout, a pack weapon's tuning)
stays the gametype's: the "played" mask. Pure: no engine headers. Masks are ae_weapon_slots.h's (bit 0 =
slot 1).
*/

#ifndef AE_DEV_PACKS_RULES_H
#define AE_DEV_PACKS_RULES_H

/* the packs the game plays with: those loaded that the gametype names (with no extra loaded: every loaded one, as
before) */
unsigned int ae_dev_packs_played(unsigned int loaded_mask, unsigned int gametype_mask);
/* the order packs are let go in, given the slots in the order they were added to the map (the gametype's in slot order,
the vehicle and bomb packs, then DEV MODE's extras): exactly the reverse, as each pack's table goes back to the one the
pack added before it left (reverse slot order is wrong once the extras come after the vehicle and bomb packs). Into
release (room for count); how many */
int ae_dev_packs_release_order(const short *added, int count, short *release);

#endif
