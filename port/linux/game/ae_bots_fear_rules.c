/*
AE_BOTS_FEAR_RULES.C

Arena Evolved: when a bot may start a flank (ae_bots_fear_rules.h).
*/

#include <string.h>
#include "ae_bots_fear_rules.h"

int ae_bots_fear_flank_may_start(long now, long cooldown_until, int path_valid, int stuck_recent)
{
	/* (a clock that started over: a cool-down beyond its own length is stale) */
	if (cooldown_until > now && cooldown_until - now <= AE_FEAR_FLANK_COOLDOWN_TICKS)
		return 0;
	if (!path_valid && stuck_recent)
		return 0;
	return 1;
}

long ae_bots_fear_flank_cooldown_end(long now, char const *why)
{
	if (why && (strcmp(why, "unseen") == 0 || strcmp(why, "target") == 0 || strcmp(why, "time") == 0))
		return now + AE_FEAR_FLANK_COOLDOWN_TICKS;
	return 0;
}
