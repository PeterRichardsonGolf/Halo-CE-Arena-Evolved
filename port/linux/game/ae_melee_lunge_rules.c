/*
AE_MELEE_LUNGE_RULES.C

See ae_melee_lunge_rules.h.
*/

#include "ae_melee_lunge_rules.h"
#include "ae_variant_ext_rules.h"

int ae_melee_lunge_params_get(int rule, struct ae_melee_lunge_params *params)
{
	params->reach = 0.0f;
	params->minimum = 0.8f;
	params->cone_degrees = 15.0f;
	params->speed = 0.0f;
	params->turn_degrees = 0.0f;
	params->cooldown_ticks = AE_MELEE_LUNGE_TICKS_PER_SECOND;
	params->ease = 0.375f;
	if (rule == AE_EXT_MELEE_LUNGE_AE)
	{
		params->reach = 1.1f;
		params->speed = 1.5f;
		params->turn_degrees = 1.5f;
		return 1;
	}
	if (rule == AE_EXT_MELEE_LUNGE_STRONG)
	{
		params->reach = 1.05f;
		params->speed = 4.0f;
		params->turn_degrees = 4.0f;
		return 1;
	}
	return 0;
}

char const *ae_melee_lunge_name(int rule)
{
	return rule == AE_EXT_MELEE_LUNGE_AE ? "ae" : rule == AE_EXT_MELEE_LUNGE_STRONG ? "strong" : "off";
}

enum ae_melee_lunge_verdict ae_melee_lunge_verdict(struct ae_melee_lunge_params const *params, float distance, long now,
	long next_time)
{
	if (distance <= params->minimum)
		return _ae_melee_lunge_plain;
	if (now < next_time)
		return _ae_melee_lunge_too_soon;
	return _ae_melee_lunge_go;
}

float ae_melee_lunge_step(struct ae_melee_lunge_params const *params, float gap, float arrival)
{
	float step = gap - arrival;
	float per_tick = params->speed / (float)AE_MELEE_LUNGE_TICKS_PER_SECOND;

	if (step > params->ease * gap)
		step = params->ease * gap;
	if (step > per_tick)
		step = per_tick;
	return step > 0.0f ? step : 0.0f;
}
