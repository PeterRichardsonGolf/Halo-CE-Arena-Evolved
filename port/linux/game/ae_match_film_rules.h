/*
AE_MATCH_FILM_RULES.H

The match film's pure rules (ae_match_film.h writes the files): the setting's values, a film's file name and which
name is one, and the removal of the oldest films once the folder is over its budget. No engine headers.
*/

#ifndef AE_MATCH_FILM_RULES_H
#define AE_MATCH_FILM_RULES_H

#include <stddef.h>

/* debug.match_film */
enum ae_match_film_mode
{
	AE_MATCH_FILM_OFF = 0,
	/* a game is recorded once a person (not a bot) is in it */
	AE_MATCH_FILM_PEOPLE,
	/* every game, a game of bots only too */
	AE_MATCH_FILM_ALL
};

/* every this many ticks a living player's pos line: 3 is 10 Hz */
#define AE_MATCH_FILM_POS_TICKS 3

/* the setting's text: people, all (on is all), off; empty or anything else is off */
int ae_match_film_rules_mode(char const *text);

/* 1: name is a film's file name, film-<8 digits>-<6 digits>-<what follows>.txt (the map and the engine, at least one
character); 0: anything else */
int ae_match_film_name_is_film(char const *name);

/* a film's file name for a time stamp (YYYYmmdd-HHMMSS), the map, the engine and a word that tells two films of one second
apart (the process id, and a count when the name is taken): film-<stamp>-<map>-<engine>-<unique>.txt. The words keep
letters, digits, _ and -, every other character becomes _; false (0) when it does not fit */
int ae_match_film_file_name(char *out, size_t size, char const *stamp, char const *map, char const *engine, char const *unique);

/* the order of two film names, oldest first: by the stamp in the name, then by the whole name (qsort's compare) */
int ae_match_film_name_compare(char const *a, char const *b);

/* the budget in megabytes as the setting asks, at least AE_MATCH_FILM_BUDGET_MINIMUM */
#define AE_MATCH_FILM_BUDGET_MINIMUM 100
long ae_match_film_budget_mb(long asked);

/* of count films (their sizes in bytes, the oldest first), the ones to remove so that the rest are within budget_bytes:
the oldest first, never the one at current_index (the film being written; -1 for none). Their indices in out (room for
count); how many */
int ae_match_film_pick_removals(long long const *sizes, int count, long long budget_bytes, int current_index, int *out);

#endif
