/*
AE_RECORD_VIEW.H

(rc2) What the SERVICE RECORD pages show, as pure rules (port/linux/tests/ae_record_view_test.c runs them): one record,
a player's or a bot's, goes in (ae_rv_in) and the words of each pane come out, a label and a value for each cell, so the
two records read alike and the drawing code only places them. No engine headers.

- A total that is not counted yet is a dash (the numbers from the games since the update, ae_record_stats.h): TIME PLAYED,
  BEST SPREE, ACCURACY and the shots, the objectives; the version 3 numbers (damage, the kill kinds, kills as the flag
  carrier, plants and defuses) wait for a game from a host that counts them. Never a made-up zero. A number that has no
  meaning for a record (a kind never played) is the dash as well.
- A block's tail ("FROM 3 GAMES", muted, after its head) says how many games its numbers cover while that is fewer than the
  record's games; the COMBAT page's block heads and the new blocks have one.
- Numbers of 1000 or more have thousands separators ("11,240"); damage is whole points, a full shield and body being 100.
- Ratios have two decimals (K/D, KDA, the damage ratio), per-game and per-minute numbers one.
*/

#ifndef __AE_RECORD_VIEW_H
#define __AE_RECORD_VIEW_H

#include "ae_service_record.h"

#define AE_RV_DASH "\xE2\x80\x94"

enum
{
	AE_RV_TOTALS = 16,
	AE_RV_ACCURACY = 6,
	AE_RV_KILL_KINDS = 7,
	AE_RV_ASSISTS = 5,
	AE_RV_DAMAGE = 6,
	AE_RV_PER_GAME = 4,
	AE_RV_OBJECTIVES = 10,
	AE_RV_HIGHLIGHTS = 5,
	AE_RV_LAST = 8,
	AE_RV_MAPS = 10,
	AE_RV_TAIL = 24
};

/* the record a page shows */
struct ae_rv_in
{
	long games, wins, ties, kills, deaths, assists, best_spree;
	struct ae_rs_stats const *x;
	struct ae_sr_line kind[AE_RS_KINDS];
	unsigned long const *medals;            /* AE_MEDAL_COUNT counts */
	struct ae_rs_map const *maps;
	int map_count;
	struct ae_rs_weapon const *weapons;
	int weapon_count;
	/* (a bot's best spree is its record's own: known with a game; a player's is a counted number) */
	int spree_from_record;
};

struct ae_rv_cell
{
	char label[32];
	char value[32];
	int dash;       /* the value is the dash: drawn muted */
};

/* a row with a count and a share of the kills */
struct ae_rv_share
{
	char label[24];
	char count[16];
	char share[16];
};

struct ae_rv_kind_row
{
	char name[12];
	char games[16], wins[16], kills[16], deaths[16], assists[16], kd[16];
};

struct ae_rv_map_row
{
	char name[AE_RS_MAP_NAME];
	char games[16], wins[16], kd[16];
};

/* the cells' makers (the post-game DETAILS page uses them too): a value, the dash, a whole number with separators, a over b with
one decimal, a ratio with two, a percent (the dash when not known or the whole is 0) */
void ae_rv_cell_set(struct ae_rv_cell *cell, char const *label, char const *value);
void ae_rv_cell_dash(struct ae_rv_cell *cell, char const *label);
void ae_rv_cell_whole(struct ae_rv_cell *cell, char const *label, long value);
void ae_rv_cell_tenth(struct ae_rv_cell *cell, char const *label, long a, long b, int known);
void ae_rv_cell_ratio(struct ae_rv_cell *cell, char const *label, long a, long b, int known);
void ae_rv_cell_percent(struct ae_rv_cell *cell, char const *label, long part, long whole_count, int known);

/* CAREER TOTALS: 8 rows of two columns, the first column's 8 then the second's. Returns 16 */
int ae_rv_totals(struct ae_rv_in const *in, struct ae_rv_cell *out);
/* ACCURACY: shots fired, landed, missed, accuracy, headshot kills, headshot share; the block's tail */
int ae_rv_accuracy(struct ae_rv_in const *in, struct ae_rv_cell *out, char *tail);
/* KILL KINDS: headshots, melee, power weapons, grenades, assassinations, splatters, sticks */
int ae_rv_kill_kinds(struct ae_rv_in const *in, struct ae_rv_share *out, char *tail);
/* ASSISTS: assists, per game, wingman, driver assists, protector (medal counts) */
int ae_rv_assists(struct ae_rv_in const *in, struct ae_rv_cell *out);
/* DAMAGE: dealt, taken, ratio, then dealt per minute, taken per minute, dealt per hit */
int ae_rv_damage(struct ae_rv_in const *in, struct ae_rv_cell *out, char *tail);
/* PER GAME: kills, deaths, assists, damage dealt */
int ae_rv_per_game(struct ae_rv_in const *in, struct ae_rv_cell *out, char *tail);
/* OBJECTIVES: flag captures, returns, grabs, kills as the flag carrier, flag carriers killed, hill time, ball time, bomb
plants, bomb defuses, race laps */
int ae_rv_objectives(struct ae_rv_in const *in, struct ae_rv_cell *out, char *tail);
/* BY KIND OF GAME: the kinds with games, in the kinds' order: how many rows */
int ae_rv_kinds(struct ae_rv_in const *in, struct ae_rv_kind_row *out);
/* BY MAP: the most played first, at most AE_RV_MAPS; `more` the maps left out; the block's tail */
int ae_rv_maps(struct ae_rv_in const *in, struct ae_rv_map_row *out, int *more, char *tail);
/* TOP WEAPONS: the names and kills (AE_RS_WEAPONS at most); the block's tail */
int ae_rv_weapons(struct ae_rv_in const *in, struct ae_rv_cell *out, char *tail);
/* a player's HIGHLIGHTS: the weapon of most kills, the favourite kind (most games, then kills), the best map (most wins, then
games), the medal kinds earned, the rivals set */
int ae_rv_highlights(struct ae_rv_in const *in, int rivals, struct ae_rv_cell *out);
/* a player's LAST GAME: result, kills, deaths, assists, K/D, best spree, the weapon of most kills, medals; head gets the
tail words "PRISONER . FLAG" as two parts (map, kind) */
int ae_rv_last_game(struct ae_sr_last const *last, struct ae_rv_cell *out, char *map, char *kind, size_t size);
/* the medal kinds with a count, and the medals in all */
int ae_rv_medal_kinds(unsigned long const *medals);
long ae_rv_medal_total(unsigned long const *medals);

/* a box's DETAILS page as a table (A on a focused box, ae_record_focus.h): a name column and up to AE_RV_TABLE_COLUMNS value columns,
every row of the box with its totals and per-game numbers; a number not counted is the dash */
enum { AE_RV_TABLE_COLUMNS = 6, AE_RV_TABLE_ROWS = 20 };

struct ae_rv_table
{
	char title[32];
	char tail[AE_RV_TAIL];
	int columns, rows;
	char head[AE_RV_TABLE_COLUMNS][20];
	struct
	{
		char name[AE_RS_MAP_NAME];
		char value[AE_RV_TABLE_COLUMNS][20];
	} row[AE_RV_TABLE_ROWS];
};

/* the table of a box (ae_record_focus.h's AE_RF_*, but not the MEDALS box, which opens the MEDALS page); `last` is a player's last
game (NULL for none). 1 when the box has a table here, 0 when it has nothing more to show */
int ae_rv_details(int box, struct ae_rv_in const *in, struct ae_sr_last const *last, struct ae_rv_table *table);
/* whether the box has something more to show (the focus stops on it): the same rule as ae_rv_details' */
int ae_rv_has_details(int box, struct ae_rv_in const *in, struct ae_sr_last const *last);
/* a row added to a table: the name and the values (NULL ends the list) */
void ae_rv_table_row(struct ae_rv_table *table, char const *name, char const *first, ...);

/* a player's record as the view's input (the player may be NULL: an empty record) */
void ae_rv_from_player(struct ae_sr_player const *player, struct ae_rv_in *in);

#endif
