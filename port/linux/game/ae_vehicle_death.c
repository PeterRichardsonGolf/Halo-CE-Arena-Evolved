/*
AE_VEHICLE_DEATH.C

VEHICLES DESTRUCTIBLE as the gametype's rule (ae_vehicle_death.h; the bit, the table and the Halo 2 way:
ae_vehicle_death_rules.h). Grown from the 0.3.8 spike (HALO_VEHICLE_DEATH=1, now the gametype's bit).

Stock Halo CE multiplayer vehicles cannot die because of their tags: the collision models of the multiplayer
warthog, ghost, scorpion, banshee and turret have a maximum body vitality of 0 (object_initialize_vitality gives them
no health), their materials' body damage multipliers are 0, and they have no depleted effect. The engine's own death
path for vehicles (object_deplete_body: riders killed, dying regions) is whole: campaign ghosts, banshees and wraiths
use it.

Under ON the host (or a local game) lets only explosives hurt a vehicle, gives one whose tag has no health the
table's on its first hit (its 0 multipliers count as 1), holds one at 0 while a living rider sits in it, and at 0
with no rider lets the engine's death path run: this file then plays an explosion from the map's own tags (owned by
the destroyer, so what it kills is theirs), deletes the wreck after 10 seconds, and the gametype's vehicle respawn
(game_engine.c) puts a new one on its pad.

Clients take the host's word: the object state's dead bit (network_objects.c) kills a client's copy, which runs
object_deplete_body there too (so an AE client plays the explosion); the wreck's deletion and the new vehicle are
the host's object messages.

HALO_VEHICLE_DEATH_TEST=<seconds> (test driver, host or local game only; runs with the rule off too, to compare): at
that time the last player is seated in the nearest vehicle of HALO_VEHICLE_DEATH_TEST_KIND (a word of the table,
default warthog; HALO_VEHICLE_DEATH_TEST_EMPTY=1 seats no one); from 3 seconds on, every 2 seconds a hit of
HALO_VEHICLE_DEATH_TEST_DAMAGE (a damage effect's name, default the rocket's explosion) goes off on it, owned by
another player: an explosion's area, or the vehicle alone with HALO_VEHICLE_DEATH_TEST_DIRECT=1.
HALO_VEHICLE_DEATH_TEST_EXIT=<n>: the rider gets out after hit n. HALO_VEHICLE_DEATH_TEST_ALL=1: every vehicle of the
map is hit instead (the wrecks' frame time). Each second of the test logs the wrecks and the wall time a tick took.
*/

#include "cseries.h"
#include "cseries/errors.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "objects/objects.h"
#include "objects/damage.h"
#include "cseries/cseries_windows.h"
#include "effects/effects.h"
#include "physics/collision_model_definitions.h"
#include "tag_files/tag_files.h"
#include "units/units.h"
#include "units/unit_definitions.h"

#include "ae_vehicle_death.h"
#include "ae_vehicle_death_rules.h"

#include <stdlib.h>
#include <string.h>

long tag_loaded(long group_tag, const char *name);
boolean network_game_distributed_client(void);

/* (game_engine.h names the bit the rules header reads; the ticks are the engine's) */
typedef char ae_vehicle_death_bit_check[_game_variant_vehicles_destructible_bit == AE_VEHICLE_DEATH_BIT ? 1 : -1];
typedef char ae_vehicle_death_ticks_check[TICKS_PER_SECOND == 30 ? 1 : -1];

enum
{
	AE_VEHICLE_DEATH_WRECKS = 64,
	AE_VEHICLE_DEATH_HELD = 32,
	AE_VEHICLE_DEATH_RIDERS = 8,
};

static struct
{
	long object_index;
	long died_at;
} ae_vehicle_death_wrecks[AE_VEHICLE_DEATH_WRECKS];

/* the vehicles holding at 0 with riders: who rode at the last look, and the last damage they took (its owner is
the one credited when the last rider dies in it) */
static struct
{
	long object_index;
	short rider_count;
	long riders[AE_VEHICLE_DEATH_RIDERS];
	struct damage_data damage;
} ae_vehicle_death_held_list[AE_VEHICLE_DEATH_HELD];

static struct
{
	long vehicle_index;
	long rider_index;
	long hits;
	unsigned long last_tick_at;
	unsigned long second_ms;
	unsigned long second_slowest;
	long second_ticks;
} ae_vehicle_death_test = { NONE, NONE, 0, 0, 0, 0, 0 };

static long ae_vehicle_death_effect_index(long object_index);

/* the wrecks and held vehicles of an earlier game forgotten (object indices are the new map's): game time went
back */
static void ae_vehicle_death_new_game_check(void)
{
	static long last_time = -1;
	long now = game_time_get();
	short index;

	if (last_time < 0 || now < last_time)
	{
		for (index = 0; index < AE_VEHICLE_DEATH_WRECKS; index++)
			ae_vehicle_death_wrecks[index].object_index = NONE;
		for (index = 0; index < AE_VEHICLE_DEATH_HELD; index++)
			ae_vehicle_death_held_list[index].object_index = NONE;
		ae_vehicle_death_test.vehicle_index = NONE;
		ae_vehicle_death_test.rider_index = NONE;
		ae_vehicle_death_test.hits = 0;
	}
	last_time = now;
}

/* the running game's rule (the host's variant on a client); OFF with no game */
static int ae_vehicle_death_rule(void)
{
	struct game_variant const *variant = game_engine_running() ? game_engine_get_variant() : NULL;

	return variant ? ae_vehicle_death_get(variant->universal_variant.flags) : _ae_vehicle_death_off;
}

/* the rule decides here: a multiplayer game this machine deals damage in (the host, or a local game) */
static boolean ae_vehicle_death_on(void)
{
	return ae_vehicle_death_rule() == _ae_vehicle_death_on && !network_game_distributed_client();
}

static struct collision_model *ae_vehicle_death_collision(long object_index)
{
	struct object_definition *definition = object_definition_get(object_get(object_index)->definition_index);

	return definition->object.collision_model.index == NONE ? NULL :
		collision_model_definition_get(definition->object.collision_model.index);
}

/* a vehicle the rule makes mortal: its tag gives it no health (a stock multiplayer one) and the table has one */
static boolean ae_vehicle_death_mortal_by_rule(long object_index)
{
	struct object_datum *object = object_get(object_index);
	struct collision_model *collision;

	if (object->object.type != _object_type_vehicle)
		return FALSE;
	collision = ae_vehicle_death_collision(object_index);
	return collision && collision->resistance.maximum_body_vitality <= 0.0f &&
		ae_vehicle_death_health_for(tag_get_name(object->definition_index)) > 0.0f;
}

real ae_vehicle_death_body_multiplier(long object_index, boolean explosive, real multiplier)
{
	struct object_datum *object = object_get(object_index);
	boolean mortal;
	real result;

	if (object->object.type != _object_type_vehicle || !ae_vehicle_death_on())
		return multiplier;
	mortal = ae_vehicle_death_mortal_by_rule(object_index);
	if (mortal && explosive && object->object.maximum_body_vitality <= 0.0f &&
		!TEST_FLAG(object->object.damage_flags, _object_dead_bit))
	{
		real vitality = ae_vehicle_death_health_for(tag_get_name(object->definition_index));

		object->object.maximum_body_vitality = vitality;
		object->object.body_vitality = 1.0f;
		error(_error_silent, "AE vehicle death: %s %08lX gets %.0f health", tag_get_name(object->definition_index),
			(unsigned long)object_index, (double)vitality);
	}
	result = ae_vehicle_death_multiplier(_ae_vehicle_death_on, mortal, explosive, multiplier);
	return result;
}

boolean ae_vehicle_death_occupied_only(long object_index)
{
	return ae_vehicle_death_only_while_occupied(ae_vehicle_death_on() ? _ae_vehicle_death_on : _ae_vehicle_death_off,
		ae_vehicle_death_mortal_by_rule(object_index));
}

/* the living bipeds seated in a vehicle (up to maximum written to riders) */
static short ae_vehicle_death_living_riders(long object_index, long *riders, short maximum)
{
	struct object_datum *object = object_get(object_index);
	long child_index = object->object.first_child_object_index;
	short count = 0;

	while (child_index != NONE)
	{
		struct object_datum *child = object_get(child_index);

		if (child->object.type == _object_type_biped && !TEST_FLAG(child->object.damage_flags, _object_dead_bit) &&
			((struct unit_datum *)child)->unit.parent_seat_index != NONE)
		{
			if (riders && count < maximum)
				riders[count] = child_index;
			count++;
		}
		child_index = child->object.next_object_index;
	}
	return count;
}

boolean ae_vehicle_death_held(long object_index, struct damage_data const *damage)
{
	struct object_datum *object = object_get(object_index);
	long riders[AE_VEHICLE_DEATH_RIDERS];
	short count;
	short index;
	short slot = NONE;

	if (object->object.type != _object_type_vehicle || !ae_vehicle_death_on())
		return FALSE;
	count = ae_vehicle_death_living_riders(object_index, riders, AE_VEHICLE_DEATH_RIDERS);
	if (!ae_vehicle_death_holds(_ae_vehicle_death_on, count))
		return FALSE;
	object->object.body_vitality = 0.0f;
	ae_vehicle_death_new_game_check();
	for (index = 0; index < AE_VEHICLE_DEATH_HELD; index++)
	{
		if (ae_vehicle_death_held_list[index].object_index == object_index)
		{
			slot = index;
			break;
		}
		if (slot == NONE && (ae_vehicle_death_held_list[index].object_index == NONE ||
			!object_try_and_get(ae_vehicle_death_held_list[index].object_index)))
		{
			slot = index;
		}
	}
	if (slot == NONE)
		return TRUE;
	if (ae_vehicle_death_held_list[slot].object_index != object_index)
	{
		error(_error_silent, "AE vehicle death: %s %08lX holds at 0 health with %d rider%s in it",
			tag_get_name(object->definition_index), (unsigned long)object_index, (int)count, count == 1 ? "" : "s");
	}
	ae_vehicle_death_held_list[slot].object_index = object_index;
	ae_vehicle_death_held_list[slot].rider_count = MIN(count, (short)AE_VEHICLE_DEATH_RIDERS);
	memcpy(ae_vehicle_death_held_list[slot].riders, riders, sizeof(riders));
	ae_vehicle_death_held_list[slot].damage = *damage;
	ae_vehicle_death_held_list[slot].damage.material_response = NULL;
	return TRUE;
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
		{
			free_index = index;
		}
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

	if (!ae_vehicle_death_on() || !ae_vehicle_death_mortal_by_rule(object_index) ||
		object->object.maximum_body_vitality <= 0.0f)
	{
		return;
	}
	error(_error_silent, "AE vehicle death: %s %08lX destroyed (damage %s, owner player %08lX object %08lX)",
		tag_get_name(object->definition_index), (unsigned long)object_index,
		damage->definition_index == NONE ? "none" : tag_get_name(damage->definition_index),
		(unsigned long)damage->owner_player_index, (unsigned long)damage->owner_object_index);
	/* (no living rider is left under the Halo 2 way; a dead one still seated, or an AI the engine seated past
	the hold, dies as the destroying damage's owner's kill, not an act of god's) */
	for (child_index = object->object.first_child_object_index; child_index != NONE;)
	{
		struct object_datum *child = object_get(child_index);
		long next_index = child->object.next_object_index;

		if (child->object.type == _object_type_biped && !TEST_FLAG(child->object.damage_flags, _object_dead_bit) &&
			((struct unit_datum *)child)->unit.parent_seat_index != NONE)
		{
			struct damage_data kill = *damage;

			SET_FLAG(kill.flags, _damage_kill_instantly_bit, TRUE);
			SET_FLAG(kill.flags, _damage_area_of_effect_bit, FALSE);
			kill.material_response = NULL;
			object_cause_damage(&kill, child_index, NONE, NONE, NONE, NULL);
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

	if (!game_engine_running() || object_get(object_index)->object.type != _object_type_vehicle ||
		ae_vehicle_death_rule() != _ae_vehicle_death_on)
	{
		return NONE;
	}
	collision = ae_vehicle_death_collision(object_index);
	if (!collision || collision->resistance.body_depleted_effect.index != NONE ||
		collision->resistance.maximum_body_vitality > 0.0f)
	{
		return NONE;
	}
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

long ae_vehicle_death_effect(long object_index)
{
	return network_game_distributed_client() ? ae_vehicle_death_effect_index(object_index) : NONE;
}

long ae_vehicle_death_respawn_ticks(long respawn_ticks)
{
	return ae_vehicle_death_respawn(ae_vehicle_death_on() ? _ae_vehicle_death_on : _ae_vehicle_death_off,
		respawn_ticks);
}

boolean ae_vehicle_death_intact_returns(void)
{
	return ae_vehicle_death_intact_returns_rule(ae_vehicle_death_on() ? _ae_vehicle_death_on : _ae_vehicle_death_off,
		(long)game_variant_options_get()->vehicle_respawn_time * TICKS_PER_SECOND) ? TRUE : FALSE;
}

/* the held vehicles: riders gone, it dies (one died in it) or gets a quarter of its health back */
static void ae_vehicle_death_update_held(void)
{
	short index;

	for (index = 0; index < AE_VEHICLE_DEATH_HELD; index++)
	{
		long object_index = ae_vehicle_death_held_list[index].object_index;
		struct object_datum *object;
		long riders[AE_VEHICLE_DEATH_RIDERS];
		short count;
		short known;
		boolean died = FALSE;

		if (object_index == NONE)
			continue;
		object = object_try_and_get(object_index);
		if (!object || TEST_FLAG(object->object.damage_flags, _object_dead_bit))
		{
			ae_vehicle_death_held_list[index].object_index = NONE;
			continue;
		}
		count = ae_vehicle_death_living_riders(object_index, riders, AE_VEHICLE_DEATH_RIDERS);
		if (count > 0)
		{
			ae_vehicle_death_held_list[index].rider_count = MIN(count, (short)AE_VEHICLE_DEATH_RIDERS);
			memcpy(ae_vehicle_death_held_list[index].riders, riders, sizeof(riders));
			continue;
		}
		/* (the riders of the last look: one dead or gone since died in it) */
		for (known = 0; known < ae_vehicle_death_held_list[index].rider_count; known++)
		{
			struct object_datum *rider = object_try_and_get(ae_vehicle_death_held_list[index].riders[known]);

			if (!rider || TEST_FLAG(rider->object.damage_flags, _object_dead_bit))
				died = TRUE;
		}
		ae_vehicle_death_held_list[index].object_index = NONE;
		if (ae_vehicle_death_after_riders(died) == _ae_vehicle_death_riders_gone_dies)
		{
			struct damage_data damage = ae_vehicle_death_held_list[index].damage;

			error(_error_silent, "AE vehicle death: %s %08lX: its last rider died in it, it goes",
				tag_get_name(object->definition_index), (unsigned long)object_index);
			object_deplete_body(object_index);
			ae_vehicle_death_depleted(object_index, &damage);
		}
		else
		{
			object->object.body_vitality = ae_vehicle_death_restored_vitality(object->object.body_vitality);
			error(_error_silent, "AE vehicle death: %s %08lX: its riders got out alive, health back to %.2f",
				tag_get_name(object->definition_index), (unsigned long)object_index,
				(double)object->object.body_vitality);
		}
	}
}

void ae_vehicle_death_log_rules(unsigned long flags)
{
	error(_error_silent, "AE vehicles destructible: %s", ae_vehicle_death_name(ae_vehicle_death_get(flags)));
}

/* ---------- the test driver */

static char const *ae_vehicle_death_test_value(char const *name)
{
	char const *value = getenv(name);

	return value && *value ? value : NULL;
}

static long ae_vehicle_death_test_ticks(void)
{
	static long ticks = -1;

	if (ticks < 0)
	{
		char const *value = ae_vehicle_death_test_value("HALO_VEHICLE_DEATH_TEST");

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
	char const *kind = ae_vehicle_death_test_value("HALO_VEHICLE_DEATH_TEST_KIND");
	struct object_iterator vehicles;
	long nearest_index = NONE;
	real nearest_distance = 0.0f;
	short seat_index;

	if (!kind)
		kind = "warthog";
	if (!last || last->unit_index == NONE)
		return;
	object_iterator_new(&vehicles, _object_mask_vehicle, 0);
	while (object_iterator_next(&vehicles))
	{
		char const *name = tag_get_name(object_get(vehicles.index)->definition_index);
		real distance = distance_squared3d(&object_get(last->unit_index)->object.position,
			&object_get(vehicles.index)->object.position);

		if (name && ae_vehicle_death_contains(name, kind) && (nearest_index == NONE || distance < nearest_distance))
		{
			nearest_index = vehicles.index;
			nearest_distance = distance;
		}
	}
	if (nearest_index == NONE)
	{
		error(_error_silent, "AE vehicle death test: no %s", kind);
		return;
	}
	ae_vehicle_death_test.vehicle_index = nearest_index;
	ae_vehicle_death_test.rider_index = NONE;
	if (ae_vehicle_death_test_value("HALO_VEHICLE_DEATH_TEST_EMPTY"))
	{
		error(_error_silent, "AE vehicle death test: %s %08lX, empty", tag_get_name(object_get(nearest_index)->definition_index),
			(unsigned long)nearest_index);
		return;
	}
	for (seat_index = 0; seat_index < unit_definition_get(object_get(nearest_index)->definition_index)->unit.seats.count;
		seat_index++)
	{
		if (unit_seat_is_driver(nearest_index, seat_index) && unit_enter_seat(last->unit_index, nearest_index, seat_index))
		{
			ae_vehicle_death_test.rider_index = last->unit_index;
			error(_error_silent, "AE vehicle death test: unit %08lX drives %s %08lX", (unsigned long)last->unit_index,
				tag_get_name(object_get(nearest_index)->definition_index), (unsigned long)nearest_index);
			return;
		}
	}
	error(_error_silent, "AE vehicle death test: unit %08lX cannot drive %08lX, the vehicle is shot empty",
		(unsigned long)last->unit_index, (unsigned long)nearest_index);
}

static void ae_vehicle_death_test_hit_one(long vehicle_index, long damage_index)
{
	struct object_datum *vehicle = object_try_and_get(vehicle_index);
	struct damage_data damage;
	struct data_iterator iterator;
	struct player_datum *player;

	if (!vehicle || TEST_FLAG(vehicle->object.damage_flags, _object_dead_bit))
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
	/* (DIRECT: a hit on the vehicle alone, not an explosion's area, which the riders take a share of) */
	if (ae_vehicle_death_test_value("HALO_VEHICLE_DEATH_TEST_DIRECT"))
		object_cause_damage(&damage, vehicle_index, NONE, NONE, NONE, NULL);
	else
		area_of_effect_cause_damage(&damage, NONE);
	vehicle = object_try_and_get(vehicle_index);
	error(_error_silent, "AE vehicle death test: hit %ld (%s) on %08lX (owner player %08lX): health %.3f of %.0f%s",
		ae_vehicle_death_test.hits, tag_get_name(damage_index), (unsigned long)vehicle_index,
		(unsigned long)damage.owner_player_index, vehicle ? (double)vehicle->object.body_vitality : 0.0,
		vehicle ? (double)vehicle->object.maximum_body_vitality : 0.0,
		!vehicle ? " (gone)" : TEST_FLAG(vehicle->object.damage_flags, _object_dead_bit) ? " (dead)" : "");
}

static void ae_vehicle_death_test_hit(void)
{
	char const *damage_name = ae_vehicle_death_test_value("HALO_VEHICLE_DEATH_TEST_DAMAGE");
	char const *exit_value = ae_vehicle_death_test_value("HALO_VEHICLE_DEATH_TEST_EXIT");
	long damage_index = tag_loaded('jpt!', damage_name ? damage_name : "weapons\\rocket launcher\\explosion");

	if (damage_index == NONE)
	{
		error(_error_silent, "AE vehicle death test: no damage effect %s", damage_name ? damage_name : "(rocket)");
		return;
	}
	ae_vehicle_death_test.hits++;
	if (ae_vehicle_death_test_value("HALO_VEHICLE_DEATH_TEST_ALL"))
	{
		struct object_iterator vehicles;
		long indices[64];
		short count = 0;
		short index;

		object_iterator_new(&vehicles, _object_mask_vehicle, 0);
		while (object_iterator_next(&vehicles) && count < (short)NUMBEROF(indices))
			indices[count++] = vehicles.index;
		for (index = 0; index < count; index++)
			ae_vehicle_death_test_hit_one(indices[index], damage_index);
	}
	else
	{
		ae_vehicle_death_test_hit_one(ae_vehicle_death_test.vehicle_index, damage_index);
	}
	if (exit_value && ae_vehicle_death_test.hits == atol(exit_value) && ae_vehicle_death_test.rider_index != NONE &&
		object_try_and_get(ae_vehicle_death_test.rider_index) &&
		!TEST_FLAG(object_get(ae_vehicle_death_test.rider_index)->object.damage_flags, _object_dead_bit))
	{
		error(_error_silent, "AE vehicle death test: unit %08lX gets out", (unsigned long)ae_vehicle_death_test.rider_index);
		unit_exit_seat_end(ae_vehicle_death_test.rider_index);
	}
}

/* each second of the test: the wrecks there and the wall time a game tick took (the frame time with wrecks) */
static void ae_vehicle_death_test_time(long now)
{
	unsigned long at = system_milliseconds();

	if (ae_vehicle_death_test.last_tick_at)
	{
		unsigned long took = at - ae_vehicle_death_test.last_tick_at;

		ae_vehicle_death_test.second_ms += took;
		if (took > ae_vehicle_death_test.second_slowest)
			ae_vehicle_death_test.second_slowest = took;
		ae_vehicle_death_test.second_ticks++;
	}
	ae_vehicle_death_test.last_tick_at = at;
	if (now % TICKS_PER_SECOND == 0 && ae_vehicle_death_test.second_ticks > 0)
	{
		short wrecks = 0;
		short index;

		for (index = 0; index < AE_VEHICLE_DEATH_WRECKS; index++)
		{
			if (ae_vehicle_death_wrecks[index].object_index != NONE &&
				object_try_and_get(ae_vehicle_death_wrecks[index].object_index))
			{
				wrecks++;
			}
		}
		error(_error_silent, "AE vehicle death test: second %ld: %d wrecks, a tick %.1f ms (slowest %lu ms)",
			now / TICKS_PER_SECOND, (int)wrecks,
			(double)ae_vehicle_death_test.second_ms / (double)ae_vehicle_death_test.second_ticks,
			ae_vehicle_death_test.second_slowest);
		ae_vehicle_death_test.second_ms = 0;
		ae_vehicle_death_test.second_slowest = 0;
		ae_vehicle_death_test.second_ticks = 0;
	}
}

void ae_vehicle_death_update(void)
{
	long now = game_time_get();
	long test_ticks;
	short index;

	if (!game_engine_running() || network_game_distributed_client())
		return;
	ae_vehicle_death_new_game_check();
	if (ae_vehicle_death_on())
	{
		ae_vehicle_death_update_held();
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
	}
	test_ticks = ae_vehicle_death_test_ticks();
	if (test_ticks <= 0)
		return;
	ae_vehicle_death_test_time(now);
	/* (a vehicle the respawn puts on its pad) */
	{
		static long last_vehicles = -1;
		struct object_iterator vehicles;
		long count = 0;

		object_iterator_new(&vehicles, _object_mask_vehicle, 0);
		while (object_iterator_next(&vehicles))
			count++;
		if (last_vehicles >= 0 && count != last_vehicles)
			error(_error_silent, "AE vehicle death test: %ld vehicles in the world (was %ld)", count, last_vehicles);
		last_vehicles = count;
	}
	if (now == test_ticks)
	{
		error(_error_silent, "AE vehicle death test: the gametype's vehicle respawn time %d s",
			(int)game_variant_options_get()->vehicle_respawn_time);
		ae_vehicle_death_test_seat();
	}
	if (now > test_ticks + 3 * TICKS_PER_SECOND && (now - test_ticks) % (2 * TICKS_PER_SECOND) == 0)
		ae_vehicle_death_test_hit();
}
