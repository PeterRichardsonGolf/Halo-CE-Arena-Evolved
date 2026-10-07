/* (snprintf under -std=c89 as well) */
#define _POSIX_C_SOURCE 200112L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ae_catalog.h"

static int failures, logged, log_bad;
static char last_logged[256];
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

/* counts what is logged; a logged line must be short and free of control characters */
static void count_log(char const *what, char const *line)
{
	size_t index;

	(void)what;
	logged++;
	if (strlen(line) > 120)
		log_bad++;
	for (index = 0; line[index]; index++)
		log_bad += (unsigned char)line[index] < 0x20;
	snprintf(last_logged, sizeof(last_logged), "%s", line);
}

/* (about 300 KB each: static, never on the stack) */
static struct ae_catalog catalog, again;
static char text[256 * 1024], written[256 * 1024], other[256 * 1024];

/* every string within its room, every count within its cap, every fact sane */
static void check_invariants(struct ae_catalog const *c)
{
	int i, j, maps = 0, gametypes = 0;

	CHECK(c->facts_count >= 0 && c->facts_count <= AE_CATALOG_MAXIMUM_ITEMS);
	CHECK(c->favourite_count >= 0 && c->favourite_count <= AE_CATALOG_MAXIMUM_ITEMS);
	CHECK(c->recent_count >= 0 && c->recent_count <= AE_CATALOG_RECENT * 2);
	CHECK(c->playlist_count >= 0 && c->playlist_count <= AE_CATALOG_PLAYLISTS);
	for (i = 0; i < c->facts_count; i++)
	{
		struct ae_map_facts const *f = &c->facts[i];

		CHECK(strlen(f->file) > 0 && strlen(f->file) < AE_CATALOG_NAME && !strchr(f->file, '/') &&
			!strchr(f->file, '\\') && !strstr(f->file, ".."));
		CHECK(strlen(f->name) < AE_CATALOG_NAME && strlen(f->tags) < AE_CATALOG_NAME &&
			strlen(f->credits) < AE_CATALOG_NAME);
		CHECK(f->players_min >= 1 && f->players_min <= f->players_max && f->players_max <= 128);
		CHECK(f->size == 0 || f->size == 's' || f->size == 'm' || f->size == 'l');
	}
	for (i = 0; i < c->favourite_count; i++)
		CHECK(strlen(c->favourites[i]) < AE_CATALOG_NAME &&
			(!strncmp(c->favourites[i], "map:", 4) || !strncmp(c->favourites[i], "gametype:", 9)));
	for (i = 0; i < c->recent_count; i++)
	{
		CHECK(strlen(c->recent[i]) < AE_CATALOG_NAME);
		maps += !strncmp(c->recent[i], "map:", 4);
		gametypes += !strncmp(c->recent[i], "gametype:", 9);
	}
	CHECK(maps <= AE_CATALOG_RECENT && gametypes <= AE_CATALOG_RECENT && maps + gametypes == c->recent_count);
	for (i = 0; i < c->playlist_count; i++)
	{
		CHECK(strlen(c->playlists[i].name) < AE_CATALOG_NAME);
		CHECK(c->playlists[i].count >= 0 && c->playlists[i].count <= AE_CATALOG_PLAYLIST_MAPS);
		for (j = 0; j < c->playlists[i].count; j++)
			CHECK(strlen(c->playlists[i].maps[j]) > 0 && strlen(c->playlists[i].maps[j]) < AE_CATALOG_NAME);
		for (j = 0; j < i; j++)
			CHECK(strcmp(c->playlists[i].name, c->playlists[j].name) != 0);
	}
}

static void check_edges(void)
{
	static char const *const bad_players[] =
	{
		"99999999999-1", "-1-2", "2-", "2--3", "1-129", "0-5", "5-0", "", "-", "2-8x", "2 -8", "1-99999999999"
	};
	char entry[64];
	long length;
	int i;

	/* players: overflow, negatives, out of range, junk */
	for (i = 0; i < (int)(sizeof(bad_players) / sizeof(bad_players[0])); i++)
	{
		memset(&again, 0, sizeof(again));
		snprintf(text, sizeof(text), "p.map\tP\t%s\tm\t\t\n", bad_players[i]);
		ae_catalog_read_facts(&again, text);
		CHECK(again.facts_count == 0 && again.skipped_lines == 1);
	}
	memset(&again, 0, sizeof(again));
	ae_catalog_read_facts(&again, "p.map\tP\t1-128\tm\t\t\nq.map\tQ\t16-16\ts\t\t\n");
	CHECK(again.facts_count == 2 && again.skipped_lines == 0);
	/* file names are never paths */
	memset(&again, 0, sizeof(again));
	ae_catalog_read_facts(&again, "a/b.map\tA\t2-8\tm\t\t\nc\\d.map\tC\t2-8\tm\t\t\n..\tD\t2-8\tm\t\t\nx..y\tE\t2-8\tm\t\t\n");
	CHECK(again.facts_count == 0 && again.skipped_lines == 4);
	/* the log: the original line (all its fields), cut to 120 bytes, no control characters */
	logged = 0;
	memset(&again, 0, sizeof(again));
	ae_catalog_read_facts(&again, "short.map\tShort\t2-8\n");
	CHECK(logged == 1 && !strcmp(last_logged, "short.map?Short?2-8"));
	memset(text, 'y', 600);
	text[600] = 0;
	ae_catalog_read_favourites(&again, text);
	CHECK(logged == 2 && strlen(last_logged) == 120);
	/* at most 16 lines of a catalog logged, then one line for the rest */
	text[0] = 0;
	for (i = 0; i < 40; i++)
		strcat(text, "junk\n");
	logged = 0;
	memset(&again, 0, sizeof(again));
	ae_catalog_read_favourites(&again, text);
	CHECK(again.skipped_lines == 40 && logged == 17);
	/* a byte-order mark */
	memset(&again, 0, sizeof(again));
	ae_catalog_read_favourites(&again, "\xEF\xBB\xBFmap:first.map\nmap:second.map\n");
	CHECK(again.favourite_count == 2 && !strcmp(again.favourites[0], "map:first.map") && again.skipped_lines == 0);
	memset(&again, 0, sizeof(again));
	ae_catalog_read_facts(&again, "\xEF\xBB\xBF# comment\nb.map\tB\t2-8\tm\t\t\n");
	CHECK(again.facts_count == 1 && again.skipped_lines == 0);
	/* trimmed entries and map lines */
	memset(&again, 0, sizeof(again));
	ae_catalog_read_favourites(&again, "  map:spaced.map \t\n");
	CHECK(again.favourite_count == 1 && !strcmp(again.favourites[0], "map:spaced.map"));
	ae_catalog_read_playlists(&again, " [ List ] \n  one.map  \n\ttwo.map\nbad\x01.map\n");
	CHECK(again.playlist_count == 1 && again.playlists[0].count == 2 && !strcmp(again.playlists[0].maps[0], "one.map"));
	CHECK(again.skipped_lines == 1);
	/* a playlist read again replaces the earlier one (no second of the same name) */
	ae_catalog_read_playlists(&again, "[ List ]\nthree.map\n");
	CHECK(again.playlist_count == 1 && again.playlists[0].count == 1 && !strcmp(again.playlists[0].maps[0], "three.map"));
	/* a 1000+ byte favourites line */
	memset(&again, 0, sizeof(again));
	memcpy(text, "map:", 4);
	memset(text + 4, 'z', 1500);
	strcpy(text + 1504, "\nmap:after.map\n");
	ae_catalog_read_favourites(&again, text);
	CHECK(again.favourite_count == 1 && !strcmp(again.favourites[0], "map:after.map") && again.skipped_lines == 1);

	/* the caps: 512 facts, 512 favourites (and a toggle at the cap), 32 playlists */
	memset(&catalog, 0, sizeof(catalog));
	{
		char *end = text;

		for (i = 0; i < AE_CATALOG_MAXIMUM_ITEMS + 1; i++)
			end += sprintf(end, "m%03d.map\tM\t2-8\tm\t\t\n", i);
	}
	ae_catalog_read_facts(&catalog, text);
	CHECK(catalog.facts_count == AE_CATALOG_MAXIMUM_ITEMS && catalog.skipped_lines == 1);
	for (i = 0; i < AE_CATALOG_MAXIMUM_ITEMS; i++)
	{
		snprintf(entry, sizeof(entry), "map:%059d", i);                           /* 63 bytes: the most */
		CHECK(ae_catalog_favourite_toggle(&catalog, entry) == 1);
	}
	CHECK(ae_catalog_favourite_toggle(&catalog, "map:one-more.map") == -1);       /* full */
	CHECK(ae_catalog_favourite_toggle(&catalog, entry) == 0);                     /* removing still works */
	CHECK(ae_catalog_favourite_toggle(&catalog, entry) == 1 && catalog.favourite_count == AE_CATALOG_MAXIMUM_ITEMS);
	length = ae_catalog_write_favourites(&catalog, written, AE_CATALOG_FAVOURITES_TEXT);
	CHECK(length > 0 && length < AE_CATALOG_FAVOURITES_TEXT);                       /* the stated size holds them all */
	memset(&again, 0, sizeof(again));
	ae_catalog_read_favourites(&again, written);
	ae_catalog_read_favourites(&again, "map:over.map\n");
	CHECK(again.favourite_count == AE_CATALOG_MAXIMUM_ITEMS && again.skipped_lines == 1);
	{
		char *end = text;

		for (i = 0; i < AE_CATALOG_PLAYLISTS + 1; i++)
		{
			int map;

			end += sprintf(end, "[%062d]\n", i);
			for (map = 0; map < AE_CATALOG_PLAYLIST_MAPS; map++)
				end += sprintf(end, "%059d.map\n", map);
		}
	}
	memset(&catalog, 0, sizeof(catalog));
	ae_catalog_read_playlists(&catalog, text);
	CHECK(catalog.playlist_count == AE_CATALOG_PLAYLISTS);
	CHECK(catalog.skipped_lines == 1 + AE_CATALOG_PLAYLIST_MAPS);   /* the 33rd's header and its maps */
	length = ae_catalog_write_playlists(&catalog, written, AE_CATALOG_PLAYLISTS_TEXT);
	CHECK(length > 0 && length < AE_CATALOG_PLAYLISTS_TEXT);
	/* exact fits: room for the text and its NUL works, one byte less doesn't */
	CHECK(ae_catalog_write_playlists(&catalog, other, length + 1) == length && !strcmp(other, written));
	CHECK(ae_catalog_write_playlists(&catalog, other, length) == -1);
	memset(&again, 0, sizeof(again));
	for (i = 0; i < 25; i++)
	{
		snprintf(entry, sizeof(entry), i % 2 ? "map:%058d" : "gametype:%053d", i);
		ae_catalog_recent_push(&again, entry);
	}
	length = ae_catalog_write_recent(&again, written, AE_CATALOG_RECENT_TEXT);
	CHECK(again.recent_count == 20 && length > 0 && length < AE_CATALOG_RECENT_TEXT);
	CHECK(ae_catalog_write_recent(&again, other, length + 1) == length);
	CHECK(ae_catalog_write_recent(&again, other, length) == -1);
	CHECK(ae_catalog_write_favourites(&again, other, 0) == -1 && ae_catalog_write_favourites(&again, NULL, 100) == -1);
}

/* texts built from the formats' own pieces (so many lines are valid, and caps are reached), read into one catalog
again and again: every invariant holds */
static void check_structured_fuzz(void)
{
	static char const *const pieces[] =
	{
		"map:", "gametype:", "\t", "\t", "[", "]", "\n", "\r\n", "#", "-", "2-8", "1-128", "0-5", "s", "m", "l",
		"bloodgulch.map", "AE FFA SLAY", "x", "..", "/", "\\", "\xC3\xA9", "\xEF\xBB\xBF", " ", "\x01", "\x7F",
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
	};
	int round, i;

	srand(2024);
	memset(&catalog, 0, sizeof(catalog));
	for (round = 0; round < 400; round++)
	{
		size_t used = 0;
		int count = rand() % 600;

		for (i = 0; i < count; i++)
		{
			char const *piece = pieces[rand() % (int)(sizeof(pieces) / sizeof(pieces[0]))];
			size_t length = strlen(piece);

			if (used + length + 1 >= sizeof(text))
				break;
			memcpy(text + used, piece, length);
			used += length;
		}
		text[used] = 0;
		switch (round % 4)
		{
		case 0: ae_catalog_read_facts(&catalog, text); break;
		case 1: ae_catalog_read_favourites(&catalog, text); break;
		case 2: ae_catalog_read_recent(&catalog, text); break;
		default: ae_catalog_read_playlists(&catalog, text); break;
		}
		if (round % 7 == 0)
		{
			ae_catalog_recent_push(&catalog, text);
			ae_catalog_favourite_toggle(&catalog, text);
		}
		check_invariants(&catalog);
		if (round % 50 == 0)
		{
			ae_catalog_write_favourites(&catalog, written, sizeof(written));
			ae_catalog_write_recent(&catalog, written, sizeof(written));
			ae_catalog_write_playlists(&catalog, written, sizeof(written));
		}
	}
}

int main(void)
{
	struct ae_map_facts const *facts;
	char name[200];
	long length;
	int i;

	ae_catalog_set_log(count_log);

	/* maps.txt: a valid line, comments, CRLF, too few fields, players swapped or bad, a bad size, an over-long name */
	memset(&catalog, 0, sizeof(catalog));
	memset(name, 'N', sizeof(name) - 1);
	name[sizeof(name) - 1] = 0;
	snprintf(text, sizeof(text),
		"# file\tname\tplayers\tsize\ttags\tcredits\r\n"
		"bloodgulch.map\tBlood Gulch\t4-16\tl\tvehicles,ctf\tBungie\r\n"
		"chillout.map\tChill Out\t2-8\ts\tsmall\tBungie\n"
		"short.map\tShort\t2-8\n"
		"swapped.map\tSwapped\t8-2\tm\t\t\n"
		"words.map\tWords\tmany\tm\t\t\n"
		"badsize.map\tBad Size\t2-8\tx\t\t\n"
		"long.map\t%s\t2-4\t-\t\t\n"
		"\n"
		"last.map\tNo newline at the end\t1-1\tm\ttag\tsomeone", name);
	ae_catalog_read_facts(&catalog, text);
	CHECK(catalog.facts_count == 4);
	CHECK(catalog.skipped_lines == 4 && logged == 4);
	facts = ae_catalog_facts(&catalog, "bloodgulch.map");
	CHECK(facts && !strcmp(facts->name, "Blood Gulch") && facts->players_min == 4 && facts->players_max == 16 &&
		facts->size == 'l' && !strcmp(facts->tags, "vehicles,ctf") && !strcmp(facts->credits, "Bungie"));
	facts = ae_catalog_facts(&catalog, "long.map");
	CHECK(facts && strlen(facts->name) == AE_CATALOG_NAME - 1 && facts->size == 0);   /* cut, not overflowed */
	facts = ae_catalog_facts(&catalog, "last.map");
	CHECK(facts && facts->players_min == 1 && !strcmp(facts->credits, "someone"));
	CHECK(!ae_catalog_facts(&catalog, "swapped.map") && !ae_catalog_facts(&catalog, "missing.map"));
	CHECK(!ae_catalog_facts(&catalog, NULL));
	/* read again: a map's facts replace the earlier ones */
	ae_catalog_read_facts(&catalog, "chillout.map\tChill Out (remake)\t2-6\tm\t\t\n");
	CHECK(catalog.facts_count == 4 && ae_catalog_facts(&catalog, "chillout.map")->players_max == 6);
	/* a name cut inside a UTF-8 character is cut before it */
	memset(name, 'a', 62);
	strcpy(name + 62, "\xC3\xA9tail");                                 /* "é" across the 64-byte room */
	snprintf(text, sizeof(text), "utf8.map\t%s\t2-8\tm\t\t\n", name);
	ae_catalog_read_facts(&catalog, text);
	facts = ae_catalog_facts(&catalog, "utf8.map");
	CHECK(facts && strlen(facts->name) == 62);

	/* empty and NULL text */
	memset(&again, 0, sizeof(again));
	ae_catalog_read_facts(&again, "");
	ae_catalog_read_facts(&again, NULL);
	ae_catalog_read_favourites(&again, "");
	ae_catalog_read_recent(&again, NULL);
	ae_catalog_read_playlists(&again, "\r\n\r\n");
	CHECK(again.facts_count == 0 && again.favourite_count == 0 && again.recent_count == 0 &&
		again.playlist_count == 0 && again.skipped_lines == 0);

	/* favourites: toggling twice, entries checked */
	memset(&catalog, 0, sizeof(catalog));
	CHECK(ae_catalog_favourite_toggle(&catalog, "map:bloodgulch.map") == 1);
	CHECK(ae_catalog_favourite_toggle(&catalog, "gametype:AE FFA SLAY") == 1);
	CHECK(ae_catalog_favourite_toggle(&catalog, "map:chillout.map") == 1);
	CHECK(ae_catalog_favourite_toggle(&catalog, "map:bloodgulch.map") == 0 && catalog.favourite_count == 2);
	CHECK(ae_catalog_favourite_toggle(&catalog, "map:bloodgulch.map") == 1 && catalog.favourite_count == 3);
	CHECK(ae_catalog_favourite_toggle(&catalog, "bloodgulch.map") == -1);         /* no kind */
	CHECK(ae_catalog_favourite_toggle(&catalog, "map:") == -1);                   /* no name */
	CHECK(ae_catalog_favourite_toggle(&catalog, "map:a\nb") == -1);               /* a control character */
	CHECK(ae_catalog_favourite_toggle(&catalog, NULL) == -1 && catalog.favourite_count == 3);
	length = ae_catalog_write_favourites(&catalog, written, sizeof(written));
	CHECK(length > 0 && (size_t)length == strlen(written));
	memset(&again, 0, sizeof(again));
	ae_catalog_read_favourites(&again, written);
	CHECK(again.favourite_count == 3 && again.skipped_lines == 0);
	for (i = 0; i < 3; i++)
		CHECK(!strcmp(again.favourites[i], catalog.favourites[i]));
	CHECK(ae_catalog_write_favourites(&catalog, written, 10) == -1);               /* doesn't fit */
	ae_catalog_read_favourites(&again, "map:bloodgulch.map\nnot an entry\nmap:new.map\r\n");
	CHECK(again.favourite_count == 4 && again.skipped_lines == 1);

	/* recent: newest first, unique, at most 10 of each kind */
	memset(&catalog, 0, sizeof(catalog));
	for (i = 0; i < 14; i++)
	{
		char entry[32];

		snprintf(entry, sizeof(entry), "map:m%d.map", i);
		ae_catalog_recent_push(&catalog, entry);
		snprintf(entry, sizeof(entry), "gametype:g%d", i % 3);
		ae_catalog_recent_push(&catalog, entry);
	}
	CHECK(catalog.recent_count == 13);                                            /* 10 maps, 3 gametypes */
	CHECK(!strcmp(catalog.recent[0], "gametype:g1") && !strcmp(catalog.recent[1], "map:m13.map"));
	ae_catalog_recent_push(&catalog, "map:m5.map");                               /* again: to the front, once */
	CHECK(catalog.recent_count == 13 && !strcmp(catalog.recent[0], "map:m5.map"));
	{
		int maps = 0, oldest_gone = 1;

		for (i = 0; i < catalog.recent_count; i++)
		{
			maps += !strncmp(catalog.recent[i], "map:", 4);
			oldest_gone &= strcmp(catalog.recent[i], "map:m3.map") != 0;
		}
		CHECK(maps == 10 && oldest_gone);
	}
	ae_catalog_recent_push(&catalog, "nothing");
	CHECK(catalog.recent_count == 13);
	length = ae_catalog_write_recent(&catalog, written, sizeof(written));
	CHECK(length > 0);
	memset(&again, 0, sizeof(again));
	ae_catalog_read_recent(&again, written);
	CHECK(again.recent_count == catalog.recent_count && again.skipped_lines == 0);
	for (i = 0; i < again.recent_count; i++)
		CHECK(!strcmp(again.recent[i], catalog.recent[i]));
	/* a file with more than 10 of a kind: the newest 10 kept */
	text[0] = 0;
	for (i = 0; i < 12; i++)
	{
		char entry[32];

		snprintf(entry, sizeof(entry), "map:r%d.map\n", i);
		strcat(text, entry);
	}
	memset(&again, 0, sizeof(again));
	ae_catalog_read_recent(&again, text);
	CHECK(again.recent_count == 10 && again.skipped_lines == 2 && !strcmp(again.recent[0], "map:r0.map"));

	/* playlists: a round trip; 65 maps (64 kept, 1 skipped); maps outside a playlist; bad headers */
	memset(&catalog, 0, sizeof(catalog));
	strcpy(text, "# playlists\nstray.map\n[Team Slayer]\r\nbloodgulch.map\ndamnation.map\n[]\n[Big]\n");
	for (i = 0; i < 65; i++)
	{
		char line[32];

		snprintf(line, sizeof(line), "map%02d.map\n", i);
		strcat(text, line);
	}
	strcat(text, "[unclosed\nafter.map\n");
	ae_catalog_read_playlists(&catalog, text);
	CHECK(catalog.playlist_count == 2);
	CHECK(!strcmp(catalog.playlists[0].name, "Team Slayer") && catalog.playlists[0].count == 2);
	CHECK(!strcmp(catalog.playlists[1].name, "Big") && catalog.playlists[1].count == 64);
	CHECK(!strcmp(catalog.playlists[1].maps[63], "map63.map"));
	/* (skipped: stray.map, [], map64.map, [unclosed, after.map) */
	CHECK(catalog.skipped_lines == 5);
	length = ae_catalog_write_playlists(&catalog, written, sizeof(written));
	CHECK(length > 0);
	memset(&again, 0, sizeof(again));
	ae_catalog_read_playlists(&again, written);
	CHECK(again.playlist_count == 2 && again.skipped_lines == 0);
	CHECK(ae_catalog_write_playlists(&again, other, sizeof(other)) == length && !strcmp(written, other));
	CHECK(ae_catalog_write_playlists(&catalog, written, 100) == -1);

	check_edges();
	check_structured_fuzz();

	/* corrupt text: random bytes, a very long line, no crash (the sanitizers) and nothing past its room */
	srand(99);
	for (i = 0; i < 300; i++)
	{
		size_t n = (size_t)(rand() % 4000), k;

		for (k = 0; k < n; k++)
			text[k] = (char)(1 + rand() % 255);
		text[n] = 0;
		memset(&again, 0, sizeof(again));
		ae_catalog_read_facts(&again, text);
		ae_catalog_read_favourites(&again, text);
		ae_catalog_read_recent(&again, text);
		ae_catalog_read_playlists(&again, text);
		ae_catalog_write_playlists(&again, written, sizeof(written));
	}
	memset(text, 'x', 100000);
	text[100000] = 0;
	memset(&again, 0, sizeof(again));
	ae_catalog_read_facts(&again, text);
	ae_catalog_read_playlists(&again, text);
	CHECK(again.facts_count == 0 && again.playlist_count == 0);

	CHECK(log_bad == 0);
	ae_catalog_set_log(NULL);
	if (failures)
		return 1;
	printf("ae_catalog: ok\n");
	return 0;
}
