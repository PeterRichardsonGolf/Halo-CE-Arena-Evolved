/*
AE_PGCR_SCREEN_RULES.H

The post-game report screen's decisions as pure code (port/linux/tests/ae_pgcr_screen_rules_test.c runs them): what a
report holds for the board and the pane, where the cursor goes, which medals a row's strip shows and in what order, how a
medal reads without MCC's pictures (its name, in its family's colour), and the words of the numbers. No engine headers:
ae_glue_pgcr.c fills a report from the game, ae_screen_pgcr.c draws it.

A number the report does not have is AE_PGR_UNKNOWN (-1) and reads as a dash: a host that is not Arena Evolved sends no
summaries, and an older build's summaries lack what a newer one counts.
*/

#ifndef __AE_PGCR_SCREEN_RULES_H
#define __AE_PGCR_SCREEN_RULES_H

#include <stddef.h>

#include "ae_pgcr_rules.h"
#include "ae_service_record.h"

enum
{
	AE_PGR_ROWS = 128,
	AE_PGR_TEAMS = 2,
	/* a number the report does not have */
	AE_PGR_UNKNOWN = -1,
	/* the board's medal strip: this many distinct medals, then "+N" */
	AE_PGR_STRIP_MEDALS = 5,
	/* a page of an FFA board, for LB and RB */
	AE_PGR_PAGE = 8,
	AE_PGR_OBJECTIVES = 3,
	AE_PGR_WORD = 24,
	AE_PGR_NAME = 48
};

/* the game's kind, which picks the objective numbers of the pane's STATS block */
enum ae_pgr_mode
{
	AE_PGR_MODE_SLAYER,
	AE_PGR_MODE_FLAG,
	AE_PGR_MODE_BALL,
	AE_PGR_MODE_HILL,
	AE_PGR_MODE_RACE,
	AE_PGR_MODE_BOMB,
	AE_PGR_MODE_OTHER
};

/* a medal's family: the colour of its name when there is no picture */
enum ae_pgr_family { AE_PGR_FAMILY_MULTIKILL, AE_PGR_FAMILY_SPREE, AE_PGR_FAMILY_SKILL, AE_PGR_FAMILY_OBJECTIVE };

struct ae_pgr_row
{
	int slot;                       /* the player's slot (absolute index), the summary's key */
	short team;                     /* the board's group: 0 the first team shown, 1 the second; -1 free for all */
	int me;                         /* a player of this machine */
	int quit;                       /* left before the end */
	int bot;                        /* a LAN bot's: no card, no record */
	long rank;                      /* 0 the first place (a team game: the rank in the team), the game's own order */
	int won;                        /* the winner (ae_pgr_decide_winners) */
	int rival;                      /* a rival of this machine's first player (ae_pgr_mark_rivals) */
	unsigned int chip;              /* the colour chip, 0xRRGGBBAA */
	char name[AE_PGR_NAME];
	char place[8];                  /* "1ST" .. , FFA's rank column; teams: the rank in the team */
	char score[AE_PGR_WORD];        /* the game's own score text (a time in ball and hill games) */
	long score_value;               /* the number behind it */
	long kills, deaths, assists, betrayals, suicides;
	long shots_fired, shots_hit;
	/* the objective numbers of the mode (AE_PGR_UNKNOWN for none) */
	long objective[AE_PGR_OBJECTIVES];
	/* the host's summary (valid = 0: none for this player) and its weapons' names (the summary's tags, read by the game) */
	struct ae_pgcr_summary summary;
	char weapon_name[AE_PGCR_SUMMARY_WEAPONS][AE_PGR_WORD];
};

struct ae_pgr_team
{
	int count;                      /* its rows */
	int first;                      /* the index of its first row */
	char name[16];                  /* "RED TEAM" */
	int index;                      /* the game's team number (0 red, 1 blue) */
	char score[AE_PGR_WORD];
	long score_value;
	long kills, deaths, assists;
	unsigned int color;
};

struct ae_pgr_report
{
	int valid;
	int teams;
	enum ae_pgr_mode mode;
	int rows;
	/* rows in order: team 0's rows, then team 1's (the winner first), each by rank; FFA by place */
	struct ae_pgr_row row[AE_PGR_ROWS];
	struct ae_pgr_team team[AE_PGR_TEAMS];
	/* the banner: "RED TEAM WINS", its score "50 - 45" and its colour */
	char banner[AE_PGR_NAME];
	char banner_score[AE_PGR_WORD];
	unsigned int banner_color, banner_text_color, banner_stripe;
	/* the line under it: gametype, map, time played, "YOU: 4TH" */
	char gametype[48], map[48], played[16], you[16];
	/* what the first STATS line is called: SCORE, or the game's own clock (TIME WITH THE BALL, TIME ON THE HILL) */
	char score_label[AE_PGR_WORD];
	/* the objective numbers' labels for the mode, "" past the last */
	char objective_label[AE_PGR_OBJECTIVES][AE_PGR_WORD];
	/* rounds (a round game): the teams' rounds won; rounds = 0: not a round game */
	int rounds;
	long rounds_won[AE_PGR_TEAMS];
	/* a summary arrived for at least one player: the numbers beyond the game's own are the host's */
	int have_summaries;
	/* a made-up report (HALO_AE_PGCR_DEMO): never added to the service record */
	int demo;
};

/* ---------- the cursor */

/* up (-1) or down (+1) one row, held to the board's ends */
int ae_pgr_cursor_step(struct ae_pgr_report const *report, int cursor, int delta);
/* LB (-1) / RB (+1): a team game goes to the other team's top row (the same rank when it has it); a free for all
board of more than AE_PGR_PAGE rows moves a page */
int ae_pgr_cursor_jump(struct ae_pgr_report const *report, int cursor, int direction);
/* the cursor's first guess: this machine's first player's row, else the top */
int ae_pgr_cursor_start(struct ae_pgr_report const *report);
/* the team of a row, 0 when it has none */
int ae_pgr_row_team(struct ae_pgr_report const *report, int row);

/* ---------- medals */

/* a medal's points (ae_medal_ids.h's: how rare it is) and family */
int ae_pgr_medal_points(int medal);
enum ae_pgr_family ae_pgr_medal_family(int medal);
/* the medal's name as the menus read it: "Open Season" as "OPEN SEASON" */
int ae_pgr_medal_name(int medal, char *out, size_t size);
/* the medals of a summary, rarest first (points, then the order of their numbers; the distinct ones): their ids and
counts into the arrays, returning how many (at most AE_PGCR_SUMMARY_MEDALS) */
int ae_pgr_medals_sorted(struct ae_pgcr_summary const *summary, int *medals, int *counts);
/* the total of a summary's medals' counts */
int ae_pgr_medal_total(struct ae_pgcr_summary const *summary);
/* the rest of the count behind a strip that shows these many distinct medals: the "+N" (0: none) */
int ae_pgr_strip_rest(struct ae_pgcr_summary const *summary, int shown_medals);
/* the family's colour, 0xRRGGBBAA */
unsigned int ae_pgr_family_color(enum ae_pgr_family family);

/* ---------- words */

/* kills over deaths, two decimals: "1.27"; no deaths is the kills ("5.00"); a dash for unknown */
void ae_pgr_kd_text(long kills, long deaths, char *out, size_t size);
/* shots hit over shots fired as a whole percent: "38%"; a dash with none fired or unknown */
void ae_pgr_accuracy_text(long fired, long hit, char *out, size_t size);
/* a number, or a dash for AE_PGR_UNKNOWN */
void ae_pgr_number_text(long value, char *out, size_t size);
/* a weapon's name from its damage tag's path: weapons\battle rifle\bullet as "BATTLE RIFLE", vehicles\warthog\bullet as
"WARTHOG", else the path's last part; "" for no path */
void ae_pgr_weapon_name(char const *path, char *out, size_t size);
/* a count of kills: "7 kills", "1 kill" */
void ae_pgr_kills_text(long kills, char *out, size_t size);
/* the points that make up a score, when they add up to it: kills less suicides and betrayals. Fills the three numbers
and returns 1 when kills - suicides - betrayals equals the score, else 0 (the pane shows no breakdown) */
int ae_pgr_points_add_up(struct ae_pgr_row const *row, long *kills, long *suicides, long *betrayals);

/* ---------- winners, rivals and the service record */

/* who won: a team game's first team when its score is the higher (a tie: nobody; the first team shown is the leading one);
a free for all's first place when it is held by one player alone. Sets every row's won */
void ae_pgr_decide_winners(struct ae_pgr_report *report);
/* the name of this machine's first player (the rivals' owner), "" for none */
char const *ae_pgr_owner_name(struct ae_pgr_report const *report);
/* every other row whose name is a rival of the owner gets rival = 1 (is_rival is the store's); the count */
int ae_pgr_mark_rivals(struct ae_pgr_report *report, int (*is_rival)(void *context, char const *owner, char const *name),
	void *context);
/* the report as the service record takes a game: 1 when there is a game to take (a valid report that is not made up) */
int ae_pgr_to_record_game(struct ae_pgr_report const *report, struct ae_sr_game *game);

/* ---------- a made-up report (HALO_AE_PGCR_DEMO, for looking at the screen and the tests' frames; never a real game's) */

/* "4v4" (teams of 4), "8v8", "ffa8" (a free for all of 8): a team size or a player count after the prefix. 1 when the text is
one of these (the report filled, summaries too, the player of slot 3 this machine's), else 0 and the report left alone */
int ae_pgr_demo_report(struct ae_pgr_report *report, char const *kind);

#endif
