/*
AE_CAMPAIGN_SCORE_RULES.H

The campaign's score (ae_campaign_score.c): what a kill is worth, a level's par time, the log the totals are summed from, and
the saved game's score file. Pure (port/linux/tests/ae_campaign_score_rules_test.c): the values are
campaign_score_values.h's, the engine's facts come in as numbers. Derived from AGPL-3.0-or-later code (README credits).

- a kill is worth its enemy's base points (by its actor's type and its variant's name, or a vehicle's by its name), times its
  style's multiplier (one shot one kill, a headshot, a melee hit, a backsmack: from behind), times the difficulty's, times the
  skulls' (ae_campaign_score_skulls_multiplier: 1.0 until skulls exist), rounded to a point; an enemy the values do not know
  scores AE_CAMPAIGN_SCORE_UNKNOWN_POINTS;
- an ally killed, another player killed (a betrayal), a death and Revert to Checkpoint take points as they are; a checkpoint
  reverted to takes nothing else from the score or the time;
- when the level is won, its points are multiplied by its time's multiplier, by its time over its par time (a level with no par
  time has x1).
*/

#ifndef __AE_CAMPAIGN_SCORE_RULES_H
#define __AE_CAMPAIGN_SCORE_RULES_H

struct campaign_score_values;
struct campaign_score_actor_value;
struct campaign_score_match_value;

enum
{
	/* the points of an enemy of a type the values have nothing for, when it is hostile to the players */
	AE_CAMPAIGN_SCORE_UNKNOWN_POINTS = 10,
	/* the entries one level logs (a level places a few hundred enemies) */
	AE_CAMPAIGN_SCORE_MAXIMUM_ENTRIES = 8192,
	/* the players a score is kept for (their slots) */
	AE_CAMPAIGN_SCORE_PLAYERS = 16,
	/* the difficulties: easy, normal, heroic, legendary */
	AE_CAMPAIGN_SCORE_DIFFICULTIES = 4,
	/* ticks in a second */
	AE_CAMPAIGN_SCORE_TICKS_PER_SECOND = 30
};

/* what an entry of the log is */
enum
{
	AE_CAMPAIGN_SCORE_ENTRY_KILL = 0,
	/* an ally killed, or a friendly vehicle destroyed */
	AE_CAMPAIGN_SCORE_ENTRY_ALLY,
	/* another player killed */
	AE_CAMPAIGN_SCORE_ENTRY_BETRAYAL,
	/* the player's death */
	AE_CAMPAIGN_SCORE_ENTRY_DEATH,
	/* Revert to Checkpoint, from the pause menu */
	AE_CAMPAIGN_SCORE_ENTRY_REVERT,
	/* a medal (ae_campaign_medals_rules.h): variant its medal */
	AE_CAMPAIGN_SCORE_ENTRY_MEDAL,
	AE_CAMPAIGN_SCORE_ENTRY_COUNT
};

/* a kill's style bits */
enum
{
	AE_CAMPAIGN_SCORE_STYLE_ONE_SHOT = 0,
	AE_CAMPAIGN_SCORE_STYLE_HEADSHOT,
	AE_CAMPAIGN_SCORE_STYLE_MELEE,
	AE_CAMPAIGN_SCORE_STYLE_BACKSMACK,
	AE_CAMPAIGN_SCORE_STYLE_COUNT
};

struct ae_campaign_score_entry
{
	long game_time;
	/* the enemy's actor variant or the vehicle's definition (a tag index; -1 none), or a medal's number */
	long variant_index;
	long points;
	short player_slot;
	unsigned char kind;
	unsigned char style;
};

struct ae_campaign_score_log
{
	struct ae_campaign_score_entry entries[AE_CAMPAIGN_SCORE_MAXIMUM_ENTRIES];
	long count;
	/* the level's time in ticks, counted on through checkpoints reverted to */
	long ticks;
};

/* ---------- values */

/* a tag name's lower case into out (size bytes at least 1); a NULL name is "" */
void ae_campaign_score_lower(char const *name, char *out, long size);

/* the points of the first entry with a word in the lower-case name (0 for none) */
long ae_campaign_score_match_points(struct campaign_score_match_value const *values, short count, char const *lower);

/* an actor variant's points by its actor's type and its name's words (0 for a type not in the table) */
long ae_campaign_score_actor_points(struct campaign_score_actor_value const *values, short count, short actor_type,
	char const *lower);

/* a style's multiplier: those of its bits multiplied together (or the highest), the regular kill's with none */
float ae_campaign_score_style_multiplier(struct campaign_score_values const *values, unsigned char style);

/* a kill's points: base x style x difficulty (0-3, pinned) x skulls, rounded; 0 for a base of 0 or less */
long ae_campaign_score_kill_points(struct campaign_score_values const *values, long base, unsigned char style, int difficulty,
	float skulls_multiplier);

/* a medal's points: the values' (0 for none), times the difficulty's and the skulls' when the values say so */
long ae_campaign_score_medal_points(struct campaign_score_values const *values, char const *medal_key, int difficulty,
	float skulls_multiplier);

/* the points of an enemy the table has no row for: the stated default when it is hostile, else nothing */
long ae_campaign_score_unknown_points(int hostile);

/* the penalty of Revert to Checkpoint now: the values' (negative), or 0 when the cost is off */
long ae_campaign_score_revert_points(struct campaign_score_values const *values, int revert_cost_on);

/* ---------- time */

/* a level's par time in seconds by its name (the last part of levels\a10\a10, any case), times the difficulty's par multiplier
(and a daily-challenge multiplier of 1 here): -1 for a level with none */
long ae_campaign_score_par_seconds_of_level(struct campaign_score_values const *values, char const *level_name, int difficulty);

/* the time's multiplier: x1 for no par time, else by the time over the par time */
float ae_campaign_score_time_multiplier_of(struct campaign_score_values const *values, long par_seconds, long ticks);

/* the level's time over its par time (< 0: none) */
float ae_campaign_score_par_fraction_of(long par_seconds, long ticks);

/* a score at the level's end: the time's multiplier on it, none on less than nothing */
long ae_campaign_score_final(long points, float time_multiplier);

/* ---------- the log */

void ae_campaign_score_log_reset(struct ae_campaign_score_log *log);

/* an entry added: 1, or 0 when the log is full or the slot / kind is out of range */
int ae_campaign_score_log_append(struct ae_campaign_score_log *log, long game_time, int kind, long variant_index, int slot,
	long points, unsigned char style);

/* a player's points (a slot), and all players' together */
long ae_campaign_score_log_total(struct ae_campaign_score_log const *log, int slot);
long ae_campaign_score_log_team_total(struct ae_campaign_score_log const *log);

/* a player's points as shown: their own when each player keeps their own score ("competitive"), else the team's */
long ae_campaign_score_log_points(struct ae_campaign_score_log const *log, int slot, int competitive);

/* how many entries a player has of a kind (slot -1: any player's), and how many kills have a style bit */
long ae_campaign_score_log_count(struct ae_campaign_score_log const *log, int slot, int kind);
long ae_campaign_score_log_style_count(struct ae_campaign_score_log const *log, int slot, int style_bit);

/* ---------- what a HUD panel and an end card read */

/* the par time in ticks (-1 for none) */
long ae_campaign_score_par_ticks_of(long par_seconds);

/* the time multiplier's next step: 1 with the tick (of the level's time) at which it first drops and the value it drops to; 0 when it is
at its last step or the level has no par time */
int ae_campaign_score_next_drop_of(struct campaign_score_values const *values, long par_seconds, long ticks, long *drop_tick,
	float *next_multiplier);

/* a player's place by their points among the slots that have an entry: 1 first; players with the same points share a place; 0 for a slot
with no entry */
int ae_campaign_score_place(struct ae_campaign_score_log const *log, int slot);

/* what a level's log adds up to for a slot (-1: every player's) */
struct ae_campaign_score_summary
{
	long kills, headshots, melee_kills, backsmack_kills, one_shot_kills;
	long deaths, reverts, betrayals, allies_killed, medals;
	/* the points of the kill entries (as scored: base x style x difficulty x skulls), the medals', and each penalty kind's (negative) */
	long kill_points, medal_points, death_points, revert_points, betrayal_points, ally_points;
	long penalty_points, penalty_count;
	/* kills + medals + penalties: the log's total */
	long points;
};
void ae_campaign_score_summarize(struct ae_campaign_score_log const *log, int slot, struct ae_campaign_score_summary *summary);

/* the co-op score type's setting text ("competitive": each player's own; anything else, shared): 1 for each its own */
int ae_campaign_score_type_competitive(char const *type);

/* ---------- the score file beside a saved game */

enum
{
	AE_CAMPAIGN_SCORE_FILE_HEADER_SIZE = 32,
	AE_CAMPAIGN_SCORE_FILE_ENTRY_SIZE = 16,
	AE_CAMPAIGN_SCORE_FILE_TRAILER_SIZE = 4,
	AE_CAMPAIGN_SCORE_FILE_MAXIMUM_SIZE = AE_CAMPAIGN_SCORE_FILE_HEADER_SIZE +
		AE_CAMPAIGN_SCORE_MAXIMUM_ENTRIES * AE_CAMPAIGN_SCORE_FILE_ENTRY_SIZE + AE_CAMPAIGN_SCORE_FILE_TRAILER_SIZE
};

/* what binds a score file to its saved game: the saved game's own checksum, and a hash of the level's name */
unsigned long ae_campaign_score_name_hash(char const *name);

/* the bytes of the score file for a log: the number written (0 when `size` is too small) */
long ae_campaign_score_file_encode(struct ae_campaign_score_log const *log, unsigned long binding, unsigned long level_hash,
	int competitive, unsigned char *bytes, long size);

/* a score file read: 0 for good (the log, its time and the co-op type are set), else a reason (a static text) in *why. Anything
not a score file of this version, of this saved game and this level, or too big, or damaged, is refused; the log is then left
empty */
int ae_campaign_score_file_decode(unsigned char const *bytes, long size, unsigned long binding, unsigned long level_hash,
	struct ae_campaign_score_log *log, int *competitive, char const **why);

#endif
