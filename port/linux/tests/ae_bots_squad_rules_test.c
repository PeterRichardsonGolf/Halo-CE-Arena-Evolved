#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "ae_bots_squad_rules.h"

#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); exit(1); } } while (0)

int main(void)
{
	/* each member stands a step further */
	CHECK(ae_bots_squad_settle(0) == AE_SQUAD_SETTLE);
	CHECK(ae_bots_squad_settle(2) > ae_bots_squad_settle(1));
	CHECK(ae_bots_squad_settle(-1) == AE_SQUAD_SETTLE);
	/* standing: sets off past the settle distance and the margin, not before */
	CHECK(!ae_bots_squad_follow_wants_move(AE_SQUAD_SETTLE + 1.0f, 0, 0));
	CHECK(!ae_bots_squad_follow_wants_move(AE_SQUAD_SETTLE + AE_SQUAD_GO_MARGIN, 0, 0));
	CHECK(ae_bots_squad_follow_wants_move(AE_SQUAD_SETTLE + AE_SQUAD_GO_MARGIN + 0.1f, 0, 0));
	/* walking: on until within the settle distance */
	CHECK(ae_bots_squad_follow_wants_move(AE_SQUAD_SETTLE + 1.0f, 1, 0));
	CHECK(!ae_bots_squad_follow_wants_move(AE_SQUAD_SETTLE - 0.1f, 1, 0));
	/* a later member settles further out */
	CHECK(!ae_bots_squad_follow_wants_move(ae_bots_squad_settle(3) + 0.1f, 0, 3));
	CHECK(ae_bots_squad_follow_wants_move(ae_bots_squad_settle(3) + 0.1f, 1, 3));
	/* the enemy's score: near beats far, ahead beats behind, the current one is favoured */
	CHECK(ae_bots_squad_foe_score(10.0f, 0.0f, 0) < ae_bots_squad_foe_score(20.0f, 0.0f, 0));
	CHECK(ae_bots_squad_foe_score(10.0f, 0.0f, 0) < ae_bots_squad_foe_score(10.0f, 1.0f, 0));
	CHECK(ae_bots_squad_foe_score(10.0f, 1.0f, 0) == ae_bots_squad_foe_score(10.0f, -1.0f, 0));
	CHECK(ae_bots_squad_foe_score(12.0f, 0.0f, 1) < ae_bots_squad_foe_score(10.0f, 0.0f, 0));
	/* the reach */
	CHECK(ae_bots_squad_foe_in_reach(AE_SQUAD_ENGAGE_REACH));
	CHECK(!ae_bots_squad_foe_in_reach(AE_SQUAD_ENGAGE_REACH + 1.0f));
	/* the deck held: seen, then held 3 s after it was last seen; seen again in the hold, held on from then */
	{
		long until = 0;

		CHECK(!ae_bots_squad_deck_held(0, 100, &until));
		CHECK(ae_bots_squad_deck_held(1, 100, &until) && until == 100 + AE_SQUAD_DECK_HOLD_TICKS);
		CHECK(ae_bots_squad_deck_held(0, 100 + AE_SQUAD_DECK_HOLD_TICKS - 1, &until));
		CHECK(!ae_bots_squad_deck_held(0, 100 + AE_SQUAD_DECK_HOLD_TICKS, &until));
		CHECK(ae_bots_squad_deck_held(1, 150, &until) && ae_bots_squad_deck_held(0, 150 + AE_SQUAD_DECK_HOLD_TICKS - 1, &until));
		/* dry: a weapon in reach of the member and near the player is gone for; one too far from either is not */
		CHECK(ae_bots_squad_pickup_wanted(AE_SQUAD_PICKUP_REACH, AE_SQUAD_PICKUP_LEASH, 0));
		CHECK(!ae_bots_squad_pickup_wanted(AE_SQUAD_PICKUP_REACH + 0.1f, 1.0f, 0));
		CHECK(!ae_bots_squad_pickup_wanted(1.0f, AE_SQUAD_PICKUP_LEASH + 0.1f, 0));
		/* in a fight only a near one */
		CHECK(ae_bots_squad_pickup_wanted(AE_SQUAD_PICKUP_REACH_FIGHTING, 1.0f, 1));
		CHECK(!ae_bots_squad_pickup_wanted(AE_SQUAD_PICKUP_REACH_FIGHTING + 0.1f, 1.0f, 1));
		/* the fallback: only after 15 s with no pickup in reach, 3 s with no enemy seen, a magazine every 5 s */
		CHECK(!ae_bots_squad_refill_step(AE_SQUAD_DRY_RECOVER_TICKS - 1, 1000, 1000));
		CHECK(!ae_bots_squad_refill_step(AE_SQUAD_DRY_RECOVER_TICKS, AE_SQUAD_REFILL_QUIET_TICKS - 1, 1000));
		CHECK(!ae_bots_squad_refill_step(AE_SQUAD_DRY_RECOVER_TICKS, 1000, AE_SQUAD_REFILL_STEP_TICKS - 1));
		CHECK(ae_bots_squad_refill_step(AE_SQUAD_DRY_RECOVER_TICKS, AE_SQUAD_REFILL_QUIET_TICKS, AE_SQUAD_REFILL_STEP_TICKS));
		/* on the track: stopped while it walked, it jumps; moving on, or standing by choice, it does not */
		CHECK(ae_bots_squad_track_jump(1, 0.1f));
		CHECK(!ae_bots_squad_track_jump(1, AE_SQUAD_TRACK_STUCK_MOVE + 0.1f));
		CHECK(!ae_bots_squad_track_jump(0, 0.0f));
		/* the edge's drops: a deck's step is held at a smaller drop than the ground's */
		CHECK(AE_SQUAD_DECK_DROP < AE_SQUAD_EDGE_DROP);
	}
	printf("ok\n");
	/* FORMATION: every spot a member tries is behind or beside the facing, the near ring first, its own side first */
	{
		float dx[AE_SQUAD_SPOT_CANDIDATES], dy[AE_SQUAD_SPOT_CANDIDATES];
		long slot;
		int count, each;

		for (slot = 0; slot < 4; slot++)
		{
			count = ae_bots_squad_spot_candidates(slot, 0.7f, dx, dy, AE_SQUAD_SPOT_CANDIDATES);
			CHECK(count == AE_SQUAD_SPOT_CANDIDATES);
			for (each = 0; each < count; each++)
				CHECK(ae_bots_squad_spot_behind(0.7f, dx[each], dy[each]));
			CHECK(fabs(sqrt(dx[0] * dx[0] + dy[0] * dy[0]) - AE_SQUAD_SPOT_RING_FIRST) < 0.01);
		}
		/* the first member behind-left, the second behind-right (facing +x: left is +y) */
		count = ae_bots_squad_spot_candidates(0, 0.0f, dx, dy, 1);
		CHECK(count == 1 && dx[0] < 0.0f && dy[0] > 0.0f);
		count = ae_bots_squad_spot_candidates(1, 0.0f, dx, dy, 1);
		CHECK(count == 1 && dx[0] < 0.0f && dy[0] < 0.0f);
		/* in front of the aim is never a spot */
		CHECK(!ae_bots_squad_spot_behind(0.0f, 3.0f, 0.5f));
		CHECK(ae_bots_squad_spot_behind(0.0f, 0.0f, 3.0f));
		CHECK(ae_bots_squad_spot_crowded(AE_SQUAD_SPOT_SPACING - 0.1f, 5.0f));
		CHECK(ae_bots_squad_spot_crowded(5.0f, AE_SQUAD_SPOT_LEADER_GAP - 0.1f));
		CHECK(!ae_bots_squad_spot_crowded(AE_SQUAD_SPOT_SPACING, AE_SQUAD_SPOT_LEADER_GAP));
		CHECK(ae_bots_squad_spot_refresh(-1, 0.0f));
		CHECK(!ae_bots_squad_spot_refresh(AE_SQUAD_SPOT_REFRESH_TICKS - 1, AE_SQUAD_SPOT_MOVED - 0.1f));
		CHECK(ae_bots_squad_spot_refresh(AE_SQUAD_SPOT_REFRESH_TICKS, 0.0f));
		CHECK(ae_bots_squad_spot_refresh(1, AE_SQUAD_SPOT_MOVED));
		/* (the player into another room of the level: chosen again at once; no room known on either side: not) */
		CHECK(ae_bots_squad_room_changed(3, 4) && !ae_bots_squad_room_changed(3, 3));
		CHECK(!ae_bots_squad_room_changed(-1, 4) && !ae_bots_squad_room_changed(3, -1));
	}
	/* TEAM LAYER: the team's state over its order */
	CHECK(ae_bots_squad_team_state(AE_SQUAD_ORDER_FOLLOW, 0, 0, 0) == AE_SQUAD_TEAM_NONE);
	CHECK(ae_bots_squad_team_state(AE_SQUAD_ORDER_FOLLOW, 0, 2, 2) == AE_SQUAD_TEAM_FORMED);
	CHECK(ae_bots_squad_team_state(AE_SQUAD_ORDER_FOLLOW, 0, 2, 1) == AE_SQUAD_TEAM_GATHERING);
	CHECK(ae_bots_squad_team_state(AE_SQUAD_ORDER_FOLLOW, 1, 2, 2) == AE_SQUAD_TEAM_MOVING);
	CHECK(ae_bots_squad_team_state(AE_SQUAD_ORDER_REGROUP, 1, 2, 1) == AE_SQUAD_TEAM_REGROUPING);
	CHECK(ae_bots_squad_team_state(AE_SQUAD_ORDER_REGROUP, 1, 2, 2) == AE_SQUAD_TEAM_FORMED);
	CHECK(ae_bots_squad_team_state(AE_SQUAD_ORDER_HOLD, 1, 2, 0) == AE_SQUAD_TEAM_HOLDING);
	CHECK(!strcmp(ae_bots_squad_team_state_name(AE_SQUAD_TEAM_REGROUPING), "regrouping"));
	CHECK(ae_bots_squad_in_position(AE_SQUAD_ORDER_FOLLOW, AE_SQUAD_IN_POSITION_FOLLOW));
	CHECK(!ae_bots_squad_in_position(AE_SQUAD_ORDER_REGROUP, AE_SQUAD_IN_POSITION_REGROUP + 0.01f) &&
		!ae_bots_squad_in_position(AE_SQUAD_ORDER_FOLLOW, AE_SQUAD_IN_POSITION_FOLLOW + 0.01f));
	CHECK(ae_bots_squad_in_position(AE_SQUAD_ORDER_REGROUP, AE_SQUAD_IN_POSITION_REGROUP));
	CHECK(!ae_bots_squad_regroup_timed_out(AE_SQUAD_REGROUP_TIMEOUT_TICKS - 1) && ae_bots_squad_regroup_timed_out(AE_SQUAD_REGROUP_TIMEOUT_TICKS));
	CHECK(!ae_bots_squad_keep_up_due(AE_SQUAD_KEEP_UP_TICKS - 1, -1));
	CHECK(ae_bots_squad_keep_up_due(AE_SQUAD_KEEP_UP_TICKS, -1));
	CHECK(!ae_bots_squad_keep_up_due(AE_SQUAD_KEEP_UP_TICKS, AE_SQUAD_KEEP_UP_TICKS - 1));
	/* SMART: the SMART tap's pick, by tier: an enemy first, then the squad's own marker, a vehicle, a weapon, a place */
	CHECK(ae_bots_squad_smart_pick(1, 1, 0.0f, 1, 1) == AE_SQUAD_SMART_FOCUS);
	CHECK(ae_bots_squad_smart_pick(0, 1, AE_SQUAD_SMART_CANCEL_REACH, 1, 1) == AE_SQUAD_SMART_CANCEL);
	CHECK(ae_bots_squad_smart_pick(0, 1, AE_SQUAD_SMART_CANCEL_REACH + 0.1f, 1, 1) == AE_SQUAD_SMART_MOUNT);
	CHECK(ae_bots_squad_smart_pick(0, 0, 0.0f, 1, 1) == AE_SQUAD_SMART_MOUNT);
	CHECK(ae_bots_squad_smart_pick(0, 0, 0.0f, 0, 1) == AE_SQUAD_SMART_TAKE);
	CHECK(ae_bots_squad_smart_pick(0, 0, 0.0f, 0, 0) == AE_SQUAD_SMART_MOVE);
	CHECK(ae_bots_squad_smart_in_reach(AE_SQUAD_SMART_MOUNT, AE_SQUAD_SMART_MOUNT_REACH));
	CHECK(!ae_bots_squad_smart_in_reach(AE_SQUAD_SMART_MOUNT, AE_SQUAD_SMART_MOUNT_REACH + 0.1f));
	CHECK(!ae_bots_squad_smart_in_reach(AE_SQUAD_SMART_TAKE, AE_SQUAD_SMART_TAKE_REACH + 0.1f));
	CHECK(ae_bots_squad_smart_in_reach(AE_SQUAD_SMART_MOVE, 1000.0f));
	/* COVER: exposure by the enemy's lines, two thresholds, half cover first */
	CHECK(ae_bots_squad_cover_exposure(1, 1) == 1.0f && ae_bots_squad_cover_exposure(0, 0) == 0.0f);
	CHECK(ae_bots_squad_cover_exposure(0, 1) == 0.5f && ae_bots_squad_cover_exposure(1, 0) == 0.75f);
	CHECK(ae_bots_squad_cover_leaves(1.0f) && ae_bots_squad_cover_leaves(0.75f) && !ae_bots_squad_cover_leaves(0.5f));
	CHECK(ae_bots_squad_cover_takes(0.5f) && ae_bots_squad_cover_takes(0.0f) && !ae_bots_squad_cover_takes(0.75f));
	/* (a spot between the thresholds is neither left nor taken: it holds) */
	CHECK(!ae_bots_squad_cover_leaves(0.6f) && !ae_bots_squad_cover_takes(0.6f));
	CHECK(ae_bots_squad_cover_value(0.5f, 3.5f) < ae_bots_squad_cover_value(0.0f, 2.0f));
	CHECK(ae_bots_squad_cover_value(0.5f, 2.0f) < ae_bots_squad_cover_value(0.5f, 3.5f));
	/* the gunner: the multiplayer gunner's cone, held, at range too */
	{
		float degree = 3.14159265f / 180.0f;

		CHECK(fabs(ae_bots_squad_gunner_cone(40.0f) - (atan(0.6 / 40.0) + 3.0 * degree)) < 0.0001);
		CHECK(ae_bots_squad_gunner_cone(0.1f) == ae_bots_squad_gunner_cone(0.5f));
		/* (3.5 degrees off at 40 units: held; the burst rule's cone there is under 1.5 degrees) */
		CHECK(ae_bots_squad_gunner_holds(3.5f * degree, 0.0f, 40.0f, 0, 30));
		CHECK(!ae_bots_squad_gunner_holds(4.5f * degree, 0.0f, 40.0f, 0, 30));
		CHECK(ae_bots_squad_gunner_holds(0.0f, 5.0f * degree, 40.0f, 0, 30));
		CHECK(!ae_bots_squad_gunner_holds(0.0f, 6.5f * degree, 40.0f, 0, 30));
		CHECK(!ae_bots_squad_gunner_holds(0.0f, 0.0f, 40.0f, AE_SQUAD_GUNNER_SEEN_TICKS + 1, 30));
		CHECK(!ae_bots_squad_gunner_holds(0.0f, 0.0f, 40.0f, 0, AE_SQUAD_GUNNER_REACT_TICKS - 1));
		CHECK(ae_bots_squad_gunner_holds(0.0f, 0.0f, 40.0f, AE_SQUAD_GUNNER_SEEN_TICKS, AE_SQUAD_GUNNER_REACT_TICKS));
		/* (a seated gunner sees half as far again: heroic's 42 is 63; on foot as before) */
		CHECK(fabs(ae_bots_squad_gunner_reach(42.0f, 1) - 63.0f) < 0.01 && ae_bots_squad_gunner_reach(42.0f, 0) == 42.0f);
		CHECK(ae_bots_squad_foe_in_reach(AE_SQUAD_ENGAGE_REACH * AE_SQUAD_GUNNER_REACH_SCALE / ae_bots_squad_gunner_reach(1.0f, 1)));
	}
	/* cut off by a barrier: the line blocked and a step away; not when beside the player, not with a clear line */
	CHECK(ae_bots_squad_cut_off(1, AE_SQUAD_CUT_OFF_GAP) && ae_bots_squad_cut_off(1, 5.0f));
	CHECK(!ae_bots_squad_cut_off(1, AE_SQUAD_CUT_OFF_GAP - 0.1f) && !ae_bots_squad_cut_off(0, 5.0f));
	/* (members 3.8 and 5.0 units from the player across a barrier, no track walk wanted: cut off, they walk) */
	CHECK(!ae_bots_squad_follow_wants_move(5.0f, 0, 0) && ae_bots_squad_cut_off(1, 5.0f));
	/* (two side by side at a barrier, 0.45 apart, 4.9 and 5.0 from the player: neither stops the other's jump) */
	CHECK(!ae_bots_squad_track_blocked(0.45f, 4.9f, 5.0f) && !ae_bots_squad_track_blocked(0.45f, 5.0f, 4.9f));
	CHECK(ae_bots_squad_track_blocked(1.0f, 3.0f, 4.0f) && !ae_bots_squad_track_blocked(AE_SQUAD_TRACK_CLOSE, 3.0f, 4.0f));
	/* track follow indoors: single file, the first 2.5 behind the player, the next 1.5 further; 1 more to set off standing */
	CHECK(!ae_bots_squad_track_wants_move(3.0f, 0, 0) && ae_bots_squad_track_wants_move(3.6f, 0, 0) && ae_bots_squad_track_wants_move(2.6f, 1, 0));
	CHECK(!ae_bots_squad_track_wants_move(4.5f, 0, 1) && ae_bots_squad_track_wants_move(4.1f, 1, 1) && !ae_bots_squad_track_wants_move(3.9f, 1, 1));
	CHECK(ae_bots_squad_track_follows(1, 1, 1) && !ae_bots_squad_track_follows(0, 1, 1) && !ae_bots_squad_track_follows(1, 0, 1) &&
		!ae_bots_squad_track_follows(1, 1, 0));
	CHECK(ae_bots_squad_track_off(3.4f, 0) == 0.0f && fabsf(ae_bots_squad_track_off(6.0f, 0) - 2.5f) < 0.01f &&
		ae_bots_squad_track_off(4.9f, 1) == 0.0f);
	/* (a member on a barrier 0.2 above the player, 2.9 away: it walks on; on the floor, or beside the player: it stands) */
	CHECK(ae_bots_squad_perched(0.2f, 2.9f) && !ae_bots_squad_perched(0.0f, 2.9f) && !ae_bots_squad_perched(0.2f, 1.4f));
	CHECK(ae_bots_squad_indoors(1, 6.0f) && !ae_bots_squad_indoors(0, 0.0f) && !ae_bots_squad_indoors(1, 12.0f));
	/* watch while walking: 12 probes 30 degrees apart from 0; a corridor ahead and behind (6 long), a door on the left (probe 3) */
	{
		float pi = 3.14159265f, degree = pi / 180.0f, lengths[12], taken[1];
		int probe;

		for (probe = 0; probe < 12; probe++)
			lengths[probe] = 1.0f;
		lengths[0] = 6.0f;
		lengths[6] = 6.0f;
		lengths[3] = 4.0f;
		/* (the player looks down the corridor and nothing watches behind on the walk: the door) */
		CHECK(ae_bots_squad_watch_pick(12, lengths, 0.0f, 0.0f, 0.0f, taken, 0) == 3);
		/* (a mate watches the door: none left) */
		taken[0] = 90.0f * degree;
		CHECK(ae_bots_squad_watch_pick(12, lengths, 0.0f, 0.0f, 0.0f, taken, 1) == -1);
		/* (the player looks into the door: the corridor ahead) */
		CHECK(ae_bots_squad_watch_pick(12, lengths, 0.0f, 0.0f, 90.0f * degree, taken, 0) == 0);
		/* (the rear guard, its way back down the corridor: behind) */
		CHECK(ae_bots_squad_watch_pick(12, lengths, 0.0f, pi, 0.0f, taken, 0) == 6);
		/* (a side room is the longest of its neighbours, not the ray beside it) */
		lengths[4] = 5.0f;
		CHECK(ae_bots_squad_watch_pick(12, lengths, 0.0f, 0.0f, 0.0f, taken, 0) == 4);
		/* (too short for an opening: none) */
		lengths[3] = 1.8f;
		lengths[4] = 1.0f;
		CHECK(ae_bots_squad_watch_pick(12, lengths, 0.0f, 0.0f, 0.0f, taken, 0) == -1);
		/* (two as far off the walk: the longer) */
		lengths[2] = 3.0f;
		lengths[10] = 5.0f;
		CHECK(ae_bots_squad_watch_pick(12, lengths, 0.0f, 0.0f, 0.0f, taken, 0) == 10);
	}
	/* door stack: a door facing +x at the origin, its activation radius 1 */
	CHECK(ae_bots_squad_door_side(1.0f, 0.0f, -2.0f, 0.3f) == -1 && ae_bots_squad_door_side(1.0f, 0.0f, 2.0f, 0.0f) == 1);
	CHECK(ae_bots_squad_door_open_left(5.0f, 2.0f) && !ae_bots_squad_door_open_left(2.0f, 5.0f));
	{
		/* (the entry order: nearest first, a dead or seated member (-1) left out) */
		float distances[3] = { 2.5f, -1.0f, 1.5f };
		int order[3];

		CHECK(ae_bots_squad_entry_order(distances, 3, order) == 2 && order[0] == 2 && order[1] == 0);
	}
	/* (a grenade first: an enemy seen or heard in the last 10 s, one to throw, the player 4 or more from the blast) */
	CHECK(ae_bots_squad_entry_grenade(299, 1, 6.0f, 1) && !ae_bots_squad_entry_grenade(301, 1, 6.0f, 1));
	CHECK(!ae_bots_squad_entry_grenade(10, 0, 6.0f, 1) && !ae_bots_squad_entry_grenade(10, 1, 3.9f, 1) &&
		!ae_bots_squad_entry_grenade(-1, 1, 6.0f, 1));
	/* (GO by the player's own walk into or through the door: never a grenade, the player is in the doorway) */
	CHECK(!ae_bots_squad_entry_grenade(10, 1, 6.0f, 0));
	/* (a new order, even of the same kind (a new serial), ends the stack; the door's own order change does not) */
	CHECK(ae_bots_squad_door_new_order(4, 5) && !ae_bots_squad_door_new_order(5, 5));
	/* (a stack on its own (EVERY DOOR) is let go when the player walks off on the squad's side, 8 or more away) */
	CHECK(ae_bots_squad_door_left(8.1f, 1) && !ae_bots_squad_door_left(7.9f, 1) && !ae_bots_squad_door_left(9.0f, 0));
	/* run-up at a barrier: back off 12 ticks, then run at it; the jump once moving fast enough, or 10 ticks into the run */
	CHECK(ae_bots_squad_runup_phase(0) == AE_SQUAD_RUNUP_BACK && ae_bots_squad_runup_phase(11) == AE_SQUAD_RUNUP_BACK);
	CHECK(ae_bots_squad_runup_phase(12) == AE_SQUAD_RUNUP_RUN && ae_bots_squad_runup_phase(41) == AE_SQUAD_RUNUP_RUN);
	CHECK(ae_bots_squad_runup_phase(42) == AE_SQUAD_RUNUP_DONE && ae_bots_squad_runup_phase(-1) == AE_SQUAD_RUNUP_DONE);
	CHECK(ae_bots_squad_runup_jump(1.6f, 2) && !ae_bots_squad_runup_jump(1.0f, 9) && ae_bots_squad_runup_jump(0.2f, 10));
	/* the walk's goal on the player's track never goes back: a new goal 0.5 or more farther from the player keeps the old */
	CHECK(ae_bots_squad_goal_keeps(3.6f, 3.0f) && !ae_bots_squad_goal_keeps(3.4f, 3.0f) && !ae_bots_squad_goal_keeps(2.0f, 3.0f));
	/* a door the level keeps shut or unpowered is locked for the squad */
	CHECK(ae_bots_squad_door_shut_for_good(1, 1.0f) && ae_bots_squad_door_shut_for_good(0, 0.0f) &&
		!ae_bots_squad_door_shut_for_good(0, 0.5f));
	/* (every door: the player still 2 s within 4 of a closed door the level has not locked) */
	CHECK(ae_bots_squad_door_auto(1, 60, 3.9f, 1, 0) && !ae_bots_squad_door_auto(0, 60, 3.9f, 1, 0));
	CHECK(!ae_bots_squad_door_auto(1, 59, 3.9f, 1, 0) && !ae_bots_squad_door_auto(1, 60, 4.1f, 1, 0) &&
		!ae_bots_squad_door_auto(1, 60, 3.9f, 0, 0) && !ae_bots_squad_door_auto(1, 60, 3.9f, 1, 1));
	/* (locked: still shut 1 s after the first man came into its reach) */
	CHECK(ae_bots_squad_door_locked(30, 0.0f) && !ae_bots_squad_door_locked(29, 0.0f) && !ae_bots_squad_door_locked(30, 0.2f));
	/* PLACES: a grid point is walkable with a floor 0.6 or less below, flat (normal z 0.7),
	0.6 headroom and a clear line from the stack point */
	CHECK(ae_bots_squad_grid_walkable(0.5f, 0.9f, 0.7f, 1) && !ae_bots_squad_grid_walkable(0.7f, 0.9f, 0.7f, 1));
	CHECK(!ae_bots_squad_grid_walkable(0.5f, 0.6f, 0.7f, 1) && !ae_bots_squad_grid_walkable(0.5f, 0.9f, 0.5f, 1) &&
		!ae_bots_squad_grid_walkable(0.5f, 0.9f, 0.7f, 0));
	/* (a candidate skipped: within 0.3 of the stack point, on the room's side, or in the door's reach + 0.6) */
	CHECK(ae_bots_squad_stack_skip(0.2f, 0, 3.0f, 0.9f) && ae_bots_squad_stack_skip(1.0f, 1, 3.0f, 0.9f) &&
		ae_bots_squad_stack_skip(1.0f, 0, 1.4f, 0.9f) && !ae_bots_squad_stack_skip(1.0f, 0, 1.6f, 0.9f));
	/* (its score: +1 a wall beside, -10 no line to the stack point, -2 the room sees it, -2 a known foe near) */
	CHECK(ae_bots_squad_stack_score(1, 1, 0, 0) == 1.0f && ae_bots_squad_stack_score(0, 0, 0, 0) == -10.0f &&
		ae_bots_squad_stack_score(1, 1, 1, 1) == -3.0f);
	{
		/* (two men, three places: by turns the cheapest, distance/2 - score x 0.5; a taken place blocks all within 0.45) */
		float places[3][3] = { { 0.0f, 1.0f, 1.0f }, { 0.0f, 1.3f, 1.0f }, { 0.0f, 2.5f, 0.0f } };
		float men[2][2] = { { 0.0f, 0.0f }, { 0.0f, 0.2f } };
		int slot[2];

		CHECK(ae_bots_squad_stack_assign((float const (*)[3])places, 3, (float const (*)[2])men, 2, slot) == 2 &&
			slot[0] == 0 && slot[1] == 2);
		/* (one place for two: the second holds where he is) */
		CHECK(ae_bots_squad_stack_assign((float const (*)[3])places, 1, (float const (*)[2])men, 2, slot) == 1 && slot[1] == -1);
	}
	{
		/* (entry order: nearest the door first, 0.5 less for one already on the room's side, a tie to the nearer his own place;
		under 0 left out) */
		float to_door[3] = { 1.2f, 1.6f, -1.0f }, to_place[3] = { 0.0f, 0.0f, 0.0f };
		int far[3] = { 0, 1, 0 }, order[3];

		CHECK(ae_bots_squad_clear_order(to_door, far, to_place, 3, order) == 2 && order[0] == 1 && order[1] == 0);
	}
	{
		/* (the clear list: the nearest place not visited; a member within 0.6 visits it) */
		float points[3][2] = { { 1.0f, 0.0f }, { 3.0f, 0.0f }, { 5.0f, 0.0f } };
		int visited[3] = { 0, 0, 0 };

		CHECK(ae_bots_squad_clear_next((float const (*)[2])points, visited, 3, 2.6f, 0.0f) == 1);
		CHECK(ae_bots_squad_clear_visit((float const (*)[2])points, visited, 3, 3.5f, 0.0f) == 1 && visited[1] &&
			!visited[2] && ae_bots_squad_clear_next((float const (*)[2])points, visited, 3, 2.6f, 0.0f) == 0);
		visited[0] = visited[2] = 1;
		CHECK(ae_bots_squad_clear_next((float const (*)[2])points, visited, 3, 0.0f, 0.0f) == -1);
	}
	/* (a following member keeps out of a door's reach + 0.3 unless the player went through it) */
	CHECK(ae_bots_squad_door_keep_out(1.1f, 0.9f, 0) && !ae_bots_squad_door_keep_out(1.3f, 0.9f, 0) &&
		!ae_bots_squad_door_keep_out(1.1f, 0.9f, 1));
	/* (EVERY DOOR: the door ahead, within 45 degrees of the player's facing and 4 units) */
	CHECK(ae_bots_squad_door_ahead(0.7f, 3.9f) && !ae_bots_squad_door_ahead(0.9f, 3.0f) && !ae_bots_squad_door_ahead(0.0f, 4.1f));
	/* JUMP UNDER A LOW CEILING: a lip ahead at the jumping head's height: no jump, a crouched walk on into it; clear: the jump; in
	the air pressing on, crouched from its first tick for AE_SQUAD_JUMP_CROUCH_TICKS */
	CHECK(ae_bots_squad_jump_plan(1, 1, -1) == AE_SQUAD_JUMP_CREEP && ae_bots_squad_jump_plan(1, 0, -1) == AE_SQUAD_JUMP_GO);
	CHECK(ae_bots_squad_jump_plan(0, 0, -1) == AE_SQUAD_JUMP_NONE && ae_bots_squad_jump_plan(0, 1, -1) == AE_SQUAD_JUMP_NONE);
	CHECK(ae_bots_squad_jump_plan(0, 0, 0) == AE_SQUAD_JUMP_NONE && ae_bots_squad_jump_plan(0, 1, 1) == AE_SQUAD_JUMP_AIR &&
		ae_bots_squad_jump_plan(1, 0, AE_SQUAD_JUMP_CROUCH_TICKS) == AE_SQUAD_JUMP_AIR &&
		ae_bots_squad_jump_plan(0, 0, AE_SQUAD_JUMP_CROUCH_TICKS + 1) == AE_SQUAD_JUMP_NONE);
	/* (a lip: the ceiling 0.6 ahead lower than here and under 1.4; the corridor's 1.15 into the tunnel's 1.1 is one, inside it no) */
	CHECK(ae_bots_squad_ceiling_lip(1.15f, 1.1f) && !ae_bots_squad_ceiling_lip(1.1f, 1.1f) && !ae_bots_squad_ceiling_lip(3.0f, 2.0f));
	/* (a crouched walk at a lip that gains under AE_SQUAD_CREEP_GAIN in AE_SQUAD_CREEP_GIVE_UP_TICKS is given up: the jump goes,
	crouched in the air; one that gains, or has not walked so long, goes on) */
	CHECK(!ae_bots_squad_creep_gave_up(-1, 0.0f) && !ae_bots_squad_creep_gave_up(AE_SQUAD_CREEP_GIVE_UP_TICKS - 1, 0.0f));
	CHECK(ae_bots_squad_creep_gave_up(AE_SQUAD_CREEP_GIVE_UP_TICKS, 0.1f) && ae_bots_squad_creep_gave_up(200, AE_SQUAD_CREEP_GAIN - 0.01f));
	CHECK(!ae_bots_squad_creep_gave_up(AE_SQUAD_CREEP_GIVE_UP_TICKS, AE_SQUAD_CREEP_GAIN));
	/* (the squad's log: four short lines of counters, each under 200 letters with every counter at 99999) */
	{
		long values[AE_SQUAD_STAT_COUNT];
		char line[512];
		int stat, group;
		static char const *const groups[] = { "path", "slots", "warps", "cost" };

		memset(values, 0, sizeof values);
		CHECK(ae_bots_squad_stats_line(line, (int)sizeof line, "warps", values) > 0 && strstr(line, "squad warps: ") == line &&
			strstr(line, "stuck 0") && strstr(line, "teleport 0") && strstr(line, "lift 0") && strstr(line, "stranded 0"));
		values[AE_SQUAD_STAT_CLOSEST_PAIR] = 40;
		CHECK(ae_bots_squad_stats_line(line, (int)sizeof line, "slots", values) > 0 && strstr(line, "pair 0.40"));
		CHECK(ae_bots_squad_stats_line(line, (int)sizeof line, "nothing", values) == 0);
		for (stat = 0; stat < AE_SQUAD_STAT_COUNT; stat++)
			values[stat] = 99999;
		for (group = 0; group < 4; group++)
		{
			int length = ae_bots_squad_stats_line(line, (int)sizeof line, groups[group], values);

			CHECK(length > 0 && length < 200 && (int)strlen(line) == length);
		}
		/* (every counter is in one line) */
		for (stat = 0; stat < AE_SQUAD_STAT_COUNT; stat++)
			CHECK(ae_bots_squad_stat_group(stat) >= 0 && ae_bots_squad_stat_group(stat) < 4);
	}
	/* (the squad's path data: a door closes the searches' surfaces only while the level keeps it shut; a machine is never learnt
	in the way, scenery for AE_SQUAD_LEARN_EXPIRY_TICKS) */
	{
		long expires = -1;

		CHECK(ae_bots_squad_door_closes_paths(1, 1) && !ae_bots_squad_door_closes_paths(1, 0) && !ae_bots_squad_door_closes_paths(0, 1));
		CHECK(!ae_bots_squad_learn(1, 100, &expires));
		CHECK(ae_bots_squad_learn(0, 100, &expires) && expires == 100 + AE_SQUAD_LEARN_EXPIRY_TICKS && AE_SQUAD_LEARN_EXPIRY_TICKS == 300);
		/* (a door: closed as it locks, reopened as it unlocks; scenery reopened when its time is up) */
		CHECK(ae_bots_squad_closure_due(1, 1, 0, 0, 50) == 1 && ae_bots_squad_closure_due(1, 0, 1, 0, 50) == -1);
		CHECK(ae_bots_squad_closure_due(1, 1, 1, 0, 50) == 0 && ae_bots_squad_closure_due(1, 0, 0, 0, 50) == 0);
		CHECK(ae_bots_squad_closure_due(0, 0, 1, 400, 399) == 0 && ae_bots_squad_closure_due(0, 0, 1, 400, 400) == -1 &&
			ae_bots_squad_closure_due(0, 0, 0, 400, 500) == 0);
	}
	/* (a squad walk's path: failed when the search found no surface, did not reach, ends over 0.5 from the goal or is over 3 times
	the straight way; a member arrives within 0.35 and sets off again past 0.8; a goal moves at 0.05, a storey counts 10 times) */
	CHECK(ae_bots_squad_path_verdict(1, 0.2f, 4.0f, 3.0f, 1) == AE_SQUAD_PATH_OK);
	/* (a search short of a goal on a closed surface (beside a shut door) whose path ends within 0.5 of it has arrived) */
	CHECK(ae_bots_squad_path_verdict(0, 0.2f, 4.0f, 3.0f, 1) == AE_SQUAD_PATH_OK && ae_bots_squad_path_verdict(0, 0.6f, 1.5f, 1.0f, 1) == AE_SQUAD_PATH_FAILED);
	CHECK(ae_bots_squad_path_verdict(1, 0.6f, 1.5f, 1.0f, 1) == AE_SQUAD_PATH_FAILED);
	/* (a long way across open ground: a search ending short but at least half the way there goes on from its end) */
	CHECK(ae_bots_squad_path_verdict(0, 6.0f, 8.0f, 13.0f, 1) == AE_SQUAD_PATH_OK && ae_bots_squad_path_verdict(0, 7.0f, 8.0f, 13.0f, 1) == AE_SQUAD_PATH_FAILED);
	CHECK(ae_bots_squad_path_verdict(1, 0.2f, 9.1f, 3.0f, 1) == AE_SQUAD_PATH_FAILED);
	CHECK(ae_bots_squad_path_verdict(1, 0.2f, 1.4f, 0.3f, 1) == AE_SQUAD_PATH_OK);
	CHECK(ae_bots_squad_path_verdict(1, 0.2f, 4.0f, 3.0f, 0) == AE_SQUAD_PATH_FAILED);
	/* (no path at all: it ends where the member stands, the whole way short) */
	CHECK(ae_bots_squad_path_verdict(0, 3.0f, 0.0f, 3.0f, 1) == AE_SQUAD_PATH_FAILED);
	CHECK(!ae_bots_squad_walk_state(1, 0.34f) && !ae_bots_squad_walk_state(0, 0.6f) && ae_bots_squad_walk_state(0, 0.81f) &&
		ae_bots_squad_walk_state(1, 0.5f));
	CHECK(ae_bots_squad_goal_moves(ae_bots_squad_goal_shift(0.04f, 0.0f, 0.0f)) == 0 &&
		ae_bots_squad_goal_moves(ae_bots_squad_goal_shift(0.03f, 0.04f, 0.0f)) == 1);
	CHECK(ae_bots_squad_goal_shift(0.0f, 0.0f, 0.2f) < 0.01f && ae_bots_squad_goal_shift(0.0f, 0.0f, 0.4f) > 3.9f);
	CHECK(ae_bots_squad_research(0.5f, 0) && !ae_bots_squad_research(0.49f, 0) && ae_bots_squad_research(0.0f, 1));
	CHECK(AE_SQUAD_IN_POSITION_FOLLOW == 0.8f && AE_SQUAD_ARRIVE == 0.35f && AE_SQUAD_SET_OFF == 0.8f);
	/* (the walkable grid: a step to a neighbour within 0.3 up or down with a clear knee line; a cell's key holds its bsp) */
	CHECK(ae_bots_squad_grid_step_ok(0.25f, 1) && ae_bots_squad_grid_step_ok(-0.3f, 1) && !ae_bots_squad_grid_step_ok(0.35f, 1) &&
		!ae_bots_squad_grid_step_ok(0.1f, 0));
	CHECK(ae_bots_squad_grid_key(0, 1.0f, 2.0f, 0.1f) != ae_bots_squad_grid_key(1, 1.0f, 2.0f, 0.1f));
	CHECK(ae_bots_squad_grid_key(0, 1.01f, 2.02f, 0.1f) == ae_bots_squad_grid_key(0, 1.2f, 2.2f, 0.4f));
	CHECK(ae_bots_squad_grid_key(0, 1.01f, 2.02f, 0.1f) != ae_bots_squad_grid_key(0, 1.26f, 2.02f, 0.1f));
	CHECK(ae_bots_squad_grid_key(0, -0.1f, 0.0f, 0.0f) != ae_bots_squad_grid_key(0, 0.1f, 0.0f, 0.0f));
	/* FOLLOW SLOTS: the player's facing is his motion when he moves, his aim only once turned 60 degrees for a second; a slot is
	in the ring 0.7-1.8, behind him and behind where he will be, its walk not past him, out of door reach; its cost is its distance
	from 1.1 plus 4.2 in another room or off his floor plus 1.7 by a threat; men take slots by the least total, 0.8 apart, a kept
	slot 0.3 cheaper; a re-plan only on the ground, when a slot fails or he moved 1 and changed room or 3 s went */
	{
		static float cost[3][AE_SQUAD_SLOTS_MAX];
		static float where[AE_SQUAD_SLOTS_MAX][2];
		int slot_of_man[3], kept[3] = { -1, -1, -1 };

		CHECK(fabs(ae_bots_squad_leader_yaw(0.0f, 2.0f, 0.0f, 0.0f, 0) - 1.5708f) < 0.01f);
		CHECK(ae_bots_squad_leader_yaw(0.1f, 0.0f, 1.5f, 0.0f, 29) == 0.0f && ae_bots_squad_leader_yaw(0.1f, 0.0f, 1.5f, 0.0f, 30) == 1.5f);
		CHECK(ae_bots_squad_leader_yaw(0.1f, 0.0f, 0.9f, 0.0f, 90) == 0.0f);
		CHECK(ae_bots_squad_slot_ok(1.0f, -0.9f, -0.9f, 0.8f, 0, 1) && !ae_bots_squad_slot_ok(0.6f, -0.9f, -0.9f, 0.8f, 0, 1) &&
			!ae_bots_squad_slot_ok(1.9f, -0.9f, -0.9f, 0.8f, 0, 1));
		CHECK(!ae_bots_squad_slot_ok(1.0f, -0.9f, -0.2f, 0.8f, 0, 1) && ae_bots_squad_slot_ok(1.0f, -0.2f, 0.5f, 0.8f, 0, 0) &&
			!ae_bots_squad_slot_ok(1.0f, 0.1f, -0.9f, 0.8f, 0, 0));
		CHECK(!ae_bots_squad_slot_ok(1.0f, -0.9f, -0.9f, 0.4f, 0, 1) && !ae_bots_squad_slot_ok(1.0f, -0.9f, -0.9f, 0.8f, 1, 1));
		CHECK(ae_bots_squad_slot_cost(1.1f, 0, 0, 0) == 0.0f && ae_bots_squad_slot_cost(0.8f, 0, 0, 0) < ae_bots_squad_slot_cost(1.6f, 0, 0, 0));
		CHECK(fabs(ae_bots_squad_slot_cost(1.1f, 1, 0, 0) - 4.2f) < 0.001f && fabs(ae_bots_squad_slot_cost(1.1f, 0, 1, 1) - 5.9f) < 0.001f);
		/* (two men crossing: the greedy first pick would leave the second far; the least total swaps them) */
		memset(cost, 0, sizeof cost);
		where[0][0] = 0.0f; where[0][1] = 0.0f;
		where[1][0] = 2.0f; where[1][1] = 0.0f;
		where[2][0] = 0.3f; where[2][1] = 0.0f;
		cost[0][0] = 1.0f; cost[0][1] = 1.5f; cost[0][2] = 9.0f;
		cost[1][0] = 1.2f; cost[1][1] = 9.0f; cost[1][2] = 9.0f;
		CHECK(ae_bots_squad_slot_assign(cost, (float const (*)[2])where, 2, 3, kept, slot_of_man) &&
			slot_of_man[0] == 1 && slot_of_man[1] == 0);
		/* (a slot within 0.8 of a taken one is out: man 1 cannot take slot 2 beside slot 0) */
		cost[0][1] = 9.0f; cost[1][0] = 9.0f; cost[1][2] = 0.1f; cost[1][1] = 3.0f;
		CHECK(ae_bots_squad_slot_assign(cost, (float const (*)[2])where, 2, 3, kept, slot_of_man) &&
			slot_of_man[0] == 0 && slot_of_man[1] == 1);
		/* (a kept slot is 0.3 cheaper: man 0 keeps slot 1 over a slot 0.2 cheaper) */
		cost[0][0] = 1.0f; cost[0][1] = 1.2f; cost[1][1] = 9.0f; cost[1][2] = 9.0f; cost[1][0] = 9.0f;
		where[1][0] = 2.0f;
		kept[0] = 1;
		CHECK(ae_bots_squad_slot_assign(cost, (float const (*)[2])where, 1, 3, kept, slot_of_man) && slot_of_man[0] == 1);
		CHECK(!ae_bots_squad_replan(2.0f, 1, 200, 1, 1) && ae_bots_squad_replan(0.0f, 0, 0, 1, 0));
		CHECK(!ae_bots_squad_replan(0.5f, 1, 200, 0, 0) && ae_bots_squad_replan(1.0f, 1, 10, 0, 0) &&
			ae_bots_squad_replan(1.0f, 0, 90, 0, 0) && !ae_bots_squad_replan(1.0f, 0, 89, 0, 0));
	}
	/* (a member's mode changes only once the new one was wanted 1.5 s, except to a seat, a mount or a door at once; a step aside
	only with no wall within 0.5 that way; a turn back over 150 degrees within a second out of a fight is an oscillation; in file
	indoors 0.8 apart) */
	CHECK(ae_bots_squad_mode_pick(AE_SQUAD_MODE_TRACK, AE_SQUAD_MODE_SLOT, 44) == AE_SQUAD_MODE_SLOT &&
		ae_bots_squad_mode_pick(AE_SQUAD_MODE_TRACK, AE_SQUAD_MODE_SLOT, 45) == AE_SQUAD_MODE_TRACK);
	CHECK(ae_bots_squad_mode_pick(AE_SQUAD_MODE_DOOR, AE_SQUAD_MODE_SLOT, 0) == AE_SQUAD_MODE_DOOR &&
		ae_bots_squad_mode_pick(AE_SQUAD_MODE_SEAT, AE_SQUAD_MODE_TRACK, 0) == AE_SQUAD_MODE_SEAT &&
		ae_bots_squad_mode_pick(AE_SQUAD_MODE_SLOT, AE_SQUAD_MODE_SLOT, 0) == AE_SQUAD_MODE_SLOT);
	CHECK(!ae_bots_squad_override_ok(0.4f) && ae_bots_squad_override_ok(0.6f));
	/* (a turn back as the player turned back is following him, no oscillation) */
	CHECK(ae_bots_squad_oscillation_after(0.0f, 3.0f, 20, 0, 90) && !ae_bots_squad_oscillation_after(0.0f, 3.0f, 20, 0, 89));
	CHECK(ae_bots_squad_oscillation(0.0f, 3.0f, 20, 0) && !ae_bots_squad_oscillation(0.0f, 3.0f, 31, 0) &&
		!ae_bots_squad_oscillation(0.0f, 2.5f, 10, 0) && !ae_bots_squad_oscillation(0.0f, 3.0f, 10, 1));
	CHECK(AE_SQUAD_FILE_GAP == 0.8f);
	/* (MAKE WAY: a member in the player's way (within 1.5 ahead, 0.3 across, him moving) steps to the side with 0.6 of room, else
	walks on ahead; FILE REVERSAL: a turn back over 120 degrees in a corridor under 1.6 wide) */
	CHECK(ae_bots_squad_in_strip(1.0f, 0.2f, 1.0f) && !ae_bots_squad_in_strip(1.6f, 0.0f, 1.0f) && !ae_bots_squad_in_strip(1.0f, 0.4f, 1.0f) &&
		!ae_bots_squad_in_strip(-0.2f, 0.0f, 1.0f) && !ae_bots_squad_in_strip(1.0f, 0.0f, 0.2f));
	CHECK(ae_bots_squad_make_way(0.7f, 0.2f) == AE_SQUAD_WAY_LEFT && ae_bots_squad_make_way(0.2f, 0.9f) == AE_SQUAD_WAY_RIGHT &&
		ae_bots_squad_make_way(0.8f, 0.9f) == AE_SQUAD_WAY_RIGHT && ae_bots_squad_make_way(0.5f, 0.5f) == AE_SQUAD_WAY_AHEAD);
	/* the side a member made way to is kept, unless its room closed under AE_SQUAD_WAY_ROOM with more on the other side */
	CHECK(ae_bots_squad_make_way_kept(AE_SQUAD_WAY_RIGHT, 1.0f, 0.7f) == AE_SQUAD_WAY_RIGHT);
	CHECK(ae_bots_squad_make_way_kept(AE_SQUAD_WAY_RIGHT, 1.0f, 0.36f) == AE_SQUAD_WAY_LEFT);
	CHECK(ae_bots_squad_make_way_kept(AE_SQUAD_WAY_LEFT, 0.2f, 0.9f) == AE_SQUAD_WAY_RIGHT);
	CHECK(ae_bots_squad_make_way_kept(AE_SQUAD_WAY_LEFT, 0.3f, 0.2f) == AE_SQUAD_WAY_AHEAD);
	CHECK(ae_bots_squad_make_way_kept(AE_SQUAD_WAY_LEFT, 0.5f, 0.4f) == AE_SQUAD_WAY_AHEAD);
	CHECK(ae_bots_squad_make_way_kept(AE_SQUAD_WAY_AHEAD, 0.5f, 0.4f) == AE_SQUAD_WAY_AHEAD);
	CHECK(ae_bots_squad_make_way_kept(AE_SQUAD_WAY_AHEAD, 0.9f, 0.4f) == AE_SQUAD_WAY_AHEAD);
	/* a squadmate close ahead (dx, dy, dz, the member's heading, the squadmate's velocity): walking on away, the member slows behind
	it on its way; coming back or across it, the member backs off; standing, far, beside, behind or a storey off: nothing */
	CHECK(ae_bots_squad_yield(1.0f, 0.0f, 0.0f, 0.0f, 0.05f, 0.0f) == AE_SQUAD_YIELD_SLOW);
	CHECK(ae_bots_squad_yield(0.7f, 0.3f, 0.2f, 0.0f, 0.04f, 0.01f) == AE_SQUAD_YIELD_SLOW);
	CHECK(ae_bots_squad_yield(1.0f, 0.0f, 0.0f, 0.0f, -0.05f, 0.0f) == AE_SQUAD_YIELD_BACK);
	CHECK(ae_bots_squad_yield(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.05f) == AE_SQUAD_YIELD_BACK);
	CHECK(ae_bots_squad_yield(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f) == AE_SQUAD_YIELD_NONE);
	CHECK(ae_bots_squad_yield(1.2f, 0.0f, 0.0f, 0.0f, 0.05f, 0.0f) == AE_SQUAD_YIELD_NONE);
	CHECK(ae_bots_squad_yield(0.0f, 1.0f, 0.0f, 0.0f, 0.05f, 0.0f) == AE_SQUAD_YIELD_NONE);
	CHECK(ae_bots_squad_yield(-1.0f, 0.0f, 0.0f, 0.0f, 0.05f, 0.0f) == AE_SQUAD_YIELD_NONE);
	CHECK(ae_bots_squad_yield(1.0f, 0.0f, 2.0f, 0.0f, 0.05f, 0.0f) == AE_SQUAD_YIELD_NONE);
	CHECK(ae_bots_squad_file_reversal(130.0f, 1.2f) && !ae_bots_squad_file_reversal(110.0f, 1.2f) && !ae_bots_squad_file_reversal(130.0f, 1.7f));
	/* (DOOR KEEP-OUT as a filter: a place within a door's reach + 0.3 is out unless the walk goes through that door; the player
	is through at 0.9 past its plane and back at 0.9 the other way; stack places within 1.6; the same eighth of a turn to the
	door and to the stack point costs 2; stacked only with every member within 0.5 of a place within 1.6 of the door) */
	CHECK(ae_bots_squad_in_any_door_reach(1.1f, 0.9f, 0) && !ae_bots_squad_in_any_door_reach(1.1f, 0.9f, 1) &&
		!ae_bots_squad_in_any_door_reach(1.3f, 0.9f, 0));
	CHECK(!ae_bots_squad_player_through(0.8f, 0) && ae_bots_squad_player_through(0.9f, 0) && ae_bots_squad_player_through(0.0f, 1) &&
		ae_bots_squad_player_through(-0.8f, 1) && !ae_bots_squad_player_through(-0.9f, 1));
	CHECK(AE_SQUAD_PLACE_REACH == 1.6f);
	CHECK(ae_bots_squad_stack_sector(0.1f, 0.3f) == -2 && ae_bots_squad_stack_sector(0.1f, 1.0f) == 0 &&
		ae_bots_squad_stack_sector(3.1f, -3.1f) == -2);
	CHECK(ae_bots_squad_stacked(2, 2, 1.5f) && !ae_bots_squad_stacked(1, 2, 1.0f) && !ae_bots_squad_stacked(2, 2, 1.7f) &&
		!ae_bots_squad_stacked(0, 0, 0.0f));
	/* (a fight indoors on narrow ground stays within 3 of the player, else 6; a firing line passes 0.4 or more from him; the pace:
	full far from the slot or the player, his own near the slot with a pull toward it, none once arrived) */
	CHECK(ae_bots_squad_fight_leash(1, 1) == 3.0f && ae_bots_squad_fight_leash(0, 1) == 6.0f && ae_bots_squad_fight_leash(1, 0) == 6.0f);
	CHECK(ae_bots_squad_fire_line_ok(0.4f) && !ae_bots_squad_fire_line_ok(0.39f));
	CHECK(ae_bots_squad_throttle(1.0f, 2.0f, 1.4f, 1.0f) == 1.0f && ae_bots_squad_throttle(1.0f, 2.0f, 0.5f, 2.8f) == 1.0f);
	CHECK(fabs(ae_bots_squad_throttle(1.0f, 2.0f, 0.55f, 1.0f) - 0.6f) < 0.001f);
	CHECK(ae_bots_squad_throttle(0.0f, 2.0f, 0.4f, 1.0f) == 0.3f && ae_bots_squad_throttle(1.0f, 2.0f, 0.3f, 1.0f) == 0.0f);
	/* (a jump crumb is jumped within 0.6 of it, not on the way to it; one member crosses at a time: another waits while the one
	crossing is within 1.0 of the crumb, 3 s at most) */
	CHECK(ae_bots_squad_cue_jump_now(0.6f) && !ae_bots_squad_cue_jump_now(0.7f));
	CHECK(ae_bots_squad_crossing_waits(1, 0.9f, 10) && !ae_bots_squad_crossing_waits(1, 1.1f, 10) &&
		!ae_bots_squad_crossing_waits(1, 0.5f, 90) && !ae_bots_squad_crossing_waits(0, 0.5f, 10));
	/* (the cost's 99th percentile from a histogram of tenths of a millisecond: 99 ticks at 0.3 and one at 9 give 0.3) */
	{
		long histogram[AE_SQUAD_COST_BUCKETS];

		memset(histogram, 0, sizeof histogram);
		histogram[3] = 99;
		histogram[AE_SQUAD_COST_BUCKETS - 1] = 1;
		CHECK(ae_bots_squad_cost_p99(histogram) == 30);
		histogram[3] = 10;
		histogram[AE_SQUAD_COST_BUCKETS - 1] = 10;
		CHECK(ae_bots_squad_cost_p99(histogram) == (AE_SQUAD_COST_BUCKETS - 1) * 10);
		memset(histogram, 0, sizeof histogram);
		CHECK(ae_bots_squad_cost_p99(histogram) == 0);
	}
	/* (a member may be stalled only while its walk failed lately, it is cut off or it walks the track; never in cover or at its
	slot, where it stands on purpose) */
	CHECK(ae_bots_squad_stalled(1, 0, 0, 0, 0, 0) && ae_bots_squad_stalled(0, 1, 0, 0, 0, 0) && ae_bots_squad_stalled(0, 0, 1, 0, 0, 0));
	CHECK(!ae_bots_squad_stalled(0, 0, 0, 0, 0, 0) && !ae_bots_squad_stalled(1, 1, 1, 1, 0, 0) && !ae_bots_squad_stalled(1, 0, 1, 0, 1, 0));
	/* (on an order away from the player (go here, attack, defend, watch) its walk is the order's: a warp to him would undo it) */
	CHECK(!ae_bots_squad_stalled(1, 1, 1, 0, 0, 1));
	/* (a report line: written when its text changed, when AE_SQUAD_REPORT_QUIET_TICKS passed since it was last written, or when
	the game's time went back (a new level); not otherwise) */
	CHECK(ae_bots_squad_report_due("", "squad walk: follow ticks 0", -1, 0));
	CHECK(!ae_bots_squad_report_due("squad walk: follow ticks 5", "squad walk: follow ticks 5", 100, 130));
	CHECK(ae_bots_squad_report_due("squad walk: follow ticks 5", "squad walk: follow ticks 6", 100, 130));
	CHECK(!ae_bots_squad_report_due("squad walk: a", "squad walk: a", 100, 100 + AE_SQUAD_REPORT_QUIET_TICKS - 1));
	CHECK(ae_bots_squad_report_due("squad walk: a", "squad walk: a", 100, 100 + AE_SQUAD_REPORT_QUIET_TICKS));
	CHECK(ae_bots_squad_report_due("squad walk: a", "squad walk: a", 100, 40));
	/* (OUTCOME: the 90th percentile of a member's gap to the player, its buckets' upper edge in hundredths of a unit; a tick
	stuck: it wants to walk, under AE_SQUAD_STUCK_SPEED, not fighting) */
	{
		long gaps[AE_SQUAD_GAP_BUCKETS];

		memset(gaps, 0, sizeof gaps);
		CHECK(ae_bots_squad_gap_p90(gaps) == 0);
		gaps[3] = 100;
		CHECK(ae_bots_squad_gap_p90(gaps) == 200);
		gaps[10] = 12;
		CHECK(ae_bots_squad_gap_p90(gaps) == 550);
		gaps[AE_SQUAD_GAP_BUCKETS - 1] = 1000;
		CHECK(ae_bots_squad_gap_p90(gaps) == AE_SQUAD_GAP_BUCKETS * 50);
	}
	CHECK(ae_bots_squad_stuck_tick(1, 0.1f, 0) && !ae_bots_squad_stuck_tick(0, 0.1f, 0));
	CHECK(!ae_bots_squad_stuck_tick(1, AE_SQUAD_STUCK_SPEED, 0) && !ae_bots_squad_stuck_tick(1, 0.1f, 1));
	return 0;
}
