#include <stdio.h>
#include <string.h>
#include "ae_pgcr_screen_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static struct ae_pgr_report report;

static void teams_report(int red, int blue)
{
	int index;

	memset(&report, 0, sizeof(report));
	report.valid = 1;
	report.teams = 1;
	report.rows = red + blue;
	report.team[0].first = 0;
	report.team[0].count = red;
	report.team[1].first = red;
	report.team[1].count = blue;
	for (index = 0; index < red + blue; index++)
		report.row[index].team = index < red ? 0 : 1;
}

static void test_cursor(void)
{
	teams_report(4, 3);
	CHECK(ae_pgr_cursor_step(&report, 0, -1) == 0);
	CHECK(ae_pgr_cursor_step(&report, 3, 1) == 4);
	CHECK(ae_pgr_cursor_step(&report, 6, 1) == 6);
	/* LB / RB: the other team, the same rank, held to its last */
	CHECK(ae_pgr_cursor_jump(&report, 1, 1) == 5);
	CHECK(ae_pgr_cursor_jump(&report, 3, 1) == 6);
	CHECK(ae_pgr_cursor_jump(&report, 5, -1) == 1);
	/* a team with nobody: nowhere to go */
	teams_report(4, 0);
	CHECK(ae_pgr_cursor_jump(&report, 2, 1) == 2);
	/* FFA: a page when there is more than one, else nothing */
	memset(&report, 0, sizeof(report));
	report.rows = 20;
	CHECK(ae_pgr_cursor_jump(&report, 3, 1) == 11);
	CHECK(ae_pgr_cursor_jump(&report, 3, -1) == 0);
	CHECK(ae_pgr_cursor_jump(&report, 15, 1) == 19);
	report.rows = 6;
	CHECK(ae_pgr_cursor_jump(&report, 3, 1) == 3);
	report.rows = 0;
	CHECK(ae_pgr_cursor_step(&report, 5, 1) == 0);
	/* the start: this machine's row */
	report.rows = 5;
	report.row[3].me = 1;
	CHECK(ae_pgr_cursor_start(&report) == 3);
	report.row[3].me = 0;
	CHECK(ae_pgr_cursor_start(&report) == 0);
}

static void test_medals(void)
{
	struct ae_pgcr_summary summary;
	int medals[AE_PGCR_SUMMARY_MEDALS], counts[AE_PGCR_SUMMARY_MEDALS], n;

	memset(&summary, 0, sizeof(summary));
	/* none without a summary */
	CHECK(ae_pgr_medals_sorted(&summary, medals, counts) == 0);
	summary.valid = 1;
	summary.medal_count = 4;
	summary.medals[0].id = AE_MEDAL_DOUBLE_KILL; summary.medals[0].count = 3;      /* 5 points */
	summary.medals[1].id = AE_MEDAL_FLAG_CAPTURED; summary.medals[1].count = 1;    /* 100 */
	summary.medals[2].id = AE_MEDAL_KILLTACULAR; summary.medals[2].count = 1;      /* 25 */
	summary.medals[3].id = AE_MEDAL_TRIPLE_OF_EACH_KILL; summary.medals[3].count = 2; /* 25, a lower number */
	n = ae_pgr_medals_sorted(&summary, medals, counts);
	CHECK(n == 4);
	CHECK(medals[0] == AE_MEDAL_FLAG_CAPTURED);
	CHECK(medals[1] == AE_MEDAL_TRIPLE_OF_EACH_KILL && counts[1] == 2);
	CHECK(medals[2] == AE_MEDAL_KILLTACULAR);
	CHECK(medals[3] == AE_MEDAL_DOUBLE_KILL && counts[3] == 3);
	CHECK(ae_pgr_medal_total(&summary) == 7);
	CHECK(ae_pgr_strip_rest(&summary, 4) == 0);
	CHECK(ae_pgr_strip_rest(&summary, 2) == 4);
	CHECK(ae_pgr_strip_rest(&summary, 0) == 7);
	/* a medal this build does not know is left out */
	summary.medals[0].id = 250;
	CHECK(ae_pgr_medals_sorted(&summary, medals, counts) == 3);
	CHECK(ae_pgr_medal_total(&summary) == 4);
	CHECK(ae_pgr_medal_family(AE_MEDAL_KILLTACULAR) == AE_PGR_FAMILY_MULTIKILL);
	CHECK(ae_pgr_medal_family(AE_MEDAL_KILLING_SPREE) == AE_PGR_FAMILY_SPREE);
	CHECK(ae_pgr_medal_family(AE_MEDAL_HEADSHOT) == AE_PGR_FAMILY_SKILL);
	CHECK(ae_pgr_medal_family(AE_MEDAL_FLAG_CAPTURED) == AE_PGR_FAMILY_OBJECTIVE);
	{
		char name[48];

		CHECK(ae_pgr_medal_name(AE_MEDAL_OPEN_SEASON, name, sizeof(name)) > 0 && !strcmp(name, "OPEN SEASON"));
		CHECK(ae_pgr_medal_name(-1, name, sizeof(name)) == 0 && !name[0]);
	}
}

static void test_words(void)
{
	char text[48];

	ae_pgr_kd_text(14, 11, text, sizeof(text));
	CHECK(!strcmp(text, "1.27"));
	ae_pgr_kd_text(8, 14, text, sizeof(text));
	CHECK(!strcmp(text, "0.57"));
	ae_pgr_kd_text(5, 0, text, sizeof(text));
	CHECK(!strcmp(text, "5.00"));
	ae_pgr_kd_text(0, 0, text, sizeof(text));
	CHECK(!strcmp(text, "0.00"));
	ae_pgr_kd_text(-1, 3, text, sizeof(text));
	CHECK(!strcmp(text, "\xE2\x80\x94"));
	ae_pgr_accuracy_text(100, 38, text, sizeof(text));
	CHECK(!strcmp(text, "38%"));
	ae_pgr_accuracy_text(3, 1, text, sizeof(text));
	CHECK(!strcmp(text, "33%"));
	ae_pgr_accuracy_text(0, 0, text, sizeof(text));
	CHECK(!strcmp(text, "\xE2\x80\x94"));
	ae_pgr_accuracy_text(10, 12, text, sizeof(text));
	CHECK(!strcmp(text, "100%"));
	ae_pgr_number_text(AE_PGR_UNKNOWN, text, sizeof(text));
	CHECK(!strcmp(text, "\xE2\x80\x94"));
	ae_pgr_number_text(0, text, sizeof(text));
	CHECK(!strcmp(text, "0"));
	ae_pgr_kills_text(1, text, sizeof(text));
	CHECK(!strcmp(text, "1 kill"));
	ae_pgr_kills_text(7, text, sizeof(text));
	CHECK(!strcmp(text, "7 kills"));
	ae_pgr_weapon_name("weapons\\battle rifle\\bullet", text, sizeof(text));
	CHECK(!strcmp(text, "BATTLE RIFLE"));
	ae_pgr_weapon_name("vehicles\\warthog\\bullet", text, sizeof(text));
	CHECK(!strcmp(text, "WARTHOG"));
	ae_pgr_weapon_name("globals\\falling", text, sizeof(text));
	CHECK(!strcmp(text, "FALLING"));
	ae_pgr_weapon_name("", text, sizeof(text));
	CHECK(!text[0]);
	ae_pgr_weapon_name(NULL, text, sizeof(text));
	CHECK(!text[0]);
	ae_pgr_weapon_name("weapons\\sniper rifle\\sniper bullet", text, 6);
	CHECK(!strcmp(text, "SNIPE"));
}

static void test_points(void)
{
	struct ae_pgr_row row;
	long k, s, b;

	memset(&row, 0, sizeof(row));
	row.kills = 14; row.suicides = 1; row.betrayals = 0; row.score_value = 13;
	CHECK(ae_pgr_points_add_up(&row, &k, &s, &b) && k == 14 && s == 1 && b == 0);
	row.score_value = 17;
	CHECK(!ae_pgr_points_add_up(&row, &k, &s, &b));
	row.score_value = 13;
	row.betrayals = AE_PGR_UNKNOWN;
	CHECK(!ae_pgr_points_add_up(&row, &k, &s, &b));
}

static void test_demo(void)
{
	CHECK(ae_pgr_demo_report(&report, "4v4") && report.teams && report.rows == 8 && report.team[1].first == 4);
	CHECK(report.row[3].me && ae_pgr_cursor_start(&report) == 3);
	CHECK(ae_pgr_demo_report(&report, "8v8") && report.rows == 16);
	CHECK(ae_pgr_demo_report(&report, "ffa8") && !report.teams && report.rows == 8);
	CHECK(!ae_pgr_demo_report(&report, "nothing") && !ae_pgr_demo_report(&report, "ffa0") && !ae_pgr_demo_report(&report, NULL));
	CHECK(ae_pgr_demo_report(&report, "ffa8") && ae_pgr_medal_total(&report.row[0].summary) > 0);
}

static int rival_of(void *context, char const *owner, char const *name)
{
	(void)context;
	return !strcmp(owner, "Reaver") && (!strcmp(name, "Vale") || !strcmp(name, "Reaver"));
}

static void test_winners_rivals_record(void)
{
	struct ae_sr_game game;
	int row;

	/* teams: the first team wins on the higher score; a tie, nobody */
	teams_report(2, 2);
	report.team[0].score_value = 50;
	report.team[1].score_value = 45;
	ae_pgr_decide_winners(&report);
	CHECK(report.row[0].won && report.row[1].won && !report.row[2].won && !report.row[3].won);
	report.team[1].score_value = 50;
	ae_pgr_decide_winners(&report);
	CHECK(!report.row[0].won && !report.row[2].won);
	/* a free for all: first place alone wins; a shared first place, nobody */
	memset(&report, 0, sizeof(report));
	report.valid = 1;
	report.rows = 3;
	report.row[0].rank = 0; report.row[1].rank = 1; report.row[2].rank = 2;
	ae_pgr_decide_winners(&report);
	CHECK(report.row[0].won && !report.row[1].won && !report.row[2].won);
	report.row[1].rank = 0;
	ae_pgr_decide_winners(&report);
	CHECK(!report.row[0].won && !report.row[1].won);
	/* rivals: the owner is the first player of this machine; their own row is never one */
	CHECK(ae_pgr_demo_report(&report, "ffa8"));
	CHECK(report.demo && !strcmp(ae_pgr_owner_name(&report), "Kite"));
	CHECK(ae_pgr_mark_rivals(&report, rival_of, NULL) == 0);
	snprintf(report.row[3].name, sizeof(report.row[3].name), "Reaver");
	snprintf(report.row[0].name, sizeof(report.row[0].name), "Zed");
	snprintf(report.row[1].name, sizeof(report.row[1].name), "Vale");
	CHECK(ae_pgr_mark_rivals(&report, rival_of, NULL) == 1);
	for (row = 0; row < report.rows; row++)
		CHECK(report.row[row].rival == (row == 1));
	CHECK(ae_pgr_mark_rivals(&report, NULL, NULL) == 0 && !report.row[1].rival);
	/* a made-up report is not a game for the record; a real one is, with its summaries */
	CHECK(!ae_pgr_to_record_game(&report, &game));
	report.demo = 0;
	CHECK(ae_pgr_to_record_game(&report, &game) && game.count == 8 && game.mode == (int)AE_PGR_MODE_SLAYER);
	CHECK(game.player[3].me && !strcmp(game.player[3].name, "Reaver") && game.player[0].won);
	CHECK(game.player[0].medal_count == report.row[0].summary.medal_count);
	report.row[2].summary.versus_count = 1;
	report.row[2].summary.versus[0].slot = 5;
	report.row[2].summary.versus[0].killed = 4;
	ae_pgr_to_record_game(&report, &game);
	CHECK(game.player[2].versus_count == 1 && game.player[2].versus[0].slot == 5 && game.player[2].versus[0].killed == 4);
	/* a bot's row is left out */
	report.row[7].bot = 1;
	ae_pgr_to_record_game(&report, &game);
	CHECK(game.count == 7);
	memset(&report, 0, sizeof(report));
	CHECK(!ae_pgr_to_record_game(&report, &game));
}

int main(void)
{
	test_cursor();
	test_medals();
	test_words();
	test_points();
	test_demo();
	test_winners_rivals_record();
	if (failures)
		return 1;
	printf("ae_pgcr_screen_rules_test: ok\n");
	return 0;
}
