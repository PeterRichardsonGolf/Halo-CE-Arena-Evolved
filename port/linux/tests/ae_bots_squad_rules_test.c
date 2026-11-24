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
	CHECK(!ae_bots_squad_in_position(AE_SQUAD_ORDER_REGROUP, AE_SQUAD_IN_POSITION_FOLLOW));
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
	return 0;
}
