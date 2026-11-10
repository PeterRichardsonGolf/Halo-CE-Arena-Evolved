#include <stdio.h>
#include <string.h>
#include "bots_combat.h"
#include "ae_bots_combat_rules.h"

typedef float real;

struct bots_skill_definition
{
	char const *name;
	real reaction_seconds;
	real sight_distance;
	real field_of_view_degrees;
	real aim_error_degrees;
	real aim_settle_seconds;
	real turn_degrees_per_tick;
	real fire_cone;
	real lead;
	real grenade_chance;
	short burst_ticks;
	short pause_ticks;
	real jump_chance;
};

#define AE_BOTS_SKILL_TABLE_TEST
#include "ae_bots_skill.inc"

static int failures;
static int checks;
#define CHECK(c) do { checks++; if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static void test_levels(void)
{
	static char const *const names[AE_BOTS_LEVELS] =
		{ "recruit", "easy", "normal", "heroic", "legendary", "demon" };
	static float const reactions[AE_BOTS_LEVELS] = { 1.4f, 0.66f, 0.49f, 0.30f, 0.24f, 0.18f };
	static float const sights[AE_BOTS_LEVELS] = { 16.0f, 25.0f, 31.0f, 42.0f, 48.0f, 55.0f };
	static short const bursts[AE_BOTS_LEVELS] = { 4, 7, 9, 12, 14, 16 };
	static short const pauses[AE_BOTS_LEVELS] = { 24, 13, 10, 6, 5, 4 };
	int level;

	for (level = 0; level < AE_BOTS_LEVELS; level++)
	{
		short skill = ae_bots_skill_of_level(level);
		struct bots_skill_definition const *definition = ae_bots_skill_definition(skill);

		CHECK(!strcmp(definition->name, names[level]));
		CHECK(definition->reaction_seconds == reactions[level]);
		CHECK(definition->sight_distance == sights[level]);
		CHECK(definition->burst_ticks == bursts[level]);
		CHECK(definition->pause_ticks == pauses[level]);
		CHECK(ae_bots_level_of_skill(skill) == level);
	}
}

static void test_ids(void)
{
	CHECK(AE_BOTS_SKILL_RECRUIT == _bots_skill_recruit);
	CHECK(ae_bots_skill_of_level(AE_BOTS_LEVEL_EASY) == _bots_skill_easy);
	CHECK(ae_bots_skill_of_level(AE_BOTS_LEVEL_NORMAL) == _bots_skill_normal);
	CHECK(ae_bots_skill_of_level(AE_BOTS_LEVEL_HEROIC) == _bots_skill_heroic);
	CHECK(AE_BOTS_SKILL_LEGENDARY == _bots_skill_veteran);
	CHECK(AE_BOTS_SKILL_DEMON == _bots_skill_legendary);
	CHECK(!strcmp(ae_bots_skill_definition(-1)->name, "normal"));
	CHECK(!strcmp(ae_bots_skill_definition(NUMBER_OF_BOTS_SKILLS)->name, "normal"));
	CHECK(ae_bots_level_of_skill(-1) == AE_BOTS_LEVEL_NORMAL);
	CHECK(ae_bots_skill_of_level(-1) == _bots_skill_normal);
}

int main(void)
{
	test_levels();
	test_ids();
	printf("%d checks, %d failures\n", checks, failures);
	return failures ? 1 : 0;
}
