/*
AE_RECORD_STATS.H

(rc2) The numbers a service record keeps beyond games, wins, kills, deaths and assists, one set of rules for a player's
record (ae_service_record.h) and a bot's (ae_bot_record_rules.h): time played, accuracy, damage, kill kinds, the objective
numbers, the top weapons and the maps. Pure code (no engine headers): port/linux/tests/ae_record_stats_test.c runs it.

- ONE GAME for one player or bot (struct ae_rs_game) is made from a post-game report's row and its summary
  (ae_rs_game_make) and added to the record's totals (ae_rs_add). The counts that come from a version 3 summary (damage,
  the kill kinds, kills as the flag carrier, carrier time, plants and defuses, headshots) are added only when that game's
  summary was a version 3 one (have_v3): a game from an older host leaves them out, and the totals keep the number of games
  they cover (counted_v3, with the kills, seconds and hits of those games) so a share is never made of two different sets.
- A record written before the update has totals from every game but these numbers only from the games counted since:
  `counted` games carried the new numbers (time, shots, objectives), `counted_v3` games the version 3 ones. A cell of a
  number that is not counted yet is a dash (ae_rs_known); the COMBAT page's block heads say "FROM n GAMES" (ae_rs_from_games)
  while n is below the record's games. Nothing is backfilled.
- Damage is in points where a full shield and body is 100; shown whole with thousands separators (ae_rs_number_text).
  The damage ratio is dealt over taken; KDA is (kills + assists / 3) over deaths.
*/

#ifndef __AE_RECORD_STATS_H
#define __AE_RECORD_STATS_H

#include <stddef.h>

#include "ae_pgcr_rules.h"

enum
{
	AE_RS_WEAPONS = 5,
	AE_RS_WEAPON_NAME = 24,
	AE_RS_MAPS = 16,
	AE_RS_MAP_NAME = 48,
	AE_RS_KINDS = 7,
	/* the groups ae_rs_stats_write makes */
	AE_RS_GROUPS = 8,
	/* the post-game report's game kinds: ae_pgr_mode's order */
	AE_RS_KIND_SLAYER = 0,
	AE_RS_KIND_FLAG,
	AE_RS_KIND_BALL,
	AE_RS_KIND_HILL,
	AE_RS_KIND_RACE,
	AE_RS_KIND_BOMB,
	AE_RS_KIND_OTHER
};

/* the numbers, summed over the games counted (a best is the highest, a best lap the lowest nonzero) */
struct ae_rs_stats
{
	long counted, counted_v3;
	long ties, best_spree, seconds;
	long fired, hit;
	long flag_caps, flag_returns, flag_grabs;
	long hill_ticks, ball_ticks, ball_carrier_kills, as_ball_carrier;
	long laps, best_lap;
	/* the version 3 numbers, and the kills, seconds and hits of the games they cover */
	long dealt, taken, headshots, melee, power, grenade, assassin, splatter, stick;
	long as_carrier, carrier_seconds, bomb_plants, bomb_defuses;
	long kills_v3, seconds_v3, hit_v3;
};

/* one game's numbers for one player or bot: x holds the game's (counted fields unused) */
struct ae_rs_game
{
	int tied;
	int have_v3;
	struct ae_rs_stats x;
};

struct ae_rs_weapon
{
	char name[AE_RS_WEAPON_NAME];
	long kills;
};

struct ae_rs_map
{
	char name[AE_RS_MAP_NAME];
	long games, wins, kills, deaths;
};

/* a report row's facts (the screen rules fill it from a row; the bot glue the same way) */
struct ae_rs_input
{
	int mode;                       /* AE_RS_KIND_ */
	int won, quit, tied;
	long kills, deaths, assists;
	long seconds;                   /* the game's length (0 for a player who quit) */
	long fired, hit;                /* -1: not known */
	long score_value;               /* a hill's or ball's clock in ticks */
	long objective[3];              /* flag: grabs, returns, captures; -1: not known */
	long laps, best_lap_ticks;      /* a race's (-1: not known) */
	long ball_carrier_kills, as_ball_carrier;   /* -1: not known */
	struct ae_pgcr_summary const *summary;      /* NULL for none */
};

/* the game's numbers from the facts */
void ae_rs_game_make(struct ae_rs_game *game, struct ae_rs_input const *input);
/* a game added to the totals */
void ae_rs_add(struct ae_rs_stats *stats, struct ae_rs_game const *game);

/* the top weapons: kills merged by name (the most kills first, the fewest replaced past the limit); names are made safe
for the file ("|", ",", ":" and control characters as "?") */
void ae_rs_weapon_add(struct ae_rs_weapon *list, int *count, int limit, char const *name, long kills);
/* the maps: a game on a map (compared by name, letter case aside); past `limit` the one with the fewest games (the first
of equals) is replaced */
void ae_rs_map_add(struct ae_rs_map *list, int *count, int limit, char const *name, int won, long kills, long deaths);
/* the maps most played first (then most won, then by name): their indices into list, at most `most`: how many */
int ae_rs_map_order(struct ae_rs_map const *list, int count, int *out, int most);

/* ---- the file: the numbers as the groups of one line (after its key), and back */
int ae_rs_stats_write(struct ae_rs_stats const *stats, char *out, size_t size);
/* text "g | g | ..." read into stats (made empty first): 1 taken, 0 when a group is not whole numbers; fewer groups than
the writer makes leave the rest 0 */
int ae_rs_stats_read(char const *text, struct ae_rs_stats *stats);

/* the text after the groups' last bar (a line's field that follows the numbers), NULL when the text has fewer groups */
char const *ae_rs_after_groups(char const *text);

/* ---- DEV MODE's made-up numbers (the staged scenes of `record_medals` and `botrecord`), from a seed */
/* the shared numbers for a record of `games` games (kills, deaths, assists given) of which `counted` carry the new numbers and
`counted_v3` the version 3 ones (each at most the games before it), the top weapons (all AE_RS_WEAPONS), `maps` maps (at most
map_limit) with the games spread over them; the same seed makes the same numbers */
void ae_rs_demo(struct ae_rs_stats *x, struct ae_rs_weapon *weapons, int *weapon_count, struct ae_rs_map *maps, int *map_count,
	int map_limit, int map_wanted, long games, long wins, long kills, long deaths, long assists, long counted, long counted_v3,
	unsigned long seed);
/* the games, wins, kills, deaths and assists split over the game kinds (the first kinds get the most; every sum is exact) */
void ae_rs_demo_kinds(long kind[AE_RS_KINDS][5], long games, long wins, long kills, long deaths, long assists,
	unsigned long seed);
/* the same over the first `kinds` kinds in the kinds' order (1 .. AE_RS_KINDS; 0 or 3: ae_rs_demo_kinds's three) */
void ae_rs_demo_kinds_over(long kind[AE_RS_KINDS][5], long games, long wins, long kills, long deaths, long assists,
	unsigned long seed, int kinds);

/* ---- the words */
/* a whole number with thousands separators: "11,240" */
void ae_rs_number_text(long value, char *out, size_t size);
/* a ratio x100 as "1.27" */
void ae_rs_ratio_text(long ratio100, char *out, size_t size);
/* a ratio of two counts x100: a over b, a x100 when b is 0 */
long ae_rs_ratio100(long a, long b);
/* (kills + assists / 3) over deaths, x100 */
long ae_rs_kda100(long kills, long assists, long deaths);
/* a share of a whole as a whole percent (0 for no whole) */
long ae_rs_percent(long part, long whole);
/* seconds as "47M", "12H 04M" or "1D 3H" (a dash text is the caller's) */
void ae_rs_time_text(long seconds, char *out, size_t size);
/* a clock in ticks as "12:04" (a lap or hill time) */
void ae_rs_clock_text(long ticks, char *out, size_t size);
/* whether a number is counted at all (the basic group, the version 3 group) */
int ae_rs_known(struct ae_rs_stats const *stats);
int ae_rs_known_v3(struct ae_rs_stats const *stats);
/* the muted tail of a block head: "FROM 3 GAMES" ("FROM 1 GAME") when `counted` games are fewer than `games`, else "" (1 shown) */
int ae_rs_from_games(long counted, long games, char *out, size_t size);

#endif
