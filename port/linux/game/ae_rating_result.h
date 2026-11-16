/*
AE_RATING_RESULT.H

The result of one rated game (0.7.7), as a stable, versioned record: everything the rating update consumes, so that a game's
change can be computed again from the record alone (by this PC now, by a service that holds signed results later). Pure code
(no engine headers): port/linux/tests/ae_rating_result_test.c runs it.

For each player: a key (his name until the player key exists), the level he had BEFORE the game, his placement (0 = first,
-1 none), team, party, whether he left (quit), in which order (0 = first to leave), whether he rejoined before the end (a
rejoined player is not a quit), and whether he is rated (a bot, a guest or a player with no level is not). The record also holds
the playlist, the length of the game and the game time of the first quit.

Text, one small file or message, lines in this order:

	# AE rating result
	version = 1
	playlist = team_slayer
	duration = 612000
	first_quit = -1
	player = goat | 12 0 0 3 0 -1 0 1
	player = tiger | 11 1 1 -1 1 0 0 1

player: key | level place team party quit quit_order rejoined rated. A reader skips lines of a kind it does not know and fields
past the ones it knows, and a version past its own sets newer; a file with no header, or a line that is not "key = value", is
refused. Keys are written with "|" and control characters turned to "?".
*/

#ifndef __AE_RATING_RESULT_H
#define __AE_RATING_RESULT_H

#include <stddef.h>

#include "ae_rating_rules.h"

enum
{
	AE_RATING_RESULT_VERSION = 1,
	AE_RATING_RESULT_PLAYERS = AE_RATING_BALANCE_PLAYERS,
	AE_RATING_RESULT_KEY = 48,
	AE_RATING_RESULT_PLAYLIST = 32,
	AE_RATING_RESULT_TEXT = 4096
};

struct ae_rating_result_player
{
	char key[AE_RATING_RESULT_KEY];
	int level;          /* before the game */
	int place;          /* -1 none */
	int team;
	int party;          /* -1 alone */
	int quit;
	int quit_order;     /* 0 = first to leave; -1 none */
	int rejoined;
	int rated;
};

struct ae_rating_result
{
	int newer;
	char playlist[AE_RATING_RESULT_PLAYLIST];
	long duration_ms;
	long first_quit_ms;     /* -1 for none */
	int count;
	struct ae_rating_result_player player[AE_RATING_RESULT_PLAYERS];
};

void ae_rating_result_init(struct ae_rating_result *result);
/* the entries the update takes (a rejoined player is no quit); count of them */
int ae_rating_result_entries(struct ae_rating_result const *result, struct ae_rating_entry *entries, int maximum);
/* the points change of player `self`, with quit protection, from the record alone */
long ae_rating_result_change(struct ae_rating_result const *result, int self, long points);
/* whether his quit counter goes up */
int ae_rating_result_counts_quit(struct ae_rating_result const *result, int self);
/* the player of this key, -1 for none */
int ae_rating_result_find(struct ae_rating_result const *result, char const *key);

/* the text into buffer: its length, -1 when it does not fit */
long ae_rating_result_write(struct ae_rating_result const *result, char *buffer, long size);
/* a text read (the record made empty first): 1 taken, 0 refused */
int ae_rating_result_read(struct ae_rating_result *result, char const *text);

#endif
