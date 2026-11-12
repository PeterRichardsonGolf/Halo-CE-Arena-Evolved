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
	CHECK(length > 0 && strstr(text, "version = 1"));
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

int main(void)
{
	test_keys();
	test_accumulate();
	test_rivals();
	test_eviction();
	test_file();
	test_versions();
	if (failures)
		return 1;
	printf("ok\n");
	return 0;
}
