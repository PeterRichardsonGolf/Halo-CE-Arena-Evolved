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

float ae_bots_team_exposed_penalty(
	int level,
	int exposed)
{
	float each, penalty;

	if (level < AE_BOTS_LEVEL_HEROIC || level >= AE_BOTS_LEVELS || exposed <= 1)
		return 0.0f;
	each = level == AE_BOTS_LEVEL_HEROIC ? 0.1f : level == AE_BOTS_LEVEL_LEGENDARY ? 0.15f : 0.2f;
	penalty = each * (float)(exposed - 1);
	return penalty > 0.6f ? 0.6f : penalty;
}

float ae_bots_team_hearing_range(
	int level)
{
	static const float range[AE_BOTS_LEVELS] = { 0.0f, 0.0f, 15.0f, 20.0f, 25.0f, 30.0f };

	return level >= 0 && level < AE_BOTS_LEVELS ? range[level] : 0.0f;
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
