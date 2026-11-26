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
/* the host's Firefight rules over ae_net_extensions' packet: the host's current rules as values with their wire widths
(0 when maximum is too small), the widths alone, the rules a client received applied for its game (held to what the game
reads), cleared when that game or its session ends, and whether a received set is in use */
int ae_firefight_net_rules(unsigned long *values, unsigned char *widths, int maximum);
int ae_firefight_net_widths(unsigned char *widths, int maximum);
void ae_firefight_net_apply(unsigned long const *values, int count);
void ae_firefight_net_clear(void);
int ae_firefight_net_active(void);
/* the bots take nothing off a Firefight score here */
float bots_score_multiplier_penalty(void);

#endif
