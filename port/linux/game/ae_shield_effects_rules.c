/*
AE_SHIELD_EFFECTS_RULES.C

ae_shield_effects_rules.h has the rules.
*/

#include "ae_shield_effects_rules.h"

int ae_shield_effects_own(
	int network_server,
	int accepts_remote)
{
	return network_server && !accepts_remote;
}

int ae_arm_shield_log_due(
	float shield,
	int charging,
	unsigned long now_ms,
	unsigned long last_ms)
{
	if (!(shield > 1.f) && !charging)
		return 0;
	return last_ms == 0 || now_ms - last_ms >= 1000UL;
}

char const *ae_shield_effects_name(
	int effects)
{
	static char const *const names[] = { "stock", "glow", "light_show", "overshield", "glow_overshield",
		"light_show_overshield", "players" };

	if (effects < 0)
		return "players' own";
	return effects < (int)(sizeof(names) / sizeof(names[0])) ? names[effects] : "?";
}
