/*
AE_FALL_STUN_RULES.C

See ae_fall_stun_rules.h.
*/

#include "ae_fall_stun_rules.h"
#include "ae_variant_ext_rules.h"

int ae_fall_stun_off(int rule)
{
	return rule == AE_EXT_FALL_STUN_OFF;
}
