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
	/* (low shields: under 0.1 for HEROIC, 0.4 for LEGENDARY) */
	CHECK(ae_bots_combat_melee(AE_BOTS_LEVEL_HEROIC, 1.0f, 1, 0, 0.3f, 0, 0.0f) == 0);
	CHECK(ae_bots_combat_melee(AE_BOTS_LEVEL_LEGENDARY, 1.0f, 1, 0, 0.3f, 0, 0.0f) == 1);
	/* the wait in reach before a blow: RECRUIT 1-3 s, EASY 0.6-1.5, NORMAL 0.35-0.7, HEROIC 0.25-0.5, LEGENDARY
	0.18-0.35, DEMON 0.1-0.3: each level's quickest and slowest quicker than the level below's */
	CHECK(ae_bots_combat_melee_delay(AE_BOTS_LEVEL_RECRUIT, 0) == 30);
	CHECK(ae_bots_combat_melee_delay(AE_BOTS_LEVEL_RECRUIT, 1) == 90);
	CHECK(ae_bots_combat_melee_delay(AE_BOTS_LEVEL_EASY, 0) == 18);
	CHECK(ae_bots_combat_melee_delay(AE_BOTS_LEVEL_NORMAL, 0) >= 10 && ae_bots_combat_melee_delay(AE_BOTS_LEVEL_NORMAL, 0) <= 11);
	CHECK(ae_bots_combat_melee_delay(AE_BOTS_LEVEL_NORMAL, 1) == 21);
	CHECK(ae_bots_combat_melee_delay(AE_BOTS_LEVEL_DEMON, 0) == 3);
	CHECK(ae_bots_combat_melee_delay(AE_BOTS_LEVEL_DEMON, 1) == 9);
	for (level = AE_BOTS_LEVEL_EASY; level < AE_BOTS_LEVELS; level++)
	{
		CHECK(ae_bots_combat_melee_delay(level, 0) < ae_bots_combat_melee_delay(level - 1, 0));
		CHECK(ae_bots_combat_melee_delay(level, 1) < ae_bots_combat_melee_delay(level - 1, 1));
	}
	/* cooldowns: RECRUIT waits longest, each a range */
	CHECK(ae_bots_combat_melee_cooldown(AE_BOTS_LEVEL_RECRUIT, 0) == 90);
	CHECK(ae_bots_combat_melee_cooldown(AE_BOTS_LEVEL_RECRUIT, 1) == 135);
	CHECK(ae_bots_combat_melee_cooldown(AE_BOTS_LEVEL_EASY, 0) == 60);
	CHECK(ae_bots_combat_melee_cooldown(AE_BOTS_LEVEL_NORMAL, 0) == 36);
	CHECK(ae_bots_combat_melee_cooldown(AE_BOTS_LEVEL_HEROIC, 0) == 30);
	CHECK(ae_bots_combat_melee_cooldown(AE_BOTS_LEVEL_DEMON, 0) == 27);
	for (level = AE_BOTS_LEVEL_EASY; level < AE_BOTS_LEVELS; level++)
		CHECK(ae_bots_combat_melee_cooldown(level, 0) < ae_bots_combat_melee_cooldown(level - 1, 0));
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
		CHECK(!ae_bots_combat_retreat_step(&state, AE_BOTS_LEVEL_HEROIC, 0.0f, 0.5f, now, 0.0f, &started, &ended));
	for (now = 0; now < 100; now++)
		CHECK(!ae_bots_combat_retreat_step(&state, AE_BOTS_LEVEL_HEROIC, 0.2f, 0.0f, now, 0.0f, &started, &ended));
	for (now = 100; now < 105; now++)
		CHECK(!ae_bots_combat_retreat_step(&state, AE_BOTS_LEVEL_HEROIC, 0.05f, 0.0f, now, 0.0f, &started, &ended));
	CHECK(!ae_bots_combat_retreat_step(&state, AE_BOTS_LEVEL_HEROIC, 0.9f, 0.0f, 105, 0.0f, &started, &ended));
	CHECK(!ae_bots_combat_retreat_step(&state, AE_BOTS_LEVEL_HEROIC, 0.05f, 0.0f, 106, 0.0f, &started, &ended));
	CHECK(!ae_bots_combat_retreat_step(&state, AE_BOTS_LEVEL_HEROIC, 0.05f, 0.0f, 111, 0.0f, &started, &ended));
	CHECK(ae_bots_combat_retreat_step(&state, AE_BOTS_LEVEL_HEROIC, 0.05f, 0.0f, 112, 0.0f, &started, &ended));
	/* (HEROIC and up: at least 3 s) */
	CHECK(ae_bots_combat_retreat_step(&state, AE_BOTS_LEVEL_HEROIC, 0.9f, 1.0f, 112 + 89, 0.0f, &started, &ended));
	CHECK(!ae_bots_combat_retreat_step(&state, AE_BOTS_LEVEL_HEROIC, 0.9f, 1.0f, 112 + 90, 0.0f, &started, &ended));
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
	/* shared sightings: none below HEROIC, 10 units at HEROIC, 20 at LEGENDARY, anywhere at DEMON */
	CHECK(ae_bots_combat_share_distance(AE_BOTS_LEVEL_RECRUIT) == 0.0f);
	CHECK(ae_bots_combat_share_distance(AE_BOTS_LEVEL_NORMAL) == 0.0f);
	CHECK(ae_bots_combat_share_distance(AE_BOTS_LEVEL_HEROIC) == 10.0f);
	CHECK(ae_bots_combat_share_distance(AE_BOTS_LEVEL_LEGENDARY) == 20.0f);
	CHECK(ae_bots_combat_share_distance(AE_BOTS_LEVEL_DEMON) > 1000.0f);
	/* LONG RANGE (owner 10-20: "a bit too good with their distance shots"): no floor within 10 units, a floor that
	grows past it, larger for lower levels, none at DEMON (the old LEGENDARY, kept as it was) */
	CHECK(ae_bots_combat_range_error_floor(AE_BOTS_LEVEL_NORMAL, 9.0f) == 0.0f);
	CHECK(ae_bots_combat_range_error_floor(AE_BOTS_LEVEL_LEGENDARY, 15.0f) > 0.5f);
	CHECK(ae_bots_combat_range_error_floor(AE_BOTS_LEVEL_LEGENDARY, 15.0f) < 0.7f);
	CHECK(ae_bots_combat_range_error_floor(AE_BOTS_LEVEL_LEGENDARY, 20.0f) == 0.0f);
	CHECK(ae_bots_combat_range_error_floor(AE_BOTS_LEVEL_DEMON, 30.0f) == 0.0f);
	for (int level = AE_BOTS_LEVEL_EASY; level < AE_BOTS_LEVEL_DEMON; level++)
	{
		CHECK(ae_bots_combat_range_error_floor(level, 12.0f) < ae_bots_combat_range_error_floor(level, 15.0f));
		CHECK(level == AE_BOTS_LEVEL_EASY ||
			ae_bots_combat_range_error_floor(level, 15.0f) < ae_bots_combat_range_error_floor(level - 1, 15.0f));
	}
	/* FAR SHOTS (owner 10-30): from 15 units; falling with distance, rising with level, under 25%; DEMON aims as it is */
	CHECK(ae_bots_combat_long_range_hit_chance(AE_BOTS_LEVEL_HEROIC, 15.0f) < 0.0f);
	CHECK(ae_bots_combat_long_range_hit_chance(AE_BOTS_LEVEL_HEROIC, 18.0f) == 0.18f);
	CHECK(ae_bots_combat_long_range_hit_chance(AE_BOTS_LEVEL_HEROIC, 28.0f) == 0.09f);
	CHECK(ae_bots_combat_long_range_hit_chance(AE_BOTS_LEVEL_DEMON, 25.0f) < 0.0f);
	for (int level = AE_BOTS_LEVEL_RECRUIT; level < AE_BOTS_LEVEL_DEMON; level++)
	{
		CHECK(ae_bots_combat_long_range_hit_chance(level, 18.0f) < 0.25f);
		CHECK(ae_bots_combat_long_range_hit_chance(level, 40.0f) < ae_bots_combat_long_range_hit_chance(level, 18.0f));
		CHECK(level == AE_BOTS_LEVEL_RECRUIT ||
			ae_bots_combat_long_range_hit_chance(level, 22.0f) > ae_bots_combat_long_range_hit_chance(level - 1, 22.0f));
	}
	/* SQUADS THAT SPLIT (owner 10-30): small squads as ever; about a third of a wide squad flanks, at most 6 */
	CHECK(ae_bots_combat_squad_flankers(AE_BOTS_LEVEL_NORMAL, 3) == 1);
	CHECK(ae_bots_combat_squad_flankers(AE_BOTS_LEVEL_HEROIC, 3) == 1);
	CHECK(ae_bots_combat_squad_flankers(AE_BOTS_LEVEL_LEGENDARY, 3) == 2);
	CHECK(ae_bots_combat_squad_flankers(AE_BOTS_LEVEL_NORMAL, 8) == 3);
	CHECK(ae_bots_combat_squad_flankers(AE_BOTS_LEVEL_HEROIC, 16) == 5);
	CHECK(ae_bots_combat_squad_flankers(AE_BOTS_LEVEL_HEROIC, 32) == 6);
	CHECK(ae_bots_combat_flank_seconds(3, 40.0f) == 10.0f);
	CHECK(ae_bots_combat_flank_seconds(8, 2.0f) == 10.0f);
	CHECK(ae_bots_combat_flank_seconds(8, 22.0f) == 15.0f);
	CHECK(ae_bots_combat_flank_seconds(8, 200.0f) == 25.0f);
	CHECK(ae_bots_combat_flank_unseen_seconds(3) == 2.0f);
	CHECK(ae_bots_combat_flank_unseen_seconds(4) == 8.0f);
	/* CLOSE RANGE (owner 10-29): NORMAL only, between 2 and 10 units */
	CHECK(ae_bots_combat_near_error_floor(AE_BOTS_LEVEL_NORMAL, 5.0f) == AE_BOTS_COMBAT_NEAR_FLOOR);
	CHECK(ae_bots_combat_near_error_floor(AE_BOTS_LEVEL_NORMAL, 1.0f) == 0.0f);
	CHECK(ae_bots_combat_near_error_floor(AE_BOTS_LEVEL_NORMAL, 12.0f) == 0.0f);
	CHECK(ae_bots_combat_near_error_floor(AE_BOTS_LEVEL_EASY, 5.0f) == 0.0f);
	CHECK(ae_bots_combat_near_error_floor(AE_BOTS_LEVEL_HEROIC, 5.0f) == 0.0f);
	CHECK(ae_bots_combat_near_error_floor(AE_BOTS_LEVEL_LEGENDARY, 5.0f) == 0.0f);
	/* WEAPON RANGE (corpus: shotgun shots from 5-10 units): a shotgun holds fire past 5 units, a flamethrower past 4;
	others have no limit */
	CHECK(ae_bots_combat_max_fire_distance("weapons\\shotgun\\shotgun") == 5.0f);
	CHECK(ae_bots_combat_max_fire_distance("weapons\\flamethrower\\flamethrower") == 4.0f);
	CHECK(ae_bots_combat_max_fire_distance("weapons\\pistol\\pistol") > 1000.0f);
	CHECK(ae_bots_combat_max_fire_distance(0) > 1000.0f);
	/* CAMO (owner 10-20: "bots notice you instantly when you are using camo"): seen at 0.4 of bots.c's camo reach,
	twice the reaction and the first aim error */
	CHECK(ae_bots_combat_camo_reach_scale(1) == 0.4f && ae_bots_combat_camo_reach_scale(0) == 1.0f);
	CHECK(ae_bots_combat_camo_error_scale(1) == 2.0f && ae_bots_combat_camo_error_scale(0) == 1.0f);
	CHECK(ae_bots_combat_camo_reaction_scale(1) == 2.0f && ae_bots_combat_camo_reaction_scale(0) == 1.0f);
	/* DEMON (owner 10-20: the old LEGENDARY) fights as LEGENDARY does in every other rule */
	CHECK(ae_bots_combat_retreat_threshold(AE_BOTS_LEVEL_DEMON) == ae_bots_combat_retreat_threshold(AE_BOTS_LEVEL_LEGENDARY));
	CHECK(ae_bots_combat_vertical_delay(AE_BOTS_LEVEL_DEMON, 0.5f) == ae_bots_combat_vertical_delay(AE_BOTS_LEVEL_LEGENDARY, 0.5f));
	CHECK(ae_bots_combat_initial_halt_ticks(AE_BOTS_LEVEL_DEMON, 1) == ae_bots_combat_initial_halt_ticks(AE_BOTS_LEVEL_LEGENDARY, 1));
	CHECK(ae_bots_combat_halt_ticks(AE_BOTS_LEVEL_DEMON, 0.0f, 1) == ae_bots_combat_halt_ticks(AE_BOTS_LEVEL_LEGENDARY, 0.0f, 1));
	CHECK(ae_bots_combat_strafe_ticks(AE_BOTS_LEVEL_DEMON, 0.5f) == ae_bots_combat_strafe_ticks(AE_BOTS_LEVEL_LEGENDARY, 0.5f));
	CHECK(ae_bots_combat_melee_cooldown(AE_BOTS_LEVEL_DEMON, 0) < ae_bots_combat_melee_cooldown(AE_BOTS_LEVEL_LEGENDARY, 0)); /* (melee ladder, 2026-10-20) */
	CHECK(ae_bots_combat_grenade_cooldown(AE_BOTS_LEVEL_DEMON, 0) == ae_bots_combat_grenade_cooldown(AE_BOTS_LEVEL_LEGENDARY, 0));
	CHECK(ae_bots_combat_grenade_lead(AE_BOTS_LEVEL_DEMON) == 1.0f);
	CHECK(ae_bots_combat_charge(AE_BOTS_LEVEL_DEMON, 0.9f, 0.0f, 0.2f, 2.0f) ==
		ae_bots_combat_charge(AE_BOTS_LEVEL_LEGENDARY, 0.9f, 0.0f, 0.2f, 2.0f));
}

/* SPENT WEAPONS: a weapon of rounds is left to bots.c; the stock plasma pistol (a magazine, every maximum 0) and a
weapon with no magazine are spent only with their battery out */
static void test_weapon_spent(void)
{
	/* (weapons of rounds: loaded or in reserve; then the plasma pistol's magazine of no rounds; no magazine) */
	CHECK(ae_bots_combat_weapon_spent(1, 12, 120, 0.0f) == -1);
	CHECK(ae_bots_combat_weapon_spent(1, 4, 0, 1.0f) == -1);
	CHECK(ae_bots_combat_weapon_spent(1, 0, 24, 0.0f) == -1);
	CHECK(ae_bots_combat_weapon_spent(1, 0, 0, 0.0f) == 0);
	CHECK(ae_bots_combat_weapon_spent(1, 0, 0, 0.96f) == 0);
	CHECK(ae_bots_combat_weapon_spent(1, 0, 0, 1.0f) == 1);
	CHECK(ae_bots_combat_weapon_spent(0, 0, 0, 0.5f) == 0);
	CHECK(ae_bots_combat_weapon_spent(0, 0, 0, 1.0f) == 1);
}

/* PRO TRAITS: pacing, the descope reflex, high ground, the grenade at the last-seen spot, retreat as shields pop */
static void test_vertical_view(void)
{
	int level;

	for (level = AE_BOTS_LEVEL_RECRUIT; level < AE_BOTS_LEVEL_DEMON; level++)
		CHECK(ae_bots_combat_vertical_half_view(level) < ae_bots_combat_vertical_half_view(level + 1));
	CHECK(ae_bots_combat_in_vertical_view(AE_BOTS_LEVEL_HEROIC, 30.0f, 0.0f));
	CHECK(!ae_bots_combat_in_vertical_view(AE_BOTS_LEVEL_HEROIC, 45.0f, 0.0f));
	CHECK(ae_bots_combat_in_vertical_view(AE_BOTS_LEVEL_HEROIC, 45.0f, 20.0f));
	CHECK(!ae_bots_combat_in_vertical_view(AE_BOTS_LEVEL_HEROIC, -40.0f, 0.0f));
	CHECK(ae_bots_combat_in_vertical_view(AE_BOTS_LEVEL_DEMON, 39.0f, 0.0f));
	CHECK(!ae_bots_combat_in_vertical_view(AE_BOTS_LEVEL_RECRUIT, 23.0f, 0.0f));
}

static void test_step_in(void)
{
	CHECK(!ae_bots_combat_step_in(AE_BOTS_LEVEL_RECRUIT, 1.0f, 0.0f));
	CHECK(ae_bots_combat_step_in(AE_BOTS_LEVEL_EASY, 2.4f, 0.4f));
	CHECK(!ae_bots_combat_step_in(AE_BOTS_LEVEL_EASY, 2.6f, 0.4f));
	CHECK(!ae_bots_combat_step_in(AE_BOTS_LEVEL_EASY, 1.0f, 0.6f));
	CHECK(ae_bots_combat_step_in(AE_BOTS_LEVEL_NORMAL, 3.4f, 0.7f));
	CHECK(!ae_bots_combat_step_in(AE_BOTS_LEVEL_NORMAL, 3.4f, 0.9f));
	CHECK(ae_bots_combat_step_in(AE_BOTS_LEVEL_HEROIC, 3.9f, 1.0f));
	CHECK(!ae_bots_combat_step_in(AE_BOTS_LEVEL_DEMON, 4.1f, 0.0f));
	CHECK(ae_bots_combat_step_in(AE_BOTS_LEVEL_DEMON, 1.0f, 1.2f));
}

static void test_pro_traits(void)
{
	struct ae_bots_combat_retreat state;
	int started, ended, level;
	long now;

	/* (pacing: NORMAL and up past 8 units, under the level's limit or after its wait) */
	CHECK(!ae_bots_combat_pace_holds(AE_BOTS_LEVEL_EASY, 20.0f, 1.0f, 0));
	CHECK(!ae_bots_combat_pace_holds(AE_BOTS_LEVEL_NORMAL, 8.0f, 1.0f, 0));
	CHECK(ae_bots_combat_pace_holds(AE_BOTS_LEVEL_NORMAL, 12.0f, 0.7f, 0));
	CHECK(!ae_bots_combat_pace_holds(AE_BOTS_LEVEL_NORMAL, 12.0f, 0.5f, 0));
	CHECK(!ae_bots_combat_pace_holds(AE_BOTS_LEVEL_NORMAL, 12.0f, 0.7f, 6));
	CHECK(ae_bots_combat_pace_holds(AE_BOTS_LEVEL_DEMON, 12.0f, 0.3f, 11));
	CHECK(!ae_bots_combat_pace_holds(AE_BOTS_LEVEL_DEMON, 12.0f, 0.3f, 12));
	CHECK(!ae_bots_combat_pace_holds(AE_BOTS_LEVEL_LEGENDARY, 12.0f, 0.3f, 0));

	/* (zoom: NORMAL and up past 6 units; the moment again after a hit, quicker by level) */
	CHECK(!ae_bots_combat_zooms(AE_BOTS_LEVEL_RECRUIT, 30.0f));
	CHECK(!ae_bots_combat_zooms(AE_BOTS_LEVEL_EASY, 30.0f));
	CHECK(!ae_bots_combat_zooms(AE_BOTS_LEVEL_NORMAL, 6.0f));
	CHECK(ae_bots_combat_zooms(AE_BOTS_LEVEL_NORMAL, 6.5f));
	CHECK(ae_bots_combat_zooms(AE_BOTS_LEVEL_DEMON, 30.0f));
	CHECK(ae_bots_combat_rescope_ticks(AE_BOTS_LEVEL_EASY, 0.5f) == 0);
	CHECK(ae_bots_combat_rescope_ticks(AE_BOTS_LEVEL_NORMAL, 0.0f) == 18);
	CHECK(ae_bots_combat_rescope_ticks(AE_BOTS_LEVEL_NORMAL, 1.0f) == 24);
	CHECK(ae_bots_combat_rescope_ticks(AE_BOTS_LEVEL_DEMON, 0.0f) == 5);
	for (level = AE_BOTS_LEVEL_NORMAL; level < AE_BOTS_LEVEL_DEMON; level++)
		CHECK(ae_bots_combat_rescope_ticks(level + 1, 1.0f) < ae_bots_combat_rescope_ticks(level, 0.0f));
	CHECK(!ae_bots_combat_descoped_holds_fire(10.0f));
	CHECK(ae_bots_combat_descoped_holds_fire(10.5f));

	/* (a scoped rifle: NORMAL and up past half its aim assist's reach, the carbine's 17 units) */
	CHECK(!ae_bots_combat_rifle_zooms(AE_BOTS_LEVEL_EASY, 14.0f, 17.0f));
	CHECK(!ae_bots_combat_rifle_zooms(AE_BOTS_LEVEL_NORMAL, 8.5f, 17.0f));
	CHECK(ae_bots_combat_rifle_zooms(AE_BOTS_LEVEL_NORMAL, 9.0f, 17.0f));
	CHECK(ae_bots_combat_rifle_zooms(AE_BOTS_LEVEL_DEMON, 30.0f, 17.0f));
	CHECK(!ae_bots_combat_rifle_zooms(AE_BOTS_LEVEL_HEROIC, 30.0f, 0.0f));
	/* (the battle rifle's 20 units: past 10) */
	CHECK(!ae_bots_combat_rifle_zooms(AE_BOTS_LEVEL_NORMAL, 10.0f, 20.0f));
	CHECK(ae_bots_combat_rifle_zooms(AE_BOTS_LEVEL_NORMAL, 10.5f, 20.0f));
	CHECK(!ae_bots_combat_rifle_zooms(AE_BOTS_LEVEL_RECRUIT, 16.0f, 20.0f));

	/* (high ground: LEGENDARY and up, capped; nothing below) */
	CHECK(ae_bots_combat_high_ground(AE_BOTS_LEVEL_HEROIC, 3.0f) == 0.0f);
	CHECK(ae_bots_combat_high_ground(AE_BOTS_LEVEL_LEGENDARY, -2.0f) == 0.0f);
	CHECK(ae_bots_combat_high_ground(AE_BOTS_LEVEL_LEGENDARY, 2.0f) == 3.0f);
	CHECK(ae_bots_combat_high_ground(AE_BOTS_LEVEL_LEGENDARY, 10.0f) == 4.5f);
	CHECK(ae_bots_combat_high_ground(AE_BOTS_LEVEL_DEMON, 2.0f) == 4.0f);
	CHECK(ae_bots_combat_high_ground(AE_BOTS_LEVEL_DEMON, 10.0f) == 6.0f);

	/* (the grenade at the last-seen spot: LEGENDARY and up, in its window and distance) */
	CHECK(!ae_bots_combat_prenade(AE_BOTS_LEVEL_HEROIC, 1.0f, 10.0f, 1, 0.0f));
	CHECK(ae_bots_combat_prenade(AE_BOTS_LEVEL_LEGENDARY, 1.0f, 10.0f, 1, 0.4f));
	CHECK(!ae_bots_combat_prenade(AE_BOTS_LEVEL_LEGENDARY, 1.0f, 10.0f, 1, 0.6f));
	CHECK(ae_bots_combat_prenade(AE_BOTS_LEVEL_DEMON, 1.0f, 10.0f, 1, 0.6f));
	CHECK(!ae_bots_combat_prenade(AE_BOTS_LEVEL_DEMON, 0.5f, 10.0f, 1, 0.0f));
	CHECK(!ae_bots_combat_prenade(AE_BOTS_LEVEL_DEMON, 3.5f, 10.0f, 1, 0.0f));
	CHECK(!ae_bots_combat_prenade(AE_BOTS_LEVEL_DEMON, 1.0f, 4.0f, 1, 0.0f));
	CHECK(!ae_bots_combat_prenade(AE_BOTS_LEVEL_DEMON, 1.0f, 21.0f, 1, 0.0f));
	CHECK(!ae_bots_combat_prenade(AE_BOTS_LEVEL_DEMON, 1.0f, 10.0f, 0, 0.0f));

	/* (retreat as shields pop: LEGENDARY and up, losing, after 3 to 9 ticks; HEROIC still waits for 0.1) */
	ae_bots_combat_retreat_reset(&state);
	for (now = 100; now < 103; now++)
		CHECK(!ae_bots_combat_retreat_step(&state, AE_BOTS_LEVEL_LEGENDARY, 0.3f, 0.0f, now, 0.0f, &started, &ended));
	CHECK(ae_bots_combat_retreat_step(&state, AE_BOTS_LEVEL_LEGENDARY, 0.3f, 0.0f, 103, 0.0f, &started, &ended));
	CHECK(started);
	ae_bots_combat_retreat_reset(&state);
	for (now = 100; now < 140; now++)
		CHECK(!ae_bots_combat_retreat_step(&state, AE_BOTS_LEVEL_HEROIC, 0.3f, 0.0f, now, 0.0f, &started, &ended));
	ae_bots_combat_retreat_reset(&state);
	/* (shields still up, or winning: no) */
	for (now = 100; now < 140; now++)
	{
		CHECK(!ae_bots_combat_retreat_step(&state, AE_BOTS_LEVEL_DEMON, 0.3f, 0.2f, now, 0.0f, &started, &ended));
		CHECK(!ae_bots_combat_retreat_step(&state, AE_BOTS_LEVEL_DEMON, 0.4f, 0.0f, now, 0.0f, &started, &ended));
	}
}

/* INFECTION ZOMBIES: every level strikes within reach, shields up or not; RECRUIT closer and now and then; quicker */
static void test_zombie_melee(void)
{
	int level;

	for (level = 0; level < AE_BOTS_LEVELS; level++)
	{
		CHECK(ae_bots_combat_zombie_melee(level, 0.3f, 0, 0.0f) == 0);
		CHECK(ae_bots_combat_zombie_melee(level, 1.2f, 1, 0.0f) == 0);
		CHECK(ae_bots_combat_zombie_melee(level, 0.5f, 1, 0.0f) == 1);
		CHECK(ae_bots_combat_zombie_melee_delay(level, 1.0f) <= ae_bots_combat_melee_delay(level, 1.0f));
		CHECK(ae_bots_combat_zombie_melee_cooldown(level, 1.0f) < ae_bots_combat_melee_cooldown(level, 1.0f));
		CHECK(ae_bots_combat_zombie_melee_cooldown(level, 0.0f) > 0);
	}
	for (level = AE_BOTS_LEVEL_EASY; level < AE_BOTS_LEVELS; level++)
		CHECK(ae_bots_combat_zombie_melee(level, 1.0f, 1, 0.99f) == 1);
	CHECK(ae_bots_combat_zombie_melee(AE_BOTS_LEVEL_RECRUIT, 1.0f, 1, 0.0f) == 0);
	CHECK(ae_bots_combat_zombie_melee(AE_BOTS_LEVEL_RECRUIT, 0.5f, 1, 0.9f) == 2);
	/* (HEROIC strikes a shielded zombie's target from 1 unit, where its ordinary blow waits for point blank) */
	CHECK(ae_bots_combat_melee(AE_BOTS_LEVEL_HEROIC, 1.0f, 1, 0, 1, 0, 0.0f) == 0);
	CHECK(ae_bots_combat_zombie_melee(AE_BOTS_LEVEL_HEROIC, 1.0f, 1, 0.0f) == 1);
}

static void test_friendly_fire(void)
{
	/* The owner's Sali case: Jenkins, the owner, then the enemy along the same shot line. */
	CHECK(ae_bots_combat_teammate_blocks(-6.3f, -4.2f, 0.0f, 2.0f, -0.4f, 0.0f,
		5.9f, 1.1f, 0.0f));
	CHECK(!ae_bots_combat_teammate_blocks(-6.3f, -4.2f, 0.0f, 2.0f, 2.0f, 0.0f,
		5.9f, 1.1f, 0.0f));
	CHECK(!ae_bots_combat_teammate_blocks(-6.3f, -4.2f, 0.0f, 8.0f, 2.0f, 0.0f,
		5.9f, 1.1f, 0.0f));
	CHECK(!ae_bots_combat_teammate_blocks(0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f,
		10.0f, 0.0f, 0.0f));
	CHECK(ae_bots_combat_grenade_near_teammate(10.0f, 3.0f, 0.0f, 12.0f, 4.0f, 0.0f));
	CHECK(!ae_bots_combat_grenade_near_teammate(10.0f, 3.0f, 0.0f, 14.0f, 4.0f, 0.0f));
}

static void test_crouch_jump(void)
{
	/* the top of the jump: in the air, past the press, no longer rising; not on the ground or on the way up */
	CHECK(ae_bots_combat_crouch_jump(1, 10, 0.0f));
	CHECK(ae_bots_combat_crouch_jump(1, 10, -0.05f));
	CHECK(!ae_bots_combat_crouch_jump(1, 10, 0.05f));
	CHECK(!ae_bots_combat_crouch_jump(0, 10, 0.0f));
	CHECK(!ae_bots_combat_crouch_jump(1, 1, 0.0f));
	CHECK(!ae_bots_combat_crouch_jump(1, 90, 0.0f));
}

int main(void)
{
	test_crouch_jump();
	test_hash();
	test_reaction_and_look();
	test_confidence();
	test_melee();
	test_zombie_melee();
	test_grenades();
	test_aim_head();
	test_retreat();
	test_charge_jump_share();
	test_weapon_spent();
	test_pro_traits();
	test_step_in();
	test_vertical_view();
	test_friendly_fire();
	printf("%d checks, %d failures\n", checks, failures);
	/* THREATS: gun on gun first, then the carrier; nothing for RECRUIT */
	{
		struct ae_bots_threat_view threat = { AE_BOTS_LEVEL_HEROIC, 1, 1, 0, 0, 0, 0 };
		struct ae_bots_threat_view carrier = { AE_BOTS_LEVEL_HEROIC, 0, 1, 1, 0, 0, 0 };
		struct ae_bots_threat_view plain = { AE_BOTS_LEVEL_HEROIC, 0, 0, 0, 0, 0, 0 };

		CHECK(ae_bots_combat_threat_score(&threat) > ae_bots_combat_threat_score(&carrier));
		CHECK(ae_bots_combat_threat_score(&plain) == 0.0f);
		threat.self_on_gun = 0;
		CHECK(ae_bots_combat_threat_score(&threat) > ae_bots_combat_threat_score(&carrier));
		threat.level = AE_BOTS_LEVEL_RECRUIT;
		CHECK(ae_bots_combat_threat_score(&threat) == 0.0f);
	}
	return failures ? 1 : 0;
}
