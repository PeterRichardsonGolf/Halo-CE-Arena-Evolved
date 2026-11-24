/* ae_shield_pop_rules.c: when COMBAT FEEDBACK's early shield pop shows (ae_shield_pop_rules.h). */

#include "ae_shield_pop_rules.h"

enum ae_shield_pop_hit ae_shield_pop_hit_kind(int headshots, int profile, int no_spill, int rounds_a_pull)
{
	if (profile && no_spill)
		return _ae_shield_pop_hit_no_spill;
	if (profile && rounds_a_pull > 1)
		return _ae_shield_pop_hit_burst;
	return headshots ? _ae_shield_pop_hit_headshot : _ae_shield_pop_hit_plain;
}

float ae_shield_pop_points(enum ae_shield_pop_hit kind, float finisher_points, float hit_shield_damage)
{
	switch (kind)
	{
	case _ae_shield_pop_hit_no_spill:
		return 0.f;
	case _ae_shield_pop_hit_headshot:
		if (!(hit_shield_damage > 0.f))
			return 0.f;
		return hit_shield_damage < finisher_points ? hit_shield_damage : finisher_points;
	default:
		return finisher_points;
	}
}

int ae_shield_pop_shows(float shield_left, float points, float margin)
{
	return shield_left <= points + margin;
}

int ae_shield_pop_fires(int was_down, int down, float shield_before, float shield_now)
{
	return down && !was_down && shield_now < shield_before;
}
