/*
AE_GHOST_TUNE.C

THE AE GHOST (ae_ghost_tune.h; the numbers and where it applies: ae_ghost_tune_rules.h). The map's loaded tags are
changed in place: its stock multiplayer ghost's top speed and acceleration (the vehicle's speed block, read through
the AE hook ae_vehicle_definition_speeds in vehicles.c) and its cannon's first trigger (rate of fire, spin-up,
spread; with the first and last rate the same the spin-up has nothing to do). The tags' own numbers are kept when the map loads and put back when a game's rule is off, so a stock
gametype played after an AE one on the same map gets Halo CE's ghost.
*/

#include "cseries.h"
#include "cseries/errors.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "items/weapon_definitions.h"
#include "tag_files/tag_files.h"
#include "units/unit_definitions.h"
#include "units/vehicle_definitions.h"
#include "ae_ghost_tune.h"
#include "ae_ghost_tune_rules.h"
#include "ae_vehicle_death_rules.h"

long tag_loaded(long group_tag, const char *name);
/* vehicles.c (AE hook): the vehicle tag's speed block, four reals: top speed forward, reverse, acceleration,
deceleration */
real *ae_vehicle_definition_speeds(long definition_index);

#define DEGREES(x) ((x) * 0.017453292f)

/* unit_definition::initial_weapons' element (units.c, 0x24 bytes): its weapon first */
struct ae_ghost_tune_weapon
{
	struct tag_reference weapon;
	byte pad[0x24 - sizeof(struct tag_reference)];
};

/* the numbers changed, as the map's tags have them */
struct ae_ghost_tune_numbers
{
	real forward_speed;
	real speed_acceleration;
	real initial_rate_of_fire;
	real final_rate_of_fire;
	real inner_cone_angle;
	real error_lower_bound;
	real error_upper_bound;
};

static struct
{
	long vehicle_definition_index;
	long weapon_definition_index;
	struct ae_ghost_tune_numbers stock;
	boolean tuned;
} ae_ghost_tune = { NONE, NONE };

static struct weapon_trigger_definition *ae_ghost_tune_trigger(void)
{
	struct weapon_definition *weapon = weapon_definition_get(ae_ghost_tune.weapon_definition_index);

	return weapon->weapon.triggers.count < 1 ? NULL :
		TAG_BLOCK_GET_ELEMENT(&weapon->weapon.triggers, 0, struct weapon_trigger_definition);
}

void ae_ghost_tune_new_map(void)
{
	long vehicle_definition_index = tag_loaded(VEHICLE_DEFINITION_TAG, AE_GHOST_TUNE_TAG);
	struct unit_definition *vehicle;
	struct weapon_trigger_definition *trigger;
	real *speeds;

	ae_ghost_tune.vehicle_definition_index = NONE;
	ae_ghost_tune.weapon_definition_index = NONE;
	ae_ghost_tune.tuned = FALSE;
	if (vehicle_definition_index == NONE || !ae_ghost_tune_is_ghost(tag_get_name(vehicle_definition_index)))
		return;
	vehicle = vehicle_definition_get(vehicle_definition_index);
	if (vehicle->unit.initial_weapons.count < 1)
		return;
	ae_ghost_tune.weapon_definition_index = TAG_BLOCK_GET_ELEMENT(&vehicle->unit.initial_weapons, 0,
		struct ae_ghost_tune_weapon)->weapon.index;
	if (ae_ghost_tune.weapon_definition_index == NONE || !(trigger = ae_ghost_tune_trigger()))
	{
		ae_ghost_tune.weapon_definition_index = NONE;
		return;
	}
	ae_ghost_tune.vehicle_definition_index = vehicle_definition_index;
	speeds = ae_vehicle_definition_speeds(vehicle_definition_index);
	ae_ghost_tune.stock.forward_speed = speeds[0];
	ae_ghost_tune.stock.speed_acceleration = speeds[2];
	ae_ghost_tune.stock.initial_rate_of_fire = trigger->initial_rate_of_fire;
	ae_ghost_tune.stock.final_rate_of_fire = trigger->final_rate_of_fire;
	ae_ghost_tune.stock.inner_cone_angle = trigger->projectile_error_inner_cone_angle;
	ae_ghost_tune.stock.error_lower_bound = trigger->projectile_error_angle_lower_bound;
	ae_ghost_tune.stock.error_upper_bound = trigger->projectile_error_angle_upper_bound;
}

static void ae_ghost_tune_put(struct ae_ghost_tune_numbers const *numbers)
{
	struct weapon_trigger_definition *trigger = ae_ghost_tune_trigger();
	real *speeds = ae_vehicle_definition_speeds(ae_ghost_tune.vehicle_definition_index);

	speeds[0] = numbers->forward_speed;
	speeds[2] = numbers->speed_acceleration;
	trigger->initial_rate_of_fire = numbers->initial_rate_of_fire;
	trigger->final_rate_of_fire = numbers->final_rate_of_fire;
	trigger->projectile_error_inner_cone_angle = numbers->inner_cone_angle;
	trigger->projectile_error_angle_lower_bound = numbers->error_lower_bound;
	trigger->projectile_error_angle_upper_bound = numbers->error_upper_bound;
}

void ae_ghost_tune_update(void)
{
	struct game_variant const *variant;
	boolean want;

	if (ae_ghost_tune.vehicle_definition_index == NONE)
		return;
	variant = game_engine_running() ? game_engine_get_variant() : NULL;
	want = variant && ae_vehicle_death_get(variant->universal_variant.flags) == _ae_vehicle_death_on;
	if (want == ae_ghost_tune.tuned)
		return;
	if (want)
	{
		struct ae_ghost_tune_numbers tuned = ae_ghost_tune.stock;

		tuned.forward_speed *= AE_GHOST_TUNE_SPEED_SCALE;
		tuned.speed_acceleration *= AE_GHOST_TUNE_SPEED_SCALE;
		tuned.initial_rate_of_fire = ae_ghost_tune_rate(ae_ghost_tune.stock.final_rate_of_fire);
		tuned.final_rate_of_fire = tuned.initial_rate_of_fire;
		tuned.inner_cone_angle *= AE_GHOST_TUNE_SPREAD_SCALE;
		tuned.error_lower_bound *= AE_GHOST_TUNE_SPREAD_SCALE;
		tuned.error_upper_bound *= AE_GHOST_TUNE_SPREAD_SCALE;
		ae_ghost_tune_put(&tuned);
		error(_error_silent, "ae ghost: AE ghost (speed %.3f, acceleration %.4f, cannon %.1f shots a second, spread "
			"%.2f-%.2f degrees)", (double)tuned.forward_speed, (double)tuned.speed_acceleration,
			(double)tuned.final_rate_of_fire, (double)(tuned.error_lower_bound / DEGREES(1.0f)),
			(double)(tuned.error_upper_bound / DEGREES(1.0f)));
	}
	else
	{
		ae_ghost_tune_put(&ae_ghost_tune.stock);
		error(_error_silent, "ae ghost: the map's own ghost (speed %.3f, cannon %.1f-%.1f shots a second)",
			(double)ae_ghost_tune.stock.forward_speed, (double)ae_ghost_tune.stock.initial_rate_of_fire,
			(double)ae_ghost_tune.stock.final_rate_of_fire);
	}
	ae_ghost_tune.tuned = want;
}
