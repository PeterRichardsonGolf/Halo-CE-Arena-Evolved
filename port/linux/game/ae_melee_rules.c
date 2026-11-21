/*
AE_MELEE_RULES.C

See ae_melee_rules.h.
*/

#include <math.h>

#include "ae_melee_rules.h"
#include "ae_variant_ext_rules.h"

int ae_melee_rule_on(int rule)
{
	return rule == AE_EXT_MELEE_AE;
}

float ae_melee_blow_points(int rule, float points, float maximum_body, float scale)
{
	float share;

	if (!ae_melee_rule_on(rule))
		return points;
	share = AE_MELEE_BODY_SHARE * maximum_body * scale;
	return share > points ? share : points;
}

float ae_melee_body_after(float blow, float shield_left)
{
	return blow > shield_left ? blow - shield_left : 0.0f;
}

float ae_melee_reach_forward(void)
{
	return 0.163f;
}

void ae_melee_origin_from(float px, float py, float head_z, float aim_x, float aim_y, float out[3])
{
	float length = sqrtf(aim_x * aim_x + aim_y * aim_y);
	float reach = length > 0.0001f ? ae_melee_reach_forward() / length : 0.0f;

	out[0] = px + aim_x * reach;
	out[1] = py + aim_y * reach;
	out[2] = head_z;
}

int ae_melee_bleed_mode(int rule, int is_melee, int row_mode)
{
	return (is_melee && ae_melee_rule_on(rule)) ? AE_EXT_SHIELD_BLEED_HALO2 : row_mode;
}

float ae_melee_bleed_excess(float excess, float raw, float unraised)
{
	if (raw < 0.0001f || unraised < 0.0001f)
		return excess;
	return excess * raw / unraised;
}

float ae_melee_kill_points(float points, float maximum_body)
{
	if (maximum_body > 0.0f && points >= maximum_body - 0.001f && points < maximum_body + 0.001f)
		return maximum_body + 0.001f;
	return points;
}

int ae_melee_body_raise(int counted_by_shield, float body_points)
{
	return !counted_by_shield && body_points > 0.0f;
}

int ae_melee_kills_from_behind(int rule, int is_melee, int enemies, int seated, int unsuspecting)
{
	return ae_melee_rule_on(rule) && is_melee && enemies && !seated && unsuspecting;
}

float ae_melee_pop_points(int rule, float finisher_points)
{
	/* (the owner, 11-09: AE's own pop, COMBAT FEEDBACK's early one at the finisher's points, stays under both: a
	shield it pops is one a melee finishes, as the AE blow kills through as much) */
	(void)rule;
	return finisher_points;
}

int ae_melee_from_behind(float dir_x, float dir_y, float look_x, float look_y)
{
	float dir_length = sqrtf(dir_x * dir_x + dir_y * dir_y);
	float look_length = sqrtf(look_x * look_x + look_y * look_y);

	if (dir_length < 0.0001f || look_length < 0.0001f)
		return 0;
	return (dir_x * look_x + dir_y * look_y) / (dir_length * look_length) >= AE_MELEE_BEHIND_COS;
}
