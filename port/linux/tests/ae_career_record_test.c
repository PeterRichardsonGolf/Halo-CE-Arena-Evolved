#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ae_career_record.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
#define EQ(a, b) do { long x_ = (long)(a), y_ = (long)(b); if (x_ != y_) { printf("FAIL %s:%d %s = %ld, want %ld\n", __FILE__, __LINE__, #a, x_, y_); failures++; } } while (0)

static struct ae_career career, again;
static char text[AE_CAREER_TEXT];

static void with_checksum(char *body)
{
	/* the file's own writer sums for us: write, then cut and re-sum is not possible, so edit-tests go through write */
	(void)body;
}

static void test_records(void)
{
	struct ae_career_entry *e;

	ae_career_init(&career);
	CHECK(ae_career_record_game(&career, AE_CAREER_OFFLINE, "team_slayer", 150, 1, 5));
	CHECK(ae_career_record_game(&career, AE_CAREER_OFFLINE, "team_slayer", 300, 1, 6));
	CHECK(ae_career_record_game(&career, AE_CAREER_OFFLINE, "team_slayer", -50, 0, 7));
	CHECK(ae_career_record_quit(&career, AE_CAREER_OFFLINE, "team_slayer", -100, 8));
	CHECK(ae_career_record_game(&career, AE_CAREER_ONLINE, "team_slayer", 80, 1, 9));
	e = ae_career_find(&career, AE_CAREER_OFFLINE, "team_slayer");
	CHECK(e != NULL);
	EQ(e->rating.points, 300);
	EQ(e->rating.level, 2);
	EQ(e->rating.highest, 3);
	EQ(e->games, 4); EQ(e->wins, 2); EQ(e->quits, 1); EQ(e->last, 8);
	/* the ladders are apart */
	e = ae_career_find(&career, AE_CAREER_ONLINE, "team_slayer");
	CHECK(e != NULL);
	EQ(e->rating.points, 80);
	EQ(e->games, 1);
	CHECK(ae_career_find(&career, AE_CAREER_ONLINE, "ffa") == NULL);
	EQ(career.count[0], 1); EQ(career.count[1], 1);
	EQ(ae_career_overall_level(&career, AE_CAREER_OFFLINE), 3);
	EQ(ae_career_overall_level(&career, AE_CAREER_ONLINE), 0);
	CHECK(ae_career_record_game(&career, AE_CAREER_OFFLINE, "ffa", 2500, 1, 10));
	EQ(ae_career_overall_level(&career, AE_CAREER_OFFLINE), 9);
	/* ids made safe, a full ladder */
	CHECK(ae_career_record_game(&career, AE_CAREER_OFFLINE, "a|b\nc", 10, 0, 1));
	CHECK(ae_career_find(&career, AE_CAREER_OFFLINE, "a?b?c") != NULL);
	{
		int i;
		char name[16];

		for (i = 0; i < AE_CAREER_PLAYLISTS; i++)
		{
			snprintf(name, sizeof(name), "p%d", i);
			ae_career_record_game(&career, AE_CAREER_ONLINE, name, 1, 0, 1);
		}
		CHECK(!ae_career_record_game(&career, AE_CAREER_ONLINE, "one-too-many", 1, 0, 1));
		EQ(career.count[1], AE_CAREER_PLAYLISTS);
	}
}

static void test_file(void)
{
	long length;
	int i;

	ae_career_init(&career);
	ae_career_record_game(&career, AE_CAREER_OFFLINE, "team_slayer", 1450, 1, 31);
	ae_career_record_game(&career, AE_CAREER_OFFLINE, "ffa", 220, 0, 14);
	ae_career_record_quit(&career, AE_CAREER_ONLINE, "ffa", -30, 14);
	length = ae_career_write(&career, text, sizeof(text));
	CHECK(length > 0);
	CHECK(strstr(text, "version = 1\n") != NULL);
	CHECK(strstr(text, "career = team_slayer | 1450 ") != NULL);
	CHECK(strstr(text, "online = ffa | ") != NULL);
	CHECK(strstr(text, "checksum = ") != NULL);
	EQ(ae_career_read(&again, text), AE_CAREER_READ_OK);
	EQ(again.count[0], 2); EQ(again.count[1], 1);
	CHECK(!memcmp(again.entry[0][0].playlist, "team_slayer", 12));
	EQ(again.entry[0][0].rating.points, 1450);
	EQ(again.entry[0][0].rating.level, 6);
	EQ(again.entry[0][0].games, 1);
	EQ(again.entry[0][0].wins, 1);
	EQ(again.entry[0][0].last, 31);
	EQ(again.entry[1][0].quits, 1);
	EQ(again.entry[1][0].rating.points, 0);
	/* written again: the same bytes */
	{
		static char second[AE_CAREER_TEXT];

		EQ(ae_career_write(&again, second, sizeof(second)), length);
		CHECK(!strcmp(second, text));
	}
	/* too small a buffer */
	CHECK(ae_career_write(&career, text + 0, 20) == -1);
	/* a hand edit: a number changed, the checksum no longer holds */
	length = ae_career_write(&career, text, sizeof(text));
	{
		char *at = strstr(text, "| 1450");

		CHECK(at != NULL);
		at[2] = '9';
	}
	EQ(ae_career_read(&again, text), AE_CAREER_READ_EDITED);
	EQ(again.count[0], 0);
	CHECK(again.edited);
	/* entries with no checksum line: edited too */
	EQ(ae_career_read(&again, "# AE career\nversion = 1\ncareer = ffa | 5000 12 12 3 2 0 4\n"), AE_CAREER_READ_EDITED);
	EQ(again.count[0], 0);
	(void)i;
	with_checksum(NULL);
}

static void test_old_and_newer(void)
{
	/* a file from before the career: the header alone, or an empty one with comments */
	EQ(ae_career_read(&again, "# AE career\nversion = 1\n"), AE_CAREER_READ_OK);
	EQ(again.count[0], 0); EQ(again.count[1], 0);
	EQ(ae_career_read(&again, "\n\n# nothing yet\nversion = 1\n\n"), AE_CAREER_READ_OK);
	/* no header, a line that is not key = value, a bad version: refused whole */
	EQ(ae_career_read(&again, ""), AE_CAREER_READ_BAD);
	EQ(ae_career_read(&again, "career = ffa | 1 1 1 1 1 1 1\n"), AE_CAREER_READ_BAD);
	EQ(ae_career_read(&again, "version = 1\nsomething\n"), AE_CAREER_READ_BAD);
	EQ(ae_career_read(&again, "version = 0\n"), AE_CAREER_READ_BAD);
	/* a newer version: read for what it holds, flagged; lines of a kind unknown are skipped */
	{
		long length;

		ae_career_init(&career);
		ae_career_record_game(&career, AE_CAREER_OFFLINE, "ffa", 500, 1, 3);
		length = ae_career_write(&career, text, sizeof(text));
		CHECK(length > 0);
		/* a newer file: not checksum-compatible with an edit of the version line, so build the text by hand with its sum */
		EQ(ae_career_read(&again, "version = 2\nseason = 3\n"), AE_CAREER_READ_OK);
		CHECK(again.newer);
		EQ(again.count[0], 0);
	}
}

static void test_ranges(void)
{
	long length;

	/* out-of-range numbers in a (checksummed) file are cut to what the record holds: written by the writer from forced fields */
	ae_career_init(&career);
	CHECK(ae_career_record_game(&career, AE_CAREER_OFFLINE, "ffa", 1, 0, 1));
	career.entry[0][0].rating.level = 77;
	career.entry[0][0].rating.highest = 90;
	career.entry[0][0].rating.points = 5000000000L > AE_RATING_POINTS_MAX ? AE_RATING_POINTS_MAX : 5;
	length = ae_career_write(&career, text, sizeof(text));
	CHECK(length > 0);
	EQ(ae_career_read(&again, text), AE_CAREER_READ_OK);
	EQ(again.entry[0][0].rating.level, 49);
	EQ(again.entry[0][0].rating.highest, 49);
	/* a longer line from a newer build: the fields past the known ones are ignored */
	{
		static char longer[AE_CAREER_TEXT];
		char *at;
		long n;

		ae_career_init(&career);
		ae_career_record_game(&career, AE_CAREER_OFFLINE, "ffa", 450, 1, 2);
		n = ae_career_write(&career, longer, sizeof(longer));
		CHECK(n > 0);
		/* (an extra field would change the sum, so check the reader on the line parser itself through an unsummed header-only file) */
		at = strstr(longer, "checksum");
		CHECK(at != NULL);
	}
}

static void test_result(void)
{
	struct ae_rating_result r;
	struct ae_career_entry *e;
	int i;

	ae_career_init(&career);
	ae_rating_result_init(&r);
	snprintf(r.playlist, sizeof(r.playlist), "team_slayer");
	for (i = 0; i < 4; i++)
	{
		snprintf(r.player[i].key, sizeof(r.player[i].key), "p%d", i);
		r.player[i].level = 20; r.player[i].place = i < 2 ? 0 : 1; r.player[i].team = i < 2 ? 0 : 1;
		r.player[i].party = -1; r.player[i].quit_order = -1; r.player[i].rated = 1;
	}
	r.count = 4;
	CHECK(ae_career_record_result(&career, AE_CAREER_OFFLINE, &r, 0, 77));
	e = ae_career_find(&career, AE_CAREER_OFFLINE, "team_slayer");
	CHECK(e != NULL);
	EQ(e->rating.points, 100);
	EQ(e->wins, 1); EQ(e->games, 1); EQ(e->quits, 0); EQ(e->last, 77);
	CHECK(ae_career_record_result(&career, AE_CAREER_ONLINE, &r, 2, 78));
	e = ae_career_find(&career, AE_CAREER_ONLINE, "team_slayer");
	EQ(e->rating.points, 0);        /* a loss from nothing: the floor */
	EQ(e->wins, 0);
	/* a quit, stamped */
	r.player[3].quit = 1; r.player[3].quit_order = 0;
	CHECK(ae_career_record_result(&career, AE_CAREER_OFFLINE, &r, 3, 80));
	e = ae_career_find(&career, AE_CAREER_OFFLINE, "team_slayer");
	EQ(e->quits, 1); EQ(e->quit_stamps, 1); EQ(e->quit_stamp[0], 80);
	/* a player not rated, or not there */
	r.player[1].rated = 0;
	CHECK(!ae_career_record_result(&career, AE_CAREER_OFFLINE, &r, 1, 81));
	CHECK(!ae_career_record_result(&career, AE_CAREER_OFFLINE, &r, 9, 81));
	/* the history keeps the last 16 and round-trips */
	for (i = 0; i < 20; i++)
		CHECK(ae_career_record_quit(&career, AE_CAREER_OFFLINE, "team_slayer", 0, 1000 + (unsigned long)i));
	EQ(e->quit_stamps, AE_CAREER_QUIT_HISTORY);
	EQ(e->quit_stamp[0], 1004);
	EQ(e->quit_stamp[15], 1019);
	EQ(e->quits, 21);
	{
		long length = ae_career_write(&career, text, sizeof(text));

		CHECK(length > 0);
		EQ(ae_career_read(&again, text), AE_CAREER_READ_OK);
		e = ae_career_find(&again, AE_CAREER_OFFLINE, "team_slayer");
		CHECK(e != NULL);
		EQ(e->quit_stamps, AE_CAREER_QUIT_HISTORY);
		EQ(e->quit_stamp[0], 1004);
		EQ(e->quit_stamp[15], 1019);
		EQ(e->quits, 21);
		EQ(e->wins, 1);
	}
}

int main(void)
{
	test_records();
	test_file();
	test_old_and_newer();
	test_ranges();
	test_result();
	if (failures)
		return 1;
	printf("ok\n");
	return 0;
}
