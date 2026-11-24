#include <stdio.h>
#include <stdlib.h>
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
	return 0;
}
