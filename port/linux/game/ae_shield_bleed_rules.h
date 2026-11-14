/*
AE_SHIELD_BLEED_RULES.H

SHIELD BLEED's pure rule (ae_shield_bleed.c plays it; the gametype's byte is ae_variant_ext's shield_bleed,
AE_EXT_SHIELD_BLEED_*). When a hit pops a shield, the damage left over goes on to the body:
- CLASSIC: Halo CE's leftover, the shield's damage multiplier still in it (a weapon strong against shields carries
  its multiplier into the body; an explosion weak against them carries less);
- HALO 2: the leftover without the shield's multiplier (the later Halos' way).
A no-spill rifle's breaking round carries nothing in either (the shield takes it all).
*/

#ifndef AE_SHIELD_BLEED_RULES_H
#define AE_SHIELD_BLEED_RULES_H

/* the leftover that goes on to the body: excess (Halo CE's), the hit's damage before the shield's multiplier (raw) and
after it (shielded); an unknown mode is CLASSIC */
float ae_shield_bleed_excess_rule(int mode, float excess, float raw, float shielded, int no_spill);

#endif
