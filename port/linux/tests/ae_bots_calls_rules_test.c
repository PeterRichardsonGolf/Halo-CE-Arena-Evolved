/* The bots' wheel calls' limits (port/linux/game/ae_bots_calls_rules.c): humans' teams only, a living bot, the gaps, no repeats */
#include <stdio.h>

#include "ae_bots_calls_rules.h"
#include "ae_wheel_rules.h"

static int failures;
#define AE_CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

#define NOW 10000L

static int allowed(struct ae_bots_call_team const *team, long last, int low, int human, int alive, int foot, int driver, int entry,
	float x, long now)
{
	return ae_bots_call_allowed(team, last, low, human, alive, foot, driver, entry, x, 0.0f, 0.0f, now);
}

static void test_no_human_no_call(void)
{
	struct ae_bots_call_team team;

	ae_bots_call_team_reset(&team);
	AE_CHECK(allowed(&team, 0, 0, 0, 1, 1, 0, AE_WHEEL_ENEMY, 0.0f, NOW) == 0);
	AE_CHECK(allowed(&team, 0, 0, 1, 1, 1, 0, AE_WHEEL_ENEMY, 0.0f, NOW) == 1);
}

static void test_calls_need_a_living_bot(void)
{
	struct ae_bots_call_team team;

	ae_bots_call_team_reset(&team);
	AE_CHECK(allowed(&team, 0, 0, 1, 0, 1, 0, AE_WHEEL_ENEMY, 0.0f, NOW) == 0);
	AE_CHECK(allowed(&team, 0, 0, 1, 1, 0, 0, AE_WHEEL_ENEMY, 0.0f, NOW) == 0);
	AE_CHECK(allowed(&team, 0, 0, 1, 1, 1, 0, AE_WHEEL_RIDE, 0.0f, NOW) == 0);
	AE_CHECK(allowed(&team, 0, 0, 1, 1, 1, 1, AE_WHEEL_RIDE, 0.0f, NOW) == 1);
	AE_CHECK(allowed(&team, 0, 0, 1, 1, 1, 1, AE_WHEEL_COUNT, 0.0f, NOW) == 0);
}

static void test_bot_gap(void)
{
	struct ae_bots_call_team team;

	ae_bots_call_team_reset(&team);
	AE_CHECK(allowed(&team, NOW - 179, 0, 1, 1, 1, 0, AE_WHEEL_ENEMY, 0.0f, NOW) == 0);
	AE_CHECK(allowed(&team, NOW - 180, 0, 1, 1, 1, 0, AE_WHEEL_ENEMY, 0.0f, NOW) == 1);
	AE_CHECK(allowed(&team, NOW - 359, 1, 1, 1, 1, 0, AE_WHEEL_ENEMY, 0.0f, NOW) == 0);
	AE_CHECK(allowed(&team, NOW - 360, 1, 1, 1, 1, 0, AE_WHEEL_ENEMY, 0.0f, NOW) == 1);
	AE_CHECK(allowed(&team, 0, 1, 1, 1, 1, 0, AE_WHEEL_ENEMY, 0.0f, NOW) == 1);
}

static void test_team_gap(void)
{
	struct ae_bots_call_team team;

	ae_bots_call_team_reset(&team);
	ae_bots_call_made(&team, AE_WHEEL_ENEMY, 0.0f, 0.0f, 0.0f, NOW);
	AE_CHECK(allowed(&team, 0, 0, 1, 1, 1, 0, AE_WHEEL_GOING, 100.0f, NOW + 59) == 0);
	AE_CHECK(allowed(&team, 0, 0, 1, 1, 1, 0, AE_WHEEL_GOING, 100.0f, NOW + 60) == 1);
}

static void test_repeat(void)
{
	struct ae_bots_call_team team;

	ae_bots_call_team_reset(&team);
	ae_bots_call_made(&team, AE_WHEEL_ENEMY, 0.0f, 0.0f, 0.0f, NOW);
	AE_CHECK(allowed(&team, 0, 0, 1, 1, 1, 0, AE_WHEEL_ENEMY, 5.9f, NOW + 599) == 0);
	AE_CHECK(allowed(&team, 0, 0, 1, 1, 1, 0, AE_WHEEL_ENEMY, 5.9f, NOW + 600) == 1);
	AE_CHECK(allowed(&team, 0, 0, 1, 1, 1, 0, AE_WHEEL_ENEMY, 6.1f, NOW + 100) == 1);
	AE_CHECK(allowed(&team, 0, 0, 1, 1, 1, 0, AE_WHEEL_GOING, 0.0f, NOW + 100) == 1);
}

static void test_ring_wraps(void)
{
	struct ae_bots_call_team team;
	int i;

	ae_bots_call_team_reset(&team);
	for (i = 0; i < AE_BOTS_CALL_RECENT + 2; i++)
		ae_bots_call_made(&team, AE_WHEEL_ENEMY, 100.0f * (float)i, 0.0f, 0.0f, NOW + i);
	/* the newest is kept, the oldest is gone */
	AE_CHECK(allowed(&team, 0, 0, 1, 1, 1, 0, AE_WHEEL_ENEMY, 100.0f * (float)(AE_BOTS_CALL_RECENT + 1), NOW + 200) == 0);
	AE_CHECK(allowed(&team, 0, 0, 1, 1, 1, 0, AE_WHEEL_ENEMY, 0.0f, NOW + 200) == 1);
}

static void test_ring_holds_the_repeat_window(void)
{
	struct ae_bots_call_team team;
	int i;

	ae_bots_call_team_reset(&team);
	/* 9 calls at the team's gap, far apart (a 20 s window holds up to 10); the first one again, 60 ticks after the ninth and
	inside its 20 s, is refused: a ring of 8 would have lost it */
	for (i = 0; i < 9; i++)
		ae_bots_call_made(&team, AE_WHEEL_ENEMY, 100.0f * (float)i, 0.0f, 0.0f, NOW + i * AE_BOTS_CALL_TEAM_GAP_TICKS);
	AE_CHECK(allowed(&team, 0, 0, 1, 1, 1, 0, AE_WHEEL_ENEMY, 0.0f, NOW + 9 * AE_BOTS_CALL_TEAM_GAP_TICKS) == 0);
	AE_CHECK(allowed(&team, 0, 0, 1, 1, 1, 0, AE_WHEEL_ENEMY, 0.0f, NOW + AE_BOTS_CALL_REPEAT_TICKS) == 1);
}

static void test_reset(void)
{
	struct ae_bots_call_team team;

	ae_bots_call_team_reset(&team);
	ae_bots_call_made(&team, AE_WHEEL_ENEMY, 0.0f, 0.0f, 0.0f, NOW);
	AE_CHECK(allowed(&team, 0, 0, 1, 1, 1, 0, AE_WHEEL_ENEMY, 0.0f, NOW + 1) == 0);
	ae_bots_call_team_reset(&team);
	AE_CHECK(allowed(&team, 0, 0, 1, 1, 1, 0, AE_WHEEL_ENEMY, 0.0f, NOW + 1) == 1);
}

int main(void)
{
	test_no_human_no_call();
	test_calls_need_a_living_bot();
	test_bot_gap();
	test_team_gap();
	test_repeat();
	test_ring_wraps();
	test_ring_holds_the_repeat_window();
	test_reset();
	if (failures)
		return 1;
	printf("ok\n");
	return 0;
}
