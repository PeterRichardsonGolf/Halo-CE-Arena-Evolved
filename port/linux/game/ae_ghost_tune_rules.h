/*
AE_GHOST_TUNE_RULES.H

THE AE GHOST, pure (port/linux/tests/ae_ghost_tune_rules_test.c): the stock multiplayer ghost
(vehicles\ghost\ghost_mp) made quicker and its cannon sharper in every AE gametype (owner, 2026-10-31: "use the stock
one and juice up the cannons and speed", the medium numbers). ae_ghost_tune.c reads it.

Where it applies: a gametype with VEHICLES DESTRUCTIBLE ON (ae_vehicle_death_rules.h, bit 11: seed revision 23 has it
ON in every AE gametype with vehicles, the stock gametypes OFF), the rule AE's other vehicle changes follow. Stock and
custom gametypes keep Halo CE's ghost. Every machine applies it from the gametype's flags (the host's variant on a
client), so an AE client drives the same ghost the host simulates; an older build, OpenCE or ChupathingyCE client
keeps the stock numbers and may see its ghost corrected by the host.

The numbers scale the map's own tag, so a map with its own ghost_mp keeps its character:
- SPEED: top speed and acceleration x1.2 (stock 0.225 -> 0.27 world units a tick, 0.005 -> 0.006).
- CANNON: at least 8 shots a second from the first shot (stock 5 rising to 6 over a 1 s spin-up); the bolts' spread
  halved (stock 0.5 degrees at the centre, 0.5-3 as it grows -> 0.25 and 0.25-1.5). Damage a bolt is the tag's.
*/

#ifndef __AE_GHOST_TUNE_RULES_H
#define __AE_GHOST_TUNE_RULES_H

#include <string.h>

#define AE_GHOST_TUNE_TAG "vehicles\\ghost\\ghost_mp"
#define AE_GHOST_TUNE_SPEED_SCALE 1.2f
#define AE_GHOST_TUNE_ROUNDS_PER_SECOND 8.0f
#define AE_GHOST_TUNE_SPREAD_SCALE 0.5f

/* the tag the AE ghost is made from (any case, either slash) */
static inline int ae_ghost_tune_is_ghost(char const *name)
{
	char const *want = AE_GHOST_TUNE_TAG;

	if (!name)
		return 0;
	for (; *name && *want; name++, want++)
	{
		char a = *name >= 'A' && *name <= 'Z' ? (char)(*name - 'A' + 'a') : *name;

		if (a == '/')
			a = '\\';
		if (a != *want)
			return 0;
	}
	return *name == '\0' && *want == '\0';
}

/* the cannon's rate: the tag's own when it is already faster */
static inline float ae_ghost_tune_rate(float tag_rate)
{
	return tag_rate > AE_GHOST_TUNE_ROUNDS_PER_SECOND ? tag_rate : AE_GHOST_TUNE_ROUNDS_PER_SECOND;
}

#endif
