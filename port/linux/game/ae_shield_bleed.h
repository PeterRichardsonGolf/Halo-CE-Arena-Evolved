#ifndef AE_SHIELD_BLEED_H
#define AE_SHIELD_BLEED_H

/*
AE_SHIELD_BLEED.H

SHIELD BLEED, the gametype's (ae_variant_ext's shield_bleed; the rule: ae_shield_bleed_rules.h). damage.c's AE hook.
*/

/* (damage.c, a hit that pops the shield of that object) the leftover that goes on to its body: Halo CE's excess, or in
HALO 2 for a player's unit the leftover without the shield's multiplier (raw: the hit's damage before it, shielded:
after it); a round of a no-spill AE rifle (the EVO RIFLE: the damage definition's profile,
ae_burst_rifle_damage_profile)
carries nothing in any mode */
real ae_shield_bleed_excess(long object_index, long damage_definition_index, real excess, real raw, real shielded);

#endif
