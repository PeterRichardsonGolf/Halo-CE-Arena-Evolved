/*
AE_REMATCH_RULES.H

The post-game lobby's rules (the NEXT GAME stage of the AE TOURNAMENT lobby): who leaves between games, the "Still there?" counter, the
header's words and the vote's choices. Pure code (no engine headers): port/linux/tests/ae_rematch_rules_test.c runs it; nothing reads a
clock or a file.

- LEAVERS: each seat that may leave (not a local player, not a party member) leaves with AE_REMATCH_LEAVE_PERCENT percent, seeded, at
  most AE_REMATCH_LEAVE_MAX a game and never more than a quarter of the seats. A fill-in of the leaver's tier takes the seat.
- STILL THERE: after AE_REMATCH_IDLE_GAMES auto-started games in a row with no input from a local seat the countdown stops at the
  vote's end and asks; any input starts the count again.
- VOTE: three choices, the first SAME AGAIN (the map just played, the one explicit way to replay it), then two picks.
*/

#ifndef __AE_REMATCH_RULES_H
#define __AE_REMATCH_RULES_H

#include "ae_social_lobby_rules.h"

enum
{
	AE_REMATCH_LEAVE_PERCENT = 15,
	AE_REMATCH_LEAVE_MAX = 2,
	AE_REMATCH_IDLE_GAMES = 2,
	AE_REMATCH_VOTE_MS = 20000,
	AE_REMATCH_NEWS_LINES = 3
};

/* the seats that leave (indexes, up to max, ascending): may_leave[i] 1 for a seat that can; count seats in all */
int ae_rematch_leavers(unsigned long seed, int game, int count, int const *may_leave, int *out, int max);

struct ae_still_there
{
	int idle_games;             /* auto-started games in a row without input */
	int asked;                  /* the question is up */
};

void ae_still_there_init(struct ae_still_there *state);
/* a local seat pressed something: the count starts again and the question goes */
void ae_still_there_input(struct ae_still_there *state);
/* the vote ended and the next game starts by itself (input_seen: a local seat pressed something during this lobby). 1 when the game
   starts, 0 when the lobby stops to ask (asked is set) */
int ae_still_there_auto_start(struct ae_still_there *state, int input_seen);
/* the question is up: the lobby waits for A */
int ae_still_there_asking(struct ae_still_there const *state);

/* the rows' counts: "5 of 8 staying" */
void ae_rematch_staying_text(int staying, int total, char *out, int size);
/* the sub line under NEXT GAME IN n: "Rematch / your party of 3 stays / votes in 5 / 8" (online: "5 of 8 staying" in the place of
   "Rematch"); party 0 or 1: no party part; voters 0: no votes part */
void ae_rematch_sub_text(int online, int party, int staying, int total, int votes, int voters, char *out, int size);

/* the news lines under the vote */
enum ae_news_kind { AE_NEWS_LEFT, AE_NEWS_JOINED_BOT, AE_NEWS_JOINED, AE_NEWS_STAYING, AE_NEWS_INVITE_SENT, AE_NEWS_PARTY_JOINED, AE_NEWS_PARTY_LEFT,
	AE_NEWS_PARTY_FULL };
void ae_rematch_news(int kind, char const *name, int tier, char *out, int size);

/* a fill-in's name for a leaver: the first name of the seeded pool (ae_social_lobby_names, at most AE_SOCIAL_SEATS of them) that none of the
count names in taken (24 letters each) has. 1 and the name in out (24 letters), 0 when every name of the pool is taken */
int ae_rematch_fill_in(unsigned long seed, char const taken[][24], int count, char *out);

/* the vote's header word of a choice: "SAME AGAIN" for the first choice of a rematch vote, else "" */
char const *ae_rematch_choice_word(int index);
#endif
