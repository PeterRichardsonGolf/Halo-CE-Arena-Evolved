/*
AE_ALLY_RETICLE.C

ALLY RETICLE (HUD > ALLY RETICLE, display.ally_reticle): CE turns the reticle
red when your aim assist finds an enemy; here it turns green when the unit
under it is an ally (a teammate in a team game,
another player or a marine in co-op and the campaign). Who counts is
ae_ally_reticle_rules.c's.

Only the drawing is added: the game's aim assist only ever looks for enemies,
so this casts its own ray from the view's centre, the way a shot goes (a
projectile's collision test, ignoring your own unit), and reads the team of
the biped it meets. Nothing is sent and no state is kept but a cache of the
setting, so it shows on any host, a stock OpenCE one's too. The colour is
changed on the aim reticle's pieces only, with their own alpha; the red frame
(an enemy under the aim assist) is left as it is.

The idea is aLTis94's ally_retical_modified_by_altis.lua for Chimera (ideas
only: it has no licence, and no code of it is here).
*/

#include "cseries.h"
#include "cseries/errors.h"
#include "math/real_math.h"
#include "camera/observer.h"
#include "game/game.h"
#include "game/game_allegiance.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "objects/objects.h"
#include "physics/collisions.h"
#include "units/bipeds.h"
#include "units/units.h"

#include "ae_ally_reticle_rules.h"
#include "ae_ally_reticle.h"

/* port/linux/src/ae_config_players.c: each player's own (split screen) */
int ae_config_player_boolean(int player, const char *name);
unsigned long ae_config_player_changes(void);

enum
{
	/* how far the reticle's ray reaches (world units), as a shot's aiming ray */
	AE_ALLY_RETICLE_RANGE = 128,
};

static boolean ae_ally_reticle_enabled(
	short local_player_index)
{
	static unsigned long changes = (unsigned long)-1;
	static boolean enabled[MAXIMUM_NUMBER_OF_LOCAL_PLAYERS];

	if (changes != ae_config_player_changes())
	{
		short index;

		changes = ae_config_player_changes();
		for (index = 0; index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS; index++)
			enabled[index] = ae_config_player_boolean(index, "display.ally_reticle") != 0;
	}
	return VALID_INDEX(local_player_index, MAXIMUM_NUMBER_OF_LOCAL_PLAYERS) ? enabled[local_player_index] : enabled[0];
}

/* whether the unit under the view's centre is the local player's ally */
static boolean ae_ally_reticle_target_is_ally(
	short local_player_index)
{
	long player_index = local_player_get_player_index(local_player_index);
	struct player_datum *player;
	struct observer_result const *camera;
	struct collision_result collision;
	real_vector3d ray;
	long object_index;
	struct object_datum *object;

	if (player_index == NONE || (player = player_try_and_get(player_index)) == NULL || player->unit_index == NONE)
		return FALSE;
	camera = observer_get_camera(local_player_index);
	if (!camera)
		return FALSE;
	scale_vector3d(&camera->forward, (real)AE_ALLY_RETICLE_RANGE, &ray);
	if (!collision_test_vector(_collision_test_for_projectiles_flags, &camera->position, &ray, player->unit_index,
		&collision) || collision.type != _collision_result_object || collision.object_index == NONE)
		return FALSE;
	/* (a hit on a weapon or an item a unit holds is the unit's) */
	object_index = object_get_ultimate_parent(collision.object_index);
	if (object_index == NONE)
		return FALSE;
	object = object_get(object_index);
	if (!TEST_FLAG(_object_mask_biped, object->object.type))
		return FALSE;
	return ae_ally_reticle_rules_is_ally(object_index == player->unit_index,
		TEST_FLAG(object->object.damage_flags, _object_dead_bit),
		game_engine_running() && game_engine_has_teams(),
		game_is_cooperative() || !game_engine_running(),
		object->object.owner_team_index == player->team_index,
		game_team_is_enemy(player->team_index, object->object.owner_team_index)) != 0;
}

unsigned long ae_ally_reticle_color(
	short local_player_index,
	short aim_frame,
	unsigned long color)
{
	static long last_time = 0;
	static boolean logged = FALSE;
	long now = game_time_get();
	boolean ally;

	/* (a new game starts the clock again: its first green is logged) */
	if (now < last_time)
		logged = FALSE;
	last_time = now;
	if (!ae_ally_reticle_enabled(local_player_index) || aim_frame != 0)
		return color;
	ally = ae_ally_reticle_target_is_ally(local_player_index);
	if (!ae_ally_reticle_rules_green(TRUE, ally, aim_frame))
		return color;
	if (!logged)
	{
		logged = TRUE;
		error(_error_silent, "ally reticle: green over an ally (local player %d, game time %d)", (int)local_player_index,
			(int)now);
	}
	return ae_ally_reticle_rules_color(color);
}
