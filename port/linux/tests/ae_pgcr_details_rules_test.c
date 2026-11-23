#include <stdio.h>
#include <string.h>
#include "ae_pgcr_details_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
#define STR(a, b) do { if (strcmp((a), (b))) { printf("FAIL %s:%d %s = \"%s\", want \"%s\"\n", __FILE__, __LINE__, #a, (a), (b)); failures++; } } while (0)

static struct ae_pgr_report report;
static struct ae_rv_cell cells[16];

/* a player of the mockup's DETAILS page: 17 kills, 11 deaths, 6 assists, 120 shots, 37 landed */
static void mockup_row(struct ae_pgr_row *row)
{
	memset(row, 0, sizeof(*row));
	snprintf(row->name, sizeof(row->name), "Rook7");
	row->kills = 17; row->deaths = 11; row->assists = 6;
	row->shots_fired = 120; row->shots_hit = 37;
	row->objective[0] = row->objective[1] = row->objective[2] = AE_PGR_UNKNOWN;
	row->laps = row->best_lap = row->ball_carrier_kills = row->as_ball_carrier = AE_PGR_UNKNOWN;
	row->summary.valid = 1;
	row->summary.extended = 1;
	row->summary.best_spree = 5;
	row->summary.multikills = 2;
	row->summary.headshots = 4; row->summary.melee = 2; row->summary.power = 1; row->summary.grenade = 1;
	row->summary.assassination = 0; row->summary.splatter = 0; row->summary.stick = 1;
	row->summary.damage_dealt = 1980; row->summary.damage_taken = 1440;
	row->summary.kills_as_flag_carrier = 1;
	row->summary.bomb_plants = 2; row->summary.bomb_defuses = 1;
}

static void test_player(void)
{
	struct ae_pgr_row row;

	mockup_row(&row);
	CHECK(ae_pgd_this_game(&row, cells) == AE_PGD_THIS_GAME);
	STR(cells[0].label, "KILLS"); STR(cells[0].value, "17"); STR(cells[1].value, "11");
	STR(cells[2].label, "KILL SPREAD"); STR(cells[2].value, "+6"); STR(cells[3].value, "6");
	STR(cells[4].label, "K/D"); STR(cells[4].value, "1.55");
	STR(cells[5].label, "KDA"); STR(cells[5].value, "1.73");
	STR(cells[6].label, "BEST SPREE"); STR(cells[6].value, "5");
	STR(cells[7].label, "MULTIKILLS"); STR(cells[7].value, "2");
	CHECK(ae_pgd_accuracy_damage(&row, cells) == AE_PGD_ACCURACY);
	STR(cells[0].label, "SHOTS FIRED"); STR(cells[0].value, "120"); STR(cells[1].value, "37"); STR(cells[2].value, "83");
	STR(cells[3].label, "ACCURACY"); STR(cells[3].value, "31%"); STR(cells[4].label, "HEADSHOT KILLS"); STR(cells[4].value, "4");
	STR(cells[5].label, "DAMAGE DEALT"); STR(cells[5].value, "1,980"); STR(cells[6].value, "1,440");
	STR(cells[7].label, "DAMAGE RATIO"); STR(cells[7].value, "1.38");
	STR(cells[8].label, "DEALT PER HIT"); STR(cells[8].value, "53.5");
	/* (1440 over the 11 deaths) */
	STR(cells[9].label, "TAKEN PER LIFE"); STR(cells[9].value, "130.9");
	CHECK(ae_pgd_kill_kinds(&row, cells) == AE_PGD_KINDS);
	STR(cells[0].label, "HEADSHOTS"); STR(cells[0].value, "4"); STR(cells[1].value, "2"); STR(cells[2].label, "POWER WEAPONS");
	STR(cells[5].label, "SPLATTERS"); STR(cells[5].value, "0"); STR(cells[6].value, "1");
	/* no summary (a host that is not AE): what the game's own statistics have, the rest the dash */
	row.summary.valid = 0;
	ae_pgd_this_game(&row, cells);
	STR(cells[0].value, "17"); STR(cells[6].value, AE_RV_DASH); STR(cells[7].value, AE_RV_DASH);
	ae_pgd_accuracy_damage(&row, cells);
	STR(cells[0].value, "120"); STR(cells[4].value, AE_RV_DASH); STR(cells[5].value, AE_RV_DASH); STR(cells[7].value, AE_RV_DASH);
	ae_pgd_kill_kinds(&row, cells);
	STR(cells[0].value, AE_RV_DASH); STR(cells[6].value, AE_RV_DASH);
	/* an older host's summary (version 2): the spree and multikills are there, the version 3 numbers not */
	mockup_row(&row);
	row.summary.extended = 0;
	ae_pgd_this_game(&row, cells);
	STR(cells[6].value, "5");
	ae_pgd_accuracy_damage(&row, cells);
	STR(cells[4].value, AE_RV_DASH); STR(cells[5].value, AE_RV_DASH); STR(cells[8].value, AE_RV_DASH); STR(cells[3].value, "31%");
	ae_pgd_kill_kinds(&row, cells);
	STR(cells[0].value, AE_RV_DASH);
	/* shots not known (the engine's statistic missing): the dash, not 0 */
	mockup_row(&row);
	row.shots_fired = AE_PGR_UNKNOWN; row.shots_hit = AE_PGR_UNKNOWN;
	ae_pgd_accuracy_damage(&row, cells);
	STR(cells[0].value, AE_RV_DASH); STR(cells[1].value, AE_RV_DASH); STR(cells[3].value, AE_RV_DASH); STR(cells[8].value, AE_RV_DASH);
	/* nothing taken: no ratio; no deaths: the K/D is the kills */
	mockup_row(&row);
	row.summary.damage_taken = 0; row.deaths = 0;
	ae_pgd_accuracy_damage(&row, cells);
	STR(cells[7].value, AE_RV_DASH); STR(cells[9].value, "0.0");
	ae_pgd_this_game(&row, cells);
	STR(cells[4].value, "17.00");
}

static void test_assists_mode(void)
{
	struct ae_pgr_row row;
	char title[AE_PGD_TITLE];

	mockup_row(&row);
	row.summary.medal_count = 3;
	row.summary.medals[0].id = AE_MEDAL_WINGMAN; row.summary.medals[0].count = 1;
	row.summary.medals[1].id = AE_MEDAL_PROTECTOR; row.summary.medals[1].count = 3;
	row.summary.medals[2].id = AE_MEDAL_FLAG_CARRIER_KILL; row.summary.medals[2].count = 2;
	CHECK(ae_pgd_assists(&row, cells) == AE_PGD_ASSISTS);
	STR(cells[0].label, "ASSISTS"); STR(cells[0].value, "6"); STR(cells[1].label, "WINGMAN"); STR(cells[1].value, "1");
	STR(cells[2].label, "DRIVER ASSISTS"); STR(cells[2].value, "0"); STR(cells[3].label, "PROTECTOR"); STR(cells[3].value, "3");
	row.summary.valid = 0;
	ae_pgd_assists(&row, cells);
	STR(cells[0].value, "6"); STR(cells[1].value, AE_RV_DASH); STR(cells[3].value, AE_RV_DASH);
	/* FLAG */
	mockup_row(&row);
	row.summary.medal_count = 1;
	row.summary.medals[0].id = AE_MEDAL_FLAG_CARRIER_KILL; row.summary.medals[0].count = 2;
	memset(&report, 0, sizeof(report));
	report.valid = 1;
	report.mode = AE_PGR_MODE_FLAG;
	row.objective[0] = 3; row.objective[1] = 2; row.objective[2] = 1;
	CHECK(ae_pgd_mode_block(&report, &row, title, cells) == 5);
	STR(title, "FLAG");
	STR(cells[0].label, "CAPTURES"); STR(cells[0].value, "1"); STR(cells[1].label, "RETURNS"); STR(cells[1].value, "2");
	STR(cells[2].label, "GRABS"); STR(cells[2].value, "3"); STR(cells[3].label, "KILLS AS CARRIER"); STR(cells[3].value, "1");
	STR(cells[4].label, "CARRIERS KILLED"); STR(cells[4].value, "2");
	/* a flag number the engine did not give: the dash */
	row.objective[1] = AE_PGR_UNKNOWN;
	ae_pgd_mode_block(&report, &row, title, cells);
	STR(cells[1].value, AE_RV_DASH);
	/* BALL: the clock, the carrier numbers */
	report.mode = AE_PGR_MODE_BALL;
	row.score_value = 30 * 125; row.ball_carrier_kills = 3; row.as_ball_carrier = 1;
	CHECK(ae_pgd_mode_block(&report, &row, title, cells) == 3);
	STR(title, "BALL"); STR(cells[0].label, "BALL TIME"); STR(cells[0].value, "2:05"); STR(cells[1].value, "3"); STR(cells[2].value, "1");
	/* HILL */
	report.mode = AE_PGR_MODE_HILL;
	CHECK(ae_pgd_mode_block(&report, &row, title, cells) == 1);
	STR(title, "HILL"); STR(cells[0].label, "HILL TIME"); STR(cells[0].value, "2:05");
	/* BOMB: pickups and scores from the game's statistics, plants and defuses from the summary */
	report.mode = AE_PGR_MODE_BOMB;
	row.objective[0] = 4; row.objective[2] = 2;
	CHECK(ae_pgd_mode_block(&report, &row, title, cells) == 4);
	STR(title, "BOMB"); STR(cells[0].label, "PLANTS"); STR(cells[0].value, "2"); STR(cells[1].label, "DEFUSES"); STR(cells[1].value, "1");
	STR(cells[2].label, "PICKUPS"); STR(cells[2].value, "4"); STR(cells[3].label, "SCORES"); STR(cells[3].value, "2");
	row.summary.extended = 0;
	ae_pgd_mode_block(&report, &row, title, cells);
	STR(cells[0].value, AE_RV_DASH); STR(cells[1].value, AE_RV_DASH); STR(cells[2].value, "4");
	/* RACE */
	report.mode = AE_PGR_MODE_RACE;
	row.laps = 5; row.best_lap = 30 * 64;
	CHECK(ae_pgd_mode_block(&report, &row, title, cells) == 2);
	STR(title, "RACE"); STR(cells[0].label, "LAPS"); STR(cells[0].value, "5"); STR(cells[1].label, "BEST LAP"); STR(cells[1].value, "1:04");
	row.best_lap = 0;
	ae_pgd_mode_block(&report, &row, title, cells);
	STR(cells[1].value, AE_RV_DASH);
	/* SLAYER: the player's medals */
	report.mode = AE_PGR_MODE_SLAYER;
	row.summary.extended = 1;
	row.summary.medal_count = 2;
	row.summary.medals[0].id = AE_MEDAL_WINGMAN; row.summary.medals[0].count = 1;
	row.summary.medals[1].id = AE_MEDAL_PROTECTOR; row.summary.medals[1].count = 3;
	CHECK(ae_pgd_mode_block(&report, &row, title, cells) == 3);
	STR(title, "MEDALS"); STR(cells[0].label, "EARNED"); STR(cells[0].value, "4"); STR(cells[1].label, "KINDS"); STR(cells[1].value, "2");
	STR(cells[2].label, "POINTS");
	row.summary.valid = 0;
	ae_pgd_mode_block(&report, &row, title, cells);
	STR(cells[0].value, AE_RV_DASH);
}

static void test_lines(void)
{
	struct ae_pgd_line lines[AE_PGD_LINES];
	char title[AE_PGD_TITLE];
	int index;

	/* a team game: two lines, the totals of kills, deaths, assists (the teams'), accuracy and damage of the team's rows */
	memset(&report, 0, sizeof(report));
	report.valid = 1;
	report.teams = 1;
	report.rows = 4;
	for (index = 0; index < 4; index++)
	{
		mockup_row(&report.row[index]);
		report.row[index].team = index < 2 ? 0 : 1;
		report.row[index].slot = index;
	}
	snprintf(report.team[0].name, sizeof(report.team[0].name), "BLUE TEAM"); report.team[0].color = 0x2E86DEFFu;
	report.team[0].kills = 50; report.team[0].deaths = 46; report.team[0].assists = 21; report.team[0].first = 0; report.team[0].count = 2;
	snprintf(report.team[1].name, sizeof(report.team[1].name), "RED TEAM"); report.team[1].color = 0xC0392BFFu;
	report.team[1].kills = 45; report.team[1].deaths = 52; report.team[1].assists = 22; report.team[1].first = 2; report.team[1].count = 2;
	/* (team 0's two rows: 37 of 120 each: 31%; damage 1980 each: 3960; team 1's: one row has no summary) */
	report.row[3].summary.valid = 0;
	report.row[2].shots_hit = 60;
	CHECK(ae_pgd_lines(&report, lines, title) == 2);
	STR(title, "TEAMS");
	STR(lines[0].name, "BLUE TEAM"); STR(lines[0].k, "50"); STR(lines[0].d, "46"); STR(lines[0].a, "21"); STR(lines[0].acc, "31%");
	STR(lines[0].damage, "3,960"); CHECK(lines[0].chip == 0x2E86DEFFu);
	STR(lines[1].name, "RED TEAM"); STR(lines[1].k, "45");
	/* (accuracy: 97 of 240 shots: 40%; the damage is the rows that have a summary: 1,980, and the dash if none have) */
	STR(lines[1].acc, "40%"); STR(lines[1].damage, "1,980");
	report.row[2].summary.valid = 0;
	ae_pgd_lines(&report, lines, title);
	STR(lines[1].damage, AE_RV_DASH);
	/* no shots known on a team's rows: the dash */
	report.row[2].shots_fired = report.row[3].shots_fired = AE_PGR_UNKNOWN;
	ae_pgd_lines(&report, lines, title);
	STR(lines[1].acc, AE_RV_DASH);
	/* a free for all: the top three by the board's order */
	memset(&report, 0, sizeof(report));
	report.valid = 1;
	report.rows = 5;
	for (index = 0; index < 5; index++)
	{
		mockup_row(&report.row[index]);
		report.row[index].rank = index;
		report.row[index].kills = 20 - index;
		report.row[index].chip = 0x112233FFu + (unsigned int)index * 256u;
		snprintf(report.row[index].name, sizeof(report.row[index].name), "P%d", index);
	}
	report.row[1].me = 1;
	CHECK(ae_pgd_lines(&report, lines, title) == 3);
	STR(title, "TOP PLAYERS");
	STR(lines[0].name, "P0"); STR(lines[0].k, "20"); STR(lines[2].name, "P2"); STR(lines[0].acc, "31%"); STR(lines[0].damage, "1,980");
	CHECK(lines[1].me && !lines[0].me);
	/* fewer players than lines */
	report.rows = 2;
	CHECK(ae_pgd_lines(&report, lines, title) == 2);
	report.rows = 0;
	CHECK(ae_pgd_lines(&report, lines, title) == 0);
}

int main(void)
{
	test_player();
	test_assists_mode();
	test_lines();
	if (failures)
		printf("%d failures\n", failures);
	else
		printf("ae_pgcr_details_rules_test: all passed\n");
	return failures ? 1 : 0;
}
