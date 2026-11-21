/*
AE_PARTY_TEAMS_RULES.C

See ae_party_teams_rules.h.
*/

#include "ae_party_teams_rules.h"

int ae_party_join_team(struct ae_party_player const *players, int count, int party, int party_size, int team_cap)
{
	int in_party[2] = { 0, 0 }, total[2] = { 0, 0 }, people[2] = { 0, 0 };
	int index, smaller, pass;

	if (!players || count < 0 || count > AE_PARTY_PLAYERS_MAX)
		return -1;
	if (party_size < 1)
		party_size = 1;
	for (index = 0; index < count; index++)
	{
		int team = players[index].team;

		if (team != 0 && team != 1)
			continue;
		total[team]++;
		if (!players[index].bot)
			people[team]++;
		if (players[index].party == party && !players[index].bot)
			in_party[team]++;
	}
	if (in_party[0] || in_party[1])
	{
		int team = in_party[1] > in_party[0] ? 1 : 0;

		return total[team] < team_cap ? team : -1;
	}
	smaller = total[1] < total[0] ? 1 : 0;
	/* (room for all, then room once the bots give way) */
	for (pass = 0; pass < 2; pass++)
	{
		int const *held = pass ? people : total;

		if (held[smaller] + party_size <= team_cap)
			return smaller;
		if (held[1 - smaller] + party_size <= team_cap)
			return 1 - smaller;
	}
	return -1;
}

int ae_party_balance(struct ae_party_player *players, int count)
{
	int moved = 0, rounds;

	if (!players || count < 0 || count > AE_PARTY_PLAYERS_MAX)
		return 0;
	for (rounds = 0; rounds < AE_PARTY_PLAYERS_MAX; rounds++)
	{
		int total[2] = { 0, 0 }, index, big, difference, best_size = 0, best_party = 0, best_result = 0;

		for (index = 0; index < count; index++)
			if (players[index].team == 0 || players[index].team == 1)
				total[players[index].team]++;
		difference = total[0] - total[1];
		big = difference > 0 ? 0 : 1;
		if (difference < 0)
			difference = -difference;
		if (difference <= 1)
			break;
		for (index = 0; index < count; index++)
		{
			int other, size = 0, result;

			if (players[index].team != big)
				continue;
			for (other = 0; other < index; other++)
				if (players[other].team == big && players[other].party == players[index].party)
					break;
			if (other < index)
				continue;
			for (other = index; other < count; other++)
				if (players[other].team == big && players[other].party == players[index].party)
					size++;
			if (size >= difference)
				continue;
			result = difference - 2 * size;
			if (result < 0)
				result = -result;
			/* (the closest result; of equals, the later party, as the old balance moved the bigger team's last player) */
			if (!best_size || result <= best_result)
				best_size = size, best_party = players[index].party, best_result = result;
		}
		if (!best_size)
			break;
		for (index = 0; index < count; index++)
			if (players[index].team == big && players[index].party == best_party)
				players[index].team = 1 - big;
		moved += best_size;
	}
	return moved;
}

void ae_party_deal(int const *party, int count, unsigned long seed, signed char *teams)
{
	int order[AE_PARTY_PLAYERS_MAX], lead[AE_PARTY_PLAYERS_MAX], size[AE_PARTY_PLAYERS_MAX];
	int groups = 0, index, total[2] = { 0, 0 }, first;

	if (!party || !teams || count <= 0)
		return;
	if (count > AE_PARTY_PLAYERS_MAX)
		count = AE_PARTY_PLAYERS_MAX;
	for (index = 0; index < count; index++)
		order[index] = index;
	/* (Fisher-Yates, with the small LCG of the plain deal: the game's random numbers are its simulation's) */
	for (index = count - 1; index > 0; index--)
	{
		int other, held;

		seed = (seed * 1664525ul + 1013904223ul) & 0xFFFFFFFFul;
		other = (int)((seed >> 8) % (unsigned long)(index + 1));
		held = order[index];
		order[index] = order[other];
		order[other] = held;
	}
	seed = (seed * 1664525ul + 1013904223ul) & 0xFFFFFFFFul;
	first = (int)((seed >> 16) & 1);
	/* (the parties in the shuffled order of their first player, the biggest first, equals as shuffled) */
	for (index = 0; index < count; index++)
	{
		int group, player = order[index];

		for (group = 0; group < groups; group++)
			if (party[lead[group]] == party[player])
				break;
		if (group == groups)
			lead[groups] = player, size[groups++] = 0;
		size[group]++;
	}
	for (index = 1; index < groups; index++)
	{
		int at = index, held_lead = lead[index], held_size = size[index];

		while (at > 0 && size[at - 1] < held_size)
		{
			lead[at] = lead[at - 1];
			size[at] = size[at - 1];
			at--;
		}
		lead[at] = held_lead;
		size[at] = held_size;
	}
	for (index = 0; index < groups; index++)
	{
		int team = total[0] == total[1] ? first : total[1] < total[0] ? 1 : 0, player;

		for (player = 0; player < count; player++)
			if (party[player] == party[lead[index]])
				teams[player] = (signed char)team;
		total[team] += size[index];
	}
}

int ae_party_death_may_move(int party_size, int own, int other)
{
	return party_size <= 1 && own >= other + 2;
}

int ae_party_group_size(struct ae_party_player const *players, int count, int party, int team)
{
	int index, size = 0;

	if (!players || count < 0 || count > AE_PARTY_PLAYERS_MAX)
		return 0;
	for (index = 0; index < count; index++)
		if (players[index].party == party && players[index].team == team && !players[index].bot)
			size++;
	return size;
}

int ae_party_follow(struct ae_party_player *players, int count, int sender, int old_team, int new_team)
{
	int index, moved = 0;

	if (!players || count < 0 || count > AE_PARTY_PLAYERS_MAX || sender < 0 || sender >= count || players[sender].bot ||
		(old_team != 0 && old_team != 1) || (new_team != 0 && new_team != 1) || old_team == new_team)
	{
		return 0;
	}
	for (index = 0; index < count; index++)
	{
		if (index != sender && !players[index].bot && players[index].party == players[sender].party &&
			players[index].team == old_team)
		{
			players[index].team = new_team;
			moved++;
		}
	}
	return moved;
}
