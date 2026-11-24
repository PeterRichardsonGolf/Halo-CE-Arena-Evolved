/*
AE_FIREFIGHT_GLUE.H

What Firefight (firefight*.c, from Content Evolved) asks of the campaign's
score, and of the stats and rank screens this build leaves out: the score's
names mapped to AE's own (ae_campaign_score.h), and the rest answered with
nothing (ae_firefight_glue.c).
*/

#ifndef __AE_FIREFIGHT_GLUE_H
#define __AE_FIREFIGHT_GLUE_H

#include "ae_campaign_score.h"

#define campaign_score_skulls_multiplier() ae_campaign_score_skulls_multiplier()
#define campaign_score_team_points() ae_campaign_score_team_points()
#define campaign_score_player_points(player_index) ae_campaign_score_player_points(player_index)
#define campaign_score_player_kills(player_index) ae_campaign_score_player_kills(player_index)
#define campaign_score_player_deaths(player_index) ae_campaign_score_player_deaths(player_index)
#define campaign_score_level_won() ae_campaign_score_level_won()
#define campaign_score_award_points(player_index, points) ae_campaign_score_award(player_index, points)
void campaign_score_player_display_name(long player_index, wchar_t *name, long count);
/* the bots take nothing off a Firefight score here */
float bots_score_multiplier_penalty(void);

#endif
