#include <stdio.h>
#include "ae_bots_combat_rules.h"

static int failures;
static int checks;
#define CHECK(c) do { checks++; if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static void test_hash(void)
{
	unsigned long a;
	int low = 0, high = 0;

	for (a = 0; a < 2000; a++)
	{
		float h = ae_bots_combat_hash01(a, 77);

		CHECK(h >= 0.0f && h < 1.0f);
		low += h < 0.5f;
		high += h >= 0.5f;
		/* (the same pair, the same number) */
		CHECK(h == ae_bots_combat_hash01(a, 77));
	}
	/* (spread: about half each side) */
	CHECK(low > 800 && high > 800);
	CHECK(ae_bots_combat_hash01(1, 2) != ae_bots_combat_hash01(2, 1));
}

/* a reaction is never quicker than 0.85 of the skill's, at most 1.35 of it; RECRUIT's never quicker than its own */
static void test_reaction_and_look(void)
{
	CHECK(ae_bots_combat_reaction_ticks(AE_BOTS_LEVEL_NORMAL, 42, 0.0f) == 36);
	CHECK(ae_bots_combat_reaction_ticks(AE_BOTS_LEVEL_NORMAL, 42, 0.999f) == 57);
	CHECK(ae_bots_combat_reaction_ticks(AE_BOTS_LEVEL_NORMAL, 42, -3.0f) == 36);
	CHECK(ae_bots_combat_reaction_ticks(AE_BOTS_LEVEL_NORMAL, 42, 5.0f) == 57);
	CHECK(ae_bots_combat_reaction_ticks(AE_BOTS_LEVEL_RECRUIT, 42, 0.0f) == 42);
	CHECK(ae_bots_combat_reaction_ticks(AE_BOTS_LEVEL_RECRUIT, 42, -3.0f) == 42);
	CHECK(ae_bots_combat_reaction_ticks(AE_BOTS_LEVEL_RECRUIT, 42, 0.999f) == 63);
	CHECK(ae_bots_combat_look_jitter(0.0f) == 0);
	CHECK(ae_bots_combat_look_jitter(0.5f) == 1);
	CHECK(ae_bots_combat_look_jitter(0.99f) == 2);
	CHECK(ae_bots_combat_look_jitter(1.0f) == 2);
}

static void test_confidence(void)
{
	float even = ae_bots_combat_confidence(1, 1, 1, 1, 5, 9, 1, 0);

	CHECK(even > 0.49f && even < 0.51f);
	CHECK(ae_bots_combat_confidence(1, 1, 0, 0.2f, 5, 9, 1, 0) > 0.9f);
	CHECK(ae_bots_combat_confidence(0, 0.3f, 1, 1, 5, 9, 1, 0) < 0.2f);
	/* (an empty magazine, a target out of reach: less sure; from above: more) */
	CHECK(ae_bots_combat_confidence(1, 1, 1, 1, 5, 9, 0.1f, 0) < even - 0.1f);
	CHECK(ae_bots_combat_confidence(1, 1, 1, 1, 20, 9, 1, 0) < even);
	CHECK(ae_bots_combat_confidence(1, 1, 1, 1, 5, 9, 1, 3) > even);
	CHECK(ae_bots_combat_confidence(1, 1, 1, 1, 5, 9, 1, 100) < even + 0.11f);
	/* (always 0..1, an overshield too) */
	CHECK(ae_bots_combat_confidence(3, 1, 0, 0, 1, 9, 1, 9) <= 1.0f);
	CHECK(ae_bots_combat_confidence(0, 0, 3, 1, 50, 9, 0, -9) >= 0.0f);
}

static void test_melee(void)
{
	int level;

	/* waiting, or out of reach: never */
	for (level = 0; level < AE_BOTS_LEVELS; level++)
	{
		CHECK(ae_bots_combat_melee(level, 0.3f, 0, 1, 0, 1, 0.0f) == 0);
		CHECK(ae_bots_combat_melee(level, 1.2f, 1, 1, 0, 1, 0.0f) == 0);
	}
	/* RECRUIT: point blank only, and only now and then */
	CHECK(ae_bots_combat_melee(AE_BOTS_LEVEL_RECRUIT, 0.9f, 1, 1, 0, 0, 0.0f) == 0);
	CHECK(ae_bots_combat_melee(AE_BOTS_LEVEL_RECRUIT, 0.5f, 1, 0, 1, 0, 0.1f) == 1);
	CHECK(ae_bots_combat_melee(AE_BOTS_LEVEL_RECRUIT, 0.5f, 1, 0, 1, 0, 0.5f) == 2);
	/* EASY and NORMAL: within reach */
	CHECK(ae_bots_combat_melee(AE_BOTS_LEVEL_EASY, 1.0f, 1, 0, 1, 0, 0.5f) == 1);
	CHECK(ae_bots_combat_melee(AE_BOTS_LEVEL_EASY, 1.0f, 1, 0, 1, 0, 0.9f) == 2);
	CHECK(ae_bots_combat_melee(AE_BOTS_LEVEL_NORMAL, 1.0f, 1, 0, 1, 0, 0.99f) == 1);
	/* HEROIC and up: the back turned, shields low or closing in; else point blank */
	for (level = AE_BOTS_LEVEL_HEROIC; level <= AE_BOTS_LEVEL_LEGENDARY; level++)
	{
		CHECK(ae_bots_combat_melee(level, 1.0f, 1, 0, 1, 0, 0.0f) == 0);
		CHECK(ae_bots_combat_melee(level, 1.0f, 1, 1, 1, 0, 0.0f) == 1);
		CHECK(ae_bots_combat_melee(level, 1.0f, 1, 0, 0.05f, 0, 0.0f) == 1);
		CHECK(ae_bots_combat_melee(level, 1.0f, 1, 0, 1, 1, 0.0f) == 1);
		CHECK(ae_bots_combat_melee(level, 0.5f, 1, 0, 1, 0, 0.99f) == 1);
	}
	/* (low shields: under 0.1 for HEROIC, 0.4 for LEGENDARY: Infinite's ODST and Spartan) */
	CHECK(ae_bots_combat_melee(AE_BOTS_LEVEL_HEROIC, 1.0f, 1, 0, 0.3f, 0, 0.0f) == 0);
	CHECK(ae_bots_combat_melee(AE_BOTS_LEVEL_LEGENDARY, 1.0f, 1, 0, 0.3f, 0, 0.0f) == 1);
	/* the wait in reach before a blow: RECRUIT 1-3 s, NORMAL 0.1-0.5, HEROIC up 0.1-0.3 */
	CHECK(ae_bots_combat_melee_delay(AE_BOTS_LEVEL_RECRUIT, 0) == 30);
	CHECK(ae_bots_combat_melee_delay(AE_BOTS_LEVEL_RECRUIT, 1) == 90);
	CHECK(ae_bots_combat_melee_delay(AE_BOTS_LEVEL_EASY, 0) == 15);
	CHECK(ae_bots_combat_melee_delay(AE_BOTS_LEVEL_NORMAL, 1) == 15);
	CHECK(ae_bots_combat_melee_delay(AE_BOTS_LEVEL_LEGENDARY, 0) == 3);
	CHECK(ae_bots_combat_melee_delay(AE_BOTS_LEVEL_LEGENDARY, 1) == 9);
	/* cooldowns: RECRUIT waits longest, each a range */
	CHECK(ae_bots_combat_melee_cooldown(AE_BOTS_LEVEL_RECRUIT, 0) == 90);
	CHECK(ae_bots_combat_melee_cooldown(AE_BOTS_LEVEL_RECRUIT, 1) == 135);
	CHECK(ae_bots_combat_melee_cooldown(AE_BOTS_LEVEL_EASY, 0) == 60);
	CHECK(ae_bots_combat_melee_cooldown(AE_BOTS_LEVEL_NORMAL, 0) == 30);
	CHECK(ae_bots_combat_melee_cooldown(AE_BOTS_LEVEL_LEGENDARY, 0) == 27);
	for (level = 1; level < AE_BOTS_LEVELS; level++)
		CHECK(ae_bots_combat_melee_cooldown(AE_BOTS_LEVEL_RECRUIT, 0) >= ae_bots_combat_melee_cooldown(level, 1));
}

/* the chance a second of a throw, from many ticks' randoms */
static float grenade_rate(int level, float distance, int first_contact, int about_to_die, float skill_chance)
{
	int tick, throws = 0;

	for (tick = 0; tick < 30000; tick++)
		throws += ae_bots_combat_grenade(level, distance, 1, 1, first_contact, about_to_die, skill_chance,
			(float)tick / 30000.0f);
	return (float)throws / 1000.0f;
}

static void test_grenades(void)
{
	int level;

	for (level = 0; level < AE_BOTS_LEVELS; level++)
	{
		/* no grenade, not reacted, too near or too far: never */
		CHECK(!ae_bots_combat_grenade(level, 12, 0, 1, 1, 1, 1, 0.0f));
		CHECK(!ae_bots_combat_grenade(level, 12, 1, 0, 1, 1, 1, 0.0f));
		CHECK(!ae_bots_combat_grenade(level, 4, 1, 1, 1, 1, 1, 0.0f));
		CHECK(!ae_bots_combat_grenade(level, 25, 1, 1, 1, 1, 1, 0.0f));
	}
	/* RECRUIT: never, whatever its state */
	CHECK(grenade_rate(AE_BOTS_LEVEL_RECRUIT, 8, 1, 1, 0) == 0.0f);
	CHECK(grenade_rate(AE_BOTS_LEVEL_RECRUIT, 15, 1, 1, 0) == 0.0f);
	/* EASY: about to die, else rarely */
	CHECK(grenade_rate(AE_BOTS_LEVEL_EASY, 12, 0, 0, 0.04f) < 0.03f);
	CHECK(grenade_rate(AE_BOTS_LEVEL_EASY, 12, 0, 1, 0.04f) > 0.5f);
	CHECK(grenade_rate(AE_BOTS_LEVEL_EASY, 12, 1, 0, 0.04f) < 0.03f);
	/* NORMAL: bots.c's rate (its chance three times a second's) */
	CHECK(grenade_rate(AE_BOTS_LEVEL_NORMAL, 12, 0, 0, 0.10f) > 0.29f &&
		grenade_rate(AE_BOTS_LEVEL_NORMAL, 12, 0, 0, 0.10f) < 0.31f);
	CHECK(grenade_rate(AE_BOTS_LEVEL_NORMAL, 12, 1, 1, 0.10f) < 0.31f);
	/* HEROIC and up: first contact and about to die add */
	CHECK(grenade_rate(AE_BOTS_LEVEL_HEROIC, 12, 1, 0, 0.16f) > grenade_rate(AE_BOTS_LEVEL_HEROIC, 12, 0, 0, 0.16f) + 0.4f);
	CHECK(grenade_rate(AE_BOTS_LEVEL_LEGENDARY, 12, 0, 1, 0.22f) > grenade_rate(AE_BOTS_LEVEL_LEGENDARY, 12, 0, 0, 0.22f) + 0.5f);
	CHECK(ae_bots_combat_grenade_cooldown(AE_BOTS_LEVEL_RECRUIT, 0) == 600);
	CHECK(ae_bots_combat_grenade_cooldown(AE_BOTS_LEVEL_EASY, 1) == 315);
	CHECK(ae_bots_combat_grenade_cooldown(AE_BOTS_LEVEL_NORMAL, 0) == 30);
	CHECK(ae_bots_combat_grenade_cooldown(AE_BOTS_LEVEL_LEGENDARY, 0) == 3);
	CHECK(ae_bots_combat_grenade_cooldown(AE_BOTS_LEVEL_LEGENDARY, 1) == 153);
	CHECK(!ae_bots_combat_grenade(AE_BOTS_LEVEL_LEGENDARY, 19, 1, 1, 1, 1, 1, 0.0f));
	CHECK(ae_bots_combat_grenade_lead(AE_BOTS_LEVEL_RECRUIT) == 0.0f);
	CHECK(ae_bots_combat_grenade_lead(AE_BOTS_LEVEL_EASY) == 0.0f);
	CHECK(ae_bots_combat_grenade_lead(AE_BOTS_LEVEL_NORMAL) == 0.0f);
	CHECK(ae_bots_combat_grenade_lead(AE_BOTS_LEVEL_HEROIC) == 1.0f);
	CHECK(ae_bots_combat_grenade_lead(AE_BOTS_LEVEL_LEGENDARY) == 1.0f);
	CHECK(ae_bots_combat_about_to_die(0, 0.5f));
	CHECK(!ae_bots_combat_about_to_die(0.1f, 0.5f));
	CHECK(!ae_bots_combat_about_to_die(0, 0.7f));
}

static void test_aim_head(void)
{
	CHECK(!ae_bots_combat_aim_head(AE_BOTS_LEVEL_RECRUIT, 0, 1));
	CHECK(!ae_bots_combat_aim_head(AE_BOTS_LEVEL_EASY, 0, 1));
	CHECK(ae_bots_combat_aim_head(AE_BOTS_LEVEL_NORMAL, 0, 1));
	CHECK(ae_bots_combat_aim_head(AE_BOTS_LEVEL_LEGENDARY, 0, 1));
	CHECK(!ae_bots_combat_aim_head(AE_BOTS_LEVEL_LEGENDARY, 0.1f, 1));
	CHECK(!ae_bots_combat_aim_head(AE_BOTS_LEVEL_LEGENDARY, 0, 0));
}

static void test_retreat(void)
{
	struct ae_bots_combat_retreat state;
	int started, ended, level;
	long now;

	/* RECRUIT and EASY never */
	for (level = AE_BOTS_LEVEL_RECRUIT; level <= AE_BOTS_LEVEL_EASY; level++)
	{
		ae_bots_combat_retreat_reset(&state);
		for (now = 0; now < 300; now++)
			CHECK(!ae_bots_combat_retreat_step(&state, level, 0.0f, 0.0f, now, 0.5f, &started, &ended));
		CHECK(ae_bots_combat_retreat_threshold(level) == 0.0f);
	}
	/* NORMAL: low (under 0.1), after its hesitation (6 + 12 x 0.5 = 12 ticks) */
	ae_bots_combat_retreat_reset(&state);
	CHECK(!ae_bots_combat_retreat_step(&state, AE_BOTS_LEVEL_NORMAL, 0.1f, 0.1f, 99, 0.5f, &started, &ended));
	for (now = 100; now < 112; now++)
		CHECK(!ae_bots_combat_retreat_step(&state, AE_BOTS_LEVEL_NORMAL, 0.05f, 0.1f, now, 0.5f, &started, &ended));
	CHECK(ae_bots_combat_retreat_step(&state, AE_BOTS_LEVEL_NORMAL, 0.05f, 0.1f, 112, 0.5f, &started, &ended));
	CHECK(started && !ended);
	/* (at least 2 s, though its shields are back) */
	CHECK(ae_bots_combat_retreat_step(&state, AE_BOTS_LEVEL_NORMAL, 0.9f, 1.0f, 171, 0.5f, &started, &ended));
	CHECK(!started && !ended);
	CHECK(!ae_bots_combat_retreat_step(&state, AE_BOTS_LEVEL_NORMAL, 0.3f, 1.0f, 172, 0.5f, &started, &ended));
	CHECK(ended);
	/* (then not again for 6 s) */
	for (now = 173; now < 172 + 180; now++)
		CHECK(!ae_bots_combat_retreat_step(&state, AE_BOTS_LEVEL_NORMAL, 0.0f, 0.0f, now, 0.0f, &started, &ended));
	for (now = 352; now < 358; now++)
		CHECK(!ae_bots_combat_retreat_step(&state, AE_BOTS_LEVEL_NORMAL, 0.0f, 0.0f, now, 0.0f, &started, &ended));
	CHECK(ae_bots_combat_retreat_step(&state, AE_BOTS_LEVEL_NORMAL, 0.0f, 0.0f, 358, 0.0f, &started, &ended));
	/* (at most 7 s) */
	CHECK(ae_bots_combat_retreat_step(&state, AE_BOTS_LEVEL_NORMAL, 0.0f, 0.0f, 358 + 209, 0.0f, &started, &ended));
	CHECK(!ae_bots_combat_retreat_step(&state, AE_BOTS_LEVEL_NORMAL, 0.0f, 0.0f, 358 + 210, 0.0f, &started, &ended));
	CHECK(ended);
	/* shields up, or sure enough: no retreat; a confidence that comes back starts the hesitation over */
	ae_bots_combat_retreat_reset(&state);
	for (now = 0; now < 100; now++)
		CHECK(!ae_bots_combat_retreat_step(&state, AE_BOTS_LEVEL_LEGENDARY, 0.0f, 0.5f, now, 0.0f, &started, &ended));
	for (now = 0; now < 100; now++)
		CHECK(!ae_bots_combat_retreat_step(&state, AE_BOTS_LEVEL_LEGENDARY, 0.2f, 0.0f, now, 0.0f, &started, &ended));
	for (now = 100; now < 105; now++)
		CHECK(!ae_bots_combat_retreat_step(&state, AE_BOTS_LEVEL_LEGENDARY, 0.05f, 0.0f, now, 0.0f, &started, &ended));
	CHECK(!ae_bots_combat_retreat_step(&state, AE_BOTS_LEVEL_LEGENDARY, 0.9f, 0.0f, 105, 0.0f, &started, &ended));
	CHECK(!ae_bots_combat_retreat_step(&state, AE_BOTS_LEVEL_LEGENDARY, 0.05f, 0.0f, 106, 0.0f, &started, &ended));
	CHECK(!ae_bots_combat_retreat_step(&state, AE_BOTS_LEVEL_LEGENDARY, 0.05f, 0.0f, 111, 0.0f, &started, &ended));
	CHECK(ae_bots_combat_retreat_step(&state, AE_BOTS_LEVEL_LEGENDARY, 0.05f, 0.0f, 112, 0.0f, &started, &ended));
	/* (HEROIC and up: at least 3 s) */
	CHECK(ae_bots_combat_retreat_step(&state, AE_BOTS_LEVEL_LEGENDARY, 0.9f, 1.0f, 112 + 89, 0.0f, &started, &ended));
	CHECK(!ae_bots_combat_retreat_step(&state, AE_BOTS_LEVEL_LEGENDARY, 0.9f, 1.0f, 112 + 90, 0.0f, &started, &ended));
	CHECK(ae_bots_combat_retreat_threshold(AE_BOTS_LEVEL_NORMAL) == 0.1f);
}

static void test_charge_jump_share(void)
{
	CHECK(!ae_bots_combat_charge(AE_BOTS_LEVEL_NORMAL, 0.9f, 0, 0.3f, 3));
	CHECK(ae_bots_combat_charge(AE_BOTS_LEVEL_HEROIC, 0.9f, 0, 0.3f, 3));
	CHECK(!ae_bots_combat_charge(AE_BOTS_LEVEL_HEROIC, 0.5f, 0, 0.3f, 3));
	CHECK(!ae_bots_combat_charge(AE_BOTS_LEVEL_HEROIC, 0.9f, 0.2f, 0.3f, 3));
	CHECK(!ae_bots_combat_charge(AE_BOTS_LEVEL_HEROIC, 0.9f, 0, 0.8f, 3));
	CHECK(!ae_bots_combat_charge(AE_BOTS_LEVEL_LEGENDARY, 0.9f, 0, 0.3f, 7));
	CHECK(!ae_bots_combat_charge(AE_BOTS_LEVEL_LEGENDARY, 0.9f, 0, 0.3f, 0.5f));
	/* jumps in a fight: NORMAL up (3-5 s, HEROIC 1-4, LEGENDARY 1-3) */
	CHECK(ae_bots_combat_vertical_delay(AE_BOTS_LEVEL_RECRUIT, 0.5f) == 0);
	CHECK(ae_bots_combat_vertical_delay(AE_BOTS_LEVEL_EASY, 0.5f) == 0);
	CHECK(ae_bots_combat_vertical_delay(AE_BOTS_LEVEL_NORMAL, 0) == 90);
	CHECK(ae_bots_combat_vertical_delay(AE_BOTS_LEVEL_NORMAL, 1) == 150);
	CHECK(ae_bots_combat_vertical_delay(AE_BOTS_LEVEL_HEROIC, 1) == 120);
	CHECK(ae_bots_combat_vertical_delay(AE_BOTS_LEVEL_LEGENDARY, 0) == 30);
	/* strafe segments and halts: RECRUIT mostly stands, LEGENDARY jiggles */
	CHECK(ae_bots_combat_strafe_ticks(AE_BOTS_LEVEL_RECRUIT, 0) == 30 && ae_bots_combat_strafe_ticks(AE_BOTS_LEVEL_RECRUIT, 1) == 120);
	CHECK(ae_bots_combat_strafe_ticks(AE_BOTS_LEVEL_NORMAL, 0) == 60 && ae_bots_combat_strafe_ticks(AE_BOTS_LEVEL_NORMAL, 1) == 120);
	CHECK(ae_bots_combat_strafe_ticks(AE_BOTS_LEVEL_LEGENDARY, 0) == 6 && ae_bots_combat_strafe_ticks(AE_BOTS_LEVEL_LEGENDARY, 1) == 24);
	CHECK(ae_bots_combat_halt_ticks(AE_BOTS_LEVEL_RECRUIT, 0.0f, 0) == 0);
	CHECK(ae_bots_combat_halt_ticks(AE_BOTS_LEVEL_EASY, 0.0f, 1) == 0);
	CHECK(ae_bots_combat_halt_ticks(AE_BOTS_LEVEL_NORMAL, 0.1f, 1) == 60);
	CHECK(ae_bots_combat_halt_ticks(AE_BOTS_LEVEL_NORMAL, 0.3f, 1) == 0);
	CHECK(ae_bots_combat_halt_ticks(AE_BOTS_LEVEL_LEGENDARY, 0.04f, 1) == 3);
	CHECK(ae_bots_combat_halt_ticks(AE_BOTS_LEVEL_LEGENDARY, 0.06f, 1) == 0);
	CHECK(ae_bots_combat_initial_halt_ticks(AE_BOTS_LEVEL_RECRUIT, 1) == 0);
	CHECK(ae_bots_combat_initial_halt_ticks(AE_BOTS_LEVEL_EASY, 1) == 0);
	CHECK(ae_bots_combat_initial_halt_ticks(AE_BOTS_LEVEL_NORMAL, 1) == 6);
	/* shared sightings: none below HEROIC, 10 units at HEROIC, anywhere at LEGENDARY */
	CHECK(ae_bots_combat_share_distance(AE_BOTS_LEVEL_RECRUIT) == 0.0f);
	CHECK(ae_bots_combat_share_distance(AE_BOTS_LEVEL_NORMAL) == 0.0f);
	CHECK(ae_bots_combat_share_distance(AE_BOTS_LEVEL_HEROIC) == 10.0f);
	CHECK(ae_bots_combat_share_distance(AE_BOTS_LEVEL_LEGENDARY) > 1000.0f);
}

int main(void)
{
	test_hash();
	test_reaction_and_look();
	test_confidence();
	test_melee();
	test_grenades();
	test_aim_head();
	test_retreat();
	test_charge_jump_share();
	printf("%d checks, %d failures\n", checks, failures);
	return failures ? 1 : 0;
}
