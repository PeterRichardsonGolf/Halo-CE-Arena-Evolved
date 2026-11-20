/*
AE_SHIELD_BLEED_RULES.C

See ae_shield_bleed_rules.h.
*/

#include "ae_shield_bleed_rules.h"
#include "ae_variant_ext_rules.h"

float ae_shield_bleed_excess_rule(int mode, float excess, float raw, float shielded, int no_spill)
{
	if (no_spill)
		return 0.0f;
	if (mode != AE_EXT_SHIELD_BLEED_HALO2 || raw < 0.0001f || shielded < 0.0001f)
		return excess;
	return excess * raw / shielded;
}
