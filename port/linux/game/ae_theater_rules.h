/*
AE_THEATER_RULES.H

The Theater's pure rules (ae_theater_rules.c; unit test port/linux/tests/ae_theater_rules_test.c): how a recording's row reads, how long
it is, the playback's speed steps, where a skip lands and which player a button picks next. No engine header.
*/

#ifndef AE_THEATER_RULES_H
#define AE_THEATER_RULES_H

#include <stddef.h>

/* the seconds a skip of the d-pad or an arrow key moves, and the speeds the shoulder buttons step through (percent) */
#define AE_THEATER_SKIP_SECONDS 10
#define AE_THEATER_SPEEDS 5

/* seconds as "M:SS" (or "H:MM:SS" from an hour); negative counts as 0 */
void ae_theater_time_text(long seconds, char *text, size_t size);
/* a recording's file name ("20261119-071225_bloodgulch.ocreplay") as a row's title: the map in capitals (underscores as spaces,
the map's own name only) and the day and time ("BLOODGULCH  2026-11-19 07:12"); the name itself, cut to fit, when it is not one
this build made */
void ae_theater_title_text(const char *file_name, char *text, size_t size);
/* the speed one step up (direction 1) or down (-1) from percent, within 25 to 400; an unlisted speed goes to the nearest step first */
long ae_theater_speed_step(long percent, int direction);
/* the speed as "1x", "0.5x", "0.25x", "2x", "4x" */
void ae_theater_speed_text(long percent, char *text, size_t size);
/* where a skip lands: from seconds by direction * AE_THEATER_SKIP_SECONDS, held within 0 and length */
long ae_theater_skip_target(long seconds, int direction, long length);
/* the part of the recording played, 0 to 1 (0 for none) */
float ae_theater_progress(long position, long length);
/* the player a button picks after `current` (an absolute player index, -1 for none) from the ascending list of those with a unit:
the next (backwards 0) or the one before, wrapping; the first or last when current is not in the list; -1 for an empty list */
int ae_theater_next_player(const int *players, int count, int current, int backwards);

#endif
