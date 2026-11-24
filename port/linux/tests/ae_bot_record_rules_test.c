#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ae_bot_record_rules.h"
#include "ae_bot_record_demo.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
#define EQ(a, b) do { long x_ = (long)(a), y_ = (long)(b); if (x_ != y_) { printf("FAIL %s:%d %s = %ld, want %ld\n", __FILE__, __LINE__, #a, x_, y_); failures++; } } while (0)
#define STR(a, b) do { if (strcmp((a), (b))) { printf("FAIL %s:%d %s = \"%s\", want \"%s\"\n", __FILE__, __LINE__, #a, (a), (b)); failures++; } } while (0)

static struct ae_bot_records records, again;
static char text[AE_BOT_RECORD_TEXT], copy[AE_BOT_RECORD_TEXT];

/* a team game on prisoner: red (team 0) score, blue score, one profile "Chief" on red */
static void team_game(struct ae_bot_game *game, long red, long blue)
{
	memset(game, 0, sizeof(*game));
	snprintf(game->playlist, sizeof(game->playlist), "team-slayer");
	snprintf(game->map, sizeof(game->map), "prisoner");
	game->teams = 1;
	game->red_score = red;
	game->blue_score = blue;
	game->profile_count = 1;
	snprintf(game->profile[0].key, sizeof(game->profile[0].key), "Chief");
	game->profile[0].team = 0;
	game->profile[0].place = -1;
}

static void bot_of(struct ae_bot_game_bot *bot, char const *name, int team, int tier)
{
	memset(bot, 0, sizeof(*bot));
	snprintf(bot->name, sizeof(bot->name), "%s", name);
	bot->team = team;
	bot->place = -1;
	bot->tier = tier;
	bot->shooting = 4;
	bot->movement = 5;
	bot->style = 1;
	bot->favourite = 3;
	bot->kills = 10;
	bot->deaths = 8;
	bot->assists = 2;
	bot->spree = 4;
}

static void test_team(void)
{
	struct ae_bot_game game;
	struct ae_bot_game_bot bots[2];
	struct ae_bot_record *r;
	struct ae_bot_versus *v;

	ae_bot_records_init(&records);
	/* game 1: red wins 50-38; Rook7 on blue (against Chief), Tree on red (with) */
	team_game(&game, 50, 38);
	bot_of(&bots[0], "Rook7", 1, 9);
	bots[0].kills_on[0] = 4;
	bots[0].killed_by[0] = 6;
	bots[0].weapon_count = 2;
	snprintf(bots[0].weapons[0].name, sizeof(bots[0].weapons[0].name), "battle_rifle");
	bots[0].weapons[0].kills = 7;
	snprintf(bots[0].weapons[1].name, sizeof(bots[0].weapons[1].name), "frag_grenade");
	bots[0].weapons[1].kills = 3;
	bot_of(&bots[1], "Tree", 0, 9);
	EQ(ae_bot_records_add_game(&records, &game, bots, 2), 2);
	EQ(records.clock, 1);
	r = ae_bot_record_find(&records, "ROOK7");
	CHECK(r != NULL);
	if (!r)
		return;
	STR(r->key, "rook7");
	STR(r->name, "Rook7");
	STR(r->first_playlist, "team-slayer");
	EQ(r->games, 1); EQ(r->wins, 0); EQ(r->ties, 0); EQ(r->first, 1); EQ(r->last, 1);
	v = ae_bot_record_versus(&records, "chief", "Rook7");
	CHECK(v != NULL);
	if (v)
	{
		EQ(v->against, 1); EQ(v->you_won, 1); EQ(v->it_won, 0); EQ(v->with, 0);
		EQ(v->its_kills, 4); EQ(v->your_kills, 6);
		CHECK(v->lastgame.has);
		STR(v->lastgame.map, "prisoner");
		EQ(v->lastgame.result, AE_BOT_RECORD_LOSS); EQ(v->lastgame.its_kills, 4); EQ(v->lastgame.place, -1);
	}
	v = ae_bot_record_versus(&records, "chief", "tree");
	CHECK(v != NULL && v->with == 1 && v->with_won == 1 && v->against == 0);
	/* game 2: a tie 40-40, Rook7 at tier 12, more battle rifle kills, a new best spree */
	team_game(&game, 40, 40);
	bot_of(&bots[0], "rook7", 1, 12);
	bots[0].spree = 9;
	bots[0].weapon_count = 1;
	snprintf(bots[0].weapons[0].name, sizeof(bots[0].weapons[0].name), "battle_rifle");
	bots[0].weapons[0].kills = 5;
	bots[0].kills_on[0] = 1;
	EQ(ae_bot_records_add_game(&records, &game, bots, 1), 1);
	EQ(r->games, 2); EQ(r->ties, 1); EQ(r->wins, 0);
	EQ(r->kills, 20); EQ(r->deaths, 16); EQ(r->assists, 4); EQ(r->best_spree, 9);
	EQ(r->tier_low, 9); EQ(r->tier_high, 12); EQ(r->last, 2); EQ(r->first, 1);
	/* (the name as last seen) */
	STR(r->name, "rook7");
	EQ(r->weapon_count, 2);
	STR(r->weapons[0].name, "battle_rifle");
	EQ(r->weapons[0].kills, 12);
	v = ae_bot_record_versus(&records, "chief", "rook7");
	CHECK(v != NULL && v->against == 2 && v->you_won == 1 && v->it_won == 0 && v->its_kills == 5);
	if (v)
		EQ(v->lastgame.result, AE_BOT_RECORD_TIE);
	/* (no name: nothing) */
	bot_of(&bots[0], "  ", 0, 1);
	EQ(ae_bot_records_add_game(&records, &game, bots, 1), 0);
	EQ(records.count, 2);
}

static void test_ffa(void)
{
	struct ae_bot_game game;
	struct ae_bot_game_bot bots[2];
	struct ae_bot_versus *v;

	ae_bot_records_init(&records);
	memset(&game, 0, sizeof(game));
	snprintf(game.playlist, sizeof(game.playlist), "free-for-all");
	game.players = 8;
	game.profile_count = 2;
	snprintf(game.profile[0].key, sizeof(game.profile[0].key), "chief");
	game.profile[0].team = -1;
	game.profile[0].place = 2;
	snprintf(game.profile[1].key, sizeof(game.profile[1].key), "arbiter");
	game.profile[1].team = -1;
	game.profile[1].place = 6;
	/* top half of 8 wins: places 0 .. 3 */
	EQ(ae_bot_record_result(&game, -1, 3), AE_BOT_RECORD_WIN);
	EQ(ae_bot_record_result(&game, -1, 4), AE_BOT_RECORD_LOSS);
	game.players = 7;
	EQ(ae_bot_record_result(&game, -1, 3), AE_BOT_RECORD_WIN);
	EQ(ae_bot_record_result(&game, -1, 4), AE_BOT_RECORD_LOSS);
	EQ(ae_bot_record_result(&game, -1, -1), AE_BOT_RECORD_LOSS);
	game.players = 8;
	bot_of(&bots[0], "Stain", -1, 5);
	bots[0].place = 4;
	bot_of(&bots[1], "Dork", -1, 5);
	bots[1].place = 2;
	EQ(ae_bot_records_add_game(&records, &game, bots, 2), 2);
	/* (a free for all is always against: above / below by place, the same place neither) */
	v = ae_bot_record_versus(&records, "chief", "stain");
	CHECK(v != NULL && v->against == 1 && v->you_won == 1 && v->it_won == 0 && v->lastgame.place == 4);
	v = ae_bot_record_versus(&records, "arbiter", "stain");
	CHECK(v != NULL && v->against == 1 && v->you_won == 0 && v->it_won == 1);
	v = ae_bot_record_versus(&records, "chief", "dork");
	CHECK(v != NULL && v->against == 1 && v->you_won == 0 && v->it_won == 0);
	EQ(records.versus_count, 4);
	CHECK(ae_bot_record_find(&records, "stain")->wins == 0);
	CHECK(ae_bot_record_find(&records, "dork")->wins == 1);
}

static void test_rival_line(void)
{
	struct ae_bot_versus v;
	char head[32], sentence[256], kills[128];

	memset(&v, 0, sizeof(v));
	ae_bot_record_rival_line(NULL, head, sizeof(head), sentence, sizeof(sentence), kills, sizeof(kills));
	STR(head, "");
	v.against = 1;
	v.you_won = 1;
	ae_bot_record_rival_line(&v, head, sizeof(head), sentence, sizeof(sentence), kills, sizeof(kills));
	STR(head, "FIRST MEETING");
	STR(sentence, "Your first game against it: you placed above it.");
	v.you_won = 0;
	v.it_won = 1;
	ae_bot_record_rival_line(&v, head, sizeof(head), sentence, sizeof(sentence), NULL, 0);
	STR(sentence, "Your first game against it: it placed above you.");
	memset(&v, 0, sizeof(v));
	v.with = 1;
	ae_bot_record_rival_line(&v, head, sizeof(head), sentence, sizeof(sentence), NULL, 0);
	STR(head, "FIRST MEETING");
	STR(sentence, "Your first game with it: on your team.");
	v.with = 3;
	v.with_won = 2;
	ae_bot_record_rival_line(&v, head, sizeof(head), sentence, sizeof(sentence), NULL, 0);
	STR(head, "TEAMMATE");
	STR(sentence, "Only on your team so far: 3 games, 2 wins.");
	memset(&v, 0, sizeof(v));
	v.against = 10;
	v.you_won = 7;
	v.it_won = 3;
	v.its_kills = 23;
	v.your_kills = 31;
	ae_bot_record_rival_line(&v, head, sizeof(head), sentence, sizeof(sentence), kills, sizeof(kills));
	STR(head, "YOU LEAD 7-3");
	STR(sentence, "You have the edge. It has killed you 23 times; you have killed it 31.");
	STR(kills, "It has killed you 23 times; you have killed it 31.");
	CHECK(!ae_bot_record_is_rival(&v));
	v.you_won = 6;
	v.it_won = 8;
	v.its_kills = 1;
	ae_bot_record_rival_line(&v, head, sizeof(head), sentence, sizeof(sentence), kills, sizeof(kills));
	STR(head, "IT LEADS 8-6");
	STR(kills, "It has killed you 1 time; you have killed it 31.");
	CHECK(ae_bot_record_is_rival(&v));
	v.you_won = v.it_won = 5;
	v.its_kills = v.your_kills = 18;
	ae_bot_record_rival_line(&v, head, sizeof(head), sentence, sizeof(sentence), kills, sizeof(kills));
	STR(head, "EVEN 5-5");
	STR(sentence, "Nothing between you. Kills 18 each way.");
	v.against = 9;
	CHECK(!ae_bot_record_is_rival(&v));
	EQ(ae_bot_record_kd100(17, 8), 213);
	EQ(ae_bot_record_kd100(5, 0), 500);
	EQ(ae_bot_record_kd100(0, 4), 0);
}

static void test_bounds(void)
{
	struct ae_bot_game game;
	struct ae_bot_game_bot bot;
	int index;
	char name[32];

	ae_bot_records_init(&records);
	team_game(&game, 1, 0);
	/* 512 bots: the whole 451 name pool never evicts */
	for (index = 0; index < AE_BOT_RECORD_BOTS; index++)
	{
		snprintf(name, sizeof(name), "bot%d", index);
		bot_of(&bot, name, 1, 3);
		EQ(ae_bot_records_add_game(&records, &game, &bot, 1), 1);
	}
	EQ(records.count, AE_BOT_RECORD_BOTS);
	EQ(records.versus_count, AE_BOT_RECORD_BOTS);
	/* (bot0 again: bot1 is now the oldest, and goes with its versus line) */
	bot_of(&bot, "bot0", 1, 3);
	ae_bot_records_add_game(&records, &game, &bot, 1);
	bot_of(&bot, "newcomer", 1, 3);
	ae_bot_records_add_game(&records, &game, &bot, 1);
	EQ(records.count, AE_BOT_RECORD_BOTS);
	CHECK(ae_bot_record_find(&records, "bot1") == NULL);
	CHECK(ae_bot_record_versus(&records, "chief", "bot1") == NULL);
	CHECK(ae_bot_record_find(&records, "newcomer") != NULL);
	CHECK(ae_bot_record_versus(&records, "chief", "newcomer") != NULL);
	EQ(records.versus_count, AE_BOT_RECORD_BOTS);
	/* versus lines: 1024 at most, the oldest replaced */
	ae_bot_records_init(&records);
	game.profile_count = 4;
	for (index = 0; index < 4; index++)
	{
		snprintf(game.profile[index].key, sizeof(game.profile[index].key), "p%d", index);
		game.profile[index].team = 0;
	}
	for (index = 0; index < AE_BOT_RECORD_VERSUS / 4 + 1; index++)
	{
		snprintf(name, sizeof(name), "b%d", index);
		bot_of(&bot, name, 1, 3);
		ae_bot_records_add_game(&records, &game, &bot, 1);
	}
	EQ(records.versus_count, AE_BOT_RECORD_VERSUS);
	CHECK(ae_bot_record_versus(&records, "p0", "b0") == NULL);
	CHECK(ae_bot_record_versus(&records, "p3", "b256") != NULL);
	/* weapons: the 4 with the most kills, most first */
	ae_bot_records_init(&records);
	game.profile_count = 0;
	for (index = 0; index < AE_BOT_RECORD_WEAPONS + 1; index++)
	{
		bot_of(&bot, "Tool", 1, 3);
		bot.weapon_count = 1;
		snprintf(bot.weapons[0].name, sizeof(bot.weapons[0].name), "gun:%d", index);
		bot.weapons[0].kills = index == AE_BOT_RECORD_WEAPONS ? 1 : 5 + index;
		ae_bot_records_add_game(&records, &game, &bot, 1);
	}
	{
		struct ae_bot_record *r = ae_bot_record_find(&records, "tool");

		CHECK(r != NULL);
		if (r)
		{
			EQ(r->weapon_count, AE_BOT_RECORD_WEAPONS);
			/* (":" in a name made "?") */
			STR(r->weapons[0].name, "gun?4");
			EQ(r->weapons[0].kills, 9);
			EQ(r->weapons[AE_BOT_RECORD_WEAPONS - 1].kills, 5);
		}
	}
	EQ(ae_bot_records_forget(&records, "TOOL"), 1);
	EQ(ae_bot_records_forget(&records, "tool"), 0);
	EQ(records.count, 0);
}

static unsigned long sum(char const *body)
{
	unsigned long hash = 2166136261u;

	for (; *body; body++)
		hash = ((hash ^ (unsigned char)*body) * 16777619u) & 0xffffffffu;
	return hash;
}

static void test_file(void)
{
	struct ae_bot_game game;
	struct ae_bot_game_bot bots[2];
	struct ae_bot_record *r;
	struct ae_bot_versus *v;
	long length;
	char *at;
	int index;

	ae_bot_records_init(&records);
	team_game(&game, 50, 38);
	bot_of(&bots[0], "Rook7", 1, 9);
	bots[0].kills_on[0] = 4;
	bots[0].killed_by[0] = 6;
	bots[0].weapon_count = 1;
	snprintf(bots[0].weapons[0].name, sizeof(bots[0].weapons[0].name), "battle_rifle");
	bots[0].weapons[0].kills = 7;
	bot_of(&bots[1], "Tree", 0, 3);
	bots[1].style = -1;
	ae_bot_records_add_game(&records, &game, bots, 2);
	length = ae_bot_records_write(&records, text, sizeof(text));
	CHECK(length > 0);
	CHECK(strstr(text, "clock = 1\n") != NULL);
	CHECK(strstr(text, "bot = rook7 | Rook7 | 1 1 team-slayer | 1 0 0 10 8 2 4 | 4 5 9 9 1 3 -1 | battle_rifle:7\n") != NULL);
	CHECK(strstr(text, "versus = chief | rook7 | 1 1 0 0 0 6 4 1\n") != NULL);
	CHECK(strstr(text, "lastgame = chief | rook7 | team-slayer | prisoner | 1 10 8 2 4 4 -1\n") != NULL);
	CHECK(strstr(text, "versus = chief | tree | 0 0 0 1 1 0 0 1\n") != NULL);
	EQ(ae_bot_records_read(&again, text), AE_BOT_RECORD_READ_OK);
	EQ(again.count, 2); EQ(again.versus_count, 2); EQ(again.clock, 1); EQ(again.newer, 0);
	r = ae_bot_record_find(&again, "tree");
	CHECK(r != NULL && r->style == -1 && r->rank == AE_BOT_RECORD_NO_RANK && r->wins == 1 && r->weapon_count == 0);
	v = ae_bot_record_versus(&again, "chief", "rook7");
	CHECK(v != NULL && v->lastgame.has && v->lastgame.its_kills == 4 && v->your_kills == 6);
	/* (written again: the same text) */
	CHECK(ae_bot_records_write(&again, copy, sizeof(copy)) == length && !strcmp(copy, text));
	/* a hand edit: the checksum no longer matches: empty, edited */
	memcpy(copy, text, (size_t)length + 1);
	at = strstr(copy, "| 1 0 0 10");
	CHECK(at != NULL);
	if (at)
		at[2] = '9';
	EQ(ae_bot_records_read(&again, copy), AE_BOT_RECORD_READ_EDITED);
	EQ(again.count, 0);
	EQ(again.edited, 1);
	/* no checksum with records: edited; no header: bad; a header alone: empty and fine */
	EQ(ae_bot_records_read(&again, "version = 1\nbot = a | A | 1 1 x | 1 0 0 0 0 0 0\n"), AE_BOT_RECORD_READ_EDITED);
	EQ(ae_bot_records_read(&again, "bot = a | A | 1 1 x | 1 0 0 0 0 0 0\n"), AE_BOT_RECORD_READ_BAD);
	EQ(ae_bot_records_read(&again, "# AE bot records\nversion = 1\n"), AE_BOT_RECORD_READ_OK);
	EQ(again.count, 0);
	/* a newer file: read, flagged newer; its unknown kinds and extra fields skipped; a short identity group read in part */
	snprintf(copy, sizeof(copy), "# AE bot records\nversion = 9\nclock = 9\n"
		"bot = zed | Zed | 2 9 ctf | 3 1 1 9 9 1 4 | 2 2 5 7 | needler:3 | more\n"
		"medal = zed | killjoy | 3\n"
		"versus = chief | zed | 3 1 2 0 0 4 5 9 77\n"
		"lastgame = chief | zed | ctf | bloodgulch | 1 2 3 4 5 6 -1 88\n");
	index = (int)strlen(copy);
	snprintf(copy + index, sizeof(copy) - (size_t)index, "checksum = %08lx\n", sum(copy));
	EQ(ae_bot_records_read(&again, copy), AE_BOT_RECORD_READ_OK);
	EQ(again.newer, 1);
	EQ(again.clock, 9);
	r = ae_bot_record_find(&again, "zed");
	CHECK(r != NULL && r->games == 3 && r->tier_high == 7 && r->style == -1 && r->weapon_count == 1);
	v = ae_bot_record_versus(&again, "chief", "zed");
	CHECK(v != NULL && v->its_kills == 5 && v->lastgame.has && v->lastgame.result == AE_BOT_RECORD_LOSS);
	/* a key twice, a versus twice: edited */
	snprintf(copy, sizeof(copy), "# AE bot records\nversion = 1\n"
		"bot = zed | Zed | 1 1 ctf | 1 0 0 0 0 0 0 | 1 1 1 1\nbot = zed | ZED | 1 1 ctf | 1 0 0 0 0 0 0 | 1 1 1 1\n");
	index = (int)strlen(copy);
	snprintf(copy + index, sizeof(copy) - (size_t)index, "checksum = %08lx\n", sum(copy));
	EQ(ae_bot_records_read(&again, copy), AE_BOT_RECORD_READ_EDITED);
	snprintf(copy, sizeof(copy), "# AE bot records\nversion = 1\n"
		"bot = zed | Zed | 1 1 ctf | 1 0 0 0 0 0 0 | 1 1 1 1\nversus = p | zed | 1 0 0 0 0 0 0 1\n"
		"versus = p | zed | 1 0 0 0 0 0 0 1\n");
	index = (int)strlen(copy);
	snprintf(copy + index, sizeof(copy) - (size_t)index, "checksum = %08lx\n", sum(copy));
	EQ(ae_bot_records_read(&again, copy), AE_BOT_RECORD_READ_EDITED);
	/* every table full of long names fits the text bound and reads back */
	ae_bot_records_init(&records);
	for (index = 0; index < AE_BOT_RECORD_BOTS; index++)
	{
		struct ae_bot_record *b = &records.bot[records.count++];
		int item;

		memset(b, 0, sizeof(*b));
		snprintf(b->key, sizeof(b->key), "%046d", index);
		snprintf(b->name, sizeof(b->name), "%046d", index);
		snprintf(b->first_playlist, sizeof(b->first_playlist), "%030d", index);
		b->games = b->kills = b->deaths = b->assists = b->best_spree = 2000000000l;
		b->first = b->last = 2000000000l;
		b->tier_low = b->tier_high = 19;
		b->rank = -1;
		b->weapon_count = AE_BOT_RECORD_WEAPONS;
		for (item = 0; item < AE_BOT_RECORD_WEAPONS; item++)
		{
			snprintf(b->weapons[item].name, sizeof(b->weapons[item].name), "%022d", item);
			b->weapons[item].kills = 2000000000l;
		}
	}
	for (index = 0; index < AE_BOT_RECORD_VERSUS; index++)
	{
		struct ae_bot_versus *w = &records.versus[records.versus_count++];

		memset(w, 0, sizeof(*w));
		snprintf(w->profile, sizeof(w->profile), "%046d", index % 4);
		snprintf(w->bot, sizeof(w->bot), "%046d", index / 4);
		w->against = w->your_kills = w->its_kills = w->last = 2000000000l;
		w->lastgame.has = 1;
		snprintf(w->lastgame.playlist, sizeof(w->lastgame.playlist), "%030d", index);
		snprintf(w->lastgame.map, sizeof(w->lastgame.map), "%046d", index);
		w->lastgame.kills = w->lastgame.its_kills = 2000000000l;
	}
	length = ae_bot_records_write(&records, text, sizeof(text));
	CHECK(length > 0 && length < (long)sizeof(text));
	EQ(ae_bot_records_read(&again, text), AE_BOT_RECORD_READ_OK);
	EQ(again.count, AE_BOT_RECORD_BOTS);
	EQ(again.versus_count, AE_BOT_RECORD_VERSUS);
}

/* ---- (rc2) version 2: the kinds, medals, maps and the shared numbers of a bot's record */

static void extras(struct ae_bot_game_bot *bot, int mode, int won, long seconds, int with_v3)
{
	struct ae_rs_input in;
	static struct ae_pgcr_summary summary;

	memset(&in, 0, sizeof(in));
	in.mode = mode;
	in.won = won;
	in.kills = bot->kills;
	in.deaths = bot->deaths;
	in.assists = bot->assists;
	in.seconds = seconds;
	in.fired = 50;
	in.hit = 20;
	in.objective[0] = in.objective[1] = in.objective[2] = -1;
	in.laps = in.best_lap_ticks = in.ball_carrier_kills = in.as_ball_carrier = -1;
	memset(&summary, 0, sizeof(summary));
	summary.valid = 1;
	summary.extended = with_v3;
	summary.best_spree = (unsigned short)bot->spree;
	summary.damage_dealt = with_v3 ? 800 : 0;
	summary.damage_taken = with_v3 ? 700 : 0;
	summary.bomb_plants = (unsigned char)(with_v3 ? 2 : 0);
	in.summary = &summary;
	ae_rs_game_make(&bot->x, &in);
	bot->has_x = 1;
}

static void medal_of(struct ae_bot_game_bot *bot, int id, int count)
{
	bot->medals[bot->medal_count].id = (unsigned char)id;
	bot->medals[bot->medal_count++].count = (unsigned char)count;
}

static void test_rc2_add(void)
{
	struct ae_bot_game game;
	struct ae_bot_game_bot bot;
	struct ae_bot_record *r;
	int index;

	ae_bot_records_init(&records);
	/* game 1: a flag game won by red; Rook7 (red) with the new numbers, two medals, three weapons */
	team_game(&game, 50, 38);
	game.mode = AE_RS_KIND_FLAG;
	snprintf(game.map, sizeof(game.map), "Blood Gulch");
	bot_of(&bot, "Rook7", 0, 9);
	extras(&bot, AE_RS_KIND_FLAG, 1, 600, 1);
	medal_of(&bot, 5, 3);
	medal_of(&bot, 9, 1);
	bot.weapon_count = 3;
	for (index = 0; index < 3; index++)
	{
		snprintf(bot.weapons[index].name, sizeof(bot.weapons[index].name), "gun%d", index);
		bot.weapons[index].kills = 3 - index;
	}
	EQ(ae_bot_records_add_game(&records, &game, &bot, 1), 1);
	/* game 2: a slayer tie (equal scores) on the same map, an older host's summary (no version 3) */
	team_game(&game, 40, 40);
	game.mode = AE_RS_KIND_SLAYER;
	snprintf(game.map, sizeof(game.map), "blood gulch");
	bot_of(&bot, "rook7", 0, 9);
	extras(&bot, AE_RS_KIND_SLAYER, 0, 300, 0);
	medal_of(&bot, 5, 2);
	EQ(ae_bot_records_add_game(&records, &game, &bot, 1), 1);
	/* game 3: a loss with no new numbers (a caller that has none) */
	team_game(&game, 10, 40);
	game.mode = AE_RS_KIND_SLAYER;
	snprintf(game.map, sizeof(game.map), "Prisoner");
	bot_of(&bot, "rook7", 0, 9);
	EQ(ae_bot_records_add_game(&records, &game, &bot, 1), 1);
	r = ae_bot_record_find(&records, "rook7");
	CHECK(r != NULL);
	if (!r)
		return;
	EQ(r->games, 3); EQ(r->wins, 1); EQ(r->ties, 1);
	EQ(r->x.counted, 2); EQ(r->x.counted_v3, 1); EQ(r->x.seconds, 900); EQ(r->x.dealt, 800); EQ(r->x.bomb_plants, 2);
	EQ(r->x.fired, 100); EQ(r->x.hit, 40);
	/* (a bot's ties are its record's own: the numbers' ties stay out of it) */
	EQ(r->x.ties, 0);
	/* the kinds: the games counted, by the game's kind */
	EQ(r->kind[AE_RS_KIND_FLAG].games, 1); EQ(r->kind[AE_RS_KIND_FLAG].wins, 1); EQ(r->kind[AE_RS_KIND_FLAG].kills, 10);
	EQ(r->kind[AE_RS_KIND_SLAYER].games, 1); EQ(r->kind[AE_RS_KIND_SLAYER].wins, 0); EQ(r->kind[AE_RS_KIND_SLAYER].deaths, 8);
	EQ(r->kind[AE_RS_KIND_RACE].games, 0);
	/* the medals, summed by id */
	EQ(r->medals[5], 5); EQ(r->medals[9], 1); EQ(r->medals[6], 0);
	/* the maps: the case aside one map, the third game had none */
	EQ(r->map_count, 1); EQ(r->map[0].games, 2); EQ(r->map[0].wins, 1); EQ(r->map[0].kills, 20);
	/* a bot's spree is its record's best */
	EQ(r->best_spree, 4);
	/* a bot's maps are kept to AE_BOT_RECORD_MAPS, the fewest played replaced */
	for (index = 0; index < AE_BOT_RECORD_MAPS + 2; index++)
	{
		team_game(&game, 1, 0);
		game.mode = AE_RS_KIND_SLAYER;
		snprintf(game.map, sizeof(game.map), "map%d", index);
		bot_of(&bot, "Rook7", 0, 9);
		extras(&bot, AE_RS_KIND_SLAYER, 1, 60, 1);
		ae_bot_records_add_game(&records, &game, &bot, 1);
	}
	r = ae_bot_record_find(&records, "rook7");
	EQ(r->map_count, AE_BOT_RECORD_MAPS);
	/* a replaced bot's record starts clean (eviction memset) */
	ae_bot_records_init(&records);
	team_game(&game, 1, 0);
	for (index = 0; index < AE_BOT_RECORD_BOTS; index++)
	{
		char name[32];

		snprintf(name, sizeof(name), "bot%d", index);
		bot_of(&bot, name, 0, 3);
		extras(&bot, AE_RS_KIND_SLAYER, 1, 60, 1);
		medal_of(&bot, 2, 4);
		ae_bot_records_add_game(&records, &game, &bot, 1);
	}
	bot_of(&bot, "newcomer", 0, 3);
	ae_bot_records_add_game(&records, &game, &bot, 1);
	r = ae_bot_record_find(&records, "newcomer");
	CHECK(r != NULL && r->x.counted == 0 && r->medals[2] == 0 && r->map_count == 0 && r->kind[0].games == 0);
}

static void test_rc2_file(void)
{
	struct ae_bot_game game;
	struct ae_bot_game_bot bot;
	struct ae_bot_record *r;
	long length;
	char *line;
	int index, medal_lines = 0;

	ae_bot_records_init(&records);
	team_game(&game, 50, 38);
	game.mode = AE_RS_KIND_FLAG;
	snprintf(game.map, sizeof(game.map), "Blood Gulch");
	bot_of(&bot, "Rook7", 0, 9);
	extras(&bot, AE_RS_KIND_FLAG, 1, 600, 1);
	for (index = 0; index < AE_PGCR_SUMMARY_MEDALS; index++)
	{
		bot.medals[index].id = (unsigned char)(index + 1);
		bot.medals[index % AE_PGCR_SUMMARY_MEDALS].count = 200;
	}
	bot.medal_count = AE_PGCR_SUMMARY_MEDALS;
	ae_bot_records_add_game(&records, &game, &bot, 1);
	r = ae_bot_record_find(&records, "rook7");
	/* a bot with every medal many times over (the longest medals line) */
	for (index = 0; index < AE_MEDAL_COUNT; index++)
		r->medals[index] = 2000000000ul;
	length = ae_bot_records_write(&records, text, sizeof(text));
	CHECK(length > 0 && strstr(text, "version = 2\n") && strstr(text, "botx = rook7 | ") && strstr(text, "botkinds = rook7 | ") &&
		strstr(text, "botmedals = rook7 | ") && strstr(text, "botmap = rook7 | Blood Gulch | 1 1 10 8\n"));
	/* no line is as long as a version 1 reader's limit (a longer one would make it refuse the whole file); the medals are
	split over lines; the old kinds' lines are as they were */
	for (line = text; *line;)
	{
		size_t n = strcspn(line, "\n");

		CHECK(n < AE_BOT_RECORD_OLD_LINE);
		medal_lines += !strncmp(line, "botmedals = ", 12);
		line += n + (line[n] ? 1 : 0);
	}
	CHECK(medal_lines >= 2);
	CHECK(strstr(text, "bot = rook7 | Rook7 | 1 1 team-slayer | 1 1 0 10 8 2 4 | 4 5 9 9 1 3 -1 | \n") != NULL ||
		strstr(text, "bot = rook7 | Rook7 | 1 1 team-slayer | 1 1 0 10 8 2 4 | 4 5 9 9 1 3 -1 |") != NULL);
	EQ(ae_bot_records_read(&again, text), AE_BOT_RECORD_READ_OK);
	EQ(again.newer, 0);
	r = ae_bot_record_find(&again, "rook7");
	CHECK(r != NULL && r->x.counted == 1 && r->x.seconds == 600 && r->x.dealt == 800 && r->kind[AE_RS_KIND_FLAG].games == 1 &&
		r->kind[AE_RS_KIND_FLAG].kills == 10 && r->map_count == 1 && !strcmp(r->map[0].name, "Blood Gulch"));
	for (index = 0; index < AE_MEDAL_COUNT; index++)
		CHECK(r->medals[index] == 2000000000ul);
	/* the second write is the same bytes (the checksum too) */
	CHECK(ae_bot_records_write(&again, copy, sizeof(copy)) == length && !strcmp(copy, text));
	/* a version 1 file: read as ever, the new numbers empty, not newer; a game then counts and it writes as version 2 */
	snprintf(copy, sizeof(copy), "# AE bot records\nversion = 1\nclock = 1\n"
		"bot = zed | Zed | 1 1 ctf | 3 1 1 9 9 1 4 | 2 2 5 7 | needler:3\n");
	index = (int)strlen(copy);
	snprintf(copy + index, sizeof(copy) - (size_t)index, "checksum = %08lx\n", sum(copy));
	EQ(ae_bot_records_read(&again, copy), AE_BOT_RECORD_READ_OK);
	r = ae_bot_record_find(&again, "zed");
	CHECK(r != NULL && !again.newer && r->games == 3 && r->x.counted == 0 && r->map_count == 0 && r->medals[5] == 0);
	/* lines of a bot with no bot line, and bad lines, are dropped; the rest read; the checksum still covers all */
	snprintf(copy, sizeof(copy), "# AE bot records\nversion = 2\nclock = 1\n"
		"botx = ghost | 1 2 3\nbotkinds = ghost | 1 1 1 1 1\nbotmedals = ghost | 5:1\nbotmap = ghost | gulch | 1 1 1 1\n"
		"bot = zed | Zed | 1 1 ctf | 3 1 1 9 9 1 4 | 2 2 5 7 |\n"
		"botx = zed | 1 two\nbotkinds = zed | 1 1 1\nbotmedals = zed | 5:x,999:3,7:2\nbotmap = zed | gulch | 1 x 1 1\nbotmap = zed | ok | 3 2 7 4\n");
	index = (int)strlen(copy);
	snprintf(copy + index, sizeof(copy) - (size_t)index, "checksum = %08lx\n", sum(copy));
	EQ(ae_bot_records_read(&again, copy), AE_BOT_RECORD_READ_OK);
	r = ae_bot_record_find(&again, "zed");
	CHECK(r != NULL && ae_bot_record_find(&again, "ghost") == NULL && r->x.counted == 0 && r->kind[0].games == 0 &&
		r->medals[7] == 2 && r->map_count == 1 && r->map[0].games == 3);
	/* forget takes the new numbers with the bot */
	EQ(ae_bot_records_forget(&again, "zed"), 1);
	EQ(again.count, 0);
	/* every table at its limits, with the new lines, fits the text bound and reads back */
	ae_bot_records_init(&records);
	for (index = 0; index < AE_BOT_RECORD_BOTS; index++)
	{
		struct ae_bot_record *b = &records.bot[records.count++];
		int item;

		memset(b, 0, sizeof(*b));
		snprintf(b->key, sizeof(b->key), "%046d", index);
		snprintf(b->name, sizeof(b->name), "%046d", index);
		snprintf(b->first_playlist, sizeof(b->first_playlist), "%030d", index);
		b->games = b->kills = b->deaths = b->assists = b->best_spree = 2000000000l;
		b->tier_low = b->tier_high = 19;
		b->rank = -1;
		b->weapon_count = AE_BOT_RECORD_WEAPONS;
		for (item = 0; item < AE_BOT_RECORD_WEAPONS; item++)
		{
			snprintf(b->weapons[item].name, sizeof(b->weapons[item].name), "%022d", item);
			b->weapons[item].kills = 2000000000l;
		}
		for (item = 0; item < (int)(sizeof(b->x) / sizeof(long)); item++)
			((long *)&b->x)[item] = 2000000000l;
		for (item = 0; item < AE_RS_KINDS; item++)
			b->kind[item].games = b->kind[item].wins = b->kind[item].kills = b->kind[item].deaths = b->kind[item].assists = 2000000000l;
		for (item = 0; item < AE_MEDAL_COUNT; item++)
			b->medals[item] = 2000000000ul;
		for (item = 0; item < AE_BOT_RECORD_MAPS; item++)
		{
			snprintf(b->map[b->map_count].name, sizeof(b->map[0].name), "%046d", item);
			b->map[b->map_count].games = b->map[b->map_count].wins = b->map[b->map_count].kills = b->map[b->map_count].deaths = 2000000000l;
			b->map_count++;
		}
	}
	length = ae_bot_records_write(&records, text, sizeof(text));
	CHECK(length > 0 && length < (long)sizeof(text));
	EQ(ae_bot_records_read(&again, text), AE_BOT_RECORD_READ_OK);
	EQ(again.count, AE_BOT_RECORD_BOTS);
	EQ(again.bot[511].map_count, AE_BOT_RECORD_MAPS);
}

static void test_demo(void)
{
	struct ae_bot_versus *v;
	int index, rivals = 0, teammates = 0, firsts = 0;
	char head[64];

	EQ(ae_bot_records_demo(&records, 4, 0, "Chief"), AE_BOT_RECORD_DEMO_BOTS);
	EQ(records.count, AE_BOT_RECORD_DEMO_BOTS);
	EQ(records.versus_count, AE_BOT_RECORD_DEMO_BOTS);
	for (index = 0; index < records.versus_count; index++)
	{
		v = &records.versus[index];
		CHECK(v->you_won + v->it_won <= v->against);
		CHECK(v->with_won <= v->with);
		CHECK(v->lastgame.has);
		rivals += ae_bot_record_is_rival(v);
		ae_bot_record_rival_line(v, head, sizeof(head), NULL, 0, NULL, 0);
		teammates += !strcmp(head, "TEAMMATE");
		firsts += !strcmp(head, "FIRST MEETING");
	}
	CHECK(rivals >= 1);
	EQ(teammates, 1);
	CHECK(firsts >= 1);
	for (index = 0; index < records.count; index++)
	{
		struct ae_bot_record const *r = &records.bot[index];

		CHECK(r->key[0] && r->name[0]);
		CHECK(r->wins + r->ties <= r->games);
		CHECK(r->tier_low <= r->tier_high);
		CHECK(r->style >= 0);
	}
	/* (the same seed: the same store; the bound) */
	ae_bot_records_demo(&again, 4, 0, "chief");
	CHECK(!strcmp(again.bot[5].name, records.bot[5].name) && again.bot[5].kills == records.bot[5].kills);
	EQ(ae_bot_records_demo(&records, 1, 500, NULL), AE_BOT_RECORD_DEMO_MOST);
	EQ(records.versus_count, 0);
	/* set: a made-up bot's numbers */
	ae_bot_records_init(&records);
	CHECK(ae_bot_records_demo_set(&records, "Zed", "chief", "games", 9, 0));
	CHECK(ae_bot_records_demo_set(&records, "zed", "chief", "wins", 12, 0));
	CHECK(ae_bot_record_find(&records, "zed")->games == 12);
	CHECK(ae_bot_records_demo_set(&records, "zed", "chief", "vs", 3, 5));
	v = ae_bot_record_versus(&records, "chief", "zed");
	CHECK(v != NULL && v->you_won == 3 && v->it_won == 5 && v->against == 8);
	CHECK(ae_bot_records_demo_set(&records, "zed", "chief", "with", 4, 0));
	CHECK(v != NULL && v->with == 4);
	CHECK(!ae_bot_records_demo_set(&records, "zed", "chief", "colour", 4, 0));
	CHECK(!ae_bot_records_demo_set(&records, "zed", "chief", "kills", -1, 0));
	CHECK(!ae_bot_records_demo_set(&records, "zed", "", "vs", 1, 1));
}

static int medal_kinds(unsigned long const *medals)
{
	int at, kinds = 0;

	for (at = 0; at < AE_MEDAL_COUNT; at++)
		kinds += medals[at] != 0;
	return kinds;
}

static void test_rc2_demo(void)
{
	int index;
	struct ae_bot_record const *r;

	ae_bot_records_demo(&records, 3, 0, "Chief");
	for (index = 0; index < records.count; index++)
	{
		long games = 0;
		int map;

		r = &records.bot[index];
		for (map = 0; map < AE_RS_KINDS; map++)
			games += r->kind[map].games;
		/* (the kinds hold the games; the numbers are counted for every game but the fourth bot's, an older record's) */
		CHECK(games == r->games);
		CHECK(index == 3 ? (r->x.counted == 2 && r->x.counted_v3 == 0 && r->x.dealt == 0) :
			(r->x.counted == r->games && r->x.counted_v3 == r->games && r->x.dealt > 0));
		CHECK(r->map_count >= 1 && r->map_count <= AE_BOT_RECORD_MAPS && r->weapon_count >= 2);
		CHECK(medal_kinds(r->medals) > 0);
	}
	/* and it is written and read back whole (the made-up store is never written, but its lines are the file's) */
	CHECK(ae_bot_records_write(&records, text, sizeof(text)) > 0);
	EQ(ae_bot_records_read(&again, text), AE_BOT_RECORD_READ_OK);
	CHECK(again.bot[0].x.counted == records.bot[0].x.counted && again.bot[0].map_count == records.bot[0].map_count);
	/* set: the numbers of a made-up bot */
	ae_bot_records_init(&records);
	CHECK(ae_bot_records_demo_set(&records, "Zed", "chief", "games", 9, 0));
	CHECK(ae_bot_records_demo_set(&records, "zed", "chief", "counted", 3, 0));
	CHECK(ae_bot_records_demo_set(&records, "zed", "chief", "counted_v3", 2, 0));
	CHECK(ae_bot_records_demo_set(&records, "zed", "chief", "seconds", 1800, 0));
	CHECK(ae_bot_records_demo_set(&records, "zed", "chief", "plants", 4, 0));
	CHECK(ae_bot_records_demo_set(&records, "zed", "chief", "defuses", 2, 0));
	CHECK(ae_bot_records_demo_set(&records, "zed", "chief", "hill", 90, 0));
	CHECK(ae_bot_records_demo_set(&records, "zed", "chief", "ball", 60, 0));
	CHECK(ae_bot_records_demo_set(&records, "zed", "chief", "caps", 5, 0));
	CHECK(ae_bot_records_demo_set(&records, "zed", "chief", "returns", 6, 0));
	CHECK(ae_bot_records_demo_set(&records, "zed", "chief", "grabs", 7, 0));
	CHECK(ae_bot_records_demo_set(&records, "zed", "chief", "carrier", 3, 0));
	CHECK(ae_bot_records_demo_set(&records, "zed", "chief", "laps", 8, 0));
	r = ae_bot_record_find(&records, "zed");
	CHECK(r->x.counted == 3 && r->x.counted_v3 == 2 && r->x.seconds == 1800 && r->x.bomb_plants == 4 && r->x.bomb_defuses == 2 &&
		r->x.hill_ticks == 2700 && r->x.ball_ticks == 1800 && r->x.flag_caps == 5 && r->x.laps == 8 && r->x.as_carrier == 3);
	CHECK(!ae_bot_records_demo_set(&records, "zed", "chief", "plants", -1, 0));
}

static void test_list(void)
{
	int order[AE_BOT_RECORD_BOTS], count, index;

	ae_bot_records_init(&records);
	CHECK(ae_bot_records_demo_set(&records, "Ann", "chief", "vs", 1, 1));
	CHECK(ae_bot_records_demo_set(&records, "Bo", "chief", "vs", 2, 6));
	CHECK(ae_bot_records_demo_set(&records, "Cy", "chief", "with", 3, 0));
	CHECK(ae_bot_records_demo_set(&records, "Di", "other", "vs", 9, 9));
	ae_bot_record_versus(&records, "chief", "ann")->last = 50;
	ae_bot_record_versus(&records, "chief", "bo")->last = 10;
	ae_bot_record_versus(&records, "chief", "cy")->last = 30;
	/* (Di met another profile only: not on chief's list) */
	count = ae_bot_records_list(&records, "Chief", AE_BOT_RECORD_SORT_GAMES, order, AE_BOT_RECORD_BOTS);
	EQ(count, 3);
	CHECK(!strcmp(records.bot[order[0]].key, "bo") && !strcmp(records.bot[order[1]].key, "cy") &&
		!strcmp(records.bot[order[2]].key, "ann"));
	count = ae_bot_records_list(&records, "chief", AE_BOT_RECORD_SORT_LAST, order, AE_BOT_RECORD_BOTS);
	CHECK(count == 3 && !strcmp(records.bot[order[0]].key, "ann") && !strcmp(records.bot[order[2]].key, "bo"));
	count = ae_bot_records_list(&records, "chief", AE_BOT_RECORD_SORT_RECORD, order, AE_BOT_RECORD_BOTS);
	CHECK(count == 3 && !strcmp(records.bot[order[0]].key, "bo"));
	/* (a cut list, no profile, a sort out of range) */
	EQ(ae_bot_records_list(&records, "chief", AE_BOT_RECORD_SORT_GAMES, order, 2), 2);
	EQ(ae_bot_records_list(&records, "", AE_BOT_RECORD_SORT_GAMES, order, AE_BOT_RECORD_BOTS), 0);
	EQ(ae_bot_records_list(&records, "chief", 9, order, AE_BOT_RECORD_BOTS), 3);
	CHECK(!strcmp(ae_bot_record_sort_name(AE_BOT_RECORD_SORT_LAST), "LAST MET"));
	/* (the made-up store: every bot met by its profile, and the RIVAL among them) */
	ae_bot_records_demo(&records, 4, 0, "chief");
	count = ae_bot_records_list(&records, "chief", AE_BOT_RECORD_SORT_GAMES, order, AE_BOT_RECORD_BOTS);
	EQ(count, AE_BOT_RECORD_DEMO_BOTS);
	for (index = 1; index < count; index++)
	{
		struct ae_bot_versus *a = ae_bot_record_versus(&records, "chief", records.bot[order[index - 1]].key);
		struct ae_bot_versus *b = ae_bot_record_versus(&records, "chief", records.bot[order[index]].key);

		CHECK(a->against + a->with >= b->against + b->with);
	}
}

int main(void)
{
	test_demo();
	test_rc2_demo();
	test_list();
	test_team();
	test_ffa();
	test_rival_line();
	test_bounds();
	test_file();
	test_rc2_add();
	test_rc2_file();
	if (failures)
		return 1;
	printf("ae_bot_record_rules: ok\n");
	return 0;
}
