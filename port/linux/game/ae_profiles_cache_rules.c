/*
AE_PROFILES_CACHE_RULES.C

See ae_profiles_cache_rules.h.
*/

#include "ae_profiles_cache_rules.h"

int ae_profiles_cache_fresh(int valid, unsigned long now, unsigned long built_at, unsigned long ttl_ms)
{
	/* (a 32-bit clock on Windows wraps round: the difference taken in 32 bits) */
	return valid && (unsigned long)(unsigned int)(now - built_at) < ttl_ms;
}
