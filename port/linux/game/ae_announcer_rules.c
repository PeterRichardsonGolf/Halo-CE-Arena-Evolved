/*
AE_ANNOUNCER_RULES.C

The announcer's pure rules (ae_announcer_rules.h).
*/

#include <string.h>

#include "ae_announcer_rules.h"

int ae_announcer_take(int clean, int others, unsigned long draw)
{
	unsigned long pick = draw / AE_ANNOUNCER_ODDS;

	if (clean < 0)
		clean = 0;
	if (others < 0)
		others = 0;
	if (clean + others == 0)
		return -1;
	if (!clean)
		return (int)(pick % (unsigned long)others);
	if (others && draw % AE_ANNOUNCER_ODDS == 0)
		return clean + (int)(pick % (unsigned long)others);
	return (int)(pick % (unsigned long)clean);
}

int ae_announcer_run_kill(int *run)
{
	if (*run < 10000)
		(*run)++;
	return *run == AE_ANNOUNCER_RUN_KILLS;
}

void ae_announcer_run_end(int *run)
{
	*run = 0;
}

int ae_announcer_burst_kill(struct ae_announcer_burst *burst, long tick, int who, long source)
{
	if (!burst->count || burst->tick != tick || burst->who != who || burst->source != source)
	{
		burst->tick = tick;
		burst->who = who;
		burst->source = source;
		burst->count = 0;
	}
	if (burst->count < 10000)
		burst->count++;
	return burst->count == AE_ANNOUNCER_BURST_KILLS;
}

int ae_announcer_auto_pick(int const *lines, int count)
{
	int index, best = -1;

	if (!lines)
		return -1;
	for (index = 0; index < count; index++)
	{
		if (lines[index] >= AE_ANNOUNCER_AUTO_LEAST && (best < 0 || lines[index] > lines[best]))
			best = index;
	}
	return best;
}

/* the medals an announcer says aloud in play; every other medal is shown and stays quiet */
static char const *const spoken_medals[] = {
	"medal_double_kill", "medal_triple_of_each_kill", "medal_overkill", "medal_killtacular", "medal_killtrocity",
	"medal_killimanjaro", "medal_killtastrophe", "medal_killpocalypse", "medal_killionaire",
	"medal_killing_spree", "medal_killing_frenzy", "medal_running_riot", "medal_rampage",
	"medal_untouchable_row_wit_untouchable", "medal_invincible", "medal_inconceivable",
	"medal_unfriggenbelievable_row_wit_unfriggenbelievable",
	"medal_sniper_rifle_w_spree", "medal_sharpshooter_rifle_w_sharpshooter", "medal_shotgun_shotgun_spree",
	"medal_splatter_row_wit_spree", "medal_vehicle_row_wit_manslaughter", "medal_juggernaut_spree",
	"medal_killjoy", "medal_revenge", "medal_first_strike", "medal_perfect", "medal_perfection",
	"medal_extermination", "medal_hail_to_the_king", "medal_immovable_object", "medal_open_season",
	"medal_flag_captured", "medal_flag_returned", "medal_flag_taken" };

int ae_announcer_pack_speaks_medal(char const *key)
{
	size_t index;

	if (!key)
		return 0;
	for (index = 0; index < sizeof(spoken_medals) / sizeof(spoken_medals[0]); index++)
		if (!strcmp(key, spoken_medals[index]))
			return 1;
	return 0;
}
