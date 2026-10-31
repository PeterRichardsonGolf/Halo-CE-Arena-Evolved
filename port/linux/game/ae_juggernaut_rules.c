/*
AE_JUGGERNAUT_RULES.C

JUGGERNAUT's rules (ae_juggernaut_rules.h): pure, no engine includes.
The kill handler is the AE Juggernaut's
("Who becomes the juggernaut"), but its betrayal point loss and its allied
hunters, which this first release leaves out.
*/

#include "ae_juggernaut_rules.h"

int ae_juggernaut_h2(unsigned char byte)
{
	return (byte & AE_JUGGERNAUT_BYTE_BIT) != 0;
}

unsigned char ae_juggernaut_byte_set(unsigned char byte, int on)
{
	return (unsigned char)(on ? (byte | AE_JUGGERNAUT_BYTE_BIT) : (byte & ~AE_JUGGERNAUT_BYTE_BIT));
}

char const *ae_juggernaut_rules_name(int on)
{
	return on ? "halo 2" : "halo ce";
}

struct ae_juggernaut_kill ae_juggernaut_kill_decide(int killer_is_player, int suicide, int friendly_fire,
	int killer_is_juggernaut, int victim_is_juggernaut, int killer_alive)
{
	struct ae_juggernaut_kill result = { 0, AE_JUGGERNAUT_KEEP };
	int by_another = killer_is_player && !suicide;

	if (victim_is_juggernaut)
	{
		/* (killer != victim, the killer takes it; but CE's juggernaut needs a body to carry it) */
		result.succession = by_another && killer_alive ? AE_JUGGERNAUT_TO_KILLER : AE_JUGGERNAUT_TO_RANDOM;
	}
	else if (by_another && !friendly_fire && killer_is_juggernaut)
	{
		result.killer_scores = 1;
	}

	return result;
}

int ae_juggernaut_eligible(int present, int alive, int quit, int already_juggernaut, int excluded)
{
	return present && alive && !quit && !already_juggernaut && !excluded;
}

int ae_juggernaut_pick(int count, unsigned random)
{
	if (count <= 0)
		return -1;
	return (int)((random & 0xFFFFu) % (unsigned)count);
}

struct ae_juggernaut_shield ae_juggernaut_shield_tick(float shield, float shield_before, int over_charging, int wait,
	int stun_ticks)
{
	struct ae_juggernaut_shield result = { AE_JUGGERNAUT_SHIELD_NONE, 0.0f, wait > 0 ? wait - 1 : 0 };
	float drop = shield_before - shield;

	if (drop > 0.0f && drop <= AE_JUGGERNAUT_SHIELD_DECAY + 0.00001f && shield >= 1.0f && !over_charging)
	{
		result.action = AE_JUGGERNAUT_SHIELD_GIVE_BACK;
		result.shield_give = drop;
		return result;
	}
	if (drop > AE_JUGGERNAUT_SHIELD_DECAY + 0.00001f)
		result.wait = stun_ticks > 0 ? stun_ticks : 1;
	if (over_charging || result.wait > 0)
		return result;
	if (shield >= 0.9999f && shield < 3.0f)
		result.action = AE_JUGGERNAUT_SHIELD_OVERCHARGE;
	return result;
}
