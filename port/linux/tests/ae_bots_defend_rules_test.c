#include <stdio.h>
#include "ae_bots_defend_rules.h"
#include "ae_bots_combat_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	struct ae_defend_numbers a, b;
	float c, m;

	CHECK(ae_defend_profile(4) == AE_DEFEND_ARENA);
	CHECK(ae_defend_profile(5) == AE_DEFEND_ARENA);
	CHECK(ae_defend_profile(6) == AE_DEFEND_BTB);
	ae_defend_numbers_of(AE_DEFEND_ARENA, &a);
	ae_defend_numbers_of(AE_DEFEND_BTB, &b);
	CHECK(a.radius == 8.0f && a.spacing == 3.0f && a.leash == 2.0f);
	CHECK(b.radius == 12.0f && b.spacing == 4.5f && b.leash == 2.0f && b.vehicle_block);
	CHECK(b.sniper_weight > a.sniper_weight && b.morale_decay_seconds > a.morale_decay_seconds);

	/* scope */
	CHECK(ae_defend_in_scope(1, AE_DEFEND_ARENA, 1, _ae_order_defend, AE_ROLE_NONE));
	CHECK(!ae_defend_in_scope(1, AE_DEFEND_ARENA, 0, _ae_order_defend, AE_ROLE_DEFEND_OURS)); /* arena roles untouched */
	CHECK(ae_defend_in_scope(1, AE_DEFEND_BTB, 0, _ae_order_defend, AE_ROLE_DEFEND_OURS));
	CHECK(ae_defend_in_scope(1, AE_DEFEND_BTB, 0, _ae_order_defend, AE_ROLE_HILL));
	CHECK(ae_defend_in_scope(1, AE_DEFEND_BTB, 0, _ae_order_defend, AE_ROLE_SITE_A));
	CHECK(!ae_defend_in_scope(1, AE_DEFEND_BTB, 0, _ae_order_watch, AE_ROLE_SITE_B));
	CHECK(!ae_defend_in_scope(1, AE_DEFEND_BTB, 0, _ae_order_defend, AE_ROLE_PROTECT));
	CHECK(!ae_defend_in_scope(1, AE_DEFEND_BTB, 1, _ae_order_watch, AE_ROLE_NONE));
	CHECK(!ae_defend_in_scope(0, AE_DEFEND_ARENA, 1, _ae_order_defend, AE_ROLE_NONE)); /* switch_off_no_scope */
	CHECK(!ae_defend_in_scope(0, AE_DEFEND_BTB, 0, _ae_order_defend, AE_ROLE_DEFEND_OURS));

	/* direction-aware protection: walls toward the threat beat the same count away from it */
	{
		float yaw = 0.0f;
		unsigned t = ae_defend_threat_sides(&yaw, 1);

		CHECK(t & 1u);
		CHECK(ae_defend_place_score(0x01u, 0, t, 3.0f, 8.0f, 1) > ae_defend_place_score(0x10u, 0, t, 3.0f, 8.0f, 1));
		CHECK(ae_defend_place_score(0x03u, 0, t, 3.0f, 8.0f, 1) > ae_defend_place_score(0x03u, 0, t, 3.0f, 8.0f, 0)); /* sees its way */
		CHECK(ae_defend_place_score(0x03u, 0, t, 2.0f, 8.0f, 1) > ae_defend_place_score(0x03u, 0, t, 7.0f, 8.0f, 1)); /* closer */
		CHECK(ae_defend_place_score(0x03u, 0, t, 8.5f, 8.0f, 1) <= 0.0f); /* outside the area: unusable */
		CHECK(ae_defend_place_score(0x00u, 0, t, 2.0f, 8.0f, 1) <= 0.0f); /* open ground: not a place */
		CHECK(ae_defend_place_score(0x00u, 1, t, 2.0f, 8.0f, 0) > 0.0f);
		CHECK(ae_defend_place_score(0x03u, 0, t, 7.99f, 8.0f, 1) > 0.0f);
	}
	/* (a yaw round the circle: pi is side 4, a negative quarter turn side 6) */
	{
		float yaws[2] = { 3.14159265f, -1.5707963f };
		unsigned t = ae_defend_threat_sides(yaws, 2);

		CHECK((t & (1u << 4)) && (t & (1u << 6)) && (t & (1u << 3)) && (t & (1u << 7)) && !(t & 1u));
	}
	CHECK(!ae_defend_spaced(2.9f, 3.0f) && ae_defend_spaced(3.0f, 3.0f) && ae_defend_spaced(-1.0f, 3.0f)); /* -1: none */
	CHECK(ae_defend_within_leash(9.9f, 8.0f, 2.0f) && !ae_defend_within_leash(10.1f, 8.0f, 2.0f));

	/* hold: exposure marks, sticky, moving in the open */
	CHECK(!ae_defend_exposed_step(0, 29.0f, 300, AE_BOTS_LEVEL_HEROIC));	/* below the enter mark */
	CHECK(ae_defend_exposed_step(0, 31.0f, 300, AE_BOTS_LEVEL_HEROIC));
	CHECK(ae_defend_exposed_step(1, 21.0f, 300, AE_BOTS_LEVEL_HEROIC));	/* still exposed until below 20 */
	CHECK(!ae_defend_exposed_step(1, 19.0f, 300, AE_BOTS_LEVEL_HEROIC));
	CHECK(!ae_defend_exposed_step(0, 50.0f, 10, AE_BOTS_LEVEL_HEROIC));	/* just arrived: settles first */
	CHECK(ae_defend_exposure(0, 0, 0, 0) == 0.0f && ae_defend_exposure(3, 3, 1, 1) <= 70.0f);
	CHECK(ae_defend_exposure(1, 1, 1, 1) > ae_defend_exposure(1, 0, 0, 0));
	CHECK(ae_defend_sticky(10.0f, 0, AE_BOTS_LEVEL_HEROIC) == 15.0f);	/* +50 % while fresh */
	CHECK(ae_defend_sticky(10.0f, 3 * 30 + 1, AE_BOTS_LEVEL_HEROIC) == 10.0f);
	CHECK(ae_defend_sticky(10.0f, 0, AE_BOTS_LEVEL_RECRUIT) < 15.0f);	/* lower skill holds worse */
	for (c = 0.0f; c <= 1.0f; c += 0.25f)
	{
		CHECK(ae_defend_jink_ticks(AE_DEFEND_ARENA, c) >= 9 && ae_defend_jink_ticks(AE_DEFEND_ARENA, c) <= 30);
		CHECK(ae_defend_jink_ticks(AE_DEFEND_ARENA, c) <= ae_defend_jink_ticks(AE_DEFEND_BTB, c));
	}
	CHECK(ae_defend_open_action(AE_DEFEND_BTB, 1, 1) == AE_DEFEND_OPEN_BREAK_LINE);
	CHECK(ae_defend_open_action(AE_DEFEND_ARENA, 0, 1) == AE_DEFEND_OPEN_JINK);
	CHECK(ae_defend_open_action(AE_DEFEND_BTB, 1, 0) == AE_DEFEND_OPEN_JINK);	/* no breaker: keep moving */

	/* morale and the phases */
	m = ae_defend_morale_step(0.0f, 2, 2, 0, 1.0f, 4.0f);
	CHECK(m > 0.0f);
	CHECK(ae_defend_morale_step(0.8f, 0, 0, 0, 4.0f, 4.0f) < 0.8f * 0.4f);	/* ~1/e per decay time */
	CHECK(ae_defend_morale_step(0.9f, 5, 5, 1, 1.0f, 4.0f) <= 1.0f);
	CHECK(ae_defend_morale_step(0.0f, 0, 0, 0, 1.0f, 0.0f) == 0.0f);	/* no decay time: no divide by zero */
	/* (a member not yet spawned is not down; one seen alive and gone is) */
	CHECK(!ae_defend_member_down(0, 0) && !ae_defend_member_down(1, 1) && !ae_defend_member_down(0, 1) && ae_defend_member_down(1, 0));
	/* human-led: a quiet leader death holds; leader dead AND broken falls back */
	CHECK(ae_defend_phase_step(AE_DEFEND_HOLD, 1, 1, 0.9f, 0, 0, 3, 4, 1) == AE_DEFEND_HOLD);
	CHECK(ae_defend_phase_step(AE_DEFEND_HOLD, 1, 0, 0.3f, 0, 0, 3, 4, 1) == AE_DEFEND_HOLD);
	CHECK(ae_defend_phase_step(AE_DEFEND_HOLD, 1, 0, 0.7f, 0, 0, 3, 4, 1) == AE_DEFEND_FALL_BACK);
	/* bot-only: broken falls back */
	CHECK(ae_defend_phase_step(AE_DEFEND_HOLD, 0, 0, 0.7f, 0, 0, 3, 4, 1) == AE_DEFEND_FALL_BACK);
	CHECK(ae_defend_phase_step(AE_DEFEND_FALL_BACK, 0, 0, 0.7f, 1, 0, 3, 4, 0) == AE_DEFEND_REGROUP);
	CHECK(ae_defend_phase_step(AE_DEFEND_FALL_BACK, 0, 0, 0.7f, 0, 0, 3, 4, 0) == AE_DEFEND_FALL_BACK);
	/* regroup first, then return only when calm (and leader alive / enough back) */
	CHECK(ae_defend_phase_step(AE_DEFEND_REGROUP, 0, 0, 0.1f, 1, 0, 3, 4, 0) == AE_DEFEND_REGROUP);
	CHECK(ae_defend_phase_step(AE_DEFEND_REGROUP, 0, 0, 0.1f, 1, 1, 3, 4, 0) == AE_DEFEND_RETURN);
	CHECK(ae_defend_phase_step(AE_DEFEND_REGROUP, 1, 0, 0.1f, 1, 1, 3, 4, 0) == AE_DEFEND_REGROUP);	/* leader not back */
	CHECK(ae_defend_phase_step(AE_DEFEND_REGROUP, 1, 1, 0.1f, 1, 1, 3, 4, 0) == AE_DEFEND_RETURN);	/* return_goes_to_anchor_not_leader: a phase, not a leader goal */
	CHECK(ae_defend_phase_step(AE_DEFEND_REGROUP, 0, 0, 0.3f, 1, 1, 3, 4, 0) == AE_DEFEND_REGROUP);	/* not calm yet */
	CHECK(ae_defend_phase_step(AE_DEFEND_REGROUP, 0, 0, 0.1f, 1, 1, 1, 4, 0) == AE_DEFEND_REGROUP);	/* 1 of 4: wait */
	CHECK(ae_defend_phase_step(AE_DEFEND_REGROUP, 0, 0, 0.1f, 1, 1, 1, 1, 0) == AE_DEFEND_RETURN);	/* lone_survivor_returns */
	CHECK(ae_defend_phase_step(AE_DEFEND_REGROUP, 0, 0, 0.1f, 1, 1, 1, 0, 0) == AE_DEFEND_RETURN);	/* (no squad left: no lock) */
	CHECK(ae_defend_phase_step(AE_DEFEND_RETURN, 0, 0, 0.1f, 0, 0, 3, 4, 1) == AE_DEFEND_HOLD);
	CHECK(ae_defend_phase_step(AE_DEFEND_RETURN, 0, 0, 0.1f, 0, 0, 3, 4, 0) == AE_DEFEND_RETURN);
	CHECK(ae_defend_objective_break(1, 1, 0, 0) && ae_defend_objective_break(1, 0, 1, 0) && ae_defend_objective_break(1, 0, 0, 1));
	CHECK(!ae_defend_objective_break(0, 1, 1, 1) && !ae_defend_objective_break(1, 0, 0, 0));

	if (failures)
		return 1;
	printf("ae_bots_defend_rules: all passed\n");
	return 0;
}
