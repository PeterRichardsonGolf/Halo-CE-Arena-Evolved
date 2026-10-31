/*
AE_PAUSE_ROWS.H

The AE pause menu's rows (0.4.0; ae_pause_rows.c, the screen ae_screen_pause.c; the approved design): which rows a player's pause has for the game it is in, each row's label,
help line, look and confirm. Pure: no engine includes (unit test port/linux/tests/ae_pause_rows_test.c).

A multiplayer game: RESUME, SETTINGS, CHANGE TEAMS (a team game's), SCOREBOARD, then the host's rows under "GAME
(HOST)": GAME OPTIONS and CHANGE MAP (an AE lobby's game: the next game's), RESTART GAME (an AE lobby's game: again
at once with those) and RESTART ROUND (a round game's), BOTS, END GAME; then QUIT. A campaign:
RESUME, SETTINGS, REVERT TO SAVED, SAVE AND QUIT; network co-op: RESUME, SETTINGS, FRIENDLY FIRE, QUIT (the classic
pause's own rows: co-op has no revert). A multiplayer map played alone: RESUME, SETTINGS, QUIT.
*/

#ifndef __AE_PAUSE_ROWS_H
#define __AE_PAUSE_ROWS_H

enum
{
	AE_PAUSE_RESUME, AE_PAUSE_SETTINGS, AE_PAUSE_TEAMS, AE_PAUSE_SCOREBOARD, AE_PAUSE_HOST_HEADER, AE_PAUSE_OPTIONS,
	AE_PAUSE_MAP, AE_PAUSE_BOTS, AE_PAUSE_END, AE_PAUSE_FRIENDLY_FIRE, AE_PAUSE_REVERT, AE_PAUSE_QUIT,
	AE_PAUSE_RESTART_GAME, AE_PAUSE_RESTART_ROUND,
	AE_PAUSE_ROW_KINDS,
	AE_PAUSE_ROWS_MAXIMUM = 14
};
/* the game the pause is over */
enum { AE_PAUSE_MULTIPLAYER, AE_PAUSE_CAMPAIGN, AE_PAUSE_COOP, AE_PAUSE_ALONE };
/* a row's look: a group header (not focusable), an action (A does it), a sub-screen (a chevron, its value muted), a
value stepped in place (left / right; A steps it too) */
enum { AE_PAUSE_LOOK_HEADER, AE_PAUSE_LOOK_ACTION, AE_PAUSE_LOOK_SUB, AE_PAUSE_LOOK_VALUE };

struct ae_pause_facts
{
	int game;              /* AE_PAUSE_MULTIPLAYER .. */
	int host;              /* this PC hosts the game */
	int teams;             /* a team game */
	int lobby;             /* the host's game came from AE's CREATE GAME: its GAME OPTIONS and CHANGE MAP (the next
	                          game's) are there to change */
	int local_players;     /* the players on this PC (QUIT's help) */
	int rounds;            /* a game played in rounds (SEARCH AND DESTROY, Bomb's): RESTART ROUND (0.7.0) */
};

/* the rows in order (headers among them); their count */
short ae_pause_rows(struct ae_pause_facts const *facts, short *rows, short maximum);
const char *ae_pause_row_label(int row, struct ae_pause_facts const *facts);
const char *ae_pause_row_help(int row, struct ae_pause_facts const *facts);
int ae_pause_row_look(int row);
/* whether A asks first (CHANGE TEAMS, END GAME, the RESTARTs, QUIT, REVERT TO SAVED; CHANGE MAP asks once its map is picked), and the dialog's
title, body and its yes (the second choice; CANCEL first, the safe one) */
int ae_pause_row_confirm(int row, struct ae_pause_facts const *facts, const char **title, const char **body,
	const char **yes);
/* a focus that landed on a header moved past it the way it was going (up -1, down 1), else back (the list's ends
do not wrap): the index to focus */
short ae_pause_row_skip(short const *rows, short count, short focus, short direction);

#endif
