#ifndef AE_TEAM_WAIT_H
#define AE_TEAM_WAIT_H

/*
AE_TEAM_WAIT.H

Wait for players (ae_team_wait.c): AE's team gametypes go on for a minute when a team is emptied by players leaving.
*/

#include "cseries.h"

/* game_engine_should_end_game's AE hook: TRUE keeps the game going although fewer than two teams can still play */
boolean ae_team_wait_holds_end(void);
/* a new game: no wait carried over */
void ae_team_wait_reset(void);

#endif
