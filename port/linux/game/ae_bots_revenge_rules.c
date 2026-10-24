/*
AE_BOTS_REVENGE_RULES.C

Arena Evolved: REVENGE, a bot hunting the player who keeps killing it (ae_bots_revenge_rules.h).
*/

#include "ae_bots_revenge_rules.h"
#include "ae_bots_combat_rules.h"

void ae_bots_revenge_clear(
	struct ae_bots_revenge *state)
{
	state->killer = -1;
	state->streak = 0;
	state->nemesis = -1;
	state->until_tick = 0;
}

int ae_bots_revenge_death(
	struct ae_bots_revenge *state,
	int killer,
	int now,
	int ticks_per_second)
{
	if (killer == -1)
		return 0;
	if (killer == state->killer)
		state->streak++;
	else
	{
		state->killer = killer;
		state->streak = 1;
	}
	if (state->streak < AE_BOTS_REVENGE_DEATHS)
		return 0;
	/* (the count starts again: three more in a row after this hunt are another one) */
	state->streak = 0;
	state->until_tick = now + AE_BOTS_REVENGE_SECONDS * ticks_per_second;
	if (state->nemesis == killer)
		return 0;
	state->nemesis = killer;
	return 1;
}

int ae_bots_revenge_kill(
	struct ae_bots_revenge *state,
	int victim)
{
	if (victim == -1)
		return 0;
	if (victim == state->killer)
	{
		state->killer = -1;
		state->streak = 0;
	}
	if (victim != state->nemesis)
		return 0;
	state->nemesis = -1;
	state->until_tick = 0;
	return 1;
}

int ae_bots_revenge_nemesis(
	struct ae_bots_revenge *state,
	int now)
{
	if (state->nemesis != -1 && now >= state->until_tick)
		state->nemesis = -1;
	return state->nemesis;
}

int ae_bots_revenge_style(
	int level)
{
	if (level >= AE_BOTS_LEVEL_LEGENDARY)
		return AE_BOTS_REVENGE_SMART;
	if (level == AE_BOTS_LEVEL_HEROIC)
		return AE_BOTS_REVENGE_DIRECT;
	return AE_BOTS_REVENGE_RECKLESS;
}

float ae_bots_revenge_confidence(
	int level)
{
	switch (ae_bots_revenge_style(level))
	{
	case AE_BOTS_REVENGE_RECKLESS: return 0.3f;
	case AE_BOTS_REVENGE_DIRECT: return 0.1f;
	default: return 0.0f;
	}
}
