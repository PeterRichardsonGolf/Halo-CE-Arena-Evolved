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

int ae_bots_team_human_squad(int humans, int bots)
{
	if (humans <= 0 || bots <= 0)
		return 0;
	if (humans > 1)
		return bots >= humans * 4 ? 2 : 1;
	return bots >= 6 ? 3 : bots >= 4 ? 2 : 1;
}

int ae_bots_team_human_squad_leaving_crews(int squad, int able_bots)
{
	int spare = able_bots - AE_BOTS_TEAM_FLAG_CREWS_BOTS;

	if (squad <= 1)
		return squad;
	return spare < 1 ? 1 : spare < squad ? spare : squad;
}

int ae_bots_team_mini_squads(int free_bots, int team_bots)
{
	if (team_bots < AE_BOTS_TEAM_MINI_SQUADS_FROM || free_bots < 2)
		return 0;
	return free_bots >= 6 ? free_bots / 3 : free_bots / 2;
}

int ae_bots_team_mini_squad_of(int ordinal, int free_bots, int squads)
{
	return squads > 0 && free_bots > 0 ? ordinal * squads / free_bots : 0;
}

int ae_bots_team_mini_squad_route(int squad, long epoch, int team)
{
	return (int)(((long)squad + epoch + (long)(team & 1) * 2) % AE_BOTS_ROUTES);
}
