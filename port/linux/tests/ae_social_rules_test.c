#include <stdio.h>
#include "ae_social_rules.h"

static int failures;
#define EQ(a, b) do { long x_ = (long)(a), y_ = (long)(b); if (x_ != y_) { printf("FAIL %s:%d %s = %ld, want %ld\n", __FILE__, __LINE__, #a, x_, y_); failures++; } } while (0)

int main(void)
{
	int rank, outcome, players, place;
	long exp;

	/* a profile lost to a game's teardown is put back; a guest, one never had, or one still held is not */
	EQ(ae_social_profile_put_back(0x1234, -1, 0), 1);
	EQ(ae_social_profile_put_back(0x1234, 0x1234, 0), 0);
	EQ(ae_social_profile_put_back(-1, -1, 0), 0);
	EQ(ae_social_profile_put_back(0x1234, -1, 1), 0);

	/* every outcome's EXP */
	EQ(ae_social_exp_change(AE_SOCIAL_WIN), 1);
	EQ(ae_social_exp_change(AE_SOCIAL_TIE), 1);
	EQ(ae_social_exp_change(AE_SOCIAL_LOSS), 0);
	EQ(ae_social_exp_change(AE_SOCIAL_QUIT), -1);
	/* the floor and the cap */
	EQ(ae_social_exp_after(0, AE_SOCIAL_QUIT), 0);
	EQ(ae_social_exp_after(5, AE_SOCIAL_QUIT), 4);
	EQ(ae_social_exp_after(AE_SOCIAL_EXP_MAX, AE_SOCIAL_WIN), AE_SOCIAL_EXP_MAX);
	/* team outcomes */
	EQ(ae_social_outcome_team(1, 0), AE_SOCIAL_WIN);
	EQ(ae_social_outcome_team(0, 1), AE_SOCIAL_TIE);
	EQ(ae_social_outcome_team(0, 0), AE_SOCIAL_LOSS);
	/* FFA of N players, places 0 .. N-1: the top half, at least one */
	for (players = 1; players <= 16; players++)
	{
		int winners = players / 2 < 1 ? 1 : players / 2;

		for (place = 0; place < players; place++)
		{
			outcome = ae_social_outcome_ffa(place, players);
			EQ(outcome, place < winners ? AE_SOCIAL_WIN : AE_SOCIAL_LOSS);
		}
	}
	EQ(ae_social_outcome_ffa(3, 8), AE_SOCIAL_WIN);
	EQ(ae_social_outcome_ffa(4, 8), AE_SOCIAL_LOSS);
	EQ(ae_social_outcome_ffa(1, 5), AE_SOCIAL_WIN);
	EQ(ae_social_outcome_ffa(2, 5), AE_SOCIAL_LOSS);
	EQ(ae_social_outcome_ffa(0, 2), AE_SOCIAL_WIN);
	EQ(ae_social_outcome_ffa(1, 2), AE_SOCIAL_LOSS);
	EQ(ae_social_outcome_ffa(-1, 8), AE_SOCIAL_LOSS);
	/* table A, every rank: the rank at its EXP, one below one short of it, strictly rising */
	for (rank = 1; rank <= AE_SOCIAL_RANKS; rank++)
	{
		exp = ae_social_rank_exp(rank);
		EQ(ae_social_rank(exp), rank);
		if (rank > 1)
		{
			EQ(ae_social_rank(exp - 1), rank - 1);
			EQ(ae_social_rank_exp(rank) > ae_social_rank_exp(rank - 1), 1);
		}
	}
	EQ(ae_social_rank_exp(1), 0);
	EQ(ae_social_rank_exp(0), 0);
	EQ(ae_social_rank_exp(51), 0);
	/* the spec's anchors: wins to rank 10 = 15, rank 25 = 160, rank 50 = 2000 */
	EQ(ae_social_rank_exp(10), 15);
	EQ(ae_social_rank_exp(25), 160);
	EQ(ae_social_rank_exp(50), 2000);
	/* 37 EXP: rank 14, 3 to rank 15 */
	EQ(ae_social_rank(37), 14);
	EQ(ae_social_exp_to_next(37), 3);
	EQ(ae_social_rank(0), 1);
	EQ(ae_social_exp_to_next(0), 1);
	EQ(ae_social_rank(1), 2);
	EQ(ae_social_rank(99999), 50);
	EQ(ae_social_exp_to_next(2000), 0);
	EQ(ae_social_exp_to_next(5000), 0);
	EQ(ae_social_rank_percent(2000), 100);
	EQ(ae_social_rank_percent(35), 0);
	EQ(ae_social_rank_percent(37), 40);
	EQ(ae_social_rank_percent(0), 0);
	/* the overall table: 10 = 40, 25 = 400, 50 = 5000; 312 EXP is rank 22 */
	EQ(ae_social_overall_rank_exp(10), 40);
	EQ(ae_social_overall_rank_exp(25), 400);
	EQ(ae_social_overall_rank_exp(50), 5000);
	EQ(ae_social_overall_rank(312), 22);
	EQ(ae_social_overall_rank(253), 20);
	EQ(ae_social_overall_exp_to_next(312), 38);
	EQ(ae_social_overall_rank(AE_SOCIAL_OVERALL_EXP_MAX), 50);
	for (rank = 2; rank <= AE_SOCIAL_RANKS; rank++)
		EQ(ae_social_overall_rank(ae_social_overall_rank_exp(rank)), rank);
	if (failures)
		return 1;
	printf("ae_social_rules_test: ok\n");
	return 0;
}
