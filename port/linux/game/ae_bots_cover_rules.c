/*
AE_BOTS_COVER_RULES.C

Arena Evolved: an objective carrier hides and holds (ae_bots_cover_rules.h).
*/

#include "ae_bots_cover_rules.h"
#include "ae_bots_combat_rules.h"

static int ae_bots_cover_level(int level)
{
	return level < AE_BOTS_LEVEL_RECRUIT ? AE_BOTS_LEVEL_RECRUIT : level >= AE_BOTS_LEVELS ? AE_BOTS_LEVEL_DEMON : level;
}

static int ae_bots_cover_bits(unsigned walls)
{
	int count = 0, side;

	for (side = 0; side < AE_BOTS_COVER_SIDES; side++)
		count += (walls >> side) & 1u;
	return count;
}

int ae_bots_cover_score(unsigned walls, int roof)
{
	int count = ae_bots_cover_bits(walls), corners = 0, side, score;

	if (count < 2 && !roof)
		return 0;
	for (side = 0; side < AE_BOTS_COVER_SIDES; side++)
	{
		if (((walls >> side) & 1u) && ((walls >> ((side + 1) % AE_BOTS_COVER_SIDES)) & 1u))
			corners++;
	}
	score = count + (roof ? 3 : 0) + (corners > 3 ? 3 : corners);
	/* (an alcove: walled on five sides or more) */
	if (count >= 5)
		score += 2;
	return score;
}

int ae_bots_cover_open_side(unsigned walls)
{
	unsigned all = (1u << AE_BOTS_COVER_SIDES) - 1u;
	int best = -1, best_run = 0, side;

	walls &= all;
	if (walls == all)
		return -1;
	if (walls == 0u)
		return 0;
	/* (the middle of the longest run of open sides, round the circle: each run starts after a wall) */
	for (side = 0; side < AE_BOTS_COVER_SIDES; side++)
	{
		int run = 0;

		if (((walls >> side) & 1u) || !((walls >> ((side + AE_BOTS_COVER_SIDES - 1) % AE_BOTS_COVER_SIDES)) & 1u))
			continue;
		while (!((walls >> ((side + run) % AE_BOTS_COVER_SIDES)) & 1u))
			run++;
		if (run > best_run)
		{
			best_run = run;
			best = (side + (run - 1) / 2) % AE_BOTS_COVER_SIDES;
		}
	}
	return best;
}

float ae_bots_cover_spot_score(int level, float own_distance, float threat_distance, int cover, int allies_near,
	int through, float chance)
{
	static float const jitter[AE_BOTS_LEVELS] = { 14.0f, 10.0f, 5.0f, 3.0f, 1.5f, 0.5f };
	static float const cover_weight[AE_BOTS_LEVELS] = { 0.5f, 0.8f, 1.2f, 1.5f, 1.8f, 2.0f };
	float score, threat;

	level = ae_bots_cover_level(level);
	/* (away from the threat first, up to the tracker's reach; then near, covered, by teammates) */
	threat = threat_distance < 0.0f ? 25.0f : threat_distance > 25.0f ? 25.0f : threat_distance;
	score = threat * 1.5f - own_distance * 0.6f + (float)cover * cover_weight[level] + (float)(allies_near > 2 ? 2 : allies_near) * 2.0f;
	/* (a spot by an enemy is no hiding place; nor one through them) */
	if (threat_distance >= 0.0f && threat_distance < AE_BOTS_COVER_THREAT_NEAR)
		score -= 20.0f;
	if (through)
		score -= 15.0f;
	return score + chance * jitter[level];
}

float ae_bots_cover_seen_penalty(int level, int seen_by)
{
	static float const each[AE_BOTS_LEVELS] = { 4.0f, 6.0f, 10.0f, 14.0f, 18.0f, 20.0f };

	return seen_by <= 0 ? 0.0f : each[ae_bots_cover_level(level)] * (float)seen_by;
}

int ae_bots_cover_candidates(int level)
{
	static int const counts[AE_BOTS_LEVELS] = { 2, 3, 5, 7, 9, 12 };

	return counts[ae_bots_cover_level(level)];
}

long ae_bots_cover_hold_ticks(int level)
{
	static long const seconds[AE_BOTS_LEVELS] = { 8, 12, 20, 24, 28, 30 };

	return seconds[ae_bots_cover_level(level)] * 30L;
}

long ae_bots_cover_watch_ticks(int level)
{
	static long const ticks[AE_BOTS_LEVELS] = { 30, 24, 15, 10, 8, 6 };

	return ticks[ae_bots_cover_level(level)];
}

int ae_bots_cover_leave(int level, float nearest_enemy, int seen_by, int seen_by_near, int hurt, long held_ticks,
	int camouflaged)
{
	float near = camouflaged ? AE_BOTS_COVER_THREAT_CAMO : AE_BOTS_COVER_THREAT_NEAR;

	if (nearest_enemy >= 0.0f && nearest_enemy < near)
		return 1;
	if (camouflaged ? seen_by_near > 0 : seen_by > 0)
		return 2;
	if (hurt)
		return 3;
	if (held_ticks >= ae_bots_cover_hold_ticks(level))
		return 4;
	return 0;
}
