/*
AE_BOTS_TEAM_RULES.C

Arena Evolved: the bots playing as a team (ae_bots_team_rules.h).
*/

#include "ae_bots_team_rules.h"
#include "ae_bots_combat_rules.h"

int ae_bots_team_stance(
	int level,
	int allies_alive,
	int enemies_alive)
{
	int lead = allies_alive - enemies_alive;

	if (level < AE_BOTS_LEVEL_NORMAL || level >= AE_BOTS_LEVELS)
		return 0;
	return lead <= -2 ? -1 : lead >= 2 ? 1 : 0;
}

float ae_bots_team_outnumbered_penalty(
	int level,
	int enemies_near,
	int allies_near)
{
	int extra = enemies_near - allies_near;
	float each, penalty;

	if (level < AE_BOTS_LEVEL_NORMAL || level >= AE_BOTS_LEVELS || extra <= 0)
		return 0.0f;
	each = level == AE_BOTS_LEVEL_NORMAL ? 0.2f : level == AE_BOTS_LEVEL_HEROIC ? 0.25f : 0.3f;
	penalty = each * (float)extra;
	return penalty > 0.6f ? 0.6f : penalty;
}

int ae_bots_team_wingman_level(
	int level)
{
	return level >= AE_BOTS_LEVEL_EASY && level < AE_BOTS_LEVELS;
}

int ae_bots_team_wingman_move(
	float distance)
{
	return distance > AE_BOTS_WINGMAN_FAR ? 1 : 2;
}
