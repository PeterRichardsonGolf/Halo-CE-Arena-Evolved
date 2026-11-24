/*
AE_BOTS_SQUAD_RULES.C

Arena Evolved: a campaign AI squad member's follow and choice of an enemy (ae_bots_squad_rules.h).
*/

#include <math.h>
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

float ae_bots_squad_gunner_cone(float distance)
{
	return (float)atan(AE_SQUAD_GUNNER_CONE_SIZE / (distance > 0.5f ? distance : 0.5f)) + AE_SQUAD_GUNNER_CONE_DEGREES * 3.14159265f / 180.0f;
}

int ae_bots_squad_gunner_holds(float yaw_off, float pitch_off, float distance, long seen_ago, long seen_for)
{
	float cone = ae_bots_squad_gunner_cone(distance);

	if (yaw_off < 0.0f)
		yaw_off = -yaw_off;
	if (pitch_off < 0.0f)
		pitch_off = -pitch_off;
	return seen_ago <= AE_SQUAD_GUNNER_SEEN_TICKS && seen_for >= AE_SQUAD_GUNNER_REACT_TICKS && yaw_off < cone && pitch_off < cone * 1.5f;
}

float ae_bots_squad_gunner_reach(float sight, int seated_gunner)
{
	return seated_gunner ? sight * AE_SQUAD_GUNNER_REACH_SCALE : sight;
}

int ae_bots_squad_cut_off(int line_blocked, float distance)
{
	return line_blocked && distance >= AE_SQUAD_CUT_OFF_GAP;
}

int ae_bots_squad_track_blocked(float to_other, float other_to_player, float me_to_player)
{
	return to_other < AE_SQUAD_TRACK_CLOSE && other_to_player < me_to_player - AE_SQUAD_TRACK_AHEAD;
}
