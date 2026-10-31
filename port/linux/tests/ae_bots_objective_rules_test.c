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
	s.bomb = s.enemy_goal = s.own_goal = s.bomb_planted_own = s.bomb_planted_enemy = -1.0f;
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
			s.bomb = s.enemy_goal = s.own_goal = s.bomb_planted_own = 0.0f;
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

	s.enemy_flag = s.ball = s.hill = s.bomb = s.enemy_goal = s.own_goal = s.bomb_planted_own = 1.0f;
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
	s.wingman = 1;
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
	s.wingman = 1;
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

/* Bomb: the loose bomb fetched (an attacker from across the map, a defender only close), its carrier to the enemy
goal whatever it meets, teammates escorting, the other team hunting the carrier or keeping its goal, a bomb planted in
the own goal saved */
static void test_bomb(void)
{
	struct ae_bots_situation s = situation(AE_BOTS_MODE_BOMB);

	s.bomb = 40.0f;
	s.own_goal = 30.0f;
	s.enemy_goal = 30.0f;
	CHECK(best(&s) == AE_BOTS_BOMB_TAKE);
	s.defender = 1;
	CHECK(best(&s) == AE_BOTS_BOMB_DEFEND);
	s.bomb = 0.5f;
	CHECK(best(&s) == AE_BOTS_BOMB_TAKE);
	s.defender = 0;
	/* a fight close up wins over the bomb far; the bomb near over a fight far */
	s.bomb = 40.0f;
	s.fighting = 1;
	s.enemy_distance = 2.0f;
	CHECK(best(&s) == AE_BOTS_FIGHT);
	s.enemy_distance = 30.0f;
	s.bomb = 3.0f;
	CHECK(best(&s) == AE_BOTS_BOMB_TAKE);
	/* the carrier to the enemy goal: point blank, nearly dead; with no enemy goal, nothing to carry it to */
	s.bomb = -1.0f;
	s.carrying = 1;
	s.enemy_distance = 0.0f;
	s.health = 0.05f;
	CHECK(best(&s) == AE_BOTS_BOMB_CARRY);
	s.enemy_goal = -1.0f;
	CHECK(best(&s) != AE_BOTS_BOMB_CARRY);
	/* a teammate carries: escorted (a defender keeps home) */
	s = situation(AE_BOTS_MODE_BOMB);
	s.own_goal = 30.0f;
	s.enemy_goal = 30.0f;
	s.ally_carrier = 12.0f;
	s.wingman = 1;
	CHECK(best(&s) == AE_BOTS_ESCORT);
	s.defender = 1;
	CHECK(best(&s) == AE_BOTS_BOMB_DEFEND);
	/* an enemy carries: hunted; a defender stays home until it comes near */
	s = situation(AE_BOTS_MODE_BOMB);
	s.own_goal = 30.0f;
	s.enemy_carrier = 25.0f;
	CHECK(best(&s) == AE_BOTS_BALL_CHASE);
	s.defender = 1;
	CHECK(best(&s) == AE_BOTS_BOMB_DEFEND);
	s.enemy_carrier = 2.0f;
	CHECK(best(&s) == AE_BOTS_BALL_CHASE);
	/* the bomb planted in the own goal: onto it, even in a fight close by */
	s = situation(AE_BOTS_MODE_BOMB);
	s.own_goal = 3.0f;
	s.bomb_planted_own = 3.0f;
	s.fighting = 1;
	s.enemy_distance = 1.0f;
	CHECK(best(&s) == AE_BOTS_BOMB_SAVE);
	/* the carrier, the bomb's fetcher and the saver keep going while they fight; the guard fights */
	CHECK(ae_bots_objective_runs_while_fighting(AE_BOTS_BOMB_CARRY, 0));
	CHECK(ae_bots_objective_runs_while_fighting(AE_BOTS_BOMB_TAKE, 0));
	CHECK(ae_bots_objective_runs_while_fighting(AE_BOTS_BOMB_SAVE, 0));
	CHECK(!ae_bots_objective_runs_while_fighting(AE_BOTS_BOMB_DEFEND, 0));
	CHECK(strcmp(ae_bots_mode_name(AE_BOTS_MODE_BOMB), "bomb") == 0);
	/* the other modes give no Bomb behaviour */
	{
		int mode;

		for (mode = 0; mode < AE_BOTS_MODES; mode++)
		{
			float utility[AE_BOTS_BEHAVIOURS];

			if (mode == AE_BOTS_MODE_BOMB)
				continue;
			s = situation(mode);
			s.bomb = s.enemy_goal = s.own_goal = s.bomb_planted_own = 1.0f;
			s.carrying = 1;
			ae_bots_objective_utilities(&s, utility);
			CHECK(utility[AE_BOTS_BOMB_TAKE] == 0.0f && utility[AE_BOTS_BOMB_CARRY] == 0.0f &&
				utility[AE_BOTS_BOMB_DEFEND] == 0.0f && utility[AE_BOTS_BOMB_SAVE] == 0.0f);
		}
	}
}

/* SEARCH AND DESTROY: the attacking team moves together and guards a plant; the defenders leave one bot at the
site, then one defuses while the others cover it */
static void test_bomb_snd(void)
{
	struct ae_bots_situation s = situation(AE_BOTS_MODE_BOMB);

	s.bomb_snd = 1;
	s.bomb_attacking = 1;
	s.own_goal = s.enemy_goal = 30.0f;
	/* an attacker never guards its spawn when there is no bomb to act on */
	CHECK(best(&s) == AE_BOTS_FIGHT);
	/* once a teammate has it, the push stays together even under point-blank fire */
	s.ally_carrier = 12.0f;
	s.wingman = 1;
	s.fighting = 1;
	s.enemy_distance = 0.0f;
	CHECK(best(&s) == AE_BOTS_ESCORT);
	CHECK(ae_bots_objective_runs_while_fighting(AE_BOTS_ESCORT, 1));
	/* the other attackers still push, but only the assigned bot has wingman priority */
	s.wingman = 0;
	s.fighting = 0;
	CHECK(best(&s) == AE_BOTS_ESCORT);
	/* after the plant, attackers hold the bomb instead of roaming after enemies */
	s.ally_carrier = -1.0f;
	s.bomb_planted_enemy = 8.0f;
	CHECK(best(&s) == AE_BOTS_BOMB_DEFEND);
	CHECK(ae_bots_objective_runs_while_fighting(AE_BOTS_BOMB_DEFEND, 1));

	/* one defender holds the site; the rest meet a push and hunt its carrier */
	s = situation(AE_BOTS_MODE_BOMB);
	s.bomb_snd = 1;
	s.own_goal = 30.0f;
	CHECK(best(&s) == AE_BOTS_FIGHT);
	s.defender = 1;
	CHECK(best(&s) == AE_BOTS_BOMB_DEFEND);
	s.enemy_carrier = 25.0f;
	CHECK(best(&s) == AE_BOTS_BOMB_DEFEND);
	s.defender = 0;
	CHECK(best(&s) == AE_BOTS_BALL_CHASE);

	/* a planted bomb gets one defuser; its teammates cover it, even with an enemy close */
	s = situation(AE_BOTS_MODE_BOMB);
	s.bomb_snd = 1;
	s.bomb_planted_own = 3.0f;
	s.bomb_defuser = 1;
	CHECK(best(&s) == AE_BOTS_BOMB_SAVE);
	s.bomb_defuser = 0;
	s.fighting = 1;
	s.enemy_distance = 0.0f;
	CHECK(best(&s) == AE_BOTS_BOMB_DEFEND);
	CHECK(!ae_bots_objective_runs_while_fighting(AE_BOTS_ESCORT, 0));
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

static struct ae_bots_carrier_situation carrier(int mode, int level)
{
	struct ae_bots_carrier_situation s;

	memset(&s, 0, sizeof(s));
	s.mode = mode;
	s.skill_level = level;
	s.bomb_snd = mode == AE_BOTS_MODE_BOMB;
	s.carrying = 1;
	s.enemy_engaging = 1;
	s.under_fire = 1;
	s.gun_loaded = 1;
	s.enemy_distance = 3.0f;
	s.gun_range = 8.0f;
	s.teammate_distance = -1.0f;
	s.goal_distance = 20.0f;
	s.cover_distance = 15.0f;
	return s;
}

static void test_carrier_fight(void)
{
	struct ae_bots_carrier_situation s = carrier(AE_BOTS_MODE_ODDBALL, 3);

	CHECK(ae_bots_carrier_choice(&s) == AE_BOTS_CARRIER_DROP);
	/* no gun, no engagement, a close teammate, or established escorted cover keeps the scoring weapon */
	s.gun_loaded = 0;
	CHECK(ae_bots_carrier_choice(&s) == AE_BOTS_CARRIER_KEEP);
	s.gun_loaded = 1;
	s.enemy_engaging = 0;
	CHECK(ae_bots_carrier_choice(&s) == AE_BOTS_CARRIER_KEEP);
	s.enemy_engaging = 1;
	s.teammate_distance = 6.0f;
	CHECK(ae_bots_carrier_choice(&s) == AE_BOTS_CARRIER_KEEP);
	s.teammate_distance = 10.0f;
	s.in_cover = 1;
	CHECK(ae_bots_carrier_choice(&s) == AE_BOTS_CARRIER_KEEP);
	s.in_cover = 0;
	s.enemy_distance = 16.0f;
	CHECK(ae_bots_carrier_choice(&s) == AE_BOTS_CARRIER_KEEP);
	/* skill: recruit never drops; easy only drops after a hit and later than heroic */
	s = carrier(AE_BOTS_MODE_ODDBALL, 0);
	CHECK(ae_bots_carrier_choice(&s) == AE_BOTS_CARRIER_KEEP);
	s.skill_level = 1;
	s.enemy_distance = 4.0f;
	CHECK(ae_bots_carrier_choice(&s) == AE_BOTS_CARRIER_KEEP);
	s.enemy_distance = 3.0f;
	s.under_fire = 0;
	CHECK(ae_bots_carrier_choice(&s) == AE_BOTS_CARRIER_KEEP);
	s.under_fire = 1;
	CHECK(ae_bots_carrier_choice(&s) == AE_BOTS_CARRIER_DROP);
	/* SnD plants and its last living attacker keep the bomb; ordinary Bomb never uses this rule */
	s = carrier(AE_BOTS_MODE_BOMB, 4);
	s.goal_distance = 4.0f;
	CHECK(ae_bots_carrier_choice(&s) == AE_BOTS_CARRIER_KEEP);
	s.goal_distance = 5.0f;
	CHECK(ae_bots_carrier_choice(&s) == AE_BOTS_CARRIER_DROP);
	/* a teammate at the shoulder keeps it; one 6 units off does not */
	s.goal_distance = 20.0f;
	s.teammate_distance = 3.0f;
	CHECK(ae_bots_carrier_choice(&s) == AE_BOTS_CARRIER_KEEP);
	s.teammate_distance = 6.0f;
	CHECK(ae_bots_carrier_choice(&s) == AE_BOTS_CARRIER_DROP);
	s.teammate_distance = -1.0f;
	s.last_alive = 1;
	CHECK(ae_bots_carrier_choice(&s) == AE_BOTS_CARRIER_KEEP);
	s.last_alive = 0;
	CHECK(ae_bots_carrier_choice(&s) == AE_BOTS_CARRIER_DROP);
	s.bomb_snd = 0;
	CHECK(ae_bots_carrier_choice(&s) == AE_BOTS_CARRIER_KEEP);
	/* a drop is held through the fight and a quiet interval, then recovered; pickup cooldown prevents flip-flop */
	s = carrier(AE_BOTS_MODE_ODDBALL, 3);
	s.carrying = 0;
	s.dropped = 1;
	CHECK(ae_bots_carrier_choice(&s) == AE_BOTS_CARRIER_WAIT);
	s.enemy_engaging = 0;
	s.clear_seconds = 0.9f;
	CHECK(ae_bots_carrier_choice(&s) == AE_BOTS_CARRIER_WAIT);
	s.clear_seconds = 1.0f;
	CHECK(ae_bots_carrier_choice(&s) == AE_BOTS_CARRIER_RECOVER);
	s = carrier(AE_BOTS_MODE_ODDBALL, 5);
	s.drop_cooldown_seconds = 0.1f;
	CHECK(ae_bots_carrier_choice(&s) == AE_BOTS_CARRIER_KEEP);
}

static void test_wingman(void)
{
	struct ae_bots_wingman_candidate bots[4];

	memset(bots, 0, sizeof(bots));
	bots[0].eligible = bots[1].eligible = bots[2].eligible = 1;
	bots[0].distance = 12.0f;
	bots[1].distance = 8.0f;
	bots[2].distance = 10.0f;
	/* the nearest free living bot takes the role; its two-unit margin prevents small swaps */
	CHECK(ae_bots_wingman_pick(bots, 4, -1, 2.0f) == 1);
	CHECK(ae_bots_wingman_pick(bots, 4, 2, 2.0f) == 2);
	bots[1].distance = 7.9f;
	CHECK(ae_bots_wingman_pick(bots, 4, 2, 2.0f) == 1);
	/* a carrier, dead bot or occupied bot is ineligible; a vanished current role is replaced */
	bots[1].eligible = 0;
	CHECK(ae_bots_wingman_pick(bots, 4, 1, 2.0f) == 2);
	bots[0].eligible = bots[2].eligible = 0;
	CHECK(ae_bots_wingman_pick(bots, 4, 2, 2.0f) == -1);
}

static void test_recovery(void)
{
	struct ae_bots_recovery_situation s;

	memset(&s, 0, sizeof(s));
	s.dropper_alive = 1;
	s.remembered_free = 1;
	s.goal_valid = 1;
	s.clear_seconds = 1.0f;
	CHECK(ae_bots_recovery_choice(&s) == AE_BOTS_RECOVERY_GO);
	s.enemy_engaging = 1;
	CHECK(ae_bots_recovery_choice(&s) == AE_BOTS_RECOVERY_WAIT);
	/* a reset free objective is adopted; a team may hand a dead dropper's recovery to a nearby bot */
	s.enemy_engaging = 0;
	s.remembered_free = 0;
	s.replacement_free = 1;
	CHECK(ae_bots_recovery_choice(&s) == AE_BOTS_RECOVERY_RESET);
	s.dropper_alive = 0;
	s.team_game = 1;
	s.near_teammate = 1;
	CHECK(ae_bots_recovery_choice(&s) == AE_BOTS_RECOVERY_HANDOFF);
	/* every terminal loss has a countable abandonment reason */
	s.near_teammate = 0;
	CHECK(ae_bots_recovery_choice(&s) == AE_BOTS_RECOVERY_ABANDON_DEAD);
	s.dropper_alive = 1;
	s.replacement_free = 0;
	s.objective_carried = 1;
	CHECK(ae_bots_recovery_choice(&s) == AE_BOTS_RECOVERY_ABANDON_TAKEN);
	s.objective_carried = 0;
	s.missing_seconds = 1.9f;
	CHECK(ae_bots_recovery_choice(&s) == AE_BOTS_RECOVERY_WAIT);
	s.missing_seconds = 2.0f;
	CHECK(ae_bots_recovery_choice(&s) == AE_BOTS_RECOVERY_ABANDON_GONE);
	s.remembered_free = 1;
	s.goal_valid = 0;
	CHECK(ae_bots_recovery_choice(&s) == AE_BOTS_RECOVERY_ABANDON_NO_GOAL);
}

static void test_snd_roles(void)
{
	int roles[8], slot, anchors, supports;

	/* four defenders: an anchor, holders and one forward player; a death promotes */
	ae_bots_snd_roles(0, 4, 0xfUL, 0, roles);
	CHECK(roles[0] == AE_BOTS_SND_ROLE_ANCHOR && roles[2] == AE_BOTS_SND_ROLE_FORWARD);
	ae_bots_snd_roles(0, 4, 0xeUL, 0, roles);
	CHECK(roles[0] == AE_BOTS_SND_ROLE_NONE && roles[1] == AE_BOTS_SND_ROLE_ANCHOR);
	/* under three living, no forward player */
	ae_bots_snd_roles(0, 4, 0x5UL, 0, roles);
	CHECK(roles[0] == AE_BOTS_SND_ROLE_ANCHOR && roles[2] == AE_BOTS_SND_ROLE_HOLDER);
	/* the rounds turn the roles: nobody is the same role every round */
	ae_bots_snd_roles(0, 4, 0xfUL, 1, roles);
	CHECK(roles[0] != AE_BOTS_SND_ROLE_ANCHOR);
	for (slot = 0, anchors = 0; slot < 4; slot++)
		anchors += roles[slot] == AE_BOTS_SND_ROLE_ANCHOR;
	CHECK(anchors == 1);
	/* attackers: entry, support, lurker; a lone survivor is an entry; the lurker is promoted to support */
	ae_bots_snd_roles(1, 3, 0x7UL, 0, roles);
	CHECK(roles[0] == AE_BOTS_SND_ROLE_ENTRY && roles[1] == AE_BOTS_SND_ROLE_SUPPORT && roles[2] == AE_BOTS_SND_ROLE_LURKER);
	ae_bots_snd_roles(1, 3, 0x5UL, 0, roles);
	CHECK(roles[0] == AE_BOTS_SND_ROLE_ENTRY && roles[2] == AE_BOTS_SND_ROLE_SUPPORT);
	ae_bots_snd_roles(1, 3, 0x4UL, 0, roles);
	CHECK(roles[2] == AE_BOTS_SND_ROLE_ENTRY);
	/* eight attackers always keep a support, and every living bot has a role */
	ae_bots_snd_roles(1, 8, 0xffUL, 3, roles);
	for (slot = 0, supports = 0; slot < 8; slot++)
		supports += roles[slot] == AE_BOTS_SND_ROLE_SUPPORT;
	CHECK(supports >= 1);
	ae_bots_snd_roles(1, 8, 0x80UL, 3, roles);
	CHECK(roles[7] == AE_BOTS_SND_ROLE_ENTRY && roles[0] == AE_BOTS_SND_ROLE_NONE);
	CHECK(strcmp(ae_bots_snd_role_name(AE_BOTS_SND_ROLE_LURKER), "lurker") == 0);
}

static void test_snd_positions(void)
{
	test_snd_roles();
	/* Points make progress down different sides; a stacked or nearly straight point is rejected. */
	CHECK(ae_bots_lane_score(0.0f, 0.0f, 30.0f, 0.0f, 8.0f, 5.0f, 1, 100.0f) >
		ae_bots_lane_score(0.0f, 0.0f, 30.0f, 0.0f, 8.0f, 5.0f, -1, 100.0f));
	CHECK(ae_bots_lane_score(0.0f, 0.0f, 30.0f, 0.0f, 8.0f, -5.0f, -1, 100.0f) > 0.0f);
	CHECK(ae_bots_lane_score(0.0f, 0.0f, 30.0f, 0.0f, 8.0f, 0.5f, 0, 100.0f) < 0.0f);
	CHECK(ae_bots_lane_score(0.0f, 0.0f, 30.0f, 0.0f, 8.0f, 5.0f, 1, 4.0f) < 0.0f);
}

static void test_roles_and_names(void)
{
	int behaviour;

	CHECK(ae_bots_ctf_defenders_with_human(8) == 1);
	CHECK(ae_bots_ctf_defenders_with_human(4) == 1);
	CHECK(ae_bots_ctf_defenders_with_human(9) == 2);
	CHECK(ae_bots_ctf_defenders(0) == 0);
	CHECK(ae_bots_ctf_defenders(2) == 0);
	CHECK(ae_bots_ctf_defenders(3) == 1);
	CHECK(ae_bots_ctf_defenders(5) == 1);
	CHECK(ae_bots_ctf_defenders(6) == 2);
	CHECK(ae_bots_ctf_defenders(16) == 2);
	CHECK(ae_bots_objective_runs_while_fighting(AE_BOTS_CTF_CARRY, 0));
	CHECK(ae_bots_objective_runs_while_fighting(AE_BOTS_HILL_HOLD, 0));
	CHECK(!ae_bots_objective_runs_while_fighting(AE_BOTS_FIGHT, 0));
	CHECK(!ae_bots_objective_runs_while_fighting(AE_BOTS_CTF_DEFEND, 0));
	CHECK(!ae_bots_objective_runs_while_fighting(AE_BOTS_JUGG_HUNT, 0));
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
	test_bomb();
	test_bomb_snd();
	test_hysteresis();
	test_carrier_fight();
	test_wingman();
	test_recovery();
	test_snd_positions();
	test_roles_and_names();
	printf("%d checks, %d failures\n", checks, failures);
	return failures ? 1 : 0;
}
