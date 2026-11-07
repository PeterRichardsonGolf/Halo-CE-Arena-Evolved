#include <stdio.h>
#include <string.h>
#include "ae_fireteam_rules.h"
#include "ae_wheel_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static void brains(short *list, short first, short count)
{
	short i;

	for (i = 0; i < count; i++)
		list[i] = (short)(first + i);
}

/* the sizes of the fireteams of a team, in table order */
static short sizes(struct ae_fireteam_table const *table, short team, short *out)
{
	short i, n = 0;

	for (i = 0; i < table->count; i++)
		if (table->fireteams[i].team == team)
			out[n++] = table->fireteams[i].members;
	return n;
}

static void split_even_test(void)
{
	struct ae_fireteam_table table;
	long leaders[2] = { 10, 11 };
	short bots[40], out[16], n;

	ae_fireteam_clear(&table);
	brains(bots, 0, 7);
	ae_fireteam_split(&table, 0, leaders, 2, bots, 7);
	n = sizes(&table, 0, out);
	CHECK(n == 2 && out[0] == 4 && out[1] == 3);
	CHECK(ae_fireteam_leader(&table, ae_fireteam_of_bot(&table, 0)) == 10);
	CHECK(ae_fireteam_leader(&table, ae_fireteam_of_bot(&table, 6)) == 11);
	CHECK(ae_fireteam_check(&table));
}

static void split_cap_test(void)
{
	struct ae_fireteam_table table;
	long leaders[2] = { 10, 11 };
	short bots[40], out[16], n, i;

	/* 12 bots, 2 leaders: 5 + 5 and the 2 left over join a fireteam, never a squad of 2 */
	ae_fireteam_clear(&table);
	brains(bots, 0, 12);
	ae_fireteam_split(&table, 0, leaders, 2, bots, 12);
	n = sizes(&table, 0, out);
	CHECK(n == 2);
	CHECK(out[0] + out[1] == 12);
	CHECK(ae_fireteam_check(&table));
	/* 16 bots, 2 leaders: 5 + 5 and 6 left: two squads of 3 */
	ae_fireteam_clear(&table);
	brains(bots, 0, 16);
	ae_fireteam_split(&table, 0, leaders, 2, bots, 16);
	n = sizes(&table, 0, out);
	CHECK(n == 4 && out[0] == 5 && out[1] == 5 && out[2] == 3 && out[3] == 3);
	CHECK(ae_fireteam_leader(&table, ae_fireteam_of_bot(&table, 15)) == -1);
	/* 40 bots, no leader: squads of 3 to 5 only */
	ae_fireteam_clear(&table);
	brains(bots, 0, 40);
	ae_fireteam_split(&table, 1, NULL, 0, bots, 40);
	n = sizes(&table, 1, out);
	CHECK(n == 8);
	for (i = 0; i < n; i++)
		CHECK(out[i] >= 3 && out[i] <= 5);
	CHECK(ae_fireteam_check(&table));
	/* 2 bots alone: one squad of 2, not none */
	ae_fireteam_clear(&table);
	ae_fireteam_split(&table, 0, NULL, 0, bots, 2);
	n = sizes(&table, 0, out);
	CHECK(n == 1 && out[0] == 2);
	/* a leader and no bots: a fireteam of no one */
	ae_fireteam_clear(&table);
	ae_fireteam_split(&table, 0, leaders, 1, bots, 0);
	n = sizes(&table, 0, out);
	CHECK(n == 1 && out[0] == 0);
	CHECK(ae_fireteam_lead_brain(&table, 0) == -1);
}

static void recruit_test(void)
{
	struct ae_fireteam_table table;
	long leaders[2] = { 10, 11 };
	short bots[8], a, b;

	ae_fireteam_clear(&table);
	brains(bots, 0, 3);
	ae_fireteam_split(&table, 0, leaders, 2, bots, 3);
	a = (short)ae_fireteam_of_bot(&table, 0);
	b = (short)ae_fireteam_of_bot(&table, 2);
	CHECK(a != b && table.fireteams[a].members == 2 && table.fireteams[b].members == 1);
	/* the lone bot of b cannot leave it */
	CHECK(ae_fireteam_recruit(&table, 2, a) == 0);
	CHECK(ae_fireteam_of_bot(&table, 2) == b);
	/* a bot of a may go to b */
	CHECK(ae_fireteam_recruit(&table, 1, b) == 1);
	CHECK(ae_fireteam_of_bot(&table, 1) == b);
	CHECK(table.fireteams[a].members == 1 && table.fireteams[b].members == 2);
	/* already there, a free slot, an unknown bot */
	CHECK(ae_fireteam_recruit(&table, 1, b) == 0);
	CHECK(ae_fireteam_recruit(&table, 1, 40) == 0);
	CHECK(ae_fireteam_recruit(&table, 99, b) == 0);
	CHECK(ae_fireteam_check(&table));
	CHECK(ae_fireteam_lead_brain(&table, b) == 1);
}

static void leader_left_test(void)
{
	struct ae_fireteam_table table;
	long leaders[2] = { 10, 11 };
	short bots[16], i, n, out[16];

	ae_fireteam_clear(&table);
	brains(bots, 0, 16);
	ae_fireteam_split(&table, 0, leaders, 2, bots, 16);
	ae_fireteam_leader_left(&table, 10);
	CHECK(ae_fireteam_check(&table));
	for (i = 0; i < 16; i++)
		CHECK(ae_fireteam_of_bot(&table, i) >= 0);
	for (i = 0; i < table.count; i++)
		CHECK(table.fireteams[i].team == -1 || table.fireteams[i].leader != 10);
	/* the last leader leaving makes the one fireteam a bot squad, bots kept */
	ae_fireteam_clear(&table);
	ae_fireteam_split(&table, 0, leaders, 1, bots, 4);
	ae_fireteam_leader_left(&table, 10);
	n = sizes(&table, 0, out);
	CHECK(n == 1 && out[0] == 4);
	CHECK(ae_fireteam_leader(&table, ae_fireteam_of_bot(&table, 0)) == -1);
	CHECK(ae_fireteam_check(&table));
}

static void sequence_test(void)
{
	struct ae_fireteam_table table;
	long leaders[3] = { 10, 11, 12 };
	long leaders1[2] = { 20, 21 };
	short bots[40], i, round;

	/* both teams, bots coming and going, a recruit between: always one fireteam each */
	ae_fireteam_clear(&table);
	for (round = 0; round < 30; round++)
	{
		short count0 = (short)((round * 7) % 20), count1 = (short)((round * 5) % 17);

		brains(bots, 0, count0);
		ae_fireteam_split(&table, 0, leaders, (short)(round % 3), bots, count0);
		brains(bots, 60, count1);
		ae_fireteam_split(&table, 1, leaders1, (short)(round % 3), bots, count1);
		if (count0 > 2)
			ae_fireteam_recruit(&table, 0, (short)ae_fireteam_of_bot(&table, 1));
		if (round % 4 == 0)
			ae_fireteam_leader_left(&table, 11);
		CHECK(ae_fireteam_check(&table));
		for (i = 0; i < count0; i++)
			CHECK(ae_fireteam_of_bot(&table, i) >= 0 && table.fireteams[ae_fireteam_of_bot(&table, i)].team == 0);
		for (i = 0; i < count1; i++)
			CHECK(ae_fireteam_of_bot(&table, (short)(60 + i)) >= 0 &&
				table.fireteams[ae_fireteam_of_bot(&table, (short)(60 + i))].team == 1);
		for (i = count0; i < 60; i++)
			CHECK(ae_fireteam_of_bot(&table, i) == -1);
	}
}

static void order_test(void)
{
	struct ae_fireteam_table table;
	long leaders[2] = { 10, 11 };
	short bots[40];
	float spot[3] = { 1.0f, 2.0f, 3.0f }, got[3];
	int fireteam, other;

	ae_fireteam_clear(&table);
	brains(bots, 0, 8);
	ae_fireteam_split(&table, 0, leaders, 2, bots, 8);
	fireteam = ae_fireteam_of_bot(&table, 0);
	other = ae_fireteam_of_bot(&table, 7);
	CHECK(fireteam != other);
	CHECK(ae_fireteam_order_kind(&table, fireteam) == _ae_order_none);
	/* one order per fireteam: a new one replaces the old; the other fireteam is untouched */
	ae_fireteam_set_order(&table, fireteam, _ae_order_defend, spot, -1);
	CHECK(ae_fireteam_order_kind(&table, fireteam) == _ae_order_defend);
	CHECK(ae_fireteam_order_point(&table, fireteam, got) && got[2] == 3.0f);
	ae_fireteam_set_order(&table, fireteam, _ae_order_follow, 0, -1);
	CHECK(ae_fireteam_order_kind(&table, fireteam) == _ae_order_follow);
	CHECK(!ae_fireteam_order_point(&table, fireteam, got));
	CHECK(ae_fireteam_order_kind(&table, other) == _ae_order_none);
	ae_fireteam_set_order(&table, fireteam, 99, 0, -1);
	CHECK(ae_fireteam_order_kind(&table, fireteam) == _ae_order_follow);
	/* a wipe changes nothing: the members and the order stand; so does a rebuild of the rosters (same leader) */
	ae_fireteam_set_order(&table, fireteam, _ae_order_attack, spot, 7);
	ae_fireteam_split(&table, 0, leaders, 2, bots, 8);
	fireteam = ae_fireteam_of_bot(&table, 0);
	CHECK(ae_fireteam_order_kind(&table, fireteam) == _ae_order_attack);
	CHECK(ae_fireteam_order_object(&table, fireteam) == 7);
	CHECK(ae_fireteam_order_point(&table, fireteam, got) && got[0] == 1.0f);
	CHECK(ae_fireteam_check(&table));
	/* the leader quits: its bots merge, no order is left dangling on a free slot */
	ae_fireteam_leader_left(&table, 10);
	CHECK(ae_fireteam_check(&table));
	CHECK(ae_fireteam_of_bot(&table, 0) >= 0);
}

static void follow_test(void)
{
	/* inside the leash: left alone (fighting or not); outside: pulled in */
	CHECK(!ae_fireteam_follow_wanted(0.0f));
	CHECK(!ae_fireteam_follow_wanted(AE_FIRETEAM_LEASH));
	CHECK(ae_fireteam_follow_wanted(AE_FIRETEAM_LEASH + 0.1f));
	CHECK(AE_FIRETEAM_LEASH == 10.0f);
}

static void regroup_test(void)
{
	struct ae_fireteam_table table;
	long leaders[1] = { 10 };
	short bots[10];
	int fireteam;

	ae_fireteam_clear(&table);
	brains(bots, 0, 3);
	ae_fireteam_split(&table, 0, leaders, 1, bots, 3);
	fireteam = ae_fireteam_of_bot(&table, 0);
	ae_fireteam_set_order(&table, fireteam, _ae_order_regroup, 0, -1);
	/* not yet: 6 wu is not within 6 wu; two of three arrived */
	CHECK(!ae_fireteam_regroup_note(&table, fireteam, 0, 6.0f));
	CHECK(!ae_fireteam_regroup_note(&table, fireteam, 0, 5.9f));
	CHECK(!ae_fireteam_regroup_note(&table, fireteam, 1, 2.0f));
	/* an arrived bot stays counted when it wanders off again */
	CHECK(!ae_fireteam_regroup_note(&table, fireteam, 0, 30.0f));
	CHECK(ae_fireteam_order_kind(&table, fireteam) == _ae_order_regroup);
	CHECK(ae_fireteam_regroup_note(&table, fireteam, 2, 0.5f));
	CHECK(ae_fireteam_order_kind(&table, fireteam) == _ae_order_none);
	/* a new regroup starts over; other orders are not touched by a note */
	ae_fireteam_set_order(&table, fireteam, _ae_order_regroup, 0, -1);
	CHECK(!ae_fireteam_regroup_note(&table, fireteam, 1, 1.0f));
	ae_fireteam_set_order(&table, fireteam, _ae_order_follow, 0, -1);
	CHECK(!ae_fireteam_regroup_note(&table, fireteam, 1, 1.0f));
	CHECK(ae_fireteam_order_kind(&table, fireteam) == _ae_order_follow);
}

/* a fireteam of three bots, one leader (player 10), and the order on it */
static int team_with_order(struct ae_fireteam_table *table, short kind, float const *point)
{
	long leaders[1] = { 10 };
	short bots[10];
	int fireteam;

	ae_fireteam_clear(table);
	brains(bots, 0, 3);
	ae_fireteam_split(table, 0, leaders, 1, bots, 3);
	fireteam = ae_fireteam_of_bot(table, 0);
	ae_fireteam_set_order(table, fireteam, kind, point, -1);
	return fireteam;
}

/* one simulated bot under a hold: a fight drags its goal 30 wu off, the order sets it back afterwards */
static void defend_test(void)
{
	float anchor[3] = { 5.0f, 5.0f, 0.0f };
	float goal_distance = 0.0f, position_distance = 2.0f;
	int fight;

	CHECK(ae_fireteam_order_holds(_ae_order_defend));
	CHECK(ae_fireteam_order_holds(_ae_order_watch));
	CHECK(!ae_fireteam_order_holds(_ae_order_attack));
	CHECK(AE_FIRETEAM_ANCHOR_RADIUS == 8.0f);
	/* inside the anchor with its goal there: nothing to set */
	CHECK(!ae_fireteam_press_wanted(_ae_order_defend, position_distance, goal_distance));
	for (fight = 0; fight < 3; fight++)
	{
		/* the fight: the bot ends 30 wu away, its goal (the enemy last seen) too */
		position_distance = 30.0f;
		goal_distance = 30.0f;
		CHECK(ae_fireteam_press_wanted(_ae_order_defend, position_distance, goal_distance));
		/* the fight is over: the goal goes back to the anchor, the bot walks home */
		goal_distance = 0.0f;
		CHECK(goal_distance <= AE_FIRETEAM_ANCHOR_RADIUS);
		position_distance = 3.0f;
		CHECK(!ae_fireteam_press_wanted(_ae_order_defend, position_distance, goal_distance));
	}
	/* the anchor's edge: 8 wu is held, past it is pulled in */
	CHECK(!ae_fireteam_press_wanted(_ae_order_defend, AE_FIRETEAM_ANCHOR_RADIUS, 0.0f));
	CHECK(ae_fireteam_press_wanted(_ae_order_defend, AE_FIRETEAM_ANCHOR_RADIUS + 0.1f, 0.0f));
	/* a goal that wandered off (a roam, a fetched weapon) while the bot still stands inside is set back */
	CHECK(ae_fireteam_press_wanted(_ae_order_defend, 1.0f, 20.0f));
	(void)anchor;
}

/* Review Focus 5: a flag carried past the anchor wins (the evaluator yields to it); when it is over, the goal the carrier
left behind is far from the anchor and the hold puts it back */
static void carrier_priority_test(void)
{
	/* the carrier's run: the bot is 40 wu off, its goal at the base */
	CHECK(ae_fireteam_press_wanted(_ae_order_defend, 40.0f, 60.0f));
	/* the flag is gone (captured, dropped): same distances, the hold resumes at once */
	CHECK(ae_fireteam_press_wanted(_ae_order_defend, 2.0f, 60.0f));
	/* and a bot back home with its goal home is left be */
	CHECK(!ae_fireteam_press_wanted(_ae_order_defend, 2.0f, 0.0f));
}

static void watch_test(void)
{
	/* WATCH holds like DEFEND (v1: it stands at the point; a vantage comes later) */
	int kind_index;
	short holds[2] = { _ae_order_defend, _ae_order_watch };

	for (kind_index = 0; kind_index < 2; kind_index++)
	{
		CHECK(ae_fireteam_press_wanted(holds[kind_index], 9.0f, 0.0f));
		CHECK(!ae_fireteam_press_wanted(holds[kind_index], 7.0f, 7.0f));
	}
}

static void attack_test(void)
{
	/* ATTACK is pressed again when a member drifts past 1.5 leashes of the point; inside it, the bot is left to fight */
	float drift = AE_FIRETEAM_LEASH * AE_FIRETEAM_ATTACK_DRIFT;

	CHECK(drift == 15.0f);
	CHECK(!ae_fireteam_press_wanted(_ae_order_attack, 0.0f, 100.0f));
	CHECK(!ae_fireteam_press_wanted(_ae_order_attack, drift, 100.0f));
	CHECK(ae_fireteam_press_wanted(_ae_order_attack, drift + 0.1f, 0.0f));
	CHECK(!ae_fireteam_press_wanted(_ae_order_none, 100.0f, 100.0f));
	CHECK(!ae_fireteam_press_wanted(_ae_order_follow, 100.0f, 100.0f));
}

static void go_test(void)
{
	struct ae_fireteam_table table;
	float point[3] = { 1.0f, 2.0f, 3.0f };
	int fireteam = team_with_order(&table, _ae_order_go, point);

	/* nobody there yet: no hold clock, the order stands however long it takes */
	CHECK(!ae_fireteam_go_note(&table, fireteam, 0, 40.0f, 100.0f));
	CHECK(!ae_fireteam_go_note(&table, fireteam, 1, 40.0f, 200.0f));
	/* the first member arrives at t 300: 15 s of holding from then, by any member's report */
	CHECK(!ae_fireteam_go_note(&table, fireteam, 0, 2.0f, 300.0f));
	CHECK(!ae_fireteam_go_note(&table, fireteam, 1, 40.0f, 314.9f));
	CHECK(ae_fireteam_order_kind(&table, fireteam) == _ae_order_go);
	CHECK(ae_fireteam_go_note(&table, fireteam, 1, 40.0f, 315.0f));
	CHECK(ae_fireteam_order_kind(&table, fireteam) == _ae_order_none);
	/* a new GOING starts the clock over; other orders are not touched */
	ae_fireteam_set_order(&table, fireteam, _ae_order_go, point, -1);
	CHECK(!ae_fireteam_go_note(&table, fireteam, 0, 2.0f, 400.0f));
	ae_fireteam_set_order(&table, fireteam, _ae_order_go, point, -1);
	CHECK(!ae_fireteam_go_note(&table, fireteam, 1, 40.0f, 500.0f));
	ae_fireteam_set_order(&table, fireteam, _ae_order_defend, point, -1);
	CHECK(!ae_fireteam_go_note(&table, fireteam, 0, 2.0f, 600.0f));
	CHECK(ae_fireteam_order_kind(&table, fireteam) == _ae_order_defend);
	CHECK(AE_FIRETEAM_GO_HOLD_SECONDS == 15.0f);
}

static void recruit_check_test(void)
{
	struct ae_fireteam_table table;
	long leaders[2] = { 10, 11 };
	short bots[10];
	int mine, theirs;

	ae_fireteam_clear(&table);
	brains(bots, 0, 6);
	ae_fireteam_split(&table, 0, leaders, 2, bots, 6);
	mine = ae_fireteam_of_bot(&table, 0);
	theirs = ae_fireteam_of_bot(&table, 5);
	CHECK(mine != theirs);
	/* a bot of the other fireteam, free: recruited */
	CHECK(ae_fireteam_recruit_check(&table, 5, mine, 0, 0) == AE_RECRUIT_OK);
	/* its own fireteam: nothing to do, and not a refusal to answer */
	CHECK(ae_fireteam_recruit_check(&table, 0, mine, 0, 0) == AE_RECRUIT_SAME);
	/* no such fireteam or bot */
	CHECK(ae_fireteam_recruit_check(&table, 5, 40, 0, 0) == AE_RECRUIT_NONE);
	CHECK(ae_fireteam_recruit_check(&table, 99, mine, 0, 0) == AE_RECRUIT_NONE);
	/* a carrier and a seated driver stay */
	CHECK(ae_fireteam_recruit_check(&table, 5, mine, 1, 0) == AE_RECRUIT_OBJECTIVE);
	CHECK(ae_fireteam_recruit_check(&table, 5, mine, 0, 1) == AE_RECRUIT_SEAT);
	CHECK(ae_fireteam_recruit_check(&table, 5, mine, 1, 1) == AE_RECRUIT_OBJECTIVE);
	/* the last member of a fireteam stays */
	while (table.fireteams[theirs].members > 1)
	{
		short b;

		for (b = 0; b < 6 && ae_fireteam_of_bot(&table, b) != theirs; b++)
			;
		CHECK(ae_fireteam_recruit(&table, b, mine));
	}
	{
		short last;

		for (last = 0; last < 6 && ae_fireteam_of_bot(&table, last) != theirs; last++)
			;
		CHECK(ae_fireteam_recruit_check(&table, last, mine, 0, 0) == AE_RECRUIT_LAST);
		CHECK(ae_fireteam_check(&table));
	}
}

static void smart_test(void)
{
	/* a bot of my own fireteam: FOLLOW; a bot of another: recruit; neither, on the ground */
	CHECK(ae_fireteam_smart_resolve(1, 1, 0, 99.0f) == AE_SMART_FOLLOW);
	CHECK(ae_fireteam_smart_resolve(1, 0, 0, 99.0f) == AE_SMART_RECRUIT);
	/* the ground within 5 wu of me: FOLLOW; at 5 and past it: the plain mark */
	CHECK(ae_fireteam_smart_resolve(0, 0, 1, 4.9f) == AE_SMART_FOLLOW);
	CHECK(ae_fireteam_smart_resolve(0, 0, 1, AE_FIRETEAM_SMART_GROUND_RADIUS) == AE_SMART_FOLLOW);
	CHECK(ae_fireteam_smart_resolve(0, 0, 1, 5.1f) == AE_SMART_PLAIN);
	/* an item, a vehicle or an enemy near me is not ground */
	CHECK(ae_fireteam_smart_resolve(0, 0, 0, 1.0f) == AE_SMART_PLAIN);
	CHECK(AE_FIRETEAM_SMART_GROUND_RADIUS == 5.0f);
}

/* wire compatibility: a 0.7.5 machine's call is the same bytes; only the host reads them differently */
static void old_client_test(void)
{
	int entry;

	/* the wire's entry 3 is REGROUP (a 0.7.5 build's NEED WEAPON): a non-leader's (or any call with fireteams off, which
	leaves nobody leading: fireteam -1) sets nothing and asks nothing of the team's bots, a plain chat line */
	CHECK(AE_WHEEL_REGROUP == 3);
	CHECK(ae_fireteam_call_order(-1, AE_WHEEL_REGROUP) == _ae_order_none);
	CHECK(!ae_fireteam_call_shares(AE_WHEEL_REGROUP));
	/* a leader's sets the regroup order */
	CHECK(ae_fireteam_call_order(0, AE_WHEEL_REGROUP) == _ae_order_regroup);
	CHECK(ae_fireteam_call_order(7, AE_WHEEL_REGROUP) == _ae_order_regroup);
	/* fireteams off or not a leader: no entry gives an order and no SMART CALL is tried; every call but REGROUP shares as
	the marks always did */
	for (entry = 0; entry < AE_WHEEL_COUNT; entry++)
	{
		CHECK(ae_fireteam_call_order(-1, entry) == _ae_order_none);
		CHECK(!ae_fireteam_call_smart(-1, entry, 1));
		CHECK(ae_fireteam_call_shares(entry) == (entry != AE_WHEEL_REGROUP));
		/* (a value the wire could carry past the table is no order either) */
	}
	CHECK(ae_fireteam_call_order(0, AE_WHEEL_COUNT) == _ae_order_none);
	CHECK(ae_fireteam_call_order(0, 255) == _ae_order_none);
	CHECK(ae_fireteam_call_order(0, AE_WHEEL_SMART) == _ae_order_none);
	CHECK(ae_fireteam_call_order(0, AE_WHEEL_ENEMY) == _ae_order_none);
	CHECK(ae_fireteam_call_order(0, AE_WHEEL_RIDE) == _ae_order_none);
	CHECK(ae_fireteam_call_order(0, AE_WHEEL_ATTACK) == _ae_order_attack);
	CHECK(ae_fireteam_call_order(0, AE_WHEEL_DEFEND) == _ae_order_defend);
	CHECK(ae_fireteam_call_order(0, AE_WHEEL_WATCH) == _ae_order_watch);
	CHECK(ae_fireteam_call_order(0, AE_WHEEL_GOING) == _ae_order_go);
}

/* a SMART place mark from a machine that never learned fireteams (a plain place mark on the wire) is resolved on the host
alone: the same answer whoever sent it, from the aim and the caller's feet, nothing the sender had to know */
static void smart_old_client_test(void)
{
	CHECK(ae_fireteam_call_smart(0, AE_WHEEL_SMART, 1));
	CHECK(!ae_fireteam_call_smart(0, AE_WHEEL_SMART, 0));	/* (an item or an enemy under the aim: the plain mark) */
	CHECK(!ae_fireteam_call_smart(0, AE_WHEEL_ATTACK, 1));
	CHECK(ae_fireteam_smart_resolve(1, 1, 0, 40.0f) == AE_SMART_FOLLOW);
	CHECK(ae_fireteam_smart_resolve(1, 0, 0, 40.0f) == AE_SMART_RECRUIT);
	CHECK(ae_fireteam_smart_resolve(0, 0, 1, 0.0f) == AE_SMART_FOLLOW);
	CHECK(ae_fireteam_smart_resolve(0, 0, 1, 30.0f) == AE_SMART_PLAIN);
	/* (SMART's HERE: the plain mark of a far place sends the squad GOING; the near ground and a bot of its own FOLLOW) */
	CHECK(ae_fireteam_smart_order(ae_fireteam_smart_resolve(0, 0, 1, 30.0f)) == _ae_order_go);
	CHECK(ae_fireteam_smart_order(ae_fireteam_smart_resolve(0, 0, 1, 2.0f)) == _ae_order_follow);
	CHECK(ae_fireteam_smart_order(ae_fireteam_smart_resolve(1, 1, 0, 40.0f)) == _ae_order_follow);
	CHECK(ae_fireteam_smart_order(ae_fireteam_smart_resolve(1, 0, 0, 40.0f)) == _ae_order_none);
}

/* squad roles */
static void role_table_test(void)
{
	short roles[AE_ROLE_TABLE_MAX];

	CHECK(ae_fireteam_role_table(AE_ROLE_MODE_NONE, roles) == 0);
	CHECK(ae_fireteam_role_table(AE_ROLE_MODE_COUNT, roles) == 0);
	CHECK(ae_fireteam_role_table(AE_ROLE_MODE_CTF, roles) == 3);
	CHECK(roles[0] == AE_ROLE_DEFEND_OURS && roles[1] == AE_ROLE_PUSH_THEIRS && roles[2] == AE_ROLE_ROVE);
	CHECK(ae_fireteam_role_table(AE_ROLE_MODE_SND_DEFENDERS, roles) == 3 && roles[0] == AE_ROLE_SITE_A && roles[1] == AE_ROLE_SITE_B);
	CHECK(ae_fireteam_role_table(AE_ROLE_MODE_SND_ATTACKERS, roles) == 3 && roles[0] == AE_ROLE_ENTRY);
	CHECK(ae_fireteam_role_table(AE_ROLE_MODE_KING, roles) == 2 && roles[0] == AE_ROLE_HILL && roles[1] == AE_ROLE_APPROACH);
	CHECK(ae_fireteam_role_table(AE_ROLE_MODE_ODDBALL, roles) == 2 && roles[0] == AE_ROLE_PROTECT && roles[1] == AE_ROLE_HUNT);
	CHECK(ae_fireteam_role_table(AE_ROLE_MODE_SLAYER, roles) == 2 && roles[0] == AE_ROLE_POWER);
	/* defend-ours holds, push-theirs presses; rove, power items and roam are the bots' own goals */
	CHECK(ae_fireteam_role_order_kind(AE_ROLE_DEFEND_OURS) == _ae_order_defend);
	CHECK(ae_fireteam_role_order_kind(AE_ROLE_PUSH_THEIRS) == _ae_order_attack);
	CHECK(ae_fireteam_role_order_kind(AE_ROLE_HILL) == _ae_order_defend);
	CHECK(ae_fireteam_role_order_kind(AE_ROLE_APPROACH) == _ae_order_attack);
	CHECK(ae_fireteam_role_order_kind(AE_ROLE_SITE_B) == _ae_order_watch);
	CHECK(ae_fireteam_role_order_kind(AE_ROLE_ROVE) == _ae_order_none);
	CHECK(ae_fireteam_role_order_kind(AE_ROLE_POWER) == _ae_order_none);
	CHECK(ae_fireteam_role_order_kind(AE_ROLE_ROAM) == _ae_order_none);
}

/* two leaders and bots over two teams: fireteams 0,1 are team 0's, 2,3 team 1's */
static void roles_table(struct ae_fireteam_table *table)
{
	long leaders[2] = { 10, 11 };
	short bots[24];

	ae_fireteam_clear(table);
	brains(bots, 0, 8);
	ae_fireteam_split(table, 0, leaders, 2, bots, 8);
	brains(bots, 8, 8);
	leaders[0] = 20;
	ae_fireteam_split(table, 1, leaders, 1, bots, 8);
}

/* the same two teams with bot-led squads only (roles go to bot-led squads) */
static void roles_table_bots(struct ae_fireteam_table *table)
{
	short bots[24];

	ae_fireteam_clear(table);
	brains(bots, 0, 8);
	ae_fireteam_split(table, 0, NULL, 0, bots, 8);
	brains(bots, 8, 8);
	ae_fireteam_split(table, 1, NULL, 0, bots, 8);
}

static void role_assign_test(void)
{
	struct ae_fireteam_table table;
	float spot[3] = { 1.0f, 2.0f, 3.0f };
	int f;

	roles_table_bots(&table);
	/* round-robin over each team's fireteams in table order, each team on its own */
	CHECK(ae_fireteam_role_of(&table, 0, AE_ROLE_MODE_CTF) == AE_ROLE_DEFEND_OURS);
	CHECK(ae_fireteam_role_of(&table, 1, AE_ROLE_MODE_CTF) == AE_ROLE_PUSH_THEIRS);
	CHECK(ae_fireteam_role_of(&table, 2, AE_ROLE_MODE_CTF) == AE_ROLE_DEFEND_OURS);
	CHECK(ae_fireteam_role_of(&table, 5, AE_ROLE_MODE_CTF) == AE_ROLE_NONE);	/* (a free slot) */
	CHECK(ae_fireteam_role_of(&table, 0, AE_ROLE_MODE_NONE) == AE_ROLE_NONE);
	/* a fourth squad of a team starts the table over */
	for (f = 0; f < 4; f++)
		CHECK(ae_fireteam_role_of(&table, f, AE_ROLE_MODE_KING) == (f == 0 || f == 2 ? AE_ROLE_HILL : AE_ROLE_APPROACH));
	/* the role's order, not explicit, with its point */
	CHECK(ae_fireteam_role_wants_order(&table, 0, 1, 0));	/* (no order yet) */
	ae_fireteam_set_role_order(&table, 0, AE_ROLE_DEFEND_OURS, spot);
	CHECK(ae_fireteam_order_kind(&table, 0) == _ae_order_defend && !ae_fireteam_order_is_explicit(&table, 0));
	CHECK(ae_fireteam_role(&table, 0) == AE_ROLE_DEFEND_OURS);
	{
		float got[3];

		CHECK(ae_fireteam_order_point(&table, 0, got) && got[0] == 1.0f && got[2] == 3.0f);
	}
	/* a role of no order, or no point, leaves the squad without one */
	ae_fireteam_set_role_order(&table, 0, AE_ROLE_ROVE, spot);
	CHECK(ae_fireteam_order_kind(&table, 0) == _ae_order_none && ae_fireteam_role(&table, 0) == AE_ROLE_ROVE);
	ae_fireteam_set_role_order(&table, 0, AE_ROLE_HILL, NULL);
	CHECK(ae_fireteam_order_kind(&table, 0) == _ae_order_none);
	CHECK(ae_fireteam_check(&table));
}

/* the mix of a team's bot squads over every wipe sequence: fixed, with an attacking role from two squads on */
static int attacking_role(int role)
{
	return role == AE_ROLE_PUSH_THEIRS || role == AE_ROLE_ENTRY || role == AE_ROLE_APPROACH || role == AE_ROLE_HUNT;
}

static void role_balance_test(void)
{
	static int const modes[] = { AE_ROLE_MODE_CTF, AE_ROLE_MODE_SND_ATTACKERS, AE_ROLE_MODE_KING, AE_ROLE_MODE_ODDBALL,
		AE_ROLE_MODE_SLAYER, AE_ROLE_MODE_SND_DEFENDERS };
	short roles[AE_FIRETEAM_MAX];
	int m, want, wipe;

	/* CTF with five squads: 2 DEFEND, 2 PUSH, 1 ROVE (the owner's example) */
	CHECK(ae_fireteam_role_mix(AE_ROLE_MODE_CTF, 5, roles) == 5);
	{
		int d = 0, p = 0, r = 0, i;

		for (i = 0; i < 5; i++)
		{
			d += roles[i] == AE_ROLE_DEFEND_OURS;
			p += roles[i] == AE_ROLE_PUSH_THEIRS;
			r += roles[i] == AE_ROLE_ROVE;
		}
		CHECK(d == 2 && p == 2 && r == 1);
	}
	CHECK(ae_fireteam_role_mix(AE_ROLE_MODE_NONE, 5, roles) == 0 && ae_fireteam_role_mix(AE_ROLE_MODE_CTF, 0, roles) == 0);
	for (m = 0; m < 6; m++)
	{
		for (want = 2; want <= 8; want++)
		{
			int squads;
			struct ae_fireteam_table table;
			short bots[64];
			int count[AE_ROLE_COUNT] = { 0 }, role, i, attack, expect_attack;
			short mix[AE_FIRETEAM_MAX];

			ae_fireteam_clear(&table);
			brains(bots, 0, want * 4);
			ae_fireteam_split(&table, 0, NULL, 0, bots, (short)(want * 4));
			squads = ae_fireteam_bot_squads(&table, 0);
			CHECK(squads >= 2);
			ae_fireteam_role_mix(modes[m], squads, mix);
			expect_attack = 0;
			for (i = 0; i < squads; i++)
				expect_attack |= attacking_role(mix[i]);
			/* the unwiped mix, then every wipe in turn, twice round, and some in a row */
			for (wipe = -1; wipe < squads * 3; wipe++)
			{
				if (wipe >= 0)
					ae_fireteam_role_wiped(&table, (wipe * 7 + wipe / 2) % squads);
				memset(count, 0, sizeof(count));
				attack = 0;
				for (i = 0; i < squads; i++)
				{
					role = ae_fireteam_role_of(&table, i, modes[m]);
					CHECK(role != AE_ROLE_NONE && role < AE_ROLE_COUNT);
					count[role]++;
					attack |= attacking_role(role);
				}
				/* the same roles as the mix, only who holds which has moved */
				for (i = 0; i < squads; i++)
					count[mix[i]]--;
				for (i = 0; i < AE_ROLE_COUNT; i++)
					CHECK(count[i] == 0);
				CHECK(attack == expect_attack);
			}
		}
	}
	/* human-led squads are not counted: two leaders and 8 bots, the bot squads' mix is for the bot squads alone */
	{
		struct ae_fireteam_table table;
		long leaders[2] = { 10, 11 };
		short bots[24];
		int i, bot_led = 0, push = 0;

		ae_fireteam_clear(&table);
		brains(bots, 0, 14);
		ae_fireteam_split(&table, 0, leaders, 2, bots, 14);
		for (i = 0; i < table.count; i++)
		{
			if (table.fireteams[i].leader == -1)
			{
				bot_led++;
				push += ae_fireteam_role_of(&table, i, AE_ROLE_MODE_CTF) == AE_ROLE_PUSH_THEIRS;
			}
			else
				CHECK(ae_fireteam_role_of(&table, i, AE_ROLE_MODE_CTF) == AE_ROLE_NONE);
		}
		CHECK(bot_led == ae_fireteam_bot_squads(&table, 0));
		CHECK(bot_led < 2 || push >= 1);
	}
	/* a squad that went (a team rebuilt) starts the places over: still a whole mix */
	{
		struct ae_fireteam_table table;
		short bots[40];
		int i, push = 0;

		ae_fireteam_clear(&table);
		brains(bots, 0, 20);
		ae_fireteam_split(&table, 0, NULL, 0, bots, 20);
		ae_fireteam_role_wiped(&table, 1);
		ae_fireteam_role_wiped(&table, 2);
		ae_fireteam_split(&table, 0, NULL, 0, bots, 12);
		for (i = 0; i < table.count; i++)
			if (table.fireteams[i].team == 0)
				push += ae_fireteam_role_of(&table, i, AE_ROLE_MODE_CTF) == AE_ROLE_PUSH_THEIRS;
		CHECK(push >= 1);
	}
}

static void formation_test(void)
{
	float dx, dy, bx, by, held;
	int index, other;

	/* facing +x (east, left is +y): member 0 is behind and right, member 1 behind and left, a rank further each pair */
	ae_fireteam_formation_slot(0, 0.0f, &dx, &dy);
	CHECK(dx > -1.51f && dx < -1.49f && dy < -1.99f && dy > -2.01f);
	ae_fireteam_formation_slot(1, 0.0f, &bx, &by);
	CHECK(bx == dx && by == -dy);
	ae_fireteam_formation_slot(2, 0.0f, &bx, &by);
	CHECK(by < -3.99f && by > -4.01f && bx < -2.99f && bx > -3.01f);
	/* turned with the leader: facing north (+y), member 1 (behind and left) is behind (south) and west */
	ae_fireteam_formation_slot(1, 1.5707963f, &bx, &by);
	CHECK(by < -1.49f && by > -1.51f && bx < -1.99f && bx > -2.01f);
	/* no slot of a squad of five is out of the follow radius, or on another's */
	for (index = 0; index < 5; index++)
	{
		ae_fireteam_formation_slot(index, 0.7f, &dx, &dy);
		CHECK(dx * dx + dy * dy < (AE_FIRETEAM_FOLLOW_RADIUS - 1.0f) * (AE_FIRETEAM_FOLLOW_RADIUS - 1.0f));
		for (other = index + 1; other < 5; other++)
		{
			ae_fireteam_formation_slot(other, 0.7f, &bx, &by);
			CHECK((dx - bx) * (dx - bx) + (dy - by) * (dy - by) > 2.4f * 2.4f);
		}
	}
	/* the facing slots are read at: held until the leader turns past the re-slot turn, either way round the circle */
	held = ae_fireteam_formation_facing(0.0f, 0.3f, 0);
	CHECK(held == 0.0f);
	CHECK(ae_fireteam_formation_facing(0.0f, 0.5f, 0) == 0.5f);
	CHECK(ae_fireteam_formation_facing(3.1f, -3.1f, 0) == 3.1f);	/* (0.08 rad apart across the seam) */
	CHECK(ae_fireteam_formation_facing(0.0f, 1.0f, 1) == 1.0f);
}

static void settle_test(void)
{
	/* defend and watch stand at their anchor: near the point, or in the anchor with a crowd about; nothing else settles */
	CHECK(ae_fireteam_settled(_ae_order_defend, 3.0f, 0) && ae_fireteam_settled(_ae_order_watch, 0.5f, 0));
	CHECK(!ae_fireteam_settled(_ae_order_defend, 5.0f, 1));
	CHECK(ae_fireteam_settled(_ae_order_defend, 5.0f, AE_FIRETEAM_SETTLE_CROWD));
	CHECK(!ae_fireteam_settled(_ae_order_defend, AE_FIRETEAM_ANCHOR_RADIUS + 0.5f, 4));
	CHECK(!ae_fireteam_settled(_ae_order_attack, 1.0f, 4) && !ae_fireteam_settled(_ae_order_go, 1.0f, 4) &&
		!ae_fireteam_settled(_ae_order_follow, 1.0f, 4) && !ae_fireteam_settled(_ae_order_none, 1.0f, 4));
	CHECK(AE_FIRETEAM_SETTLE_RADIUS < AE_FIRETEAM_ANCHOR_RADIUS);
}

static void tether_test(void)
{
	float tol = AE_FIRETEAM_FOLLOW_TOLERANCE;

	/* past the radius: walk back, fighting or not (a fight never keeps it out) */
	CHECK(ae_fireteam_tether(AE_FIRETEAM_FOLLOW_RADIUS + 0.1f, 12.0f, 1, 1.0f, tol) == AE_TETHER_GO);
	CHECK(ae_fireteam_tether(AE_FIRETEAM_FOLLOW_RADIUS + 0.1f, 12.0f, 0, 2.0f, tol) == AE_TETHER_GO);
	/* fighting inside: free, until near the edge and walking out, which is stopped; walking in or across is free */
	CHECK(ae_fireteam_tether(3.0f, 3.0f, 1, 1.0f, tol) == AE_TETHER_FREE);
	CHECK(ae_fireteam_tether(AE_FIRETEAM_FOLLOW_RADIUS - 1.0f, 2.0f, 1, 0.9f, tol) == AE_TETHER_STOP);
	CHECK(ae_fireteam_tether(AE_FIRETEAM_FOLLOW_RADIUS - 1.0f, 2.0f, 1, -0.9f, tol) == AE_TETHER_FREE);
	CHECK(ae_fireteam_tether(AE_FIRETEAM_FOLLOW_RADIUS - 1.0f, 2.0f, 1, 0.0f, tol) == AE_TETHER_FREE);
	CHECK(ae_fireteam_tether(AE_FIRETEAM_FOLLOW_RADIUS - 1.0f, 2.0f, 1, 2.0f, tol) == AE_TETHER_FREE);	/* (standing) */
	/* not fighting: to the slot when off it by more than the order's tolerance, else stand in it; REGROUP is tighter */
	CHECK(ae_fireteam_tether(5.0f, tol + 0.1f, 0, 2.0f, tol) == AE_TETHER_GO);
	CHECK(ae_fireteam_tether(5.0f, tol - 0.1f, 0, 2.0f, tol) == AE_TETHER_HOLD);
	CHECK(ae_fireteam_tether(5.0f, 2.0f, 0, 2.0f, AE_FIRETEAM_REGROUP_TOLERANCE) == AE_TETHER_GO);
	CHECK(AE_FIRETEAM_REGROUP_TOLERANCE < AE_FIRETEAM_FOLLOW_TOLERANCE && AE_FIRETEAM_FOLLOW_TOLERANCE < AE_FIRETEAM_FOLLOW_RADIUS);
}

static void role_rotation_test(void)
{
	struct ae_fireteam_table table;
	float spot[3] = { 4.0f, 5.0f, 6.0f };

	roles_table_bots(&table);
	/* a wipe swaps the squad's place with the next bot squad of its team: the team's mix stays, the other team is untouched */
	ae_fireteam_role_wiped(&table, 0);
	CHECK(ae_fireteam_role_of(&table, 0, AE_ROLE_MODE_CTF) == AE_ROLE_PUSH_THEIRS);
	CHECK(ae_fireteam_role_of(&table, 1, AE_ROLE_MODE_CTF) == AE_ROLE_DEFEND_OURS);
	CHECK(ae_fireteam_role_of(&table, 2, AE_ROLE_MODE_CTF) == AE_ROLE_DEFEND_OURS);
	ae_fireteam_role_wiped(&table, 0);
	CHECK(ae_fireteam_role_of(&table, 0, AE_ROLE_MODE_CTF) == AE_ROLE_DEFEND_OURS);	/* (swapped back) */
	CHECK(ae_fireteam_role_of(&table, 1, AE_ROLE_MODE_CTF) == AE_ROLE_PUSH_THEIRS);
	ae_fireteam_role_wiped(&table, 9);	/* (no such squad) */
	/* a flip gives the same role its new point: the role's own order is re-issued, the role stays */
	ae_fireteam_set_role_order(&table, 1, AE_ROLE_PUSH_THEIRS, spot);
	CHECK(ae_fireteam_role_wants_order(&table, 1, 1, 1));
	spot[0] = 40.0f;
	ae_fireteam_set_role_order(&table, 1, AE_ROLE_PUSH_THEIRS, spot);
	{
		float got[3];

		CHECK(ae_fireteam_order_point(&table, 1, got) && got[0] == 40.0f);
	}
	CHECK(ae_fireteam_role_of(&table, 1, AE_ROLE_MODE_CTF) == AE_ROLE_PUSH_THEIRS);
	/* a spent order (a regroup that finished) is taken over by the role again */
	ae_fireteam_set_order(&table, 1, _ae_order_regroup, NULL, -1);
	CHECK(!ae_fireteam_role_wants_order(&table, 1, 1, 0));
	{
		int brain;

		for (brain = 0; brain < AE_FIRETEAM_BRAINS; brain++)
			if (ae_fireteam_of_bot(&table, (short)brain) == 1)
				ae_fireteam_regroup_note(&table, 1, (short)brain, 0.0f);
	}
	CHECK(ae_fireteam_order_kind(&table, 1) == _ae_order_none && ae_fireteam_role_wants_order(&table, 1, 1, 0));
}

static void role_override_test(void)
{
	struct ae_fireteam_table table;
	float role_spot[3] = { 0.0f, 0.0f, 0.0f }, call_spot[3] = { 9.0f, 9.0f, 9.0f };

	roles_table(&table);
	CHECK(ae_fireteam_leader(&table, 0) == 10);
	/* a human-led squad has no role at all, and no role pass ever takes it */
	CHECK(ae_fireteam_role_of(&table, 0, AE_ROLE_MODE_CTF) == AE_ROLE_NONE);
	CHECK(ae_fireteam_role_of(&table, 1, AE_ROLE_MODE_CTF) == AE_ROLE_NONE);
	CHECK(!ae_fireteam_role_wants_order(&table, 0, 1, 0));
	CHECK(!ae_fireteam_role_wants_order(&table, 0, 1, 1));
	/* a human leader's own call holds against everything: events, wipes, a dead leader at a flip */
	ae_fireteam_set_order(&table, 0, _ae_order_follow, NULL, -1);
	CHECK(!ae_fireteam_role_wants_order(&table, 0, 1, 1));
	CHECK(!ae_fireteam_role_wants_order(&table, 0, 0, 1));
	CHECK(ae_fireteam_order_kind(&table, 0) == _ae_order_follow);
	ae_fireteam_set_order(&table, 0, _ae_order_attack, call_spot, -1);
	CHECK(ae_fireteam_order_is_explicit(&table, 0));
	CHECK(!ae_fireteam_role_wants_order(&table, 0, 1, 1));
	CHECK(!ae_fireteam_role_wants_order(&table, 0, 0, 1));
	CHECK(ae_fireteam_order_kind(&table, 0) == _ae_order_attack);
	/* bot-led squads still run their role, and a leader gone to a bot lets the role in */
	roles_table_bots(&table);
	CHECK(ae_fireteam_leader(&table, 0) == -1);
	ae_fireteam_set_role_order(&table, 0, AE_ROLE_DEFEND_OURS, role_spot);
	CHECK(ae_fireteam_role_of(&table, 0, AE_ROLE_MODE_CTF) == AE_ROLE_DEFEND_OURS);
	CHECK(ae_fireteam_role_wants_order(&table, 0, 1, 0));
	ae_fireteam_set_order(&table, 0, _ae_order_attack, call_spot, -1);
	CHECK(!ae_fireteam_role_wants_order(&table, 0, 1, 1));	/* (an explicit order of a bot-led squad holds) */
	ae_fireteam_set_order(&table, 0, _ae_order_none, NULL, -1);
	CHECK(!ae_fireteam_order_is_explicit(&table, 0) && ae_fireteam_role_wants_order(&table, 0, 1, 0));
	/* the override survives a rebuild (found again by its leader) */
	{
		long leaders[2] = { 10, 11 };
		short bots[8];

		brains(bots, 0, 8);
		ae_fireteam_clear(&table);
		ae_fireteam_split(&table, 0, leaders, 2, bots, 8);
		ae_fireteam_set_order(&table, 0, _ae_order_defend, call_spot, -1);
		ae_fireteam_split(&table, 0, leaders, 2, bots, 8);
		CHECK(ae_fireteam_order_is_explicit(&table, ae_fireteam_of_bot(&table, 0)));
		CHECK(!ae_fireteam_role_wants_order(&table, ae_fireteam_of_bot(&table, 0), 1, 1));
	}
}

static void stall_test(void)
{
	/* REGROUP and GOING hold on distance: only a squad with no member able to arrive lets go, after 60 s */
	CHECK(!ae_fireteam_gave_up(_ae_order_regroup, 1, 20.0f));
	CHECK(!ae_fireteam_gave_up(_ae_order_regroup, 3, 600.0f));	/* (a member alive and able, however long it takes) */
	CHECK(!ae_fireteam_gave_up(_ae_order_regroup, 0, 59.0f));
	CHECK(ae_fireteam_gave_up(_ae_order_regroup, 0, 60.0f));
	CHECK(!ae_fireteam_gave_up(_ae_order_go, 2, 120.0f));
	CHECK(ae_fireteam_gave_up(_ae_order_go, 0, 61.0f));
	CHECK(!ae_fireteam_gave_up(_ae_order_follow, 0, 600.0f));
	CHECK(!ae_fireteam_gave_up(_ae_order_defend, 0, 600.0f));
}

static void role_rebuild_test(void)
{
	struct ae_fireteam_table table;
	float spot[3] = { 3.0f, 3.0f, 3.0f };
	short bots[12];
	long generation;
	int f;

	/* a team of bots only: bot-led squads of 3 to 5 */
	ae_fireteam_clear(&table);
	brains(bots, 0, 11);
	ae_fireteam_split(&table, 0, NULL, 0, bots, 11);
	generation = table.generation;
	CHECK(generation > 0);
	for (f = 0; f < table.count; f++)
		if (table.fireteams[f].team == 0)
			ae_fireteam_set_role_order(&table, f, ae_fireteam_role_of(&table, f, AE_ROLE_MODE_CTF), spot);
	CHECK(ae_fireteam_order_kind(&table, 0) == _ae_order_defend);
	/* a bot joins: the table is rebuilt, the bot-led squads lose their orders and get a new generation: roles are handed out again */
	brains(bots, 0, 12);
	ae_fireteam_split(&table, 0, NULL, 0, bots, 12);
	CHECK(table.generation > generation);
	for (f = 0; f < table.count; f++)
		if (table.fireteams[f].team == 0)
		{
			CHECK(ae_fireteam_order_kind(&table, f) == _ae_order_none);
			CHECK(ae_fireteam_role_wants_order(&table, f, 1, 0));
			ae_fireteam_set_role_order(&table, f, ae_fireteam_role_of(&table, f, AE_ROLE_MODE_CTF), spot);
		}
	CHECK(ae_fireteam_check(&table));
	/* a fresh table starts the count over */
	ae_fireteam_clear(&table);
	CHECK(table.generation == 0);
}

static void order_serial_test(void)
{
	struct ae_fireteam_table table;
	float spot[3] = { 1.0f, 2.0f, 3.0f };
	long leaders[1] = { 10 };
	short bots[4];
	long first;

	ae_fireteam_clear(&table);
	brains(bots, 0, 4);
	ae_fireteam_split(&table, 0, leaders, 1, bots, 4);
	CHECK(ae_fireteam_order_serial(&table, 0) == 0);
	ae_fireteam_set_order(&table, 0, _ae_order_regroup, NULL, -1);
	first = ae_fireteam_order_serial(&table, 0);
	CHECK(first > 0);
	/* the same kind again is a new order (the host's clock starts over) */
	ae_fireteam_set_order(&table, 0, _ae_order_regroup, NULL, -1);
	CHECK(ae_fireteam_order_serial(&table, 0) > first);
	/* a leader's order kept through a rebuild keeps its serial */
	first = ae_fireteam_order_serial(&table, 0);
	ae_fireteam_set_order(&table, 0, _ae_order_go, spot, -1);
	first = ae_fireteam_order_serial(&table, 0);
	ae_fireteam_split(&table, 0, leaders, 1, bots, 4);
	CHECK(ae_fireteam_order_kind(&table, 0) == _ae_order_go);
	CHECK(ae_fireteam_order_serial(&table, 0) == first);
	/* a cleared order has none; a free fireteam has none */
	ae_fireteam_set_order(&table, 0, _ae_order_none, NULL, -1);
	CHECK(ae_fireteam_order_kind(&table, 0) == _ae_order_none);
	CHECK(ae_fireteam_order_serial(&table, 40) == 0);
}

/* BIG TEAM OR ARENA: the rule, and a person's squad playing a role (not following) under the arena rules only */
static void arena_rules_test(void)
{
	struct ae_fireteam_table table;
	long leaders[1] = { 10 };
	short bots[3] = { 0, 1, 2 };
	char const *reason = NULL;

	CHECK(ae_fireteam_rules_are_arena(1, 1, 4, &reason) == 0);	/* AE BTB: big team whatever the count */
	CHECK(reason && strstr(reason, "BTB"));
	CHECK(ae_fireteam_rules_are_arena(1, 0, 12, &reason) == 1);	/* any other AE gametype: arena */
	CHECK(ae_fireteam_rules_are_arena(0, 0, 4, &reason) == 1);	/* not AE's: by team size */
	CHECK(ae_fireteam_rules_are_arena(0, 0, 5, &reason) == 0);
	CHECK(ae_fireteam_rules_are_arena(1, 0, 4, NULL) == 1);
	/* 4v4: the leader and 3 bots in one squad */
	ae_fireteam_clear(&table);
	ae_fireteam_split(&table, 0, leaders, 1, bots, 3);
	CHECK(table.arena_rules == 0);
	CHECK(ae_fireteam_role_of(&table, 0, AE_ROLE_MODE_CTF) == AE_ROLE_NONE);	/* big team: no role */
	CHECK(!ae_fireteam_role_wants_order(&table, 0, 1, 0));
	ae_fireteam_set_rules(&table, 1);
	CHECK(ae_fireteam_role_of(&table, 0, AE_ROLE_MODE_CTF) != AE_ROLE_NONE);	/* arena: its role */
	CHECK(ae_fireteam_bot_squads(&table, 0) == 1);
	CHECK(ae_fireteam_role_wants_order(&table, 0, 1, 0));
	/* a FOLLOW ME holds against the role; the role's own order does not */
	ae_fireteam_set_order(&table, 0, _ae_order_follow, NULL, -1);
	CHECK(ae_fireteam_order_is_explicit(&table, 0));
	CHECK(!ae_fireteam_role_wants_order(&table, 0, 1, 0));
	ae_fireteam_clear(&table);
	ae_fireteam_split(&table, 0, leaders, 1, bots, 3);
	ae_fireteam_set_rules(&table, 1);
	{
		float spot[3] = { 1.0f, 2.0f, 3.0f };

		ae_fireteam_set_role_order(&table, 0, AE_ROLE_DEFEND_OURS, spot);
		CHECK(ae_fireteam_order_kind(&table, 0) == _ae_order_defend);
		CHECK(ae_fireteam_role_wants_order(&table, 0, 1, 0));
	}
}

/* ARENA ORDERS ARE TEMPORARY: the release rule */
static void arena_release_test(void)
{
	struct ae_fireteam_table table;
	long leaders[1] = { 10 };
	short bots[3] = { 0, 1, 2 };
	short brain;

	/* an order that is not the leader's own, or none, is never released by it */
	CHECK(ae_fireteam_arena_release(_ae_order_none, 1, 99.0f, 99.0f, 1, 0, 1) == AE_RELEASE_NONE);
	CHECK(ae_fireteam_arena_release(_ae_order_defend, 0, 99.0f, 99.0f, 1, 0, 1) == AE_RELEASE_NONE);
	/* the leader dead beats everything; then an objective event; then fulfilled and calm; then the cap */
	CHECK(ae_fireteam_arena_release(_ae_order_follow, 1, 1.0f, 0.0f, 0, 0, 1) == AE_RELEASE_LEADER_DEAD);
	CHECK(ae_fireteam_arena_release(_ae_order_follow, 1, 1.0f, 0.0f, 0, 1, 1) == AE_RELEASE_OBJECTIVE);
	CHECK(ae_fireteam_arena_release(_ae_order_regroup, 1, 4.0f, 5.0f, 1, 1, 0) == AE_RELEASE_FULFILLED);
	CHECK(ae_fireteam_arena_release(_ae_order_regroup, 1, 4.0f, 4.9f, 1, 1, 0) == AE_RELEASE_NONE);	/* still in a fight */
	CHECK(ae_fireteam_arena_release(_ae_order_regroup, 1, 4.0f, 60.0f, 0, 1, 0) == AE_RELEASE_NONE);	/* not there yet */
	CHECK(ae_fireteam_arena_release(_ae_order_regroup, 1, 29.9f, 0.0f, 0, 1, 0) == AE_RELEASE_NONE);
	CHECK(ae_fireteam_arena_release(_ae_order_regroup, 1, 30.0f, 0.0f, 0, 1, 0) == AE_RELEASE_CAP);
	CHECK(ae_fireteam_arena_release(_ae_order_follow, 1, 44.9f, 60.0f, 0, 1, 0) == AE_RELEASE_NONE);
	CHECK(ae_fireteam_arena_release(_ae_order_follow, 1, 45.0f, 0.0f, 0, 1, 0) == AE_RELEASE_CAP);
	CHECK(strcmp(ae_fireteam_release_name(AE_RELEASE_OBJECTIVE), "objective") == 0);
	/* arena REGROUP: arrival marks, never clears; complete when every member is in; release ends it */
	ae_fireteam_clear(&table);
	ae_fireteam_split(&table, 0, leaders, 1, bots, 3);
	ae_fireteam_set_rules(&table, 1);
	ae_fireteam_set_order(&table, 0, _ae_order_regroup, NULL, -1);
	for (brain = 0; brain < 3; brain++)
	{
		CHECK(!ae_fireteam_regroup_complete(&table, 0));
		CHECK(ae_fireteam_regroup_note(&table, 0, brain, 1.0f) == 0);
		CHECK(ae_fireteam_order_kind(&table, 0) == _ae_order_regroup);
	}
	CHECK(ae_fireteam_regroup_complete(&table, 0));
	ae_fireteam_release_order(&table, 0);
	CHECK(ae_fireteam_order_kind(&table, 0) == _ae_order_none);
	/* big team: arrival still ends it at once, as before */
	ae_fireteam_set_rules(&table, 0);
	ae_fireteam_set_order(&table, 0, _ae_order_regroup, NULL, -1);
	for (brain = 0; brain < 3; brain++)
		ae_fireteam_regroup_note(&table, 0, brain, 1.0f);
	CHECK(ae_fireteam_order_kind(&table, 0) == _ae_order_none);
}

int main(void)
{
	arena_release_test();
	arena_rules_test();
	role_table_test();
	role_assign_test();
	role_rotation_test();
	role_balance_test();
	formation_test();
	tether_test();
	settle_test();
	role_override_test();
	stall_test();
	role_rebuild_test();
	old_client_test();
	smart_old_client_test();
	recruit_check_test();
	smart_test();
	split_even_test();
	split_cap_test();
	recruit_test();
	leader_left_test();
	sequence_test();
	order_test();
	follow_test();
	regroup_test();
	defend_test();
	carrier_priority_test();
	watch_test();
	attack_test();
	go_test();
	order_serial_test();
	if (!failures)
		printf("ae_fireteam_rules_test: ok\n");
	return failures != 0;
}
