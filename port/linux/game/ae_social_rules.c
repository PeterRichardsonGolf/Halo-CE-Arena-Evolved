/* The offline social career's EXP and ranks (see the header). Pure code. */
#include "ae_social_rules.h"

/* table A: the EXP of rank 1 .. 50 */
static long const table_a[AE_SOCIAL_RANKS] =
{
	0, 1, 2, 3, 4, 5, 7, 10, 12, 15,
	20, 25, 30, 35, 40, 50, 60, 70, 85, 100,
	110, 120, 135, 150, 160, 170, 185, 200, 225, 250,
	275, 300, 325, 350, 375, 400, 425, 450, 475, 500,
	600, 700, 800, 900, 1000, 1200, 1400, 1600, 1800, 2000
};

/* the overall rank's table */
static long const table_overall[AE_SOCIAL_RANKS] =
{
	0, 2, 5, 8, 10, 12, 18, 25, 30, 40,
	50, 60, 75, 90, 100, 125, 150, 175, 200, 250,
	275, 300, 350, 375, 400, 425, 450, 500, 550, 625,
	700, 750, 800, 875, 950, 1000, 1050, 1100, 1150, 1200,
	1500, 1800, 2000, 2250, 2500, 3000, 3500, 4000, 4500, 5000
};

int ae_social_outcome_team(int first_alone, int first_shared)
{
	return first_alone ? AE_SOCIAL_WIN : first_shared ? AE_SOCIAL_TIE : AE_SOCIAL_LOSS;
}

int ae_social_outcome_ffa(int place, int players)
{
	int winners = players / 2;

	if (winners < 1)
		winners = 1;
	return place >= 0 && place < winners ? AE_SOCIAL_WIN : AE_SOCIAL_LOSS;
}

int ae_social_exp_change(int outcome)
{
	return outcome == AE_SOCIAL_WIN || outcome == AE_SOCIAL_TIE ? 1 : outcome == AE_SOCIAL_QUIT ? -1 : 0;
}

long ae_social_exp_after(long exp, int outcome)
{
	exp += ae_social_exp_change(outcome);
	if (exp < 0)
		exp = 0;
	if (exp > AE_SOCIAL_EXP_MAX)
		exp = AE_SOCIAL_EXP_MAX;
	return exp;
}

static long rank_exp_of(long const *table, int rank, int table_b)
{
	if (rank < 1 || rank > AE_SOCIAL_RANKS)
		return 0;
	if (table_b && rank > 40)
		return table[39] + 25L * (rank - 40);
	return table[rank - 1];
}

static int rank_of(long const *table, long exp, int table_b)
{
	int rank = 1;

	while (rank < AE_SOCIAL_RANKS && exp >= rank_exp_of(table, rank + 1, table_b))
		rank++;
	return rank;
}

static long to_next(long const *table, long exp, int table_b)
{
	int rank = rank_of(table, exp, table_b);

	return rank >= AE_SOCIAL_RANKS ? 0 : rank_exp_of(table, rank + 1, table_b) - exp;
}

static int percent(long const *table, long exp, int table_b)
{
	int rank = rank_of(table, exp, table_b);
	long from, span;

	if (rank >= AE_SOCIAL_RANKS)
		return 100;
	from = rank_exp_of(table, rank, table_b);
	span = rank_exp_of(table, rank + 1, table_b) - from;
	return (int)((exp - from) * 100 / span);
}

long ae_social_rank_exp(int rank)
{
	return rank_exp_of(table_a, rank, AE_SOCIAL_TABLE_B);
}

int ae_social_rank(long exp)
{
	return rank_of(table_a, exp, AE_SOCIAL_TABLE_B);
}

long ae_social_exp_to_next(long exp)
{
	return to_next(table_a, exp, AE_SOCIAL_TABLE_B);
}

int ae_social_rank_percent(long exp)
{
	return percent(table_a, exp, AE_SOCIAL_TABLE_B);
}

long ae_social_overall_rank_exp(int rank)
{
	return rank_exp_of(table_overall, rank, 0);
}

int ae_social_overall_rank(long exp)
{
	return rank_of(table_overall, exp, 0);
}

long ae_social_overall_exp_to_next(long exp)
{
	return to_next(table_overall, exp, 0);
}

int ae_social_overall_percent(long exp)
{
	return percent(table_overall, exp, 0);
}
