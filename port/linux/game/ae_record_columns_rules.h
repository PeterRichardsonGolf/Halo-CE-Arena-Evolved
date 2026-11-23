/*
AE_RECORD_COLUMNS_RULES.H

The SERVICE RECORD's detail window (ae_screen_record_details.c) as pure rules (port/linux/tests/ae_record_columns_rules_test.c
runs them): what its header band, its kind chips and its columns say, for a player's record and a bot's alike (ae_record_view.h's
ae_rv_in). No engine headers: the window places what these decide.

- Four pages, stepped with LB / RB (Tab / Shift+Tab): MODES, COMBAT, WEAPONS & MAPS, MEDALS.
- MODES: a column for each kind played (SLAYER, FLAG, HILL, BALL, BOMB, RACE, OTHER, in that order); a kind with no games takes
  no column, and the objective kinds not played yet share one quiet NOT PLAYED YET column, last. Each column has its games, wins
  and losses, its win rate (a bar), three hero numbers (the kind's own, fixed per kind) with their per-game figure, plain rows
  (totals only), and that kind's medals, rarest first (SLAYER's own medals; OTHER has none of its own). SLAYER and OTHER show
  KILLS, K/D and ASSISTS from the kind's line, DEATHS and KILL SPREAD. The column with the best win rate of 5 or more games is
  tagged BEST WIN RATE (when two columns or more show).
- The kind chips over the columns, in the columns' order, are one selection with the ring: the lit chip is the ringed column's
  kind (the favourite kind's column is ringed at open; no chip is lit on NOT PLAYED YET). More columns than fit at five across
  show as a run of five that moves the least to keep the ringed column in view.
- Every time is a clock: m:ss, h:mm:ss from an hour; a per-game time is a clock too.
- A number the record does not have for these games is the dash; a cell with no meaning (the per game of a ratio or a best) is
  empty. A block whose numbers come from fewer games than the record has says so at its foot ("FROM 196 OF 218 GAMES").
- A per-game figure is shown only when its divisor is the games its total covers. A player's kind lines count every game, the
  objective numbers only the games counted since they were kept: their per game shows while every game was counted (counted ==
  games, counted_v3 == games for the version 3 ones). A bot's kind lines are counted with its numbers: its per game shows for
  the basic ones always, for the version 3 ones while counted_v3 == counted. Kills, deaths and medals count every game of the kind.
- TOP WEAPONS' share is of all the record's kills while every game was counted; otherwise of the five kept weapons, and the
  column's head says OF TOP 5.
*/

#ifndef __AE_RECORD_COLUMNS_RULES_H
#define __AE_RECORD_COLUMNS_RULES_H

#include <stddef.h>

#include "ae_record_view.h"

enum
{
	AE_RC_MODES,
	AE_RC_COMBAT,
	AE_RC_WEAPONS_MAPS,
	AE_RC_MEDALS,
	AE_RC_PAGES
};

enum
{
	AE_RC_OBJECTIVE_KINDS = 5,
	AE_RC_MODE_COLUMNS = AE_RS_KINDS,
	/* MODES' run: the columns shown at once when they do not all fit at five across */
	AE_RC_RUN = 5,
	/* the ring's kind on the NOT PLAYED YET column */
	AE_RC_RING_NOT_PLAYED = AE_RS_KINDS,
	AE_RC_HEROES = 3,
	AE_RC_ROWS = 4,
	AE_RC_BARS = 8,
	AE_RC_COLUMN_MEDALS = 12,
	AE_RC_COMBAT_COLUMNS = 4,
	AE_RC_MEDAL_COLUMNS = 4,
	AE_RC_MAP_ROWS = 16,
	AE_RC_TEXT = 96,
	AE_RC_NOTE = 128,
	/* the grid in design units: content 96 .. 1824, the columns 24 apart, at most 677 wide; the NOT PLAYED YET column 326 */
	AE_RC_ROOM = 1728,
	AE_RC_GAP = 24,
	AE_RC_WIDEST = 677,
	AE_RC_NOT_PLAYED_W = 326
};

/* a page's chip word ("" out of range), and the next page (LB -1, RB +1), round */
char const *ae_rc_page_name(int page);
int ae_rc_page_step(int page, int direction);

/* the page a record page's box opens (ae_record_focus.h's AE_RF_*) and the column ringed there (-1 for none; MODES: -1, the
caller rings the kind's column); -1 for a box that opens no page of the window (MEDALS opens the MEDALS page, VS YOU its table) */
int ae_rc_page_of_box(int box, int *column);

/* ---------- the header band */

struct ae_rc_head
{
	/* GAMES, WIN RATE, K/D, BEST SPREE: the dash when not known */
	char games[16], win_rate[16], kd[16], best_spree[16];
	/* the live subtitle's second part from a player's last game: "LAST: WON ON PRISONER, FLAG" ("" for none) */
	char last[AE_RC_NOTE];
	/* the emblem's letter: the name's first letter or digit, upper case ("?" for none) */
	char initial[4];
};

void ae_rc_head(struct ae_rv_in const *in, struct ae_sr_last const *last, char const *name, struct ae_rc_head *out);

/* ---------- MODES */

/* a kind chip: the kind's word and "41 GAMES  59%" (games: "41 GAMES", for a strip with no room for the rate); on for the
favourite kind (most games, then most kills) until ae_rc_chips_light lights the ringed one */
struct ae_rc_chip
{
	int kind;
	char name[12];
	char count[AE_RC_TEXT];
	char games[32];
	int on;
};

/* the kinds with games, in the columns' order (AE_RS_KINDS at most): how many */
int ae_rc_chips(struct ae_rv_in const *in, struct ae_rc_chip *out);
/* lights the chip of `kind` alone (none for AE_RC_RING_NOT_PLAYED or a kind with no chip): its index, -1 for none */
int ae_rc_chips_light(struct ae_rc_chip *chips, int count, int kind);
/* the favourite kind: most games, then most kills; -1 with no games */
int ae_rc_favourite_kind(struct ae_rv_in const *in);

struct ae_rc_hero
{
	char label[AE_RC_TEXT];
	char value[AE_RC_TEXT];
	/* the right word: "0.9 A GAME", "PERSONAL BEST", "612 KILLS" or "" */
	char side[AE_RC_TEXT];
	int dash;
};

struct ae_rc_row
{
	char label[AE_RC_TEXT];
	char value[AE_RC_TEXT];
	int dash;
};

/* a bar of a list (KILL KINDS, TOP WEAPONS): label, a count and a share, the bar's length 0..1 (of the largest) */
struct ae_rc_bar
{
	char label[AE_RC_TEXT];
	char count[16];
	char share[16];
	float length;
};

/* a column of the window: MODES' kinds, COMBAT's blocks, the weapons */
struct ae_rc_column
{
	int kind;                       /* MODES: AE_RS_KIND_*; else -1 */
	char name[AE_RC_TEXT];          /* "FLAG", "ACCURACY" */
	char tag[AE_RC_TEXT];           /* "BEST WIN RATE" or "" */
	char games[AE_RC_TEXT];         /* "41 GAMES  24 W  17 L", "41,880 SHOTS" */
	char meter_word[AE_RC_TEXT];    /* "59% WON", "41% LANDED" ("" with no bar) */
	int meter;                      /* the bar shows */
	float fill;                     /* its fill 0..1 */
	int hero_count;
	struct ae_rc_hero hero[AE_RC_HEROES];
	int row_count;
	struct ae_rc_row row[AE_RC_ROWS];
	int bar_count;
	struct ae_rc_bar bar[AE_RC_BARS];
	/* the medals' head ("FLAG MEDALS", "" for none), the medals earned (rarest first) and their counts, or the empty words */
	char medals_head[AE_RC_TEXT];
	int medal_count;
	int medal[AE_RC_COLUMN_MEDALS];
	unsigned long medal_n[AE_RC_COLUMN_MEDALS];
	/* the medals of the list earned in all (more than medal_count when the column keeps fewer) */
	int medals_earned;
	char medals_empty[AE_RC_NOTE];
	/* the coverage note at the foot ("" for none) */
	char note[AE_RC_NOTE];
};

/* MODES: the kinds played as columns (AE_RC_MODE_COLUMNS at most); the objective kinds not played into not_played (room for
AE_RC_OBJECTIVE_KINDS, AE_RS_KIND_*) and their count. Returns the columns */
int ae_rc_mode_columns(struct ae_rv_in const *in, struct ae_rc_column *out, int *not_played, int *not_played_count);
/* MODES' ringed column of `count` columns and the NOT PLAYED YET one after them when it shows: the column of `kind` (-1 none,
AE_RC_RING_NOT_PLAYED the NOT PLAYED YET column), else `column` (-1 none) kept within the columns, else the favourite kind's
column, else the first; -1 with no column at all */
int ae_rc_mode_ring(struct ae_rc_column const *columns, int count, int not_played_shows, int kind, int column, int favourite);
/* MODES' columns shown at once: every one (the NOT PLAYED YET column counted) while they fit at five across's width or wider,
else a run of AE_RC_RUN */
int ae_rc_mode_shown(int columns, int not_played_shows);
/* the run's first column: `first` moved the least to keep `ring` (-1 none) in view, within 0 .. slots - shown */
int ae_rc_mode_first(int slots, int shown, int first, int ring);
/* the NOT PLAYED YET column's sentence for a record of that name: a bot's names it, a player's own says "your" */
void ae_rc_not_played_text(char const *name, int bot, int own, char *out, size_t size);
/* a kind's objective word: "FLAG" ... ("" for SLAYER / OTHER) */
char const *ae_rc_kind_word(int kind);

/* a column's width for `columns` columns (with or without the NOT PLAYED YET column): min(677, room / columns) */
float ae_rc_column_width(int columns, int not_played_shows);

/* ---------- COMBAT: ACCURACY, KILL KINDS, DAMAGE, PER GAME (AE_RC_COMBAT_COLUMNS) */

int ae_rc_combat_columns(struct ae_rv_in const *in, struct ae_rc_column *out);

/* ---------- WEAPONS & MAPS */

/* the weapons as a column of bars (share of all kills, or of the top 5) */
void ae_rc_weapons_column(struct ae_rv_in const *in, struct ae_rc_column *out);

struct ae_rc_map_row
{
	char name[AE_RS_MAP_NAME];
	char games[16], win_rate[16], kd[16];
	float won;
};

/* every kept map, the most played first (AE_RC_MAP_ROWS at most): how many; the maps' head words into games ("12 MAPS") */
int ae_rc_maps(struct ae_rv_in const *in, struct ae_rc_map_row *out, char *head, size_t size);

/* ---------- MEDALS: one column a family (MULTIKILL, SPREE, SKILL, OBJECTIVE), the earned medals rarest first */

int ae_rc_medal_columns(struct ae_rv_in const *in, struct ae_rc_column *out);

/* ---------- CAREER's RECORDS card: the focused record in a column's look (games, wins, losses, ties, the win bar; KILLS A
GAME with its K/D, MEDALS with a game figure, FAVOURITE KIND with its games; LAST GAME, BEST MAP, MOST KILLS WITH). last may be
NULL (a bot) */

void ae_rc_card(struct ae_rv_in const *in, struct ae_sr_last const *last, struct ae_rc_column *out);

/* ---------- words */

/* seconds as a clock: "0:44", "49:30", "1:02:05" */
void ae_rc_clock(long seconds, char *out, size_t size);
/* whether a per-game figure of a basic (version 2) objective number, or of a version 3 one, has its right divisor */
int ae_rc_per_game_known(struct ae_rv_in const *in, int v3);

#endif
