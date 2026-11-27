/*
AE_SCREEN_LEADERBOARD.H

The LEADERBOARD screen (ae_screen_leaderboard.c): a campaign mission's best runs of every profile on this PC (each profile's
campaign_records.txt: Content Evolved's local leaderboard, not its online one), best score first or best time first. LB and RB
move to the previous and next mission.
*/

#ifndef __AE_SCREEN_LEADERBOARD_H
#define __AE_SCREEN_LEADERBOARD_H

/* opens LEADERBOARD over the stack on a stock level (0 to 9) */
int ae_screen_leaderboard_open(short level);
/* whether any profile of this PC has a record of the level (the campaign screen's Y is offered only then) */
int ae_screen_leaderboard_has_runs(short level);

/* (DEV MODE `menu leaderboard [<1-10>] [empty]`) four made-up runs on a level (none: empty) */
int ae_screen_leaderboard_open_staged(short level, int empty);

#endif
