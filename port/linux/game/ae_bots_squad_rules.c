/*
AE_BOTS_SQUAD_RULES.C

Arena Evolved: a campaign AI squad member's follow and choice of an enemy (ae_bots_squad_rules.h).
*/

#include <math.h>
#include <stdio.h>
#include <string.h>
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

/* the angle from a to b, -pi to pi */
static float ae_bots_squad_turn(float a, float b)
{
	float pi = 3.14159265f, turn = fmodf(b - a, 2.0f * pi);

	if (turn > pi)
		turn -= 2.0f * pi;
	else if (turn < -pi)
		turn += 2.0f * pi;
	return turn;
}

int ae_bots_squad_watch_pick(int count, float const *lengths, float first_yaw, float way_yaw, float player_yaw, float const *taken,
	int taken_count)
{
	float step = 2.0f * 3.14159265f / (float)(count > 0 ? count : 1), best_off = 0.0f;
	int probe, best = -1;

	for (probe = 0; probe < count; probe++)
	{
		float yaw = first_yaw + step * (float)probe, off;
		int mate, near_mate = 0;

		if (lengths[probe] < AE_SQUAD_WATCH_OPEN || lengths[probe] < lengths[(probe + count - 1) % count] ||
			lengths[probe] < lengths[(probe + 1) % count])
			continue;
		off = fabsf(ae_bots_squad_turn(way_yaw, yaw));
		if (off > AE_SQUAD_WATCH_BEHIND || fabsf(ae_bots_squad_turn(player_yaw, yaw)) < AE_SQUAD_WATCH_ARC)
			continue;
		for (mate = 0; mate < taken_count; mate++)
			if (fabsf(ae_bots_squad_turn(taken[mate], yaw)) < AE_SQUAD_WATCH_ARC)
				near_mate = 1;
		if (near_mate)
			continue;
		if (best < 0 || off < best_off - 0.001f || (off < best_off + 0.001f && lengths[probe] > lengths[best]))
		{
			best = probe;
			best_off = off;
		}
	}
	return best;
}

int ae_bots_squad_room_changed(long was, long now)
{
	return was >= 0 && now >= 0 && was != now;
}

int ae_bots_squad_door_side(float forward_x, float forward_y, float dx, float dy)
{
	return forward_x * dx + forward_y * dy < 0.0f ? -1 : 1;
}

int ae_bots_squad_door_open_left(float left_ray, float right_ray)
{
	return left_ray > right_ray;
}

int ae_bots_squad_entry_order(float const *distances, int count, int *order)
{
	int index, kept = 0;

	for (index = 0; index < count; index++)
	{
		int at;

		if (distances[index] < 0.0f)
			continue;
		for (at = kept; at > 0 && distances[order[at - 1]] > distances[index]; at--)
			order[at] = order[at - 1];
		order[at] = index;
		kept++;
	}
	return kept;
}

int ae_bots_squad_entry_grenade(long ticks_since_seen_or_heard, int has_grenade, float player_to_blast, int ordered)
{
	return ordered && ticks_since_seen_or_heard >= 0 && ticks_since_seen_or_heard <= AE_SQUAD_DOOR_HEARD_TICKS && has_grenade &&
		player_to_blast >= AE_SQUAD_GRENADE_SAFE;
}

int ae_bots_squad_door_auto(int every_door, long player_still_ticks, float player_to_door, int closed, int locked)
{
	return every_door && player_still_ticks >= AE_SQUAD_DOOR_AUTO_TICKS && player_to_door <= AE_SQUAD_DOOR_AUTO_NEAR && closed &&
		!locked;
}

int ae_bots_squad_door_locked(long ticks_first_in_radius, float door_position)
{
	return ticks_first_in_radius >= AE_SQUAD_DOOR_LOCKED_TICKS && door_position < 0.05f;
}

int ae_bots_squad_door_new_order(long serial_at_stack, long serial_now)
{
	return serial_now != serial_at_stack;
}

int ae_bots_squad_door_left(float player_to_door, int player_on_squad_side)
{
	return player_on_squad_side && player_to_door >= AE_SQUAD_DOOR_LEFT;
}

int ae_bots_squad_door_shut_for_good(int locked, float power)
{
	return locked || power <= 0.0f;
}

int ae_bots_squad_runup_phase(long ticks_since_start)
{
	if (ticks_since_start < 0)
		return AE_SQUAD_RUNUP_DONE;
	if (ticks_since_start < AE_SQUAD_RUNUP_BACK_TICKS)
		return AE_SQUAD_RUNUP_BACK;
	if (ticks_since_start < AE_SQUAD_RUNUP_BACK_TICKS + AE_SQUAD_RUNUP_RUN_TICKS)
		return AE_SQUAD_RUNUP_RUN;
	return AE_SQUAD_RUNUP_DONE;
}

int ae_bots_squad_runup_jump(float speed, long run_ticks)
{
	return speed >= AE_SQUAD_RUNUP_SPEED || run_ticks >= AE_SQUAD_RUNUP_JUMP_TICKS;
}

int ae_bots_squad_goal_keeps(float new_to_player, float old_to_player)
{
	return new_to_player >= old_to_player + AE_SQUAD_GOAL_BACK;
}

int ae_bots_squad_grid_walkable(float floor_drop, float normal_z, float headroom, int line_clear)
{
	return floor_drop <= AE_SQUAD_PLACE_DROP && normal_z >= AE_SQUAD_PLACE_FLAT && headroom >= AE_SQUAD_PLACE_HEADROOM && line_clear;
}

int ae_bots_squad_stack_skip(float to_stack_point, int room_side, float to_door, float reach)
{
	return to_stack_point < AE_SQUAD_STACK_NEAR || room_side || to_door < reach + AE_SQUAD_STACK_PAD;
}

float ae_bots_squad_stack_score(int wall_beside, int line_to_stack, int room_sees, int foe_near)
{
	return (wall_beside ? 1.0f : 0.0f) - (line_to_stack ? 0.0f : 10.0f) - (room_sees ? 2.0f : 0.0f) - (foe_near ? 2.0f : 0.0f);
}

int ae_bots_squad_stack_assign(float const (*places)[3], int place_count, float const (*men)[2], int man_count, int *slot_of_man)
{
	int taken[AE_SQUAD_PLACES], man, place, placed = 0;

	if (place_count > AE_SQUAD_PLACES)
		place_count = AE_SQUAD_PLACES;
	for (place = 0; place < place_count; place++)
		taken[place] = 0;
	for (man = 0; man < man_count; man++)
	{
		int best = -1;
		float best_cost = 0.0f;

		for (place = 0; place < place_count; place++)
		{
			float dx = places[place][0] - men[man][0], dy = places[place][1] - men[man][1];
			float cost = (float)sqrt(dx * dx + dy * dy) * 0.5f - places[place][2] * AE_SQUAD_STACK_SCORE_UNIT;

			if (!taken[place] && (best < 0 || cost < best_cost))
			{
				best = place;
				best_cost = cost;
			}
		}
		slot_of_man[man] = best;
		if (best < 0)
			continue;
		placed++;
		for (place = 0; place < place_count; place++)
		{
			float dx = places[place][0] - places[best][0], dy = places[place][1] - places[best][1];

			if (dx * dx + dy * dy <= AE_SQUAD_STACK_APART * AE_SQUAD_STACK_APART)
				taken[place] = 1;
		}
	}
	return placed;
}

int ae_bots_squad_clear_order(float const *to_door, int const *room_side, float const *to_place, int count, int *order)
{
	int index, kept = 0;

	for (index = 0; index < count; index++)
	{
		int at;
		float key;

		if (to_door[index] < 0.0f)
			continue;
		key = to_door[index] - (room_side[index] ? AE_SQUAD_CLEAR_FAR_BONUS : 0.0f);
		for (at = kept; at > 0; at--)
		{
			int other = order[at - 1];
			float other_key = to_door[other] - (room_side[other] ? AE_SQUAD_CLEAR_FAR_BONUS : 0.0f);

			if (other_key < key || (other_key == key && to_place[other] <= to_place[index]))
				break;
			order[at] = other;
		}
		order[at] = index;
		kept++;
	}
	return kept;
}

int ae_bots_squad_clear_next(float const (*points)[2], int const *visited, int count, float x, float y)
{
	int index, best = -1;
	float best_d2 = 0.0f;

	for (index = 0; index < count; index++)
	{
		float dx = points[index][0] - x, dy = points[index][1] - y, d2 = dx * dx + dy * dy;

		if (!visited[index] && (best < 0 || d2 < best_d2))
		{
			best = index;
			best_d2 = d2;
		}
	}
	return best;
}

int ae_bots_squad_clear_visit(float const (*points)[2], int *visited, int count, float x, float y)
{
	int index, newly = 0;

	for (index = 0; index < count; index++)
	{
		float dx = points[index][0] - x, dy = points[index][1] - y;

		if (!visited[index] && dx * dx + dy * dy <= AE_SQUAD_CLEAR_VISIT * AE_SQUAD_CLEAR_VISIT)
		{
			visited[index] = 1;
			newly++;
		}
	}
	return newly;
}

int ae_bots_squad_door_keep_out(float to_door, float reach, int player_through)
{
	return !player_through && to_door < reach + AE_SQUAD_DOOR_KEEP_OUT;
}

int ae_bots_squad_door_ahead(float angle_off_facing, float distance)
{
	return (float)fabs(angle_off_facing) <= AE_SQUAD_DOOR_AHEAD_ANGLE && distance <= AE_SQUAD_DOOR_AUTO_NEAR;
}

int ae_bots_squad_jump_plan(int want_jump, int lip_ahead, long ticks_since_jump)
{
	if (ticks_since_jump >= 1 && ticks_since_jump <= AE_SQUAD_JUMP_CROUCH_TICKS)
		return AE_SQUAD_JUMP_AIR;
	if (!want_jump)
		return AE_SQUAD_JUMP_NONE;
	return lip_ahead ? AE_SQUAD_JUMP_CREEP : AE_SQUAD_JUMP_GO;
}

int ae_bots_squad_ceiling_lip(float here, float ahead)
{
	return ahead < AE_SQUAD_LOW_CEILING && ahead < here - 0.03f;
}

int ae_bots_squad_creep_gave_up(long creep_ticks, float moved)
{
	return creep_ticks >= AE_SQUAD_CREEP_GIVE_UP_TICKS && moved < AE_SQUAD_CREEP_GAIN;
}

static struct
{
	char const *name;
	signed char group;
	signed char hundredths;
} const squad_stats[AE_SQUAD_STAT_COUNT] =
{
	{ "reached", 0, 0 }, { "partial", 0, 0 }, { "failed", 0, 0 }, { "track", 0, 0 },
	{ "keepout", 0, 0 }, { "edge", 0, 0 }, { "unstick", 0, 0 },
	{ "replans", 1, 0 }, { "swaps", 1, 0 }, { "modes", 1, 0 }, { "strip", 1, 0 }, { "blocked", 1, 0 },
	{ "osc", 1, 0 }, { "pair", 1, 1 },
	{ "stuck", 2, 0 }, { "catchup", 2, 0 }, { "stranded", 2, 0 }, { "teleport", 2, 0 }, { "lift", 2, 0 }, { "drop", 2, 0 },
	{ "locked", 2, 0 },
	{ "rays", 3, 0 }, { "ms", 3, 1 }, { "grid", 3, 0 }, { "gridticks", 3, 0 }, { "p99ms", 3, 1 }
};

int ae_bots_squad_stat_group(int stat)
{
	return stat >= 0 && stat < AE_SQUAD_STAT_COUNT ? squad_stats[stat].group : -1;
}

int ae_bots_squad_stats_line(char *out, int size, char const *group, long const *values)
{
	static char const *const groups[] = { "path", "slots", "warps", "cost" };
	int which = -1, stat, length;

	if (!out || size < 2 || !group || !values)
		return 0;
	out[0] = 0;
	for (stat = 0; stat < 4; stat++)
		if (strcmp(group, groups[stat]) == 0)
			which = stat;
	if (which < 0)
		return 0;
	length = snprintf(out, (size_t)size, "squad %s:", group);
	for (stat = 0; stat < AE_SQUAD_STAT_COUNT && length > 0 && length < size; stat++)
	{
		if (squad_stats[stat].group != which)
			continue;
		if (squad_stats[stat].hundredths)
			length += snprintf(out + length, (size_t)(size - length), " %s %ld.%02ld", squad_stats[stat].name, values[stat] / 100,
				(values[stat] < 0 ? -values[stat] : values[stat]) % 100);
		else
			length += snprintf(out + length, (size_t)(size - length), " %s %ld", squad_stats[stat].name, values[stat]);
	}
	return length < size ? length : size - 1;
}

int ae_bots_squad_door_closes_paths(int is_door, int locked)
{
	return is_door && locked;
}

int ae_bots_squad_learn(int is_machine, long now, long *expires)
{
	if (is_machine)
		return 0;
	if (expires)
		*expires = now + AE_SQUAD_LEARN_EXPIRY_TICKS;
	return 1;
}

int ae_bots_squad_closure_due(int door, int locked, int closed, long expires, long now)
{
	if (door)
		return locked && !closed ? 1 : !locked && closed ? -1 : 0;
	return closed && expires > 0 && now >= expires ? -1 : 0;
}

int ae_bots_squad_path_verdict(int reached, float end_to_goal, float path_length, float straight, int surfaces_found)
{
	/* (a search that did not reach the goal's surface but whose path ends within AE_SQUAD_PATH_END_OFF of it has arrived: the
	goal beside a shut door, on a surface closed to the searches) */
	(void)reached;
	/* (a long way across open ground: a path ending short of the goal but at least half the way there is walked, and the search
	goes on from its end) */
	if (!surfaces_found || (end_to_goal > AE_SQUAD_PATH_END_OFF && end_to_goal > 0.5f * straight))
		return AE_SQUAD_PATH_FAILED;
	if (path_length > AE_SQUAD_PATH_DETOUR * (straight > AE_SQUAD_PATH_STRAIGHT_LEAST ? straight : AE_SQUAD_PATH_STRAIGHT_LEAST))
		return AE_SQUAD_PATH_FAILED;
	return AE_SQUAD_PATH_OK;
}

int ae_bots_squad_walk_state(int walking, float to_goal)
{
	return walking ? to_goal > AE_SQUAD_ARRIVE : to_goal > AE_SQUAD_SET_OFF;
}

float ae_bots_squad_goal_shift(float dx, float dy, float dz)
{
	float flat = (float)sqrt(dx * dx + dy * dy), up = dz < 0.0f ? -dz : dz;

	return flat + (up > AE_SQUAD_STOREY ? up * AE_SQUAD_STOREY_WEIGHT : 0.0f);
}

int ae_bots_squad_goal_moves(float shift)
{
	return shift >= AE_SQUAD_GOAL_MOVES;
}

int ae_bots_squad_research(float goal_moved, int cluster_changed)
{
	return cluster_changed || goal_moved >= AE_SQUAD_RESEARCH;
}

int ae_bots_squad_grid_step_ok(float dz, int knee_clear)
{
	return knee_clear && dz <= AE_SQUAD_GRID_RISE && dz >= -AE_SQUAD_GRID_RISE;
}

void ae_bots_squad_grid_cell(float x, float y, float z, long *ix, long *iy, long *iz)
{
	*ix = (long)floor(x / AE_SQUAD_GRID_STEP);
	*iy = (long)floor(y / AE_SQUAD_GRID_STEP);
	*iz = (long)floor(z / AE_SQUAD_GRID_STOREY);
}

unsigned long ae_bots_squad_grid_key(int bsp, float x, float y, float z)
{
	long ix, iy, iz;

	ae_bots_squad_grid_cell(x, y, z, &ix, &iy, &iz);
	return ((unsigned long)ix * 73856093UL) ^ ((unsigned long)iy * 19349663UL) ^ ((unsigned long)iz * 83492791UL) ^
		((unsigned long)bsp * 2654435761UL);
}

float ae_bots_squad_leader_yaw(float vx, float vy, float aim_yaw, float last_yaw, int aim_held_ticks)
{
	float turn;

	if (vx * vx + vy * vy > AE_SQUAD_MOVING * AE_SQUAD_MOVING)
		return (float)atan2(vy, vx);
	turn = (float)fabs(atan2(sin(aim_yaw - last_yaw), cos(aim_yaw - last_yaw)));
	return turn > AE_SQUAD_AIM_TURN && aim_held_ticks >= AE_SQUAD_AIM_HELD_TICKS ? aim_yaw : last_yaw;
}

int ae_bots_squad_slot_ok(float to_player, float behind_player, float behind_lead, float path_near_player, int in_door_reach,
	int moving)
{
	if (to_player < AE_SQUAD_RING_IN || to_player > AE_SQUAD_RING_OUT || path_near_player < AE_SQUAD_PATH_CLEAR || in_door_reach)
		return 0;
	if (moving)
		return behind_player < -AE_SQUAD_BEHIND && behind_lead < -AE_SQUAD_BEHIND;
	return behind_player < 0.0f;
}

float ae_bots_squad_slot_cost(float to_player, int other_room, int off_floor, int threat_point_near)
{
	float off = to_player - AE_SQUAD_RING_PREFER;

	return (off < 0.0f ? -off : off) + (other_room || off_floor ? AE_SQUAD_ROOM_COST : 0.0f) +
		(threat_point_near ? AE_SQUAD_THREAT_COST : 0.0f);
}

/* the best total of the men from man on, each taking one of its choices, none within AE_SQUAD_SLOT_APART of a taken one */
static float squad_assign_from(float const cost[][AE_SQUAD_SLOTS_MAX], float const (*where)[2], int men, int man,
	int const choices[][AE_SQUAD_SLOT_CHOICES], int const *choice_count, int const *kept, int *taken, int *best)
{
	float best_total = 1.0e30f;
	int pick, chosen[8], other;

	if (man >= men)
		return 0.0f;
	for (pick = 0; pick < choice_count[man]; pick++)
	{
		int slot = choices[man][pick], clash = 0;
		float total;

		for (other = 0; other < man; other++)
		{
			float dx = where[slot][0] - where[taken[other]][0], dy = where[slot][1] - where[taken[other]][1];

			if (dx * dx + dy * dy < AE_SQUAD_SLOT_APART * AE_SQUAD_SLOT_APART)
				clash = 1;
		}
		if (clash)
			continue;
		taken[man] = slot;
		total = cost[man][slot] - (kept && kept[man] == slot ? AE_SQUAD_SLOT_KEEP : 0.0f) +
			squad_assign_from(cost, where, men, man + 1, choices, choice_count, kept, taken, chosen);
		if (total < best_total)
		{
			best_total = total;
			best[man] = slot;
			for (other = man + 1; other < men; other++)
				best[other] = chosen[other];
		}
	}
	return best_total;
}

int ae_bots_squad_slot_assign(float const cost[][AE_SQUAD_SLOTS_MAX], float const (*where)[2], int men, int slots,
	int const *kept, int *slot_of_man)
{
	int choices[8][AE_SQUAD_SLOT_CHOICES], choice_count[8], taken[8], best[8], man, slot, n;
	float total;

	if (men <= 0 || men > 8 || slots <= 0 || slots > AE_SQUAD_SLOTS_MAX)
		return 0;
	/* (each man's cheapest choices, its kept slot among them) */
	for (man = 0; man < men; man++)
	{
		choice_count[man] = 0;
		for (slot = 0; slot < slots; slot++)
		{
			float value = cost[man][slot] - (kept && kept[man] == slot ? AE_SQUAD_SLOT_KEEP : 0.0f);

			for (n = choice_count[man]; n > 0; n--)
			{
				int previous = choices[man][n - 1];
				float previous_value = cost[man][previous] - (kept && kept[man] == previous ? AE_SQUAD_SLOT_KEEP : 0.0f);

				if (previous_value <= value)
					break;
				if (n < AE_SQUAD_SLOT_CHOICES)
					choices[man][n] = previous;
			}
			if (n < AE_SQUAD_SLOT_CHOICES)
			{
				choices[man][n] = slot;
				if (choice_count[man] < AE_SQUAD_SLOT_CHOICES)
					choice_count[man]++;
			}
		}
	}
	for (man = 0; man < men; man++)
		best[man] = -1;
	total = squad_assign_from(cost, where, men, 0, (int const (*)[AE_SQUAD_SLOT_CHOICES])choices, choice_count, kept, taken, best);
	for (man = 0; man < men; man++)
		slot_of_man[man] = total < 1.0e29f ? best[man] : -1;
	return total < 1.0e29f;
}

int ae_bots_squad_replan(float moved, int cluster_changed, int ticks_since, int slot_invalid, int airborne)
{
	if (airborne)
		return 0;
	if (slot_invalid)
		return 1;
	return moved >= AE_SQUAD_REPLAN_MOVED && (cluster_changed || ticks_since >= AE_SQUAD_REPLAN_TICKS);
}

int ae_bots_squad_mode_pick(int wanted, int current, long held_ticks)
{
	if (wanted == current)
		return current;
	if (wanted == AE_SQUAD_MODE_SEAT || wanted == AE_SQUAD_MODE_MOUNT || wanted == AE_SQUAD_MODE_DOOR)
		return wanted;
	return held_ticks >= AE_SQUAD_MODE_HOLD_TICKS ? wanted : current;
}

int ae_bots_squad_override_ok(float wall_ahead)
{
	return wall_ahead > AE_SQUAD_OVERRIDE_WALL;
}

int ae_bots_squad_oscillation(float heading_before, float heading_now, long ticks_between, int fighting)
{
	float turn = (float)fabs(atan2(sin(heading_now - heading_before), cos(heading_now - heading_before)));

	return !fighting && ticks_between <= AE_SQUAD_OSCILLATION_TICKS && turn > AE_SQUAD_OSCILLATION_TURN;
}

int ae_bots_squad_in_strip(float along, float across, float player_speed)
{
	return player_speed > AE_SQUAD_MOVING && along >= 0.0f && along <= AE_SQUAD_STRIP_AHEAD &&
		across <= AE_SQUAD_STRIP_HALF && across >= -AE_SQUAD_STRIP_HALF;
}

int ae_bots_squad_make_way(float room_left, float room_right)
{
	if (room_left < AE_SQUAD_WAY_ROOM && room_right < AE_SQUAD_WAY_ROOM)
		return AE_SQUAD_WAY_AHEAD;
	return room_right >= room_left ? AE_SQUAD_WAY_RIGHT : AE_SQUAD_WAY_LEFT;
}

int ae_bots_squad_yield(float dx, float dy, float dz, float heading, float velocity_x, float velocity_y)
{
	float distance = sqrtf(dx * dx + dy * dy), speed = sqrtf(velocity_x * velocity_x + velocity_y * velocity_y);
	float c = (float)cos(heading), s = (float)sin(heading);

	if (distance > AE_SQUAD_YIELD_NEAR || distance < 0.01f || fabsf(dz) > AE_SQUAD_YIELD_RISE || speed < AE_SQUAD_YIELD_MOVING ||
		dx * c + dy * s < 0.3f * distance)
		return AE_SQUAD_YIELD_NONE;
	return velocity_x * c + velocity_y * s > 0.5f * speed ? AE_SQUAD_YIELD_SLOW : AE_SQUAD_YIELD_BACK;
}

int ae_bots_squad_make_way_kept(int kept, float room_left, float room_right)
{
	if ((kept == AE_SQUAD_WAY_LEFT && room_left < AE_SQUAD_WAY_ROOM) || (kept == AE_SQUAD_WAY_RIGHT && room_right < AE_SQUAD_WAY_ROOM))
		return ae_bots_squad_make_way(room_left, room_right);
	return kept;
}

int ae_bots_squad_file_reversal(float turn_degrees, float corridor_width)
{
	return turn_degrees > AE_SQUAD_REVERSAL_DEGREES && corridor_width < AE_SQUAD_REVERSAL_WIDTH;
}

int ae_bots_squad_in_any_door_reach(float to_door, float reach, int route_crosses)
{
	return !route_crosses && to_door < reach + AE_SQUAD_DOOR_KEEP_OUT;
}

int ae_bots_squad_player_through(float past_plane, int was)
{
	return was ? past_plane > -AE_SQUAD_THROUGH : past_plane >= AE_SQUAD_THROUGH;
}

/* the eighth of a turn a yaw falls in */
static int squad_sector(float yaw)
{
	float turn = yaw / 6.2831853f;

	turn -= (float)floor(turn);
	return (int)(turn * 8.0f + 0.5f) % 8;
}

int ae_bots_squad_stack_sector(float yaw_to_door_side, float yaw_to_stack_point)
{
	return squad_sector(yaw_to_door_side) == squad_sector(yaw_to_stack_point) ? AE_SQUAD_SECTOR_SAME : 0;
}

int ae_bots_squad_stacked(int in_place, int members, float farthest_place_to_door)
{
	return members > 0 && in_place == members && farthest_place_to_door <= AE_SQUAD_PLACE_REACH;
}

float ae_bots_squad_fight_leash(int narrow, int indoors)
{
	return narrow && indoors ? AE_SQUAD_FIGHT_NARROW : AE_SQUAD_FIGHT_WIDE;
}

int ae_bots_squad_fire_line_ok(float nearest_player)
{
	return nearest_player >= AE_SQUAD_FIRE_CLEAR;
}

float ae_bots_squad_throttle(float player_speed, float max_speed, float to_slot, float to_player)
{
	float pace;

	if (to_slot > AE_SQUAD_PACE_FAR_SLOT || to_player > AE_SQUAD_PACE_FAR_PLAYER)
		return 1.0f;
	if (to_slot <= AE_SQUAD_ARRIVE)
		return 0.0f;
	pace = (max_speed > 0.0f ? player_speed / max_speed : 1.0f) + AE_SQUAD_PACE_PULL * (to_slot - AE_SQUAD_ARRIVE);
	return pace < AE_SQUAD_PACE_LEAST ? AE_SQUAD_PACE_LEAST : pace > 1.0f ? 1.0f : pace;
}

int ae_bots_squad_cue_jump_now(float to_crumb)
{
	return to_crumb <= AE_SQUAD_CUE_JUMP_NEAR;
}

int ae_bots_squad_crossing_waits(int another_crosses, float crosser_to_crumb, long crossing_ticks)
{
	return another_crosses && crosser_to_crumb < AE_SQUAD_CROSSING_CLEAR && crossing_ticks < AE_SQUAD_CROSSING_TICKS;
}

long ae_bots_squad_cost_p99(long const *histogram)
{
	long total = 0, seen = 0, bucket;

	for (bucket = 0; bucket < AE_SQUAD_COST_BUCKETS; bucket++)
		total += histogram[bucket];
	if (total <= 0)
		return 0;
	for (bucket = 0; bucket < AE_SQUAD_COST_BUCKETS; bucket++)
	{
		seen += histogram[bucket];
		if (seen * 100 >= total * 99)
			return bucket * 10;
	}
	return (AE_SQUAD_COST_BUCKETS - 1) * 10;
}

int ae_bots_squad_oscillation_after(float heading_before, float heading_now, long ticks_between, int fighting,
	long ticks_since_player_turned)
{
	return ticks_since_player_turned >= AE_SQUAD_PLAYER_TURN_TICKS &&
		ae_bots_squad_oscillation(heading_before, heading_now, ticks_between, fighting);
}

int ae_bots_squad_stalled(int walk_failed_lately, int cut_off, int on_track, int in_cover, int at_slot, int on_order)
{
	return (walk_failed_lately || cut_off || on_track) && !in_cover && !at_slot && !on_order;
}

int ae_bots_squad_report_due(char const *last, char const *text, long last_tick, long now)
{
	return last_tick < 0 || now < last_tick || now - last_tick >= AE_SQUAD_REPORT_QUIET_TICKS || strcmp(last, text) != 0;
}

long ae_bots_squad_gap_p90(long const *histogram)
{
	long total = 0, seen = 0;
	int bucket;

	for (bucket = 0; bucket < AE_SQUAD_GAP_BUCKETS; bucket++)
		total += histogram[bucket];
	if (total <= 0)
		return 0;
	for (bucket = 0; bucket < AE_SQUAD_GAP_BUCKETS; bucket++)
	{
		seen += histogram[bucket];
		if (seen * 10 >= total * 9)
			return (long)(bucket + 1) * 50;
	}
	return AE_SQUAD_GAP_BUCKETS * 50;
}

int ae_bots_squad_stuck_tick(int wants_move, float speed, int fighting)
{
	return wants_move && speed < AE_SQUAD_STUCK_SPEED && !fighting;
}
