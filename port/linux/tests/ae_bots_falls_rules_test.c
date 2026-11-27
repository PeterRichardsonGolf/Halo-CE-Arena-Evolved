#include <stdio.h>
#include <string.h>
#include "ae_bots_falls_rules.h"
#include "ae_bots_combat_rules.h"
#include <math.h>
#define NEAR(a, b) (fabs((double)(a) - (double)(b)) < 1.0e-4)

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	/* the cause of a death: nothing known is other */
	CHECK(ae_bots_death_cause(0, "weapons\\frag grenade\\explosion", 1, 0, 0, 0) == AE_BOTS_DEATH_OTHER);
	/* another player's damage, whatever it was */
	CHECK(ae_bots_death_cause(1, "weapons\\pistol\\bullet", 0, 0, 0, 0) == AE_BOTS_DEATH_PLAYER);
	CHECK(ae_bots_death_cause(1, "weapons\\frag grenade\\explosion", 0, 0, 0, 0) == AE_BOTS_DEATH_PLAYER);
	/* its own grenades, frag and plasma (stuck: "attached"), its own rockets and fuel rods */
	CHECK(ae_bots_death_cause(1, "weapons\\frag grenade\\explosion", 1, 0, 0, 0) == AE_BOTS_DEATH_OWN_GRENADE);
	CHECK(ae_bots_death_cause(1, "weapons\\plasma grenade\\attached", 1, 0, 0, 0) == AE_BOTS_DEATH_OWN_GRENADE);
	CHECK(ae_bots_death_cause(1, "weapons\\Plasma Grenade\\explosion", 1, 0, 0, 0) == AE_BOTS_DEATH_OWN_GRENADE);
	CHECK(ae_bots_death_cause(1, "weapons\\rocket launcher\\explosion", 1, 0, 0, 0) == AE_BOTS_DEATH_OWN_EXPLOSIVE);
	CHECK(ae_bots_death_cause(1, "vehicles\\ghost\\ghost bolt", 1, 0, 0, 0) == AE_BOTS_DEATH_SELF);
	/* the globals' damages: a fall past the deadliest distance, a landing, the map's kill (at once, no owner) */
	CHECK(ae_bots_death_cause(1, "globals\\distance", 0, 1, 0, 0) == AE_BOTS_DEATH_PIT);
	CHECK(ae_bots_death_cause(1, "globals\\falling", 0, 1, 0, 0) == AE_BOTS_DEATH_FALL);
	CHECK(ae_bots_death_cause(1, "globals\\falling", 0, 1, 1, 0) == AE_BOTS_DEATH_BARRIER);
	/* (a rider dies with its vehicle: not the map's kill) */
	CHECK(ae_bots_death_cause(1, "globals\\falling", 0, 1, 1, 1) == AE_BOTS_DEATH_OTHER);
	/* (only the tag's last part is read: a folder named "distance" is not a pit) */
	CHECK(ae_bots_death_cause(1, "distance\\collision", 0, 1, 0, 0) == AE_BOTS_DEATH_OTHER);
	CHECK(ae_bots_death_cause(1, NULL, 0, 1, 0, 0) == AE_BOTS_DEATH_OTHER);

	/* the words of the log */
	CHECK(!strcmp(ae_bots_death_cause_name(AE_BOTS_DEATH_PIT), "pit"));
	CHECK(!strcmp(ae_bots_death_cause_name(AE_BOTS_DEATH_OWN_GRENADE), "own_grenade"));
	CHECK(!strcmp(ae_bots_death_cause_name(-1), "other"));
	CHECK(!strcmp(ae_bots_death_cause_name(AE_BOTS_DEATH_CAUSES), "other"));

	/* the edge: further ahead each level; RECRUIT and EASY slip in a fight, never while walking; NORMAL and up always */
	{
		int level;

		for (level = AE_BOTS_LEVEL_EASY; level < AE_BOTS_LEVELS; level++)
			CHECK(ae_bots_edge_lookahead(level) >= ae_bots_edge_lookahead(level - 1));
		CHECK(ae_bots_edge_lookahead(AE_BOTS_LEVEL_RECRUIT) < ae_bots_edge_lookahead(AE_BOTS_LEVEL_NORMAL));
		CHECK(ae_bots_edge_lookahead(AE_BOTS_LEVEL_DEMON) == ae_bots_edge_lookahead(AE_BOTS_LEVEL_LEGENDARY));
		for (level = AE_BOTS_LEVEL_RECRUIT; level < AE_BOTS_LEVELS; level++)
			CHECK(ae_bots_edge_attention(level, 0) == 1.0f);
		CHECK(ae_bots_edge_attention(AE_BOTS_LEVEL_RECRUIT, 1) < ae_bots_edge_attention(AE_BOTS_LEVEL_EASY, 1));
		CHECK(ae_bots_edge_attention(AE_BOTS_LEVEL_EASY, 1) < 1.0f);
		for (level = AE_BOTS_LEVEL_NORMAL; level < AE_BOTS_LEVELS; level++)
			CHECK(ae_bots_edge_attention(level, 1) == 1.0f);
	}
	/* the lethal drop: no falling damage, the maximum falling distance; with it, the full damage's distance if less;
	15 when the globals give nothing sensible */
	CHECK(ae_bots_edge_lethal_distance(1, 16.0f, 5.0f) == 16.0f);
	CHECK(ae_bots_edge_lethal_distance(0, 16.0f, 5.0f) == 5.0f);
	CHECK(ae_bots_edge_lethal_distance(0, 16.0f, 0.0f) == 16.0f);
	CHECK(ae_bots_edge_lethal_distance(1, 0.0f, 0.0f) == 15.0f);
	CHECK(ae_bots_edge_lethal_distance(1, 1000.0f, 0.0f) == 15.0f);
	/* a step: no floor is lethal, a drop at nine tenths of it is, a ledge below is not */
	CHECK(ae_bots_edge_lethal(0, 0.0f, 16.0f));
	CHECK(ae_bots_edge_lethal(1, 14.5f, 16.0f));
	CHECK(!ae_bots_edge_lethal(1, 14.0f, 16.0f));
	CHECK(!ae_bots_edge_lethal(1, 2.0f, 16.0f));
	CHECK(!ae_bots_edge_lethal(1, -1.0f, 16.0f));
	/* the ways tried: a quarter turn to the side first, then the other, ..., back last; none past the last */
	CHECK(ae_bots_edge_turn(0, 1, 0) > 1.5f && ae_bots_edge_turn(0, 1, 0) < 1.6f);
	CHECK(ae_bots_edge_turn(0, -1, 0) < -1.5f);
	CHECK(ae_bots_edge_turn(1, 1, 0) < -1.5f);
	CHECK(ae_bots_edge_turn(AE_BOTS_EDGE_TURNS - 1, 1, 0) > 3.1f);
	CHECK(ae_bots_edge_turn(AE_BOTS_EDGE_TURNS, 1, 0) == 0.0f);
	/* (walking: back first, along the edge last) */
	CHECK(ae_bots_edge_turn(0, 1, 1) > 3.1f);
	CHECK(ae_bots_edge_turn(AE_BOTS_EDGE_TURNS - 1, 1, 1) < -0.7f && ae_bots_edge_turn(AE_BOTS_EDGE_TURNS - 1, 1, 1) > -0.8f);

	/* grenades: its own within the danger at EASY and up; never another's, never past the danger */
	CHECK(!ae_bots_grenade_flee(AE_BOTS_LEVEL_RECRUIT, 1, 2.0f));
	CHECK(ae_bots_grenade_flee(AE_BOTS_LEVEL_EASY, 1, 2.0f));
	CHECK(ae_bots_grenade_flee(AE_BOTS_LEVEL_DEMON, 1, AE_BOTS_GRENADE_DANGER - 0.1f));
	CHECK(!ae_bots_grenade_flee(AE_BOTS_LEVEL_DEMON, 1, AE_BOTS_GRENADE_DANGER + 0.1f));
	CHECK(!ae_bots_grenade_flee(AE_BOTS_LEVEL_DEMON, 0, 1.0f));
	/* someone else's grenade: judged by the dodge chance (RECRUIT never), fled within the danger after the reaction */
	CHECK(!ae_bots_grenade_judge(AE_BOTS_LEVEL_RECRUIT, 0.0f));
	CHECK(ae_bots_grenade_judge(AE_BOTS_LEVEL_EASY, 0.2f) && !ae_bots_grenade_judge(AE_BOTS_LEVEL_EASY, 0.3f));
	CHECK(ae_bots_grenade_judge(AE_BOTS_LEVEL_DEMON, 0.85f) && !ae_bots_grenade_judge(AE_BOTS_LEVEL_DEMON, 0.95f));
	CHECK(ae_bots_grenade_flee_other(AE_BOTS_LEVEL_HEROIC, 0.5f, 1, 3.0f));
	CHECK(!ae_bots_grenade_flee_other(AE_BOTS_LEVEL_HEROIC, 0.2f, 1, 3.0f));
	CHECK(!ae_bots_grenade_flee_other(AE_BOTS_LEVEL_HEROIC, 0.5f, 0, 3.0f));
	CHECK(!ae_bots_grenade_flee_other(AE_BOTS_LEVEL_HEROIC, 0.5f, 1, AE_BOTS_GRENADE_DANGER + 0.1f));
	CHECK(!ae_bots_grenade_flee_other(AE_BOTS_LEVEL_RECRUIT, 5.0f, 1, 1.0f));
	CHECK(ae_bots_grenade_flee_other(AE_BOTS_LEVEL_DEMON, 0.25f, 1, 1.0f) && !ae_bots_grenade_flee_other(AE_BOTS_LEVEL_EASY, 0.25f, 1, 1.0f));
	/* a throw: not at a wall within 4 units, not at a target within the blast */
	CHECK(ae_bots_grenade_throw_safe(1000.0f, 10.0f));
	CHECK(!ae_bots_grenade_throw_safe(2.0f, 10.0f));
	CHECK(!ae_bots_grenade_throw_safe(1000.0f, 3.0f));
	CHECK(ae_bots_grenade_throw_safe(AE_BOTS_GRENADE_WALL, AE_BOTS_GRENADE_BLAST));
	/* (a wall well short of the target, within 10 units: no; one at the target (its cover), or past 10: yes) */
	CHECK(!ae_bots_grenade_throw_safe(6.0f, 14.0f));
	CHECK(ae_bots_grenade_throw_safe(12.0f, 14.0f));
	CHECK(ae_bots_grenade_throw_safe(8.0f, 10.0f));
	CHECK(ae_bots_grenade_throw_safe(1000.0f, 1000.0f));

	/* explosives: NORMAL and up hold fire within the blast (target or wall); RECRUIT and EASY do not */
	CHECK(!ae_bots_explosive_fire_safe(AE_BOTS_LEVEL_NORMAL, 1000.0f, 3.0f));
	CHECK(ae_bots_explosive_fire_safe(AE_BOTS_LEVEL_NORMAL, 1000.0f, 4.2f));
	CHECK(!ae_bots_explosive_fire_safe(AE_BOTS_LEVEL_HEROIC, 1000.0f, 3.2f));
	CHECK(ae_bots_explosive_fire_safe(AE_BOTS_LEVEL_LEGENDARY, 3.1f, 3.1f));
	CHECK(!ae_bots_explosive_fire_safe(AE_BOTS_LEVEL_DEMON, 1000.0f, 2.5f));
	CHECK(!ae_bots_explosive_fire_safe(AE_BOTS_LEVEL_DEMON, 2.0f, 20.0f));
	CHECK(ae_bots_explosive_fire_safe(AE_BOTS_LEVEL_DEMON, 1000.0f, 8.0f));
	CHECK(ae_bots_explosive_fire_safe(AE_BOTS_LEVEL_EASY, 1000.0f, 1.0f));
	CHECK(ae_bots_explosive_fire_safe(AE_BOTS_LEVEL_RECRUIT, 0.5f, 1.0f));

	/* a small drop to a lower goal is allowed; a big one, a level goal, or a step away from the goal is not */
	/* (1.0 rc2: Warren's walkable 34 degree ramp, n.k 0.826, is walked; the same unwalkable, or a 50 degree one, slides) */
	CHECK(!ae_bots_edge_floor_slides(0.99f, 0) && !ae_bots_edge_floor_slides(0.85f, 0));
	CHECK(!ae_bots_edge_floor_slides(0.826f, 1) && !ae_bots_edge_floor_slides(0.7f, 1));
	CHECK(ae_bots_edge_floor_slides(0.826f, 0) && ae_bots_edge_floor_slides(0.64f, 1));
	CHECK(ae_bots_edge_drop_allowed(1.6f, 1.6f, 0.9f));
	CHECK(ae_bots_edge_drop_allowed(2.0f, 0.5f, 0.5f));
	CHECK(!ae_bots_edge_drop_allowed(2.1f, 3.0f, 1.0f));
	CHECK(!ae_bots_edge_drop_allowed(1.6f, 0.2f, 1.0f));
	CHECK(!ae_bots_edge_drop_allowed(1.6f, 1.6f, 0.2f));
	CHECK(!ae_bots_edge_drop_allowed(0.0f, 1.6f, 1.0f));
	/* a hurt bot's lethal drop: HALO 2 fall damage (6, 10), full body 10, half body between, none left 6 */
	CHECK(NEAR(ae_bots_edge_hurt_lethal_distance(10.0f, 0, 6.0f, 10.0f, 1.0f), 10.0f));
	CHECK(NEAR(ae_bots_edge_hurt_lethal_distance(10.0f, 0, 6.0f, 10.0f, 0.0f), 6.0f));
	CHECK(ae_bots_edge_hurt_lethal_distance(10.0f, 0, 6.0f, 10.0f, 0.5f) > 7.8f &&
		ae_bots_edge_hurt_lethal_distance(10.0f, 0, 6.0f, 10.0f, 0.5f) < 7.9f);
	/* (Damnation 11-15: a drop of 8.1 takes 0.56 of a body; with the guard's margin a bot with under 3/4 of its body goes
	round it) */
	CHECK(ae_bots_edge_lethal(1, 8.1f, ae_bots_edge_hurt_lethal_distance(10.0f, 0, 6.0f, 10.0f, 0.4f)));
	CHECK(!ae_bots_edge_lethal(1, 8.1f, ae_bots_edge_hurt_lethal_distance(10.0f, 0, 6.0f, 10.0f, 1.0f)));
	CHECK(NEAR(ae_bots_edge_hurt_lethal_distance(10.0f, 0, 6.0f, 10.0f, -1.0f), 6.0f));
	/* no falling damage, nonsense bounds: as given */
	CHECK(NEAR(ae_bots_edge_hurt_lethal_distance(14.0f, 1, 6.0f, 10.0f, 0.1f), 14.0f));
	CHECK(NEAR(ae_bots_edge_hurt_lethal_distance(10.0f, 0, 0.0f, 10.0f, 0.1f), 10.0f));
	CHECK(NEAR(ae_bots_edge_hurt_lethal_distance(10.0f, 0, 10.0f, 6.0f, 0.1f), 10.0f));
	/* never above the lethal drop given (a map whose maximum is under its full damage distance) */
	CHECK(NEAR(ae_bots_edge_hurt_lethal_distance(5.0f, 0, 6.0f, 10.0f, 0.5f), 5.0f));
	/* (a shot bot keeps its level: Prisoner's 3.2 floor drop held, a 30 degree ramp 2.5 units ahead (1.44) not; full
	shields, no fight or a carrier: as given; never above the drop given) */
	/* (Prisoner: top floor to the ground 3.6 is not taken, top to the middle floor 1.8 is; fighting or not, any shields) */
	CHECK(NEAR(ae_bots_edge_keep_level_lethal(10.0f, 0), 2.5f));
	CHECK(ae_bots_edge_lethal(1, 3.6f, ae_bots_edge_keep_level_lethal(10.0f, 0)));
	CHECK(ae_bots_edge_lethal(1, 3.2f, ae_bots_edge_keep_level_lethal(10.0f, 0)));
	CHECK(!ae_bots_edge_lethal(1, 1.8f, ae_bots_edge_keep_level_lethal(10.0f, 0)));
	CHECK(!ae_bots_edge_lethal(1, 1.44f, ae_bots_edge_keep_level_lethal(10.0f, 0)));
	CHECK(NEAR(ae_bots_edge_keep_level_lethal(10.0f, 1), 10.0f));
	CHECK(NEAR(ae_bots_edge_keep_level_lethal(1.0f, 0), 1.0f));
	/* the landing's ring: forward points 0, 1 and 7; one forward point or 3 in all */
	CHECK(ae_bots_edge_ring_forward(0) && ae_bots_edge_ring_forward(1) && ae_bots_edge_ring_forward(7));
	CHECK(!ae_bots_edge_ring_forward(2) && !ae_bots_edge_ring_forward(4) && !ae_bots_edge_ring_forward(6));
	CHECK(ae_bots_edge_ring_lethal(1, 1));
	CHECK(!ae_bots_edge_ring_lethal(2, 0));
	CHECK(ae_bots_edge_ring_lethal(3, 0));
	CHECK(!ae_bots_edge_ring_lethal(0, 0));
	/* a link jump's run-up: its own way is not guarded, a step back or aside is */
	CHECK(!ae_bots_edge_link_step_checked(1.0f));
	CHECK(!ae_bots_edge_link_step_checked(0.7f));
	CHECK(ae_bots_edge_link_step_checked(0.0f));
	CHECK(ae_bots_edge_link_step_checked(-1.0f));
	/* deadly volumes: the heights a fall from 0 to a floor 3 down passes (-2.5 .. 0.5 by 1), a step onto the level floor
	one; the room given kept */
	{
		float heights[16];

		CHECK(ae_bots_edge_volume_heights(0.0f, -3.0f, heights, 16) == 3 && NEAR(heights[0], -2.5f) &&
			NEAR(heights[2], -0.5f));
		CHECK(ae_bots_edge_volume_heights(0.0f, 0.0f, heights, 16) == 1 && NEAR(heights[0], 0.5f));
		CHECK(ae_bots_edge_volume_heights(0.0f, 1.0f, heights, 16) == 1);
		CHECK(ae_bots_edge_volume_heights(20.0f, -3.0f, heights, 4) == 4);
		CHECK(ae_bots_edge_volume_heights(0.0f, -3.0f, heights, 0) == 0);
	}
	CHECK(ae_bots_falls_volume_learn(1, 1, 0, 0, 3));
	CHECK(!ae_bots_falls_volume_learn(1, 1, 0, 0, 2));	/* two kills: a reset, a telefrag twice (0.9.4 review) */
	CHECK(!ae_bots_falls_volume_learn(1, 1, 0, 0, 1));
	CHECK(!ae_bots_falls_volume_learn(0, 1, 0, 0, 3));
	CHECK(!ae_bots_falls_volume_learn(1, 0, 0, 0, 3));
	CHECK(!ae_bots_falls_volume_learn(1, 1, 1, 0, 3));
	CHECK(!ae_bots_falls_volume_learn(1, 1, 0, 1, 3));	/* riding */
	{
		long victims[3] = { 4, 7, 0 };

		CHECK(ae_bots_falls_volume_victim_new(victims, 2, 5));
		CHECK(!ae_bots_falls_volume_victim_new(victims, 2, 7));
		CHECK(ae_bots_falls_volume_victim_new(victims, 0, 7));
		CHECK(ae_bots_falls_volume_victim_new(NULL, 0, 1));
	}
	/* kills of one tick count once (a round's reset) */
	CHECK(ae_bots_falls_volume_kill_counts(1, 0));
	CHECK(!ae_bots_falls_volume_kill_counts(0, 0));
	CHECK(!ae_bots_falls_volume_kill_counts(0, AE_BOTS_FALLS_VOLUME_KILL_APART - 1));
	CHECK(ae_bots_falls_volume_kill_counts(0, AE_BOTS_FALLS_VOLUME_KILL_APART));
	CHECK(!ae_bots_falls_volume_cleared(AE_BOTS_FALLS_VOLUME_ALIVE_TICKS - 1));
	CHECK(ae_bots_falls_volume_cleared(AE_BOTS_FALLS_VOLUME_ALIVE_TICKS));

	/* dodging: the tables as they were; the later rolls half the level's chance, none for RECRUIT; due once an interval
	has passed, a vehicle's only, not after a dodge, and again if the clock started over */
	CHECK(NEAR(ae_bots_dodge_chance(AE_BOTS_LEVEL_EASY), 0.25f) && NEAR(ae_bots_dodge_chance(AE_BOTS_LEVEL_NORMAL), 0.45f));
	CHECK(NEAR(ae_bots_dodge_reaction(AE_BOTS_LEVEL_EASY), 0.60f) && ae_bots_dodge_reaction(AE_BOTS_LEVEL_RECRUIT) > 50.0f);
	CHECK(NEAR(ae_bots_dodge_reroll_chance(AE_BOTS_LEVEL_NORMAL), 0.225f) && ae_bots_dodge_reroll_chance(AE_BOTS_LEVEL_RECRUIT) == 0.0f);
	CHECK(ae_bots_dodge_chance(99) == ae_bots_dodge_chance(AE_BOTS_LEVEL_DEMON) && ae_bots_dodge_chance(-3) == 0.0f);
	CHECK(!ae_bots_dodge_reroll_due(1, 0, 100 + AE_BOTS_DODGE_REROLL_TICKS - 1, 100));
	CHECK(ae_bots_dodge_reroll_due(1, 0, 100 + AE_BOTS_DODGE_REROLL_TICKS, 100));
	CHECK(!ae_bots_dodge_reroll_due(0, 0, 500, 100) && !ae_bots_dodge_reroll_due(1, 1, 500, 100));
	CHECK(ae_bots_dodge_reroll_due(1, 0, 10, 5000));
	/* the skill difference survives: over the rolls a warned pass gets, NORMAL beats EASY (EASY 0.9 s of warning, 3 rolls
	at most, NORMAL 1.05 s, 4) and RECRUIT never dodges */
	{
		int level;
		float last = -1.0f;

		for (level = AE_BOTS_LEVEL_RECRUIT; level <= AE_BOTS_LEVEL_DEMON; level++)
		{
			float warn = 1.5f - ae_bots_dodge_reaction(level), miss = 1.0f - ae_bots_dodge_chance(level);
			int rolls = (int)(warn * 30.0f) / AE_BOTS_DODGE_REROLL_TICKS, roll;
			float reroll_miss = 1.0f - ae_bots_dodge_reroll_chance(level);

			if (level == AE_BOTS_LEVEL_RECRUIT)
			{
				CHECK(ae_bots_dodge_chance(level) == 0.0f);
				continue;
			}
			for (roll = 0; roll < rolls; roll++)
				miss *= reroll_miss;
			CHECK(1.0f - miss > last);
			last = 1.0f - miss;
		}
	}
	/* priority: a dodge or a recovery or a freed role stops the tactical roles' moves */
	CHECK(ae_bots_tactical_move_allowed(0, 100, 50, 50));
	CHECK(!ae_bots_tactical_move_allowed(1, 100, 50, 50));
	CHECK(!ae_bots_tactical_move_allowed(0, 100, 101, 50));
	CHECK(!ae_bots_tactical_move_allowed(0, 100, 50, 101));

	if (failures)
		printf("%d failed\n", failures);
	else
		printf("ae_bots_falls_rules: all passed\n");
	return failures != 0;
}
