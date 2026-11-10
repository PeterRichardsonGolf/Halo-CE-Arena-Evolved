/*
AE_BOTS_LIVE_RULES.C

The bots of a local game in progress: the rules (ae_bots_live_rules.h).
*/

#include "ae_bots_live_rules.h"

#include <stdlib.h>

struct ae_bots_live_change ae_bots_live_decide(int wanted, int maximum, int const *bot_team, int teams, int against,
	int people_team, int red_count, int blue_count, int room)
{
	struct ae_bots_live_change change = { AE_BOTS_LIVE_NONE, -1, 0 };
	int present = 0;
	int index;

	if (people_team < 0 || wanted < 0)
		wanted = 0;
	if (wanted > maximum)
		wanted = maximum;
	for (index = 0; index < maximum; index++)
		present += bot_team[index] >= 0;

	if (present > wanted)
	{
		int larger = red_count > blue_count ? 0 : blue_count > red_count ? 1 : -1;

		change.kind = AE_BOTS_LIVE_REMOVE;
		/* (the last bot of the larger team, the teams even) */
		if (teams && !against && larger >= 0)
		{
			for (index = maximum - 1; index >= 0 && change.bot < 0; index--)
			{
				if (bot_team[index] == larger)
					change.bot = index;
			}
		}
		for (index = maximum - 1; index >= 0 && change.bot < 0; index--)
		{
			if (bot_team[index] >= 0)
				change.bot = index;
		}
		change.team = bot_team[change.bot];
		return change;
	}
	if (present < wanted && room)
	{
		for (index = 0; index < maximum; index++)
		{
			if (bot_team[index] < 0)
				break;
		}
		change.kind = AE_BOTS_LIVE_ADD;
		change.bot = index;
		if (!teams)
			change.team = 0;
		else if (against)
			change.team = people_team == 0 ? 1 : 0;
		else
			change.team = blue_count < red_count ? 1 : 0;
	}
	return change;
}

int ae_bots_live_team_move(int count, int const *bot_team, int teams, int against, int people_team, int red_count,
	int blue_count, int *team)
{
	int from, index;

	*team = -1;
	if (!teams || !bot_team || count <= 0)
		return -1;
	if (against)
	{
		if (people_team != 0 && people_team != 1)
			return -1;
		from = people_team;
	}
	else
	{
		/* (within 1 of each other: as even as the players allow) */
		if (red_count - blue_count >= 2)
			from = 0;
		else if (blue_count - red_count >= 2)
			from = 1;
		else
			return -1;
	}
	for (index = count - 1; index >= 0; index--)
	{
		if (bot_team[index] == from)
		{
			*team = 1 - from;
			return index;
		}
	}
	return -1;
}

int ae_bots_live_test_step(char const *text, int step, double *second, int *count)
{
	char const *at = text;
	int index;

	if (!text || step < 0)
		return 0;
	for (index = 0; index < step; index++)
	{
		while (*at && *at != ',')
			at++;
		if (!*at)
			return 0;
		at++;
	}
	{
		char *end;
		double value = strtod(at, &end);
		long number;

		if (end == at || *end != ':')
			return 0;
		at = end + 1;
		number = strtol(at, &end, 10);
		if (end == at || (*end && *end != ','))
			return 0;
		*second = value;
		*count = (int)number;
	}
	return 1;
}
