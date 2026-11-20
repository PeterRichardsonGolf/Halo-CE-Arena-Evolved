/*
AE_RADAR.C

PLAYERS ON RADAR ENEMIES (ae_radar.h; the rules: ae_radar_rules.c): the motion
sensor's filter for the gametype's value 3. Each machine filters its own
sensor from the PC options the host sent (network_game's variant_options): no
network message is added or changed.
*/

#include "cseries.h"
#include "game/game.h"
#include "game/game_allegiance.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "objects/objects.h"
#include "units/units.h"
#include "units/vehicles.h"

#include "ae_radar.h"
#include "ae_radar_rules.h"

/* (ae_radar_name_shown's call of game_engine_draw_object_in_motion_sensor) */
static boolean ae_radar_names;

/* the unit's side for the viewer's team, as motion_sensor.c's blip_type_get
tells it: a vehicle by its gunner, else its driver, else empty */
static enum ae_radar_contact ae_radar_contact_of(long unit_index, short viewer_team)
{
	struct unit_datum *unit = unit_try_and_get(unit_index);
	long seat_index;
	struct unit_datum *seat;

	if (!unit)
		return AE_RADAR_CONTACT_ENEMY;
	if (vehicle_try_and_get(unit_index))
	{
		seat_index = unit->unit.gunner_object_index != NONE ? unit->unit.gunner_object_index :
			unit->unit.driver_object_index;
		seat = seat_index != NONE ? unit_try_and_get(seat_index) : NULL;
		if (!seat)
			return AE_RADAR_CONTACT_EMPTY_VEHICLE;
		unit = seat;
	}
	return game_team_is_enemy(unit->object.owner_team_index, viewer_team) ? AE_RADAR_CONTACT_ENEMY :
		AE_RADAR_CONTACT_ALLY;
}

boolean ae_radar_draw_object(boolean draw_object, long unit_index, short local_player_index)
{
	boolean enemies_only;
	long player_index;
	struct player_datum *viewer;
	enum ae_radar_contact contact = AE_RADAR_CONTACT_ENEMY;

	if (!draw_object || !game_engine_running())
		return draw_object;
	enemies_only = game_variant_options_get()->radar_players == _radar_players_enemies;
	if (!enemies_only)
		return draw_object;
	player_index = local_player_index != NONE ? local_player_get_player_index(local_player_index) : NONE;
	viewer = player_index != NONE ? player_try_and_get(player_index) : NULL;
	if (viewer)
		contact = ae_radar_contact_of(unit_index, viewer->team_index);
	return (boolean)ae_radar_rules_shows(enemies_only, game_engine_has_teams(), viewer != NULL, ae_radar_names, contact);
}

boolean ae_radar_name_shown(long unit_index)
{
	boolean shown;

	ae_radar_names = TRUE;
	shown = game_engine_draw_object_in_motion_sensor(unit_index);
	ae_radar_names = FALSE;
	return shown;
}
