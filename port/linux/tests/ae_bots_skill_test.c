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
		{ "recruit", "easy", "normal", "heroic", "elite", "legendary", "demon" };
	static float const reactions[AE_BOTS_LEVELS] = { 1.4f, 0.63f, 0.49f, 0.38f, 0.30f, 0.23f, 0.18f };
	static float const sights[AE_BOTS_LEVELS] = { 16.0f, 25.0f, 31.0f, 37.0f, 43.0f, 49.0f, 55.0f };
	static short const bursts[AE_BOTS_LEVELS] = { 4, 7, 9, 11, 12, 14, 16 };
	static short const pauses[AE_BOTS_LEVELS] = { 24, 12, 10, 8, 7, 5, 4 };
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

/* the balance pass (2026-11-11): from EASY to DEMON each step is about the same ratio in reaction, aim error,
settling, turn and fire cone (within 6 percent of the ladder's own ratio, the rounding to the rows' digits) */
static void test_even_steps(void)
{
	int level;

	for (level = AE_BOTS_LEVEL_EASY; level < AE_BOTS_LEVEL_DEMON; level++)
	{
		struct bots_skill_definition const *a = ae_bots_skill_definition(ae_bots_skill_of_level(level));
		struct bots_skill_definition const *b = ae_bots_skill_definition(ae_bots_skill_of_level(level + 1));
		float reaction = b->reaction_seconds / a->reaction_seconds;
		float aim = b->aim_error_degrees / a->aim_error_degrees;
		float settle = b->aim_settle_seconds / a->aim_settle_seconds;
		float turn = b->turn_degrees_per_tick / a->turn_degrees_per_tick;
		float cone = b->fire_cone / a->fire_cone;

		CHECK(reaction > 0.73f && reaction < 0.83f);
		CHECK(aim > 0.69f && aim < 0.79f);
		CHECK(settle > 0.73f && settle < 0.83f);
		CHECK(turn > 1.17f && turn < 1.31f);
		CHECK(cone > 0.86f && cone < 0.96f);
		CHECK(b->sight_distance - a->sight_distance == 6.0f);
		CHECK(b->burst_ticks > a->burst_ticks && b->pause_ticks < a->pause_ticks);
	}
}

static void test_ids(void)
{
	CHECK(AE_BOTS_SKILL_RECRUIT == _bots_skill_recruit);
	CHECK(ae_bots_skill_of_level(AE_BOTS_LEVEL_EASY) == _bots_skill_easy);
	CHECK(ae_bots_skill_of_level(AE_BOTS_LEVEL_NORMAL) == _bots_skill_normal);
	CHECK(ae_bots_skill_of_level(AE_BOTS_LEVEL_HEROIC) == _bots_skill_heroic);
	CHECK(AE_BOTS_SKILL_ELITE == NUMBER_OF_BOTS_SKILLS);
	CHECK(ae_bots_skill_of_level(AE_BOTS_LEVEL_ELITE) == AE_BOTS_SKILL_ELITE);
	CHECK(AE_BOTS_SKILL_LEGENDARY == _bots_skill_veteran);
	CHECK(AE_BOTS_SKILL_DEMON == _bots_skill_legendary);
	CHECK(!strcmp(ae_bots_skill_definition(-1)->name, "normal"));
	CHECK(!strcmp(ae_bots_skill_definition(NUMBER_OF_BOTS_SKILLS)->name, "elite"));
	CHECK(!strcmp(ae_bots_skill_definition(NUMBER_OF_BOTS_SKILLS + 1)->name, "normal"));
	CHECK(ae_bots_level_of_skill(-1) == AE_BOTS_LEVEL_NORMAL);
	CHECK(ae_bots_skill_of_level(-1) == _bots_skill_normal);
}

int main(void)
{
	test_levels();
	test_even_steps();
	test_ids();
	printf("%d checks, %d failures\n", checks, failures);
	return failures ? 1 : 0;
}
