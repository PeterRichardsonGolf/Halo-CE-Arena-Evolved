#include <stdio.h>
#include "ae_bots_power_rules.h"
#include "ae_bots_combat_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static struct ae_bots_power_view view(int level, int kind, float distance, int up, float seconds_left)
{
	struct ae_bots_power_view v = { level, kind, distance, seconds_left, up, 0, 0, 0, 0 };

	return v;
}

int main(void)
{
	struct ae_bots_power_view v;
	int level;

	/* reach and lead step up the ladder; RECRUIT .. NORMAL never time a spawn */
	for (level = AE_BOTS_LEVEL_EASY; level < AE_BOTS_LEVELS; level++)
	{
		CHECK(ae_bots_power_reach(level) > ae_bots_power_reach(level - 1));
		CHECK(ae_bots_power_lead_seconds(level) >= ae_bots_power_lead_seconds(level - 1));
	}
	CHECK(ae_bots_power_lead_seconds(AE_BOTS_LEVEL_NORMAL) == 0.0f);
	CHECK(ae_bots_power_lead_seconds(AE_BOTS_LEVEL_HEROIC) > 0.0f);

	/* an item up within reach is wanted; out of reach it is not */
	v = view(AE_BOTS_LEVEL_NORMAL, AE_BOTS_POWER_SNIPER, 20.0f, 1, 0.0f);
	CHECK(ae_bots_power_want(&v) > 0.0f);
	v = view(AE_BOTS_LEVEL_NORMAL, AE_BOTS_POWER_SNIPER, 30.0f, 1, 0.0f);
	CHECK(ae_bots_power_want(&v) == 0.0f);
	v = view(AE_BOTS_LEVEL_RECRUIT, AE_BOTS_POWER_OVERSHIELD, 10.0f, 1, 0.0f);
	CHECK(ae_bots_power_want(&v) == 0.0f);

	/* a spawn to come: NORMAL waits for it; HEROIC goes so as to arrive at most 5 s early */
	v = view(AE_BOTS_LEVEL_NORMAL, AE_BOTS_POWER_ROCKETS, 9.0f, 0, 1.0f);
	CHECK(ae_bots_power_want(&v) == 0.0f);
	v = view(AE_BOTS_LEVEL_HEROIC, AE_BOTS_POWER_ROCKETS, 22.5f, 0, 14.0f);	/* 10 s to run + 5 s early >= 14 */
	CHECK(ae_bots_power_want(&v) > 0.0f);
	v = view(AE_BOTS_LEVEL_HEROIC, AE_BOTS_POWER_ROCKETS, 22.5f, 0, 16.0f);
	CHECK(ae_bots_power_want(&v) == 0.0f);
	v = view(AE_BOTS_LEVEL_DEMON, AE_BOTS_POWER_OVERSHIELD, 200.0f, 0, 90.0f);	/* 89 s to run */
	CHECK(ae_bots_power_want(&v) > 0.0f);

	/* fighting: only one close by */
	v = view(AE_BOTS_LEVEL_DEMON, AE_BOTS_POWER_CAMO, 12.0f, 1, 0.0f);
	v.fighting = 1;
	CHECK(ae_bots_power_want(&v) == 0.0f);
	v.distance = 6.0f;
	CHECK(ae_bots_power_want(&v) > 0.0f);

	/* never one it holds, one found taken, one a teammate goes for */
	v = view(AE_BOTS_LEVEL_LEGENDARY, AE_BOTS_POWER_SNIPER, 10.0f, 1, 0.0f);
	v.holds_it = 1;
	CHECK(ae_bots_power_want(&v) == 0.0f);
	v.holds_it = 0;
	v.known_taken = 1;
	CHECK(ae_bots_power_want(&v) == 0.0f);
	v.known_taken = 0;
	v.teammate_going = 1;
	CHECK(ae_bots_power_want(&v) == 0.0f);

	/* better and nearer is wanted more: rockets over a shotgun at the same distance, near over far */
	{
		struct ae_bots_power_view a = view(AE_BOTS_LEVEL_HEROIC, AE_BOTS_POWER_ROCKETS, 20.0f, 1, 0.0f);
		struct ae_bots_power_view b = view(AE_BOTS_LEVEL_HEROIC, AE_BOTS_POWER_SHOTGUN, 20.0f, 1, 0.0f);
		struct ae_bots_power_view c = view(AE_BOTS_LEVEL_HEROIC, AE_BOTS_POWER_ROCKETS, 40.0f, 1, 0.0f);

		CHECK(ae_bots_power_want(&a) > ae_bots_power_want(&b));
		CHECK(ae_bots_power_want(&a) > ae_bots_power_want(&c));
	}
	CHECK(ae_bots_power_want(0) == 0.0f);

	/* a stand a step above the feet is mounted: not a floor-level item, not a wall, not one far off */
	CHECK(ae_bots_power_mount_applies(0.75f, 1.5f));
	CHECK(ae_bots_power_mount_applies(0.3f, 3.5f));
	CHECK(!ae_bots_power_mount_applies(0.1f, 1.5f));
	CHECK(!ae_bots_power_mount_applies(1.4f, 1.5f));
	CHECK(!ae_bots_power_mount_applies(0.75f, 3.6f));
	/* the staging point is on the axis nearest the bot, 2.0 from the item; a quarter turn per miss */
	{
		float x, y;

		ae_bots_power_mount_point(8.7f, 8.7f, 10.0f, 7.5f, 0, &x, &y);
		CHECK(x > 10.69f && x < 10.71f && y > 8.69f && y < 8.71f);
		ae_bots_power_mount_point(8.7f, 8.7f, 8.0f, 6.0f, 0, &x, &y);
		CHECK(x > 8.69f && x < 8.71f && y > 6.69f && y < 6.71f);
		ae_bots_power_mount_point(8.7f, 8.7f, 5.0f, 9.0f, 0, &x, &y);
		CHECK(x > 6.69f && x < 6.71f && y > 8.69f && y < 8.71f);
		ae_bots_power_mount_point(8.7f, 8.7f, 8.0f, 12.0f, 0, &x, &y);
		CHECK(x > 8.69f && x < 8.71f && y > 10.69f && y < 10.71f);
		ae_bots_power_mount_point(8.7f, 8.7f, 8.0f, 12.0f, 1, &x, &y);
		CHECK(x > 6.69f && x < 6.71f && y > 8.69f && y < 8.71f);
		ae_bots_power_mount_point(8.7f, 8.7f, 8.0f, 12.0f, -1, &x, &y);
		CHECK(x > 10.69f && x < 10.71f && y > 8.69f && y < 8.71f);
	}
	/* the lane: out on the axis by 1.0 or more, within 0.3 of it */
	CHECK(ae_bots_power_mount_in_lane(0.0f, 1.5f, 0.0f, 1.0f));
	CHECK(ae_bots_power_mount_in_lane(0.2f, 1.0f, 0.0f, 1.0f));
	CHECK(!ae_bots_power_mount_in_lane(0.5f, 1.5f, 0.0f, 1.0f));
	CHECK(!ae_bots_power_mount_in_lane(0.0f, 0.8f, 0.0f, 1.0f));
	CHECK(!ae_bots_power_mount_in_lane(0.0f, -1.5f, 0.0f, 1.0f));
	/* it jumps when near and running at the item, not from afar, not from the wall's foot, not standing, not slanting */
	CHECK(ae_bots_power_mount_jump(1.7f, 0.0f, -0.07f, 0.0f, -1.7f));
	CHECK(!ae_bots_power_mount_jump(2.2f, 0.0f, -0.07f, 0.0f, -2.2f));
	CHECK(!ae_bots_power_mount_jump(1.3f, 0.0f, -0.07f, 0.0f, -1.3f));
	CHECK(!ae_bots_power_mount_jump(1.7f, 0.0f, -0.005f, 0.0f, -1.7f));
	CHECK(!ae_bots_power_mount_jump(1.7f, 0.05f, -0.05f, 0.0f, -1.7f));
	CHECK(ae_bots_power_mount_jump(1.7f, 0.01f, -0.07f, 0.0f, -1.7f));

	if (failures)
		printf("%d failed\n", failures);
	else
		printf("ae_bots_power_rules: all passed\n");
	return failures != 0;
}
