/*
AE_CAREER_CHALLENGE_RULES.H

DAILY and WEEKLY CHALLENGES: a table of challenge kinds, three of each period picked by a seed from the local date (no server: the
same date gives the same three on every PC), a counter for each and the XP each pays once when its target is reached. Pure code
(no engine headers): port/linux/tests/ae_career_xp_test.c runs it.

A day is the count of local days since 1970-01-01; a week is Monday to Sunday, counted from the same start. The picks of a period
are three kinds of that period with three different metrics. A challenge's progress is held to its target.
*/

#ifndef __AE_CAREER_CHALLENGE_RULES_H
#define __AE_CAREER_CHALLENGE_RULES_H

#include "ae_career_xp_rules.h"

enum
{
	AE_CH_DAILY = 0,
	AE_CH_WEEKLY = 1,
	AE_CH_PER_PERIOD = 3,
	AE_CH_SLOTS = 2 * AE_CH_PER_PERIOD,
	AE_CH_ID = 16,
	/* what a challenge counts */
	AE_CH_GAMES = 0,
	AE_CH_WINS,
	AE_CH_KILLS,
	AE_CH_ASSISTS,
	AE_CH_MEDALS,
	AE_CH_OBJECTIVES,
	AE_CH_MINUTES,
	AE_CH_CAMPAIGN_LEVELS,
	AE_CH_METRICS,
	/* the game kinds a challenge counts (ae_pgr_mode's order: slayer is 0; flag, ball, hill and bomb are the objective ones) */
	AE_CH_ANY_KIND = 0,
	AE_CH_SLAYER_ONLY,
	AE_CH_OBJECTIVE_ONLY
};

struct ae_ch_kind
{
	char const *id;
	int period, metric, filter;
	long target, xp;
};

/* a finished game's numbers for the counters; kind is the post-game report's game kind (0 slayer, 1 flag, 2 ball, 3 hill, 4 race,
   5 bomb, 6 other) */
struct ae_ch_game
{
	int kind, won, quit;
	long kills, assists, medals, objectives, seconds;
};

int ae_ch_kind_count(void);
struct ae_ch_kind const *ae_ch_kind_at(int index);
/* the table's place of an id, -1 for none */
int ae_ch_kind_find(char const *id);

/* the day of a local clock: seconds since the epoch plus the zone's offset, as days (the clock is the caller's, local) */
long ae_ch_day_of(long local_seconds);
/* the day of a calendar date (year 1970 or later, month 1 .. 12, day 1 .. 31) as days since 1970-01-01; 0 out of range. A caller
   with a broken-down local time uses this, which needs no zone offset */
long ae_ch_day_from_date(int year, int month, int day);
/* the week (Monday start) a day is in */
long ae_ch_week_of(long day);
/* the three kinds (table places) of a period (AE_CH_DAILY with the day, AE_CH_WEEKLY with the week); AE_CH_PER_PERIOD */
int ae_ch_pick(int period, long period_id, int *picks);

/* a challenge's line in capitals, from its kind: "PLAY 2 OBJECTIVE GAMES", "GET 40 KILLS", "FINISH 3 CAMPAIGN LEVELS"; written whole, cut short when small */
void ae_ch_label(struct ae_ch_kind const *kind, char *out, size_t size);

/* a challenge's one sentence for its detail: "Finish 3 games of any kind. A game you leave early does not count." */
void ae_ch_description(struct ae_ch_kind const *kind, char *out, size_t size);

/* what a finished game adds to a challenge's counter (0 for a quit, or a game that does not count) */
long ae_ch_game_gain(struct ae_ch_kind const *kind, struct ae_ch_game const *game);
/* what a campaign level won adds (1 for the campaign levels metric, else 0) */
long ae_ch_campaign_gain(struct ae_ch_kind const *kind);

#endif
