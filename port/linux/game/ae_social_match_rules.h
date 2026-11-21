/*
AE_SOCIAL_MATCH_RULES.H

Bot matching of the offline social career (0.9.3). Pure code (no engine headers): port/linux/tests/ae_social_match_rules_test.c
runs it. It sits on the rating core (ae_rating_rules.h) and changes none of it.

- The TIER LADDER: 19 bot tiers from SHOOTING x MOVEMENT skill steps (RECRUIT 1 .. DEMON 7). Strength S = 2 x shooting +
  movement, one tier for each S from 3 to 21; each tier has a match level (1 .. 50 as the core shows levels).
- The hidden MATCH RATING of a player in a playlist: points 0 .. 4999, level = points / 100 (0 .. 49 inside; the core's
  shown level is one more, and the tier ladder's levels are shown levels). A game moves it by the core's pair table with both
  weights at 100 % (the visible-rank loss weights would let a losing player's bots grow harder), no hysteresis, averaged over
  the opponents, kept inside +-200, and doubled for the first AE_SOCIAL_PLACEMENT_GAMES games of a playlist.
- A ROSTER for one game: every bot seat draws a tier near the player's (offsets 0 / +-1 / +-2 at 60 / 15 each / 5 each
  percent) inside the player's window (ae_rating_window_default), seeded so a staged roster repeats; the teams are balanced
  by the core's cost with the human party kept together; each bot gets the search step at which it "joins" so a lobby fills
  nearest tier first. FIXED mode gives every bot one tier and no spread.
*/

#ifndef __AE_SOCIAL_MATCH_RULES_H
#define __AE_SOCIAL_MATCH_RULES_H

#include "ae_rating_rules.h"

enum
{
	AE_SOCIAL_TIERS = 19,
	/* the skill steps of the bots' skill words: RECRUIT 1, EASY 2, NORMAL 3, HEROIC 4, ELITE 5, LEGENDARY 6, DEMON 7 */
	AE_SOCIAL_SKILL_STEPS = 7,
	AE_SOCIAL_MATCH_POINTS_MAX = 4999,
	/* a new playlist's hidden rating: level 16 (shown 17) = tier 7 NORMAL / NORMAL */
	AE_SOCIAL_MATCH_START = 1600,
	AE_SOCIAL_PLACEMENT_GAMES = 5,
	AE_SOCIAL_SEATS = AE_RATING_BALANCE_PLAYERS,
	/* a mode: AUTO matches by the hidden rating; 1 .. 7 pins the bots at that skill's tier */
	AE_SOCIAL_MODE_AUTO = 0
};

/* CALIBRATION (0.9.4): the answer a profile gives once, "How much Halo have you played?", sets where every playlist's hidden
   rating starts. Not asked yet = the old start (NORMAL / NORMAL) */
enum
{
	AE_SOCIAL_EXPERIENCE_UNSET = 0,
	AE_SOCIAL_EXPERIENCE_NEW,       /* EASY / EASY bots: tier 4 */
	AE_SOCIAL_EXPERIENCE_SOME,      /* NORMAL / NORMAL: tier 7, the old start */
	AE_SOCIAL_EXPERIENCE_A_LOT,     /* HEROIC / HEROIC: tier 10 */
	NUMBER_OF_AE_SOCIAL_EXPERIENCES,
	/* a placement game's kills minus deaths, each one worth this many points, the sum kept inside +-AE_SOCIAL_PLACEMENT_KD_MAX */
	AE_SOCIAL_PLACEMENT_KD_POINTS = 15,
	AE_SOCIAL_PLACEMENT_KD_MAX = 200
};

struct ae_social_tier
{
	int shooting;   /* 1 .. 7 */
	int movement;   /* 1 .. 7 */
	int strength;   /* 2 x shooting + movement */
	int level;      /* the match level, shown (1 .. 50) */
};

/* tier 1 .. 19; NULL out of range */
struct ae_social_tier const *ae_social_tier(int tier);
/* the skill word of a step (1 .. 7): "RECRUIT" .. "DEMON"; "" out of range */
char const *ae_social_skill_name(int step);
/* the tier a FIXED skill (1 .. 7) pins: the same-skill tier (1, 4, 7, 10, 13, 16, 19); 0 out of range */
int ae_social_tier_fixed(int skill_step);
/* the tier whose level is nearest a shown level (a tie goes to the lower tier) */
int ae_social_tier_of_level(int level_shown);
/* a tier's level on the core's own scale (0 .. 49) */
int ae_social_tier_raw_level(int tier);

/* a rating's level on the core's scale (0 .. 49) and its tier (1 .. 19) */
int ae_social_match_level(long points);
int ae_social_match_tier(long points);
/* the points at the start of a shown level (1 .. 50), and at a tier's level */
long ae_social_match_points_of_level(int level_shown);
long ae_social_match_points_of_tier(int tier);

/* what one opponent is worth: a win, a loss, or 0 for equal places; own and other levels on the core's scale. No weights */
long ae_social_match_pair_change(int own_level, int other_level, int result);
/* a game's change: the pair changes averaged over count opponents (levels on the core's scale; results: 1 won against, -1 lost
   to, 0 equal), kept inside +-200, doubled when games_played is under AE_SOCIAL_PLACEMENT_GAMES; the points are kept inside
   0 .. AE_SOCIAL_MATCH_POINTS_MAX. 0 for no opponent */
long ae_social_match_change(long points, long games_played, int const *levels, int const *results, int count);
/* the points after a change, kept inside 0 .. AE_SOCIAL_MATCH_POINTS_MAX */
long ae_social_match_apply(long points, long change);

/* a new playlist's hidden rating for a profile's calibration answer (AE_SOCIAL_EXPERIENCE_*): the same-skill tier of EASY,
   NORMAL or HEROIC; UNSET or out of range gives AE_SOCIAL_MATCH_START */
long ae_social_calibration_start(int experience);
/* the calibration answer's word ("NEW", "SOME", "A LOT"; "" for UNSET), and the bots' skill word it starts at */
char const *ae_social_experience_name(int experience);
char const *ae_social_experience_skill(int experience);
/* a game's change with its kills and deaths: in the first AE_SOCIAL_PLACEMENT_GAMES games of a playlist, kills minus deaths
   add AE_SOCIAL_PLACEMENT_KD_POINTS each (kept inside +-AE_SOCIAL_PLACEMENT_KD_MAX) to the win / loss change, so a new player
   who is wrecked drops fast and a strong one rises fast; after placement the change is as it was. Negative counts are 0 */
long ae_social_placement_change(long change, long games_played, int kills, int deaths);
/* a split screen party's hidden rating: the average of its players' (rounded down); AE_SOCIAL_MATCH_START for none */
long ae_social_party_points(long const *points, int count);

/* the window of a level (the core's default): the lowest and highest tier-ladder levels (shown) a player at this raw level may
   meet */
void ae_social_match_window(int raw_level, int *low_shown, int *high_shown);

/* ---- the roster ---- */

struct ae_social_shape
{
	int teams;      /* 0: a free for all of size players; else the number of teams */
	int size;       /* players on a team (or in the free for all) */
};

struct ae_social_seat
{
	int human;      /* 1 for a local player, 0 a bot */
	int tier;       /* a bot's tier 1 .. 19; a human's: the tier nearest his level */
	int shooting;   /* a bot's skill steps 1 .. 7 (0 for a human) */
	int movement;
	int level;      /* shown level: the bot's tier level, the human's own match level */
	int team;       /* 0 .. teams - 1; a free for all: the seat's index */
	int join_step;  /* the search step at which a bot may join (0 .. span; span + 1: after the last step); 0 for humans */
};

struct ae_social_roster
{
	int count;      /* seats in all; the humans first */
	int humans;
	int teams;
	int span;       /* the search's steps (ae_rating_search.span) */
	int player_tier;
	int player_level;   /* shown */
	int fixed;      /* 0 AUTO, else the pinned skill step */
	long cost;      /* the teams' balance cost (0 in a free for all) */
	struct ae_social_seat seat[AE_SOCIAL_SEATS];
};

/* the number of seats of a shape; 0 when it does not fit AE_SOCIAL_SEATS or has no seat */
int ae_social_shape_seats(struct ae_social_shape const *shape);

/* a roster for the shape: humans local players (1 .. 4, one party, one team) at the hidden rating match_points, in AUTO
   mode (fixed 0) or pinned at that skill step (1 .. 7). Seeded: the same arguments give the same roster. 1, 0 for a shape or
   a count that does not fit */
int ae_social_roster_draw(struct ae_social_shape const *shape, int humans, long match_points, int fixed, unsigned long seed,
	struct ae_social_roster *roster);

/* the roster as one log line: "roster team-slayer seed 7: you L17 T7; red T7 T6 T7; blue T7 T8 T7 T9" (the bots' tiers by team,
   the humans' own as "you"; a free for all: "players T4 T5 ..."). The length; the text is cut to fit size */
int ae_social_roster_text(struct ae_social_roster const *roster, char const *playlist, unsigned long seed, char *text, int size);

/* the seats' order of joining: seat indexes of the bots, nearest search step first (ties by seat index). Returns the count */
int ae_social_roster_join_order(struct ae_social_roster const *roster, int *order);

#endif
