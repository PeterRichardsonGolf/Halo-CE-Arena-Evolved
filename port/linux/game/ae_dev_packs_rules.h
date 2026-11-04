/*
AE_DEV_PACKS_RULES.H

DEV MODE's weapon packs (owner 2026-11-02: "missing evo and dmr and other weapon packs on the wheel"): in a game no
other machine can join (OFFLINE, a TEST RUN's, split screen), DEV MODE adds every installed pack to the map besides the
gametype's, for give and the dev wheel only. What the game plays with (the map's spawns, the loadout, a pack weapon's
tuning) stays the gametype's packs: the "played" mask. Pure: no engine headers. Masks are ae_weapon_slots.h's (bit 0 =
slot 1).
*/

#ifndef AE_DEV_PACKS_RULES_H
#define AE_DEV_PACKS_RULES_H

/* the slots to add besides the gametype's: every installed one it does not name, with DEV MODE on in a game nobody can
join; 0 otherwise */
unsigned int ae_dev_packs_extra(unsigned int gametype_mask, unsigned int installed_mask, int dev_mode, int joinable);
/* the packs the game plays with: those loaded that the gametype names (with no extra loaded: every loaded one, as
before) */
unsigned int ae_dev_packs_played(unsigned int loaded_mask, unsigned int gametype_mask);
/* the order packs are let go in, given the slots in the order they were added to the map (the gametype's in slot order,
the vehicle and bomb packs, then DEV MODE's extras): exactly the reverse, as each pack's table goes back to the one the
pack added before it left (reverse slot order is wrong once the extras come after the vehicle and bomb packs). Into
release (room for count); how many */
int ae_dev_packs_release_order(const short *added, int count, short *release);

#endif
