/*
SERVICE_RANK.H

Compartmentalised MCC Service Rank / Career Leveling System (11 Tours x 42 Ranks = 462 Levels).
- Awards XP for Campaign, Firefight, and Multiplayer kills, medals, completions,
  Watanoge's Daily Challenge (community.c), and Daily/Weekly mode challenges.
- Excludes Halo 3 Ranked Mode (1-50 CSR).
- Can be disabled or removed cleanly via OPENCE_ENABLE_SERVICE_RANK in opence_features.h.
*/

#ifndef __SERVICE_RANK_H
#define __SERVICE_RANK_H

#include "cseries.h"
#include "opence_features.h"
#include "halo_firefight.h"

#if OPENCE_ENABLE_SERVICE_RANK

void service_rank_compute_from_xp(
	long total_xp,
	struct firefight_mcc_rank_info *out_rank);

long service_rank_apply_xp_gain(
	long current_xp,
	long xp_amount,
	char const *reason,
	boolean announce_hud);

void service_rank_on_daily_challenge_completed(
	long score);

#else

static __inline void service_rank_compute_from_xp(
	long total_xp,
	struct firefight_mcc_rank_info *out_rank)
{
	(void)total_xp;
	if (out_rank)
	{
		csmemset(out_rank, 0, sizeof(*out_rank));
		out_rank->tour_number = 1;
		out_rank->tour_name = "Tour 1";
		out_rank->rank_in_tour = 1;
		out_rank->rank_name = "Recruit";
		out_rank->xp_for_next_rank = 2100L;
	}
}

static __inline long service_rank_apply_xp_gain(
	long current_xp,
	long xp_amount,
	char const *reason,
	boolean announce_hud)
{
	(void)xp_amount;
	(void)reason;
	(void)announce_hud;
	return current_xp;
}

static __inline void service_rank_on_daily_challenge_completed(
	long score)
{
	(void)score;
}

#endif /* OPENCE_ENABLE_SERVICE_RANK */

#endif /* __SERVICE_RANK_H */
