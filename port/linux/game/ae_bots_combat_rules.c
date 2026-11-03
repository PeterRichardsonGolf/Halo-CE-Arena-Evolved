/*
AE_BOTS_COMBAT_RULES.C

The bots' fight by level as pure rules (ae_bots_combat_rules.h): every number here is ours, measured on the build
box (.superpowers/sdd/2026-10-16-bots/combat-report.md), none from elsewhere (only a 200-250 ms
reaction budget).
*/

#include "ae_bots_combat_rules.h"

#include <string.h>

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

long ae_bots_combat_reaction_ticks(int level, long skill_ticks, float hash01)
{
	/* (RECRUIT never quicker than its own time: it must not get harder) */
	float low = level == AE_BOTS_LEVEL_RECRUIT ? 1.0f : 0.85f;

	return (long)((float)skill_ticks * (low + 0.5f * clamp01(hash01)) + 0.5f);
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
		/* (HEROIC and up: a blow when it tells (shields low: under 0.1, LEGENDARY 0.4), else only at point
		blank) */
		if (!back_turned && target_shield >= (level >= AE_BOTS_LEVEL_LEGENDARY ? 0.4f : 0.1f) && !charging)
			reach = 0.6f;
		break;
	}
	if (distance >= reach)
		return 0;
	return random < chance ? 1 : 2;
}

long ae_bots_combat_melee_delay(int level, float random)
{
	float low, high;

	switch (level)
	{
	case AE_BOTS_LEVEL_RECRUIT: low = 1.0f; high = 3.0f; break;
	case AE_BOTS_LEVEL_EASY: low = 0.6f; high = 1.5f; break;
	case AE_BOTS_LEVEL_NORMAL: low = 0.35f; high = 0.7f; break;
	case AE_BOTS_LEVEL_HEROIC: low = 0.25f; high = 0.5f; break;
	case AE_BOTS_LEVEL_LEGENDARY: low = 0.18f; high = 0.35f; break;
	default: low = 0.1f; high = 0.3f; break;
	}
	return (long)((low + (high - low) * clamp01(random)) * TICKS + 0.5f);
}

long ae_bots_combat_melee_cooldown(int level, float random)
{
	float seconds;

	random = clamp01(random);
	switch (level)
	{
	case AE_BOTS_LEVEL_RECRUIT: seconds = 3.0f + 1.5f * random; break;
	case AE_BOTS_LEVEL_EASY: seconds = 2.0f + random; break;
	case AE_BOTS_LEVEL_NORMAL: seconds = 1.2f + 0.5f * random; break;
	case AE_BOTS_LEVEL_HEROIC: seconds = 1.0f + 0.5f * random; break;
	case AE_BOTS_LEVEL_LEGENDARY: seconds = 0.95f + 0.4f * random; break;
	default: seconds = 0.9f + 0.4f * random; break;
	}
	return (long)(seconds * TICKS);
}

int ae_bots_combat_zombie_melee(int level, float distance, int cooldown_over, float random)
{
	float reach = level == AE_BOTS_LEVEL_RECRUIT ? 0.9f : 1.1f;
	float chance = level == AE_BOTS_LEVEL_RECRUIT ? 0.6f : 1.0f;

	if (!cooldown_over || distance >= reach)
		return 0;
	return random < chance ? 1 : 2;
}

long ae_bots_combat_zombie_melee_delay(int level, float random)
{
	return ae_bots_combat_melee_delay(level, random) / 2;
}

long ae_bots_combat_zombie_melee_cooldown(int level, float random)
{
	return ae_bots_combat_melee_cooldown(level, random) * 3 / 5;
}

int ae_bots_combat_about_to_die(float own_shield, float own_body)
{
	return own_shield <= 0.02f && own_body < 0.6f;
}

int ae_bots_combat_grenade(int level, float distance, int has_grenade, int reacted, int first_contact,
	int about_to_die, float skill_chance, float random)
{
	float low = 6.0f, high = 18.0f, rate;

	if (!has_grenade || !reacted)
		return 0;
	switch (level)
	{
	case AE_BOTS_LEVEL_RECRUIT:
		/* (never: RECRUIT did not throw before, and it must not get harder; the owner to decide) */
		return 0;
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
	/* (RECRUIT kept slow, 5 s + up to 10: the children's level) */
	case AE_BOTS_LEVEL_RECRUIT: seconds = 20.0f + 10.0f * random; break;
	case AE_BOTS_LEVEL_EASY: seconds = 3.0f + 7.5f * random; break;
	case AE_BOTS_LEVEL_NORMAL: seconds = 1.0f + 5.0f * random; break;
	case AE_BOTS_LEVEL_HEROIC: seconds = 0.5f + 5.0f * random; break;
	default: seconds = 0.1f + 5.0f * random; break;
	}
	return (long)(seconds * TICKS);
}

float ae_bots_combat_grenade_lead(int level)
{
	switch (level)
	{
	/* (Modern games: Recruit and Marine at the feet, ODST and Spartan ahead of them) */
	case AE_BOTS_LEVEL_RECRUIT:
	case AE_BOTS_LEVEL_EASY:
	case AE_BOTS_LEVEL_NORMAL:
		return 0.0f;
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
	case AE_BOTS_LEVEL_NORMAL:
	case AE_BOTS_LEVEL_HEROIC:
	case AE_BOTS_LEVEL_LEGENDARY:
	case AE_BOTS_LEVEL_DEMON:
		return 0.1f;
	default:
		return 0.0f;
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
	long shortest = level >= AE_BOTS_LEVEL_HEROIC ? 3 * TICKS : 2 * TICKS;
	int popped;

	*started = 0;
	*ended = 0;
	if (state->active)
	{
		long length = now - state->started;

		if (length >= 7 * TICKS || (length >= shortest && (own_shield >= 0.95f || confidence >= 0.6f)) ||
			threshold <= 0.0f)
		{
			state->active = 0;
			state->next_allowed = now + 6 * TICKS;
			state->low_since = -1;
			*ended = 1;
		}
		return state->active;
	}
	/* (LEGENDARY and up: also as its shields pop, losing, with a shorter hesitation) */
	popped = level >= AE_BOTS_LEVEL_LEGENDARY && own_shield <= 0.0f && confidence < 0.35f;
	if (threshold <= 0.0f || now < state->next_allowed ||
		(!popped && (confidence >= threshold || own_shield >= 0.3f)))
	{
		state->low_since = -1;
		return 0;
	}
	if (state->low_since < 0 || state->low_since > now)
		state->low_since = now;
	if (popped)
		hesitation = 3 + (long)(6.0f * clamp01(hesitation_hash01));
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

float ae_bots_combat_vertical_half_view(int level)
{
	static const float halves[AE_BOTS_LEVELS] = { 22.0f, 25.0f, 28.0f, 32.0f, 36.0f, 40.0f };

	return level >= 0 && level < AE_BOTS_LEVELS ? halves[level] : 40.0f;
}

int ae_bots_combat_in_vertical_view(int level, float elevation_degrees, float pitch_degrees)
{
	float off = elevation_degrees - pitch_degrees;

	if (off < 0.0f)
		off = -off;
	return off <= ae_bots_combat_vertical_half_view(level);
}

int ae_bots_combat_step_in(int level, float distance, float target_shield)
{
	switch (level)
	{
	case AE_BOTS_LEVEL_EASY: return distance < 2.5f && target_shield < 0.5f;
	case AE_BOTS_LEVEL_NORMAL: return distance < 3.5f && target_shield < 0.8f;
	case AE_BOTS_LEVEL_HEROIC:
	case AE_BOTS_LEVEL_LEGENDARY:
	case AE_BOTS_LEVEL_DEMON: return distance < 4.0f;
	default: return 0;
	}
}

long ae_bots_combat_vertical_delay(int level, float random)
{
	float low, high;

	switch (level)
	{
	case AE_BOTS_LEVEL_NORMAL: low = 3.0f; high = 5.0f; break;
	case AE_BOTS_LEVEL_HEROIC: low = 1.0f; high = 4.0f; break;
	case AE_BOTS_LEVEL_LEGENDARY:
	case AE_BOTS_LEVEL_DEMON: low = 1.0f; high = 3.0f; break;
	default: return 0;
	}
	return (long)((low + (high - low) * clamp01(random)) * TICKS + 0.5f);
}

long ae_bots_combat_strafe_ticks(int level, float random)
{
	float low, high;

	switch (level)
	{
	/* (RECRUIT: bots.c's third of a second to a second and a third, three times as long: the owner's) */
	case AE_BOTS_LEVEL_RECRUIT: low = 1.0f; high = 4.0f; break;
	case AE_BOTS_LEVEL_EASY: low = 1.5f; high = 3.0f; break;
	case AE_BOTS_LEVEL_NORMAL: low = 2.0f; high = 4.0f; break;
	case AE_BOTS_LEVEL_HEROIC: low = 1.0f; high = 2.0f; break;
	default: low = 0.2f; high = 0.8f; break;
	}
	return (long)((low + (high - low) * clamp01(random)) * TICKS + 0.5f);
}

long ae_bots_combat_halt_ticks(int level, float chance_random, float length_random)
{
	float chance, low, high;

	switch (level)
	{
	/* (RECRUIT and EASY: none. A halt 0.9 of the time for 2-3 s would suit a Recruit, but a CE bot that stands fires
	far better (measured: RECRUIT's hits a shot 0.13 -> 0.56, a standing player's deaths 3.1 -> 4.2 a game), and
	RECRUIT must never get harder) */
	case AE_BOTS_LEVEL_RECRUIT:
	case AE_BOTS_LEVEL_EASY:
		return 0;
	case AE_BOTS_LEVEL_NORMAL: chance = 0.2f; low = 1.0f; high = 2.0f; break;
	case AE_BOTS_LEVEL_HEROIC: chance = 0.1f; low = 0.1f; high = 0.2f; break;
	default: chance = 0.05f; low = 0.05f; high = 0.1f; break;
	}
	if (chance_random >= chance)
		return 0;
	return (long)((low + (high - low) * clamp01(length_random)) * TICKS + 0.5f);
}

long ae_bots_combat_initial_halt_ticks(int level, float random)
{
	float low, high;

	switch (level)
	{
	/* (RECRUIT and EASY: none, as their halts) */
	case AE_BOTS_LEVEL_RECRUIT:
	case AE_BOTS_LEVEL_EASY:
		return 0;
	case AE_BOTS_LEVEL_LEGENDARY:
	case AE_BOTS_LEVEL_DEMON: low = 0.0f; high = 0.1f; break;
	default: low = 0.0f; high = 0.2f; break;
	}
	return (long)((low + (high - low) * clamp01(random)) * TICKS + 0.5f);
}

float ae_bots_combat_share_distance(int level)
{
	/* (Modern games: every level knows what teammates within 10 units know, Spartan its teammates' targets anywhere;
	ours: RECRUIT .. NORMAL only what they see; LEGENDARY twice HEROIC's, DEMON anywhere) */
	return level >= AE_BOTS_LEVEL_DEMON ? 1.0e9f : level == AE_BOTS_LEVEL_LEGENDARY ? 20.0f :
		level == AE_BOTS_LEVEL_HEROIC ? 10.0f : 0.0f;
}

float ae_bots_combat_range_error_floor(int level, float distance)
{
	static const float slopes[AE_BOTS_LEVELS] = { 3.0f, 2.6f, 2.0f, 1.6f, 1.2f, 0.0f };

	if (level < 0 || level >= AE_BOTS_LEVELS || distance <= 10.0f || distance > AE_BOTS_COMBAT_LONG_RANGE)
		return 0.0f;
	return slopes[level] * (distance - 10.0f) / 10.0f;
}

long ae_bots_combat_squad_flankers(int level, long squad_size)
{
	long small = level >= AE_BOTS_LEVEL_LEGENDARY ? 2 : 1;

	if (squad_size >= AE_BOTS_COMBAT_WIDE_SQUAD)
	{
		long third = (squad_size + 1) / 3;

		return third > 6 ? 6 : third > small ? third : small;
	}
	return small;
}

float ae_bots_combat_flank_seconds(long squad_size, float walk_distance)
{
	float seconds = walk_distance / 2.2f + 5.0f;

	if (squad_size < AE_BOTS_COMBAT_WIDE_SQUAD)
		return 10.0f;
	return seconds < 10.0f ? 10.0f : seconds > 25.0f ? 25.0f : seconds;
}

float ae_bots_combat_flank_unseen_seconds(long squad_size)
{
	return squad_size >= AE_BOTS_COMBAT_WIDE_SQUAD ? 8.0f : 2.0f;
}

float ae_bots_combat_long_range_hit_chance(int level, float distance)
{
	static const float chances[AE_BOTS_LEVEL_DEMON][4] =
	{
		{ 0.04f, 0.03f, 0.02f, 0.01f },
		{ 0.08f, 0.05f, 0.03f, 0.02f },
		{ 0.12f, 0.08f, 0.05f, 0.03f },
		{ 0.18f, 0.12f, 0.09f, 0.05f },
		{ 0.22f, 0.16f, 0.12f, 0.07f },
	};

	if (level < 0 || level >= AE_BOTS_LEVEL_DEMON || distance <= AE_BOTS_COMBAT_LONG_RANGE)
		return -1.0f;
	return chances[level][distance <= 20.0f ? 0 : distance <= 25.0f ? 1 : distance <= 30.0f ? 2 : 3];
}

float ae_bots_combat_near_error_floor(int level, float distance)
{
	if (level != AE_BOTS_LEVEL_NORMAL || distance <= 2.0f || distance > 10.0f)
		return 0.0f;
	return AE_BOTS_COMBAT_NEAR_FLOOR;
}

float ae_bots_combat_max_fire_distance(char const *weapon_name)
{
	if (weapon_name && strstr(weapon_name, "shotgun"))
		return 5.0f;
	if (weapon_name && strstr(weapon_name, "flamethrower"))
		return 4.0f;
	return 1.0e9f;
}

float ae_bots_combat_camo_reach_scale(int camouflaged)
{
	return camouflaged ? 0.4f : 1.0f;
}

float ae_bots_combat_camo_error_scale(int camouflaged)
{
	return camouflaged ? 2.0f : 1.0f;
}

float ae_bots_combat_camo_reaction_scale(int camouflaged)
{
	return camouflaged ? 2.0f : 1.0f;
}

int ae_bots_combat_weapon_spent(int magazine_count, int rounds_loaded_maximum, int rounds_total_maximum,
	float age)
{
	if (magazine_count > 0 && (rounds_loaded_maximum > 0 || rounds_total_maximum > 0))
		return -1;
	return age >= 0.999f;
}

int ae_bots_combat_pace_holds(int level, float distance, float trigger_error, long waited_ticks)
{
	static const float limits[AE_BOTS_LEVELS] = { 2.0f, 2.0f, 0.6f, 0.45f, 0.35f, 0.25f };
	static const long waits[AE_BOTS_LEVELS] = { 0, 0, 6, 8, 10, 12 };

	if (level < AE_BOTS_LEVEL_NORMAL || level >= AE_BOTS_LEVELS || distance <= 8.0f)
		return 0;
	return trigger_error > limits[level] && waited_ticks < waits[level];
}

int ae_bots_combat_zooms(int level, float distance)
{
	return level >= AE_BOTS_LEVEL_NORMAL && level < AE_BOTS_LEVELS && distance > 6.0f;
}

long ae_bots_combat_rescope_ticks(int level, float random)
{
	float low, high;

	switch (level)
	{
	case AE_BOTS_LEVEL_NORMAL: low = 0.6f; high = 0.8f; break;
	case AE_BOTS_LEVEL_HEROIC: low = 0.4f; high = 0.5f; break;
	case AE_BOTS_LEVEL_LEGENDARY: low = 0.25f; high = 0.35f; break;
	case AE_BOTS_LEVEL_DEMON: low = 0.15f; high = 0.22f; break;
	default: return 0;
	}
	return (long)((low + (high - low) * clamp01(random)) * TICKS + 0.5f);
}

int ae_bots_combat_descoped_holds_fire(float distance)
{
	return distance > 10.0f;
}

int ae_bots_combat_rifle_zooms(int level, float distance, float autoaim_distance)
{
	return level >= AE_BOTS_LEVEL_NORMAL && level < AE_BOTS_LEVELS && autoaim_distance > 0.0f &&
		distance > 0.5f * autoaim_distance;
}

float ae_bots_combat_high_ground(int level, float height_above)
{
	float credit;

	if (height_above <= 0.0f)
		return 0.0f;
	if (level == AE_BOTS_LEVEL_LEGENDARY)
	{
		credit = 1.5f * height_above;
		return credit < 4.5f ? credit : 4.5f;
	}
	if (level == AE_BOTS_LEVEL_DEMON)
	{
		credit = 2.0f * height_above;
		return credit < 6.0f ? credit : 6.0f;
	}
	return 0.0f;
}

int ae_bots_combat_prenade(int level, float seconds_unseen, float distance, int has_grenade, float random)
{
	float chance = level == AE_BOTS_LEVEL_DEMON ? 0.75f : level == AE_BOTS_LEVEL_LEGENDARY ? 0.5f : 0.0f;

	return has_grenade && seconds_unseen >= 0.6f && seconds_unseen <= 3.0f && distance >= 5.0f && distance <= 20.0f &&
		random < chance;
}

int ae_bots_combat_teammate_blocks(float shooter_x, float shooter_y, float shooter_z,
	float teammate_x, float teammate_y, float teammate_z, float target_x, float target_y, float target_z)
{
	float tx = target_x - shooter_x, ty = target_y - shooter_y, tz = target_z - shooter_z;
	float mx = teammate_x - shooter_x, my = teammate_y - shooter_y, mz = teammate_z - shooter_z;
	float target_squared = tx * tx + ty * ty + tz * tz;
	float along, dx, dy, dz, width_squared;

	if (target_squared < 0.01f)
		return 0;
	along = (mx * tx + my * ty + mz * tz) / target_squared;
	if (along <= 0.0f || along >= 1.0f)
		return 0;
	dx = mx - tx * along;
	dy = my - ty * along;
	dz = mz - tz * along;
	/* A standing player's body plus a narrow cone that opens about 1.8 degrees. */
	width_squared = 0.25f + 0.001f * along * along * target_squared;
	return dx * dx + dy * dy + dz * dz <= width_squared;
}

int ae_bots_combat_grenade_near_teammate(float landing_x, float landing_y, float landing_z,
	float teammate_x, float teammate_y, float teammate_z)
{
	float dx = teammate_x - landing_x, dy = teammate_y - landing_y, dz = teammate_z - landing_z;

	return dx * dx + dy * dy + dz * dz <= 12.25f;
}

int ae_bots_combat_crouch_jump(int airborne, long ticks_since_jump, float vertical_velocity)
{
	return airborne && ticks_since_jump >= 3 && ticks_since_jump <= 60 && vertical_velocity <= 0.004f;
}

float ae_bots_combat_threat_score(struct ae_bots_threat_view const *view)
{
	float score = 0.0f;
	float scale;

	if (view->on_gun)
		score += view->self_on_gun ? 26.0f : 16.0f;
	if (view->carries_objective)
		score += 14.0f;
	if (view->aiming_at_me)
		score += 6.0f;
	if (view->power_weapon)
		score += 5.0f;
	if (view->driving)
		score += 4.0f;
	scale = view->level <= AE_BOTS_LEVEL_RECRUIT ? 0.0f : view->level == AE_BOTS_LEVEL_EASY ? 0.4f :
		view->level == AE_BOTS_LEVEL_NORMAL ? 0.7f : 1.0f;
	return score * scale;
}
