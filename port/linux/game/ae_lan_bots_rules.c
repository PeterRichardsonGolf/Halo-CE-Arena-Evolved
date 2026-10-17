/*
AE_LAN_BOTS_RULES.C

The bots of a LAN game's rules (ae_lan_bots_rules.h).
*/

#include "ae_lan_bots_rules.h"

int ae_lan_bots_eligible(int pregame, int local_game, int internet, int played, int host_has_person)
{
	return pregame && !local_game && !internet && played && host_has_person;
}

unsigned ae_lan_bots_free_controllers(unsigned people_mask, unsigned wanting_mask)
{
	return ~(people_mask | wanting_mask) & ((1u << AE_LAN_BOTS_CONTROLLERS) - 1);
}

int ae_lan_bots_target(int wanted, int eligible, int other_players, int maximum_players, unsigned free_mask)
{
	int count = wanted;
	int free_count = 0;
	int room = maximum_players - 1 - other_players;
	int controller;

	if (!eligible || wanted <= 0)
		return 0;
	for (controller = 0; controller < AE_LAN_BOTS_CONTROLLERS; controller++)
		free_count += (free_mask >> controller) & 1u;
	if (count > AE_LAN_BOTS_MAXIMUM)
		count = AE_LAN_BOTS_MAXIMUM;
	if (count > free_count)
		count = free_count;
	if (count > room)
		count = room;
	return count < 0 ? 0 : count;
}

int ae_lan_bots_pick_controller(unsigned free_mask, unsigned bots_mask)
{
	int controller;

	for (controller = AE_LAN_BOTS_CONTROLLERS - 1; controller >= 0; controller--)
	{
		if (((free_mask & ~bots_mask) >> controller) & 1u)
			return controller;
	}
	return -1;
}

int ae_lan_bots_team(int against, int people_team, int red_count, int blue_count)
{
	if (against)
		return people_team == 1 ? 0 : 1;
	return blue_count < red_count ? 1 : 0;
}
