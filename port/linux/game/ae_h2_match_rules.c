#include "ae_h2_match_rules.h"

int ae_h2_rounds_go_on(int rounds, int to_win, int resolve_ties, int played, int best_wins, int runner_up_wins)
{
	if (rounds <= 1)
		return 0;
	if (to_win > 0 && best_wins >= to_win)
		return 0;
	if (played < rounds)
		return 1;
	/* (all the rounds are played) */
	return resolve_ties && best_wins == runner_up_wins;
}

int ae_h2_unique_leader(long const *scores, int count)
{
	int best = -1;
	int ties = 0;
	int index;

	for (index = 0; index < count; index++)
	{
		if (best < 0 || scores[index] > scores[best])
			best = index, ties = 0;
		else if (scores[index] == scores[best])
			ties++;
	}
	return best >= 0 && !ties ? best : -1;
}

int ae_h2_bonus_points(int victim_led, int spree)
{
	return (victim_led ? 1 : 0) + (spree > 0 && spree % 5 == 0 ? 1 : 0);
}

int ae_h2_may_spawn(int max_active, int alive, int ahead_waiting)
{
	if (max_active <= 0)
		return 1;
	return alive < max_active && ahead_waiting == 0;
}

int ae_h2_end_is_round(int final_end, int rounds)
{
	return !final_end && rounds > 1;
}

int ae_h2_slot_of(long absolute_index, int slots)
{
	return absolute_index >= 0 && absolute_index < slots ? (int)absolute_index : -1;
}
