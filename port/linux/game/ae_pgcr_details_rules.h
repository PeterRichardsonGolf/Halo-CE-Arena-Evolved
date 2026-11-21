/*
AE_PGCR_DETAILS_RULES.H

(rc2) The post-game report's DETAILS page as pure rules (port/linux/tests/ae_pgcr_details_rules_test.c runs them): one
player's game in full, from the report's row and the host's summary (Y on CARNAGE opens it for the cursor's player; LB / RB
step player to player in the board's order; B returns). Its panes: THIS GAME, ACCURACY AND DAMAGE (two columns of five), KILL
KINDS, then TEAMS (a free for all: the top three players), the game kind's block (FLAG, BALL, HILL, BOMB, RACE; a game with none
shows the player's MEDALS) and ASSISTS. A number the report does not have, because a host that is not Arena Evolved sent no
summary or an older build's summary lacks it, is the dash (ae_record_view.h's words), never a zero.
*/

#ifndef __AE_PGCR_DETAILS_RULES_H
#define __AE_PGCR_DETAILS_RULES_H

#include "ae_pgcr_screen_rules.h"
#include "ae_record_view.h"

enum
{
	AE_PGD_THIS_GAME = 8,
	AE_PGD_ACCURACY = 10,
	AE_PGD_KINDS = 7,
	AE_PGD_MODE = 5,
	AE_PGD_ASSISTS = 4,
	AE_PGD_LINES = 3,
	AE_PGD_TITLE = 16
};

/* THIS GAME: kills, deaths, kill spread, assists, K/D, KDA, best spree, multikills */
int ae_pgd_this_game(struct ae_pgr_row const *row, struct ae_rv_cell *out);
/* ACCURACY AND DAMAGE: shots fired, landed, missed, accuracy, headshot kills; then damage dealt, taken, ratio, dealt per hit,
taken per life (the damage taken over the deaths, at least one) */
int ae_pgd_accuracy_damage(struct ae_pgr_row const *row, struct ae_rv_cell *out);
/* KILL KINDS: headshots, melee, power weapons, grenades, assassinations, splatters, sticks */
int ae_pgd_kill_kinds(struct ae_pgr_row const *row, struct ae_rv_cell *out);
/* the game kind's block: its title and rows (the count); a game of no kind with numbers shows MEDALS */
int ae_pgd_mode_block(struct ae_pgr_report const *report, struct ae_pgr_row const *row, char *title, struct ae_rv_cell *out);
/* ASSISTS: assists, wingman, driver assists, protector (this game's medals) */
int ae_pgd_assists(struct ae_pgr_row const *row, struct ae_rv_cell *out);

/* a TEAMS line: a team's totals (kills, deaths, assists, accuracy, damage) or, in a free for all, a player's */
struct ae_pgd_line
{
	unsigned int chip;
	char name[AE_PGR_NAME];
	char k[16], d[16], a[16], acc[16], damage[16];
	int me;
};

/* the lines: a team game's two (the winner first, as the board), a free for all's top three players; the title ("TEAMS" or
"TOP PLAYERS") */
int ae_pgd_lines(struct ae_pgr_report const *report, struct ae_pgd_line *out, char *title);

#endif
