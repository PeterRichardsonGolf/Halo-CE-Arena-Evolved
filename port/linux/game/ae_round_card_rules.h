/*
AE_ROUND_CARD_RULES.H

(0.7.9) The ROUND WON / ROUND LOST card's rules: what the card says to a viewer (the verdict, the reason line, the
footer), who the round's MVP is, how the card fades and which builds can read the END tail of the round message
(ae_round_wire.h). Pure: no engine includes; unit-tested (port/linux/tests/ae_round_card_rules_test.c).
*/
#ifndef AE_ROUND_CARD_RULES_H
#define AE_ROUND_CARD_RULES_H

#include "ae_round_wire.h"

enum
{
	/* the card shows from the POST phase's start for this long (the rest of POST shows the game as before) */
	AE_ROUND_CARD_SECONDS = 3,
};

enum ae_round_card_verdict
{
	AE_ROUND_CARD_WON = 0,
	AE_ROUND_CARD_LOST,
	AE_ROUND_CARD_TIED,
	AE_ROUND_CARD_NONE /* no result to show */
};

/* the verdict one viewer reads from the round's last result (the wire's: a side, or a player's slot without teams,
AE_ROUND_WIRE_RESULT_TIE, AE_ROUND_WIRE_RESULT_NONE). viewer_team: the viewer's side (teams), viewer_slot: their absolute
index (no teams); -1 when not known (a viewer in neither side reads LOST) */
int ae_round_card_verdict(unsigned char last_result, int teams, int viewer_team, int viewer_slot);

/* why a round ended (enum ae_round_reason): won says a side (teams) or a player won it, winner their side or slot,
timed_out the round clock ran out, exterminated a respawn round that ended by a wipe (EXTERMINATION) */
int ae_round_card_reason(int won, int winner, int teams, int exterminated, int timed_out);

/* the reason line's words ("BLUE TEAM ELIMINATED"), NULL for none or out of range */
char const *ae_round_card_reason_text(int reason);

/* the round's MVP of count candidates: the most kills, ties the fewer deaths, then a player of the winning side
(winner_side -1: none), then the lowest slot. Returns the candidate's index, -1 when nobody made a kill */
int ae_round_card_mvp(int count, long const *slot, long const *kills, long const *deaths, long const *side,
	long winner_side);

/* the footer: "NEXT  ·  ROUND 4 OF 7" (UTF-8, the middle dot between), "NEXT  ·  ROUND 4" with no round limit
(max_rounds 0). next_round 1 up; starts_in is unused (the next round's own countdown follows the card) */
void ae_round_card_footer(int next_round, int max_rounds, int starts_in, char *out, int size);

/* "n kills" ("1 kill") */
void ae_round_card_kills_text(int kills, char *out, int size);

/* the card's opacity (0-1) at seconds since it began: fade in 0.15 s, hold, fade out 0.25 s ending at
AE_ROUND_CARD_SECONDS, an exponential ease-out on each fade; 0 outside */
float ae_round_card_alpha(float seconds);

/* whether another machine's build (its Delta build name, "Arena Evolved 0.7.9-beta") reads the END tail: 0.7.9 or later,
or a -dev build of 0.7.8 (the branch's own, before VERSION moves on). 1 yes, 0 no */
int ae_round_build_has_card(char const *build);

#endif
