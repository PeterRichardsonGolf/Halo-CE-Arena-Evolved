#include <stdio.h>
#include <string.h>
#include "ae_pgcr_data.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static struct ae_pgcr_game game;

static void fresh(void)
{
	ae_pgcr_reset(&game);
}

static void test_accuracy(void)
{
	int index;

	fresh();
	/* no shots fired, no hit */
	CHECK(ae_pgcr_hit(&game, 0, 1, AE_MEDAL_CATEGORY_BULLET, 10) == 0);
	CHECK(ae_pgcr_shot(&game, 0) == 1);
	CHECK(ae_pgcr_hit(&game, 0, 1, AE_MEDAL_CATEGORY_BULLET, 10) == 1);
	/* the same tick: one hit (a shell's pellets) */
	for (index = 0; index < 8; index++)
		ae_pgcr_shot(&game, 0);
	CHECK(ae_pgcr_hit(&game, 0, 1, AE_MEDAL_CATEGORY_SHOTGUN, 10) == 0);
	CHECK(ae_pgcr_hit(&game, 0, 2, AE_MEDAL_CATEGORY_SHOTGUN, 10) == 0);
	CHECK(ae_pgcr_hit(&game, 0, 1, AE_MEDAL_CATEGORY_SHOTGUN, 11) == 1);
	CHECK(game.shots_fired[0] == 9 && game.shots_hit[0] == 2);
	/* not a hit: themselves, a grenade, a blow, a vehicle, nobody */
	CHECK(ae_pgcr_hit(&game, 0, 0, AE_MEDAL_CATEGORY_BULLET, 20) == 0);
	CHECK(ae_pgcr_hit(&game, 0, 1, AE_MEDAL_CATEGORY_GRENADE, 20) == 0);
	CHECK(ae_pgcr_hit(&game, 0, 1, AE_MEDAL_CATEGORY_MELEE, 20) == 0);
	CHECK(ae_pgcr_hit(&game, 0, 1, AE_MEDAL_CATEGORY_VEHICLE, 20) == 0);
	CHECK(ae_pgcr_hit(&game, -1, 1, AE_MEDAL_CATEGORY_BULLET, 20) == 0);
	CHECK(ae_pgcr_hit(&game, 0, 200, AE_MEDAL_CATEGORY_BULLET, 20) == 0);
	CHECK(game.shots_hit[0] == 2);
	/* never more hits than shots */
	fresh();
	ae_pgcr_shot(&game, 3);
	CHECK(ae_pgcr_hit(&game, 3, 4, AE_MEDAL_CATEGORY_PLASMA, 1) == 1);
	CHECK(ae_pgcr_hit(&game, 3, 4, AE_MEDAL_CATEGORY_PLASMA, 2) == 0);
	CHECK(ae_pgcr_shot(&game, -1) == 0);
	CHECK(ae_pgcr_shot(&game, AE_PGCR_PLAYERS) == 0);
	CHECK(ae_pgcr_category_is_gun(AE_MEDAL_CATEGORY_SNIPER) && ae_pgcr_category_is_gun(AE_MEDAL_CATEGORY_NEEDLE));
	CHECK(!ae_pgcr_category_is_gun(AE_MEDAL_CATEGORY_NONE) && !ae_pgcr_category_is_gun(AE_MEDAL_CATEGORY_MOUNTED));
}

static void test_kills(void)
{
	struct ae_pgcr_summary summary;

	fresh();
	/* slot 1 kills slot 2 three times, slot 3 once; slot 2 kills 1 once */
	ae_pgcr_kill(&game, 1, 2, 1, 500, 100);
	ae_pgcr_death(&game, 2);
	ae_pgcr_kill(&game, 1, 2, 0, 500, 150);
	ae_pgcr_death(&game, 2);
	ae_pgcr_kill(&game, 1, 3, 1, 501, 400);
	ae_pgcr_death(&game, 3);
	ae_pgcr_kill(&game, 1, 2, 0, 502, 1000);
	ae_pgcr_death(&game, 2);
	ae_pgcr_kill(&game, 2, 1, 0, 500, 1100);
	ae_pgcr_death(&game, 1);
	ae_pgcr_summarize(&game, 1, &summary);
	CHECK(summary.valid && summary.slot == 1);
	CHECK(summary.killed_most == 2 && summary.killed_most_count == 3);
	CHECK(summary.killed_by_most == 2 && summary.killed_by_most_count == 1);
	CHECK(summary.headshots == 2);
	/* kills at 100 and 150 are a multikill; 400 is 250 ticks on: a new chain */
	CHECK(summary.multikills == 1);
	/* the spree: 4 kills before dying */
	CHECK(summary.best_spree == 4);
	CHECK(summary.weapon_count == 3 && summary.weapons[0].tag == 500 && summary.weapons[0].kills == 2);
	CHECK(summary.weapons[1].tag == 501 && summary.weapons[2].tag == 502);
	ae_pgcr_summarize(&game, 2, &summary);
	CHECK(summary.best_spree == 1 && summary.killed_most == 1 && summary.killed_by_most == 1);
	CHECK(summary.killed_by_most_count == 3);
	/* the kill-less: no rivals */
	ae_pgcr_summarize(&game, 9, &summary);
	CHECK(summary.valid && summary.killed_most == AE_PGCR_NO_PLAYER && summary.killed_by_most == AE_PGCR_NO_PLAYER);
	/* the death ends the spree and the chain */
	fresh();
	ae_pgcr_kill(&game, 4, 5, 0, 1, 10);
	ae_pgcr_kill(&game, 4, 5, 0, 1, 20);
	ae_pgcr_death(&game, 4);
	ae_pgcr_kill(&game, 4, 5, 0, 1, 30);
	CHECK(game.spree[4] == 1 && game.best_spree[4] == 2 && game.chain[4] == 1 && game.multikills[4] == 1);
	/* a suicide counts for no one; a slot out of range too */
	ae_pgcr_kill(&game, 4, 4, 1, 1, 40);
	ae_pgcr_kill(&game, -1, 4, 1, 1, 40);
	ae_pgcr_kill(&game, 4, 999, 1, 1, 40);
	CHECK(game.headshots[4] == 0 && game.kills[4][4] == 0);
	/* the weapon list is capped; kills still count */
	fresh();
	{
		int index;

		for (index = 0; index < AE_PGCR_TRACKED_WEAPONS + 5; index++)
			ae_pgcr_kill(&game, 7, 8, 0, 1000 + index, index * 1000L);
		CHECK(game.weapon_count[7] == AE_PGCR_TRACKED_WEAPONS && game.kills[7][8] == AE_PGCR_TRACKED_WEAPONS + 5);
	}
	/* the table saturates at 255 */
	fresh();
	{
		int index;

		for (index = 0; index < 300; index++)
			ae_pgcr_kill(&game, 7, 8, 0, 1, index * 1000L);
		CHECK(game.kills[7][8] == 255);
	}
}

static void test_medals(void)
{
	struct ae_pgcr_summary summary;
	int index;

	fresh();
	for (index = 0; index < 3; index++)
		ae_pgcr_medal(&game, 6, 5);
	ae_pgcr_medal(&game, 6, 2);
	ae_pgcr_medal(&game, 6, AE_MEDAL_COUNT);
	ae_pgcr_medal(&game, 6, -1);
	ae_pgcr_medal(&game, 6, 9);
	ae_pgcr_medal(&game, 6, 9);
	ae_pgcr_summarize(&game, 6, &summary);
	CHECK(summary.medal_count == 3);
	CHECK(summary.medals[0].id == 5 && summary.medals[0].count == 3);
	CHECK(summary.medals[1].id == 9 && summary.medals[1].count == 2);
	CHECK(summary.medals[2].id == 2 && summary.medals[2].count == 1);
	ae_pgcr_summarize(&game, 0, &summary);
	CHECK(summary.medal_count == 0);
}

static void test_wire(void)
{
	struct ae_pgcr_summary in, out;
	unsigned char bytes[AE_PGCR_WIRE_MAXIMUM];
	int index, length;

	/* every medal kind, every weapon: the longest form fits */
	fresh();
	for (index = 0; index < AE_MEDAL_COUNT; index++)
		ae_pgcr_medal(&game, 77, index);
	for (index = 0; index < 5; index++)
		ae_pgcr_kill(&game, 77, 100 + index, index & 1, 0x01234567L + index, index * 500L);
	for (index = 0; index < 120; index++)
		ae_pgcr_kill(&game, 99, 77, 0, 5, index * 500L);
	ae_pgcr_summarize(&game, 77, &in);
	CHECK(in.medal_count == ((int)AE_MEDAL_COUNT < (int)AE_PGCR_SUMMARY_MEDALS ? (int)AE_MEDAL_COUNT : (int)AE_PGCR_SUMMARY_MEDALS));
	length = ae_pgcr_encode(&in, bytes, sizeof(bytes));
	CHECK(length > 0 && length <= AE_PGCR_WIRE_MAXIMUM);
	CHECK(ae_pgcr_decode(bytes, length, &out) == 1);
	CHECK(out.valid && out.slot == 77 && out.best_spree == in.best_spree && out.multikills == in.multikills);
	CHECK(out.headshots == in.headshots && out.killed_most == in.killed_most && out.killed_most_count == in.killed_most_count);
	CHECK(out.killed_by_most == 99 && out.killed_by_most_count == 120);
	CHECK(out.weapon_count == in.weapon_count && out.medal_count == in.medal_count);
	for (index = 0; index < in.weapon_count; index++)
		CHECK(out.weapons[index].tag == in.weapons[index].tag && out.weapons[index].kills == in.weapons[index].kills);
	for (index = 0; index < in.medal_count; index++)
		CHECK(out.medals[index].id == in.medals[index].id && out.medals[index].count == in.medals[index].count);
	/* the smallest */
	fresh();
	ae_pgcr_summarize(&game, 0, &in);
	length = ae_pgcr_encode(&in, bytes, sizeof(bytes));
	CHECK(length == 15 && ae_pgcr_decode(bytes, length, &out) && out.killed_most == AE_PGCR_NO_PLAYER);
	/* an invalid summary is not encoded; a small buffer is refused */
	memset(&in, 0, sizeof(in));
	CHECK(ae_pgcr_encode(&in, bytes, sizeof(bytes)) == 0);
	ae_pgcr_summarize(&game, 0, &in);
	CHECK(ae_pgcr_encode(&in, bytes, 10) == 0);
	/* bad bytes: every cut of a whole one, a wrong version, slot, count, medal, rival */
	fresh();
	ae_pgcr_medal(&game, 1, 3);
	ae_pgcr_kill(&game, 1, 2, 0, 9, 5);
	ae_pgcr_summarize(&game, 1, &in);
	length = ae_pgcr_encode(&in, bytes, sizeof(bytes));
	CHECK(ae_pgcr_decode(bytes, length, &out) == 1);
	for (index = 0; index < length; index++)
	{
		CHECK(ae_pgcr_decode(bytes, index, &out) == 0);
		CHECK(!out.valid);
	}
	{
		unsigned char bad[AE_PGCR_WIRE_MAXIMUM];

		memcpy(bad, bytes, length); bad[0] = 3;
		CHECK(ae_pgcr_decode(bad, length, &out) == 0);
		memcpy(bad, bytes, length); bad[1] = AE_PGCR_PLAYERS;
		CHECK(ae_pgcr_decode(bad, length, &out) == 0);
		memcpy(bad, bytes, length); bad[8] = AE_PGCR_PLAYERS;
		CHECK(ae_pgcr_decode(bad, length, &out) == 0);
		memcpy(bad, bytes, length); bad[12] = AE_PGCR_SUMMARY_WEAPONS + 1;
		CHECK(ae_pgcr_decode(bad, length, &out) == 0);
		memcpy(bad, bytes, length); bad[length - 6] = AE_MEDAL_COUNT;
		CHECK(ae_pgcr_decode(bad, length, &out) == 0);
		memcpy(bad, bytes, length); bad[length - 7] = AE_PGCR_SUMMARY_MEDALS + 1;
		CHECK(ae_pgcr_decode(bad, length, &out) == 0);
		memcpy(bad, bytes, length); bad[length - 4] = AE_PGCR_SUMMARY_VERSUS + 1;
		CHECK(ae_pgcr_decode(bad, length, &out) == 0);
		memcpy(bad, bytes, length); bad[length - 3] = AE_PGCR_PLAYERS;
		CHECK(ae_pgcr_decode(bad, length, &out) == 0);
	}
}

static void test_versus(void)
{
	struct ae_pgcr_summary in, out;
	unsigned char bytes[AE_PGCR_WIRE_MAXIMUM];
	int index, length;

	/* slot 1 against 2 (3 for 1), 3 (1 for 0, 2 against), 4 (none): the most traded first */
	fresh();
	for (index = 0; index < 3; index++)
		ae_pgcr_kill(&game, 1, 2, 0, 5, 100 + index * 500L);
	ae_pgcr_kill(&game, 2, 1, 0, 5, 9000);
	ae_pgcr_kill(&game, 3, 1, 0, 5, 9500);
	ae_pgcr_kill(&game, 3, 1, 0, 5, 9900);
	ae_pgcr_summarize(&game, 1, &in);
	CHECK(in.versus_count == 2);
	CHECK(in.versus[0].slot == 2 && in.versus[0].killed == 3 && in.versus[0].killed_by == 1);
	CHECK(in.versus[1].slot == 3 && in.versus[1].killed == 0 && in.versus[1].killed_by == 2);
	length = ae_pgcr_encode(&in, bytes, sizeof(bytes));
	CHECK(ae_pgcr_decode(bytes, length, &out) == 1 && out.versus_count == 2);
	CHECK(out.versus[0].slot == 2 && out.versus[0].killed == 3 && out.versus[1].killed_by == 2);
	/* only the eight most traded with are kept */
	fresh();
	for (index = 0; index < 12; index++)
	{
		int times;

		for (times = 0; times <= index; times++)
			ae_pgcr_kill(&game, 0, 10 + index, 0, 5, times * 500L + index * 100000L);
	}
	ae_pgcr_summarize(&game, 0, &in);
	CHECK(in.versus_count == AE_PGCR_SUMMARY_VERSUS && in.versus[0].slot == 21 && in.versus[7].slot == 14);
	length = ae_pgcr_encode(&in, bytes, sizeof(bytes));
	CHECK(length > 0 && length <= AE_PGCR_WIRE_MAXIMUM && ae_pgcr_decode(bytes, length, &out) && out.versus_count == 8);
	/* a version 1 summary (no versus list) is still read, with none */
	fresh();
	ae_pgcr_kill(&game, 1, 2, 0, 9, 5);
	ae_pgcr_summarize(&game, 1, &in);
	length = ae_pgcr_encode(&in, bytes, sizeof(bytes)) - 4;
	bytes[0] = 1;
	CHECK(ae_pgcr_decode(bytes, length, &out) == 1 && out.valid && out.versus_count == 0 && out.killed_most == 2);
}

static void test_data(void)
{
	struct ae_pgcr_summary summary;
	long number;

	ae_pgcr_data_reset();
	CHECK(!ae_pgcr_data_have_summary() && ae_pgcr_data_get(3) == NULL);
	fresh();
	ae_pgcr_kill(&game, 3, 4, 1, 7, 10);
	ae_pgcr_summarize(&game, 3, &summary);
	CHECK(ae_pgcr_data_set(&summary) == 1);
	CHECK(ae_pgcr_data_have_summary() && ae_pgcr_data_get(3) && ae_pgcr_data_get(3)->headshots == 1);
	CHECK(ae_pgcr_data_get(4) == NULL && ae_pgcr_data_get(-1) == NULL && ae_pgcr_data_get(AE_PGCR_PLAYERS) == NULL);
	number = ae_pgcr_data_report_number();
	ae_pgcr_summarize(&game, 4, &summary);
	CHECK(ae_pgcr_data_set(&summary) == 1 && ae_pgcr_data_report_number() == number);
	memset(&summary, 0, sizeof(summary));
	CHECK(ae_pgcr_data_set(&summary) == 0);
	/* the last game's stay until the next game starts */
	CHECK(ae_pgcr_data_get(3) != NULL);
	ae_pgcr_data_reset();
	CHECK(!ae_pgcr_data_have_summary() && ae_pgcr_data_get(3) == NULL);
	ae_pgcr_summarize(&game, 3, &summary);
	ae_pgcr_data_set(&summary);
	CHECK(ae_pgcr_data_report_number() == number + 1);
}

int main(void)
{
	test_accuracy();
	test_kills();
	test_medals();
	test_wire();
	test_versus();
	test_data();
	if (failures)
		printf("%d failures\n", failures);
	else
		printf("ae_pgcr_rules_test: all passed\n");
	return failures ? 1 : 0;
}
