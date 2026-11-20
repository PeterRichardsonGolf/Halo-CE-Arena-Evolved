/*
AE_BOTS_SEAT.H

Per-bot SHOOTING and MOVEMENT skill (0.9.3; the rules: ae_bots_seat_rules.h; notes/specs/2026-11-15-social-career.md
6.4). A bot SEAT is a bot's number - 1 (its machine - BOTS_FIRST_MACHINE: the first bot of a game is seat 0, its log's
bot=1). A seat with a pair plays its own shooting and movement steps (1 RECRUIT .. 7 DEMON) whatever BOT SKILL,
SHOOTING SKILL and MOVEMENT SKILL say: its BOT SKILL is its shooting step's (ae_bots_seat.inc). A seat without one plays
the settings as before, so a game no one set a pair for is unchanged. The pairs stay until cleared (their setter's
job: the social glue clears them when it restores the settings; DEV MODE's botskill command). Each bot's pair is logged
once a game ("bots: skill: bot 3 shooting LEGENDARY movement ELITE (its own)").
*/

#ifndef __AE_BOTS_SEAT_H
#define __AE_BOTS_SEAT_H

#include "cseries.h"

/* a seat's pair (steps 1-7): TRUE, or FALSE (nothing changed) for a seat or a step out of range */
boolean ae_bots_seat_skill_set(long seat, int shooting_step, int movement_step);
/* one seat, or every seat, back to the settings */
void ae_bots_seat_skill_unset(long seat);
void ae_bots_seat_skill_clear(void);
/* a seat's levels (AE_BOTS_LEVEL_RECRUIT 0 .. AE_BOTS_LEVEL_DEMON 6): TRUE, or FALSE when it plays the settings */
boolean ae_bots_seat_skill_levels(long seat, int *shooting_level, int *movement_level);
/* how many seats hold a pair */
int ae_bots_seat_skill_count(void);

/* (0.9.3) a seat's PLAY STYLE (ae_bots_style_rules.h: aggression, jumpiness, patience, a favourite weapon; mild, the
strength stays the skill's): set (FALSE for a seat out of range), one seat or every seat back to none (today's
behaviour), and a seat's (FALSE when none). Kept until cleared, as the pairs are; logged once a game ("bots: style:
bot 3 PUSHER ...") */
struct ae_bots_style;
boolean ae_bots_seat_style_set(long seat, struct ae_bots_style const *style);
void ae_bots_seat_style_unset(long seat);
void ae_bots_seat_style_clear(void);
boolean ae_bots_seat_style_get(long seat, struct ae_bots_style *style);

/* (0.9.3) a seat's NAME (AE TOURNAMENT: the lobby's name for that bot, so the scoreboard, kill feed and post-game say the
name the lobby showed): set (FALSE for a seat out of range or an empty name; cut to AE_BOTS_SEAT_NAME - 1 letters), every
seat back to none (bot_names.txt or the built-in names, as before), and a seat's into name (FALSE when none). Kept until
cleared, as the pairs are: the social glue clears them when it restores the settings. ae_bot_names_name reads it first */
#define AE_BOTS_SEAT_NAME 16
boolean ae_bots_seat_name_set(long seat, char const *name);
void ae_bots_seat_name_clear(void);
boolean ae_bots_seat_name_get(long seat, char *name, int size);

#endif
