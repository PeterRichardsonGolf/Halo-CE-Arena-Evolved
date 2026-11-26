/* ae_randomizer_rules.h */

#include "ae_randomizer_rules.h"

int ae_randomizer_rules_firefight_game(int next_variant_is_firefight, int network_variant_is_firefight)
{
	return next_variant_is_firefight != 0 || network_variant_is_firefight != 0;
}

int ae_randomizer_rules_plays(int firefight, int firing_range, int network, int solo_scenario)
{
	if (firefight)
		return 1;
	if (firing_range)
		return network == 0;
	if (network)
		return 0;

	return solo_scenario != 0;
}
