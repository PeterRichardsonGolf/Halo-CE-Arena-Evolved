/*
AE_HEADER_LINES.H

The scoreboard header's text (HUD STYLE AE, spec 11.9; plan Task 8): pure,
unit-tested in port/linux/tests/ae_header_lines_test.c.

Line 1: the mode (the display name "<FFA|TEAM> <set> <mode>" with its set
taken out: "TEAM CTF") . the map ("BLOOD GULCH") - the set's tag ("NHE").
Line 2: goal . time: "FIRST TO 50 . 15:00" ("FIRST TO 50 . 7:53 LEFT" while the game runs), "3 CAPTURES . CLOCK
COUNTS UP",
"FIRST TO 25 . NO TIME LIMIT", "NO LIMIT", "5 ROUNDS TO WIN . 2:00 ROUNDS"
(the separator is a middle dot, U+00B7).
*/

#ifndef __AE_HEADER_LINES_H
#define __AE_HEADER_LINES_H

#include <wchar.h>

#include "ae_pure_types.h"

enum ae_header_goal { _ae_goal_points, _ae_goal_captures, _ae_goal_time, _ae_goal_laps, _ae_goal_rally_points,
	_ae_goal_rounds };

struct ae_header_input {
	const wchar_t *display_name;    /* "TEAM AE SLAYER", or a gametype's own name */
	const wchar_t *set_name;        /* "AE", "AE COMP", "NHE", "TRAINING", or "" */
	const char *map_path;           /* "levels\\test\\bloodgulch\\bloodgulch" or "bloodgulch" */
	const wchar_t *map_name;        /* given: the map's name (a pack's map, "ABYSS"), in place of map_path's */
	enum ae_header_goal goal;
	long score_to_win;              /* 0: none; seconds for _ae_goal_time */
	long time_limit_minutes;        /* 0: none */
	boolean clock_counts_up;        /* a time limit, but the clock counts up (MATCH CLOCK UP, NHE EXTRAS) */
	long round_seconds;             /* rounds: each round's length */
	boolean time_left_shown;        /* the game running with a time limit: its time left (owner 2026-10-20) */
	long time_left_seconds;         /* (time_left_shown) the time left, rounded up: "7:53 LEFT" for the limit */
};

#define AE_HEADER_TEXT_LENGTH 64
#define AE_HEADER_SEPARATOR L" \x00B7 "

struct ae_header_lines {
	wchar_t mode[AE_HEADER_TEXT_LENGTH];
	wchar_t map[AE_HEADER_TEXT_LENGTH];
	wchar_t tag[AE_HEADER_TEXT_LENGTH];        /* "" for none */
	wchar_t goal_time[AE_HEADER_TEXT_LENGTH];
};

void ae_scoreboard_header_lines(const struct ae_header_input *input, struct ae_header_lines *lines);
/* a map's name for the header: the stock maps' names ("BLOOD GULCH"), else the file's name in capitals */
void ae_scoreboard_header_map_name(const char *map_path, wchar_t *out, long count);

#endif /* __AE_HEADER_LINES_H */
