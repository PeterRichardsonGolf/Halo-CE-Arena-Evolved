/*
AE_TEAM_SHUFFLE_RULES.C

The SHUFFLE's deal (ae_team_shuffle.h), pure: the unit tests build it.
*/

#include "ae_team_shuffle.h"

void ae_team_shuffle_deal(int count, unsigned long seed, signed char *teams)
{
	int order[AE_TEAM_SHUFFLE_MAXIMUM];
	int index;

	if (count <= 0)
		return;
	if (count > AE_TEAM_SHUFFLE_MAXIMUM)
		count = AE_TEAM_SHUFFLE_MAXIMUM;
	for (index = 0; index < count; index++)
		order[index] = index;
	/* (Fisher-Yates, with a small LCG of its own: the game's random numbers are its simulation's) */
	for (index = count - 1; index > 0; index--)
	{
		int other, held;

		seed = (seed * 1664525ul + 1013904223ul) & 0xFFFFFFFFul;
		other = (int)((seed >> 8) % (unsigned long)(index + 1));
		held = order[index];
		order[index] = order[other];
		order[other] = held;
	}
	/* (the first half of the shuffled order on one team, the rest on the other; which team has the first half, and so
	an odd one out, is dealt too) */
	seed = (seed * 1664525ul + 1013904223ul) & 0xFFFFFFFFul;
	for (index = 0; index < count; index++)
		teams[order[index]] = (signed char)((index < (count + 1) / 2 ? 0 : 1) ^ (int)((seed >> 16) & 1));
}
