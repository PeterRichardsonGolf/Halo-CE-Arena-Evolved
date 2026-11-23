/*
AE_MODE_GUARD_RULES.C

Bug 139's rules (ae_mode_guard_rules.h).
*/

#include <stddef.h>

#include "ae_mode_guard_rules.h"

const char *ae_mode_guard_missing(long mode, struct ae_mode_guard_map const *map)
{
	switch (mode)
	{
	case AE_MODE_GUARD_CTF:
	case AE_MODE_GUARD_BOMB:
		if (map->ctf_flags[0] <= 0)
			return "red CTF flag";
		if (map->ctf_flags[1] <= 0)
			return "blue CTF flag";
		return NULL;
	case AE_MODE_GUARD_ODDBALL:
		return map->ball_spawns > 0 ? NULL : "oddball spawn";
	case AE_MODE_GUARD_KING:
		return map->hills > 0 ? NULL : "hill";
	case AE_MODE_GUARD_RACE:
		return map->race_flags > 0 ? NULL : "race flag";
	default:
		return NULL;
	}
}

const char *ae_mode_guard_mode_name(long mode)
{
	switch (mode)
	{
	case AE_MODE_GUARD_CTF:
		return "CTF";
	case AE_MODE_GUARD_SLAYER:
		return "SLAYER";
	case AE_MODE_GUARD_ODDBALL:
		return "ODDBALL";
	case AE_MODE_GUARD_KING:
		return "KING OF THE HILL";
	case AE_MODE_GUARD_RACE:
		return "RACE";
	case AE_MODE_GUARD_BOMB:
		return "BOMB";
	default:
		return "";
	}
}

long ae_mode_guard_slayer_score(int teams)
{
	return teams ? 50 : 25;
}

int ae_mode_guard_spawn_anywhere(int spawns_for_mode, int spawns)
{
	return spawns_for_mode <= 0 && spawns > 0;
}
