/*
AE_SPAWN_MODERN_RULES.C

The rules of AE MODERN SPAWNS' recent deaths (ae_spawn_modern_rules.h).
*/

#include "ae_spawn_modern_rules.h"

int ae_spawn_recent_death(long now, long death_tick, long window)
{
	return death_tick > 0 && now >= death_tick && now - death_tick < window;
}

void ae_spawn_deaths_forget(long *death_ticks, int count)
{
	int index;

	for (index = 0; index < count; index++)
		death_ticks[index] = 0;
}
