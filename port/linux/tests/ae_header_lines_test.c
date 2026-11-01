/* The scoreboard header's text (port/linux/game/ae_header_lines.c): spec 11.9's strings */
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include "ae_header_lines.h"

static int failures;
#define AE_CHECK_WSTR(a, b) do { if (wcscmp((a), (b)) != 0) { printf("FAIL %s:%d '%ls' != '%ls'\n", __FILE__, \
	__LINE__, (a), (b)); failures++; } } while (0)

static void input(struct ae_header_input *in, const wchar_t *display, const wchar_t *set, const char *map,
	enum ae_header_goal goal, long score, long minutes)
{
	memset(in, 0, sizeof(*in));
	in->display_name = display;
	in->set_name = set;
	in->map_path = map;
	in->goal = goal;
	in->score_to_win = score;
	in->time_limit_minutes = minutes;
}

int main(void)
{
	struct ae_header_input in;
	struct ae_header_lines out;
	wchar_t map[64];

	/* (the set moves from the name to the tag) */
	input(&in, L"TEAM AE SLAYER", L"AE", "levels\\test\\bloodgulch\\bloodgulch", _ae_goal_points, 50, 15);
	ae_scoreboard_header_lines(&in, &out);
	AE_CHECK_WSTR(out.mode, L"TEAM SLAYER"); AE_CHECK_WSTR(out.map, L"BLOOD GULCH"); AE_CHECK_WSTR(out.tag, L"AE");
	AE_CHECK_WSTR(out.goal_time, L"FIRST TO 50 \x00B7 15:00");
	/* (while the game runs: the time left) */
	in.time_left_shown = 1; in.time_left_seconds = 473;
	ae_scoreboard_header_lines(&in, &out);
	AE_CHECK_WSTR(out.goal_time, L"FIRST TO 50 \x00B7 7:53 LEFT");
	in.time_left_seconds = 0;
	ae_scoreboard_header_lines(&in, &out);
	AE_CHECK_WSTR(out.goal_time, L"FIRST TO 50 \x00B7 0:00 LEFT");
	in.clock_counts_up = 1;
	ae_scoreboard_header_lines(&in, &out);
	AE_CHECK_WSTR(out.goal_time, L"FIRST TO 50 \x00B7 CLOCK COUNTS UP");
	in.clock_counts_up = 0; in.time_left_shown = 0;
	input(&in, L"FFA AE COMP SLAYER", L"AE COMP", "bloodgulch", _ae_goal_points, 25, 0);
	ae_scoreboard_header_lines(&in, &out);
	AE_CHECK_WSTR(out.mode, L"FFA SLAYER"); AE_CHECK_WSTR(out.tag, L"AE COMP");
	AE_CHECK_WSTR(out.goal_time, L"FIRST TO 25 \x00B7 NO TIME LIMIT");
	/* (a set whose names carry AE first: the Octagon drills, "FFA AE OCTAGON BR" in set OCTAGON) */
	input(&in, L"FFA AE OCTAGON BR", L"OCTAGON", "octagon_b", _ae_goal_points, 0, 0);
	ae_scoreboard_header_lines(&in, &out);
	AE_CHECK_WSTR(out.mode, L"FFA BR"); AE_CHECK_WSTR(out.tag, L"OCTAGON");
	input(&in, L"TEAM NHE CTF", L"NHE", "levels\\test\\bloodgulch\\bloodgulch", _ae_goal_captures, 3, 15);
	in.clock_counts_up = TRUE;
	ae_scoreboard_header_lines(&in, &out);
	AE_CHECK_WSTR(out.mode, L"TEAM CTF"); AE_CHECK_WSTR(out.tag, L"NHE");
	AE_CHECK_WSTR(out.goal_time, L"3 CAPTURES \x00B7 CLOCK COUNTS UP");
	/* (no goal, no time: NO LIMIT; a goal-less game with a time limit: the time) */
	input(&in, L"Slayer", L"", "prisoner", _ae_goal_points, 0, 0);
	ae_scoreboard_header_lines(&in, &out);
	AE_CHECK_WSTR(out.mode, L"Slayer"); AE_CHECK_WSTR(out.tag, L""); AE_CHECK_WSTR(out.map, L"PRISONER");
	AE_CHECK_WSTR(out.goal_time, L"NO LIMIT");
	in.time_limit_minutes = 10;
	ae_scoreboard_header_lines(&in, &out);
	AE_CHECK_WSTR(out.goal_time, L"10:00");
	/* (timed modes, race, rally, rounds) */
	input(&in, L"TEAM AE KING", L"AE", "prisoner", _ae_goal_time, 300, 15);
	ae_scoreboard_header_lines(&in, &out);
	AE_CHECK_WSTR(out.mode, L"TEAM KING"); AE_CHECK_WSTR(out.goal_time, L"FIRST TO 5:00 \x00B7 15:00");
	input(&in, L"FFA AE RACE", L"AE", "bloodgulch", _ae_goal_laps, 3, 0);
	ae_scoreboard_header_lines(&in, &out);
	AE_CHECK_WSTR(out.goal_time, L"FIRST TO 3 LAPS \x00B7 NO TIME LIMIT");
	input(&in, L"FFA AE RALLY", L"AE", "bloodgulch", _ae_goal_rally_points, 15, 0);
	ae_scoreboard_header_lines(&in, &out);
	AE_CHECK_WSTR(out.goal_time, L"FIRST TO 15 POINTS \x00B7 NO TIME LIMIT");
	input(&in, L"TEAM AE EXTERMINATION", L"AE", "bloodgulch", _ae_goal_rounds, 5, 0);
	in.round_seconds = 120;
	ae_scoreboard_header_lines(&in, &out);
	AE_CHECK_WSTR(out.goal_time, L"5 ROUNDS TO WIN \x00B7 2:00 ROUNDS");
	/* (a name whose second word is not its set: whole) */
	input(&in, L"MY LONG GAME TYPE", L"AE", "wizard", _ae_goal_points, 10, 0);
	ae_scoreboard_header_lines(&in, &out);
	AE_CHECK_WSTR(out.mode, L"MY LONG GAME TYPE"); AE_CHECK_WSTR(out.tag, L"AE"); AE_CHECK_WSTR(out.map, L"WIZARD");
	/* (map names) */
	ae_scoreboard_header_map_name("levels\\test\\putput\\putput", map, 64); AE_CHECK_WSTR(map, L"CHIRON TL34");
	ae_scoreboard_header_map_name("levels\\test\\carousel\\carousel", map, 64); AE_CHECK_WSTR(map, L"DERELICT");
	ae_scoreboard_header_map_name("homobox@ce", map, 64); AE_CHECK_WSTR(map, L"HOMOBOX");
	ae_scoreboard_header_map_name(NULL, map, 64); AE_CHECK_WSTR(map, L"");
	ae_scoreboard_header_map_name("levels\\test\\hangemhigh\\hangemhigh", map, 4); AE_CHECK_WSTR(map, L"HAN");
	printf("%s\n", failures ? "FAILED" : "ok");
	return failures != 0;
}
