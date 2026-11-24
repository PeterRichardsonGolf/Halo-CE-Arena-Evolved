/*
AE_BOTS_SEAT_RULES.H

Per-bot SHOOTING and MOVEMENT skill (0.9.3, the offline social career's spread: notes/specs/2026-11-15-social-career.md
6.4; owner ask-me 2026-11-15 "Accept all"), as pure rules: a table of bot seats (a bot's number - 1, its machine -
BOTS_FIRST_MACHINE) that may each hold a shooting step and a movement step of their own. Steps are 1 (RECRUIT) to 7
(DEMON); a seat's levels are the step - 1 (AE_BOTS_LEVEL_RECRUIT 0 .. AE_BOTS_LEVEL_DEMON 6). A seat without a pair
plays the settings (BOT SKILL, SHOOTING SKILL, MOVEMENT SKILL) as before. No engine includes:
port/linux/tests/ae_bots_seat_rules_test.c compiles these alone.
*/

#ifndef __AE_BOTS_SEAT_RULES_H
#define __AE_BOTS_SEAT_RULES_H

#define AE_SEAT_MOST 128
#define AE_SEAT_STEPS 7

struct ae_seat_table
{
	/* each seat's steps, 0: none (the settings) */
	unsigned char shooting[AE_SEAT_MOST];
	unsigned char movement[AE_SEAT_MOST];
};

/* every seat back to the settings; one seat back */
void ae_seat_clear(struct ae_seat_table *table);
void ae_seat_unset(struct ae_seat_table *table, int seat);
/* a seat's pair (steps 1-7 each): 1, or 0 (nothing changed) for a seat or a step out of range */
int ae_seat_set(struct ae_seat_table *table, int seat, int shooting_step, int movement_step);
/* a seat's levels (0-6): 1, or 0 when it has no pair */
int ae_seat_levels(struct ae_seat_table const *table, int seat, int *shooting_level, int *movement_level);
/* how many seats hold a pair */
int ae_seat_count(struct ae_seat_table const *table);

/* a step from its name ("recruit" .. "demon", any case) or its number ("1" .. "7"): 1-7, or 0 */
int ae_seat_step_of_name(char const *name);
/* a step's or a level's name ("RECRUIT" .. "DEMON"; "?" out of range) */
char const *ae_seat_step_name(int step);
char const *ae_seat_level_name(int level);

#endif
