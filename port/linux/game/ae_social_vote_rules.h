/*
AE_SOCIAL_VOTE_RULES.H

The map vote of the AE TOURNAMENT lobby: before a game the lobby offers up to three maps of the playlist, every seat votes, the
most votes wins. Pure code (no engine headers): port/linux/tests/ae_social_vote_rules_test.c runs it. The caller hands it the
milliseconds since the vote opened; nothing here reads a clock.

- CHOICES: up to AE_SOCIAL_VOTE_CHOICES distinct maps of the playlist's candidates (present maps only, the playlist's own rules),
  drawn by the entries' weights, never the map just played while others are on offer. Fewer than two distinct maps: no vote.
- LENGTH: AE_SOCIAL_VOTE_MS. It ends AE_SOCIAL_VOTE_LINGER_MS after every seat has voted.
- BOTS: each bot votes once, at a seeded time between AE_SOCIAL_VOTE_BOT_MIN_MS and the minimum plus the spread, for a choice
  drawn by the choices' weights (seeded per bot). Bots never change a vote.
- PLAYERS: a vote moves with a later vote for another choice; the same choice again changes nothing.
- WINNER: the most votes; a tie is drawn at random from the tied choices; no votes at all: drawn from all of them.
*/

#ifndef __AE_SOCIAL_VOTE_RULES_H
#define __AE_SOCIAL_VOTE_RULES_H

#include "ae_social_match_rules.h"
#include "ae_social_playlists.h"

enum
{
	AE_SOCIAL_VOTE_CHOICES = 3,
	AE_SOCIAL_VOTE_MS = 10000,
	AE_SOCIAL_VOTE_BOT_MIN_MS = 1500,
	AE_SOCIAL_VOTE_BOT_SPREAD_MS = 5501,    /* the latest bot votes at 7000 ms */
	AE_SOCIAL_VOTE_LINGER_MS = 1000,
	AE_SOCIAL_VOTE_WINNER_MS = 2000,        /* the winner stays lit this long before the lobby counts down */
	AE_SOCIAL_VOTE_FRESH_MS = 1500          /* a bot's vote is "new" (the roster's edge) this long */
};

struct ae_social_vote_choice
{
	int entry;                              /* the playlist's entry index */
	char map[AE_SOCIAL_MAP];
	char gametype[AE_SOCIAL_GAMETYPE];
	int weight;                             /* the entry's weight 1 .. 5 */
};

struct ae_social_vote
{
	int count;
	struct ae_social_vote_choice choice[AE_SOCIAL_VOTE_CHOICES];
	int seats, humans;                      /* humans: seats 0 .. humans - 1; the rest are bots */
	int ballot[AE_SOCIAL_SEATS];            /* the choice each seat voted for, -1 none */
	long cast_ms[AE_SOCIAL_SEATS];
	int cast_order[AE_SOCIAL_SEATS];        /* seats in the order their (last) vote was cast */
	int cast_count;
	long bot_ms[AE_SOCIAL_SEATS];           /* when a bot votes */
	int bot_pick[AE_SOCIAL_SEATS];
	long elapsed;
	long all_ms;                            /* when the last seat voted (every seat has), -1 while some have not */
	int closed;
	int winner, tie;                        /* tie: 1 when the winner was drawn from two or more */
	unsigned long seed;
};

/* up to max choices (at most AE_SOCIAL_VOTE_CHOICES) from the playlist; last: the entry of the map just played (-1 none). The
   count: 0 or 1 means there is nothing to vote on */
int ae_social_vote_pick_choices(struct ae_social_playlist const *playlist, ae_social_present_fn present, void *context, int last,
	unsigned long seed, struct ae_social_vote_choice *out, int max);

/* a vote on these choices; seats seats of which humans are people; bots' times and picks drawn from the seed */
void ae_social_vote_init(struct ae_social_vote *vote, struct ae_social_vote_choice const *choices, int count, int seats, int humans,
	unsigned long seed);
/* a seat votes (a human's: bots vote by themselves); 1 when it counted (new or moved), 0 for the same choice, a bad seat or
   choice, a bot's seat or a vote already closed */
int ae_social_vote_cast(struct ae_social_vote *vote, int seat, int choice);
/* the time since the vote opened (never back): bots whose time has come vote; the vote closes at the length, or the linger
   after the last seat voted */
void ae_social_vote_advance(struct ae_social_vote *vote, long elapsed_ms);
/* ends it now: the bots that have not voted vote, the winner is decided. 1 the first time, 0 when it was closed already */
int ae_social_vote_close(struct ae_social_vote *vote);
/* DEV MODE: bots vote at once, counts[choice] of them for each choice (up to the bots there are, in seat order) */
void ae_social_vote_preset(struct ae_social_vote *vote, int const *counts, int count);

long ae_social_vote_left_ms(struct ae_social_vote const *vote);
int ae_social_vote_voted(struct ae_social_vote const *vote);                    /* seats that have voted */
int ae_social_vote_count(struct ae_social_vote const *vote, int choice);        /* votes for a choice */
/* the index-th seat to have voted for a choice, in the order cast (-1 past the last) */
int ae_social_vote_voter(struct ae_social_vote const *vote, int choice, int index);
/* a bot voted in the last AE_SOCIAL_VOTE_FRESH_MS (the roster's ice edge) */
int ae_social_vote_fresh(struct ae_social_vote const *vote, int seat);

#endif
