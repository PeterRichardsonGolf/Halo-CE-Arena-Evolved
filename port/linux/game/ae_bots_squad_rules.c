/*
AE_BOTS_SQUAD_RULES.C

Arena Evolved: a campaign AI squad member's follow and choice of an enemy (ae_bots_squad_rules.h).
*/

#include "ae_bots_squad_rules.h"

float ae_bots_squad_settle(long slot)
{
	return AE_SQUAD_SETTLE + AE_SQUAD_SETTLE_STEP * (float)(slot < 0 ? 0 : slot);
}

int ae_bots_squad_follow_wants_move(float distance, int moving, long slot)
{
	float settle = ae_bots_squad_settle(slot);

	if (moving)
		return distance > settle;
	return distance > settle + AE_SQUAD_GO_MARGIN;
}

float ae_bots_squad_foe_score(float distance, float bearing, int current)
{
	return distance + (bearing < 0.0f ? -bearing : bearing) * 4.0f - (current ? 6.0f : 0.0f);
}

int ae_bots_squad_foe_in_reach(float distance_from_player)
{
	return distance_from_player <= AE_SQUAD_ENGAGE_REACH;
}

int ae_bots_squad_pickup_wanted(float from_member, float from_player, int fighting)
{
	return from_member <= (fighting ? AE_SQUAD_PICKUP_REACH_FIGHTING : AE_SQUAD_PICKUP_REACH) && from_player <= AE_SQUAD_PICKUP_LEASH;
}

int ae_bots_squad_refill_step(long no_pickup_ticks, long quiet_ticks, long since_step)
{
	return no_pickup_ticks >= AE_SQUAD_DRY_RECOVER_TICKS && quiet_ticks >= AE_SQUAD_REFILL_QUIET_TICKS &&
		since_step >= AE_SQUAD_REFILL_STEP_TICKS;
}

int ae_bots_squad_track_jump(int walked_whole_check, float moved)
{
	return walked_whole_check && moved < AE_SQUAD_TRACK_STUCK_MOVE;
}

int ae_bots_squad_deck_held(int seen, long now, long *until)
{
	if (seen)
	{
		*until = now + AE_SQUAD_DECK_HOLD_TICKS;
		return 1;
	}
	return now < *until;
}
