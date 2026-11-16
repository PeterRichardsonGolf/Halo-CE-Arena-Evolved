/* ae_playlist_test.c: a hosted playlist (ae_playlist.h): the entries, the weights and chances, the order, the picks,
the vote with SAME AGAIN, and the rotation file */

#include <stdio.h>
#include <string.h>

#include "ae_playlist.h"

static int failures;
#define CHECK(condition) do { if (!(condition)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
	failures++; } } while (0)

static struct ae_playlist playlist, back;
static char text[AE_PLAYLIST_TEXT];

int main(void)
{
	int choices[AE_PLAYLIST_VOTE_CHOICES + 2], count, index, seen[8], draw;

	ae_playlist_init(&playlist);
	CHECK(playlist.count == 0 && playlist.map_vote && playlist.no_repeats);
	CHECK(ae_playlist_add(&playlist, "", "AE TEAM SLY") < 0 && ae_playlist_add(&playlist, "sali_creek", "") < 0);
	CHECK(ae_playlist_add(&playlist, "sali_creek", "AE TEAM SLY") == 0);
	CHECK(ae_playlist_add(&playlist, "sanctuary", "AE KING") == 1);
	CHECK(ae_playlist_add(&playlist, "weld", "AE CTF") == 2);
	CHECK(ae_playlist_add(&playlist, "turf", "AE TEAM SLY") == 3);
	CHECK(playlist.entries[0].weight == 2);

	/* the weights step between 1 and 5 and stop there */
	CHECK(ae_playlist_weight_step(&playlist, 0, 1) && playlist.entries[0].weight == 3);
	CHECK(ae_playlist_weight_step(&playlist, 0, 1) && ae_playlist_weight_step(&playlist, 0, 1));
	CHECK(playlist.entries[0].weight == 5 && !ae_playlist_weight_step(&playlist, 0, 1));
	CHECK(ae_playlist_weight_step(&playlist, 1, -1) && playlist.entries[1].weight == 1 &&
		!ae_playlist_weight_step(&playlist, 1, -1));
	CHECK(!ae_playlist_weight_step(&playlist, 9, 1) && !ae_playlist_weight_step(&playlist, -1, 1));
	/* 5 + 1 + 2 + 2 = 10: the chances */
	CHECK(ae_playlist_chance(&playlist, 0) == 50 && ae_playlist_chance(&playlist, 1) == 10);
	CHECK(ae_playlist_chance(&playlist, 2) == 20 && ae_playlist_chance(&playlist, 7) == 0);

	/* the order: an entry dragged to another place, the ones between sliding over */
	CHECK(ae_playlist_move(&playlist, 0, 2));
	CHECK(!strcmp(playlist.entries[0].map, "sanctuary") && !strcmp(playlist.entries[1].map, "weld") &&
		!strcmp(playlist.entries[2].map, "sali_creek") && !strcmp(playlist.entries[3].map, "turf"));
	CHECK(playlist.entries[2].weight == 5);
	CHECK(ae_playlist_move(&playlist, 3, 0) && !strcmp(playlist.entries[0].map, "turf") &&
		!strcmp(playlist.entries[1].map, "sanctuary"));
	CHECK(!ae_playlist_move(&playlist, 1, 1) && !ae_playlist_move(&playlist, 1, 9) && !ae_playlist_move(&playlist, -1, 0));
	ae_playlist_move(&playlist, 0, 3);
	ae_playlist_move(&playlist, 0, 2);
	CHECK(playlist.count == 4);

	/* the pick: by weight, never the one just played */
	for (index = 0; index < 8; index++)
		seen[index] = 0;
	for (draw = 0; draw < 200; draw++)
	{
		int picked = ae_playlist_pick(&playlist, 1, (unsigned long)draw * 7ul + 3ul);

		CHECK(picked >= 0 && picked < 4 && picked != 1);
		seen[picked]++;
	}
	CHECK(seen[0] && seen[2] && seen[3]);
	/* (no repeats off: the last may come again; one entry alone repeats) */
	playlist.no_repeats = 0;
	seen[1] = 0;
	for (draw = 0; draw < 200; draw++)
		if (ae_playlist_pick(&playlist, 1, (unsigned long)draw) == 1)
			seen[1]++;
	CHECK(seen[1] > 0);
	playlist.no_repeats = 1;
	CHECK(ae_playlist_pick(&back, -1, 5) == -1);
	ae_playlist_init(&back);
	ae_playlist_add(&back, "a", "b");
	CHECK(ae_playlist_pick(&back, 0, 5) == 0);

	/* the vote: up to three other games, each once, then SAME AGAIN the last, whatever the draw */
	for (draw = 0; draw < 50; draw++)
	{
		count = ae_playlist_vote_choices(&playlist, 2, (unsigned long)draw * 13ul + 1ul, choices);
		CHECK(count == 4);
		CHECK(choices[3] == 2 && ae_playlist_vote_is_same_again(count, 3) && !ae_playlist_vote_is_same_again(count, 2));
		CHECK(choices[0] != 2 && choices[1] != 2 && choices[2] != 2);
		CHECK(choices[0] != choices[1] && choices[1] != choices[2] && choices[0] != choices[2]);
	}
	/* (two games: one other and SAME AGAIN; one game, no vote; MAP VOTE off, no vote) */
	ae_playlist_remove(&playlist, 3);
	ae_playlist_remove(&playlist, 0);
	CHECK(playlist.count == 2);
	count = ae_playlist_vote_choices(&playlist, 0, 4, choices);
	CHECK(count == 2 && choices[0] == 1 && choices[1] == 0 && ae_playlist_vote_is_same_again(count, 1));
	playlist.map_vote = 0;
	CHECK(ae_playlist_vote_choices(&playlist, 0, 4, choices) == 0);
	playlist.map_vote = 1;
	CHECK(ae_playlist_vote_choices(&back, 0, 4, choices) == 0);
	CHECK(ae_playlist_vote_choices(&playlist, 5, 4, choices) == 0);

	/* the file: a server's rotation, the rest in comments; read back the same */
	ae_playlist_init(&playlist);
	snprintf(playlist.name, sizeof(playlist.name), "4V4 ARENA");
	ae_playlist_add(&playlist, "sali_creek", "AE TEAM SLY");
	ae_playlist_add(&playlist, "sanctuary", "AE KING");
	playlist.entries[0].weight = 3;
	playlist.map_vote = 0;
	playlist.no_repeats = 1;
	CHECK(ae_playlist_write(&playlist, text, sizeof(text)) > 0);
	CHECK(strstr(text, "sali_creek:file=AE TEAM SLY  # weight 3\n") != NULL);
	CHECK(strstr(text, "# playlist: 4V4 ARENA\n") == text);
	CHECK(strstr(text, "# map vote: 0\n") != NULL && strstr(text, "# no repeats: 1\n") != NULL);
	{ char small[20]; CHECK(ae_playlist_write(&playlist, small, sizeof(small)) == -1); }
	ae_playlist_init(&back);
	CHECK(ae_playlist_read(&back, text) == 2);
	CHECK(!strcmp(back.name, "4V4 ARENA") && back.map_vote == 0 && back.no_repeats == 1 && back.count == 2);
	CHECK(!strcmp(back.entries[0].map, "sali_creek") && !strcmp(back.entries[0].gametype, "AE TEAM SLY") &&
		back.entries[0].weight == 3 && back.entries[1].weight == 2);
	/* a rotation a server owner wrote: no header, bare gametypes, a missing or wild weight, blank and broken lines */
	ae_playlist_init(&back);
	CHECK(ae_playlist_read(&back, "\n bloodgulch:ctf \nbogus line\nhangemhigh:slayer # weight 9\nprisoner:slayer # weight 0\n"
		":ctf\nmap:\n") == 3);
	CHECK(!strcmp(back.entries[0].gametype, "ctf") && back.entries[0].weight == 2);
	CHECK(back.entries[1].weight == AE_PLAYLIST_WEIGHT_MAX && back.entries[2].weight == 1);
	CHECK(!strcmp(back.name, "MY PLAYLIST"));
	/* more entries than a rotation holds: the rest dropped */
	{
		static char big[AE_PLAYLIST_TEXT * 4];
		int at, used = 0;

		for (at = 0; at < AE_PLAYLIST_ENTRIES + 5; at++)
			used += snprintf(big + used, sizeof(big) - (size_t)used, "m%d:g\n", at);
		CHECK(ae_playlist_read(&back, big) == AE_PLAYLIST_ENTRIES && back.count == AE_PLAYLIST_ENTRIES);
	}

	if (failures)
		return 1;
	printf("ae_playlist_test: ok\n");
	return 0;
}
