/*
AE_RADAR_RULES.C

PLAYERS ON RADAR ENEMIES (ae_radar_rules.h): pure rules.
*/

#include "ae_radar_rules.h"

int ae_radar_rules_shows(int enemies_only, int team_game, int viewer_known, int names, enum ae_radar_contact contact)
{
	if (!enemies_only || !team_game || !viewer_known || names)
		return 1;
	return contact != AE_RADAR_CONTACT_ALLY;
}
