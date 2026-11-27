/*
AE_SOCIAL_RULES.H

The offline social career's EXP and ranks (0.9.3). Pure code (no engine headers): port/linux/tests/ae_social_rules_test.c runs it.

EXP: a win +1, a team tie +1, a loss 0, leaving before the end -1. A team game is won by the team that finished first alone
and tied by a team that shares first place. A free for all gives +1 to the top half of the players (a shared place counts as
its best). EXP never goes below 0. The visible rank is a plain number 1 .. 50 read from the EXP through table A (a rank is
won at its EXP); the OVERALL rank reads the EXP of every playlist added up through its own table.
*/

#ifndef __AE_SOCIAL_RULES_H
#define __AE_SOCIAL_RULES_H

enum
{
	AE_SOCIAL_RANKS = 50,
	/* what EXP a playlist (and the sum) can hold */
	AE_SOCIAL_EXP_MAX = 99999,
	AE_SOCIAL_OVERALL_EXP_MAX = 999999,
	/* a game's outcome for EXP */
	AE_SOCIAL_LOSS = 0,
	AE_SOCIAL_WIN = 1,
	AE_SOCIAL_TIE = 2,
	AE_SOCIAL_QUIT = 3
};

/* the rank table of ranks 41 .. 50: table A (the default) steps 100, 100, 100, 100, 200 ... ; table B (1 = on, the
   alternative) is +25 a rank from rank 40 on. A named constant, not a setting */
#ifndef AE_SOCIAL_TABLE_B
#define AE_SOCIAL_TABLE_B 0
#endif

/* a team game's outcome: won (first alone), tied (first shared) or neither */
int ae_social_outcome_team(int first_alone, int first_shared);
/* a free for all's outcome: place is the zero-based place (a shared place counts as its best), players the count of players.
   The top half (at least one) wins */
int ae_social_outcome_ffa(int place, int players);
/* the EXP an outcome moves: +1 win or tie, 0 loss, -1 quit */
int ae_social_exp_change(int outcome);
/* whether a local player's saved profile is put back after a game: the game's teardown clears every local player's profile
(saved: its index before the game, -1 for none; now: its index after; guest: the player is a guest, who has no profile to
put back); only a profile the player had and lost is */
int ae_social_profile_put_back(long saved, long now, int guest);
/* the EXP after an outcome: never below 0, never past AE_SOCIAL_EXP_MAX */
long ae_social_exp_after(long exp, int outcome);

/* the EXP at which a rank (1 .. 50) starts; 0 out of range */
long ae_social_rank_exp(int rank);
/* the rank (1 .. 50) of this EXP */
int ae_social_rank(long exp);
/* the EXP still to the next rank (a win is +1 EXP, so also "wins to"); 0 at rank 50 */
long ae_social_exp_to_next(long exp);
/* how far the EXP is through its rank, 0 .. 100 (100 at rank 50) */
int ae_social_rank_percent(long exp);

/* the same for the sum of every playlist's EXP: the overall rank's table */
long ae_social_overall_rank_exp(int rank);
int ae_social_overall_rank(long exp);
long ae_social_overall_exp_to_next(long exp);
int ae_social_overall_percent(long exp);

#endif
