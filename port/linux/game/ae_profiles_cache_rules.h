/*
AE_PROFILES_CACHE_RULES.H

(0.8.2) ae_profiles_list's cache: the main menu, the PARTY manager and MY PARTY asked for the profiles' list every frame,
and each ask enumerated the saved games and opened every profile's file (on Windows hundreds of opens a second, the menus
lagging on a slow laptop). A list built once is reused for AE_PROFILES_CACHE_MS, and at once rebuilt after a change AE
makes (a new, renamed, deleted or saved profile). A profile written by another path shows within the cache's time. Pure:
port/linux/tests/ae_profiles_cache_rules_test.c.
*/

#ifndef AE_PROFILES_CACHE_RULES_H
#define AE_PROFILES_CACHE_RULES_H

#define AE_PROFILES_CACHE_MS 1000ul

/* whether a list built at built_at (milliseconds) is still the one to use at now: valid (no change since) and younger
than ttl_ms (the clock may wrap round) */
int ae_profiles_cache_fresh(int valid, unsigned long now, unsigned long built_at, unsigned long ttl_ms);

#endif
