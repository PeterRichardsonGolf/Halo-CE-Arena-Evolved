/*
AE_REACH_HEALTH_RULES.C

HEALTH's REACH (ae_reach_health_rules.h): pure rules.
*/

#include "ae_reach_health_rules.h"

float ae_reach_health_cap(float health)
{
	if (health <= AE_REACH_HEALTH_STOP_LOW)
		return AE_REACH_HEALTH_STOP_LOW;
	if (health <= AE_REACH_HEALTH_STOP_HIGH)
		return AE_REACH_HEALTH_STOP_HIGH;
	return 1.0f;
}

float ae_reach_health_step(float health, float seconds)
{
	float cap, rate;

	if (health <= 0.0f || health >= 1.0f || seconds <= 0.0f)
		return health;
	cap = ae_reach_health_cap(health);
	if (cap <= health)
		return health;
	/* (1 + 2 (2x^3 - 3x^2 + 1) = 3 - 6x^2 + 4x^3: 3 at empty, 1 at full) */
	rate = (3.0f - 6.0f * health * health + 4.0f * health * health * health) / AE_REACH_BODY_RECHARGE_SECONDS;
	health += rate * seconds;
	return health < cap ? health : cap;
}
