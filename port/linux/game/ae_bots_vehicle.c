/*
AE_BOTS_VEHICLE.C

The bots' vehicles (ae_bots_vehicle.h): the setting, what a vehicle is (its
tag's name, its seats' flags), where a rider is in the world, and halo.log's
counts. Which seat a bot takes and when it gets out are the pure rules'
(ae_bots_vehicle_rules.c); the bot's side is ae_bots_vehicle.inc, at bots.c's
end.
*/

#include "cseries.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "memory/data.h"
#include "objects/objects.h"
#include "units/units.h"
#include "units/unit_definitions.h"
#include "units/vehicles.h"
#include "units/vehicle_definitions.h"
#include "tag_files/tag_groups.h"

#include "bots.h"
#include "ae_bots_vehicle.h"

#include <stdio.h>
#include <string.h>

/* port/linux/src/port_config.c */
long config_integer(char const *name);
void platform_log(char const *format, ...);

/* the unit definitions' seat flags (unit_definitions.h's) */
#define AE_BOTS_SEAT_DRIVER_FLAG 2
#define AE_BOTS_SEAT_GUNNER_FLAG 3

long ae_bots_vehicle_setting(void)
{
	static long tick = NONE;
	static long value = 1;
	long now = game_time_get();

	if (now != tick)
	{
		tick = now;
		value = config_integer("debug.bot_vehicles");
		value = value < 0 ? 0 : value > 2 ? 2 : value;
	}
	return value;
}

void ae_bots_vehicle_world_point(long object_index, real_point3d *point)
{
	struct object_datum *object = (struct object_datum *)object_try_and_get_and_verify_type(object_index, NONE);

	if (!object)
		return;
	if (object->object.parent_object_index == NONE)
		*point = object->object.position;
	else
		object_get_origin(object_index, point);
}

int ae_bots_vehicle_class(long vehicle_index)
{
	struct object_datum *vehicle = (struct object_datum *)object_try_and_get_and_verify_type(vehicle_index,
		_object_mask_vehicle);

	if (!vehicle)
		return AE_BOTS_VEHICLE_OTHER;
	return ae_bots_vehicle_class_of_name(tag_get_name(vehicle->definition_index));
}

short ae_bots_vehicle_seat_count(long vehicle_index)
{
	struct object_datum *vehicle = (struct object_datum *)object_try_and_get_and_verify_type(vehicle_index,
		_object_mask_vehicle);

	if (!vehicle)
		return 0;
	return (short)unit_definition_get(vehicle->definition_index)->unit.seats.count;
}

int ae_bots_vehicle_seat_role(long vehicle_index, short seat_index)
{
	struct object_datum *vehicle = (struct object_datum *)object_try_and_get_and_verify_type(vehicle_index,
		_object_mask_vehicle);
	struct unit_definition *definition;
	struct unit_seat *seat;

	if (!vehicle)
		return AE_BOTS_SEAT_NONE;
	definition = unit_definition_get(vehicle->definition_index);
	if (seat_index < 0 || seat_index >= definition->unit.seats.count)
		return AE_BOTS_SEAT_NONE;
	seat = TAG_BLOCK_GET_ELEMENT(&definition->unit.seats, seat_index, struct unit_seat);
	return ae_bots_vehicle_seat_kind(ae_bots_vehicle_class(vehicle_index), TEST_FLAG(seat->flags, AE_BOTS_SEAT_DRIVER_FLAG) != 0,
		TEST_FLAG(seat->flags, AE_BOTS_SEAT_GUNNER_FLAG) != 0);
}

/* ---------- halo.log */

static struct
{
	long game_tick;
	long report_tick;
	/* this game: seats taken by role, exits by reason, kills from a seat by role, ticks riding by role, ticks in a
	parked vehicle, ticks fighting from a seat, the longest ride (seconds) */
	long entries[AE_BOTS_SEAT_KINDS];
	long entries_by_class[AE_BOTS_VEHICLE_CLASSES];
	long exits[AE_BOTS_LEAVE_REASONS];
	long kills[AE_BOTS_SEAT_KINDS];
	long riding_ticks[AE_BOTS_SEAT_KINDS];
	long parked_ticks;
	long fighting_ticks;
	real longest_ride;
	/* entries by bot */
	long bot_entries[BOTS_MAXIMUM];
} ae_bots_vehicle_totals;

/* (a new game: its own counts) */
static void ae_bots_vehicle_counts_now(void)
{
	long now = game_time_get();

	if (now < ae_bots_vehicle_totals.game_tick)
	{
		csmemset(&ae_bots_vehicle_totals, 0, sizeof(ae_bots_vehicle_totals));
		ae_bots_vehicle_totals.report_tick = now;
	}
	ae_bots_vehicle_totals.game_tick = now;
}

void ae_bots_vehicle_count_entry(long bot_index, int vehicle_class, int seat)
{
	ae_bots_vehicle_counts_now();
	if (seat >= 0 && seat < AE_BOTS_SEAT_KINDS)
		ae_bots_vehicle_totals.entries[seat]++;
	if (vehicle_class >= 0 && vehicle_class < AE_BOTS_VEHICLE_CLASSES)
		ae_bots_vehicle_totals.entries_by_class[vehicle_class]++;
	if (bot_index >= 0 && bot_index < BOTS_MAXIMUM)
		ae_bots_vehicle_totals.bot_entries[bot_index]++;
	platform_log("bots: bot %ld took the %s seat of a %s", bot_index + 1, ae_bots_vehicle_seat_name(seat),
		ae_bots_vehicle_class_name(vehicle_class));
}

void ae_bots_vehicle_count_exit(long bot_index, int vehicle_class, int reason, real seconds)
{
	ae_bots_vehicle_counts_now();
	if (reason >= 0 && reason < AE_BOTS_LEAVE_REASONS)
		ae_bots_vehicle_totals.exits[reason]++;
	if (seconds > ae_bots_vehicle_totals.longest_ride)
		ae_bots_vehicle_totals.longest_ride = seconds;
	platform_log("bots: bot %ld left the %s (%s) after %.1f s", bot_index + 1, ae_bots_vehicle_class_name(vehicle_class),
		ae_bots_vehicle_leave_name(reason), seconds);
}

void ae_bots_vehicle_count_tick(long bot_index, int seat, boolean parked, boolean fighting)
{
	(void)bot_index;
	ae_bots_vehicle_counts_now();
	if (seat >= 0 && seat < AE_BOTS_SEAT_KINDS)
		ae_bots_vehicle_totals.riding_ticks[seat]++;
	if (parked)
		ae_bots_vehicle_totals.parked_ticks++;
	if (fighting)
		ae_bots_vehicle_totals.fighting_ticks++;
}

void ae_bots_vehicle_count_kill(long bot_index, int seat)
{
	ae_bots_vehicle_counts_now();
	if (seat >= 0 && seat < AE_BOTS_SEAT_KINDS)
		ae_bots_vehicle_totals.kills[seat]++;
	platform_log("bots: bot %ld killed from the %s seat", bot_index + 1, ae_bots_vehicle_seat_name(seat));
}

void ae_bots_vehicle_report(void)
{
	long now = game_time_get();
	char line[400];
	int length = 0, index;

	ae_bots_vehicle_counts_now();
	if (now - ae_bots_vehicle_totals.report_tick < TICKS_PER_SECOND * 60)
		return;
	ae_bots_vehicle_totals.report_tick = now;
	line[0] = 0;
	for (index = 0; index < BOTS_MAXIMUM && length < (int)sizeof(line) - 16; index++)
	{
		if (ae_bots_vehicle_totals.bot_entries[index])
			length += snprintf(line + length, sizeof(line) - (size_t)length, " %d:%ld", index + 1,
				ae_bots_vehicle_totals.bot_entries[index]);
	}
	platform_log("bots: vehicle tick %ld: seats taken gunner %ld passenger %ld driver %ld (warthog %ld, rocket warthog %ld, "
		"ghost %ld, turret %ld, other %ld); exits wrecked %ld flipped %ld no driver %ld parked %ld there %ld away %ld "
		"hold %ld stuck %ld; kills gunner %ld passenger %ld driver %ld; riding s gunner %ld passenger %ld driver %ld, "
		"parked %ld, fighting %ld, longest ride %.0f s; by bot%s",
		now, ae_bots_vehicle_totals.entries[AE_BOTS_SEAT_GUNNER], ae_bots_vehicle_totals.entries[AE_BOTS_SEAT_PASSENGER],
		ae_bots_vehicle_totals.entries[AE_BOTS_SEAT_DRIVER], ae_bots_vehicle_totals.entries_by_class[AE_BOTS_VEHICLE_WARTHOG],
		ae_bots_vehicle_totals.entries_by_class[AE_BOTS_VEHICLE_ROCKET_WARTHOG],
		ae_bots_vehicle_totals.entries_by_class[AE_BOTS_VEHICLE_GHOST],
		ae_bots_vehicle_totals.entries_by_class[AE_BOTS_VEHICLE_TURRET],
		ae_bots_vehicle_totals.entries_by_class[AE_BOTS_VEHICLE_OTHER],
		ae_bots_vehicle_totals.exits[AE_BOTS_LEAVE_WRECKED], ae_bots_vehicle_totals.exits[AE_BOTS_LEAVE_FLIPPED],
		ae_bots_vehicle_totals.exits[AE_BOTS_LEAVE_NO_DRIVER], ae_bots_vehicle_totals.exits[AE_BOTS_LEAVE_PARKED],
		ae_bots_vehicle_totals.exits[AE_BOTS_LEAVE_THERE], ae_bots_vehicle_totals.exits[AE_BOTS_LEAVE_AWAY],
		ae_bots_vehicle_totals.exits[AE_BOTS_LEAVE_HOLD], ae_bots_vehicle_totals.exits[AE_BOTS_LEAVE_STUCK],
		ae_bots_vehicle_totals.kills[AE_BOTS_SEAT_GUNNER], ae_bots_vehicle_totals.kills[AE_BOTS_SEAT_PASSENGER],
		ae_bots_vehicle_totals.kills[AE_BOTS_SEAT_DRIVER],
		ae_bots_vehicle_totals.riding_ticks[AE_BOTS_SEAT_GUNNER] / TICKS_PER_SECOND,
		ae_bots_vehicle_totals.riding_ticks[AE_BOTS_SEAT_PASSENGER] / TICKS_PER_SECOND,
		ae_bots_vehicle_totals.riding_ticks[AE_BOTS_SEAT_DRIVER] / TICKS_PER_SECOND,
		ae_bots_vehicle_totals.parked_ticks / TICKS_PER_SECOND, ae_bots_vehicle_totals.fighting_ticks / TICKS_PER_SECOND,
		ae_bots_vehicle_totals.longest_ride, line);
}
