/*
AE_VEHICLE_PACK.C

The vehicle damage pack's look (ae_vehicle_pack.h; what it shows, and why: ae_vehicle_pack_rules.h). The core
(ae_vehicle_death.c) decides who dies and when; this file only shows it, with the pack's tags, found by name on
the loaded map (the pack is added to it by ae_weapon_pack.c). Everything here is this machine's own: effects and
scenery are never sent over the network, so a machine without the pack, a stock client among them, sees the core's
basic death, and the game plays the same.

THE DAMAGED LOOK needs the vehicle's health, which only the machine that deals damage knows (the host, a local
game): there a vehicle at its kind's fraction or below plays the pack's damage effect once and its loop (smoke,
fire) for as long as it stays so.

THE DEATH (every machine: the host as it destroys it, a client as the host's dead bit kills its copy): the pack's
explosion beside the map's (which deals the damage) and the parts that come off, played on the wreck (whose model
has CMT's markers): a scenery of the pack made where the vehicle is, made its child (it moves with the dead
vehicle, the way it falls or tumbles, and goes when the host clears the vehicle: deleting an object deletes its
children), and the dead vehicle hidden with what it carries (the warthog's gun: the wreck has its own). A kind with
no wreck keeps its body, burning with the damaged loop until it is cleared.
*/

#include "cseries.h"
#include "cseries/errors.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "objects/objects.h"
#include "effects/effects.h"
#include "tag_files/tag_files.h"

#include "ae_vehicle_pack.h"
#include "ae_vehicle_pack_rules.h"
#include "ae_weapon_pack.h"

#include <string.h>

long tag_loaded(long group_tag, const char *name);

#define AE_VEHICLE_PACK_EFFECT_GROUP 0x65666665 /* 'effe' */
#define AE_VEHICLE_PACK_SCENERY_GROUP 0x7363656E /* 'scen' */

enum
{
	AE_VEHICLE_PACK_TRACKED = 64,
	AE_VEHICLE_PACK_NAME_SIZE = 256,
};

/* the vehicles whose look has started on this machine */
static struct
{
	long object_index;
	short kind;
	boolean damaged;
	boolean died;
	long loop_effect;
	long wreck_index;
} ae_vehicle_pack_tracked[AE_VEHICLE_PACK_TRACKED];

/* ---------- private code */

static void ae_vehicle_pack_forget(short index)
{
	ae_vehicle_pack_tracked[index].object_index = NONE;
	ae_vehicle_pack_tracked[index].kind = _ae_vehicle_pack_none;
	ae_vehicle_pack_tracked[index].damaged = FALSE;
	ae_vehicle_pack_tracked[index].died = FALSE;
	ae_vehicle_pack_tracked[index].loop_effect = NONE;
	ae_vehicle_pack_tracked[index].wreck_index = NONE;
}

/* the looks of an earlier game forgotten (object indices are the new map's): game time went back */
static void ae_vehicle_pack_new_game_check(void)
{
	static long last_time = -1;
	long now = game_time_get();
	short index;

	if (last_time < 0 || now < last_time)
	{
		for (index = 0; index < AE_VEHICLE_PACK_TRACKED; index++)
			ae_vehicle_pack_forget(index);
	}
	last_time = now;
}

/* whether this game shows the pack's look: the running game has the rule on and the pack is on the map */
static boolean ae_vehicle_pack_shown(void)
{
	struct game_variant const *variant = game_engine_running() ? game_engine_get_variant() : NULL;

	return variant && ae_vehicle_death_get((unsigned long)variant->universal_variant.flags) == _ae_vehicle_death_on &&
		ae_weapon_pack_vehicle_pack_added();
}

/* the pack's tag of a look's name on the loaded map, or NONE */
static long ae_vehicle_pack_tag(long group_tag, char const *look_name)
{
	char name[AE_VEHICLE_PACK_NAME_SIZE];

	if (!ae_vehicle_pack_name(look_name, name, sizeof(name)))
		return NONE;
	return tag_loaded(group_tag, name);
}

static long ae_vehicle_pack_effect(long object_index, char const *look_name)
{
	long effect_index = ae_vehicle_pack_tag(AE_VEHICLE_PACK_EFFECT_GROUP, look_name);

	return effect_index == NONE ? NONE :
		effect_new_from_object(effect_index, object_index, object_index, NONE, 0.0f, 0.0f, NULL, NULL);
}

static long ae_vehicle_pack_loop(long object_index, char const *look_name)
{
	long effect_index = ae_vehicle_pack_tag(AE_VEHICLE_PACK_EFFECT_GROUP, look_name);

	return effect_index == NONE ? NONE : effect_new_looping(effect_index, object_index, NONE, NONE, NONE);
}

/* the vehicle's entry (made for it when make, and there is room), or NONE */
static short ae_vehicle_pack_entry(long object_index, short kind, boolean make)
{
	short index;
	short free_index = NONE;

	for (index = 0; index < AE_VEHICLE_PACK_TRACKED; index++)
	{
		if (ae_vehicle_pack_tracked[index].object_index == object_index)
			return index;
		if (free_index == NONE && ae_vehicle_pack_tracked[index].object_index == NONE)
			free_index = index;
	}
	if (!make || free_index == NONE)
		return NONE;
	ae_vehicle_pack_forget(free_index);
	ae_vehicle_pack_tracked[free_index].object_index = object_index;
	ae_vehicle_pack_tracked[free_index].kind = kind;
	return free_index;
}

/* the wreck made where the vehicle is, its child (it goes with it), and the dead vehicle hidden: the wreck's index,
or NONE (no such tag on the map, or no room for an object) */
static long ae_vehicle_pack_wreck(long vehicle_index, char const *look_name)
{
	long definition_index = ae_vehicle_pack_tag(AE_VEHICLE_PACK_SCENERY_GROUP, look_name);
	struct object_datum *vehicle = object_get(vehicle_index);
	struct object_placement_data data;
	long wreck_index;

	if (definition_index == NONE)
		return NONE;
	object_placement_data_new(&data, definition_index, NONE);
	data.position = vehicle->object.position;
	data.forward = vehicle->object.forward;
	data.up = vehicle->object.up;
	wreck_index = object_new(&data);
	if (wreck_index == NONE)
		return NONE;
	object_attach_to_node(vehicle_index, wreck_index, 0);
	/* (the dead vehicle hidden, and what it carries but its riders: the warthog's gun, which the wreck has its own
	of) */
	{
		long child_index;

		for (child_index = vehicle->object.first_child_object_index; child_index != NONE;
			child_index = object_get(child_index)->object.next_object_index)
		{
			struct object_datum *child = object_get(child_index);

			if (child_index != wreck_index && child->object.type != _object_type_biped)
				object_set_visibility(child_index, FALSE);
		}
	}
	object_set_visibility(vehicle_index, FALSE);
	return wreck_index;
}

/* ---------- public code */

void ae_vehicle_pack_died(long vehicle_index)
{
	struct object_datum *vehicle = object_try_and_get(vehicle_index);
	struct ae_vehicle_pack_look const *look;
	short kind;
	short entry;
	short extra;

	if (!vehicle || vehicle->object.type != _object_type_vehicle || !ae_vehicle_pack_shown())
		return;
	kind = (short)ae_vehicle_pack_kind_for(tag_get_name(vehicle->definition_index));
	if (kind == _ae_vehicle_pack_none)
		return;
	ae_vehicle_pack_new_game_check();
	entry = ae_vehicle_pack_entry(vehicle_index, kind, TRUE);
	if (entry == NONE || ae_vehicle_pack_tracked[entry].died)
		return;
	look = &ae_vehicle_pack_looks[kind];
	ae_vehicle_pack_tracked[entry].died = TRUE;
	if (ae_vehicle_pack_tracked[entry].loop_effect != NONE)
	{
		effect_stop(ae_vehicle_pack_tracked[entry].loop_effect, TRUE);
		ae_vehicle_pack_tracked[entry].loop_effect = NONE;
	}
	/* (the wreck first: the explosion and the parts coming off play on it, whose model has CMT's markers, the hood's
	and the wheels'; a kind with no wreck plays them on the vehicle, its loop on the dead body) */
	if (look->wreck)
		ae_vehicle_pack_tracked[entry].wreck_index = ae_vehicle_pack_wreck(vehicle_index, look->wreck);
	{
		long on_index = ae_vehicle_pack_tracked[entry].wreck_index != NONE ? ae_vehicle_pack_tracked[entry].wreck_index :
			vehicle_index;

		ae_vehicle_pack_effect(on_index, look->death);
		for (extra = 0; extra < AE_VEHICLE_PACK_DEATH_EXTRAS; extra++)
		{
			if (look->death_extras[extra])
				ae_vehicle_pack_effect(on_index, look->death_extras[extra]);
		}
	}
	if (!look->wreck)
		ae_vehicle_pack_tracked[entry].loop_effect = ae_vehicle_pack_loop(vehicle_index, look->damaged_loop);
	error(_error_silent, "AE vehicle pack: %s %08lX destroyed: the pack's explosion, %s",
		tag_get_name(vehicle->definition_index), (unsigned long)vehicle_index,
		ae_vehicle_pack_tracked[entry].wreck_index != NONE ? "its wreck stands in" :
		look->wreck ? "no wreck (none made)" : "its body burns");
}

void ae_vehicle_pack_update(void)
{
	struct object_iterator vehicles;
	short index;

	if (!ae_vehicle_pack_shown())
		return;
	ae_vehicle_pack_new_game_check();
	for (index = 0; index < AE_VEHICLE_PACK_TRACKED; index++)
	{
		if (ae_vehicle_pack_tracked[index].object_index != NONE &&
			!object_try_and_get(ae_vehicle_pack_tracked[index].object_index))
		{
			/* (its effects went with it: an effect on an object that is gone deletes itself) */
			ae_vehicle_pack_forget(index);
		}
	}
	object_iterator_new(&vehicles, _object_mask_vehicle, 0);
	while (object_iterator_next(&vehicles))
	{
		struct object_datum *vehicle = object_get(vehicles.index);
		boolean dead = TEST_FLAG(vehicle->object.damage_flags, _object_dead_bit);
		short kind;
		short entry;

		if (dead)
			continue;
		kind = (short)ae_vehicle_pack_kind_for(tag_get_name(vehicle->definition_index));
		if (kind == _ae_vehicle_pack_none)
			continue;
		entry = ae_vehicle_pack_entry(vehicles.index, kind, FALSE);
		if (ae_vehicle_pack_damaged(kind, vehicle->object.maximum_body_vitality, vehicle->object.body_vitality, dead))
		{
			struct ae_vehicle_pack_look const *look = &ae_vehicle_pack_looks[kind];

			if (entry != NONE && ae_vehicle_pack_tracked[entry].damaged)
				continue;
			entry = ae_vehicle_pack_entry(vehicles.index, kind, TRUE);
			if (entry == NONE)
				continue;
			ae_vehicle_pack_tracked[entry].damaged = TRUE;
			ae_vehicle_pack_effect(vehicles.index, look->damaged);
			if (look->damaged_extra)
				ae_vehicle_pack_effect(vehicles.index, look->damaged_extra);
			ae_vehicle_pack_tracked[entry].loop_effect = ae_vehicle_pack_loop(vehicles.index, look->damaged_loop);
			error(_error_silent, "AE vehicle pack: %s %08lX damaged (health %.2f): smoke and fire",
				tag_get_name(vehicle->definition_index), (unsigned long)vehicles.index, (double)vehicle->object.body_vitality);
		}
		else if (entry != NONE && ae_vehicle_pack_tracked[entry].damaged)
		{
			/* (health back above its fraction: the loop stops) */
			ae_vehicle_pack_tracked[entry].damaged = FALSE;
			if (ae_vehicle_pack_tracked[entry].loop_effect != NONE)
				effect_stop(ae_vehicle_pack_tracked[entry].loop_effect, TRUE);
			ae_vehicle_pack_tracked[entry].loop_effect = NONE;
		}
	}
}
