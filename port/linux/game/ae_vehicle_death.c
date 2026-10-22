/*
AE_VEHICLE_DEATH.C

SPIKE (throwaway, learning first; not a feature): destructible vehicles in multiplayer. Report outside the repo.

Stock Halo CE multiplayer vehicles cannot die because of their tags: the collision models of the multiplayer
warthog, ghost, scorpion and turret have a maximum body vitality of 0 (object_initialize_vitality gives them no
health), their materials' body damage multipliers are 0, and no depleted/destroyed effect. The engine's own death
path for vehicles (object_deplete_body: riders killed; object_damage_body: regions that die with the object) is
whole: campaign ghosts, banshees and wraiths use it.

With HALO_VEHICLE_DEATH=1 the host (or a local game) gives such a vehicle a health pool on its first hit and counts
its 0 multipliers as 1, so the stock rules decide the rest (only damage that detonates explosives hurts it, the tag
flag "only damaged by explosives"). At 0 the engine's death path runs; this file credits the riders' deaths to the
damage's owner, plays an explosion from the map's own tags, deletes the wreck after a while, and the gametype's
vehicle respawn (game_engine.c) puts a new one on its pad.

Clients take the host's word: the object state's dead bit (network_objects.c) kills a client's copy, which runs
object_deplete_body there too (so an AE client plays the explosion); the wreck's deletion and the new vehicle are
the host's object messages.

HALO_VEHICLE_DEATH_TEST=<seconds> (test driver, host or local game only): at that time the last player is seated in
the nearest vehicle; from 3 seconds on, every 2 seconds a rocket explosion goes off on it, owned by another player.
*/

#include "cseries.h"
#include "cseries/errors.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "objects/objects.h"
#include "objects/damage.h"
#include "effects/effects.h"
#include "physics/collision_model_definitions.h"
#include "tag_files/tag_files.h"
#include "units/units.h"
#include "units/unit_definitions.h"

#include "ae_vehicle_death.h"

#include <stdlib.h>
#include <string.h>

long tag_loaded(long group_tag, const char *name);
boolean network_game_distributed_client(void);

enum
{
	AE_VEHICLE_DEATH_WRECKS = 64,
	/* a wreck stays this long, then the host deletes it (the respawn timer then runs from there) */
	AE_VEHICLE_DEATH_WRECK_TICKS = 10 * TICKS_PER_SECOND,
	/* the respawn time of a gametype whose vehicles never respawn, with the rule on */
	AE_VEHICLE_DEATH_FALLBACK_RESPAWN_TICKS = 30 * TICKS_PER_SECOND,
};

/* the health the rule gives a vehicle (Halo 2's model vitality scaled by the rockets: Halo 2's rocket deals 200 at
its core, Halo CE's 300-330, so x1.65): warthog 250, ghost and banshee 175, scorpion 400, turrets 175 in Halo 2 */
static struct
{
	char const *name;
	real vitality;
} const ae_vehicle_death_table[] = {
	{ "warthog", 410.0f }, /* (the rocket warthog too) */
	{ "ghost", 290.0f },
	{ "banshee", 290.0f },
	{ "scorpion", 660.0f },
	{ "turret", 290.0f },
};

static struct
{
	long object_index;
	long died_at;
} ae_vehicle_death_wrecks[AE_VEHICLE_DEATH_WRECKS];

static struct
{
	long vehicle_index;
	long rider_index;
	long hits;
} ae_vehicle_death_test = { NONE, NONE, 0 };

/* the wrecks of an earlier game forgotten (object indices are the new map's): game time went back */
static void ae_vehicle_death_new_game_check(void)
{
	static long last_time = -1;
	long now = game_time_get();
	short index;

	if (last_time < 0 || now < last_time)
	{
		for (index = 0; index < AE_VEHICLE_DEATH_WRECKS; index++)
			ae_vehicle_death_wrecks[index].object_index = NONE;
		ae_vehicle_death_test.vehicle_index = NONE;
		ae_vehicle_death_test.rider_index = NONE;
		ae_vehicle_death_test.hits = 0;
	}
	last_time = now;
}

static long ae_vehicle_death_effect_index(long object_index);

static boolean ae_vehicle_death_switch(void)
{
	static int on = -1;

	if (on < 0)
	{
		char const *value = getenv("HALO_VEHICLE_DEATH");

		on = value && *value && *value != '0';
	}
	return on;
}

/* the rule decides here: a multiplayer game this machine deals damage in (the host, or a local game) */
static boolean ae_vehicle_death_on(void)
{
	return ae_vehicle_death_switch() && game_engine_running() && !network_game_distributed_client();
}

static real ae_vehicle_death_vitality(long definition_index)
{
	char const *name = tag_get_name(definition_index);
	short index;

	if (!name)
		return 0.0f;
	for (index = 0; index < (short)NUMBEROF(ae_vehicle_death_table); index++)
	{
		if (strstr(name, ae_vehicle_death_table[index].name))
			return ae_vehicle_death_table[index].vitality;
	}
	return 0.0f;
}

static struct collision_model *ae_vehicle_death_collision(long object_index)
{
	struct object_definition *definition = object_definition_get(object_get(object_index)->definition_index);

	return definition->object.collision_model.index == NONE ? NULL :
		collision_model_definition_get(definition->object.collision_model.index);
}

/* a vehicle whose tag gives it no health (a stock multiplayer one) */
static boolean ae_vehicle_death_immortal_tag(long object_index)
{
	struct object_datum *object = object_get(object_index);
	struct collision_model *collision;

	if (object->object.type != _object_type_vehicle)
		return FALSE;
	collision = ae_vehicle_death_collision(object_index);
	return collision && collision->resistance.maximum_body_vitality <= 0.0f;
}

real ae_vehicle_death_body_multiplier(long object_index, real multiplier)
{
	struct object_datum *object = object_get(object_index);

	if (!ae_vehicle_death_on() || !ae_vehicle_death_immortal_tag(object_index))
		return multiplier;
	if (object->object.maximum_body_vitality <= 0.0f)
	{
		real vitality = ae_vehicle_death_vitality(object->definition_index);

		if (vitality <= 0.0f || TEST_FLAG(object->object.damage_flags, _object_dead_bit))
			return multiplier;
		object->object.maximum_body_vitality = vitality;
		object->object.body_vitality = 1.0f;
		error(_error_silent, "AE vehicle death: %s %08lX gets %.0f health", tag_get_name(object->definition_index),
			(unsigned long)object_index, (double)vitality);
	}
	return multiplier > 0.0f ? multiplier : 1.0f;
}

static void ae_vehicle_death_note_wreck(long object_index)
{
	short index;
	short free_index = NONE;

	ae_vehicle_death_new_game_check();
	for (index = 0; index < AE_VEHICLE_DEATH_WRECKS; index++)
	{
		if (ae_vehicle_death_wrecks[index].object_index == object_index)
			return;
		if (free_index == NONE && (ae_vehicle_death_wrecks[index].object_index == NONE ||
			!object_try_and_get(ae_vehicle_death_wrecks[index].object_index)))
			free_index = index;
	}
	if (free_index != NONE)
	{
		ae_vehicle_death_wrecks[free_index].object_index = object_index;
		ae_vehicle_death_wrecks[free_index].died_at = game_time_get();
	}
}

void ae_vehicle_death_depleted(long object_index, struct damage_data const *damage)
{
	struct object_datum *object = object_get(object_index);
	long child_index;

	if (!ae_vehicle_death_on() || !ae_vehicle_death_immortal_tag(object_index) ||
		object->object.maximum_body_vitality <= 0.0f)
	{
		return;
	}
	error(_error_silent, "AE vehicle death: %s %08lX destroyed (damage %s, owner player %08lX object %08lX)",
		tag_get_name(object->definition_index), (unsigned long)object_index, tag_get_name(damage->definition_index),
		(unsigned long)damage->owner_player_index, (unsigned long)damage->owner_object_index);
	/* (object_deplete_body marked the seated bipeds to die by an act of god, a death no one is credited with: the
	destroying damage kills them first, its owner's kill) */
	for (child_index = object->object.first_child_object_index; child_index != NONE;)
	{
		struct object_datum *child = object_get(child_index);
		long next_index = child->object.next_object_index;

		if (child->object.type == _object_type_biped && !TEST_FLAG(child->object.damage_flags, _object_dead_bit) &&
			((struct unit_datum *)child)->unit.parent_seat_index != NONE)
		{
			struct damage_data kill = *damage;
			long rider_player_index = ((struct unit_datum *)child)->unit.player_index;

			SET_FLAG(kill.flags, _damage_kill_instantly_bit, TRUE);
			SET_FLAG(kill.flags, _damage_area_of_effect_bit, FALSE);
			object_cause_damage(&kill, child_index, NONE, NONE, NONE, NULL);
			error(_error_silent, "AE vehicle death: rider %08lX (player %08lX) %s", (unsigned long)child_index,
				(unsigned long)rider_player_index,
				TEST_FLAG(object_get(child_index)->object.damage_flags, _object_dead_bit) ? "killed" : "survived");
		}
		child_index = next_index;
	}
	/* the explosion (the stock tag has none): its own area damage (an effect's damage is its owner object's
	player's) is the destroyer's, so what it kills around the wreck is theirs too */
	{
		long effect_index = ae_vehicle_death_effect_index(object_index);

		object->object.owner_player_index = damage->owner_player_index;
		object->object.owner_team_index = damage->owner_team_index;
		if (effect_index != NONE)
			effect_new_from_object(effect_index, object_index, object_index, NONE, 0.0f, 0.0f, NULL, NULL);
	}
	ae_vehicle_death_note_wreck(object_index);
}

static long ae_vehicle_death_effect_index(long object_index)
{
	static char const *const names[] = {
		"vehicles\\scorpion\\shell explosion",
		"weapons\\rocket launcher\\effects\\rocket explosion",
		"weapons\\frag grenade\\effects\\explosion",
	};
	struct collision_model *collision;
	short index;

	if (!game_engine_running() || object_get(object_index)->object.type != _object_type_vehicle)
		return NONE;
	collision = ae_vehicle_death_collision(object_index);
	if (!collision || collision->resistance.body_depleted_effect.index != NONE)
		return NONE;
	for (index = 0; index < (short)NUMBEROF(names); index++)
	{
		long effect_index = tag_loaded('effe', names[index]);

		if (effect_index != NONE)
		{
			error(_error_silent, "AE vehicle death: %08lX explodes (%s)", (unsigned long)object_index, names[index]);
			return effect_index;
		}
	}
	return NONE;
}

/* (object_deplete_body: a client whenever the host's word kills one, stock multiplayer vehicles dying no other
way; the host plays its own in ae_vehicle_death_depleted, owned by the destroyer) */
long ae_vehicle_death_effect(long object_index)
{
	return network_game_distributed_client() ? ae_vehicle_death_effect_index(object_index) : NONE;
}

long ae_vehicle_death_respawn_ticks(long respawn_ticks)
{
	/* (a gametype whose vehicles never come back would lose each one destroyed for good) */
	return respawn_ticks <= 0 && ae_vehicle_death_on() ? AE_VEHICLE_DEATH_FALLBACK_RESPAWN_TICKS : respawn_ticks;
}

static long ae_vehicle_death_test_ticks(void)
{
	static long ticks = -1;

	if (ticks < 0)
	{
		char const *value = getenv("HALO_VEHICLE_DEATH_TEST");

		ticks = value ? (long)(atof(value) * TICKS_PER_SECOND) : 0;
	}
	return ticks;
}

static struct player_datum *ae_vehicle_death_last_player(void)
{
	struct data_iterator iterator;
	struct player_datum *player;
	struct player_datum *last = NULL;

	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
		last = player;
	return last;
}

static void ae_vehicle_death_test_seat(void)
{
	struct player_datum *last = ae_vehicle_death_last_player();
	struct object_iterator vehicles;
	long nearest_index = NONE;
	real nearest_distance = 0.0f;
	short seat_index;

	if (!last || last->unit_index == NONE)
		return;
	object_iterator_new(&vehicles, _object_mask_vehicle, 0);
	while (object_iterator_next(&vehicles))
	{
		real distance = distance_squared3d(&object_get(last->unit_index)->object.position,
			&object_get(vehicles.index)->object.position);

		if (ae_vehicle_death_vitality(object_get(vehicles.index)->definition_index) > 0.0f &&
			strstr(tag_get_name(object_get(vehicles.index)->definition_index), "warthog") &&
			(nearest_index == NONE || distance < nearest_distance))
		{
			nearest_index = vehicles.index;
			nearest_distance = distance;
		}
	}
	if (nearest_index == NONE)
	{
		error(_error_silent, "AE vehicle death test: no warthog");
		return;
	}
	ae_vehicle_death_test.vehicle_index = nearest_index;
	ae_vehicle_death_test.rider_index = last->unit_index;
	for (seat_index = 0; seat_index < unit_definition_get(object_get(nearest_index)->definition_index)->unit.seats.count;
		seat_index++)
	{
		if (unit_seat_is_driver(nearest_index, seat_index) && unit_enter_seat(last->unit_index, nearest_index, seat_index))
		{
			error(_error_silent, "AE vehicle death test: unit %08lX drives %s %08lX", (unsigned long)last->unit_index,
				tag_get_name(object_get(nearest_index)->definition_index), (unsigned long)nearest_index);
			return;
		}
	}
	error(_error_silent, "AE vehicle death test: unit %08lX cannot drive %08lX, the vehicle is shot empty",
		(unsigned long)last->unit_index, (unsigned long)nearest_index);
}

static void ae_vehicle_death_test_hit(void)
{
	long vehicle_index = ae_vehicle_death_test.vehicle_index;
	struct object_datum *vehicle = object_try_and_get(vehicle_index);
	long damage_index = tag_loaded('jpt!', "weapons\\rocket launcher\\explosion");
	struct damage_data damage;
	struct data_iterator iterator;
	struct player_datum *player;

	if (!vehicle || TEST_FLAG(vehicle->object.damage_flags, _object_dead_bit) || damage_index == NONE)
		return;
	damage_data_new(&damage, damage_index);
	/* (owned by a player other than the rider: the kill is theirs) */
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		if (player->unit_index != NONE && player->unit_index != ae_vehicle_death_test.rider_index)
		{
			damage.owner_player_index = iterator.datum_index;
			damage.owner_object_index = player->unit_index;
			damage.owner_team_index = object_get(player->unit_index)->object.owner_team_index;
			break;
		}
	}
	damage.scale = 1.0f;
	damage.location = vehicle->object.location;
	object_get_origin(vehicle_index, &damage.origin);
	damage.epicenter = damage.origin;
	damage.epicenter.z += 0.3f;
	damage.direction.i = 0.0f;
	damage.direction.j = 0.0f;
	damage.direction.k = -1.0f;
	ae_vehicle_death_test.hits++;
	/* (HALO_VEHICLE_DEATH_TEST_DIRECT: a hit on the vehicle alone, not an explosion's area, which the riders
	take a share of as the vehicle's children; else an explosion on it) */
	if (getenv("HALO_VEHICLE_DEATH_TEST_DIRECT"))
		object_cause_damage(&damage, vehicle_index, NONE, NONE, NONE, NULL);
	else
		area_of_effect_cause_damage(&damage, NONE);
	vehicle = object_try_and_get(vehicle_index);
	error(_error_silent, "AE vehicle death test: rocket %ld on %08lX (owner player %08lX): health %.3f of %.0f%s",
		ae_vehicle_death_test.hits, (unsigned long)vehicle_index, (unsigned long)damage.owner_player_index,
		vehicle ? (double)vehicle->object.body_vitality : 0.0, vehicle ? (double)vehicle->object.maximum_body_vitality : 0.0,
		!vehicle ? " (gone)" : TEST_FLAG(vehicle->object.damage_flags, _object_dead_bit) ? " (dead)" : "");
}

void ae_vehicle_death_update(void)
{
	long now = game_time_get();
	long test_ticks;
	short index;

	/* (the test driver runs with the rule off too: the stock behaviour to compare) */
	if (!game_engine_running() || network_game_distributed_client() ||
		(!ae_vehicle_death_switch() && ae_vehicle_death_test_ticks() <= 0))
	{
		return;
	}
	ae_vehicle_death_new_game_check();
	for (index = 0; index < AE_VEHICLE_DEATH_WRECKS; index++)
	{
		long object_index = ae_vehicle_death_wrecks[index].object_index;

		if (object_index == NONE || !object_try_and_get(object_index))
			continue;
		if (now - ae_vehicle_death_wrecks[index].died_at >= AE_VEHICLE_DEATH_WRECK_TICKS)
		{
			error(_error_silent, "AE vehicle death: wreck %08lX cleared", (unsigned long)object_index);
			ae_vehicle_death_wrecks[index].object_index = NONE;
			object_delete(object_index);
		}
	}
	/* (a vehicle the respawn puts on its pad) */
	{
		static long last_vehicles = -1;
		struct object_iterator vehicles;
		long count = 0;

		object_iterator_new(&vehicles, _object_mask_vehicle, 0);
		while (object_iterator_next(&vehicles))
			count++;
		if (last_vehicles >= 0 && count != last_vehicles)
			error(_error_silent, "AE vehicle death: %ld vehicles in the world (was %ld)", count, last_vehicles);
		last_vehicles = count;
	}
	test_ticks = ae_vehicle_death_test_ticks();
	if (test_ticks <= 0)
		return;
	if (now == test_ticks)
	{
		error(_error_silent, "AE vehicle death test: the gametype's vehicle respawn time %d s",
			(int)game_variant_options_get()->vehicle_respawn_time);
		ae_vehicle_death_test_seat();
	}
	if (now > test_ticks + 3 * TICKS_PER_SECOND && (now - test_ticks) % (2 * TICKS_PER_SECOND) == 0)
		ae_vehicle_death_test_hit();
}
