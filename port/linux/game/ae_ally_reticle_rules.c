/*
AE_ALLY_RETICLE_RULES.C

Pure rules for the ALLY RETICLE (ae_ally_reticle_rules.h). No engine includes.
*/

#include "ae_ally_reticle_rules.h"

int ae_ally_reticle_rules_is_ally(int is_self, int dead, int team_game, int side_game, int same_team,
	int engine_enemy)
{
	if (is_self || dead || engine_enemy)
		return 0;
	if (team_game)
		return same_team != 0;
	return side_game != 0;
}

int ae_ally_reticle_rules_green(int setting_on, int ally, int aim_frame)
{
	return setting_on && ally && aim_frame == 0;
}

unsigned long ae_ally_reticle_rules_color(unsigned long color)
{
	return (color & 0xFF000000ul) | AE_ALLY_RETICLE_RGB;
}
