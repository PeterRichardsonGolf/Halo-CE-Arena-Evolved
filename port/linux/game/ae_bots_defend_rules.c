/*
AE_BOTS_DEFEND_RULES.C

Arena Evolved: a defended point held with cover, spacing and morale (ae_bots_defend_rules.h).
*/

#include <math.h>
#include "ae_bots_defend_rules.h"
#include "ae_bots_combat_rules.h"
#include "ae_bots_cover_rules.h"

static int ae_defend_level(int level)
{
	return level < AE_BOTS_LEVEL_RECRUIT ? AE_BOTS_LEVEL_RECRUIT : level >= AE_BOTS_LEVELS ? AE_BOTS_LEVEL_DEMON : level;
}

static int ae_defend_bits(unsigned mask)
{
	int count = 0, side;

	for (side = 0; side < AE_BOTS_COVER_SIDES; side++)
		count += (mask >> side) & 1u;
	return count;
}

int ae_defend_profile(int largest_team)
{
	return largest_team >= 6 ? AE_DEFEND_BTB : AE_DEFEND_ARENA;
}

void ae_defend_numbers_of(int profile, struct ae_defend_numbers *out)
{
	if (profile == AE_DEFEND_BTB)
	{
		out->radius = 12.0f;
		out->spacing = 4.5f;
		out->leash = 2.0f;
		out->morale_decay_seconds = 6.0f;
		out->sniper_weight = 2.0f;
		out->vehicle_block = 1;
		out->jink_min_ticks = 15;
		out->jink_max_ticks = 30;
	}
	else
	{
		out->radius = 8.0f;
		out->spacing = 3.0f;
		out->leash = 2.0f;
		out->morale_decay_seconds = 4.0f;
		out->sniper_weight = 1.0f;
		out->vehicle_block = 0;
		out->jink_min_ticks = 9;
		out->jink_max_ticks = 21;
	}
}

int ae_defend_in_scope(int switch_on, int profile, int explicit_order, short order_kind, int role)
{
	if (!switch_on)
		return 0;
	/* (a leader's own call: DEFEND only, WATCH is left as it was) */
	if (explicit_order)
		return order_kind == _ae_order_defend;
	/* (a role's hold: BTB only; an ARENA game's roles are untouched) */
	if (profile != AE_DEFEND_BTB || order_kind != _ae_order_defend)
		return 0;
	return role == AE_ROLE_DEFEND_OURS || role == AE_ROLE_SITE_A || role == AE_ROLE_HILL;
}

unsigned ae_defend_threat_sides(float const *threat_yaws, int count)
{
	unsigned mask = 0u;
	int index;

	for (index = 0; index < count; index++)
	{
		float x = threat_yaws[index] / (3.14159265f / 4.0f);
		int side;

		/* (the nearest side, round the circle) */
		x += x < 0.0f ? -0.5f : 0.5f;
		side = (int)x;
		side = ((side % AE_BOTS_COVER_SIDES) + AE_BOTS_COVER_SIDES) % AE_BOTS_COVER_SIDES;
		mask |= 1u << side;
		mask |= 1u << ((side + 1) % AE_BOTS_COVER_SIDES);
		mask |= 1u << ((side + AE_BOTS_COVER_SIDES - 1) % AE_BOTS_COVER_SIDES);
	}
	return mask;
}

float ae_defend_place_score(unsigned walls, int roof, unsigned threat_sides, float point_distance, float radius, int sees_way)
{
	unsigned all = (1u << AE_BOTS_COVER_SIDES) - 1u;
	int toward, elsewhere;
	float score;

	if (radius <= 0.0f || point_distance > radius)
		return 0.0f;
	walls &= all;
	threat_sides &= all;
	toward = ae_defend_bits(walls & threat_sides);
	elsewhere = ae_defend_bits(walls & ~threat_sides);
	/* (open ground is no place) */
	if (toward + elsewhere == 0 && !roof)
		return 0.0f;
	score = (float)(toward * 3 + elsewhere) + (roof ? 3.0f : 0.0f) + (sees_way ? 4.0f : 0.0f);
	/* (closer is better, falling to nothing at the area's edge) */
	score *= 1.0f - point_distance / radius;
	return score > 0.0f ? score : 0.01f;
}

int ae_defend_spaced(float nearest_other_place, float spacing)
{
	return nearest_other_place < 0.0f || nearest_other_place >= spacing;
}

int ae_defend_within_leash(float anchor_distance, float radius, float leash)
{
	return anchor_distance <= radius + leash;
}

float ae_defend_exposure(int seen_by, int seen_by_near, int hurt, int open_toward_seer)
{
	float value = 15.0f * (float)(seen_by < 0 ? 0 : seen_by) + 10.0f * (float)(seen_by_near < 0 ? 0 : seen_by_near);

	if (seen_by > 0 && hurt)
		value += 15.0f;
	else if (hurt)
		value += 8.0f;
	if (seen_by > 0 && open_toward_seer)
		value += 15.0f;
	return value > 70.0f ? 70.0f : value;
}

int ae_defend_exposed_step(int was_exposed, float exposure, long held_ticks, int level)
{
	/* (seconds a defender settles at a place before it can find it too exposed; the lower levels are slower to notice) */
	static float const settle[AE_BOTS_LEVELS] = { 2.5f, 2.0f, 1.5f, 1.0f, 0.8f, 0.6f };

	if ((float)held_ticks < settle[ae_defend_level(level)] * 30.0f)
		return 0;
	return was_exposed ? exposure >= 20.0f : exposure > 30.0f;
}

float ae_defend_sticky(float hold_score, long since_chosen_ticks, int level)
{
	static float const bonus[AE_BOTS_LEVELS] = { 0.2f, 0.3f, 0.4f, 0.5f, 0.5f, 0.5f };

	if (since_chosen_ticks > 3 * 30)
		return hold_score;
	return hold_score * (1.0f + bonus[ae_defend_level(level)]);
}

long ae_defend_jink_ticks(int profile, float chance)
{
	struct ae_defend_numbers numbers;

	ae_defend_numbers_of(profile, &numbers);
	chance = chance < 0.0f ? 0.0f : chance > 1.0f ? 1.0f : chance;
	return numbers.jink_min_ticks + (long)(chance * (float)(numbers.jink_max_ticks - numbers.jink_min_ticks));
}

int ae_defend_open_action(int profile, int shot_from_range, int line_breaker_known)
{
	(void)profile;
	return shot_from_range && line_breaker_known ? AE_DEFEND_OPEN_BREAK_LINE : AE_DEFEND_OPEN_JINK;
}

float ae_defend_morale_step(float morale, int members_down, int outnumbered_by, int leader_dead, float seconds,
	float decay_seconds)
{
	float rate;

	if (seconds < 0.0f)
		seconds = 0.0f;
	if (decay_seconds < 0.1f)
		decay_seconds = 0.1f;
	if (members_down < 0)
		members_down = 0;
	if (outnumbered_by < 0)
		outnumbered_by = 0;
	if (members_down > 4)
		members_down = 4;
	if (outnumbered_by > 4)
		outnumbered_by = 4;
	rate = 0.09f * (float)members_down + 0.07f * (float)outnumbered_by + (leader_dead ? 0.30f : 0.0f);
	morale = morale * expf(-seconds / decay_seconds) + rate * seconds;
	return morale < 0.0f ? 0.0f : morale > 1.0f ? 1.0f : morale;
}

int ae_defend_member_down(int seen_alive, int alive_now)
{
	return seen_alive && !alive_now;
}

/* (the squad members that must be back before a bot-only squad returns: half, at least two, a lone survivor of one alone) */
static int ae_defend_enough_back(int squad)
{
	int half = (squad + 1) / 2;

	if (squad < 2)
		return squad < 1 ? 1 : squad;
	return half < 2 ? 2 : half;
}

int ae_defend_phase_step(int phase, int human_led, int leader_alive, float morale, int at_fallback, int all_regrouped,
	int living, int squad, int back_at_anchor)
{
	switch (phase)
	{
	case AE_DEFEND_HOLD:
		if (morale > AE_DEFEND_MORALE_BREAK && (!human_led || !leader_alive))
			return AE_DEFEND_FALL_BACK;
		return AE_DEFEND_HOLD;
	case AE_DEFEND_FALL_BACK:
		return at_fallback ? AE_DEFEND_REGROUP : AE_DEFEND_FALL_BACK;
	case AE_DEFEND_REGROUP:
		if (!all_regrouped || morale >= AE_DEFEND_MORALE_SAFE)
			return AE_DEFEND_REGROUP;
		if (human_led ? !leader_alive : living < ae_defend_enough_back(squad))
			return AE_DEFEND_REGROUP;
		return AE_DEFEND_RETURN;
	case AE_DEFEND_RETURN:
		return back_at_anchor ? AE_DEFEND_HOLD : AE_DEFEND_RETURN;
	}
	return AE_DEFEND_HOLD;
}

int ae_defend_objective_break(int role_hold, int own_flag_dropped, int enemy_has_own_flag_in_sight, int bomb_to_defuse)
{
	return role_hold && (own_flag_dropped || enemy_has_own_flag_in_sight || bomb_to_defuse);
}
