/*
AE_HIJACK.C

HIJACK (ae_hijack.h; the rules: ae_hijack_rules.h), the gametype's hijack. The host decides: it throws the driver out
of the seat (the instant exit, then a push to the side away from the hijacker and up; no damage) and seats the
hijacker; the existing object sync tells every machine. The prompt is offered on every machine from the synced
setting. The hijacker climbs in with the stock seat-entry view (third person). Log: "hijack: player x threw out
player y".
*/

#include "cseries.h"
#include "game/game.h"
#include "game/game_allegiance.h"
#include "game/players.h"
#include "math/real_math.h"
#include "memory/data.h"
#include "networking/network_game_globals.h"
#include "objects/objects.h"
#include "units/unit_definitions.h"
#include "units/units.h"

#include "ae_h2_objectives.h"
#include "infection.h"
#include "ae_hijack.h"
#include "ae_hijack_rules.h"
#include "ae_variant_ext.h"

#include <string.h>

void platform_log(const char *format, ...);

#define AE_HIJACK_THROWN_MOST 8

static struct
{
	struct { long unit_index, until_tick; } thrown[AE_HIJACK_THROWN_MOST];
	long last_tick;
} ae_hijack = { 0 };

/* (a new game: its game time starts again below the last one seen; the table is the old game's) */
static void ae_hijack_new_game_check(long now)
{
	if (now < ae_hijack.last_tick)
		memset(ae_hijack.thrown, 0, sizeof(ae_hijack.thrown));
	ae_hijack.last_tick = now;
}

static int ae_hijack_mode(void)
{
	int mode;

	/* (held from 0.8.0, see ae_hijack_rules.h: a saved gametype's byte plays as OFF) */
	if (!AE_HIJACK_SHIPPED)
		return AE_EXT_HIJACK_OFF;
	mode = ae_variant_ext_game()->hijack;

	return mode < AE_EXT_HIJACK_COUNT ? mode : AE_EXT_HIJACK_OFF;
}

/* (set while ae_hijack_try seats the hijacker: the hijack is an explicit act, whatever MIXED-TEAM VEHICLES says) */
static boolean ae_hijack_taking_seat = FALSE;

boolean ae_hijack_mixed_riders_now(void)
{
	int mixed = ae_variant_ext_game()->mixed_vehicles;

	return ae_hijack_mixed_riders(mixed < AE_EXT_MIXED_VEHICLES_COUNT ? mixed : AE_EXT_MIXED_VEHICLES_OFF,
		ae_hijack_taking_seat ? 1 : 0) ? TRUE : FALSE;
}

boolean ae_hijack_locked_out(long unit_index)
{
	long slot, now = game_time_get();

	ae_hijack_new_game_check(now);
	for (slot = 0; slot < AE_HIJACK_THROWN_MOST; slot++)
	{
		if (ae_hijack.thrown[slot].unit_index == unit_index && unit_index != NONE &&
			now >= ae_hijack.thrown[slot].until_tick - AE_HIJACK_LOCKOUT_TICKS && now < ae_hijack.thrown[slot].until_tick)
		{
			return TRUE;
		}
	}
	return FALSE;
}

static void ae_hijack_lock(long unit_index)
{
	long slot, oldest = 0, now = game_time_get();

	for (slot = 0; slot < AE_HIJACK_THROWN_MOST; slot++)
	{
		if (ae_hijack.thrown[slot].unit_index == unit_index || ae_hijack.thrown[slot].until_tick <= now)
		{
			oldest = slot;
			break;
		}
		if (ae_hijack.thrown[slot].until_tick < ae_hijack.thrown[oldest].until_tick)
			oldest = slot;
	}
	ae_hijack.thrown[oldest].unit_index = unit_index;
	ae_hijack.thrown[oldest].until_tick = now + AE_HIJACK_LOCKOUT_TICKS;
}

static boolean ae_hijack_driver_seat(long vehicle_index, short seat_index)
{
	struct unit_datum *vehicle = unit_get(vehicle_index);
	struct unit_seat *seat;

	if (seat_index < 0 || seat_index >= unit_definition_get(vehicle->definition_index)->unit.seats.count)
		return FALSE;
	seat = TAG_BLOCK_GET_ELEMENT(&unit_definition_get(vehicle->definition_index)->unit.seats, seat_index, struct unit_seat);
	return TEST_FLAG(seat->flags, _unit_seat_driver_bit) ? TRUE : FALSE;
}

boolean ae_hijack_offer(long unit_index, long occupant_unit_index, long vehicle_index, short seat_index)
{
	int mode = ae_hijack_mode();
	struct unit_datum *unit, *occupant, *vehicle;
	real_vector3d apart;

	if (mode == AE_EXT_HIJACK_OFF || unit_index == NONE || occupant_unit_index == NONE || vehicle_index == NONE)
		return FALSE;
	unit = unit_get(unit_index);
	occupant = unit_get(occupant_unit_index);
	vehicle = unit_get(vehicle_index);
	if (unit->unit.player_index == NONE || occupant->unit.player_index == NONE || occupant->unit.actor_index != NONE ||
		unit->object.parent_object_index != NONE || occupant->object.parent_object_index != vehicle_index ||
		!ae_hijack_driver_seat(vehicle_index, seat_index) || ae_hijack_locked_out(unit_index) ||
		ae_h2_seat_refused(unit_index, vehicle_index, seat_index) || infection_unit_is_zombie(unit_index))
	{
		return FALSE;
	}
	if (!ae_hijack_allowed(mode, unit->unit.player_index == occupant->unit.player_index,
		game_team_is_enemy(unit->object.owner_team_index, occupant->object.owner_team_index)))
	{
		return FALSE;
	}
	/* (velocities are a tick's; the gate is world units a second) */
	apart.i = unit->object.translational_velocity.i - vehicle->object.translational_velocity.i;
	apart.j = unit->object.translational_velocity.j - vehicle->object.translational_velocity.j;
	apart.k = unit->object.translational_velocity.k - vehicle->object.translational_velocity.k;
	return ae_hijack_speed_ok((float)(magnitude3d(&apart) * TICKS_PER_SECOND)) ? TRUE : FALSE;
}

boolean ae_hijack_try(long unit_index, long vehicle_index, short seat_index, long occupant_unit_index)
{
	struct unit_datum *unit, *occupant, *vehicle;
	real_vector3d right;
	real side, scale;
	boolean seated;

	if (network_game_distributed_client() || !ae_hijack_offer(unit_index, occupant_unit_index, vehicle_index, seat_index))
		return FALSE;
	unit = unit_get(unit_index);
	occupant = unit_get(occupant_unit_index);
	vehicle = unit_get(vehicle_index);
	/* (the vehicle's right: its forward across its up; both are unit vectors at right angles) */
	right.i = vehicle->object.forward.j * vehicle->object.up.k - vehicle->object.forward.k * vehicle->object.up.j;
	right.j = vehicle->object.forward.k * vehicle->object.up.i - vehicle->object.forward.i * vehicle->object.up.k;
	right.k = vehicle->object.forward.i * vehicle->object.up.j - vehicle->object.forward.j * vehicle->object.up.i;
	side = (unit->object.position.x - vehicle->object.position.x) * right.i +
		(unit->object.position.y - vehicle->object.position.y) * right.j +
		(unit->object.position.z - vehicle->object.position.z) * right.k;
	scale = (side > 0.f ? -AE_HIJACK_THROW_SIDE : AE_HIJACK_THROW_SIDE) / TICKS_PER_SECOND;
	unit_exit_seat_end(occupant_unit_index);
	occupant->object.translational_velocity.i = vehicle->object.translational_velocity.i + right.i * scale;
	occupant->object.translational_velocity.j = vehicle->object.translational_velocity.j + right.j * scale;
	occupant->object.translational_velocity.k = vehicle->object.translational_velocity.k + right.k * scale +
		AE_HIJACK_THROW_UP / TICKS_PER_SECOND;
	ae_hijack_lock(occupant_unit_index);
	ae_hijack_taking_seat = TRUE;
	seated = unit_can_enter_seat(unit_index, vehicle_index, seat_index, NULL) &&
		unit_enter_seat(unit_index, vehicle_index, seat_index);
	ae_hijack_taking_seat = FALSE;
	if (!seated)
	{
		platform_log("hijack: player %ld threw out player %ld but could not take the seat",
			DATUM_INDEX_TO_ABSOLUTE_INDEX(unit->unit.player_index), DATUM_INDEX_TO_ABSOLUTE_INDEX(occupant->unit.player_index));
		return TRUE;
	}
	platform_log("hijack: player %ld threw out player %ld", DATUM_INDEX_TO_ABSOLUTE_INDEX(unit->unit.player_index),
		DATUM_INDEX_TO_ABSOLUTE_INDEX(occupant->unit.player_index));
	return TRUE;
}
