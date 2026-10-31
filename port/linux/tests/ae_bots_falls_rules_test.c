#include <stdio.h>
#include <string.h>
#include "ae_bots_falls_rules.h"
#include "ae_bots_combat_rules.h"

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
	CHECK(ae_bots_edge_drop_allowed(1.6f, 1.6f, 0.9f));
	CHECK(ae_bots_edge_drop_allowed(2.0f, 0.5f, 0.5f));
	CHECK(!ae_bots_edge_drop_allowed(2.1f, 3.0f, 1.0f));
	CHECK(!ae_bots_edge_drop_allowed(1.6f, 0.2f, 1.0f));
	CHECK(!ae_bots_edge_drop_allowed(1.6f, 1.6f, 0.2f));
	CHECK(!ae_bots_edge_drop_allowed(0.0f, 1.6f, 1.0f));

	if (failures)
		printf("%d failed\n", failures);
	else
		printf("ae_bots_falls_rules: all passed\n");
	return failures != 0;
}
