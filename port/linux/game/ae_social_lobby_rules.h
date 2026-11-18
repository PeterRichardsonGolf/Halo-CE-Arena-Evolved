/*
AE_SOCIAL_LOBBY_RULES.H

The social lobby's clock (0.9.3): the stages of "finding" an offline game, when each bot seat joins, and the bots' names.
Pure code (no engine headers): port/linux/tests/ae_social_lobby_rules_test.c runs it. The screen (a later step) draws what
this says; nothing here reads a clock or a file, the caller hands it the milliseconds that passed.

Stages: SEARCHING (1.5 to 2.5 s, seeded, or until the search's range is full: the range widens one step every 250 ms), FOUND
(the bots join nearest tier first, a seeded 200 to 600 ms apart, all inside AE_SOCIAL_LOBBY_CAP_MS of the lobby's start),
STARTING (5 s: the teams are set), then GO (the game loads). A skips to GO at any stage; B leaves (CANCELLED: nothing is
recorded). A FIXED lobby has no range strip (its search is only a pause).
*/

#ifndef __AE_SOCIAL_LOBBY_RULES_H
#define __AE_SOCIAL_LOBBY_RULES_H

#include "ae_social_match_rules.h"

enum
{
	AE_SOCIAL_STAGE_SEARCHING = 0,
	AE_SOCIAL_STAGE_FOUND = 1,
	AE_SOCIAL_STAGE_STARTING = 2,
	AE_SOCIAL_STAGE_GO = 3,
	AE_SOCIAL_STAGE_CANCELLED = 4,
	AE_SOCIAL_LOBBY_STEP_MS = 250,
	AE_SOCIAL_LOBBY_SEARCH_MIN_MS = 1500,
	AE_SOCIAL_LOBBY_SEARCH_SPREAD_MS = 1001,
	AE_SOCIAL_LOBBY_GAP_MIN_MS = 200,
	AE_SOCIAL_LOBBY_GAP_SPREAD_MS = 401,
	AE_SOCIAL_LOBBY_CAP_MS = 8000,
	AE_SOCIAL_LOBBY_STARTING_MS = 5000,
	AE_SOCIAL_LOBBY_NAME = 24
};

struct ae_social_lobby
{
	int stage;
	long elapsed;               /* ms since the lobby opened */
	int seats, humans, fixed;
	int span;                   /* the range's steps (0 .. span); the strip's full width */
	int tier_level;             /* the player's level on the core's scale: the range's centre */
	long found_ms;              /* when the search ends */
	long starting_ms;           /* when the last seat has joined and STARTING begins */
	long join_ms[AE_SOCIAL_SEATS];  /* when each seat is in; humans 0 */
	int order[AE_SOCIAL_SEATS]; /* the bots in joining order (seat indexes) */
	int order_count;
	int forced;                 /* A was pressed: GO now */
};

/* a lobby for a roster; seeded (the roster's own seed is a good one). Stage SEARCHING at 0 ms */
void ae_social_lobby_init(struct ae_social_lobby *lobby, struct ae_social_roster const *roster, unsigned long seed);
/* time passed (ms >= 0): the stage follows */
void ae_social_lobby_advance(struct ae_social_lobby *lobby, long ms);
/* A: the lobby fills and the game starts (stage GO). 0 when it was over already (GO or CANCELLED) */
int ae_social_lobby_skip(struct ae_social_lobby *lobby);
/* B: leave (CANCELLED); 0 when the game had started (GO) or it was over already */
int ae_social_lobby_cancel(struct ae_social_lobby *lobby);

/* the range step the strip shows: 0 .. span, growing while SEARCHING, the step it got to after; 0 for FIXED */
int ae_social_lobby_step(struct ae_social_lobby const *lobby);
/* seats in the lobby now (humans and the bots that joined), all of them from STARTING on */
int ae_social_lobby_joined(struct ae_social_lobby const *lobby);
/* whether a seat has joined (1) and, for a bot, whether it joined in the last AE_SOCIAL_LOBBY_STEP_MS * 6 ms (1 for the
   newest bot's "JOINED" edge; 0 for the others) */
int ae_social_lobby_seat_in(struct ae_social_lobby const *lobby, int seat);
int ae_social_lobby_seat_new(struct ae_social_lobby const *lobby, int seat);
/* ms left of STARTING (0 outside it) */
long ae_social_lobby_countdown_ms(struct ae_social_lobby const *lobby);

/* the names of count bots (seeded, no repeats) into out, each up to AE_SOCIAL_LOBBY_NAME letters, from the bots' name
   pool (ae_social_names_rules.h; "BOT n" only if it ran short). 1, 0 for a count over AE_SOCIAL_SEATS */
int ae_social_lobby_names(unsigned long seed, int count, char out[][AE_SOCIAL_LOBBY_NAME]);

#endif
