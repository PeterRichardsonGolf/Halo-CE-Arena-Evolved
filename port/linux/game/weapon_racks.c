/*
WEAPON_RACKS.C

The game's own weapon rack of Forge's FIREFIGHT category (tools/prop_assets.py's
props: scenery firefight\props\weapon_rack_resupply, a copy of
altis\scenery\weaponrack\weapon rack, which SCENERY places as it is, doing
nothing), which Forge places and Firefight and any game of a saved Forge map
play: a player on foot by one is told so on
the HUD, and action there fills the reserve of every weapon the player
holds (a weapon of charge, which has no reserve, charged again), as
Firefight's resupply fills it (firefight_spawner.c).

Scenery is not anything a player uses, so it is done here each tick
(game.c): the racks are found once a second, the level's objects of the
rack's tag; each machine tells its own players, and the host (or a game
played alone) fills the weapons, whose ammunition the netcode carries.

The FIREFIGHT category's drop areas (firefight_spawner.c's) are markers
alone: outside Forge and Firefight (which takes them away itself, once it
knows where they are) they are taken away here as they are found.
*/

#include <math.h>

#include "cseries.h"
#include "cache/cache_files.h"
#include "game/game.h"
#include "game/players.h"
#include "interface/hud_messaging.h"
#include "math/real_math.h"
#include "memory/data.h"
#include "networking/network_game_globals.h"
#include "objects/objects.h"
#include "tag_files/tag_groups.h"
#include "units/units.h"

#include "halo_firefight.h"
#include "opence_features.h"

/* ---------- constants */

#define WEAPON_RACK_TAG "firefight\\props\\weapon_rack_resupply"
#define DROP_AREA_TAG "firefight\\props\\drop_area_pod"

enum
{
	MAXIMUM_WEAPON_RACKS = 64,
	WEAPON_RACK_PLAYERS = 16,
	/* the racks looked for again so often, and a player's refills apart */
	WEAPON_RACK_SCAN_TICKS = TICKS_PER_SECOND,
	WEAPON_RACK_REFILL_TICKS = TICKS_PER_SECOND,
};

/* how near a player stands to a rack's origin to use it: across, and up or
down */
#define WEAPON_RACK_REACH 1.8f
#define WEAPON_RACK_HEIGHT 2.0f

/* ---------- globals */

static struct
{
	long racks[MAXIMUM_WEAPON_RACKS];
	short rack_count;
	long next_scan_time;
	/* each player's (by its player index's absolute index): near a rack last
	tick, action held last tick, and when it last filled its weapons */
	boolean near[WEAPON_RACK_PLAYERS];
	boolean action_held[WEAPON_RACK_PLAYERS];
	long refilled_time[WEAPON_RACK_PLAYERS];
} weapon_racks;

/* ---------- private code */

/* whether this is a Forge session's level, where the markers are edited */
static boolean weapon_racks_forge_session(
	void)
{
#if OPENCE_ENABLE_FORGE
	extern int g_forge_session_enabled;

	return g_forge_session_enabled != 0;
#else
	return FALSE;
#endif
}

static void racks_scan(
	long now)
{
	long rack_tag = tag_loaded('scen', WEAPON_RACK_TAG);
	long drop_area_tag = tag_loaded('scen', DROP_AREA_TAG);
	struct object_iterator iterator;
	long markers[MAXIMUM_WEAPON_RACKS];
	short marker_count = 0;
	short index;

	weapon_racks.rack_count = 0;
	weapon_racks.next_scan_time = now + WEAPON_RACK_SCAN_TICKS;
	if (drop_area_tag != NONE && (firefight_active() || weapon_racks_forge_session()))
		drop_area_tag = NONE;
	if (rack_tag == NONE && drop_area_tag == NONE)
		return;
	object_iterator_new(&iterator, _object_mask_scenery, 0);
	while (object_iterator_next(&iterator))
	{
		struct object_datum *object = object_get(iterator.index);

		if (object->definition_index == rack_tag && weapon_racks.rack_count < MAXIMUM_WEAPON_RACKS)
			weapon_racks.racks[weapon_racks.rack_count++] = iterator.index;
		else if (object->definition_index == drop_area_tag && marker_count < MAXIMUM_WEAPON_RACKS)
			markers[marker_count++] = iterator.index;
	}
	for (index = 0; index < marker_count; index++)
		object_delete(markers[index]);
}

/* whether a unit on foot, alive and holding a weapon, is by a rack */
static boolean unit_by_rack(
	long unit_index)
{
	struct unit_datum *unit = (struct unit_datum *)object_try_and_get_and_verify_type(unit_index, _object_mask_unit);
	real_point3d position;
	short slot;
	short index;

	if (!unit || unit->object.parent_object_index != NONE || TEST_FLAG(unit->object.damage_flags, _object_dead_bit))
		return FALSE;
	/* (Forge's monitor holds none) */
	for (slot = 0; slot < MAXIMUM_WEAPONS_PER_UNIT && unit->unit.weapon_object_indices[slot] == NONE; slot++)
	{
	}
	if (slot == MAXIMUM_WEAPONS_PER_UNIT)
		return FALSE;
	object_get_origin(unit_index, &position);
	for (index = 0; index < weapon_racks.rack_count; index++)
	{
		struct object_datum *rack = object_try_and_get_and_verify_type(weapon_racks.racks[index], _object_mask_scenery);
		real dx, dy, dz;

		if (!rack)
			continue;
		dx = position.x - rack->object.position.x;
		dy = position.y - rack->object.position.y;
		dz = position.z - rack->object.position.z;
		if (dx * dx + dy * dy <= WEAPON_RACK_REACH * WEAPON_RACK_REACH && fabsf(dz) <= WEAPON_RACK_HEIGHT)
			return TRUE;
	}
	return FALSE;
}

static void local_player_say(
	short local_player_index,
	wchar_t const *text)
{
	if (local_player_index != NONE)
		hud_print_message(local_player_index, text);
}

/* ---------- public code */

/* a new level's start: no rack, no player near one and no refill is remembered from the last level (a level left before its 30th tick and
begun again at tick 0 is not told from the old one by the game's time) */
void weapon_racks_reset(
	void)
{
	memset(&weapon_racks, 0, sizeof(weapon_racks));
}

void weapon_racks_tick(
	void)
{
	long now = game_time_get();
	struct data_iterator iterator;
	struct player_datum *player;

	if (!player_data)
		return;
	/* (and at once when the time went back: a level begun again) */
	if (now >= weapon_racks.next_scan_time || now < weapon_racks.next_scan_time - WEAPON_RACK_SCAN_TICKS)
		racks_scan(now);
	if (!weapon_racks.rack_count)
		return;
	data_iterator_new(&iterator, player_data);
	while ((player = data_iterator_next(&iterator)) != NULL)
	{
		long absolute = DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index);
		struct unit_datum *unit;
		boolean near;
		boolean action;

		if (absolute < 0 || absolute >= WEAPON_RACK_PLAYERS)
			continue;
		near = player->unit_index != NONE && unit_by_rack(player->unit_index);
		unit = near ? (struct unit_datum *)object_get_and_verify_type(player->unit_index, _object_mask_unit) : NULL;
		action = unit && TEST_FLAG(unit->unit.control_flags, _unit_control_action_bit);
		if (near && !weapon_racks.near[absolute])
			local_player_say(player->local_player_index, L"Weapon rack: press action to refill your ammo.");
		if (near && action && !weapon_racks.action_held[absolute] &&
			(now < weapon_racks.refilled_time[absolute] ||
			now - weapon_racks.refilled_time[absolute] >= WEAPON_RACK_REFILL_TICKS))
		{
			weapon_racks.refilled_time[absolute] = now;
			/* (the host's weapons, which its clients are told of) */
			if (!network_game_distributed_client())
				firefight_unit_weapons_refill(player->unit_index);
			local_player_say(player->local_player_index, L"Ammo refilled.");
		}
		weapon_racks.near[absolute] = near;
		weapon_racks.action_held[absolute] = action;
	}
}
