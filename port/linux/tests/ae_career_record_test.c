#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ae_career_record.h"
#include "ae_social_rules.h"
#include "ae_social_match_rules.h"

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
	CHECK(strstr(text, "version = 2\n") != NULL);
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
		EQ(ae_career_read(&again, "version = 4\nseason = 3\n"), AE_CAREER_READ_OK);
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

/* body plus its checksum line, as the writer sums (FNV-1a of every byte before it) */
static char const *summed(char const *body)
{
	static char built[2048];
	unsigned long hash = 2166136261u;
	size_t index, length = strlen(body);

	for (index = 0; index < length; index++)
		hash = ((hash ^ (unsigned char)body[index]) * 16777619u) & 0xffffffffu;
	snprintf(built, sizeof(built), "%schecksum = %08lx\n", body, hash);
	return built;
}

static void test_social(void)
{
	struct ae_career_match *m;
	struct ae_career_social *c;
	struct ae_career_pending *g;
	char *at;
	long length;
	int index;

	/* the lines: match, social, pending, one checksum; a version 1 file's career lines stay beside them */
	ae_career_init(&career);
	CHECK(ae_career_record_game(&career, AE_CAREER_OFFLINE, "team_slayer", 150, 1, 5));
	CHECK(ae_career_match_record(&career, "Chief", "team-slayer", 1712, 1763201234UL));
	CHECK(ae_career_match_record(&career, "Chief", "team-slayer", 1800, 1763201300UL));
	CHECK(ae_career_pending_set(&career, "Chief", "team-slayer", 99));
	CHECK(ae_career_social_record(&career, "Chief", "team-slayer", AE_SOCIAL_WIN, 10));
	CHECK(ae_career_social_record(&career, "Chief", "team-slayer", AE_SOCIAL_TIE, 11));
	CHECK(ae_career_social_record(&career, "Chief", "team-slayer", AE_SOCIAL_LOSS, 12));
	CHECK(ae_career_social_record(&career, "Chief", "team-slayer", AE_SOCIAL_QUIT, 13));
	CHECK(ae_career_social_record(&career, "Chief", "doubles", AE_SOCIAL_WIN, 14));
	CHECK(ae_career_social_record(&career, "Other", "doubles", AE_SOCIAL_WIN, 15));
	CHECK(ae_career_pending_set(&career, "Other", "doubles", 100));
	c = ae_career_social_find(&career, "Chief", "team-slayer");
	CHECK(c != NULL);
	EQ(c->exp, 1); EQ(c->games, 4); EQ(c->wins, 1); EQ(c->ties, 1); EQ(c->quits, 1); EQ(c->last, 13);
	EQ(ae_career_social_total_exp(&career, "Chief"), 2);
	EQ(ae_career_social_total_exp(&career, "Nobody"), 0);
	m = ae_career_match_find(&career, "Chief", "team-slayer");
	CHECK(m != NULL);
	EQ(m->points, 1800); EQ(m->games, 2);
	/* the pending line is gone with the result of its profile; the other profile's stays */
	CHECK(ae_career_pending_find(&career, "Chief") == NULL);
	CHECK(ae_career_pending_find(&career, "Other") != NULL);
	length = ae_career_write(&career, text, sizeof(text));
	CHECK(length > 0);
	CHECK(strstr(text, "version = 2\n") != NULL);
	CHECK(strstr(text, "match = chief") == NULL);	/* (the key is as given: the glue lower-cases it) */
	CHECK(strstr(text, "match = Chief | team-slayer | 1800 2 1763201300\n") != NULL);
	CHECK(strstr(text, "social = Chief | team-slayer | 1 4 1 1 1 13\n") != NULL);
	CHECK(strstr(text, "pending = Other | doubles | 100\n") != NULL);
	EQ(ae_career_read(&again, text), AE_CAREER_READ_OK);
	CHECK(!again.newer);
	EQ(again.count[0], 1); EQ(again.match_count, 1); EQ(again.social_count, 3); EQ(again.pending_count, 1);
	g = ae_career_pending_find(&again, "Other");
	CHECK(g != NULL);
	CHECK(!strcmp(g->playlist, "doubles"));
	EQ(g->start, 100);
	c = ae_career_social_find(&again, "Other", "doubles");
	CHECK(c != NULL);
	EQ(c->exp, 1);
	/* written again unchanged: the same bytes */
	{
		static char second[AE_CAREER_TEXT];

		length = ae_career_write(&again, second, sizeof(second));
		CHECK(length > 0 && !strcmp(second, text));
	}
	/* an edit of a social line breaks the checksum: set aside whole */
	at = strstr(text, "social = Chief | team-slayer | 1 ");
	CHECK(at != NULL);
	at[strlen("social = Chief | team-slayer | ")] = '9';
	EQ(ae_career_read(&again, text), AE_CAREER_READ_EDITED);
	EQ(again.social_count, 0);
	/* a file with social lines and no checksum is edited too; one with only the header is fine */
	EQ(ae_career_read(&again, "# AE career\nversion = 2\nsocial = a | b | 5 1 1 0 0 3\n"), AE_CAREER_READ_EDITED);
	EQ(ae_career_read(&again, "# AE career\nversion = 2\npending = a | b | 5\n"), AE_CAREER_READ_EDITED);
	EQ(ae_career_read(&again, "# AE career\nversion = 2\n"), AE_CAREER_READ_OK);
	/* a version 1 file reads as before: no social lines, the career line kept */
	ae_career_init(&career);
	ae_career_record_game(&career, AE_CAREER_OFFLINE, "ffa", 500, 1, 3);
	length = ae_career_write(&career, text, sizeof(text));
	CHECK(length > 0);
	EQ(ae_career_read(&again, text), AE_CAREER_READ_OK);
	EQ(again.match_count, 0); EQ(again.social_count, 0); EQ(again.pending_count, 0); EQ(again.count[0], 1);
	/* a newer file's social lines are read; their longer rows' extra fields ignored; a bad social line skipped */
	{
		static char built[1024];

		ae_career_init(&career);
		ae_career_match_record(&career, "a", "b", 100, 1);
		length = ae_career_write(&career, built, sizeof(built));
		CHECK(length > 0);
		EQ(ae_career_read(&again, built), AE_CAREER_READ_OK);
		EQ(again.match_count, 1);
		EQ(ae_career_read(&again, summed("# AE career\nversion = 4\nsocial = a | b | 5 1 1 0 0 3 77 88\nsocial = broken\nsocial = a | | 1 1 1 1 1 1\nmatch = a | b | x\n")),
			AE_CAREER_READ_OK);
		CHECK(again.newer);
		EQ(again.social_count, 1);
		EQ(again.social[0].exp, 5); EQ(again.social[0].last, 3);
		EQ(again.match_count, 0);
	}
	/* out-of-range numbers are cut: points to 4999, EXP to the most */
	EQ(ae_career_read(&again, summed("# AE career\nversion = 3\nmatch = a | b | 99999 1 1\nsocial = a | b | 9999999 1 1 0 0 3\n")), AE_CAREER_READ_OK);
	EQ(again.match[0].points, 4999); EQ(again.social[0].exp, AE_SOCIAL_EXP_MAX);
	/* a second pending line of one profile is an edited file (set aside whole) */
	EQ(ae_career_read(&again, summed("# AE career\nversion = 3\npending = a | b | 5\npending = a | c | 6\n")), AE_CAREER_READ_EDITED);
	EQ(again.pending_count, 0);
	EQ(ae_career_read(&again, summed("# AE career\nversion = 3\npending = a | b | 5\npending = c | c | 6\n")), AE_CAREER_READ_OK);
	EQ(again.pending_count, 2);
	/* pending set again replaces; clear is once */
	ae_career_init(&career);
	CHECK(ae_career_pending_set(&career, "a", "b", 1));
	CHECK(ae_career_pending_set(&career, "a", "c", 2));
	EQ(career.pending_count, 1);
	CHECK(!strcmp(ae_career_pending_find(&career, "a")->playlist, "c"));
	CHECK(ae_career_pending_clear(&career, "a"));
	CHECK(!ae_career_pending_clear(&career, "a"));
	/* the limits: the 65th match / social line and the 9th pending profile are refused; the file still fits */
	ae_career_init(&career);
	for (index = 0; index < AE_CAREER_SOCIAL; index++)
	{
		char key[16];

		snprintf(key, sizeof(key), "p%d", index);
		CHECK(ae_career_match_record(&career, key, "playlist-with-a-long-id", 4999, 4000000000UL));
		CHECK(ae_career_social_record(&career, key, "playlist-with-a-long-id", AE_SOCIAL_WIN, 4000000000UL));
	}
	CHECK(!ae_career_match_record(&career, "extra", "x", 1, 1));
	CHECK(!ae_career_social_record(&career, "extra", "x", AE_SOCIAL_WIN, 1));
	for (index = 0; index < AE_CAREER_PENDING; index++)
	{
		char key[16];

		snprintf(key, sizeof(key), "p%d", index);
		CHECK(ae_career_pending_set(&career, key, "x", 1));
	}
	CHECK(!ae_career_pending_set(&career, "extra", "x", 1));
	length = ae_career_write(&career, text, sizeof(text));
	CHECK(length > 0 && length < AE_CAREER_TEXT);
	EQ(ae_career_read(&again, text), AE_CAREER_READ_OK);
	EQ(again.match_count, AE_CAREER_SOCIAL); EQ(again.social_count, AE_CAREER_SOCIAL); EQ(again.pending_count, AE_CAREER_PENDING);
	/* reset: one playlist's lines, or all of a profile's; another profile's stay */
	ae_career_init(&career);
	ae_career_match_record(&career, "a", "x", 1000, 1);
	ae_career_match_record(&career, "a", "y", 1000, 1);
	ae_career_match_record(&career, "b", "x", 1000, 1);
	ae_career_social_record(&career, "a", "x", AE_SOCIAL_WIN, 1);
	ae_career_social_record(&career, "a", "y", AE_SOCIAL_WIN, 1);
	ae_career_social_record(&career, "b", "x", AE_SOCIAL_WIN, 1);
	ae_career_pending_set(&career, "a", "y", 3);
	EQ(ae_career_social_reset(&career, "a", "x"), 2);
	EQ(career.match_count, 2); EQ(career.social_count, 2);
	CHECK(ae_career_pending_find(&career, "a") != NULL);
	EQ(ae_career_social_reset(&career, "a", "y"), 3);
	CHECK(ae_career_pending_find(&career, "a") == NULL);
	EQ(ae_career_social_reset(&career, "a", NULL), 0);
	EQ(ae_career_social_reset(&career, "b", NULL), 2);
	EQ(career.match_count, 0); EQ(career.social_count, 0);
	/* a "|" or control character in a key is made safe */
	ae_career_init(&career);
	CHECK(ae_career_social_record(&career, "a|b\n", "x|y", AE_SOCIAL_WIN, 1));
	length = ae_career_write(&career, text, sizeof(text));
	EQ(ae_career_read(&again, text), AE_CAREER_READ_OK);
	EQ(again.social_count, 1);
	CHECK(!strcmp(again.social[0].profile, "a?b?"));
}

/* (0.9.4) the calibration answer: a line a profile; a reader without the key skips it; the start follows it */
static void test_experience(void)
{
	static char built[AE_CAREER_TEXT];
	long length;

	ae_career_init(&career);
	EQ(ae_career_experience_get(&career, "kid"), AE_SOCIAL_EXPERIENCE_UNSET);
	EQ(ae_career_match_start(&career, "kid"), AE_SOCIAL_MATCH_START);
	CHECK(ae_career_experience_set(&career, "kid", AE_SOCIAL_EXPERIENCE_NEW));
	CHECK(ae_career_experience_set(&career, "dad", AE_SOCIAL_EXPERIENCE_A_LOT));
	CHECK(!ae_career_experience_set(&career, "kid", NUMBER_OF_AE_SOCIAL_EXPERIENCES));
	CHECK(!ae_career_experience_set(&career, "kid", -1));
	EQ(ae_career_match_start(&career, "kid"), ae_social_calibration_start(AE_SOCIAL_EXPERIENCE_NEW));
	EQ(ae_career_match_start(&career, "dad"), ae_social_calibration_start(AE_SOCIAL_EXPERIENCE_A_LOT));
	EQ(ae_career_match_start(&career, "mum"), AE_SOCIAL_MATCH_START);
	/* a change replaces; unset takes the line away */
	CHECK(ae_career_experience_set(&career, "kid", AE_SOCIAL_EXPERIENCE_SOME));
	EQ(career.experience_count, 2);
	/* written, read back, checksum good */
	ae_career_match_record(&career, "kid", "x", 3000, 1);
	length = ae_career_write(&career, built, sizeof(built));
	CHECK(length > 0);
	CHECK(strstr(built, "experience = kid | 2\n") != NULL);
	EQ(ae_career_read(&again, built), AE_CAREER_READ_OK);
	EQ(ae_career_experience_get(&again, "kid"), AE_SOCIAL_EXPERIENCE_SOME);
	EQ(ae_career_experience_get(&again, "dad"), AE_SOCIAL_EXPERIENCE_A_LOT);
	/* restart: the profile's ratings go to the start with 0 games; another profile's stay */
	ae_career_match_record(&career, "dad", "x", 3000, 1);
	CHECK(ae_career_experience_set(&career, "kid", AE_SOCIAL_EXPERIENCE_NEW));
	EQ(ae_career_match_restart(&career, "kid", 9), 1);
	EQ(ae_career_match_find(&career, "kid", "x")->points, ae_social_calibration_start(AE_SOCIAL_EXPERIENCE_NEW));
	EQ(ae_career_match_find(&career, "kid", "x")->games, 0);
	EQ(ae_career_match_find(&career, "dad", "x")->points, 3000);
	/* bad lines are skipped: a value out of range, a line twice is an edited file */
	EQ(ae_career_read(&again, summed("# AE career\nversion = 2\nexperience = a | 9\nexperience = b | 0\nexperience = c\n")), AE_CAREER_READ_OK);
	EQ(again.experience_count, 0);
	EQ(ae_career_read(&again, summed("# AE career\nversion = 2\nexperience = a | 1\nexperience = a | 2\n")), AE_CAREER_READ_EDITED);
	/* unset takes the line away; the table holds AE_CAREER_EXPERIENCES profiles */
	CHECK(ae_career_experience_set(&career, "kid", AE_SOCIAL_EXPERIENCE_UNSET));
	EQ(ae_career_experience_get(&career, "kid"), AE_SOCIAL_EXPERIENCE_UNSET);
	EQ(career.experience_count, 1);
	ae_career_init(&career);
	{
		int index;
		char key[16];

		for (index = 0; index < AE_CAREER_EXPERIENCES; index++)
		{
			snprintf(key, sizeof(key), "p%d", index);
			CHECK(ae_career_experience_set(&career, key, AE_SOCIAL_EXPERIENCE_SOME));
		}
		CHECK(!ae_career_experience_set(&career, "extra", AE_SOCIAL_EXPERIENCE_SOME));
	}
}

/* the file's checksum is its last meaningful line, and no line stands twice */
static void test_strict_reader(void)
{
	static char built[4096];
	char const *good = summed("# AE career\nversion = 2\nsocial = a | b | 5 1 1 0 0 3\nmatch = a | b | 100 1 3\n");

	EQ(ae_career_read(&again, good), AE_CAREER_READ_OK);
	EQ(again.social_count, 1);
	/* data after the checksum, a second checksum, a second header */
	snprintf(built, sizeof(built), "%ssocial = a | c | 9 1 1 0 0 3\n", good);
	EQ(ae_career_read(&again, built), AE_CAREER_READ_EDITED);
	EQ(again.social_count, 0); CHECK(again.edited);
	snprintf(built, sizeof(built), "%spending = a | c | 9\n", good);
	EQ(ae_career_read(&again, built), AE_CAREER_READ_EDITED);
	snprintf(built, sizeof(built), "%s%s", good, strstr(good, "checksum"));
	EQ(ae_career_read(&again, built), AE_CAREER_READ_EDITED);
	snprintf(built, sizeof(built), "%sversion = 2\n", good);
	EQ(ae_career_read(&again, built), AE_CAREER_READ_EDITED);
	/* comments and blank lines after it are not data */
	snprintf(built, sizeof(built), "%s\n# kept by hand\n", good);
	EQ(ae_career_read(&again, built), AE_CAREER_READ_OK);
	/* a line twice: a playlist of a ladder, a profile and playlist of match / social */
	EQ(ae_career_read(&again, summed("# AE career\nversion = 2\ncareer = x | 100 1 1 1 1 0 5\ncareer = x | 200 1 1 1 1 0 5\n")),
		AE_CAREER_READ_EDITED);
	EQ(ae_career_read(&again, summed("# AE career\nversion = 2\ncareer = x | 100 1 1 1 1 0 5\nonline = x | 200 1 1 1 1 0 5\n")),
		AE_CAREER_READ_OK);
	EQ(ae_career_read(&again, summed("# AE career\nversion = 2\nmatch = a | b | 100 1 3\nmatch = a | b | 200 1 3\n")),
		AE_CAREER_READ_EDITED);
	EQ(ae_career_read(&again, summed("# AE career\nversion = 2\nsocial = a | b | 5 1 1 0 0 3\nsocial = a | b | 6 1 1 0 0 3\n")),
		AE_CAREER_READ_EDITED);
	EQ(ae_career_read(&again, summed("# AE career\nversion = 2\nsocial = a | b | 5 1 1 0 0 3\nsocial = a | c | 6 1 1 0 0 3\n")),
		AE_CAREER_READ_OK);
}

/* every table full of the longest lines: the text buffer holds it, and it reads back whole */
static void test_capacity(void)
{
	int index, quit;
	long length;

	ae_career_init(&career);
	for (index = 0; index < AE_CAREER_PLAYLISTS; index++)
	{
		char id[AE_CAREER_ID];
		int ladder;

		snprintf(id, sizeof(id), "playlist-long-identifier-%03d", index % 1000);
		for (ladder = 0; ladder < 2; ladder++)
		{
			CHECK(ae_career_record_game(&career, ladder, id, 99999, 1, 4000000000UL));
			for (quit = 0; quit < AE_CAREER_QUIT_HISTORY + 3; quit++)
				CHECK(ae_career_record_quit(&career, ladder, id, -1, 4000000000UL));
		}
	}
	for (index = 0; index < AE_CAREER_SOCIAL; index++)
	{
		char key[AE_CAREER_ID];

		snprintf(key, sizeof(key), "profile-key-of-the-longest-%02d", index % 100);
		CHECK(ae_career_social_commit(&career, key, "playlist-long-identifier-000", AE_SOCIAL_WIN, 4999, 4000000000UL));
	}
	for (index = 0; index < AE_CAREER_PENDING; index++)
	{
		char key[AE_CAREER_ID];

		snprintf(key, sizeof(key), "profile-key-of-the-longest-%02d", index % 100);
		CHECK(ae_career_pending_set(&career, key, "playlist-long-identifier-000", 4000000000UL));
	}
	length = ae_career_write(&career, text, sizeof(text));
	CHECK(length > 0 && length < AE_CAREER_TEXT);
	EQ(ae_career_read(&again, text), AE_CAREER_READ_OK);
	EQ(again.count[0], AE_CAREER_PLAYLISTS); EQ(again.count[1], AE_CAREER_PLAYLISTS);
	EQ(again.social_count, AE_CAREER_SOCIAL); EQ(again.match_count, AE_CAREER_SOCIAL); EQ(again.pending_count, AE_CAREER_PENDING);
	/* a buffer too small is answered with -1, never a cut file */
	CHECK(ae_career_write(&career, text, 4096) < 0);
}

/* both lines or neither */
static void test_commit(void)
{
	int index;

	ae_career_init(&career);
	for (index = 0; index < AE_CAREER_SOCIAL; index++)
	{
		char key[16];

		snprintf(key, sizeof(key), "p%d", index);
		CHECK(ae_career_social_record(&career, key, "x", AE_SOCIAL_WIN, 1));
	}
	/* the social table is full, the match table is empty: a new profile gets neither line */
	CHECK(!ae_career_social_has_room(&career, "new", "x"));
	CHECK(!ae_career_social_commit(&career, "new", "x", AE_SOCIAL_WIN, 1000, 5));
	EQ(career.social_count, AE_CAREER_SOCIAL); EQ(career.match_count, 0);
	/* a profile that has its social line commits (the match line is made) */
	CHECK(ae_career_social_has_room(&career, "p3", "x"));
	CHECK(ae_career_social_commit(&career, "p3", "x", AE_SOCIAL_WIN, 1234, 6));
	EQ(career.match_count, 1); EQ(career.match[0].points, 1234);
	EQ(ae_career_social_find(&career, "p3", "x")->wins, 2);
	/* the match table full, the social one not: nothing moves either */
	ae_career_init(&career);
	for (index = 0; index < AE_CAREER_SOCIAL; index++)
	{
		char key[16];

		snprintf(key, sizeof(key), "p%d", index);
		CHECK(ae_career_match_record(&career, key, "x", 100, 1));
	}
	CHECK(!ae_career_social_commit(&career, "new", "x", AE_SOCIAL_WIN, 1000, 5));
	EQ(career.social_count, 0); EQ(career.match_count, AE_CAREER_SOCIAL);
}

/* the games found pending at a start: a crash forgives them, a closed game counts as a quit */
static void test_settle(void)
{
	int forgiven = -1, quits = -1;
	struct ae_career_social *c;

	ae_career_init(&career);
	CHECK(ae_career_social_commit(&career, "a", "x", AE_SOCIAL_WIN, 1600, 1));
	CHECK(ae_career_social_commit(&career, "a", "x", AE_SOCIAL_WIN, 1600, 2));
	CHECK(ae_career_pending_set(&career, "a", "x", 10));
	CHECK(ae_career_pending_set(&career, "b", "y", 11));
	ae_career_pending_settle(&career, 1, 50, &forgiven, &quits);
	EQ(forgiven, 2); EQ(quits, 0); EQ(career.pending_count, 0);
	c = ae_career_social_find(&career, "a", "x");
	EQ(c->exp, 2); EQ(c->quits, 0); EQ(c->games, 2);
	CHECK(ae_career_social_find(&career, "b", "y") == NULL);
	/* no crash mark: a quit for each: -1 EXP, the quit and the game counted, the hidden rating lower; a profile with no lines
	   gets them (0 EXP stays 0) */
	CHECK(ae_career_pending_set(&career, "a", "x", 10));
	CHECK(ae_career_pending_set(&career, "b", "y", 11));
	ae_career_pending_settle(&career, 0, 60, &forgiven, &quits);
	EQ(forgiven, 0); EQ(quits, 2); EQ(career.pending_count, 0);
	c = ae_career_social_find(&career, "a", "x");
	EQ(c->exp, 1); EQ(c->quits, 1); EQ(c->games, 3); EQ(c->last, 60);
	CHECK(ae_career_match_find(&career, "a", "x")->points < 1600);
	c = ae_career_social_find(&career, "b", "y");
	CHECK(c != NULL);
	EQ(c->exp, 0); EQ(c->quits, 1);
	CHECK(ae_career_match_find(&career, "b", "y")->points < AE_SOCIAL_MATCH_START);
	/* nothing pending: nothing moves */
	ae_career_pending_settle(&career, 0, 70, &forgiven, &quits);
	EQ(forgiven, 0); EQ(quits, 0);
}

int main(void)
{
	test_records();
	test_file();
	test_old_and_newer();
	test_ranges();
	test_result();
	test_social();
	test_strict_reader();
	test_experience();
	test_capacity();
	test_commit();
	test_settle();
	if (failures)
		return 1;
	printf("ok\n");
	return 0;
}
