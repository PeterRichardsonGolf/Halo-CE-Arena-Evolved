#include <stdio.h>
#include <string.h>
#include "ae_record_view.h"
#include "ae_record_focus.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
#define STR(a, b) do { if (strcmp((a), (b))) { printf("FAIL %s:%d %s = \"%s\", want \"%s\"\n", __FILE__, __LINE__, #a, (a), (b)); failures++; } } while (0)

static struct ae_rs_stats x;
static unsigned long medals[AE_MEDAL_COUNT];
static struct ae_rs_map maps[AE_RS_MAPS];
static struct ae_rs_weapon weapons[AE_RS_WEAPONS];
static struct ae_rv_in in;

/* the player of the design's mockup: 7 games, 107 kills, 23 deaths, 9 assists */
static void mockup(void)
{
	memset(&x, 0, sizeof(x));
	memset(medals, 0, sizeof(medals));
	memset(&in, 0, sizeof(in));
	in.games = 7; in.wins = 2; in.ties = 0; in.kills = 107; in.deaths = 23; in.assists = 9;
	x.counted = x.counted_v3 = 7;
	x.ties = 0; x.best_spree = 14; x.seconds = 5880;
	x.fired = 612; x.hit = 241;
	x.dealt = 11240; x.taken = 2310;
	x.headshots = 42; x.melee = 9; x.power = 22; x.grenade = 7; x.assassin = 2; x.splatter = 0; x.stick = 3;
	x.kills_v3 = 107; x.seconds_v3 = 5880; x.hit_v3 = 241;
	x.flag_caps = 2; x.flag_returns = 6; x.flag_grabs = 9; x.as_carrier = 4; x.ball_ticks = 30 * 48;
	in.x = &x;
	in.medals = medals;
	in.kind[AE_RS_KIND_SLAYER].games = 3; in.kind[AE_RS_KIND_SLAYER].wins = 2; in.kind[AE_RS_KIND_SLAYER].kills = 68;
	in.kind[AE_RS_KIND_SLAYER].deaths = 1;
	in.kind[AE_RS_KIND_FLAG].games = 3; in.kind[AE_RS_KIND_FLAG].kills = 37; in.kind[AE_RS_KIND_FLAG].deaths = 22;
	in.kind[AE_RS_KIND_FLAG].assists = 9;
	in.kind[AE_RS_KIND_BALL].games = 1; in.kind[AE_RS_KIND_BALL].kills = 2;
	in.maps = maps;
	in.weapons = weapons;
}

static struct ae_rv_cell cell_of(struct ae_rv_cell const *cells, char const *label)
{
	struct ae_rv_cell none;
	int index;

	for (index = 0; index < 16; index++)
		if (!strcmp(cells[index].label, label))
			return cells[index];
	memset(&none, 0, sizeof(none));
	snprintf(none.label, sizeof(none.label), "MISSING");
	return none;
}

static void test_totals(void)
{
	struct ae_rv_cell cells[AE_RV_TOTALS];
	int index;

	mockup();
	CHECK(ae_rv_totals(&in, cells) == AE_RV_TOTALS);
	STR(cells[0].label, "GAMES"); STR(cells[0].value, "7");
	STR(cell_of(cells, "WINS").value, "2");
	STR(cell_of(cells, "LOSSES").value, "5");
	STR(cell_of(cells, "TIES").value, "0");
	STR(cell_of(cells, "WIN RATE").value, "29%");
	STR(cell_of(cells, "BEST SPREE").value, "14");
	STR(cell_of(cells, "TIME PLAYED").value, "1H 38M");
	STR(cell_of(cells, "KILLS").value, "107");
	STR(cell_of(cells, "DEATHS").value, "23");
	STR(cell_of(cells, "KILL SPREAD").value, "+84");
	STR(cell_of(cells, "ASSISTS").value, "9");
	STR(cell_of(cells, "K/D").value, "4.65");
	STR(cell_of(cells, "KDA").value, "4.78");
	STR(cell_of(cells, "ACCURACY").value, "39%");
	STR(cell_of(cells, "DAMAGE RATIO").value, "4.87");
	STR(cell_of(cells, "MEDALS").value, "0");
	/* the first column's eight, then the second's */
	STR(cells[8].label, "KILLS"); STR(cells[15].label, "DAMAGE RATIO");
	/* the losses are what is left of the games; a negative spread; many ties */
	in.ties = 3; in.kills = 10; in.deaths = 40;
	CHECK(!strcmp(ae_rv_totals(&in, cells) ? cell_of(cells, "LOSSES").value : "", "2"));
	STR(cell_of(cells, "KILL SPREAD").value, "-30");
	in.ties = 9;
	ae_rv_totals(&in, cells);
	STR(cell_of(cells, "LOSSES").value, "0");
	/* a record from before the update: the counted numbers are the dash; the totals show */
	mockup();
	memset(&x, 0, sizeof(x));
	ae_rv_totals(&in, cells);
	STR(cell_of(cells, "GAMES").value, "7"); STR(cell_of(cells, "K/D").value, "4.65");
	for (index = 0; index < 16; index++)
	{
		int unknown = !strcmp(cells[index].label, "TIME PLAYED") || !strcmp(cells[index].label, "ACCURACY") ||
			!strcmp(cells[index].label, "DAMAGE RATIO") || !strcmp(cells[index].label, "BEST SPREE");

		CHECK(cells[index].dash == unknown);
		if (unknown)
			STR(cells[index].value, AE_RV_DASH);
	}
	/* some games counted, none by a host that counts damage: the ratio is still a dash, the time is not */
	x.counted = 2; x.seconds = 600; x.fired = 10; x.hit = 5;
	ae_rv_totals(&in, cells);
	STR(cell_of(cells, "TIME PLAYED").value, "10M"); STR(cell_of(cells, "ACCURACY").value, "50%");
	STR(cell_of(cells, "DAMAGE RATIO").value, AE_RV_DASH);
	/* no deaths: the ratios are the kills; no games: dashes all through; the damage ratio with nothing taken */
	mockup();
	in.deaths = 0;
	ae_rv_totals(&in, cells);
	STR(cell_of(cells, "K/D").value, "107.00");
	/* nothing taken: no ratio to speak of, the dash */
	x.taken = 0;
	ae_rv_totals(&in, cells);
	STR(cell_of(cells, "DAMAGE RATIO").value, AE_RV_DASH);
	memset(&in, 0, sizeof(in));
	memset(&x, 0, sizeof(x));
	in.x = &x;
	in.medals = medals;
	ae_rv_totals(&in, cells);
	for (index = 0; index < 16; index++)
		CHECK(cells[index].dash);
	/* a bot's best spree is its record's, known with a game */
	mockup();
	memset(&x, 0, sizeof(x));
	in.spree_from_record = 1;
	in.best_spree = 9;
	ae_rv_totals(&in, cells);
	STR(cell_of(cells, "BEST SPREE").value, "9");
	/* medals added, with separators */
	medals[3] = 1200; medals[5] = 34;
	ae_rv_totals(&in, cells);
	STR(cell_of(cells, "MEDALS").value, "1,234");
}

static void test_combat(void)
{
	struct ae_rv_cell cells[8];
	struct ae_rv_share shares[AE_RV_KILL_KINDS];
	char tail[AE_RV_TAIL];

	mockup();
	CHECK(ae_rv_accuracy(&in, cells, tail) == AE_RV_ACCURACY);
	STR(cells[0].value, "612"); STR(cells[1].value, "241"); STR(cells[2].value, "371"); STR(cells[3].value, "39%");
	STR(cells[4].value, "42"); STR(cells[5].value, "39%");
	STR(tail, "");
	CHECK(ae_rv_kill_kinds(&in, shares, tail) == AE_RV_KILL_KINDS);
	STR(shares[0].label, "HEADSHOTS"); STR(shares[0].count, "42"); STR(shares[0].share, "39%");
	STR(shares[1].label, "MELEE"); STR(shares[1].count, "9"); STR(shares[1].share, "8%");
	STR(shares[2].label, "POWER WEAPONS"); STR(shares[2].count, "22"); STR(shares[2].share, "21%");
	STR(shares[3].count, "7"); STR(shares[3].share, "7%");
	STR(shares[5].label, "SPLATTERS"); STR(shares[5].count, "0"); STR(shares[5].share, AE_RV_DASH);
	STR(shares[6].label, "STICKS"); STR(shares[6].share, "3%");
	/* the assists row: from the medals (wingman, driver, protector) */
	medals[AE_MEDAL_WINGMAN] = 2; medals[AE_MEDAL_DRIVER] = 1; medals[AE_MEDAL_PROTECTOR] = 3;
	CHECK(ae_rv_assists(&in, cells) == AE_RV_ASSISTS);
	STR(cells[0].value, "9"); STR(cells[1].value, "1.3"); STR(cells[2].label, "WINGMAN"); STR(cells[2].value, "2");
	STR(cells[3].label, "DRIVER ASSISTS"); STR(cells[3].value, "1"); STR(cells[4].label, "PROTECTOR"); STR(cells[4].value, "3");
	/* damage: whole, separators; per minute one decimal; per hit; the ratio two */
	CHECK(ae_rv_damage(&in, cells, tail) == AE_RV_DAMAGE);
	STR(cells[0].label, "DAMAGE DEALT"); STR(cells[0].value, "11,240"); STR(cells[1].value, "2,310"); STR(cells[2].value, "4.87");
	STR(cells[3].label, "DEALT PER MINUTE"); STR(cells[3].value, "114.7");
	STR(cells[4].value, "23.6"); STR(cells[5].label, "DEALT PER HIT"); STR(cells[5].value, "46.6");
	CHECK(ae_rv_per_game(&in, cells, tail) == AE_RV_PER_GAME);
	STR(cells[0].value, "15.3"); STR(cells[1].value, "3.3"); STR(cells[2].value, "1.3"); STR(cells[3].label, "DAMAGE DEALT");
	STR(cells[3].value, "1,606");
	/* a record from before the update: every cell the dash, the tails say how many games count; the kill kinds are dashes */
	x.counted = 2; x.counted_v3 = 0;
	memset(&x.dealt, 0, sizeof(long) * 9);
	x.headshots = x.melee = x.dealt = x.taken = x.kills_v3 = x.seconds_v3 = x.hit_v3 = 0;
	ae_rv_kill_kinds(&in, shares, tail);
	STR(tail, "FROM 0 GAMES"); STR(shares[0].count, AE_RV_DASH); STR(shares[0].share, AE_RV_DASH);
	ae_rv_damage(&in, cells, tail);
	STR(tail, "FROM 0 GAMES"); STR(cells[0].value, AE_RV_DASH); STR(cells[2].value, AE_RV_DASH); STR(cells[5].value, AE_RV_DASH);
	ae_rv_per_game(&in, cells, tail);
	STR(cells[3].value, AE_RV_DASH); STR(cells[0].value, "15.3");
	ae_rv_accuracy(&in, cells, tail);
	STR(tail, "FROM 2 GAMES"); STR(cells[0].value, "612"); STR(cells[4].value, AE_RV_DASH); STR(cells[5].value, AE_RV_DASH);
	/* nothing counted at all: the shots are the dash too, not 0 */
	x.counted = 0;
	ae_rv_accuracy(&in, cells, tail);
	STR(cells[0].value, AE_RV_DASH); STR(cells[3].value, AE_RV_DASH); STR(tail, "FROM 0 GAMES");
	/* the kills of the games with the kill kinds are the share's whole, not the career's */
	mockup();
	in.kills = 500;
	x.kills_v3 = 100; x.headshots = 25;
	ae_rv_kill_kinds(&in, shares, tail);
	STR(shares[0].share, "25%");
	/* damage taken 0: no ratio (the dash), the rest as it is */
	mockup();
	x.taken = 0;
	ae_rv_damage(&in, cells, tail);
	STR(cells[2].value, AE_RV_DASH); STR(cells[0].value, "11,240"); STR(cells[4].value, "0.0");
}

static void test_modes(void)
{
	struct ae_rv_cell cells[AE_RV_OBJECTIVES];
	struct ae_rv_kind_row kinds[AE_RS_KINDS];
	struct ae_rv_map_row rows[AE_RV_MAPS];
	struct ae_rv_cell list[AE_RS_WEAPONS];
	char tail[AE_RV_TAIL];
	int more, index;

	mockup();
	medals[AE_MEDAL_FLAG_CARRIER_KILL] = 3;
	CHECK(ae_rv_objectives(&in, cells, tail) == AE_RV_OBJECTIVES);
	STR(cells[0].label, "FLAG CAPTURES"); STR(cells[0].value, "2");
	STR(cells[1].label, "FLAG RETURNS"); STR(cells[1].value, "6");
	STR(cells[2].label, "FLAG GRABS"); STR(cells[2].value, "9");
	STR(cells[3].label, "KILLS AS FLAG CARRIER"); STR(cells[3].value, "4");
	STR(cells[4].label, "FLAG CARRIERS KILLED"); STR(cells[4].value, "3");
	/* hill never played: the dash; the ball's clock; bomb and race never played: the dash */
	STR(cells[5].label, "HILL TIME"); STR(cells[5].value, AE_RV_DASH);
	STR(cells[6].label, "BALL TIME"); STR(cells[6].value, "0:48");
	STR(cells[7].label, "BOMB PLANTS"); STR(cells[7].value, AE_RV_DASH);
	STR(cells[8].label, "BOMB DEFUSES"); STR(cells[8].value, AE_RV_DASH);
	STR(cells[9].label, "RACE LAPS"); STR(cells[9].value, AE_RV_DASH);
	/* a kind played: its numbers show, a zero too */
	in.kind[AE_RS_KIND_BOMB].games = 2; in.kind[AE_RS_KIND_RACE].games = 1; in.kind[AE_RS_KIND_HILL].games = 1;
	x.bomb_plants = 4; x.laps = 5; x.hill_ticks = 30 * 125;
	ae_rv_objectives(&in, cells, tail);
	STR(cells[7].value, "4"); STR(cells[8].value, "0"); STR(cells[9].value, "5"); STR(cells[5].value, "2:05");
	/* no flag game: the flag rows are the dash */
	in.kind[AE_RS_KIND_FLAG].games = 0;
	ae_rv_objectives(&in, cells, tail);
	for (index = 0; index < 5; index++)
		STR(cells[index].value, AE_RV_DASH);
	/* the version 3 numbers wait for a counting host: kills as carrier, plants */
	mockup();
	in.kind[AE_RS_KIND_BOMB].games = 2;
	x.counted_v3 = 0;
	ae_rv_objectives(&in, cells, tail);
	STR(cells[3].value, AE_RV_DASH); STR(cells[7].value, AE_RV_DASH); STR(cells[0].value, "2");
	/* the tail */
	x.counted = 3;
	ae_rv_objectives(&in, cells, tail);
	STR(tail, "FROM 3 GAMES");
	/* BY KIND: the kinds with games, in order, K/D two decimals */
	in.kind[AE_RS_KIND_BOMB].games = 0;
	CHECK(ae_rv_kinds(&in, kinds) == 3);
	STR(kinds[0].name, "SLAYER"); STR(kinds[0].games, "3"); STR(kinds[0].wins, "2"); STR(kinds[0].kills, "68"); STR(kinds[0].kd, "68.00");
	STR(kinds[1].name, "FLAG"); STR(kinds[1].assists, "9"); STR(kinds[1].kd, "1.68");
	STR(kinds[2].name, "BALL"); STR(kinds[2].kd, "2.00");
	/* BY MAP */
	{
		int at = 0;

		memset(maps, 0, sizeof(maps));
		snprintf(maps[0].name, sizeof(maps[0].name), "Prisoner"); maps[0].games = 2; maps[0].wins = 1; maps[0].kills = 31; maps[0].deaths = 10;
		snprintf(maps[1].name, sizeof(maps[1].name), "Blood Gulch"); maps[1].games = 3; maps[1].wins = 1; maps[1].kills = 75; maps[1].deaths = 10;
		for (at = 2; at < 14; at++)
		{
			snprintf(maps[at].name, sizeof(maps[at].name), "m%02d", at);
			maps[at].games = 1;
		}
		in.map_count = 14;
	}
	x.counted = 7;
	CHECK(ae_rv_maps(&in, rows, &more, tail) == AE_RV_MAPS && more == 4);
	STR(rows[0].name, "BLOOD GULCH"); STR(rows[0].games, "3"); STR(rows[0].wins, "1"); STR(rows[0].kd, "7.50");
	STR(rows[1].name, "PRISONER"); STR(rows[1].kd, "3.10");
	STR(tail, "");
	in.map_count = 2;
	CHECK(ae_rv_maps(&in, rows, &more, tail) == 2 && more == 0);
	/* TOP WEAPONS */
	memset(weapons, 0, sizeof(weapons));
	snprintf(weapons[0].name, sizeof(weapons[0].name), "battle rifle"); weapons[0].kills = 41;
	snprintf(weapons[1].name, sizeof(weapons[1].name), "sniper rifle"); weapons[1].kills = 2200;
	in.weapon_count = 2;
	CHECK(ae_rv_weapons(&in, list, tail) == 2);
	STR(list[0].label, "BATTLE RIFLE"); STR(list[0].value, "41"); STR(list[1].value, "2,200");
	in.weapon_count = 0;
	CHECK(ae_rv_weapons(&in, list, tail) == 0);
}

static void test_highlights_last(void)
{
	struct ae_rv_cell cells[AE_RV_HIGHLIGHTS], last_cells[AE_RV_LAST];
	struct ae_sr_last last;
	char map[48], kind[16];

	mockup();
	snprintf(weapons[0].name, sizeof(weapons[0].name), "battle rifle"); weapons[0].kills = 41;
	in.weapon_count = 1;
	snprintf(maps[0].name, sizeof(maps[0].name), "Prisoner"); maps[0].games = 5; maps[0].wins = 0;
	snprintf(maps[1].name, sizeof(maps[1].name), "Blood Gulch"); maps[1].games = 2; maps[1].wins = 2;
	in.map_count = 2;
	medals[1] = 5; medals[2] = 1;
	CHECK(ae_rv_highlights(&in, 3, cells) == AE_RV_HIGHLIGHTS);
	STR(cells[0].label, "MOST KILLS WITH"); STR(cells[0].value, "BATTLE RIFLE");
	/* (slayer and flag have 3 games each: the kills break it) */
	STR(cells[1].label, "FAVOURITE KIND"); STR(cells[1].value, "SLAYER");
	STR(cells[2].label, "BEST MAP"); STR(cells[2].value, "BLOOD GULCH");
	STR(cells[3].label, "MEDAL KINDS"); STR(cells[3].value, "2");
	STR(cells[4].label, "RIVALS SET"); STR(cells[4].value, "3");
	/* nothing yet: dashes where nothing is known, 0 where it is a count */
	memset(&in, 0, sizeof(in));
	in.medals = medals;
	memset(medals, 0, sizeof(medals));
	ae_rv_highlights(&in, 0, cells);
	STR(cells[0].value, AE_RV_DASH); STR(cells[1].value, AE_RV_DASH); STR(cells[2].value, AE_RV_DASH);
	STR(cells[3].value, "0"); STR(cells[4].value, "0");
	/* the last game */
	memset(&last, 0, sizeof(last));
	last.has = 1; last.mode = AE_RS_KIND_FLAG; snprintf(last.map, sizeof(last.map), "Prisoner"); last.result = 1;
	last.kills = 11; last.deaths = 7; last.assists = 3; last.spree = 5; last.medals = 31;
	snprintf(last.weapon, sizeof(last.weapon), "battle rifle");
	CHECK(ae_rv_last_game(&last, last_cells, map, kind, sizeof(map)) == AE_RV_LAST);
	STR(last_cells[0].label, "RESULT"); STR(last_cells[0].value, "LOSS"); STR(last_cells[1].value, "11");
	STR(last_cells[4].label, "K/D"); STR(last_cells[4].value, "1.57"); STR(last_cells[5].value, "5");
	STR(last_cells[6].label, "MOST KILLS WITH"); STR(last_cells[6].value, "BATTLE RIFLE");
	STR(last_cells[7].label, "MEDALS"); STR(last_cells[7].value, "31");
	STR(map, "PRISONER"); STR(kind, "FLAG");
	last.result = 0; last.weapon[0] = 0;
	ae_rv_last_game(&last, last_cells, map, kind, sizeof(map));
	STR(last_cells[0].value, "WIN"); STR(last_cells[6].value, AE_RV_DASH);
	last.result = 2;
	ae_rv_last_game(&last, last_cells, map, kind, sizeof(map));
	STR(last_cells[0].value, "TIE");
	/* no last game: all dashes, no head words */
	last.has = 0;
	ae_rv_last_game(&last, last_cells, map, kind, sizeof(map));
	CHECK(last_cells[0].dash && last_cells[7].dash && !map[0] && !kind[0]);
	CHECK(ae_rv_medal_kinds(medals) == 0 && ae_rv_medal_total(medals) == 0);
}

static void test_details(void)
{
	static struct ae_rv_table table;
	struct ae_sr_last last;

	mockup();
	memset(weapons, 0, sizeof(weapons));
	snprintf(weapons[0].name, sizeof(weapons[0].name), "battle rifle"); weapons[0].kills = 75;
	snprintf(weapons[1].name, sizeof(weapons[1].name), "sniper rifle"); weapons[1].kills = 25;
	in.weapon_count = 2;
	CHECK(ae_rv_details(AE_RF_WEAPONS, &in, NULL, &table) == 1 && table.rows == 2 && table.columns == 2);
	/* (every game counted: the share is of all the record's kills) */
	STR(table.title, "TOP WEAPONS"); STR(table.row[0].name, "BATTLE RIFLE"); STR(table.row[0].value[0], "75"); STR(table.head[1], "SHARE");
	CHECK(in.x->counted == in.games);
	{
		char want[16];

		snprintf(want, sizeof(want), "%ld%%", ae_rs_percent(75, in.kills));
		STR(table.row[0].value[1], want);
	}
	/* (fewer games counted than kept: a share of the kept weapons, and the head says so) */
	{
		struct ae_rs_stats partial = *in.x;
		struct ae_rs_stats const *was = in.x;

		partial.counted = in.games - 1;
		in.x = &partial;
		CHECK(ae_rv_details(AE_RF_WEAPONS, &in, NULL, &table) == 1);
		STR(table.head[1], "OF TOP 5"); STR(table.row[0].value[1], "75%"); STR(table.row[1].value[1], "25%");
		in.x = was;
	}
	/* the maps: all of them, win rate and K/D */
	memset(maps, 0, sizeof(maps));
	snprintf(maps[0].name, sizeof(maps[0].name), "prisoner"); maps[0].games = 4; maps[0].wins = 1; maps[0].kills = 40; maps[0].deaths = 10;
	in.map_count = 1;
	CHECK(ae_rv_details(AE_RF_MAPS, &in, NULL, &table) && table.rows == 1 && table.columns == 6);
	STR(table.row[0].name, "PRISONER"); STR(table.row[0].value[2], "25%"); STR(table.row[0].value[5], "4.00");
	/* the kinds */
	CHECK(ae_rv_details(AE_RF_KINDS, &in, NULL, &table) && table.rows == 3);
	STR(table.row[0].name, "SLAYER"); STR(table.row[0].value[2], "67%");
	/* accuracy: totals and per game; the accuracy's per game is the dash */
	CHECK(ae_rv_details(AE_RF_ACCURACY, &in, NULL, &table) && table.rows == 6);
	STR(table.row[0].name, "SHOTS FIRED"); STR(table.row[0].value[0], "612"); STR(table.row[0].value[1], "87.4");
	STR(table.row[3].name, "ACCURACY"); STR(table.row[3].value[0], "39%"); STR(table.row[3].value[1], AE_RV_DASH);
	/* damage: per game and per minute */
	CHECK(ae_rv_details(AE_RF_DAMAGE, &in, NULL, &table) && table.rows == 4);
	STR(table.row[0].value[0], "11,240"); STR(table.row[0].value[1], "1,605.7"); STR(table.row[0].value[2], "114.7");
	/* a record from before: the cells are dashes, the tail says how many */
	x.counted = 2; x.counted_v3 = 0;
	CHECK(ae_rv_details(AE_RF_DAMAGE, &in, NULL, &table) && !strcmp(table.tail, "FROM 0 GAMES") && !strcmp(table.row[0].value[0], AE_RV_DASH));
	/* the last game */
	memset(&last, 0, sizeof(last));
	CHECK(!ae_rv_details(AE_RF_LAST, &in, &last, &table) && !ae_rv_has_details(AE_RF_LAST, &in, &last) && !ae_rv_has_details(AE_RF_LAST, &in, NULL));
	last.has = 1; last.mode = AE_RS_KIND_FLAG; snprintf(last.map, sizeof(last.map), "Prisoner"); last.kills = 3;
	CHECK(ae_rv_details(AE_RF_LAST, &in, &last, &table) && table.rows == 10);
	STR(table.row[0].name, "MAP"); STR(table.row[0].value[0], "PRISONER"); STR(table.row[1].value[0], "FLAG");
	/* nothing to show: no table, no stop (no maps, no weapons, no kinds, no games); the versus box is the bot screen's */
	in.map_count = 0; in.weapon_count = 0;
	CHECK(!ae_rv_has_details(AE_RF_MAPS, &in, NULL) && !ae_rv_has_details(AE_RF_WEAPONS, &in, NULL) && ae_rv_has_details(AE_RF_DAMAGE, &in, NULL));
	CHECK(!ae_rv_details(AE_RF_MAPS, &in, NULL, &table) && table.rows == 0);
	memset(&in.kind, 0, sizeof(in.kind));
	CHECK(!ae_rv_has_details(AE_RF_KINDS, &in, NULL) && !ae_rv_has_details(AE_RF_VERSUS, &in, NULL));
	in.games = 0;
	CHECK(!ae_rv_has_details(AE_RF_ACCURACY, &in, NULL) && !ae_rv_has_details(AE_RF_MEDALS, &in, NULL));
	medals[AE_MEDAL_WINGMAN] = 1;
	CHECK(ae_rv_has_details(AE_RF_MEDALS, &in, NULL) && !ae_rv_details(AE_RF_MEDALS, &in, NULL, &table));
	/* objectives: the extras follow the ten; the kind's games give the per game */
	mockup();
	CHECK(ae_rv_details(AE_RF_OBJECTIVES, &in, NULL, &table) && table.rows == 14);
	STR(table.row[0].name, "FLAG CAPTURES"); STR(table.row[0].value[0], "2"); STR(table.row[0].value[1], "0.7");
	STR(table.row[10].name, "SECONDS AS FLAG CARRIER");
}

int main(void)
{
	test_details();
	test_totals();
	test_combat();
	test_modes();
	test_highlights_last();
	if (failures)
		printf("%d failures\n", failures);
	else
		printf("ae_record_view_test: all passed\n");
	return failures ? 1 : 0;
}
