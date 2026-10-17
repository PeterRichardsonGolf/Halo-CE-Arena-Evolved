#include <stdio.h>
#include <string.h>
#include "ae_bots_objective_rules.h"

static int failures;
static int checks;
#define CHECK(c) do { checks++; if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

/* a bot of the mode with no goal anywhere, full health, nothing in sight */
static struct ae_bots_situation situation(int mode)
{
	struct ae_bots_situation s;

	memset(&s, 0, sizeof(s));
	s.mode = mode;
	s.health = 1.0f;
	s.enemy_distance = -1.0f;
	s.enemy_flag = s.own_flag_away = s.own_stand = s.ally_carrier = -1.0f;
	s.ball = s.enemy_carrier = s.hill = s.race_flag = -1.0f;
	return s;
}

static int best(struct ae_bots_situation const *s)
{
	float utility[AE_BOTS_BEHAVIOURS];

	ae_bots_objective_utilities(s, utility);
	return ae_bots_objective_pick(utility, -1, 0, 0, 0.0f);
}

/* every utility 0 to 1, whatever the input (far, negative, hurt past 0, overshield past 1) */
static void test_range(void)
{
	int mode, flags;
	float utility[AE_BOTS_BEHAVIOURS];

	for (mode = 0; mode < AE_BOTS_MODES; mode++)
	{
		for (flags = 0; flags < 64; flags++)
		{
			struct ae_bots_situation s = situation(mode);
			int index;

			s.fighting = flags & 1;
			s.carrying = (flags >> 1) & 1;
			s.defender = (flags >> 2) & 1;
			s.in_hill = (flags >> 3) & 1;
			s.own_flag_carried = (flags >> 4) & 1;
			s.health = (flags >> 5) & 1 ? 3.0f : -1.0f;
			s.enemy_distance = (flags & 1) ? 0.0f : 1000.0f;
			s.enemy_flag = s.own_flag_away = s.own_stand = s.ally_carrier = 0.0f;
			s.ball = s.enemy_carrier = s.hill = s.race_flag = 0.0f;
			ae_bots_objective_utilities(&s, utility);
			for (index = 0; index < AE_BOTS_BEHAVIOURS; index++)
				CHECK(utility[index] >= 0.0f && utility[index] <= 1.0f);
		}
	}
}

/* Slayer: bots.c's brain only; any mode with nothing to go for: the same */
static void test_slayer(void)
{
	struct ae_bots_situation s = situation(AE_BOTS_MODE_SLAYER);
	float utility[AE_BOTS_BEHAVIOURS];
	int index;

	s.enemy_flag = s.ball = s.hill = 1.0f;
	ae_bots_objective_utilities(&s, utility);
	for (index = 1; index < AE_BOTS_BEHAVIOURS; index++)
		CHECK(utility[index] == 0.0f);
	CHECK(best(&s) == AE_BOTS_FIGHT);
	s = situation(AE_BOTS_MODE_KING);
	CHECK(best(&s) == AE_BOTS_FIGHT);
}

static void test_ctf(void)
{
	struct ae_bots_situation s = situation(AE_BOTS_MODE_CTF);

	/* an attacker goes for the enemy flag across the map */
	s.enemy_flag = 60.0f;
	s.own_stand = 60.0f;
	CHECK(best(&s) == AE_BOTS_CTF_TAKE);
	/* a defender keeps to its stand, unless the enemy flag is right there */
	s.defender = 1;
	CHECK(best(&s) == AE_BOTS_CTF_DEFEND);
	s.enemy_flag = 0.5f;
	CHECK(best(&s) == AE_BOTS_CTF_TAKE);
	s.enemy_flag = 60.0f;
	s.defender = 0;
	/* a fight close up wins over a flag far; a flag near over a fight far */
	s.fighting = 1;
	s.enemy_distance = 2.0f;
	CHECK(best(&s) == AE_BOTS_FIGHT);
	s.enemy_distance = 30.0f;
	s.enemy_flag = 4.0f;
	CHECK(best(&s) == AE_BOTS_CTF_TAKE);
	/* the carrier runs home: point blank, nearly dead (sacrifice play) */
	s.carrying = 1;
	s.enemy_distance = 0.0f;
	s.health = 0.05f;
	s.own_flag_away = 3.0f;
	CHECK(best(&s) == AE_BOTS_CTF_CARRY);
	s.carrying = 0;
	s.health = 1.0f;
	/* the own flag dropped near: touched, even in a fight; carried away: after its carrier */
	s.own_flag_away = 5.0f;
	s.enemy_distance = 3.0f;
	CHECK(best(&s) == AE_BOTS_CTF_RETURN);
	s.fighting = 0;
	s.own_flag_carried = 1;
	s.own_flag_away = 40.0f;
	s.enemy_flag = 60.0f;
	CHECK(best(&s) == AE_BOTS_CTF_RETURN);
	/* a teammate carries: escort it (the enemy flag is not free) */
	s = situation(AE_BOTS_MODE_CTF);
	s.own_stand = 30.0f;
	s.ally_carrier = 12.0f;
	CHECK(best(&s) == AE_BOTS_ESCORT);
	s.defender = 1;
	CHECK(best(&s) == AE_BOTS_CTF_DEFEND);
	/* hurt, an attacker still goes, if less keenly */
	{
		float utility[AE_BOTS_BEHAVIOURS], hurt[AE_BOTS_BEHAVIOURS];

		s = situation(AE_BOTS_MODE_CTF);
		s.enemy_flag = 20.0f;
		ae_bots_objective_utilities(&s, utility);
		s.health = 0.2f;
		ae_bots_objective_utilities(&s, hurt);
		CHECK(hurt[AE_BOTS_CTF_TAKE] < utility[AE_BOTS_CTF_TAKE]);
		CHECK(hurt[AE_BOTS_CTF_TAKE] > 0.4f);
	}
}

static void test_oddball(void)
{
	struct ae_bots_situation s = situation(AE_BOTS_MODE_ODDBALL);

	s.ball = 30.0f;
	CHECK(best(&s) == AE_BOTS_BALL_TAKE);
	s.carrying = 1;
	s.fighting = 1;
	s.enemy_distance = 0.0f;
	CHECK(best(&s) == AE_BOTS_BALL_HOLD);
	s = situation(AE_BOTS_MODE_ODDBALL);
	s.enemy_carrier = 25.0f;
	CHECK(best(&s) == AE_BOTS_BALL_CHASE);
	s.enemy_carrier = -1.0f;
	s.ally_carrier = 10.0f;
	CHECK(best(&s) == AE_BOTS_ESCORT);
	/* reverse tag: "it" keeps away; the others go for it */
	s = situation(AE_BOTS_MODE_TAG);
	s.carrying = 1;
	CHECK(best(&s) == AE_BOTS_BALL_HOLD);
	s.carrying = 0;
	s.enemy_carrier = 20.0f;
	CHECK(best(&s) == AE_BOTS_BALL_CHASE);
}

static void test_juggernaut(void)
{
	struct ae_bots_situation s = situation(AE_BOTS_MODE_JUGGERNAUT);

	/* the juggernaut hunts as bots.c does; badly hurt, it breaks away; the others go for it */
	s.carrying = 1;
	CHECK(best(&s) == AE_BOTS_FIGHT);
	s.health = 0.2f;
	CHECK(best(&s) == AE_BOTS_JUGG_FLEE);
	s = situation(AE_BOTS_MODE_JUGGERNAUT);
	s.enemy_carrier = 30.0f;
	CHECK(best(&s) == AE_BOTS_JUGG_HUNT);
	s.fighting = 1;
	s.enemy_distance = 1.0f;
	CHECK(best(&s) == AE_BOTS_FIGHT);
}

static void test_king_race(void)
{
	struct ae_bots_situation s = situation(AE_BOTS_MODE_KING);

	s.hill = 50.0f;
	CHECK(best(&s) == AE_BOTS_HILL_GO);
	s.in_hill = 1;
	s.fighting = 1;
	s.enemy_distance = 0.0f;
	CHECK(best(&s) == AE_BOTS_HILL_HOLD);
	s = situation(AE_BOTS_MODE_RACE);
	s.race_flag = 80.0f;
	s.fighting = 1;
	s.enemy_distance = 0.0f;
	CHECK(best(&s) == AE_BOTS_RACE_NEXT);
}

/* the current behaviour's bonus keeps it a while, then the better one takes over; a tie keeps the current one */
static void test_hysteresis(void)
{
	float utility[AE_BOTS_BEHAVIOURS];
	int index;

	for (index = 0; index < AE_BOTS_BEHAVIOURS; index++)
		utility[index] = 0.0f;
	utility[AE_BOTS_FIGHT] = 0.6f;
	utility[AE_BOTS_CTF_TAKE] = 0.65f;
	CHECK(ae_bots_objective_pick(utility, AE_BOTS_FIGHT, 10, 60, 0.15f) == AE_BOTS_FIGHT);
	CHECK(ae_bots_objective_pick(utility, AE_BOTS_FIGHT, 60, 60, 0.15f) == AE_BOTS_CTF_TAKE);
	CHECK(ae_bots_objective_pick(utility, AE_BOTS_CTF_TAKE, 0, 60, 0.15f) == AE_BOTS_CTF_TAKE);
	utility[AE_BOTS_FIGHT] = 0.9f;
	CHECK(ae_bots_objective_pick(utility, AE_BOTS_CTF_TAKE, 0, 60, 0.15f) == AE_BOTS_FIGHT);
	/* (a tie) */
	utility[AE_BOTS_FIGHT] = 0.65f;
	CHECK(ae_bots_objective_pick(utility, AE_BOTS_CTF_TAKE, 100, 60, 0.15f) == AE_BOTS_CTF_TAKE);
	CHECK(ae_bots_objective_pick(utility, -1, 0, 60, 0.15f) == AE_BOTS_FIGHT);
	/* a behaviour gone (utility 0) is not kept by its bonus; a current one out of range is none */
	utility[AE_BOTS_CTF_TAKE] = 0.0f;
	CHECK(ae_bots_objective_pick(utility, AE_BOTS_CTF_TAKE, 0, 60, 0.9f) == AE_BOTS_FIGHT);
	CHECK(ae_bots_objective_pick(utility, AE_BOTS_BEHAVIOURS + 3, 0, 60, 0.9f) == AE_BOTS_FIGHT);
	CHECK(ae_bots_objective_pick(utility, -7, 0, 60, 0.9f) == AE_BOTS_FIGHT);
}

static void test_roles_and_names(void)
{
	int behaviour;

	CHECK(ae_bots_ctf_defenders(0) == 0);
	CHECK(ae_bots_ctf_defenders(2) == 0);
	CHECK(ae_bots_ctf_defenders(3) == 1);
	CHECK(ae_bots_ctf_defenders(5) == 1);
	CHECK(ae_bots_ctf_defenders(6) == 2);
	CHECK(ae_bots_ctf_defenders(16) == 2);
	CHECK(ae_bots_objective_runs_while_fighting(AE_BOTS_CTF_CARRY));
	CHECK(ae_bots_objective_runs_while_fighting(AE_BOTS_HILL_HOLD));
	CHECK(!ae_bots_objective_runs_while_fighting(AE_BOTS_FIGHT));
	CHECK(!ae_bots_objective_runs_while_fighting(AE_BOTS_CTF_DEFEND));
	CHECK(!ae_bots_objective_runs_while_fighting(AE_BOTS_JUGG_HUNT));
	for (behaviour = 0; behaviour < AE_BOTS_BEHAVIOURS; behaviour++)
		CHECK(strcmp(ae_bots_behaviour_name(behaviour), "?") != 0);
	CHECK(strcmp(ae_bots_behaviour_name(-1), "?") == 0);
	CHECK(strcmp(ae_bots_behaviour_name(AE_BOTS_BEHAVIOURS), "?") == 0);
	CHECK(strcmp(ae_bots_mode_name(AE_BOTS_MODE_CTF), "ctf") == 0);
	CHECK(strcmp(ae_bots_mode_name(AE_BOTS_MODES), "?") == 0);
}

int main(void)
{
	test_range();
	test_slayer();
	test_ctf();
	test_oddball();
	test_juggernaut();
	test_king_race();
	test_hysteresis();
	test_roles_and_names();
	printf("%d checks, %d failures\n", checks, failures);
	return failures ? 1 : 0;
}
