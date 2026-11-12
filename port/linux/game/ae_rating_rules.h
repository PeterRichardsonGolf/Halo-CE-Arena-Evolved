/*
AE_RATING_RULES.H

The rating core (0.7.7): levels, points, the window a level may be matched with, how a finished game moves points, how a
search for a game scores and widens, the start delay by player count, the team balance cost, and the offline roster helpers
(bot anchors, a seeded draw of bot levels, balanced teams). Pure code (no engine headers): port/linux/tests/ae_rating_rules_test.c
runs it. Nothing here reads a file or the clock; the career store (ae_career_record.h) keeps the numbers.

Rules in plain words:

- A rated player has points (0 .. AE_RATING_POINTS_MAX) and a level (0 .. 49, shown 1 .. 50) per playlist. The level comes
  from the points through a table of 50 thresholds. After a loss that would drop exactly one level, the old level stays while
  the points are still above the middle of the band of the level below.
- A game moves points by PLACEMENT only (kills, score and objectives never count). For each opponent on another team with a
  known level the player either beat (placed better, or the opponent has no placement) or lost to (placed worse): the gain
  comes from a table of the level difference, scaled by a percentage of the player's own level, and the gains are averaged
  (integer division). The result is kept inside what the points can take, then inside +-200.
- Team games use the team's placement for every member and the member's own level. A rating of whole teams (parties up to a
  team) uses the same function with one entry for each team.
- A player who left before the end is kept in the game's entries as present with no placement (place -1): to every player who
  placed he is an opponent beaten, and his own change is a loss against everyone who placed (as a player placed last).
  AE_RATING_QUIT_NONE (a quitter is not moved) is kept for a host that wants it. The career store counts the quit.
- The window of a level is the highest level it may be matched with. The lowest level whose window reaches a level is "low".
  A party plays at the higher of the member's level and the low of the party's best member.
- A search starts at the party's own level and widens one step a round to the window's width; a ranked search ends there, a
  social one then opens to everyone, once. A candidate game is scored (closeness of level, same playlist, players already in,
  weak host, language, round trip) and accepted when the score reaches a minimum that falls while the search widens.
- Teams are dealt by party (largest party to the smallest team) and parties are swapped between teams while that makes the
  teams more even; evenness is a cost of seven times the spread of the team totals plus three times the spread by rank.
*/

#ifndef __AE_RATING_RULES_H
#define __AE_RATING_RULES_H

#include <stddef.h>

enum
{
	AE_RATING_LEVELS = 50,
	/* the window table holds this many levels (a window may reach past the last level) */
	AE_RATING_WINDOW_LEVELS = 128,
	AE_RATING_POINTS_MAX = 0x3fffffff,
	AE_RATING_CHANGE_MAX = 200,
	AE_RATING_NONE = -1,
	/* the table of level differences: 0 .. 254, 127 for equal levels */
	AE_RATING_DIFFERENCES = 255,
	AE_RATING_SEARCH_RESULT_LIMIT = 150,
	AE_RATING_SEARCH_READY_WAIT_MS = 5000,
	AE_RATING_SCORE_MINIMUM_START = 200,
	AE_RATING_SCORE_MINIMUM_END = 100,
	AE_RATING_SCORE_CLOSE_LEVELS = 4,
	AE_RATING_SCORE_CLOSE_BEST = 100,
	AE_RATING_SCORE_CLOSE_WORST = 25,
	AE_RATING_SCORE_SAME_PLAYLIST = 50,
	AE_RATING_SCORE_POPULATION_CAP = 180,
	AE_RATING_SCORE_POPULATION_PER_PLAYER = 2,
	AE_RATING_SCORE_WEAK_HOST = 100,
	AE_RATING_BANDWIDTH_LOW = 262144,
	AE_RATING_LANGUAGES = 9,
	AE_RATING_NEVER = 0x7fffffff,
	/* a step of the search past the window: everyone (a social search once); the search is over */
	AE_RATING_STEP_OPEN = -1,
	AE_RATING_STEP_END = -2,
	AE_RATING_BALANCE_PLAYERS = 16,
	AE_RATING_BALANCE_TEAMS = 8,
	AE_RATING_BALANCE_PARTIES = 16
};

/* the language classes whose mismatch costs most (bit = language): classes 1, 6 and 7 */
#define AE_RATING_LANGUAGES_STRICT 0xC2u

/* ---- levels and points ---- */

/* the points at which a level starts (level 0 .. 49); out of range: AE_RATING_POINTS_MAX */
long ae_rating_level_points(int level);
/* the level of this points value (0 .. 49). previous is the level before the change: when the points would be exactly one
   level lower and still above the middle of that lower band, the previous level stays. previous 0 = no hysteresis */
int ae_rating_level_from_points(long points, int previous);
/* the level shown (1 .. 50) */
int ae_rating_level_shown(int level);

/* ---- one game ---- */

/* the base of a win and of a loss for a level difference index (own - other + 127, 0 .. 254); a loss is negative */
int ae_rating_win_base(int difference);
int ae_rating_loss_base(int difference);
/* the percentage of a win and of a loss kept at the player's own level */
int ae_rating_win_weight(int level);
int ae_rating_loss_weight(int level);
/* what beating or losing to one opponent is worth. places: -1 = none. 0 when both are equal or neither placed */
long ae_rating_pair_change(int own_level, int other_level, int own_place, int other_place);

/* one player (or one team) of a finished game */
struct ae_rating_entry
{
	int present;    /* counts as a player of the game (a quitter is present with place -1; 0 = never was: skipped) */
	int rated;      /* the level is known and the entry counts (not a guest) */
	int level;      /* 0 .. 49 */
	int place;      /* 0 = first; -1 none (present but no placement: behind everyone who placed). A team's place for each member */
	int team;       /* the team (each player of a free for all alone) */
	int quit;       /* left before the end: counted at place -1 whatever place says (a player who rejoined is not a quit) */
	int quit_order; /* 0 = the first to leave, 1 .. later ones, -1 for none */
	int party;      /* the party he came in with (-1 alone; players of one party share an id) */
};

/* the change of the points of entries[self]: the average over the other entries that are present, rated and on another
   team; kept inside what the points can take and then +-AE_RATING_CHANGE_MAX. 0 for no opponent */
long ae_rating_game_change(struct ae_rating_entry const *entries, int count, int self, long points);

enum
{
	/* quit protection */
	AE_RATING_QUIT_EARLY_MS = 120000,   /* a first quit inside this much play is "early" */
	AE_RATING_QUIT_EARLY_CAP = 20,      /* an early quit caps everyone else's change to +-this */
	AE_RATING_QUIT_PARTY_PERCENT = 25   /* a later leaver of an earlier quitter's party keeps this share of his normal loss */
};

/* the change of entries[self] with quit protection. Without a quit it is ae_rating_game_change. The first quitter takes the
   quit rule (behind everyone, a loss). After the first quit: a player outside the first quitter's party who stays loses nothing
   (his gains stay); a later leaver loses nothing, unless he is of the party of an earlier quitter, then 25% of his normal
   (quit) loss; a stayer of the first quitter's party takes the normal change. When the first quit came within
   AE_RATING_QUIT_EARLY_MS of play, every player's change but the first quitter's is capped to +-AE_RATING_QUIT_EARLY_CAP.
   first_quit_ms: the game time of the first quit (-1 when unknown or none) */
long ae_rating_protected_change(struct ae_rating_entry const *entries, int count, int self, long points, long first_quit_ms);
/* whether the quit counter of this player goes up: the first quitter, and a later leaver of an earlier quitter's party */
int ae_rating_protected_counts_quit(struct ae_rating_entry const *entries, int count, int self);

/* a player's standing */
struct ae_rating_state
{
	long points;
	int level;
	int highest;
};

/* the standing after a change: the points kept in range, the level from the points (with the hysteresis), the highest level
   only up */
void ae_rating_apply(struct ae_rating_state *state, long change);
/* a state of this points (a new player: 0) */
void ae_rating_state_init(struct ae_rating_state *state, long points);

enum
{
	AE_RATING_QUIT_NONE = 0,    /* a quitter is not moved */
	AE_RATING_QUIT_AS_LAST = 1  /* the rule: a quitter has no placement, behind everyone who placed */
};

/* the change of a player who left the game before its end: 0 for AE_RATING_QUIT_NONE; for AE_RATING_QUIT_AS_LAST the change
   of entries[self] with its place taken away */
long ae_rating_quit_change(int policy, struct ae_rating_entry const *entries, int count, int self, long points);

/* ---- the window and the search ---- */

struct ae_rating_window
{
	unsigned char upper[AE_RATING_WINDOW_LEVELS];
};

/* the default window: level + 5 (levels 0 .. 5), + 6 (6 .. 15), + 7 (16 .. 25), + 8 (26 .. 30), + 9 (31 .. 35), + 10 above */
void ae_rating_window_default(struct ae_rating_window *window);
/* the highest level this one may be matched with */
int ae_rating_window_upper(struct ae_rating_window const *window, int level);
/* the lowest level whose window reaches this one */
int ae_rating_window_low(struct ae_rating_window const *window, int level);
/* the level a party plays at for one member: the member's, or the low of the party's best when that is higher */
int ae_rating_party_level(struct ae_rating_window const *window, int own_level, int party_best);

struct ae_rating_search
{
	int party_low;      /* the party's lowest member */
	int low;            /* the lowest level that may play with the party's lowest member */
	int high;           /* the higher of low and the party's best member */
	int upper;          /* the window's top for high */
	int span;           /* how many steps the search widens */
};

void ae_rating_search_setup(struct ae_rating_window const *window, int party_low, int party_high,
	struct ae_rating_search *search);
/* the levels a game may hold at this step (0 .. span); AE_RATING_STEP_OPEN: everyone (min 0, max 127) */
void ae_rating_search_range(struct ae_rating_search const *search, int step, int *minimum, int *maximum);
/* the step after this one: step + 1 until the span; at the span a ranked search is over (AE_RATING_STEP_END) and a social one
   opens (AE_RATING_STEP_OPEN), which is then over too */
int ae_rating_search_next(struct ae_rating_search const *search, int step, int ranked);
/* the least score a game needs at this step: 200 falling to 100 over the span; 100 once open or with no span */
int ae_rating_search_minimum_score(int step, int span);
/* whether a game with the advertised levels may be taken at this step */
int ae_rating_search_allows(struct ae_rating_search const *search, int step, int game_minimum, int game_maximum);
/* whether the results seen so far end the search */
int ae_rating_search_exhausted(int results_seen);

struct ae_rating_candidate
{
	int level;              /* the game's level (its players' average) */
	int same_playlist;
	int players;            /* already in */
	int has_details;        /* its host sent the bandwidth and language */
	long host_up, host_down;        /* bits a second */
	long my_up, my_down;
	int host_language;
	int my_language;
	int round_trip_known;
	long round_trip_ms;
};

/* a candidate game's score; strict language classes as AE_RATING_LANGUAGES_STRICT */
long ae_rating_candidate_score(struct ae_rating_candidate const *candidate, int my_level, unsigned strict_languages);
/* the language term alone */
int ae_rating_language_score(int mine, int theirs, unsigned strict_languages);
/* the round-trip term alone (ms) */
int ae_rating_round_trip_score(long milliseconds);
/* whether a search may take its best candidate now: all candidates answered, or a quarter of them and over the wait */
int ae_rating_search_ready(int candidates, int answered, long elapsed_ms);

/* ---- the start delay ---- */

/* the wait before a game of this many players starts: AE_RATING_NEVER outside [minimum, maximum], 0 when full, else a share of
   the longest delay (ms) */
long ae_rating_start_delay_players(long longest, int minimum, int maximum, int players);
/* the same for teams: the sizes are per team; players is the total */
long ae_rating_start_delay_teams(long longest, int team_count, int minimum_size, int maximum_size, int players);

/* ---- team balance ---- */

/* the cost of teams: 7 times the spread of the team totals plus 3 times the spread by rank. team_of[i] is player i's team;
   weights are the levels */
long ae_rating_balance_cost(int team_count, int count, long const *weights, int const *team_of);
/* the spread of the team sizes */
int ae_rating_balance_count_spread(int team_count, int count, int const *team_of);
/* parties (party[i] is player i's party, 0 .. 15) dealt to the teams, then swapped while it helps (optimize). flag_minimum ..
   flag_maximum: parties of that size only swap with each other (flag_minimum > flag_maximum: none). Fills team_of; returns
   1, or 0 when the biggest team would be over maximum_team_size (team_of is then not written). cost, when not NULL, gets
   the cost of the result */
int ae_rating_balance_teams(int team_count, int maximum_team_size, int count, long const *weights, int const *party,
	int optimize, int flag_minimum, int flag_maximum, int *team_of, long *cost);

/* ---- bots and the offline roster ---- */

enum
{
	AE_RATING_BOT_RECRUIT = 0,
	AE_RATING_BOT_NORMAL = 1,
	AE_RATING_BOT_HARD = 2,
	AE_RATING_BOT_LEGENDARY = 3,
	AE_RATING_BOT_ELITE = 4,        /* above LEGENDARY, so the full 50 levels can be reached against bots */
	AE_RATING_BOT_DIFFICULTIES = 5
};

/* a bot difficulty's anchor level (0 .. 49; AE's own numbers: shown 6, 18, 31, 43 and 49) */
int ae_rating_bot_anchor(int difficulty);
/* the difficulty whose anchor is nearest to a level (a tie goes to the lower one) */
int ae_rating_bot_difficulty(int level);

/* a seeded generator, passed in so a roster is repeatable */
struct ae_rating_rng
{
	unsigned long state;
};
void ae_rating_rng_seed(struct ae_rating_rng *rng, unsigned long seed);
/* 0 .. bound - 1 (bound > 0) */
unsigned long ae_rating_rng_below(struct ae_rating_rng *rng, unsigned long bound);

/* a bot level drawn from the player's window [low, upper], most likely the player's own level: weights fall one for each
   level of distance, to 1 at the farthest */
int ae_rating_roster_draw_level(struct ae_rating_window const *window, struct ae_rating_rng *rng, int player_level);

struct ae_rating_bot
{
	int level;          /* the level it is rated at (what the points are computed against) */
	int difficulty;     /* the nearest anchor's difficulty */
	int shooting_jitter;    /* -1, 0 or 1 level on the shooting skill */
	int movement_jitter;    /* -1, 0 or 1 level on the movement skill, drawn apart */
	int team;
};

struct ae_rating_roster
{
	int count;          /* players in all; the human is index 0 */
	int team_count;
	struct ae_rating_bot bot[AE_RATING_BALANCE_PLAYERS];    /* bot[0] is the human: its level is the player's, difficulty -1 */
	long cost;
};

/* a match for one human: team_count teams of team_size (team_count 0 or 1: a free for all of team_size players), the human
   in it and bots for every other seat, bots drawn from the human's window and the teams balanced by the cost.
   1, or 0 for a shape that does not fit (more than AE_RATING_BALANCE_PLAYERS players or AE_RATING_BALANCE_TEAMS teams) */
int ae_rating_roster_make(struct ae_rating_window const *window, struct ae_rating_rng *rng, int player_level, int team_count,
	int team_size, struct ae_rating_roster *roster);

#endif
