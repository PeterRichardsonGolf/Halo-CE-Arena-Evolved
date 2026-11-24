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

int ae_bots_squad_spot_candidates(long slot, float facing, float *dx, float *dy, int most)
{
	/* (degrees off the facing, the member's own side first) */
	static const float angles[4][AE_SQUAD_SPOT_ANGLES] = {
		{ 150.0f, 120.0f, 180.0f, -150.0f, 100.0f, -120.0f, -100.0f },
		{ -150.0f, -120.0f, 180.0f, 150.0f, -100.0f, 120.0f, 100.0f },
		{ 180.0f, 150.0f, -150.0f, 120.0f, -120.0f, 100.0f, -100.0f },
		{ 100.0f, -100.0f, 120.0f, -120.0f, 150.0f, -150.0f, 180.0f },
	};
	int ring, each, count = 0;
	long row = slot < 0 ? 0 : slot % 4;

	for (ring = 0; ring < AE_SQUAD_SPOT_RADII; ring++)
		for (each = 0; each < AE_SQUAD_SPOT_ANGLES && count < most; each++)
		{
			float radius = AE_SQUAD_SPOT_RING_FIRST + AE_SQUAD_SPOT_RING_STEP * (float)ring;
			float angle = facing + angles[row][each] * 3.14159265f / 180.0f;

			dx[count] = radius * (float)cos(angle);
			dy[count] = radius * (float)sin(angle);
			count++;
		}
	return count;
}

int ae_bots_squad_spot_behind(float facing, float dx, float dy)
{
	float length = (float)sqrt(dx * dx + dy * dy);

	if (length <= 0.0f)
		return 0;
	/* (the cosine of the angle off the facing: 0 or less is 90 degrees or more) */
	return (dx * (float)cos(facing) + dy * (float)sin(facing)) / length <= 0.0001f;
}

int ae_bots_squad_spot_crowded(float to_other_spot, float to_player)
{
	return to_other_spot < AE_SQUAD_SPOT_SPACING || to_player < AE_SQUAD_SPOT_LEADER_GAP;
}

int ae_bots_squad_spot_refresh(long since, float player_moved)
{
	return since < 0 || since >= AE_SQUAD_SPOT_REFRESH_TICKS || player_moved >= AE_SQUAD_SPOT_MOVED;
}

int ae_bots_squad_in_position(int order, float from_spot)
{
	return from_spot <= (order == AE_SQUAD_ORDER_REGROUP ? AE_SQUAD_IN_POSITION_REGROUP : AE_SQUAD_IN_POSITION_FOLLOW);
}

int ae_bots_squad_team_state(int order, int player_moving, int members, int in_position)
{
	if (members <= 0)
		return AE_SQUAD_TEAM_NONE;
	if (order == AE_SQUAD_ORDER_HOLD)
		return AE_SQUAD_TEAM_HOLDING;
	if (order == AE_SQUAD_ORDER_REGROUP)
		return in_position >= members ? AE_SQUAD_TEAM_FORMED : AE_SQUAD_TEAM_REGROUPING;
	if (player_moving)
		return AE_SQUAD_TEAM_MOVING;
	return in_position >= members ? AE_SQUAD_TEAM_FORMED : AE_SQUAD_TEAM_GATHERING;
}

char const *ae_bots_squad_team_state_name(int state)
{
	static char const *const names[] = { "none", "formed", "moving", "gathering", "regrouping", "holding" };

	return state >= 0 && state < (int)(sizeof names / sizeof names[0]) ? names[state] : "?";
}

int ae_bots_squad_regroup_timed_out(long waited)
{
	return waited >= AE_SQUAD_REGROUP_TIMEOUT_TICKS;
}

int ae_bots_squad_keep_up_due(long out_of_position, long since_said)
{
	return out_of_position >= AE_SQUAD_KEEP_UP_TICKS && (since_said < 0 || since_said >= AE_SQUAD_KEEP_UP_TICKS);
}

int ae_bots_squad_smart_pick(int on_enemy, int has_point_order, float to_order_point, int on_vehicle_with_seat, int on_weapon)
{
	if (on_enemy)
		return AE_SQUAD_SMART_FOCUS;
	if (has_point_order && to_order_point <= AE_SQUAD_SMART_CANCEL_REACH)
		return AE_SQUAD_SMART_CANCEL;
	if (on_vehicle_with_seat)
		return AE_SQUAD_SMART_MOUNT;
	if (on_weapon)
		return AE_SQUAD_SMART_TAKE;
	return AE_SQUAD_SMART_MOVE;
}

int ae_bots_squad_smart_in_reach(int pick, float member_distance)
{
	if (pick == AE_SQUAD_SMART_MOUNT)
		return member_distance <= AE_SQUAD_SMART_MOUNT_REACH;
	if (pick == AE_SQUAD_SMART_TAKE)
		return member_distance <= AE_SQUAD_SMART_TAKE_REACH;
	return 1;
}

float ae_bots_squad_cover_exposure(int body_seen, int eyes_seen)
{
	if (body_seen && eyes_seen)
		return 1.0f;
	if (body_seen)
		return 0.75f;
	return eyes_seen ? 0.5f : 0.0f;
}

int ae_bots_squad_cover_leaves(float exposure_here)
{
	return exposure_here >= AE_SQUAD_COVER_LEAVE;
}

int ae_bots_squad_cover_takes(float exposure)
{
	return exposure <= AE_SQUAD_COVER_TAKE;
}

float ae_bots_squad_cover_value(float exposure, float walk)
{
	return (float)fabs(exposure - 0.5f) * 4.0f + walk * 0.1f;
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

int ae_bots_squad_indoors(int roof_hit, float roof_height)
{
	return roof_hit && roof_height <= AE_SQUAD_ROOF;
}

int ae_bots_squad_track_follows(int indoors, int on_track, int following)
{
	return indoors && on_track && following;
}

int ae_bots_squad_track_wants_move(float distance, int moving, long slot)
{
	float stop = AE_SQUAD_TRACK_GAP_FIRST + AE_SQUAD_TRACK_GAP_NEXT * (float)(slot < 0 ? 0 : slot);

	return distance > (moving ? stop : stop + AE_SQUAD_TRACK_GO_MARGIN);
}

float ae_bots_squad_track_off(float distance, long slot)
{
	float place = AE_SQUAD_TRACK_GAP_FIRST + AE_SQUAD_TRACK_GAP_NEXT * (float)(slot < 0 ? 0 : slot) + AE_SQUAD_TRACK_GO_MARGIN;

	return distance > place ? distance - place : 0.0f;
}

int ae_bots_squad_perched(float above_player, float distance)
{
	return above_player >= AE_SQUAD_PERCH_RISE && distance >= AE_SQUAD_PERCH_NEAR;
}
