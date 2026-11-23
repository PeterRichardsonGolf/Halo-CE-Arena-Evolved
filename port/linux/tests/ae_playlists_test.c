/* ae_playlists_test.c: several named playlists (ae_playlists.h): the file and its round trip, the migration of playlist.txt, the
names, delete and use, the three orders (IN TURN, SHUFFLE, RANDOM with NO REPEATS), UP NEXT and PLAYED, and HOST A SERVER's
rotation text */

#include <stdio.h>
#include <string.h>

#include "ae_playlists.h"

static int failures;
#define CHECK(condition) do { if (!(condition)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
	failures++; } } while (0)

static struct ae_playlists set, back;
static char text[AE_PLAYLISTS_FILE];

static void fill(struct ae_playlists *s)
{
	int made;

	ae_playlists_init(s);
	made = ae_playlists_new(s, "4v4 arena", NULL);
	ae_playlist_add(&s->lists[made], "damnation", "AE TEAM SLY");
	ae_playlist_add(&s->lists[made], "prisoner", "AE SLAYER");
	ae_playlist_add(&s->lists[made], "hangemhigh", "AE KING");
	s->lists[made].order = AE_PLAYLIST_ORDER_SHUFFLE;
	s->next[made] = 3;
	made = ae_playlists_new(s, "FFA Night", NULL);
	ae_playlist_add(&s->lists[made], "sali_creek", "AE FFA");
	ae_playlist_add(&s->lists[made], "weld", "AE FFA");
	s->lists[made].order = AE_PLAYLIST_ORDER_RANDOM;
	s->lists[made].no_repeats = 0;
	s->lists[made].map_vote = 0;
	s->lists[made].entries[1].weight = 5;
	made = ae_playlists_new(s, "SWAT TIME", NULL);
	ae_playlist_add(&s->lists[made], "turf", "SWAT");
	s->lists[made].order = AE_PLAYLIST_ORDER_IN_TURN;
	s->in_use = 1;
}

int main(void)
{
	int reason, index, pass;
	char name[40], message[128];

	/* 1. the file's round trip of a 3-section set */
	fill(&set);
	CHECK(set.count == 3 && !strcmp(set.lists[0].name, "4V4 ARENA") && !strcmp(set.lists[1].name, "FFA NIGHT"));
	CHECK(ae_playlists_write(&set, text, sizeof(text)) > 0);
	CHECK(strstr(text, "# in use: FFA NIGHT") && strstr(text, "# order: shuffle") && strstr(text, "# next: 3"));
	CHECK(ae_playlists_read(&back, text) == 3);
	CHECK(back.in_use == 1 && !strcmp(back.lists[1].name, "FFA NIGHT") && back.lists[0].count == 3);
	CHECK(back.lists[0].order == AE_PLAYLIST_ORDER_SHUFFLE && back.next[0] == 3 && back.next[1] == 0);
	CHECK(back.lists[1].order == AE_PLAYLIST_ORDER_RANDOM && !back.lists[1].no_repeats && !back.lists[1].map_vote &&
		back.lists[1].entries[1].weight == 5 && !strcmp(back.lists[1].entries[1].map, "weld"));
	CHECK(back.lists[2].order == AE_PLAYLIST_ORDER_IN_TURN && !strcmp(back.lists[2].entries[0].gametype, "SWAT"));
	/* (a set with no room) */
	CHECK(ae_playlists_write(&set, text, 40) == -1);
	/* (an empty playlist is not written, and the in use one that is empty reads OFF) */
	ae_playlists_new(&set, "EMPTY ONE", NULL);
	set.in_use = 3;
	CHECK(ae_playlists_write(&set, text, sizeof(text)) > 0 && !strstr(text, "EMPTY ONE") && strstr(text, "# in use: OFF"));
	CHECK(ae_playlists_read(&back, text) == 3 && back.in_use == -1);
	/* a section with an unknown comment and no order: RANDOM, the unknown line skipped */
	CHECK(ae_playlists_read(&back, "# in use: off\n# playlist: A\n# whatever: 1\nweld:file=AE CTF  # weight 4\n") == 1);
	CHECK(back.in_use == -1 && back.lists[0].order == AE_PLAYLIST_ORDER_RANDOM && back.lists[0].count == 1 &&
		back.lists[0].entries[0].weight == 4);
	/* games before any section go to MY PLAYLIST */
	CHECK(ae_playlists_read(&back, "# in use: MY PLAYLIST\nweld:file=AE CTF\nturf:file=AE KING\n") == 1);
	CHECK(!strcmp(back.lists[0].name, "MY PLAYLIST") && back.lists[0].count == 2 && back.in_use == 0);
	/* more than 16 sections keep the first 16; a repeated name is made unique, a bad one cleaned */
	{
		static char many[AE_PLAYLISTS_FILE];
		int each, used = 0;

		for (each = 0; each < 20; each++)
			used += snprintf(many + used, sizeof(many) - (size_t)used, "# playlist: %s\nweld:file=AE CTF\n",
				each == 1 ? "LIST 0" : each == 2 ? "bad/name" : "LIST 0");
		CHECK(ae_playlists_read(&back, many) == AE_PLAYLISTS_MAX);
		CHECK(!strcmp(back.lists[0].name, "LIST 0") && !strcmp(back.lists[1].name, "LIST 0 2") &&
			!strcmp(back.lists[2].name, "BAD NAME") && !strcmp(back.lists[3].name, "LIST 0 3"));
	}

	/* 2. the migration of today's playlist.txt */
	CHECK(ae_playlists_migrate(&back, "# playlist: OLD ONE\n# map vote: 0\n# no repeats: 0\nweld:file=AE CTF  # weight 4\n"
		"sali_creek:file=AE KING  # weight 1\n") == 1);
	CHECK(back.count == 1 && back.in_use == -1 && !strcmp(back.lists[0].name, "OLD ONE") && back.lists[0].count == 2);
	CHECK(back.lists[0].order == AE_PLAYLIST_ORDER_RANDOM && !back.lists[0].map_vote && !back.lists[0].no_repeats &&
		back.lists[0].entries[0].weight == 4 && back.lists[0].entries[1].weight == 1);
	CHECK(ae_playlists_migrate(&back, "# playlist: NONE\n") == 0 && back.count == 0 && back.in_use == -1);
	CHECK(ae_playlists_migrate(&back, NULL) == 0 && back.count == 0);
	/* (the migrated set written is a one-section file that reads back the same) */
	ae_playlists_migrate(&back, "weld:file=AE CTF\n");
	CHECK(!strcmp(back.lists[0].name, "MY PLAYLIST"));

	/* 3. names */
	fill(&set);
	CHECK(ae_playlists_name_check(&set, "  my list ", -1, name, sizeof(name)) == AE_PLAYLISTS_NAME_OK && !strcmp(name, "MY LIST"));
	CHECK(ae_playlists_name_check(&set, "", -1, name, sizeof(name)) == AE_PLAYLISTS_NAME_EMPTY);
	CHECK(ae_playlists_name_check(&set, "   ", -1, name, sizeof(name)) == AE_PLAYLISTS_NAME_EMPTY);
	CHECK(ae_playlists_name_check(&set, "ABCDEFGHIJKLMNOPQRSTUVWXY", -1, name, sizeof(name)) == AE_PLAYLISTS_NAME_LONG);
	CHECK(ae_playlists_name_check(&set, "ABCDEFGHIJKLMNOPQRSTUVWX", -1, name, sizeof(name)) == AE_PLAYLISTS_NAME_OK);
	CHECK(ae_playlists_name_check(&set, "a/b", -1, name, sizeof(name)) == AE_PLAYLISTS_NAME_CHARACTER);
	CHECK(ae_playlists_name_check(&set, "Tom's - A&B 2", -1, name, sizeof(name)) == AE_PLAYLISTS_NAME_OK);
	CHECK(ae_playlists_name_check(&set, "off", -1, name, sizeof(name)) == AE_PLAYLISTS_NAME_OFF);
	CHECK(ae_playlists_name_check(&set, "ffa night", -1, name, sizeof(name)) == AE_PLAYLISTS_NAME_TAKEN && !strcmp(name, "FFA NIGHT"));
	ae_playlists_name_message(AE_PLAYLISTS_NAME_TAKEN, name, message, sizeof(message));
	CHECK(!strcmp(message, "A playlist is already called FFA NIGHT."));
	/* (a rename to its own name in another case is allowed, to another playlist's is not) */
	CHECK(ae_playlists_rename(&set, 1, "ffa night") == AE_PLAYLISTS_NAME_OK && !strcmp(set.lists[1].name, "FFA NIGHT"));
	CHECK(ae_playlists_rename(&set, 1, "Ffa Friday") == AE_PLAYLISTS_NAME_OK && !strcmp(set.lists[1].name, "FFA FRIDAY"));
	CHECK(ae_playlists_rename(&set, 1, "swat time") == AE_PLAYLISTS_NAME_TAKEN && !strcmp(set.lists[1].name, "FFA FRIDAY"));
	CHECK(ae_playlists_new(&set, "swat time", &reason) == -1 && reason == AE_PLAYLISTS_NAME_TAKEN);
	/* (the default name: the first free number from the count + 1) */
	ae_playlists_default_name(&set, name, sizeof(name));
	CHECK(!strcmp(name, "PLAYLIST 4"));
	ae_playlists_new(&set, "PLAYLIST 4", NULL);
	ae_playlists_default_name(&set, name, sizeof(name));
	CHECK(!strcmp(name, "PLAYLIST 5"));
	/* (duplicate: "<NAME> 2", then 3; a long name is cut to fit) */
	index = ae_playlists_duplicate(&set, 0);
	CHECK(index == 4 && !strcmp(set.lists[index].name, "4V4 ARENA 2") && set.lists[index].count == 3 &&
		set.lists[index].order == AE_PLAYLIST_ORDER_SHUFFLE);
	index = ae_playlists_duplicate(&set, 0);
	CHECK(index == 5 && !strcmp(set.lists[index].name, "4V4 ARENA 3"));
	ae_playlists_init(&back);
	ae_playlists_new(&back, "ABCDEFGHIJKLMNOPQRSTUVWX", NULL);
	index = ae_playlists_duplicate(&back, 0);
	CHECK(index == 1 && strlen(back.lists[1].name) <= AE_PLAYLISTS_NAME_MAX && strstr(back.lists[1].name, " 2") &&
		ae_playlists_find(&back, back.lists[1].name) == 1);
	/* (sixteen at most) */
	ae_playlists_init(&back);
	for (index = 0; index < AE_PLAYLISTS_MAX; index++)
	{
		snprintf(name, sizeof(name), "L%d", index);
		CHECK(ae_playlists_new(&back, name, NULL) == index);
	}
	CHECK(ae_playlists_new(&back, "ONE MORE", NULL) == -1 && ae_playlists_duplicate(&back, 0) == -1);

	/* 4. delete and use */
	fill(&set);
	CHECK(ae_playlists_use(&set, 2) && set.in_use == 2 && !ae_playlists_use(&set, 2));
	ae_playlists_delete(&set, 0);
	CHECK(set.count == 2 && set.in_use == 1 && !strcmp(set.lists[0].name, "FFA NIGHT"));
	ae_playlists_delete(&set, 1);
	CHECK(set.count == 1 && set.in_use == -1);
	ae_playlists_use(&set, 0);
	ae_playlists_delete(&set, 0);
	CHECK(set.count == 0 && set.in_use == -1);
	ae_playlists_delete(&set, 0);
	CHECK(set.count == 0 && !ae_playlists_use(&set, 0) && ae_playlists_use(&set, -1) == 0);
	fill(&set);
	CHECK(ae_playlists_use(&set, -1) && set.in_use == -1);
	/* (an empty playlist is dropped) */
	index = ae_playlists_new(&set, "NOTHING", NULL);
	CHECK(ae_playlists_drop_empty(&set, index) && ae_playlists_find(&set, "NOTHING") < 0 && !ae_playlists_drop_empty(&set, 0));

	/* 5. IN TURN: the next of game k is k + 1, after the last the first; a game this PC lacks is skipped */
	{
		struct ae_playlist list;
		struct ae_playlist_run run;
		unsigned char playable[4] = { 1, 1, 1, 1 };
		int got[8];

		ae_playlist_init(&list);
		list.order = AE_PLAYLIST_ORDER_IN_TURN;
		ae_playlist_add(&list, "a", "G1");
		ae_playlist_add(&list, "b", "G2");
		ae_playlist_add(&list, "c", "G3");
		ae_playlist_add(&list, "d", "G4");
		ae_playlist_run_reset(&run);
		CHECK(ae_playlist_run_advance(&list, &run, 0, NULL, 0, 1) == 0);
		CHECK(ae_playlist_run_advance(&list, &run, 0, NULL, 1, 1) == 1 && run.previous == 0);
		CHECK(ae_playlist_run_advance(&list, &run, 0, NULL, 1, 1) == 2);
		CHECK(ae_playlist_run_advance(&list, &run, 0, NULL, 1, 1) == 3);
		CHECK(ae_playlist_run_advance(&list, &run, 0, NULL, 1, 1) == 0 && run.previous == 3);
		/* (the saved place begins it) */
		ae_playlist_run_reset(&run);
		CHECK(ae_playlist_run_advance(&list, &run, 2, NULL, 0, 1) == 2);
		/* (a game missing here is skipped, the lobby's own check) */
		playable[3] = 0;
		CHECK(ae_playlist_run_advance(&list, &run, 0, playable, 1, 1) == 0);
		playable[1] = 0;
		CHECK(ae_playlist_run_advance(&list, &run, 0, playable, 1, 1) == 2);
		/* (every game missing: no pick, the run as it was) */
		memset(playable, 0, sizeof(playable));
		CHECK(ae_playlist_run_advance(&list, &run, 0, playable, 1, 1) == -1 && run.current == 2);
		/* (one game only: it again) */
		ae_playlist_init(&list);
		list.order = AE_PLAYLIST_ORDER_IN_TURN;
		ae_playlist_add(&list, "a", "G1");
		ae_playlist_run_reset(&run);
		CHECK(ae_playlist_run_advance(&list, &run, 0, NULL, 0, 1) == 0 && ae_playlist_run_advance(&list, &run, 0, NULL, 1, 1) == 0);
		(void)got;
	}

	/* 6. SHUFFLE: every game once a pass, never twice in a row, the pass boundary included */
	{
		struct ae_playlist list;
		struct ae_playlist_run run;
		int seed, game, seen[6], last = -1;

		ae_playlist_init(&list);
		list.order = AE_PLAYLIST_ORDER_SHUFFLE;
		ae_playlist_add(&list, "a", "G1");
		ae_playlist_add(&list, "b", "G2");
		ae_playlist_add(&list, "c", "G1");
		ae_playlist_add(&list, "d", "G3");
		ae_playlist_add(&list, "e", "G2");
		ae_playlist_add(&list, "f", "G3");
		list.entries[0].weight = 5;	/* (weights do not count here) */
		for (seed = 1; seed < 60; seed++)
		{
			ae_playlist_run_reset(&run);
			last = -1;
			for (pass = 0; pass < 5; pass++)
			{
				memset(seen, 0, sizeof(seen));
				for (game = 0; game < 6; game++)
				{
					int got = ae_playlist_run_advance(&list, &run, 0, NULL, game + pass > 0, (unsigned long)seed * 131ul + (unsigned long)(pass * 6 + game));

					CHECK(got >= 0 && got < 6);
					if (got < 0 || got >= 6)
						return 1;
					seen[got]++;
					CHECK(got != last);
					last = got;
				}
				for (game = 0; game < 6; game++)
					CHECK(seen[game] == 1);
			}
		}
		/* (one game only = it again; two games alternate) */
		ae_playlist_init(&list);
		list.order = AE_PLAYLIST_ORDER_SHUFFLE;
		ae_playlist_add(&list, "a", "G1");
		ae_playlist_run_reset(&run);
		CHECK(ae_playlist_run_advance(&list, &run, 0, NULL, 0, 5) == 0 && ae_playlist_run_advance(&list, &run, 0, NULL, 1, 6) == 0 &&
			ae_playlist_run_advance(&list, &run, 0, NULL, 1, 7) == 0);
		ae_playlist_add(&list, "b", "G2");
		ae_playlist_run_reset(&run);
		last = ae_playlist_run_advance(&list, &run, 0, NULL, 0, 9);
		for (game = 0; game < 20; game++)
		{
			int got = ae_playlist_run_advance(&list, &run, 0, NULL, 1, (unsigned long)(10 + game));

			CHECK(got >= 0 && got != last);
			last = got;
		}
		/* (a game this PC lacks is left out of the pass; every playable one still comes once) */
		{
			unsigned char playable[6] = { 1, 1, 1, 1, 1, 1 };

			ae_playlist_init(&list);
			list.order = AE_PLAYLIST_ORDER_SHUFFLE;
			ae_playlist_add(&list, "a", "G1");
			ae_playlist_add(&list, "b", "G2");
			ae_playlist_add(&list, "c", "G3");
			playable[1] = 0;
			ae_playlist_run_reset(&run);
			memset(seen, 0, sizeof(seen));
			for (game = 0; game < 6; game++)
				seen[ae_playlist_run_advance(&list, &run, 0, playable, game > 0, (unsigned long)(30 + game))]++;
			CHECK(seen[1] == 0 && seen[0] == 3 && seen[2] == 3);
		}
		/* (a game taken out or added makes a new pass) */
		ae_playlist_init(&list);
		list.order = AE_PLAYLIST_ORDER_SHUFFLE;
		ae_playlist_add(&list, "a", "G1");
		ae_playlist_add(&list, "b", "G2");
		ae_playlist_add(&list, "c", "G3");
		ae_playlist_run_reset(&run);
		ae_playlist_run_advance(&list, &run, 0, NULL, 0, 3);
		ae_playlist_add(&list, "d", "G4");
		memset(seen, 0, sizeof(seen));
		for (game = 0; game < 4; game++)
			seen[ae_playlist_run_advance(&list, &run, 0, NULL, 1, (unsigned long)(40 + game))]++;
		CHECK(seen[0] + seen[1] + seen[2] + seen[3] == 4);
	}

	/* 7. RANDOM: the weights, and NO REPEATS */
	{
		struct ae_playlist list;
		struct ae_playlist_run run;
		int tally[3] = { 0, 0, 0 }, draw, got, last;

		ae_playlist_init(&list);
		list.order = AE_PLAYLIST_ORDER_RANDOM;
		list.no_repeats = 0;
		ae_playlist_add(&list, "a", "G1");
		ae_playlist_add(&list, "b", "G2");
		ae_playlist_add(&list, "c", "G3");
		list.entries[0].weight = 5;
		list.entries[1].weight = 1;
		list.entries[2].weight = 1;
		ae_playlist_run_reset(&run);
		for (draw = 0; draw < 700; draw++)
		{
			got = ae_playlist_run_advance(&list, &run, 0, NULL, 1, (unsigned long)(draw * 7 + 3));
			CHECK(got >= 0 && got < 3);
			tally[got]++;
		}
		CHECK(tally[0] > tally[1] * 2 && tally[0] > tally[2] * 2);
		/* (NO REPEATS: never the one just played) */
		list.no_repeats = 1;
		ae_playlist_run_reset(&run);
		last = ae_playlist_run_advance(&list, &run, 0, NULL, 0, 1);
		for (draw = 0; draw < 300; draw++)
		{
			got = ae_playlist_run_advance(&list, &run, 0, NULL, 1, (unsigned long)(draw * 13 + 5));
			CHECK(got != last);
			last = got;
		}
		/* (a game this PC lacks is never drawn; a single playable game is drawn again whatever NO REPEATS says) */
		{
			unsigned char playable[3] = { 0, 1, 0 };

			ae_playlist_run_reset(&run);
			for (draw = 0; draw < 20; draw++)
				CHECK(ae_playlist_run_advance(&list, &run, 0, playable, draw > 0, (unsigned long)(draw + 2)) == 1);
		}
		/* (the old pick's own cases) */
		list.no_repeats = 1;
		CHECK(ae_playlist_pick(&list, 0, 0) != 0);
	}

	/* 8. UP NEXT and PLAYED follow the order; the indexes follow an edit */
	{
		struct ae_playlist list;
		struct ae_playlist_run run;

		ae_playlist_init(&list);
		list.order = AE_PLAYLIST_ORDER_IN_TURN;
		ae_playlist_add(&list, "a", "G1");
		ae_playlist_add(&list, "b", "G2");
		ae_playlist_add(&list, "c", "G3");
		ae_playlist_run_reset(&run);
		CHECK(ae_playlist_run_tag(&run, 0) == AE_PLAYLIST_TAG_NONE);
		ae_playlist_run_advance(&list, &run, 0, NULL, 0, 1);
		CHECK(ae_playlist_run_tag(&run, 0) == AE_PLAYLIST_TAG_UP_NEXT && ae_playlist_run_tag(&run, 1) == AE_PLAYLIST_TAG_NONE);
		ae_playlist_run_advance(&list, &run, 0, NULL, 1, 1);
		CHECK(ae_playlist_run_tag(&run, 0) == AE_PLAYLIST_TAG_PLAYED && ae_playlist_run_tag(&run, 1) == AE_PLAYLIST_TAG_UP_NEXT);
		/* (game 3 dragged to the top: both tags move with their games) */
		ae_playlist_move(&list, 2, 0);
		ae_playlist_run_moved(&run, 2, 0);
		CHECK(ae_playlist_run_tag(&run, 1) == AE_PLAYLIST_TAG_PLAYED && ae_playlist_run_tag(&run, 2) == AE_PLAYLIST_TAG_UP_NEXT);
		/* (the up next game taken out: no up next; a game before it taken out shifts it) */
		ae_playlist_remove(&list, 0);
		ae_playlist_run_removed(&run, 0);
		CHECK(ae_playlist_run_tag(&run, 0) == AE_PLAYLIST_TAG_PLAYED && ae_playlist_run_tag(&run, 1) == AE_PLAYLIST_TAG_UP_NEXT);
		ae_playlist_remove(&list, 1);
		ae_playlist_run_removed(&run, 1);
		CHECK(run.current == -1 && ae_playlist_run_tag(&run, 0) == AE_PLAYLIST_TAG_PLAYED);
		CHECK(ae_playlist_run_tag(&run, -1) == AE_PLAYLIST_TAG_NONE);
	}

	/* 9. HOST A SERVER's rotation text */
	{
		struct ae_playlist list;
		struct ae_playlist_host host;
		char line[512];

		ae_playlist_init(&list);
		ae_playlist_add(&list, "damnation", "AE TEAM SLY");
		ae_playlist_add(&list, "prisoner", "AE SLAYER");
		list.entries[0].weight = 4;
		list.map_vote = 1;
		list.no_repeats = 1;
		list.order = AE_PLAYLIST_ORDER_RANDOM;
		CHECK(ae_playlist_host_rotation(&list, line, sizeof(line), &host) == 2);
		CHECK(!strcmp(line, "damnation:file=AE TEAM SLY +weight=4, prisoner:file=AE SLAYER +weight=2"));
		CHECK(host.shuffle == 1 && host.map_vote == 1 && host.map_vote_choices == 3 && host.vote_no_repeat == 2);
		list.order = AE_PLAYLIST_ORDER_SHUFFLE;
		list.map_vote = 0;
		list.no_repeats = 0;
		CHECK(ae_playlist_host_rotation(&list, line, sizeof(line), &host) == 2);
		CHECK(!strcmp(line, "damnation:file=AE TEAM SLY, prisoner:file=AE SLAYER"));
		CHECK(host.shuffle == 1 && host.map_vote == 0 && host.vote_no_repeat == 0);
		list.order = AE_PLAYLIST_ORDER_IN_TURN;
		CHECK(ae_playlist_host_rotation(&list, line, sizeof(line), &host) == 2 && host.shuffle == 0 && !strstr(line, "weight"));
		/* (a name a rotation cannot hold is left out; a line too small is refused) */
		ae_playlist_add(&list, "bad,map", "AE CTF");
		CHECK(ae_playlist_host_rotation(&list, line, sizeof(line), &host) == 2);
		CHECK(ae_playlist_host_rotation(&list, line, 20, &host) == -1);
		list.count = 0;
		CHECK(ae_playlist_host_rotation(&list, line, sizeof(line), &host) == 0 && !line[0]);
	}

	/* the lobby row's and the board's words */
	{
		struct ae_playlist list;
		char words[64];

		ae_playlist_init(&list);
		strcpy(list.name, "4V4 ARENA");
		list.order = AE_PLAYLIST_ORDER_SHUFFLE;
		ae_playlist_row_text(&list, words, sizeof(words));
		CHECK(!strcmp(words, "4V4 ARENA  \xC2\xB7  SHUFFLE"));
		ae_playlist_add(&list, "a", "G1");
		ae_playlist_board_text(&list, words, sizeof(words));
		CHECK(!strcmp(words, "1 GAME  \xC2\xB7  SHUFFLE"));
		ae_playlist_add(&list, "b", "G1");
		list.order = AE_PLAYLIST_ORDER_RANDOM;
		ae_playlist_board_text(&list, words, sizeof(words));
		CHECK(!strcmp(words, "2 GAMES  \xC2\xB7  RANDOM"));
		list.order = AE_PLAYLIST_ORDER_SHUFFLE;
		ae_playlist_host_summary(&list, words, sizeof(words));
		CHECK(!strcmp(words, "Playlist: 4V4 ARENA, 2 games, shuffled"));
		list.order = AE_PLAYLIST_ORDER_IN_TURN;
		list.count = 1;
		ae_playlist_host_summary(&list, words, sizeof(words));
		CHECK(!strcmp(words, "Playlist: 4V4 ARENA, 1 game, in turn"));
	}

	if (failures)
		printf("%d FAILED\n", failures);
	else
		printf("ae_playlists_test: all passed\n");
	return failures ? 1 : 0;
}
