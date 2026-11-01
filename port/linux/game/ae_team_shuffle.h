/*
AE_TEAM_SHUFFLE.H

SHUFFLE (the PC lobby's button, beside SWITCH TEAM): the host deals the
lobby's players onto the two teams at random, evenly (one more on red with
an odd count), as one press; the stock AUTO TEAM BALANCE only moves the
bigger team's last players. Host only, in a team game, before the game
starts; the button is hidden otherwise.
*/

#ifndef AE_TEAM_SHUFFLE_H
#define AE_TEAM_SHUFFLE_H

/* (menu_functions.c, "ae lobby shuffle teams") TRUE when the teams were dealt again */
int ae_team_shuffle(void);
/* (menu_functions.c, the lobby's refresh) whether the button shows: this machine hosts a team game's open lobby */
int ae_team_shuffle_shown(void);
/* (the most players dealt: the port's network players) */
#define AE_TEAM_SHUFFLE_MAXIMUM 128

/* the teams for count players in their lobby order, dealt by a shuffle from seed: teams[i] is 0 or 1, half each
(an odd one out on either, dealt too) (pure: ae_team_shuffle_rules.c) */
void ae_team_shuffle_deal(int count, unsigned long seed, signed char *teams);

#endif
