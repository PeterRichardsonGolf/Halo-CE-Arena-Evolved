/*
AE_BOTS_COMBAT_RULES.C

The bots' fight by level as pure rules (ae_bots_combat_rules.h): every number here is ours, measured on the build
box (.superpowers/sdd/2026-10-16-bots/combat-report.md), none Halo Infinite's (it published none but its 200-250 ms
reaction budget).
*/

#include "ae_bots_combat_rules.h"

#define TICKS AE_BOTS_COMBAT_TICKS_PER_SECOND

static float clamp01(float value)
{
	return value < 0.0f ? 0.0f : value > 1.0f ? 1.0f : value;
}

static float clamp(float value, float low, float high)
{
	return value < low ? low : value > high ? high : value;
}

float ae_bots_combat_hash01(unsigned long a, unsigned long b)
{
	/* (a 32-bit mix, the same on 32- and 64-bit builds) */
	unsigned long x = (a * 0x9E3779B1UL + b * 0x85EBCA77UL + 0x165667B1UL) & 0xFFFFFFFFUL;

	x ^= x >> 15;
	x = (x * 0x2C1B3C6DUL) & 0xFFFFFFFFUL;
	x ^= x >> 12;
	x = (x * 0x297A2D39UL) & 0xFFFFFFFFUL;
	x ^= x >> 15;
	return (float)(x & 0xFFFFFFUL) / (float)0x1000000UL;
}

long ae_bots_combat_reaction_ticks(long skill_ticks, float hash01)
{
	return (long)((float)skill_ticks * (0.85f + 0.5f * clamp01(hash01)) + 0.5f);
}

long ae_bots_combat_look_jitter(float hash01)
{
	long jitter = (long)(clamp01(hash01) * 3.0f);

	return jitter > 2 ? 2 : jitter;
}

float ae_bots_combat_confidence(float own_shield, float own_body, float target_shield, float target_body,
	float distance, float weapon_range, float loaded, float height_above)
{
	float own = 0.6f * clamp(own_shield, 0.0f, 2.0f) + 0.4f * clamp01(own_body);
	float target = 0.6f * clamp(target_shield, 0.0f, 2.0f) + 0.4f * clamp01(target_body);
	float confidence = 0.5f + 0.5f * (own - target);

	if (loaded < 0.15f)
		confidence -= 0.15f;
	if (weapon_range > 0.0f && distance > weapon_range * 1.5f)
		confidence -= 0.05f;
	confidence += clamp(height_above / 5.0f, -0.1f, 0.1f);
	return clamp01(confidence);
}

/* 0: not now (out of reach, or waiting); 1: strike; 2: it chose not to (asked again after a second) */
int ae_bots_combat_melee(int level, float distance, int cooldown_over, int back_turned, float target_shield,
	int charging, float random)
{
	float reach = 1.1f, chance = 1.0f;

	if (!cooldown_over)
		return 0;
	switch (level)
	{
	case AE_BOTS_LEVEL_RECRUIT:
		reach = 0.8f;
		chance = 0.4f;
		break;
	case AE_BOTS_LEVEL_EASY:
		chance = 0.8f;
		break;
	case AE_BOTS_LEVEL_NORMAL:
		break;
	default:
		/* (HEROIC and up: a blow when it tells, else only at point blank) */
		if (!back_turned && target_shield >= 0.35f && !charging)
			reach = 0.6f;
		break;
	}
	if (distance >= reach)
		return 0;
	return random < chance ? 1 : 2;
}

long ae_bots_combat_melee_cooldown(int level, float random)
{
	float seconds;

	random = clamp01(random);
	switch (level)
	{
	case AE_BOTS_LEVEL_RECRUIT: seconds = 3.0f + 1.5f * random; break;
	case AE_BOTS_LEVEL_EASY: seconds = 2.0f + random; break;
	case AE_BOTS_LEVEL_NORMAL: seconds = 1.0f + 0.5f * random; break;
	default: seconds = 0.9f + 0.4f * random; break;
	}
	return (long)(seconds * TICKS);
}

int ae_bots_combat_about_to_die(float own_shield, float own_body)
{
	return own_shield <= 0.02f && own_body < 0.6f;
}

int ae_bots_combat_grenade(int level, float distance, int has_grenade, int reacted, int first_contact,
	int about_to_die, float skill_chance, float random)
{
	float low = 6.0f, high = 22.0f, rate;

	if (!has_grenade || !reacted)
		return 0;
	switch (level)
	{
	case AE_BOTS_LEVEL_RECRUIT:
		/* (rarely, and only far off, where it seldom lands) */
		low = 10.0f;
		rate = 0.015f;
		break;
	case AE_BOTS_LEVEL_EASY:
		rate = about_to_die ? 0.6f : 0.02f;
		break;
	case AE_BOTS_LEVEL_NORMAL:
		/* (bots.c's rate: its table's chance, three times a second's) */
		rate = skill_chance * 3.0f;
		break;
	default:
		rate = skill_chance * 3.0f + (first_contact ? 0.5f : 0.0f) + (about_to_die ? 0.6f : 0.0f);
		break;
	}
	if (distance <= low || distance >= high)
		return 0;
	return random < rate / (float)TICKS;
}

long ae_bots_combat_grenade_cooldown(int level, float random)
{
	float seconds;

	random = clamp01(random);
	switch (level)
	{
	case AE_BOTS_LEVEL_RECRUIT: seconds = 20.0f + 10.0f * random; break;
	case AE_BOTS_LEVEL_EASY: seconds = 6.0f + 3.0f * random; break;
	case AE_BOTS_LEVEL_NORMAL: seconds = 4.0f + 2.0f * random; break;
	default: seconds = 3.5f + 2.0f * random; break;
	}
	return (long)(seconds * TICKS);
}

float ae_bots_combat_grenade_lead(int level)
{
	switch (level)
	{
	case AE_BOTS_LEVEL_RECRUIT:
	case AE_BOTS_LEVEL_EASY:
		return 0.0f;
	case AE_BOTS_LEVEL_NORMAL:
		return 0.5f;
	case AE_BOTS_LEVEL_HEROIC:
		return 0.8f;
	default:
		return 1.0f;
	}
}

int ae_bots_combat_aim_head(int level, float target_shield, int precision_weapon)
{
	return level >= AE_BOTS_LEVEL_NORMAL && target_shield <= 0.0f && precision_weapon;
}

float ae_bots_combat_retreat_threshold(int level)
{
	switch (level)
	{
	case AE_BOTS_LEVEL_NORMAL: return 0.22f;
	case AE_BOTS_LEVEL_HEROIC: return 0.27f;
	case AE_BOTS_LEVEL_LEGENDARY: return 0.30f;
	default: return 0.0f;
	}
}

void ae_bots_combat_retreat_reset(struct ae_bots_combat_retreat *state)
{
	state->active = 0;
	state->low_since = -1;
	state->started = 0;
	state->next_allowed = 0;
}

int ae_bots_combat_retreat_step(struct ae_bots_combat_retreat *state, int level, float confidence, float own_shield,
	long now, float hesitation_hash01, int *started, int *ended)
{
	float threshold = ae_bots_combat_retreat_threshold(level);
	long hesitation = 6 + (long)(12.0f * clamp01(hesitation_hash01));

	*started = 0;
	*ended = 0;
	if (state->active)
	{
		long length = now - state->started;

		if (length >= 7 * TICKS || (length >= TICKS * 3 / 2 && (own_shield >= 0.95f || confidence >= 0.6f)) ||
			threshold <= 0.0f)
		{
			state->active = 0;
			state->next_allowed = now + 6 * TICKS;
			state->low_since = -1;
			*ended = 1;
		}
		return state->active;
	}
	if (threshold <= 0.0f || confidence >= threshold || own_shield >= 0.3f || now < state->next_allowed)
	{
		state->low_since = -1;
		return 0;
	}
	if (state->low_since < 0 || state->low_since > now)
		state->low_since = now;
	if (now - state->low_since < hesitation)
		return 0;
	state->active = 1;
	state->started = now;
	*started = 1;
	return 1;
}

int ae_bots_combat_charge(int level, float confidence, float target_shield, float target_body, float distance)
{
	return level >= AE_BOTS_LEVEL_HEROIC && confidence >= 0.6f && target_shield <= 0.05f && target_body < 0.5f &&
		distance < 6.0f && distance > 0.8f;
}

int ae_bots_combat_strafe_jump(int level, float random)
{
	return (level == AE_BOTS_LEVEL_HEROIC && random < 0.3f) || (level >= AE_BOTS_LEVEL_LEGENDARY && random < 0.45f);
}

int ae_bots_combat_shares_sightings(int level)
{
	return level >= AE_BOTS_LEVEL_HEROIC;
}
