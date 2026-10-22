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

int ae_lan_bots_capacity(int network_bots, unsigned free_mask, int machines)
{
	int count = 0;
	int controller;

	for (controller = 0; controller < AE_LAN_BOTS_CONTROLLERS; controller++)
		count += (free_mask >> controller) & 1u;
	if (count > AE_LAN_BOTS_MAXIMUM)
		count = AE_LAN_BOTS_MAXIMUM;
	if (!network_bots)
		return count;
	if (machines > AE_NET_BOTS_MACHINES)
		machines = AE_NET_BOTS_MACHINES;
	if (machines > 0)
		count += machines * AE_LAN_BOTS_CONTROLLERS;
	return count > AE_NET_BOTS_MAXIMUM ? AE_NET_BOTS_MAXIMUM : count;
}

int ae_lan_bots_target(int wanted, int eligible, int other_players, int maximum_players, int capacity)
{
	int count = wanted;
	int room = maximum_players - 1 - other_players;

	if (!eligible || wanted <= 0)
		return 0;
	if (count > capacity)
		count = capacity;
	if (count > room)
		count = room;
	return count < 0 ? 0 : count;
}

int ae_lan_bots_start_extra(int teams, int against, int bots, int other_players, int maximum_players, int capacity)
{
	if (!teams || against || bots <= 0 || ((bots + other_players) & 1) == 0)
		return 0;
	return bots < capacity && bots + other_players < maximum_players ? 1 : 0;
}

int ae_lan_bots_pick_machine(int const *counts, int count)
{
	int index;

	for (index = 0; index < count; index++)
	{
		if (counts[index] < AE_LAN_BOTS_CONTROLLERS)
			return index;
	}
	return count < AE_NET_BOTS_MACHINES ? count : -1;
}

int ae_lan_bots_name_number(int bot_index, int names)
{
	if (names <= 0 || bot_index < names)
		return 0;
	return bot_index / names + 1;
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
