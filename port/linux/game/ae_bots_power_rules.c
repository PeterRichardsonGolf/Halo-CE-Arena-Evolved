/*
AE_BOTS_POWER_RULES.C

Arena Evolved: the bots' power items (ae_bots_power_rules.h).
*/

#include "ae_bots_power_rules.h"
#include "ae_bots_combat_rules.h"

float ae_bots_power_reach(
	int level)
{
	static const float reach[AE_BOTS_LEVELS] = { 8.0f, 12.0f, 25.0f, 45.0f, 70.0f, 1.0e9f };

	return level >= 0 && level < AE_BOTS_LEVELS ? reach[level] : 0.0f;
}

float ae_bots_power_lead_seconds(
	int level)
{
	static const float lead[AE_BOTS_LEVELS] = { 0.0f, 0.0f, 0.0f, 5.0f, 10.0f, 15.0f };

	return level >= 0 && level < AE_BOTS_LEVELS ? lead[level] : 0.0f;
}

float ae_bots_power_value(
	int kind)
{
	static const float value[AE_BOTS_POWER_KINDS] = { 10.0f, 9.0f, 6.0f, 10.0f, 8.0f };

	return kind >= 0 && kind < AE_BOTS_POWER_KINDS ? value[kind] : 0.0f;
}

float ae_bots_power_want(
	struct ae_bots_power_view const *view)
{
	float reach, travel;

	if (!view || view->known_taken || view->holds_it || view->teammate_going || ae_bots_power_value(view->kind) <= 0.0f)
		return 0.0f;
	reach = ae_bots_power_reach(view->level);
	if (view->fighting && reach > AE_BOTS_POWER_FIGHTING_REACH)
		reach = AE_BOTS_POWER_FIGHTING_REACH;
	if (view->distance > reach)
		return 0.0f;
	if (!view->up)
	{
		float lead = ae_bots_power_lead_seconds(view->level);

		travel = view->distance / AE_BOTS_POWER_RUN_SPEED;
		/* (not up: only a level that times spawns, and not before it would arrive its lead early) */
		if (lead <= 0.0f || view->seconds_left > travel + lead)
			return 0.0f;
	}
	return ae_bots_power_value(view->kind) / (1.0f + view->distance / 20.0f);
}
