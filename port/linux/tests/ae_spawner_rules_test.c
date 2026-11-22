#include <stdio.h>
#include <string.h>

#include "ae_spawner_rules.h"

static int failures;
static int checks;
#define CHECK(c) do { checks++; if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

/* The stock table of design 9.1: the pistol is PRECISION (the battle rifle's spot), the shotgun CLOSE POWER. */
static void test_stock_table(void)
{
	CHECK(ae_spawner_stock_class(AE_STOCK_PISTOL) == AE_SPAWNER_PRECISION);
	CHECK(ae_spawner_stock_tier(AE_STOCK_PISTOL) == AE_SPAWNER_BASE);
	CHECK(ae_spawner_stock_class(AE_STOCK_SHOTGUN) == AE_SPAWNER_CLOSE);
	CHECK(ae_spawner_stock_tier(AE_STOCK_SHOTGUN) == AE_SPAWNER_POWER);
	CHECK(ae_spawner_stock_class(AE_STOCK_SNIPER_RIFLE) == AE_SPAWNER_LONG_RANGE);
	CHECK(ae_spawner_stock_tier(AE_STOCK_SNIPER_RIFLE) == AE_SPAWNER_POWER);
	CHECK(ae_spawner_stock_class(AE_STOCK_ROCKET_LAUNCHER) == AE_SPAWNER_HEAVY);
	CHECK(ae_spawner_stock_class(AE_STOCK_FLAMETHROWER) == AE_SPAWNER_HEAVY);
	CHECK(ae_spawner_stock_class(AE_STOCK_GRAVITY_RIFLE) == AE_SPAWNER_HEAVY);
	CHECK(ae_spawner_stock_tier(AE_STOCK_GRAVITY_RIFLE) == AE_SPAWNER_POWER);
	CHECK(ae_spawner_stock_class(AE_STOCK_SWORD) == AE_SPAWNER_MELEE);
	CHECK(ae_spawner_stock_tier(AE_STOCK_SWORD) == AE_SPAWNER_POWER);
	CHECK(ae_spawner_stock_class(AE_STOCK_PLASMA_PISTOL) == AE_SPAWNER_PISTOL);
	CHECK(ae_spawner_stock_tier(AE_STOCK_PLASMA_PISTOL) == AE_SPAWNER_BASE);
	CHECK(ae_spawner_stock_class(AE_STOCK_ASSAULT_RIFLE) == AE_SPAWNER_AUTOMATIC);
	CHECK(ae_spawner_stock_class(AE_STOCK_PLASMA_RIFLE) == AE_SPAWNER_AUTOMATIC);
	CHECK(ae_spawner_stock_class(AE_STOCK_NEEDLER) == AE_SPAWNER_AUTOMATIC);
	CHECK(ae_spawner_stock_tier(AE_STOCK_NEEDLER) == AE_SPAWNER_BASE);
	CHECK(ae_spawner_stock_class(-1) == -1);
	CHECK(ae_spawner_stock_tier(AE_STOCK_COUNT) == -1);
	CHECK(ae_spawner_stock_of_word("shotgun") == AE_STOCK_SHOTGUN);
	CHECK(ae_spawner_stock_of_word("ROCKET_LAUNCHER") == AE_STOCK_ROCKET_LAUNCHER);
	CHECK(ae_spawner_stock_of_word("rocket-launcher") == AE_STOCK_ROCKET_LAUNCHER);
	CHECK(ae_spawner_stock_of_word("rockets") == -1);	/* the layout alias is the layout's business */
	CHECK(ae_spawner_stock_of_word(NULL) == -1);
	CHECK(strcmp(ae_spawner_stock_word(AE_STOCK_PISTOL), "pistol") == 0);
	CHECK(strcmp(ae_spawner_stock_word(99), "") == 0);
}

/* A map item's own time of 90 s or more promotes a BASE weapon's spot to POWER; a map time never demotes. */
static void test_tier_promotion(void)
{
	CHECK(ae_spawner_tier_of(AE_SPAWNER_BASE, 0) == AE_SPAWNER_BASE);
	CHECK(ae_spawner_tier_of(AE_SPAWNER_BASE, 89) == AE_SPAWNER_BASE);
	CHECK(ae_spawner_tier_of(AE_SPAWNER_BASE, 90) == AE_SPAWNER_POWER);
	CHECK(ae_spawner_tier_of(AE_SPAWNER_BASE, 120) == AE_SPAWNER_POWER);
	CHECK(ae_spawner_tier_of(AE_SPAWNER_POWER, 0) == AE_SPAWNER_POWER);
	CHECK(ae_spawner_tier_of(AE_SPAWNER_POWER, 30) == AE_SPAWNER_POWER);	/* Hang 'em High's 30 s sniper stays POWER */
	CHECK(ae_spawner_tier_of(AE_SPAWNER_TIER_ANY, 0) == AE_SPAWNER_TIER_ANY);
	CHECK(ae_spawner_tier_of(AE_SPAWNER_TIER_ANY, 90) == AE_SPAWNER_POWER);
}

/* A mixed collection: the heaviest class wins, a tie between the heaviest is ANY, all zero is ANY. */
static void test_class_weights(void)
{
	int w[AE_SPAWNER_CLASS_COUNT];

	memset(w, 0, sizeof w);
	CHECK(ae_spawner_class_of_weights(w) == AE_SPAWNER_CLASS_ANY);
	w[AE_SPAWNER_AUTOMATIC] = 3;
	w[AE_SPAWNER_PRECISION] = 1;
	CHECK(ae_spawner_class_of_weights(w) == AE_SPAWNER_AUTOMATIC);
	w[AE_SPAWNER_PRECISION] = 3;
	CHECK(ae_spawner_class_of_weights(w) == AE_SPAWNER_CLASS_ANY);
	w[AE_SPAWNER_HEAVY] = 4;
	CHECK(ae_spawner_class_of_weights(w) == AE_SPAWNER_HEAVY);
	memset(w, 0, sizeof w);
	w[AE_SPAWNER_CLASS_ANY] = 9;	/* a weight on ANY itself counts for nothing */
	CHECK(ae_spawner_class_of_weights(w) == AE_SPAWNER_CLASS_ANY);
	w[AE_SPAWNER_MELEE] = 1;
	CHECK(ae_spawner_class_of_weights(w) == AE_SPAWNER_MELEE);
}

/* Two weapons lying together are one spot: within 1.5 units and in a common game type. */
static void test_merge(void)
{
	real a[3] = { 10.0f, -4.0f, 2.0f };
	real near[3] = { 10.0f, -5.4f, 2.0f };	/* 1.4 away */
	real far[3] = { 10.0f, -5.6f, 2.0f };	/* 1.6 away */
	real up[3] = { 10.0f, -4.0f, 3.6f };	/* 1.6 up: the distance counts height too */
	real same[3] = { 10.0f, -4.0f, 2.0f };

	CHECK(ae_spawner_merges(a, near, 0xF, 0xF));
	CHECK(!ae_spawner_merges(a, far, 0xF, 0xF));
	CHECK(!ae_spawner_merges(a, up, 0xF, 0xF));
	CHECK(ae_spawner_merges(a, same, 0x1, 0x3));	/* a common game type */
	CHECK(!ae_spawner_merges(a, same, 0x1, 0x2));	/* disjoint masks: Hang 'em High's shotgun over its sniper */
	CHECK(!ae_spawner_merges(a, same, 0x0, 0xF));
	CHECK(ae_spawner_merged_tier(AE_SPAWNER_BASE, AE_SPAWNER_POWER) == AE_SPAWNER_POWER);
	CHECK(ae_spawner_merged_tier(AE_SPAWNER_BASE, AE_SPAWNER_BASE) == AE_SPAWNER_BASE);
	CHECK(ae_spawner_merged_tier(AE_SPAWNER_BASE, AE_SPAWNER_TIER_ANY) == AE_SPAWNER_TIER_ANY);
	CHECK(ae_spawner_merged_tier(AE_SPAWNER_TIER_ANY, AE_SPAWNER_POWER) == AE_SPAWNER_POWER);
}

/* A POWER cell's default is the pad's fixed clock, a BASE cell's the pickup; the floor on a respawn. */
static void test_defaults(void)
{
	CHECK(ae_spawner_default_logic(AE_SPAWNER_CLOSE, AE_SPAWNER_POWER) == AE_SPAWNER_STATIC);
	CHECK(ae_spawner_default_logic(AE_SPAWNER_MELEE, AE_SPAWNER_POWER) == AE_SPAWNER_STATIC);
	CHECK(ae_spawner_default_logic(AE_SPAWNER_CLOSE, AE_SPAWNER_BASE) == AE_SPAWNER_PICKUP);
	CHECK(ae_spawner_default_logic(AE_SPAWNER_PRECISION, AE_SPAWNER_BASE) == AE_SPAWNER_PICKUP);
	CHECK(ae_spawner_default_logic(AE_SPAWNER_CLASS_ANY, AE_SPAWNER_TIER_ANY) == AE_SPAWNER_PICKUP);
	CHECK(ae_spawner_respawn_seconds(0) == 5);
	CHECK(ae_spawner_respawn_seconds(4) == 5);
	CHECK(ae_spawner_respawn_seconds(5) == 5);
	CHECK(ae_spawner_respawn_seconds(30) == 30);
	CHECK(ae_spawner_respawn_seconds(-7) == 5);
}

/* A pad's waypoint fades out up close, where the weapon itself is in view. */
static void test_waypoint_alpha(void)
{
	CHECK(ae_spawner_waypoint_alpha(0.0f) == 0.0f);
	CHECK(ae_spawner_waypoint_alpha(5.0f) == 0.0f);
	CHECK(ae_spawner_waypoint_alpha(6.0f) == 0.0f);
	CHECK(ae_spawner_waypoint_alpha(9.0f) > 0.49f && ae_spawner_waypoint_alpha(9.0f) < 0.51f);
	CHECK(ae_spawner_waypoint_alpha(12.0f) == 1.0f);
	CHECK(ae_spawner_waypoint_alpha(13.0f) == 1.0f);
	CHECK(ae_spawner_waypoint_alpha(70.0f) == 1.0f);
}

/* Every word round-trips, case and dashes aside; a word nobody knows is -1. */
static void test_words(void)
{
	int index;

	for (index = 0; index < AE_SPAWNER_CLASS_COUNT; index++)
		CHECK(ae_spawner_class_of_word(ae_spawner_class_word(index)) == index);
	for (index = 0; index < AE_SPAWNER_TIER_COUNT; index++)
		CHECK(ae_spawner_tier_of_word(ae_spawner_tier_word(index)) == index);
	for (index = 0; index < AE_SPAWNER_LOGIC_COUNT; index++)
		CHECK(ae_spawner_logic_of_word(ae_spawner_logic_word(index)) == index);
	CHECK(ae_spawner_class_of_word("long_range") == AE_SPAWNER_LONG_RANGE);
	CHECK(ae_spawner_class_of_word("long-range") == AE_SPAWNER_LONG_RANGE);
	CHECK(ae_spawner_tier_of_word("power") == AE_SPAWNER_POWER);
	CHECK(ae_spawner_logic_of_word("EXPIRED") == AE_SPAWNER_EXPIRED);
	CHECK(ae_spawner_class_of_word("bogus") == -1);
	CHECK(ae_spawner_tier_of_word("") == -1);
	CHECK(ae_spawner_logic_of_word(NULL) == -1);
	CHECK(ae_spawner_class_of_word("PRECISIONS") == -1);
	CHECK(strcmp(ae_spawner_class_word(AE_SPAWNER_CLOSE), "CLOSE") == 0);
	CHECK(strcmp(ae_spawner_logic_word(AE_SPAWNER_TRIGGER), "trigger") == 0);
	CHECK(strcmp(ae_spawner_class_word(-1), "") == 0);
	CHECK(strcmp(ae_spawner_tier_word(AE_SPAWNER_TIER_COUNT), "") == 0);
}

/* WEAPON SPAWNS: CLASSIC plays every spot static on one weapon, the pool's heaviest; RACKS keeps the set's logic */
static void test_look(void)
{
	static unsigned char const weights[4] = { 2, 5, 5, 1 };
	int logic;

	for (logic = 0; logic < AE_SPAWNER_LOGIC_COUNT; logic++)
	{
		CHECK(ae_spawner_look_logic(0, logic) == logic);
		CHECK(ae_spawner_look_logic(1, logic) == AE_SPAWNER_STATIC);
	}
	CHECK(ae_spawner_look_pick(4, weights) == 1);
	CHECK(ae_spawner_look_pick(1, weights) == 0);
	CHECK(ae_spawner_look_pick(3, NULL) == 0);
	CHECK(ae_spawner_look_pick(0, weights) == -1);
}

int main(void)
{
	test_stock_table();
	test_tier_promotion();
	test_class_weights();
	test_merge();
	test_defaults();
	test_waypoint_alpha();
	test_words();
	test_look();
	printf("%s: %d checks, %d failures\n", __FILE__, checks, failures);
	return failures ? 1 : 0;
}
