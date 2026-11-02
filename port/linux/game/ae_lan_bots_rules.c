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

int ae_lan_bots_start_extra(int teams, int against, int bots, int other_players, int maximum_players, int capacity,
	int playerless_machines)
{
	if (!teams || against || bots <= 0 || ((bots + other_players) & 1) == 0 || playerless_machines > 0)
		return 0;
	return bots < capacity && bots + other_players < maximum_players ? 1 : 0;
}

int ae_lan_bots_ingame_target(int wanted, int eligible, int other_players, int maximum_players, int capacity, int teams,
	int against)
{
	int count = wanted;

	if (!eligible || wanted <= 0)
		return 0;
	if (count > capacity)
		count = capacity;
	if (count > maximum_players - other_players)
		count = maximum_players - other_players;
	if (count <= 0)
		return 0;
	if (teams && !against && ((count + other_players) & 1) && count < capacity && count + other_players < maximum_players)
		count++;
	return count;
}

int ae_lan_bots_ingame_capacity(int bots, int free_seats, int new_machines)
{
	int count = (bots > 0 ? bots : 0) + (free_seats > 0 ? free_seats : 0);

	if (new_machines > AE_NET_BOTS_MACHINES)
		new_machines = AE_NET_BOTS_MACHINES;
	if (new_machines > 0)
		count += new_machines * AE_LAN_BOTS_CONTROLLERS;
	return count > AE_NET_BOTS_MAXIMUM ? AE_NET_BOTS_MAXIMUM : count;
}

int ae_lan_bots_ingame_step(int bots, int target)
{
	int step = target - bots;

	if (step > AE_LAN_BOTS_CONTROLLERS)
		return AE_LAN_BOTS_CONTROLLERS;
	if (step < -AE_LAN_BOTS_CONTROLLERS)
		return -AE_LAN_BOTS_CONTROLLERS;
	return step;
}

int ae_lan_bots_ingame_wait(int last, int next)
{
	if ((last > 0 && next > 0) || (last < 0 && next < 0))
		return AE_LAN_BOTS_STEP_TICKS;
	return AE_LAN_BOTS_SETTLE_TICKS;
}

int ae_lan_bots_leaving_team(int teams, int red_bots, int blue_bots, int red_players, int blue_players)
{
	int first;

	if (!teams || (red_bots <= 0 && blue_bots <= 0))
		return -1;
	first = blue_players > red_players ? 1 : 0;
	if ((first ? blue_bots : red_bots) > 0)
		return first;
	return first ? 0 : 1;
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

void ae_lan_bots_teams(int count, int const *chosen, int against, int people_team, int red_count, int blue_count,
	int *teams)
{
	int index, counts[2];

	counts[0] = red_count;
	counts[1] = blue_count;
	for (index = 0; index < count; index++)
	{
		if (chosen && (chosen[index] == 0 || chosen[index] == 1))
		{
			teams[index] = chosen[index];
			counts[chosen[index]]++;
		}
		else
			teams[index] = -1;
	}
	for (index = 0; index < count; index++)
	{
		if (teams[index] < 0)
		{
			teams[index] = ae_lan_bots_team(against, people_team, counts[0], counts[1]);
			counts[teams[index]]++;
		}
	}
}

void ae_lan_bots_name(char const *base, int number, int tagged, char *name, int size)
{
	static char const tag[] = "BOT ";
	int length = 0, room, index;

	if (!name || size <= 0)
		return;
	room = (size - 1 < AE_LAN_BOTS_NAME_LENGTH ? size - 1 : AE_LAN_BOTS_NAME_LENGTH) - (number ? 1 : 0);
	for (index = 0; tagged && tag[index] && length < room; index++)
		name[length++] = tag[index];
	for (index = 0; base && base[index] && length < room; index++)
		name[length++] = base[index];
	if (number && length < size - 1)
		name[length++] = (char)('0' + number % 10);
	name[length] = 0;
}
