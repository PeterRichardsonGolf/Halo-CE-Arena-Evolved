#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ae_career_record.h"
#include "ae_career_xp.h"
#include "ae_career_xp_rules.h"
#include "ae_career_view_rules.h"
#include "ae_career_challenge_rules.h"
#include "ae_mcc_rank_rules.h"
#include "ae_medal_pack_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
#define EQ(a, b) do { long x_ = (long)(a), y_ = (long)(b); if (x_ != y_) { printf("FAIL %s:%d %s = %ld, want %ld\n", __FILE__, __LINE__, #a, x_, y_); failures++; } } while (0)

static struct ae_career career, again;
static char text[AE_CAREER_TEXT];

static void test_ladder(void)
{
	int step;
	char title[96];

	EQ(ae_xp_step_start(0), 0);
	EQ(ae_xp_step_start(1), 400);
	EQ(ae_xp_step_start(2), 400 + 460);
	/* General of tour 1 (rank 30) at 35,960 XP; tour 2 starts 2,140 XP later; General of tour 11 at 416,960 */
	EQ(ae_xp_step_start(29), 35960);
	EQ(ae_xp_step_start(30), 38100);
	EQ(ae_xp_step_start(329), 416960);
	EQ(ae_xp_step_start(-1), 0);
	EQ(ae_xp_step_start(AE_XP_STEPS), 0);
	for (step = 2; step < AE_XP_STEPS; step++)
	{
		EQ(ae_xp_step(ae_xp_step_start(step)), step);
		EQ(ae_xp_step(ae_xp_step_start(step) - 1), step - 1);
	}
	EQ(ae_xp_step(0), 0);
	EQ(ae_xp_step(-5), 0);
	EQ(ae_xp_step(AE_XP_MAX), AE_XP_STEPS - 1);
	EQ(ae_xp_tour(0), 1); EQ(ae_xp_rank(0), 1);
	EQ(ae_xp_tour(29), 1); EQ(ae_xp_rank(29), 30);
	EQ(ae_xp_tour(30), 2); EQ(ae_xp_rank(30), 1);
	EQ(ae_xp_tour(329), 11); EQ(ae_xp_rank(329), 30);
	CHECK(!strcmp(ae_xp_rank_name(1), "Rookie"));
	CHECK(!strcmp(ae_xp_rank_name(30), "General"));
	CHECK(!strcmp(ae_xp_rank_name(0), ""));
	CHECK(!strcmp(ae_xp_rank_name(31), ""));
	for (step = 1; step <= AE_XP_RANKS; step++)
		CHECK(ae_xp_rank_name(step)[0] != 0);
	ae_xp_title(7, title, sizeof(title));
	CHECK(!strcmp(title, "Sergeant, tour 1"));
	ae_xp_title(30 * 2 + 29, title, sizeof(title));
	CHECK(!strcmp(title, "General, tour 3"));
	EQ(ae_xp_to_next(0), 400);
	EQ(ae_xp_to_next(100), 300);
	EQ(ae_xp_to_next(416960), 0);
	EQ(ae_xp_percent(0), 0);
	EQ(ae_xp_percent(200), 50);
	EQ(ae_xp_percent(399), 99);
	EQ(ae_xp_percent(416960), 100);
	EQ(ae_xp_add(5, -10), 0);
	EQ(ae_xp_add(AE_XP_MAX - 1, 100), AE_XP_MAX);
	EQ(ae_xp_add(10, 5), 15);
	/* the pace: a typical game of 200 XP takes General of tour 1 in 180 games */
	CHECK(ae_xp_step_start(29) / 200 >= 150 && ae_xp_step_start(29) / 200 <= 200);
}

static void test_awards(void)
{
	struct ae_xp_game game;
	struct ae_xp_award award;

	memset(&game, 0, sizeof(game));
	game.won = 1; game.kills = 12; game.medals = 3; game.objectives = 2; game.seconds = 600;
	EQ(ae_xp_game_award(&game, &award), 50 + 50 + 24 + 12 + 20);
	EQ(award.finish, 50); EQ(award.win, 50); EQ(award.kills, 24); EQ(award.medals, 12); EQ(award.objectives, 20);
	/* a tie, a loss, a cap on each part */
	game.won = 0; game.tied = 1; game.kills = 500; game.medals = 500; game.objectives = 500;
	EQ(ae_xp_game_award(&game, &award), 50 + 25 + 300);
	game.tied = 0; game.kills = game.medals = game.objectives = 0;
	EQ(ae_xp_game_award(&game, NULL), 50);
	/* a quit and a game under a minute give none; an unknown length counts */
	game.quit = 1;
	EQ(ae_xp_game_award(&game, &award), 0);
	EQ(award.total, 0);
	game.quit = 0; game.seconds = 59;
	EQ(ae_xp_game_award(&game, NULL), 0);
	game.seconds = 60;
	EQ(ae_xp_game_award(&game, NULL), 50);
	game.seconds = 0;
	EQ(ae_xp_game_award(&game, NULL), 50);
	EQ(ae_xp_game_award(NULL, NULL), 0);
	EQ(ae_xp_campaign_award(0), 150); EQ(ae_xp_campaign_award(1), 250); EQ(ae_xp_campaign_award(2), 400); EQ(ae_xp_campaign_award(3), 600);
	EQ(ae_xp_campaign_award(4), 0); EQ(ae_xp_campaign_award(-1), 0);
}

static void test_days(void)
{
	EQ(ae_ch_day_from_date(1970, 1, 1), 0);
	EQ(ae_ch_day_from_date(1970, 1, 2), 1);
	EQ(ae_ch_day_from_date(1970, 3, 1), 59);
	EQ(ae_ch_day_from_date(2000, 3, 1), 11017);
	EQ(ae_ch_day_from_date(2026, 11, 23), 20780);
	EQ(ae_ch_day_from_date(1969, 1, 1), 0);
	EQ(ae_ch_day_from_date(2026, 13, 1), 0);
	EQ(ae_ch_day_of(86399), 0); EQ(ae_ch_day_of(86400), 1); EQ(ae_ch_day_of(-1), -1);
	/* weeks start on a Monday: 2026-11-23 was a Monday, 11-22 the Sunday before it */
	EQ(ae_ch_week_of(20780), ae_ch_week_of(20786));
	EQ(ae_ch_week_of(20780) - ae_ch_week_of(20779), 1);
	EQ(ae_ch_week_of(0), 0);
	EQ(ae_ch_week_of(3), 0);
	EQ(ae_ch_week_of(4), 1);
}

static void test_picks(void)
{
	int picks[AE_CH_PER_PERIOD], again_picks[AE_CH_PER_PERIOD], day, each, other, same_all = 1, seen[2][64];

	memset(seen, 0, sizeof(seen));
	/* every kind is of its period, ids are unique and found */
	for (each = 0; each < ae_ch_kind_count(); each++)
	{
		CHECK(ae_ch_kind_at(each)->target > 0);
		CHECK(ae_ch_kind_at(each)->xp > 0);
		CHECK(strlen(ae_ch_kind_at(each)->id) < AE_CH_ID);
		EQ(ae_ch_kind_find(ae_ch_kind_at(each)->id), each);
	}
	CHECK(ae_ch_kind_at(-1) == NULL);
	CHECK(ae_ch_kind_find("nope") == -1);
	for (day = 20000; day < 20400; day++)
	{
		int period;

		for (period = 0; period < 2; period++)
		{
			long id = period ? ae_ch_week_of(day) : day;

			EQ(ae_ch_pick(period, id, picks), AE_CH_PER_PERIOD);
			EQ(ae_ch_pick(period, id, again_picks), AE_CH_PER_PERIOD);
			for (each = 0; each < AE_CH_PER_PERIOD; each++)
			{
				EQ(picks[each], again_picks[each]);
				EQ(ae_ch_kind_at(picks[each])->period, period);
				seen[period][picks[each]]++;
				for (other = 0; other < each; other++)
				{
					CHECK(picks[other] != picks[each]);
					CHECK(ae_ch_kind_at(picks[other])->metric != ae_ch_kind_at(picks[each])->metric);
				}
			}
		}
	}
	/* every kind of both periods comes up over a year, and the days differ */
	for (each = 0; each < ae_ch_kind_count(); each++)
		CHECK(seen[ae_ch_kind_at(each)->period][each] > 0);
	for (day = 20000; day < 20100; day++)
	{
		ae_ch_pick(AE_CH_DAILY, day, picks);
		ae_ch_pick(AE_CH_DAILY, day + 1, again_picks);
		if (memcmp(picks, again_picks, sizeof(picks)))
			same_all = 0;
	}
	CHECK(!same_all);
	EQ(ae_ch_pick(2, 5, picks), 0);
	EQ(ae_ch_pick(AE_CH_DAILY, 5, NULL), 0);
}

static void test_labels(void)
{
	char text[64];
	int each;

	ae_ch_label(ae_ch_kind_at(ae_ch_kind_find("d_objplay2")), text, sizeof(text));
	CHECK(!strcmp(text, "PLAY 2 OBJECTIVE GAMES"));
	ae_ch_label(ae_ch_kind_at(ae_ch_kind_find("d_kills40")), text, sizeof(text));
	CHECK(!strcmp(text, "GET 40 KILLS"));
	ae_ch_label(ae_ch_kind_at(ae_ch_kind_find("d_slay25")), text, sizeof(text));
	CHECK(!strcmp(text, "GET 25 KILLS IN SLAYER"));
	ae_ch_label(ae_ch_kind_at(ae_ch_kind_find("d_camp1")), text, sizeof(text));
	CHECK(!strcmp(text, "FINISH 1 CAMPAIGN LEVEL"));
	ae_ch_label(ae_ch_kind_at(ae_ch_kind_find("w_camp3")), text, sizeof(text));
	CHECK(!strcmp(text, "FINISH 3 CAMPAIGN LEVELS"));
	ae_ch_label(ae_ch_kind_at(ae_ch_kind_find("w_win10")), text, sizeof(text));
	CHECK(!strcmp(text, "WIN 10 GAMES"));
	for (each = 0; each < ae_ch_kind_count(); each++)
	{
		ae_ch_label(ae_ch_kind_at(each), text, sizeof(text));
		CHECK(text[0] != 0);
	}
	ae_ch_label(NULL, text, sizeof(text));
	CHECK(!text[0]);
	for (each = 0; each < ae_ch_kind_count(); each++)
	{
		char sentence[160];

		ae_ch_description(ae_ch_kind_at(each), sentence, sizeof(sentence));
		CHECK(sentence[0] != 0);
	}
	{
		char sentence[160];

		ae_ch_description(ae_ch_kind_at(ae_ch_kind_find("d_play3")), sentence, sizeof(sentence));
		CHECK(!strcmp(sentence, "Finish 3 games of any kind. A game you leave early does not count."));
	}
	ae_ch_label(ae_ch_kind_at(0), text, 5);
	EQ(strlen(text), 4);
}

static void test_view(void)
{
	char a[64], b[64], c[64];

	ae_cv_number(10350, a, sizeof(a)); CHECK(!strcmp(a, "10,350"));
	ae_cv_number(0, a, sizeof(a)); CHECK(!strcmp(a, "0"));
	ae_cv_number(416960, a, sizeof(a)); CHECK(!strcmp(a, "416,960"));
	ae_cv_number(999, a, sizeof(a)); CHECK(!strcmp(a, "999"));
	ae_cv_number(1000000, a, sizeof(a)); CHECK(!strcmp(a, "1,000,000"));
	/* a quit is one left-early game: it is not a loss as well */
	EQ(ae_cv_losses(30, 16, 1, 0), 13);
	EQ(ae_cv_losses(30, 16, 1, 3), 10);
	EQ(ae_cv_losses(5, 5, 0, 2), 0);
	EQ(ae_cv_losses(0, 0, 0, 0), 0);
	ae_cv_win_rate(30, 16, a, sizeof(a)); CHECK(!strcmp(a, "53%"));
	ae_cv_win_rate(0, 0, a, sizeof(a)); CHECK(!strcmp(a, AE_CV_DASH));
	ae_cv_win_rate(3, 3, a, sizeof(a)); CHECK(!strcmp(a, "100%"));
	ae_cv_win_rate(7, 0, a, sizeof(a)); CHECK(!strcmp(a, "0%"));
	/* the rank's name: 40 u, then 34, then 30; never cut */
	EQ(ae_cv_fit_size(300, 255, 225, 400), 40);
	EQ(ae_cv_fit_size(760, 646, 570, 700), 34);
	EQ(ae_cv_fit_size(1000, 850, 750, 700), 30);
	ae_cv_meta(13, 10350, a, sizeof(a), b, sizeof(b), c, sizeof(c));
	CHECK(!strcmp(a, "RANK 14 OF 30") && !strcmp(b, "TOUR 1 OF 11") && !strcmp(c, "10,350 XP"));
	ae_cv_career_captions(13, 710, a, sizeof(a), b, sizeof(b));
	CHECK(!strcmp(a, "710 XP TO GO") && !strcmp(b, "NEXT: COMMAND SERGEANT MAJOR"));
	/* rank 30 of a tour below the last: the next tour */
	ae_cv_career_captions(29, 2140, a, sizeof(a), b, sizeof(b));
	CHECK(!strcmp(a, "2,140 XP TO GO") && !strcmp(b, "NEXT: TOUR 2"));
	/* the last step */
	ae_cv_career_captions(AE_XP_STEPS - 1, 0, a, sizeof(a), b, sizeof(b));
	CHECK(!strcmp(a, "THE LAST RANK") && !strcmp(b, "XP KEEPS COUNTING"));
	ae_cv_skill_captions(10, 3, a, sizeof(a), b, sizeof(b));
	CHECK(!strcmp(a, "3 WINS OR TIES TO GO") && !strcmp(b, "NEXT: RANK 11"));
	ae_cv_skill_captions(10, 1, a, sizeof(a), b, sizeof(b));
	CHECK(!strcmp(a, "1 WIN OR TIE TO GO"));
	ae_cv_skill_captions(50, 0, a, sizeof(a), b, sizeof(b));
	CHECK(!strcmp(a, "THE TOP RANK") && !b[0]);
	ae_cv_overall_captions(8, 4, a, sizeof(a), b, sizeof(b));
	CHECK(!strcmp(b, "NEXT: OVERALL RANK 9"));
	ae_cv_challenge_to_go(AE_CH_GAMES, 2, a, sizeof(a)); CHECK(!strcmp(a, "2 GAMES TO GO"));
	ae_cv_challenge_to_go(AE_CH_KILLS, 1, a, sizeof(a)); CHECK(!strcmp(a, "1 KILL TO GO"));
	ae_cv_challenge_to_go(AE_CH_CAMPAIGN_LEVELS, 3, a, sizeof(a)); CHECK(!strcmp(a, "3 LEVELS TO GO"));
	ae_cv_number(5, a, 3); EQ(strlen(a), 1);
}

static void test_gain(void)
{
	struct ae_ch_game game;
	struct ae_ch_kind const *slay = ae_ch_kind_at(ae_ch_kind_find("d_slay25")), *objplay = ae_ch_kind_at(ae_ch_kind_find("d_objplay2"));
	struct ae_ch_kind const *time30 = ae_ch_kind_at(ae_ch_kind_find("d_time30")), *camp = ae_ch_kind_at(ae_ch_kind_find("d_camp1"));
	struct ae_ch_kind const *win2 = ae_ch_kind_at(ae_ch_kind_find("d_win2"));

	memset(&game, 0, sizeof(game));
	game.kind = 0; game.kills = 9; game.won = 1; game.seconds = 725;
	EQ(ae_ch_game_gain(slay, &game), 9);
	EQ(ae_ch_game_gain(objplay, &game), 0);
	EQ(ae_ch_game_gain(time30, &game), 12);
	EQ(ae_ch_game_gain(win2, &game), 1);
	EQ(ae_ch_game_gain(camp, &game), 0);
	game.kind = 1;
	EQ(ae_ch_game_gain(slay, &game), 0);
	EQ(ae_ch_game_gain(objplay, &game), 1);
	game.kind = 4;
	EQ(ae_ch_game_gain(objplay, &game), 0);
	game.quit = 1;
	EQ(ae_ch_game_gain(win2, &game), 0);
	EQ(ae_ch_campaign_gain(camp), 1);
	EQ(ae_ch_campaign_gain(win2), 0);
	EQ(ae_ch_campaign_gain(NULL), 0);
}

static void game_of(struct ae_xp_game *g, struct ae_ch_game *c, int kind, int won, long kills, long medals, long objectives, long seconds)
{
	memset(g, 0, sizeof(*g));
	memset(c, 0, sizeof(*c));
	g->won = won; g->kills = kills; g->medals = medals; g->objectives = objectives; g->seconds = seconds;
	c->kind = kind; c->won = won; c->kills = kills; c->medals = medals; c->objectives = objectives; c->seconds = seconds;
}

static void test_apply(void)
{
	struct ae_xp_game g;
	struct ae_ch_game c;
	struct ae_xp_result r;
	struct ae_career_challenges *lines;
	int picks[AE_CH_PER_PERIOD], each, paid_total = 0;
	long day = 20415, total = 0;

	ae_career_init(&career);
	CHECK(ae_career_challenges_refresh(&career, "Chief", day, &lines));
	EQ(lines->day, day); EQ(lines->week, ae_ch_week_of(day));
	ae_ch_pick(AE_CH_DAILY, day, picks);
	for (each = 0; each < 3; each++)
	{
		CHECK(!strcmp(lines->slot[each].kind, ae_ch_kind_at(picks[each])->id));
		EQ(lines->slot[each].progress, 0);
	}
	ae_ch_pick(AE_CH_WEEKLY, ae_ch_week_of(day), picks);
	for (each = 0; each < 3; each++)
		CHECK(!strcmp(lines->slot[3 + each].kind, ae_ch_kind_at(picks[each])->id));
	/* a refresh the same day changes nothing */
	CHECK(ae_career_challenges_refresh(&career, "Chief", day, &lines));
	EQ(career.challenge_count, 1);

	/* a quit and a short game pay nothing and make no line */
	game_of(&g, &c, 0, 0, 5, 0, 0, 600);
	g.quit = 1; c.quit = 1;
	CHECK(!ae_career_xp_game(&career, "Chief", &g, &c, day, 100, &r));
	EQ(r.xp_after, 0);
	game_of(&g, &c, 0, 0, 5, 0, 0, 30);
	CHECK(!ae_career_xp_game(&career, "Chief", &g, &c, day, 100, &r));
	EQ(ae_career_xp_get(&career, "Chief"), 0);

	/* games add up; the level follows; a challenge pays once */
	for (each = 0; each < 40; each++)
	{
		game_of(&g, &c, 0, each % 2, 10, 4, 1, 700);
		CHECK(ae_career_xp_game(&career, "Chief", &g, &c, day, 200 + (unsigned long)each, &r));
		total += r.award.total + r.challenge_xp;
		paid_total += r.paid_count;
		EQ(r.xp_after, total);
		EQ(r.step_after, ae_xp_step(total));
	}
	EQ(ae_career_xp_get(&career, "Chief"), total);
	CHECK(total > 40 * 100);
	CHECK(paid_total > 0);
	CHECK(paid_total <= AE_CH_SLOTS);
	{
		int done = 0, any_over = 0;

		ae_career_challenges_refresh(&career, "Chief", day, &lines);
		for (each = 0; each < AE_CH_SLOTS; each++)
		{
			struct ae_ch_kind const *k = ae_ch_kind_at(ae_ch_kind_find(lines->slot[each].kind));

			done += lines->slot[each].done;
			if (lines->slot[each].progress > k->target)
				any_over = 1;
		}
		EQ(done, paid_total);
		CHECK(!any_over);
	}
	/* another profile has its own lines */
	EQ(ae_career_xp_get(&career, "Other"), 0);
	game_of(&g, &c, 0, 1, 1, 0, 0, 0);
	CHECK(ae_career_xp_game(&career, "Other", &g, &c, day, 300, &r));
	EQ(r.xp_after, 102);
	EQ(career.xp_count, 2);

	/* a new day picks the day's three again with empty counters, the week stays */
	{
		struct ae_career_challenges week_kept;

		ae_career_challenges_refresh(&career, "Chief", day, &lines);
		week_kept = *lines;
		ae_career_challenges_refresh(&career, "Chief", day + 1, &lines);
		EQ(lines->day, day + 1);
		ae_ch_pick(AE_CH_DAILY, day + 1, picks);
		for (each = 0; each < 3; each++)
		{
			CHECK(!strcmp(lines->slot[each].kind, ae_ch_kind_at(picks[each])->id));
			EQ(lines->slot[each].progress, 0);
			EQ(lines->slot[each].done, 0);
		}
		if (ae_ch_week_of(day + 1) == ae_ch_week_of(day))
			CHECK(!memcmp(&lines->slot[3], &week_kept.slot[3], 3 * sizeof(week_kept.slot[0])));
		/* a new week: the weekly three are empty */
		ae_career_challenges_refresh(&career, "Chief", day + 7, &lines);
		EQ(lines->week, ae_ch_week_of(day + 7));
		for (each = 3; each < 6; each++)
			EQ(lines->slot[each].progress, 0);
	}
	/* an edited line whose kinds are not the picks is picked again */
	ae_career_challenges_refresh(&career, "Chief", day, &lines);
	snprintf(lines->slot[1].kind, sizeof(lines->slot[1].kind), "%s", "made_up");
	lines->slot[1].progress = 99;
	ae_career_challenges_refresh(&career, "Chief", day, &lines);
	CHECK(strcmp(lines->slot[1].kind, "made_up") != 0);
	EQ(lines->slot[1].progress, 0);
}

/* every game kind pays XP and counts for the challenges: AE TOURNAMENT (any of its modes), bot games and customs reach the same
   rules as any other finished game (the post-game report of each is one report: ae_glue_service_record.c takes them alike) */
static void test_every_mode_counts(void)
{
	struct ae_xp_game g;
	struct ae_ch_game c;
	struct ae_xp_result r;
	int kind;
	long day = 20800;

	ae_career_init(&career);
	for (kind = 0; kind < 7; kind++)
	{
		char who[16];
		struct ae_career_challenges *lines;
		int each, counted = 0;

		snprintf(who, sizeof(who), "mode%d", kind);
		game_of(&g, &c, kind, 1, 10, 2, kind == 1 || kind == 5 ? 3 : 0, 600);
		CHECK(ae_career_xp_game(&career, who, &g, &c, day, 100, &r));
		CHECK(r.award.total >= 50 + 50 + 20 + 8);
		EQ(ae_career_xp_get(&career, who), r.xp_after);
		ae_career_challenges_refresh(&career, who, day, &lines);
		for (each = 0; each < AE_CH_SLOTS; each++)
			counted += lines->slot[each].progress > 0;
		/* the day's and week's challenges that count games of any kind moved */
		CHECK(counted >= 1);
	}
	/* a game against bots only, a custom game and a tournament game differ in nothing the rules see: a win with bots as the
	   only other players pays the same as a win against people */
	game_of(&g, &c, 0, 1, 10, 2, 0, 600);
	CHECK(ae_career_xp_game(&career, "bots", &g, &c, day, 100, &r));
	EQ(r.award.total, 50 + 50 + 20 + 8);
}

static void test_campaign(void)
{
	struct ae_xp_result r;
	struct ae_career_challenges *lines;
	long day = 20500;
	int each, found_camp = 0;

	/* find a day whose three include the campaign challenge */
	for (day = 20500; day < 20700 && !found_camp; day++)
	{
		int picks[AE_CH_PER_PERIOD];

		ae_ch_pick(AE_CH_DAILY, day, picks);
		for (each = 0; each < 3; each++)
			if (!strcmp(ae_ch_kind_at(picks[each])->id, "d_camp1"))
				found_camp = 1;
		if (found_camp)
			break;
	}
	CHECK(found_camp);
	ae_career_init(&career);
	CHECK(ae_career_xp_campaign(&career, "Chief", 2, day, 5, &r));
	EQ(r.award.total, 400);
	EQ(r.paid_count, 1);
	CHECK(!strcmp(r.paid[0].id, "d_camp1"));
	EQ(r.challenge_xp, 450);
	EQ(r.xp_after, 850);
	/* it paid once: the next level pays its own XP only */
	CHECK(ae_career_xp_campaign(&career, "Chief", 3, day, 6, &r));
	EQ(r.paid_count, 0);
	EQ(r.xp_after, 1450);
	CHECK(!ae_career_xp_campaign(&career, "Chief", 4, day, 7, &r));
	EQ(ae_career_xp_get(&career, "Chief"), 1450);
	ae_career_challenges_refresh(&career, "Chief", day, &lines);
	CHECK(lines->slot[0].done || lines->slot[1].done || lines->slot[2].done);
}

static void test_file(void)
{
	struct ae_xp_game g;
	struct ae_ch_game c;
	struct ae_xp_result r;
	long length;
	long day = 20415;
	struct ae_career_challenges *a, *b;

	ae_career_init(&career);
	/* no XP lines: a version 2 file, as before */
	ae_career_record_game(&career, AE_CAREER_OFFLINE, "ffa", 220, 0, 14);
	length = ae_career_write(&career, text, sizeof(text));
	CHECK(length > 0);
	CHECK(strstr(text, "version = 2\n") != NULL);
	CHECK(strstr(text, "xp = ") == NULL);
	EQ(ae_career_read(&again, text), AE_CAREER_READ_OK);

	game_of(&g, &c, 1, 1, 8, 2, 1, 800);
	CHECK(ae_career_xp_game(&career, "chief", &g, &c, day, 1763201234ul, &r));
	CHECK(ae_career_xp_game(&career, "chief", &g, &c, day, 1763201300ul, &r));
	game_of(&g, &c, 0, 0, 3, 0, 0, 0);
	CHECK(ae_career_xp_game(&career, "tiger", &g, &c, day, 1763201400ul, &r));
	length = ae_career_write(&career, text, sizeof(text));
	CHECK(length > 0);
	CHECK(strstr(text, "version = 3\n") != NULL);
	CHECK(strstr(text, "xp = chief | ") != NULL);
	CHECK(strstr(text, "challenges = chief | 20415 2916 | ") != NULL);
	EQ(ae_career_read(&again, text), AE_CAREER_READ_OK);
	CHECK(!again.newer);
	EQ(again.xp_count, 2); EQ(again.challenge_count, 2);
	EQ(ae_career_xp_get(&again, "chief"), ae_career_xp_get(&career, "chief"));
	EQ(ae_career_xp_get(&again, "tiger"), ae_career_xp_get(&career, "tiger"));
	EQ(ae_career_xp_find(&again, "chief")->last, 1763201300ul);
	EQ(again.count[0], 1);
	a = ae_career_challenges_find(&career, "chief");
	b = ae_career_challenges_find(&again, "chief");
	CHECK(a && b);
	EQ(a->day, b->day); EQ(a->week, b->week);
	CHECK(!memcmp(a->slot, b->slot, sizeof(a->slot)));
	/* written again, the same text */
	{
		static char second[AE_CAREER_TEXT];

		EQ(ae_career_write(&again, second, sizeof(second)), length);
		CHECK(!strcmp(second, text));
	}
	/* a file of a version past 3 is newer; a version 3 file with XP lines is not a 0.9 file's version 2 */
	CHECK(AE_CAREER_VERSION_MAX == 3);
}

static char const *summed(char const *body)
{
	static char buffer[4096];
	unsigned long hash = 2166136261u;
	size_t index;

	for (index = 0; body[index]; index++)
		hash = ((hash ^ (unsigned char)body[index]) * 16777619u) & 0xffffffffu;
	snprintf(buffer, sizeof(buffer), "%schecksum = %08lx\n", body, hash);
	return buffer;
}

static void test_reader(void)
{
	/* a version 2 file with every older line reads as before: no XP */
	EQ(ae_career_read(&again, summed("# AE career\nversion = 2\ncareer = x | 100 1 1 1 1 0 5\nsocial = a | b | 5 1 1 0 0 3\n")), AE_CAREER_READ_OK);
	EQ(again.xp_count, 0); EQ(again.challenge_count, 0);
	EQ(ae_career_xp_get(&again, "a"), 0);
	/* XP lines: numbers held, a duplicate is an edited file, bad lines are skipped */
	EQ(ae_career_read(&again, summed("# AE career\nversion = 3\nxp = a | 500 7\nxp = b | 99999999 1\n")), AE_CAREER_READ_OK);
	EQ(ae_career_xp_get(&again, "a"), 500);
	EQ(ae_career_xp_get(&again, "b"), AE_XP_MAX);
	EQ(ae_career_read(&again, summed("# AE career\nversion = 3\nxp = a | 500 7\nxp = a | 600 8\n")), AE_CAREER_READ_EDITED);
	EQ(ae_career_read(&again, summed("# AE career\nversion = 3\nxp = a | -5 7\nxp = | 5 7\nxp = c\nxp = d | 1\n")), AE_CAREER_READ_OK);
	EQ(again.xp_count, 0);
	EQ(ae_career_read(&again, summed("# AE career\nversion = 3\nchallenges = a | 5 1 | d_win2 1 0 | - 0 0\nchallenges = a | 5 1\n")),
		AE_CAREER_READ_EDITED);
	EQ(ae_career_read(&again, summed("# AE career\nversion = 3\nchallenges = a | 5 1 | d_win2 1 0 | - 0 0\n")), AE_CAREER_READ_OK);
	EQ(again.challenge_count, 1);
	EQ(ae_career_challenges_find(&again, "a")->slot[0].progress, 1);
	CHECK(!strcmp(ae_career_challenges_find(&again, "a")->slot[1].kind, ""));
	EQ(ae_career_read(&again, summed("# AE career\nversion = 3\nchallenges = a | 5 1 | d_win2 1 7\nchallenges = b | x y\nchallenges = c\n")),
		AE_CAREER_READ_OK);
	EQ(again.challenge_count, 0);
	/* a version past 3 is newer, XP lines read for what they hold */
	EQ(ae_career_read(&again, summed("# AE career\nversion = 4\nxp = a | 500 7\nfuture = 1\n")), AE_CAREER_READ_OK);
	CHECK(again.newer);
	EQ(ae_career_xp_get(&again, "a"), 500);
	/* the checksum guards the new lines too: a hand edit is an edited file; XP lines with no checksum too */
	{
		char edited[1024];
		char const *good = summed("# AE career\nversion = 3\nxp = a | 500 7\n");
		char *at;

		snprintf(edited, sizeof(edited), "%s", good);
		at = strstr(edited, "500");
		memcpy(at, "900", 3);
		EQ(ae_career_read(&again, edited), AE_CAREER_READ_EDITED);
		EQ(again.xp_count, 0);
		EQ(ae_career_read(&again, "# AE career\nversion = 3\nxp = a | 500 7\n"), AE_CAREER_READ_EDITED);
	}
}

static void test_capacity(void)
{
	struct ae_xp_game g;
	struct ae_ch_game c;
	struct ae_xp_result r;
	char name[16];
	int i;

	ae_career_init(&career);
	game_of(&g, &c, 0, 1, 1, 0, 0, 0);
	for (i = 0; i < AE_CAREER_XP_PROFILES; i++)
	{
		snprintf(name, sizeof(name), "p%d", i);
		CHECK(ae_career_xp_game(&career, name, &g, &c, 20415, 5, &r));
	}
	/* a full table refuses and changes nothing */
	CHECK(!ae_career_xp_game(&career, "one-too-many", &g, &c, 20415, 5, &r));
	EQ(career.xp_count, AE_CAREER_XP_PROFILES);
	EQ(ae_career_xp_get(&career, "one-too-many"), 0);
	EQ(r.xp_after, 0);
	/* the full file still writes and reads whole */
	CHECK(ae_career_write(&career, text, sizeof(text)) > 0);
	EQ(ae_career_read(&again, text), AE_CAREER_READ_OK);
	EQ(again.xp_count, AE_CAREER_XP_PROFILES); EQ(again.challenge_count, AE_CAREER_XP_PROFILES);
}

/* ---- the MCC rank pictures' rules, over a made-up pack ---- */

static void put16(unsigned char *at, unsigned int v) { at[0] = (unsigned char)v; at[1] = (unsigned char)(v >> 8); }
static void put32(unsigned char *at, unsigned int v) { put16(at, v & 0xffff); put16(at + 2, v >> 16); }

static void record_put(unsigned char *index, int slot, char const *name, unsigned int side, unsigned int size, unsigned int offset)
{
	unsigned char *r = index + 0x40 + (size_t)slot * 480;

	memcpy(r + 4, name, strlen(name) + 1);
	put16(r + 0x34, side);
	put16(r + 0x36, side);
	put32(r + 0x48, size);
	put32(r + 0x50, offset);
}

static void test_mcc_rank(void)
{
	static unsigned char index[0x40 + 6 * 480];
	struct ae_medal_record rec;
	char name[AE_MEDAL_NAME_SIZE];
	char const *dir = getenv("AE_MCC_RANK_TEST_DIR");

	CHECK(ae_mcc_rank_name(7, 0, name, sizeof(name)) && !strcmp(name, "SMALLRANKICON_007"));
	CHECK(ae_mcc_rank_name(50, 1, name, sizeof(name)) && !strcmp(name, "LARGERANKICON_050"));
	CHECK(!ae_mcc_rank_name(0, 0, name, sizeof(name)));
	CHECK(!ae_mcc_rank_name(51, 1, name, sizeof(name)));
	CHECK(!ae_mcc_rank_name(5, 0, name, 8));
	CHECK(!ae_mcc_rank_name(5, 0, NULL, 8));
	put32(index, AE_MEDAL_INDEX_MAGIC);
	record_put(index, 0, "MEDAL_OPENSEASON", 64, 4096, 16);
	record_put(index, 1, "SMALLRANKICON_001", 52, 2704, 4112);
	record_put(index, 2, "SMALLRANKICON_0011", 52, 2704, 4112);
	record_put(index, 3, "LARGERANKICON_050", 152, 23104, 6816);
	record_put(index, 4, "SMALLRANKICON_002", 52, 2705, 4112);
	record_put(index, 5, "SMALLRANKICON_003", 150, 22500, 4112);
	CHECK(ae_mcc_rank_record(index, sizeof(index), 1, 0, &rec));
	EQ(rec.width, 52); EQ(rec.height, 52); EQ(rec.size, 2704); EQ(rec.offset, 4112);
	CHECK(!strcmp(rec.name, "SMALLRANKICON_001"));
	CHECK(ae_mcc_rank_record_fits(&rec, 6816));
	CHECK(!ae_mcc_rank_record_fits(&rec, 6815));
	CHECK(ae_mcc_rank_record(index, sizeof(index), 50, 1, &rec));
	EQ(rec.width, 152);
	CHECK(ae_mcc_rank_record_fits(&rec, 6816 + 23104));
	CHECK(!ae_mcc_rank_record_fits(&rec, 6816 + 23103));
	/* not there, a longer name is not the name, sides that are not whole blocks, a size that is not width * height */
	CHECK(!ae_mcc_rank_record(index, sizeof(index), 1, 1, &rec));
	CHECK(!ae_mcc_rank_record(index, sizeof(index), 11, 0, &rec));
	CHECK(ae_mcc_rank_record(index, sizeof(index), 2, 0, &rec));
	CHECK(!ae_mcc_rank_record_fits(&rec, 100000));
	CHECK(ae_mcc_rank_record(index, sizeof(index), 3, 0, &rec));
	CHECK(!ae_mcc_rank_record_fits(&rec, 100000));
	/* not an index, a file cut short, a null */
	CHECK(!ae_mcc_rank_record(index, 10, 1, 0, &rec));
	CHECK(!ae_mcc_rank_record(index, 0x40 + 480 + 0x20, 1, 0, &rec));
	index[0] ^= 1;
	CHECK(!ae_mcc_rank_record(index, sizeof(index), 1, 0, &rec));
	CHECK(!ae_mcc_rank_record(NULL, 100, 1, 0, &rec));
	CHECK(!ae_mcc_rank_record_fits(NULL, 100));

	/* the player's own install, when the test is pointed at the two files (AE_MCC_RANK_TEST_DIR): every rank's two pictures */
	if (dir)
	{
		char path[1024];
		FILE *file;
		static unsigned char *idx, *pix;
		long idx_size = 0, pix_size = 0;
		int rank, large, shown = 0;
		static unsigned char rgba[AE_MCC_RANK_PICTURE_MOST * AE_MCC_RANK_PICTURE_MOST * 4];

		snprintf(path, sizeof(path), "%s/mainmenuandcampaigntexturepack.perm.bin", dir);
		file = fopen(path, "rb");
		if (file)
		{
			fseek(file, 0, SEEK_END); idx_size = ftell(file); fseek(file, 0, SEEK_SET);
			idx = malloc((size_t)idx_size);
			if (fread(idx, 1, (size_t)idx_size, file) != (size_t)idx_size) idx_size = 0;
			fclose(file);
		}
		snprintf(path, sizeof(path), "%s/mainmenuandcampaigntexturepack.temp.bin", dir);
		file = fopen(path, "rb");
		if (file)
		{
			fseek(file, 0, SEEK_END); pix_size = ftell(file); fseek(file, 0, SEEK_SET);
			pix = malloc((size_t)pix_size);
			if (fread(pix, 1, (size_t)pix_size, file) != (size_t)pix_size) pix_size = 0;
			fclose(file);
		}
		CHECK(idx_size > 0 && pix_size > 0);
		CHECK(ae_medal_pixels_valid(pix, (size_t)pix_size));
		for (rank = 1; rank <= 50; rank++)
			for (large = 0; large < 2; large++)
			{
				struct ae_medal_record r2;
				unsigned long clear = 0;
				unsigned int p;

				CHECK(ae_mcc_rank_record(idx, (size_t)idx_size, rank, large, &r2));
				EQ(r2.width, large ? 152 : 52);
				CHECK(ae_mcc_rank_record_fits(&r2, (size_t)pix_size));
				CHECK(ae_medal_decode_bc3(pix + r2.offset, r2.size, r2.width, r2.height, rgba));
				for (p = 0; p < r2.width * r2.height; p++)
					clear += rgba[p * 4 + 3] > 200;
				CHECK(clear > r2.width * r2.height / 10);
				shown++;
			}
		printf("mcc rank pictures read: %d\n", shown);
	}
}

int main(void)
{
	test_ladder();
	test_awards();
	test_days();
	test_picks();
	test_labels();
	test_view();
	test_gain();
	test_apply();
	test_every_mode_counts();
	test_campaign();
	test_file();
	test_reader();
	test_capacity();
	test_mcc_rank();
	if (failures)
		return 1;
	printf("ok\n");
	return 0;
}
