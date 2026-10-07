#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ae_catalog.h"

static int failures, logged;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static void count_log(char const *what, char const *line) { (void)what; (void)line; logged++; }

/* (about 300 KB each: static, never on the stack) */
static struct ae_catalog catalog, again;
static char text[256 * 1024], written[256 * 1024], other[256 * 1024];

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
	CHECK(ae_catalog_favourite_toggle(&catalog, "bloodgulch.map") == 0);          /* no kind */
	CHECK(ae_catalog_favourite_toggle(&catalog, "map:") == 0);                    /* no name */
	CHECK(ae_catalog_favourite_toggle(&catalog, "map:a\nb") == 0);                /* a control character */
	CHECK(ae_catalog_favourite_toggle(&catalog, NULL) == 0 && catalog.favourite_count == 3);
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

	if (failures)
		return 1;
	printf("ae_catalog: ok\n");
	return 0;
}
