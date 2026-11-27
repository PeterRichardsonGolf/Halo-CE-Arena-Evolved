#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ae_service_record.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static struct ae_service_record record, again;
static struct ae_sr_game game;
static char text[AE_SR_TEXT];

static void add(char const *name, int slot, int me, int won, long k, long d, long a)
{
	struct ae_sr_game_player *p = &game.player[game.count++];

	memset(p, 0, sizeof(*p));
	snprintf(p->name, sizeof(p->name), "%s", name);
	p->slot = slot;
	p->me = me;
	p->won = won;
	p->kills = k;
	p->deaths = d;
	p->assists = a;
}

static void medal(int id, int count)
{
	struct ae_sr_game_player *p = &game.player[game.count - 1];

	p->medals[p->medal_count].id = (unsigned char)id;
	p->medals[p->medal_count++].count = (unsigned char)count;
}

static void versus(int slot, int killed, int by)
{
	struct ae_sr_game_player *p = &game.player[game.count - 1];

	p->versus[p->versus_count].slot = (unsigned char)slot;
	p->versus[p->versus_count].killed = (unsigned char)killed;
	p->versus[p->versus_count++].killed_by = (unsigned char)by;
}

static void fresh(int mode)
{
	memset(&game, 0, sizeof(game));
	game.mode = mode;
}

static void test_keys(void)
{
	char a[AE_SR_KEY], b[AE_SR_KEY];

	ae_sr_key("Goat", a, sizeof(a));
	ae_sr_key("  goat ", b, sizeof(b));
	CHECK(!strcmp(a, "goat") && !strcmp(a, b));
	ae_sr_key("a|b\tc", a, sizeof(a));
	CHECK(!strcmp(a, "a?b?c"));
	ae_sr_key("", a, sizeof(a));
	CHECK(!a[0]);
	ae_sr_key("0123456789012345678901234567890123456789012345678901234567890123", a, sizeof(a));
	CHECK(strlen(a) == AE_SR_KEY - 1);
}

static void test_accumulate(void)
{
	struct ae_sr_player const *goat;

	ae_sr_init(&record);
	fresh(0);
	add("Goat", 0, 1, 1, 10, 4, 2);
	medal(5, 3);
	medal(9, 1);
	add("Tiger", 1, 0, 0, 4, 10, 0);
	medal(5, 1);
	CHECK(ae_sr_record_game(&record, &game));
	fresh(1);
	add("goat", 3, 1, 0, 2, 5, 1);
	medal(5, 2);
	add("Tiger", 4, 0, 1, 5, 2, 0);
	CHECK(ae_sr_record_game(&record, &game));
	CHECK(record.player_count == 2);
	goat = ae_sr_find(&record, "goat");
	CHECK(goat && goat->total.games == 2 && goat->total.wins == 1 && goat->total.kills == 12 && goat->total.deaths == 9);
	CHECK(goat->total.assists == 3 && goat->medals[5] == 5 && goat->medals[9] == 1 && goat->medals[0] == 0);
	CHECK(goat->mode[0].games == 1 && goat->mode[0].wins == 1 && goat->mode[1].games == 1 && goat->mode[1].wins == 0);
	CHECK(goat->mode[1].kills == 2 && goat->mode[2].games == 0);
	/* the name as last seen */
	CHECK(!strcmp(goat->name, "goat"));
	/* a player who quit does not win */
	fresh(0);
	add("Quitter", 0, 0, 1, 1, 1, 0);
	game.player[0].quit = 1;
	ae_sr_record_game(&record, &game);
	CHECK(ae_sr_find(&record, "quitter")->total.games == 1 && ae_sr_find(&record, "quitter")->total.wins == 0);
	/* an empty game and nameless players add nothing */
	fresh(0);
	CHECK(!ae_sr_record_game(&record, &game));
	add("", 0, 0, 0, 1, 1, 1);
	CHECK(!ae_sr_record_game(&record, &game));
	/* a mode out of range counts as OTHER */
	fresh(99);
	add("Odd", 0, 0, 0, 1, 0, 0);
	ae_sr_record_game(&record, &game);
	CHECK(ae_sr_find(&record, "odd")->mode[AE_SR_MODES - 1].games == 1);
}

static void test_rivals(void)
{
	struct ae_sr_versus const *vs;

	ae_sr_init(&record);
	CHECK(ae_sr_rival_set(&record, "Goat", "Tiger", 1));
	CHECK(ae_sr_is_rival(&record, "goat", "TIGER") && !ae_sr_is_rival(&record, "tiger", "goat"));
	CHECK(!ae_sr_rival_set(&record, "Goat", "goat", 1));
	CHECK(!ae_sr_rival_set(&record, "Goat", "", 1));
	CHECK(ae_sr_rival_set(&record, "Goat", "Tiger", 1) && record.rival_count == 1);
	fresh(0);
	add("Goat", 0, 1, 1, 6, 3, 0);
	versus(1, 4, 2);
	versus(2, 2, 1);
	add("Tiger", 1, 0, 0, 2, 4, 0);
	add("Bear", 2, 0, 0, 1, 2, 0);
	ae_sr_record_game(&record, &game);
	ae_sr_record_game(&record, &game);
	vs = ae_sr_versus_of(&record, "goat", "tiger");
	CHECK(vs && vs->games == 2 && vs->kills == 8 && vs->deaths == 4);
	CHECK(!ae_sr_versus_of(&record, "goat", "bear"));
	/* a rival not in the summary's list counts the game and no kills */
	fresh(0);
	add("Goat", 0, 1, 1, 6, 3, 0);
	versus(2, 2, 1);
	add("Tiger", 1, 0, 0, 2, 4, 0);
	ae_sr_record_game(&record, &game);
	vs = ae_sr_versus_of(&record, "goat", "tiger");
	CHECK(vs->games == 3 && vs->kills == 8);
	/* a game with no owner of ours in it (not me) tracks nothing new */
	fresh(0);
	add("Goat", 0, 0, 1, 6, 3, 0);
	versus(1, 9, 9);
	add("Tiger", 1, 0, 0, 2, 4, 0);
	ae_sr_record_game(&record, &game);
	CHECK(ae_sr_versus_of(&record, "goat", "tiger")->games == 3);
	/* taken off: the versus goes with it */
	CHECK(ae_sr_rival_set(&record, "goat", "tiger", 0));
	CHECK(!ae_sr_is_rival(&record, "goat", "tiger") && !ae_sr_versus_of(&record, "goat", "tiger") && record.versus_count == 0);
	CHECK(ae_sr_rival_set(&record, "goat", "tiger", 0));
	/* the owner's list is full at 8 */
	{
		char name[16];
		int index;

		for (index = 0; index < AE_SR_RIVALS_PER_OWNER; index++)
		{
			snprintf(name, sizeof(name), "r%d", index);
			CHECK(ae_sr_rival_set(&record, "goat", name, 1));
		}
		CHECK(!ae_sr_rival_set(&record, "goat", "one more", 1));
		CHECK(ae_sr_rival_set(&record, "tiger", "one more", 1));
	}
}

static void test_eviction(void)
{
	char name[16];
	int index;

	ae_sr_init(&record);
	ae_sr_rival_set(&record, "p0", "p1", 1);
	for (index = 0; index < AE_SR_PLAYERS + 40; index++)
	{
		fresh(0);
		snprintf(name, sizeof(name), "p%d", index);
		add(name, 0, 0, 0, 1, 1, 1);
		ae_sr_record_game(&record, &game);
	}
	CHECK(record.player_count == AE_SR_PLAYERS);
	/* the held (owner and rival) stay; the oldest of the others went; the newest is there */
	CHECK(ae_sr_find(&record, "p0") && ae_sr_find(&record, "p1"));
	CHECK(!ae_sr_find(&record, "p2"));
	CHECK(ae_sr_find(&record, "p295"));
}

static void test_file(void)
{
	long length;
	struct ae_sr_player const *goat;
	struct ae_sr_versus const *vs;

	ae_sr_init(&record);
	ae_sr_rival_set(&record, "Goat", "Ti|ger", 1);
	fresh(3);
	add("Goat", 0, 1, 1, 6, 3, 1);
	medal(5, 3);
	medal(30, 7);
	versus(1, 4, 2);
	add("Ti|ger", 1, 0, 0, 2, 4, 0);
	ae_sr_record_game(&record, &game);
	length = ae_sr_write(&record, text, sizeof(text));
	CHECK(length > 0 && strstr(text, "version = 2"));
	/* a round trip, byte for byte the second time */
	CHECK(ae_sr_read(&again, text) == 1 && !again.newer);
	CHECK(again.clock == record.clock && again.player_count == 2 && again.rival_count == 1 && again.versus_count == 1);
	goat = ae_sr_find(&again, "goat");
	CHECK(goat && goat->total.games == 1 && goat->total.wins == 1 && goat->total.kills == 6 && goat->total.assists == 1);
	CHECK(goat->medals[5] == 3 && goat->medals[30] == 7 && goat->mode[3].games == 1 && goat->mode[3].kills == 6);
	CHECK(ae_sr_find(&again, "ti?ger") && ae_sr_is_rival(&again, "goat", "ti?ger"));
	vs = ae_sr_versus_of(&again, "goat", "ti?ger");
	CHECK(vs && vs->games == 1 && vs->kills == 4 && vs->deaths == 2);
	{
		static char second[AE_SR_TEXT];

		CHECK(ae_sr_write(&again, second, sizeof(second)) == length && !strcmp(second, text));
	}
	/* a buffer too small is refused */
	CHECK(ae_sr_write(&record, text, 40) == -1);
	/* the empty record writes and reads */
	ae_sr_init(&record);
	length = ae_sr_write(&record, text, sizeof(text));
	CHECK(length > 0 && ae_sr_read(&again, text) && again.player_count == 0);
}

static void test_versions(void)
{
	/* a newer version: its known lines read, an unknown kind and extra fields skipped, flagged newer */
	char const *newer =
		"# AE service record\nversion = 7\nclock = 9\nfuture = a | b\n"
		"player = goat | Goat | 9 | 3 2 10 5 1 | 3 2 10 5 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 | 5:2 | extra | more\n"
		"rival = goat | tiger | Tiger | colour\n"
		"versus = goat | tiger | 3 4 5 | tail\n";
	char const *shorter = "version = 1\nplayer = x | X | 1 | 1 1 1 1 1\nplayer = goat | Goat | 9 | 3 2 10 5 1 | "
		"3 2 10 5 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 |\n";

	CHECK(ae_sr_read(&again, newer) == 1 && again.newer && again.clock == 9);
	CHECK(again.player_count == 1 && ae_sr_find(&again, "goat")->medals[5] == 2);
	CHECK(again.rival_count == 1 && again.versus_count == 1 && ae_sr_versus_of(&again, "goat", "tiger")->deaths == 5);
	/* a line too short for its kind is dropped, the rest read */
	CHECK(ae_sr_read(&again, shorter) == 1 && again.player_count == 1 && ae_sr_find(&again, "goat"));
	/* refused whole: no header, a bad version, a line that is not key = value, a header not first */
	CHECK(ae_sr_read(&again, "") == 0);
	CHECK(ae_sr_read(&again, "clock = 1\n") == 0);
	CHECK(ae_sr_read(&again, "version = 0\n") == 0);
	CHECK(ae_sr_read(&again, "version = x\n") == 0);
	CHECK(ae_sr_read(&again, "version = 1\nnonsense\n") == 0 && again.player_count == 0);
	/* medals of an id this build does not know are ignored */
	CHECK(ae_sr_read(&again, "version = 1\nplayer = a | A | 1 | 1 0 0 0 0 | 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 | 999:4,2:3\n"));
	CHECK(ae_sr_find(&again, "a")->medals[2] == 0 || ae_sr_find(&again, "a")->medals[2] == 3);
	/* a very long line is refused rather than overrun */
	{
		static char big[3000];

		memset(big, 'a', sizeof(big) - 1);
		memcpy(big, "version = 1\nx = ", 15);
		big[sizeof(big) - 1] = 0;
		CHECK(ae_sr_read(&again, big) == 0);
	}
}

/* ---- (rc2) version 2: the new numbers, the maps, the last game, and the old format beside them */

/* the new numbers of the last player added: a game of that kind and map, with these weapons */
static void extras(int mode, int won, int tied, long kills, long deaths, long assists, long seconds, int with_v3)
{
	struct ae_sr_game_player *p = &game.player[game.count - 1];
	struct ae_rs_input in;
	static struct ae_pgcr_summary summary;

	memset(&in, 0, sizeof(in));
	in.mode = mode;
	in.won = won;
	in.tied = tied;
	in.kills = kills;
	in.deaths = deaths;
	in.assists = assists;
	in.seconds = seconds;
	in.fired = 100;
	in.hit = 40;
	in.objective[0] = in.objective[1] = in.objective[2] = -1;
	in.laps = in.best_lap_ticks = in.ball_carrier_kills = in.as_ball_carrier = -1;
	if (mode == AE_RS_KIND_FLAG)
		in.objective[0] = 3, in.objective[1] = 2, in.objective[2] = 1;
	memset(&summary, 0, sizeof(summary));
	summary.valid = 1;
	summary.best_spree = 5;
	summary.extended = with_v3;
	summary.damage_dealt = with_v3 ? 2000 : 0;
	summary.damage_taken = with_v3 ? 1500 : 0;
	summary.melee = with_v3 ? 2 : 0;
	in.summary = &summary;
	ae_rs_game_make(&p->x, &in);
	p->has_x = 1;
	p->medals_earned = 4;
}

static void weapon(char const *name, long kills)
{
	struct ae_sr_game_player *p = &game.player[game.count - 1];

	snprintf(p->weapons[p->weapon_count].name, sizeof(p->weapons[0].name), "%s", name);
	p->weapons[p->weapon_count++].kills = kills;
}

static void test_rc2_accumulate(void)
{
	struct ae_sr_player const *goat;

	ae_sr_init(&record);
	/* game 1: a flag win on a map, with the version 3 numbers */
	fresh(1);
	snprintf(game.map, sizeof(game.map), "Blood Gulch");
	add("Goat", 0, 1, 1, 10, 4, 2);
	extras(1, 1, 0, 10, 4, 2, 600, 1);
	weapon("battle rifle", 6);
	weapon("sniper rifle", 4);
	CHECK(ae_sr_record_game(&record, &game) == 1);
	/* game 2: a tie in slayer on the same map (the case differs), from an older host: no version 3 numbers */
	fresh(0);
	snprintf(game.map, sizeof(game.map), "blood gulch");
	add("Goat", 0, 1, 0, 8, 8, 1);
	extras(0, 0, 1, 8, 8, 1, 300, 0);
	weapon("battle rifle", 3);
	CHECK(ae_sr_record_game(&record, &game) == 1);
	/* game 3: a loss on another map, from a caller with no new numbers */
	fresh(0);
	snprintf(game.map, sizeof(game.map), "Prisoner");
	add("Goat", 0, 1, 0, 1, 9, 0);
	CHECK(ae_sr_record_game(&record, &game) == 1);
	goat = ae_sr_find(&record, "goat");
	CHECK(goat && goat->total.games == 3 && goat->total.wins == 1);
	CHECK(goat->x.counted == 2 && goat->x.counted_v3 == 1 && goat->x.ties == 1);
	CHECK(goat->x.seconds == 900 && goat->x.flag_caps == 1 && goat->x.flag_grabs == 3 && goat->x.best_spree == 5);
	CHECK(goat->x.dealt == 2000 && goat->x.taken == 1500 && goat->x.melee == 2 && goat->x.kills_v3 == 10);
	CHECK(goat->x.fired == 200 && goat->x.hit == 80 && goat->x.hit_v3 == 40);
	CHECK(goat->weapon_count == 2 && !strcmp(goat->weapons[0].name, "battle rifle") && goat->weapons[0].kills == 9 &&
		goat->weapons[1].kills == 4);
	/* the maps: the third game had no new numbers and made none */
	CHECK(goat->map_count == 1 && goat->map[0].games == 2 && goat->map[0].wins == 1 && goat->map[0].kills == 18 &&
		goat->map[0].deaths == 12);
	/* the last game is the last one that had the new numbers: a tie in slayer */
	CHECK(goat->last.has && goat->last.mode == 0 && goat->last.result == 2 && goat->last.kills == 8 &&
		goat->last.deaths == 8 && goat->last.assists == 1 && goat->last.spree == 5 && goat->last.medals == 4 &&
		!strcmp(goat->last.map, "blood gulch"));
	/* a loss and a win are told apart by the result, and a quit game is a loss with no time */
	fresh(0);
	snprintf(game.map, sizeof(game.map), "Prisoner");
	add("Goat", 0, 1, 1, 12, 2, 0);
	extras(0, 1, 0, 12, 2, 0, 400, 1);
	CHECK(ae_sr_record_game(&record, &game) == 1);
	goat = ae_sr_find(&record, "goat");
	CHECK(goat->last.result == 0 && goat->map_count == 2);
	fresh(0);
	snprintf(game.map, sizeof(game.map), "Prisoner");
	add("Goat", 0, 1, 0, 2, 5, 0);
	game.player[0].quit = 1;
	extras(0, 0, 0, 2, 5, 0, 400, 1);
	game.player[0].x.x.seconds = 0;
	CHECK(ae_sr_record_game(&record, &game) == 1);
	goat = ae_sr_find(&record, "goat");
	CHECK(goat->last.result == 1 && goat->x.seconds == 900 + 400);
}

static void test_rc2_maps_full(void)
{
	int index;
	struct ae_sr_player const *p;

	ae_sr_init(&record);
	for (index = 0; index < AE_SR_MAPS + 3; index++)
	{
		fresh(0);
		snprintf(game.map, sizeof(game.map), "map %d", index);
		add("Goat", 0, 1, 1, 1, 1, 0);
		extras(0, 1, 0, 1, 1, 0, 10, 1);
		/* (the first maps are played again, so they have the most games) */
		ae_sr_record_game(&record, &game);
		if (index < 4)
			ae_sr_record_game(&record, &game);
	}
	p = ae_sr_find(&record, "goat");
	CHECK(p->map_count == AE_SR_MAPS);
	/* the four played twice stay; the last maps replaced the ones played once */
	for (index = 0; index < 4; index++)
	{
		char name[16];
		int at, found = 0;

		snprintf(name, sizeof(name), "map %d", index);
		for (at = 0; at < p->map_count; at++)
			if (!strcmp(p->map[at].name, name) && p->map[at].games == 2)
				found = 1;
		CHECK(found);
	}
}

static void test_rc2_file(void)
{
	static char second[AE_SR_TEXT];
	struct ae_sr_player const *goat;
	long length;
	char *line;

	ae_sr_init(&record);
	fresh(1);
	snprintf(game.map, sizeof(game.map), "Blood Gulch");
	add("Goat", 0, 1, 1, 10, 4, 2);
	extras(1, 1, 0, 10, 4, 2, 600, 1);
	weapon("battle rifle", 6);
	weapon("sniper rifle", 4);
	medal(5, 3);
	add("Tiger", 1, 0, 0, 2, 9, 0);
	ae_sr_record_game(&record, &game);
	ae_sr_rival_set(&record, "Goat", "Tiger", 1);
	length = ae_sr_write(&record, text, sizeof(text));
	CHECK(length > 0 && strstr(text, "version = 2") && strstr(text, "playerx = goat | ") && strstr(text, "map = goat | Blood Gulch | 1 1 10 4") &&
		strstr(text, "lastgame = goat | flag | Blood Gulch | 0 10 4 2 5 4 | battle rifle"));
	/* Tiger's game had no new numbers: no such lines for him */
	CHECK(!strstr(text, "playerx = tiger") && !strstr(text, "map = tiger") && !strstr(text, "lastgame = tiger"));
	/* the new lines follow their player's */
	CHECK(strstr(text, "player = goat") < strstr(text, "playerx = goat") && strstr(text, "playerx = goat") < strstr(text, "rival = "));
	/* the round trip, byte for byte the second time */
	CHECK(ae_sr_read(&again, text) == 1 && !again.newer);
	goat = ae_sr_find(&again, "goat");
	CHECK(goat && goat->x.counted == 1 && goat->x.counted_v3 == 1 && goat->x.seconds == 600 && goat->x.dealt == 2000);
	CHECK(goat->weapon_count == 2 && !strcmp(goat->weapons[1].name, "sniper rifle") && goat->weapons[1].kills == 4);
	CHECK(goat->map_count == 1 && !strcmp(goat->map[0].name, "Blood Gulch") && goat->map[0].kills == 10);
	CHECK(goat->last.has && goat->last.mode == 1 && !strcmp(goat->last.map, "Blood Gulch") && goat->last.medals == 4 &&
		!strcmp(goat->last.weapon, "battle rifle"));
	CHECK(ae_sr_write(&again, second, sizeof(second)) == length && !strcmp(second, text));
	/* what a version 1 reader reads is untouched: its own lines are the old format, the new ones its unknown kinds, and no
	line is long enough for its limit (1024) */
	for (line = text; *line;)
	{
		size_t n = strcspn(line, "\n");

		CHECK(n < 1000);
		if (!strncmp(line, "player = ", 9))
		{
			int bars = 0;
			size_t at;

			for (at = 0; at < n; at++)
				bars += line[at] == '|';
			CHECK(bars == 5);
		}
		else if (strncmp(line, "playerx = ", 10) && strncmp(line, "map = ", 6) && strncmp(line, "lastgame = ", 11) &&
			strncmp(line, "rival = ", 8) && strncmp(line, "versus = ", 9) && strncmp(line, "clock = ", 8) &&
			strncmp(line, "version = ", 10) && line[0] != '#' && n)
			CHECK(0);
		line += n + (line[n] ? 1 : 0);
	}
}

static void test_rc2_old_files(void)
{
	/* a version 1 file: read as it always was, the new numbers empty, and it is not newer */
	char const *v1 = "# AE service record\nversion = 1\nclock = 4\nplayer = goat | Goat | 4 | 3 2 10 5 1 | 3 2 10 5 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 | 5:2\n";
	struct ae_sr_player const *goat;
	long length;

	CHECK(ae_sr_read(&again, v1) == 1 && !again.newer);
	goat = ae_sr_find(&again, "goat");
	CHECK(goat && goat->total.games == 3 && goat->x.counted == 0 && goat->weapon_count == 0 && goat->map_count == 0 && !goat->last.has);
	/* a game now counted on top: the totals continue, the new numbers cover one game */
	fresh(0);
	snprintf(game.map, sizeof(game.map), "Prisoner");
	add("Goat", 0, 1, 1, 7, 3, 1);
	extras(0, 1, 0, 7, 3, 1, 500, 1);
	CHECK(ae_sr_record_game(&again, &game) == 1);
	goat = ae_sr_find(&again, "goat");
	CHECK(goat->total.games == 4 && goat->x.counted == 1 && goat->x.counted_v3 == 1);
	/* and it writes as version 2 and reads back the same */
	length = ae_sr_write(&again, text, sizeof(text));
	CHECK(length > 0 && strstr(text, "version = 2") && ae_sr_read(&record, text) == 1);
	CHECK(ae_sr_find(&record, "goat")->x.counted == 1 && ae_sr_find(&record, "goat")->total.games == 4);
	/* the new lines of a key with no player line are skipped; bad numbers drop one line, not the file */
	CHECK(ae_sr_read(&again,
		"version = 2\nplayerx = nobody | 1 2 3 | 4 5\nmap = nobody | gulch | 1 1 1 1\nlastgame = nobody | flag | gulch | 0 1 1 1 1 1\n"
		"player = goat | Goat | 4 | 3 2 10 5 1 | 3 2 10 5 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 |\n"
		"playerx = goat | 1 two 3\nmap = goat | gulch | 1 x 1 1\nlastgame = goat | nonsense | gulch | 0 1 1 1 1 1\nmap = goat | ok | 2 1 5 4\n") == 1);
	goat = ae_sr_find(&again, "goat");
	CHECK(ae_sr_find(&again, "nobody") == NULL && goat->x.counted == 0 && goat->map_count == 1 && goat->map[0].games == 2 &&
		!goat->last.has);
	/* a file of a later version: its known lines read, the new kinds too, flagged newer */
	CHECK(ae_sr_read(&again, "version = 3\nplayer = goat | Goat | 4 | 3 2 10 5 1 | 3 2 10 5 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 |\n"
		"playerx = goat | 0 0 90 | 1 1 | 0 0 | 0 0 0 0 0 0 0 | 0 0 0 0 0 | 0 0 0 0 | 0 0 0 0 | 1 0 0 0 0 | a:1 | far | more\nfuture = x\n") == 1);
	CHECK(again.newer && ae_sr_find(&again, "goat")->x.seconds == 90 && ae_sr_find(&again, "goat")->x.counted == 1);
}

static void test_rc2_size(void)
{
	static char big[AE_SR_TEXT];
	int index, at;
	long length;
	struct ae_sr_player *p;

	/* every player at the limits writes within the file's size and reads back */
	ae_sr_init(&record);
	for (index = 0; index < AE_SR_PLAYERS; index++)
	{
		p = &record.player[record.player_count++];
		snprintf(p->key, sizeof(p->key), "player-with-a-long-name-number-%03d", index % 1000);
		memcpy(p->name, p->key, sizeof(p->name));
		p->x.counted = p->x.counted_v3 = 123456;
		p->x.dealt = p->x.taken = 123456789;
		for (at = 0; at < AE_SR_MAPS; at++)
		{
			snprintf(p->map[p->map_count].name, sizeof(p->map[0].name), "a map with a long name %02d xxxxx", at % 100);
			p->map[p->map_count].games = p->map[p->map_count].wins = p->map[p->map_count].kills = p->map[p->map_count].deaths = 99999;
			p->map_count++;
		}
		for (at = 0; at < AE_SR_WEAPONS; at++)
		{
			snprintf(p->weapons[p->weapon_count].name, sizeof(p->weapons[0].name), "a weapon name %d", at % 10);
			p->weapons[p->weapon_count++].kills = 99999;
		}
		for (at = 0; at < AE_MEDAL_COUNT; at++)
			p->medals[at] = 99999;
		p->last.has = 1;
		snprintf(p->last.map, sizeof(p->last.map), "a map with a long name");
	}
	length = ae_sr_write(&record, big, sizeof(big));
	CHECK(length > 0 && length < AE_SR_TEXT);
	CHECK(ae_sr_read(&again, big) == 1 && again.player_count == AE_SR_PLAYERS && again.player[255].map_count == AE_SR_MAPS);
}

/* a custom map's names (its map and its weapons' tag paths) with a newline or a "|": the last game's line still reads
back */
static void test_last_game_names(void)
{
	struct ae_sr_player const *goat;

	ae_sr_init(&record);
	fresh(1);
	snprintf(game.map, sizeof(game.map), "Blood\nGulch");
	add("Goat", 0, 1, 1, 10, 4, 2);
	extras(1, 1, 0, 10, 4, 2, 600, 1);
	weapon("battle\nrifle | x", 6);
	ae_sr_record_game(&record, &game);
	CHECK(ae_sr_write(&record, text, sizeof(text)) > 0);
	CHECK(ae_sr_read(&again, text) == 1);
	goat = ae_sr_find(&again, "goat");
	CHECK(goat && goat->last.has && !strcmp(goat->last.map, "Blood?Gulch") && !strcmp(goat->last.weapon, "battle?rifle ? x"));
}

int main(void)
{

	test_keys();
	test_accumulate();
	test_rivals();
	test_eviction();
	test_file();
	test_versions();
	test_rc2_accumulate();
	test_rc2_maps_full();
	test_rc2_file();
	test_rc2_old_files();
	test_rc2_size();
	test_last_game_names();
	if (failures)
		return 1;
	printf("ok\n");
	return 0;
}
