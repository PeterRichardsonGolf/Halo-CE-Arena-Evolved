#include <stdio.h>
#include <string.h>
#include "ae_record_stats.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static struct ae_rs_input input(int mode)
{
	struct ae_rs_input in;

	memset(&in, 0, sizeof(in));
	in.mode = mode;
	in.fired = in.hit = -1;
	in.objective[0] = in.objective[1] = in.objective[2] = -1;
	in.laps = in.best_lap_ticks = in.ball_carrier_kills = in.as_ball_carrier = -1;
	return in;
}

static struct ae_pgcr_summary summary_v3(void)
{
	struct ae_pgcr_summary s;

	memset(&s, 0, sizeof(s));
	s.valid = 1;
	s.extended = 1;
	s.best_spree = 7;
	s.headshots = 9;
	s.damage_dealt = 11240;
	s.damage_taken = 9000;
	s.melee = 2; s.power = 3; s.grenade = 4; s.assassination = 1; s.splatter = 5; s.stick = 6;
	s.kills_as_flag_carrier = 2;
	s.flag_carrier_seconds = 41;
	s.bomb_plants = 3;
	s.bomb_defuses = 1;
	return s;
}

static void test_game_make(void)
{
	struct ae_rs_game game;
	struct ae_rs_input in = input(AE_RS_KIND_FLAG);
	struct ae_pgcr_summary s = summary_v3();

	/* a flag game with a version 3 summary */
	in.kills = 12; in.seconds = 600; in.fired = 300; in.hit = 120;
	in.objective[0] = 4; in.objective[1] = 2; in.objective[2] = 1;
	in.summary = &s;
	ae_rs_game_make(&game, &in);
	CHECK(game.have_v3 && !game.tied);
	CHECK(game.x.flag_grabs == 4 && game.x.flag_returns == 2 && game.x.flag_caps == 1);
	CHECK(game.x.fired == 300 && game.x.hit == 120 && game.x.seconds == 600 && game.x.best_spree == 7);
	CHECK(game.x.dealt == 11240 && game.x.taken == 9000 && game.x.headshots == 9 && game.x.melee == 2 && game.x.stick == 6);
	CHECK(game.x.as_carrier == 2 && game.x.carrier_seconds == 41 && game.x.bomb_plants == 3 && game.x.bomb_defuses == 1);
	CHECK(game.x.kills_v3 == 12 && game.x.seconds_v3 == 600 && game.x.hit_v3 == 120);
	/* the same flag numbers in another kind's game are not kept */
	in.mode = AE_RS_KIND_SLAYER;
	ae_rs_game_make(&game, &in);
	CHECK(game.x.flag_grabs == 0 && game.x.flag_caps == 0);
	/* hill and ball time are the score's clock; a ball's carrier kills; a race's laps */
	in = input(AE_RS_KIND_HILL); in.score_value = 3000;
	ae_rs_game_make(&game, &in);
	CHECK(game.x.hill_ticks == 3000 && game.x.ball_ticks == 0 && !game.have_v3);
	in = input(AE_RS_KIND_BALL); in.score_value = 900; in.ball_carrier_kills = 3; in.as_ball_carrier = 2;
	ae_rs_game_make(&game, &in);
	CHECK(game.x.ball_ticks == 900 && game.x.ball_carrier_kills == 3 && game.x.as_ball_carrier == 2 && game.x.hill_ticks == 0);
	in = input(AE_RS_KIND_RACE); in.laps = 5; in.best_lap_ticks = 1500;
	ae_rs_game_make(&game, &in);
	CHECK(game.x.laps == 5 && game.x.best_lap == 1500);
	/* a number not known (-1) is 0, never negative */
	in = input(AE_RS_KIND_RACE);
	ae_rs_game_make(&game, &in);
	CHECK(game.x.laps == 0 && game.x.best_lap == 0 && game.x.fired == 0 && game.x.seconds == 0);
	/* an older host's summary (version 2): spree is known, the version 3 numbers are not counted */
	s.extended = 0;
	s.damage_dealt = 0;
	in = input(AE_RS_KIND_SLAYER); in.summary = &s; in.kills = 5; in.seconds = 300;
	ae_rs_game_make(&game, &in);
	CHECK(!game.have_v3 && game.x.best_spree == 7 && game.x.dealt == 0 && game.x.headshots == 0 && game.x.kills_v3 == 0);
	/* no summary at all */
	in.summary = NULL;
	ae_rs_game_make(&game, &in);
	CHECK(!game.have_v3 && game.x.best_spree == 0);
	/* a tie */
	in.tied = 1; in.won = 0;
	ae_rs_game_make(&game, &in);
	CHECK(game.tied);
	/* a win is never also a tie, and a quit game is neither */
	in.won = 1;
	ae_rs_game_make(&game, &in);
	CHECK(!game.tied);
	in.won = 0; in.tied = 1; in.quit = 1;
	ae_rs_game_make(&game, &in);
	CHECK(!game.tied);
}

static void test_add(void)
{
	struct ae_rs_stats stats;
	struct ae_rs_game game;
	struct ae_rs_input in = input(AE_RS_KIND_RACE);
	struct ae_pgcr_summary s = summary_v3();

	memset(&stats, 0, sizeof(stats));
	in.seconds = 100; in.fired = 10; in.hit = 5; in.laps = 3; in.best_lap_ticks = 2000; in.kills = 4;
	in.summary = &s;
	ae_rs_game_make(&game, &in);
	ae_rs_add(&stats, &game);
	/* a second game: an older host's (no v3), a better lap, a lower spree */
	s.extended = 0; s.best_spree = 3;
	in.best_lap_ticks = 1800; in.laps = 2; in.seconds = 50; in.tied = 1; in.kills = 6; in.fired = 20; in.hit = 8;
	ae_rs_game_make(&game, &in);
	ae_rs_add(&stats, &game);
	CHECK(stats.counted == 2 && stats.counted_v3 == 1 && stats.ties == 1);
	CHECK(stats.seconds == 150 && stats.fired == 30 && stats.hit == 13 && stats.laps == 5);
	CHECK(stats.best_lap == 1800 && stats.best_spree == 7);
	CHECK(stats.dealt == 11240 && stats.kills_v3 == 4 && stats.seconds_v3 == 100 && stats.hit_v3 == 5);
	/* a worse lap does not replace the best; a lap time of 0 (none) never does */
	in.best_lap_ticks = 2500;
	ae_rs_game_make(&game, &in);
	ae_rs_add(&stats, &game);
	in.best_lap_ticks = 0;
	ae_rs_game_make(&game, &in);
	ae_rs_add(&stats, &game);
	CHECK(stats.best_lap == 1800 && stats.counted == 4);
}

static void test_weapons(void)
{
	struct ae_rs_weapon list[AE_RS_WEAPONS];
	int count = 0, index;
	char name[16];

	ae_rs_weapon_add(list, &count, AE_RS_WEAPONS, "battle rifle", 10);
	ae_rs_weapon_add(list, &count, AE_RS_WEAPONS, "sniper rifle", 20);
	ae_rs_weapon_add(list, &count, AE_RS_WEAPONS, "battle rifle", 15);
	CHECK(count == 2 && !strcmp(list[0].name, "battle rifle") && list[0].kills == 25 && list[1].kills == 20);
	/* a name with the file's separators is made safe, an empty one or no kills is no weapon */
	ae_rs_weapon_add(list, &count, AE_RS_WEAPONS, "a|b,c:d", 1);
	CHECK(count == 3 && !strcmp(list[2].name, "a?b?c?d"));
	ae_rs_weapon_add(list, &count, AE_RS_WEAPONS, "", 4);
	ae_rs_weapon_add(list, &count, AE_RS_WEAPONS, "pistol", 0);
	CHECK(count == 3);
	/* past the limit the fewest is replaced by a better one, a worse one is not kept */
	for (index = 0; index < 4; index++)
	{
		snprintf(name, sizeof(name), "w%d", index);
		ae_rs_weapon_add(list, &count, AE_RS_WEAPONS, name, 5 + index);
	}
	CHECK(count == AE_RS_WEAPONS);
	ae_rs_weapon_add(list, &count, AE_RS_WEAPONS, "weak", 1);
	for (index = 0; index < count; index++)
		CHECK(strcmp(list[index].name, "weak") != 0);
	ae_rs_weapon_add(list, &count, AE_RS_WEAPONS, "strong", 50);
	CHECK(count == AE_RS_WEAPONS && !strcmp(list[0].name, "strong"));
	for (index = 1; index < count; index++)
		CHECK(list[index - 1].kills >= list[index].kills);
}

static void test_maps(void)
{
	struct ae_rs_map list[AE_RS_MAPS];
	int count = 0, index, order[AE_RS_MAPS];

	ae_rs_map_add(list, &count, AE_RS_MAPS, "Blood Gulch", 1, 10, 4);
	ae_rs_map_add(list, &count, AE_RS_MAPS, "blood gulch", 0, 5, 6);
	ae_rs_map_add(list, &count, AE_RS_MAPS, "Prisoner", 1, 1, 1);
	ae_rs_map_add(list, &count, AE_RS_MAPS, "", 1, 1, 1);
	CHECK(count == 2 && list[0].games == 2 && list[0].wins == 1 && list[0].kills == 15 && list[0].deaths == 10);
	CHECK(ae_rs_map_order(list, count, order, 10) == 2 && order[0] == 0 && order[1] == 1);
	/* a full table: the map with the fewest games goes, the first of equals */
	count = 0;
	for (index = 0; index < 3; index++)
	{
		char name[16];

		snprintf(name, sizeof(name), "map%d", index);
		ae_rs_map_add(list, &count, 3, name, 1, 1, 0);
	}
	ae_rs_map_add(list, &count, 3, "map1", 1, 1, 0);
	ae_rs_map_add(list, &count, 3, "map2", 1, 1, 0);
	ae_rs_map_add(list, &count, 3, "new", 0, 2, 2);
	CHECK(count == 3 && !strcmp(list[0].name, "new") && list[0].games == 1 && list[0].wins == 0);
	CHECK(!strcmp(list[1].name, "map1") && list[1].games == 2);
	/* the order: most played, then most won, then by name; the most asked for caps it */
	ae_rs_map_add(list, &count, 3, "map2", 0, 0, 0);
	CHECK(ae_rs_map_order(list, count, order, 2) == 2);
	CHECK(ae_rs_map_order(list, count, order, 10) == 3 && !strcmp(list[order[0]].name, "map2") &&
		!strcmp(list[order[1]].name, "map1") && !strcmp(list[order[2]].name, "new"));
	CHECK(ae_rs_map_order(list, 0, order, 10) == 0);
}

static void test_file(void)
{
	struct ae_rs_stats a, b;
	char text[512], copy[1024];
	int index;
	long *cell = (long *)&a;

	memset(&a, 0, sizeof(a));
	for (index = 0; index < (int)(sizeof(a) / sizeof(long)); index++)
		cell[index] = 1000 + index * 7;
	CHECK(ae_rs_stats_write(&a, text, sizeof(text)) > 0 && strlen(text) < 400 && AE_RS_GROUPS == 8);
	snprintf(copy, sizeof(copy), "%s", text);
	CHECK(ae_rs_stats_read(copy, &b) == 1 && !memcmp(&a, &b, sizeof(a)));
	/* too small a buffer is refused */
	CHECK(ae_rs_stats_write(&a, text, 20) < 0);
	/* fewer groups (an older writer's): the rest 0; not numbers: refused */
	snprintf(copy, sizeof(copy), "1 2 3 | 4 5");
	CHECK(ae_rs_stats_read(copy, &b) == 1 && b.ties == 1 && b.best_spree == 2 && b.seconds == 3 && b.fired == 4 && b.hit == 5);
	CHECK(b.flag_caps == 0 && b.counted == 0 && b.dealt == 0);
	snprintf(copy, sizeof(copy), "1 two 3");
	CHECK(ae_rs_stats_read(copy, &b) == 0);
	snprintf(copy, sizeof(copy), "1 2 -3");
	CHECK(ae_rs_stats_read(copy, &b) == 0);
	/* more numbers than it knows (a newer writer's): ignored */
	snprintf(copy, sizeof(copy), "1 2 3 99 98 | 4 5 6 7 | 1");
	CHECK(ae_rs_stats_read(copy, &b) == 1 && b.seconds == 3 && b.fired == 4);
	CHECK(ae_rs_stats_read("", &b) == 1 && b.counted == 0);
	/* the field after the 8 groups */
	CHECK(ae_rs_stats_write(&a, text, sizeof(text)) > 0);
	snprintf(copy, sizeof(copy), "%s | battle rifle:4", text);
	CHECK(ae_rs_stats_read(copy, &b) == 1 && !memcmp(&a, &b, sizeof(a)));
	CHECK(!strcmp(ae_rs_after_groups(copy), "battle rifle:4"));
	CHECK(ae_rs_after_groups(text) == NULL);
	CHECK(ae_rs_after_groups("1 | 2") == NULL);
}

static void test_words(void)
{
	char text[32];

	ae_rs_number_text(11240, text, sizeof(text)); CHECK(!strcmp(text, "11,240"));
	ae_rs_number_text(999, text, sizeof(text)); CHECK(!strcmp(text, "999"));
	ae_rs_number_text(1000, text, sizeof(text)); CHECK(!strcmp(text, "1,000"));
	ae_rs_number_text(1234567, text, sizeof(text)); CHECK(!strcmp(text, "1,234,567"));
	ae_rs_number_text(0, text, sizeof(text)); CHECK(!strcmp(text, "0"));
	ae_rs_number_text(-1500, text, sizeof(text)); CHECK(!strcmp(text, "-1,500"));
	ae_rs_number_text(1234567, text, 6); CHECK(strlen(text) < 6);
	CHECK(ae_rs_ratio100(11240, 9000) == 125 && ae_rs_ratio100(5, 0) == 500 && ae_rs_ratio100(0, 0) == 0);
	ae_rs_ratio_text(125, text, sizeof(text)); CHECK(!strcmp(text, "1.25"));
	ae_rs_ratio_text(5, text, sizeof(text)); CHECK(!strcmp(text, "0.05"));
	CHECK(ae_rs_kda100(10, 6, 4) == 300 && ae_rs_kda100(10, 0, 0) == 1000 && ae_rs_kda100(0, 2, 3) == 22);
	CHECK(ae_rs_percent(1, 3) == 33 && ae_rs_percent(2, 3) == 67 && ae_rs_percent(5, 0) == 0);
	ae_rs_time_text(0, text, sizeof(text)); CHECK(!strcmp(text, "0M"));
	ae_rs_time_text(47 * 60 + 20, text, sizeof(text)); CHECK(!strcmp(text, "47M"));
	ae_rs_time_text(12 * 3600 + 4 * 60, text, sizeof(text)); CHECK(!strcmp(text, "12H 04M"));
	ae_rs_time_text(27 * 3600 + 3600, text, sizeof(text)); CHECK(!strcmp(text, "1D 4H"));
	ae_rs_clock_text(30 * 124, text, sizeof(text)); CHECK(!strcmp(text, "2:04"));
	ae_rs_clock_text(-5, text, sizeof(text)); CHECK(!strcmp(text, "0:00"));
}

static void test_demo(void)
{
	struct ae_rs_stats a, b;
	struct ae_rs_weapon weapons[AE_RS_WEAPONS], weapons_b[AE_RS_WEAPONS];
	struct ae_rs_map maps[AE_RS_MAPS], maps_b[AE_RS_MAPS];
	int weapon_count, weapon_count_b, map_count, map_count_b, index;
	long kind[AE_RS_KINDS][5], sum[5];

	/* every game counted: all the numbers there, in sane proportions; the same seed gives the same, another does not */
	ae_rs_demo(&a, weapons, &weapon_count, maps, &map_count, AE_RS_MAPS, 10, 40, 18, 600, 400, 120, 40, 40, 3);
	ae_rs_demo(&b, weapons_b, &weapon_count_b, maps_b, &map_count_b, AE_RS_MAPS, 10, 40, 18, 600, 400, 120, 40, 40, 3);
	CHECK(!memcmp(&a, &b, sizeof(a)) && weapon_count == weapon_count_b && map_count == map_count_b);
	CHECK(a.counted == 40 && a.counted_v3 == 40 && a.seconds > 0 && a.hit <= a.fired && a.fired > 0);
	CHECK(a.dealt > 0 && a.taken > 0 && a.headshots <= a.kills_v3 && a.kills_v3 == 600);
	CHECK(weapon_count == AE_RS_WEAPONS && map_count == 10);
	for (index = 1; index < weapon_count; index++)
		CHECK(weapons[index - 1].kills >= weapons[index].kills);
	{
		long played = 0;

		for (index = 0; index < map_count; index++)
		{
			played += maps[index].games;
			CHECK(maps[index].wins <= maps[index].games && maps[index].name[0]);
		}
		CHECK(played == 40);
	}
	ae_rs_demo(&b, weapons_b, &weapon_count_b, maps_b, &map_count_b, AE_RS_MAPS, 10, 40, 18, 600, 400, 120, 40, 40, 4);
	CHECK(memcmp(&a, &b, sizeof(a)) != 0);
	/* a record from before: few games counted, none with the version 3 numbers; the maps are limited */
	ae_rs_demo(&a, weapons, &weapon_count, maps, &map_count, 8, 12, 40, 18, 600, 400, 120, 5, 0, 3);
	CHECK(a.counted == 5 && a.counted_v3 == 0 && a.dealt == 0 && a.headshots == 0 && a.fired > 0 && map_count <= 5);
	CHECK(weapon_count >= 1 && weapons[0].kills <= 600 * 5 / 40);
	ae_rs_demo(&a, weapons, &weapon_count, maps, &map_count, 8, 12, 40, 18, 600, 400, 120, 0, 0, 3);
	CHECK(a.counted == 0 && a.seconds == 0 && map_count == 0 && weapon_count == 0 && a.flag_caps == 0);
	/* counts above the games are cut */
	ae_rs_demo(&a, weapons, &weapon_count, maps, &map_count, 8, 12, 4, 1, 6, 4, 1, 90, 99, 3);
	CHECK(a.counted == 4 && a.counted_v3 == 4);
	/* the kinds: every sum is exact, wins within a kind's games, a tiny record fits */
	ae_rs_demo_kinds(kind, 40, 18, 600, 400, 120, 5);
	memset(sum, 0, sizeof(sum));
	for (index = 0; index < AE_RS_KINDS; index++)
	{
		int field;

		for (field = 0; field < 5; field++)
			sum[field] += kind[index][field];
		CHECK(kind[index][1] <= kind[index][0]);
	}
	CHECK(sum[0] == 40 && sum[1] == 18 && sum[2] == 600 && sum[3] == 400 && sum[4] == 120);
	ae_rs_demo_kinds(kind, 1, 1, 5, 2, 0, 5);
	CHECK(kind[AE_RS_KIND_SLAYER][0] == 1 && kind[AE_RS_KIND_SLAYER][1] == 1 && kind[AE_RS_KIND_SLAYER][2] == 5);
	ae_rs_demo_kinds(kind, 0, 0, 0, 0, 0, 5);
	CHECK(kind[0][0] == 0);
	/* over every kind: each one played, the sums still exact; three is the usual split */
	ae_rs_demo_kinds_over(kind, 200, 100, 3000, 2000, 600, 5, AE_RS_KINDS);
	memset(sum, 0, sizeof(sum));
	for (index = 0; index < AE_RS_KINDS; index++)
	{
		int field;

		for (field = 0; field < 5; field++)
			sum[field] += kind[index][field];
		CHECK(kind[index][0] > 0 && kind[index][1] <= kind[index][0]);
	}
	CHECK(sum[0] == 200 && sum[1] == 100 && sum[2] == 3000 && sum[3] == 2000 && sum[4] == 600);
	CHECK(kind[AE_RS_KIND_SLAYER][0] > kind[AE_RS_KIND_OTHER][0]);
	{
		long usual[AE_RS_KINDS][5];

		ae_rs_demo_kinds(usual, 40, 18, 600, 400, 120, 5);
		ae_rs_demo_kinds_over(kind, 40, 18, 600, 400, 120, 5, 0);
		CHECK(!memcmp(usual, kind, sizeof(usual)));
	}
}

static void test_from_games(void)
{
	char text[32];
	struct ae_rs_stats s;

	/* the muted tail: only while the games that carry the numbers are fewer than the games */
	CHECK(ae_rs_from_games(3, 14, text, sizeof(text)) == 1 && !strcmp(text, "FROM 3 GAMES"));
	CHECK(ae_rs_from_games(1, 14, text, sizeof(text)) == 1 && !strcmp(text, "FROM 1 GAME"));
	CHECK(ae_rs_from_games(0, 14, text, sizeof(text)) == 1 && !strcmp(text, "FROM 0 GAMES"));
	CHECK(ae_rs_from_games(14, 14, text, sizeof(text)) == 0 && !text[0]);
	CHECK(ae_rs_from_games(20, 14, text, sizeof(text)) == 0 && !text[0]);
	CHECK(ae_rs_from_games(0, 0, text, sizeof(text)) == 0);
	/* a total shows a dash until a game is counted */
	memset(&s, 0, sizeof(s));
	CHECK(!ae_rs_known(&s) && !ae_rs_known_v3(&s));
	s.counted = 1;
	CHECK(ae_rs_known(&s) && !ae_rs_known_v3(&s));
	s.counted_v3 = 1;
	CHECK(ae_rs_known_v3(&s));
}

int main(void)
{
	test_game_make();
	test_add();
	test_weapons();
	test_maps();
	test_file();
	test_words();
	test_from_games();
	test_demo();
	if (failures)
		printf("%d failures\n", failures);
	else
		printf("ae_record_stats_test: all passed\n");
	return failures ? 1 : 0;
}
