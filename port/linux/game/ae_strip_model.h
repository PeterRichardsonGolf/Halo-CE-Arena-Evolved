/*
AE_STRIP_MODEL.H

The score strip (HUD STYLE AE, notes/specs/2026-10-11-score-strip.md 11): what
it says in every mode, made from a snapshot of the engine. Pure code (no engine
calls): ae_strip_engine.c fills the snapshot, ae_score_strip.c draws the model,
port/linux/tests/ae_strip_model_test.c checks it.

Left is always my team (team games) or me (FFA); right is the enemy team, or
the leader (the second place when I lead). Colours are roles (enum
ae_strip_color), which the renderer turns into the strip's tokens.

Strings are wide (the game's wchar_t is 16-bit, the tests' 32-bit): this file
never calls the C library's wide string functions.
*/

#ifndef __AE_STRIP_MODEL_H
#define __AE_STRIP_MODEL_H

#include <wchar.h>

#include "ae_pure_types.h"

enum ae_strip_mode { _ae_strip_slayer, _ae_strip_ctf, _ae_strip_oddball, _ae_strip_king, _ae_strip_race,
	_ae_strip_rally, _ae_strip_juggernaut, _ae_strip_accumulation, _ae_strip_elimination, _ae_strip_phantoms,
	_ae_strip_rounds };
enum ae_strip_color { _ae_strip_color_white, _ae_strip_color_grey, _ae_strip_color_team_red,
	_ae_strip_color_team_blue, _ae_strip_color_me, _ae_strip_color_clock };
enum ae_strip_flag_state { _ae_flag_home, _ae_flag_carried, _ae_flag_dropped,
	_ae_flag_none /* (addition) the team has no flag in play: no flag line */ };
enum ae_strip_hill_state { _ae_hill_empty, _ae_hill_yours, _ae_hill_contested, _ae_hill_theirs };
/* whose the context line's player is (ball holder, juggernaut, IT, target): none or unknown reads as the other
side's (white in FFA) */
enum ae_strip_side { _ae_strip_side_none, _ae_strip_side_me, _ae_strip_side_other };
/* (0.3.9) rounds on Bomb with AE's rules: the viewer's side's job this round (sides swap) */
enum ae_strip_bomb_role { _ae_bomb_role_none, _ae_bomb_role_attack, _ae_bomb_role_defend };

#define AE_STRIP_NAME_LENGTH 16
#define AE_STRIP_TEXT_LENGTH 48

struct ae_strip_side_snapshot {
	long score;                 /* points, or seconds for timed modes; race: the lap the side is on (1-based);
	                               rally: points */
	long team_index;            /* 0 red, 1 blue; NONE in FFA */
	wchar_t name[AE_STRIP_NAME_LENGTH]; /* FFA: the player's name (YOU side: the viewer's) */
	long place;                 /* 1-based place (FFA/race) */
	long players, alive;        /* rounds/elimination: players on the side, alive now */
	long rounds_won;
	enum ae_strip_flag_state flag; wchar_t flag_carrier[AE_STRIP_NAME_LENGTH]; long flag_return_seconds; /* CTF:
	                               this side's own flag; its carrier is on the other side */
};
struct ae_strip_snapshot {
	enum ae_strip_mode mode;
	boolean teams;              /* team game */
	boolean timed;              /* scores are times (oddball/king/accumulation are timed whatever this says) */
	long score_to_win;          /* 0 = no limit; seconds for timed modes */
	long clock_seconds;         /* time left, or elapsed when counts_up */
	boolean clock_counts_up;
	boolean clock_hidden;       /* NHE maps: no clock cell */
	struct ae_strip_side_snapshot me, other;  /* left = my team / me; right = enemy team / leader (2nd when I lead) */
	boolean i_lead;
	wchar_t context_name[AE_STRIP_NAME_LENGTH]; /* ball holder, juggernaut, phantoms target, "IT" player; empty:
	                               none now (no context line) */
	enum ae_strip_hill_state hill; long hill_move_seconds; /* king; -1 = not moving */
	long checkpoint, checkpoints; long laps; long next_flag_meters; /* race / rally; -1 unknown */
	long lives_left, players_left, players_total;            /* elimination / endurance; 0 = not shown */
	long rounds_to_win, respawns_left;                        /* rounds (0.2.4); respawns_left -1 = not shown */
	/* (additions to the plan's snapshot, rulings in the build report) */
	enum ae_strip_side context_side; /* whose context_name is */
	long own_score;             /* team oddball: the viewer's own time held, for "YOU 0:31" */
	boolean reverse_tag;        /* accumulation mode played as Reverse Tag: "IT <name>" */
	long balls_held, balls_total; /* accumulation: "YOU HOLD 3 OF 16" */
	boolean played_shown;       /* MATCH CLOCK BOTH with a time limit: the time played over the time left */
	long played_seconds;
	/* (0.3.9) rounds on Bomb: the viewer's job ("ATTACK" / "DEFEND" over the clock), and the planted bomb ("BOMB
	PLANTED" there, in the planters' colour; the clock is then its fuse, which the reader puts in clock_seconds) */
	enum ae_strip_bomb_role bomb_role;
	boolean bomb_planted;
	boolean bomb_defusing;      /* (attackers') a defender is on the planted bomb: "BOMB DEFUSING" */
	boolean bomb_carrying;      /* (0.4.3) SEARCH AND DESTROY: the viewer carries the bomb: "YOU HAVE THE BOMB" */
	enum ae_strip_side bomb_planted_side;
};
struct ae_strip_text {
	wchar_t text[AE_STRIP_TEXT_LENGTH];
	enum ae_strip_color color;  /* the line's colour: its first character's (grey when empty) */
	/* (addition to the plan's text) each character's colour, so a line can be "label grey, value white, name in
	its team's colour" ("DROPPED 0:18", "HILL YOURS", "LAP 2 OF 3") */
	unsigned char colors[AE_STRIP_TEXT_LENGTH];
};
struct ae_strip_model {
	struct ae_strip_text left_score, right_score;     /* counter digits; times as m:ss; race: "2ND" / a name */
	struct ae_strip_text clock;                       /* m:ss / mm:ss; empty when hidden */
	boolean clock_small;                              /* timed + race modes: label-size grey clock */
	struct ae_strip_text context;                     /* line above the clock (lead "+7" lives here when no context) */
	struct ae_strip_text lead;                        /* "+7" in the leader's colour, "TIED"; empty when context used */
	struct ae_strip_text goal;                        /* "TO 50" / "TO 5:00"; empty with no limit */
	struct ae_strip_text left_sub, right_sub;         /* "YOU 3RD", "LEADER bot3", "DROPPED 0:18", "LAP 2 OF 3" */
	boolean bars; real left_fill, right_fill;         /* 0..1, outer end -> clock */
	boolean round_cells; long left_cells, left_lit, right_cells, right_lit; /* rounds: one per player, <= 4 */
	struct ae_strip_text left_alive, right_alive;     /* "6 ALIVE" when > 4 per team (per match) */
	long pips_total, left_pips_lit, right_pips_lit;   /* rounds to win */
	boolean marker_left;                              /* my-side triangle over the left score */
	/* (addition) CTF: each sub-line is a team's flag line, a flag glyph in that team's colour before its text */
	boolean flag_glyphs;
	/* (addition) MATCH CLOCK BOTH: the time played, a small grey line over the clock ("3:12"); empty otherwise */
	struct ae_strip_text played;
};
void ae_strip_model_build(const struct ae_strip_snapshot *snapshot, struct ae_strip_model *model);
void ae_strip_format_time(long seconds, wchar_t *out, long out_count);   /* "1:42", "12:05" */
void ae_strip_format_place(long place, wchar_t *out, long out_count);    /* "1ST", "2ND", "3RD", "11TH" */

#endif /* __AE_STRIP_MODEL_H */
