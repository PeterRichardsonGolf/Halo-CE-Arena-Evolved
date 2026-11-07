/*
AE_GRENADES_NETWORK.C

AE's grenade message (_distributed_message_ae_grenades, 108; the entry's bytes: ae_grenade_wire_rules.h), the
host's and the clients' sides. Only in a game whose grenade layer is on (a mask not 0, and a catalogue type on the
map): a game without sends nothing, the wire as before.

- The host sends, reliably, each player unit's extra grenade counts (slots 2-9) and scramble, and each vehicle's
  stall: when they change, and every 2 seconds for each while it has any, as a repair.
- A client takes an entry only when the host's slot map (type count and mask) is its own. Counts: the host's, but
  for its own players' units the round trip guard (as MCC's message: mcc_network.c): a count its own throw just
  lowered is kept until the host's lower one comes back. Effects: the end tick from the ticks left, less half the
  round trip, so the driver's own prediction stalls too (ae_grenades_stall_controls) and a scramble ends in time.
*/

#include "cseries.h"
#include "cseries/errors.h"
#include "game/game.h"
#include "game/players.h"
#include "objects/objects.h"
#include "units/units.h"
#include "network_distributed.h"

#include <math.h>

#include "ae_grenade_catalogue.h"
#include "ae_grenade_wire_rules.h"
#include "ae_grenades.h"
#include "mcc_grenades.h"

boolean network_objects_client_has(long object_index);

/* the repair's interval */
#define REPAIR_TICKS (2 * TICKS_PER_SECOND)
/* the entries one message holds */
#define ENTRIES_PER_MESSAGE 32

struct sent_unit
{
	long unit;
	unsigned char counts[8];
	long scramble_left;
	long sent;
};
static struct sent_unit sent_units[MAXIMUM_TRACKED_OBJECTS];

/* a client's own units' counts, and when its own throw lowered each */
struct own_unit
{
	long unit;
	unsigned char counts[8];
	long changed[8];
};
static struct own_unit own_units[MAXIMUM_TRACKED_OBJECTS];
static long refused_logged = NONE;
static long received_entries;
static boolean silent_host_logged;

void ae_grenades_network_new_game(void)
{
	long slot;
	short type;

	for (slot = 0; slot < MAXIMUM_TRACKED_OBJECTS; slot++)
	{
		memset(&sent_units[slot], 0, sizeof(sent_units[slot]));
		sent_units[slot].unit = NONE;
		sent_units[slot].sent = NONE;
		memset(&own_units[slot], 0, sizeof(own_units[slot]));
		own_units[slot].unit = NONE;
		for (type = 0; type < 8; type++)
			own_units[slot].changed[type] = NONE;
	}
	refused_logged = NONE;
	received_entries = 0;
	silent_host_logged = FALSE;
}

word ae_grenades_network_entry_size(void)
{
	return AE_GRENADE_WIRE_ENTRY_SIZE;
}

/* ---------- the host */

struct outgoing
{
	struct distributed_message_header header;
	byte entries[ENTRIES_PER_MESSAGE][AE_GRENADE_WIRE_ENTRY_SIZE];
};

static void outgoing_flush(struct outgoing *message, short *count)
{
	if (!*count)
		return;
	distributed_send(message, _distributed_message_ae_grenades, *count,
		(word)(sizeof(message->header) + *count * AE_GRENADE_WIRE_ENTRY_SIZE), _distributed_to_clients_reliably);
	*count = 0;
}

static void outgoing_add(struct outgoing *message, short *count, struct ae_grenade_wire_entry const *entry)
{
	ae_grenade_wire_put(message->entries[*count], entry);
	if (++*count == ENTRIES_PER_MESSAGE)
		outgoing_flush(message, count);
}

void ae_grenades_network_host_tick(void)
{
	static struct outgoing message;
	struct ae_grenade_wire_entry entry;
	short count = 0, type_count = mcc_grenades_type_count(), slot;
	long now = game_time_get(), cursor = 0, handle;
	struct data_iterator iterator;
	struct player_datum *player;

	if (!ae_grenades_layer_on() || game_connection() != _game_connection_network_server)
		return;
	memset(&entry, 0, sizeof(entry));
	entry.type_count = type_count;
	entry.mask = ae_grenades_mask();
	/* (each player's living unit: its counts and scramble, on a change or for the repair) */
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		long unit_index = player->unit_index, absolute, scramble;
		struct sent_unit *sent;
		boolean changed;

		if (unit_index == NONE || !unit_try_and_get(unit_index) || !distributed_object_index_valid(unit_index))
			continue;
		absolute = DATUM_INDEX_TO_ABSOLUTE_INDEX(unit_index);
		if (absolute < 0 || absolute >= MAXIMUM_TRACKED_OBJECTS)
			continue;
		entry.kind = AE_GRENADE_WIRE_UNIT;
		entry.handle = (unsigned long)unit_index;
		for (slot = 2; slot < AE_GRENADE_SLOTS; slot++)
			entry.counts[slot - 2] = (unsigned char)(slot < type_count ? mcc_grenades_get(unit_index, slot) : 0);
		scramble = ae_grenades_scramble_ticks_left(unit_index);
		entry.scramble_ticks = (unsigned)scramble;
		entry.stall_ticks = 0;
		sent = &sent_units[absolute];
		changed = sent->unit != unit_index || memcmp(sent->counts, entry.counts, sizeof(entry.counts)) ||
			ae_grenades_state_dirty(unit_index) || (sent->scramble_left > 0) != (scramble > 0);
		if (!changed && (sent->sent != NONE && now - sent->sent < REPAIR_TICKS))
			continue;
		sent->unit = unit_index;
		memcpy(sent->counts, entry.counts, sizeof(sent->counts));
		sent->scramble_left = scramble;
		sent->sent = now;
		ae_grenades_state_sent(unit_index);
		outgoing_add(&message, &count, &entry);
	}
	/* (each vehicle with a stall, or whose stall just changed) */
	memset(entry.counts, 0, sizeof(entry.counts));
	entry.scramble_ticks = 0;
	while ((handle = ae_grenades_state_next(&cursor)) != NONE)
	{
		if (!object_try_and_get_and_verify_type(handle, _object_mask_vehicle) || !distributed_object_index_valid(handle))
			continue;
		if (!ae_grenades_state_dirty(handle) && now % REPAIR_TICKS)
			continue;
		entry.kind = AE_GRENADE_WIRE_VEHICLE;
		entry.handle = (unsigned long)handle;
		entry.stall_ticks = (unsigned)ae_grenades_stall_ticks_left(handle);
		ae_grenades_state_sent(handle);
		outgoing_add(&message, &count, &entry);
		error(_error_silent, "grenades: sent vehicle %08lX's stall, %u ticks left", (unsigned long)handle,
			entry.stall_ticks);
	}
	outgoing_flush(&message, &count);
}

/* ---------- a client */

static struct own_unit *own_note(long unit_index, boolean compare)
{
	long slot = DATUM_INDEX_TO_ABSOLUTE_INDEX(unit_index);
	struct own_unit *own;
	short type;

	if (slot < 0 || slot >= MAXIMUM_TRACKED_OBJECTS)
		return NULL;
	own = &own_units[slot];
	if (own->unit != unit_index)
	{
		own->unit = unit_index;
		compare = FALSE;
		for (type = 0; type < 8; type++)
			own->changed[type] = NONE;
	}
	for (type = 0; type < 8; type++)
	{
		unsigned char count = (unsigned char)mcc_grenades_get(unit_index, (short)(type + 2));

		if (compare && count < own->counts[type])
			own->changed[type] = game_time_get();
		own->counts[type] = count;
	}
	return own;
}

void ae_grenades_network_client_tick(void)
{
	short local;

	if (!ae_grenades_layer_on() || game_connection() != _game_connection_network_client)
		return;
	/* (a host that sends nothing in a game this machine reads a mask in: a build without the grenade layer hosting a
	gametype saved with one; its grenades play as that host has them, and this says so once) */
	if (!received_entries && !silent_host_logged && game_time_get() > 15 * TICKS_PER_SECOND)
	{
		silent_host_logged = TRUE;
		error(_error_silent, "grenades: the host runs no grenade layer (no grenade message 15 s into a game whose mask "
			"here is 0x%02X): this machine's grenade types are not the host's; its game plays as the host has it",
			ae_grenades_mask());
	}
	for (local = 0; local < MAXIMUM_LOCAL_PLAYERS; local++)
	{
		long player_index = local_player_get_player_index(local);
		struct player_datum *player = player_index != NONE ? player_try_and_get(player_index) : NULL;
		long unit = distributed_living_unit(player);

		if (unit != NONE)
			own_note(unit, TRUE);
	}
}

void ae_grenades_network_handle(void const *entries, short count)
{
	short index, type;
	real round_trip;

	if (!ae_grenades_layer_on() || game_connection() != _game_connection_network_client || count < 0)
		return;
	round_trip = distributed_own_round_trip_ticks();
	for (index = 0; index < count; index++)
	{
		struct ae_grenade_wire_entry entry;
		long handle;

		if (!ae_grenade_wire_take((byte const *)entries + index * AE_GRENADE_WIRE_ENTRY_SIZE, &entry))
			continue;
		if (!ae_grenade_wire_slot_map_matches(&entry, mcc_grenades_type_count(), ae_grenades_mask()))
		{
			if (refused_logged == NONE || game_time_get() - refused_logged > 10 * TICKS_PER_SECOND)
			{
				refused_logged = game_time_get();
				error(_error_silent, "grenades: the host's grenade state is for another slot map (host %d types, mask "
					"0x%02X; here %d, 0x%02X): not taken", entry.type_count, entry.mask,
					(int)mcc_grenades_type_count(), ae_grenades_mask());
			}
			continue;
		}
		handle = ae_grenade_wire_handle(entry.handle);
		if (!distributed_object_index_valid(handle) || !network_objects_client_has(handle))
			continue;
		received_entries++;
		if (entry.kind == AE_GRENADE_WIRE_VEHICLE)
		{
			long ticks = ae_grenade_wire_effect_ticks(entry.stall_ticks, round_trip);

			if (!object_try_and_get_and_verify_type(handle, _object_mask_vehicle))
				continue;
			if (ticks > 0 && !ae_grenades_vehicle_stalled(handle))
				error(_error_silent, "grenades: the host stalls vehicle %08lX: %ld ticks here", (unsigned long)handle,
					ticks);
			ae_grenades_state_set_stall(handle, ticks);
			continue;
		}
		{
			struct unit_datum *unit = (struct unit_datum *)object_try_and_get_and_verify_type(handle, _object_mask_unit);
			struct own_unit *own = NULL;
			long ticks = ae_grenade_wire_effect_ticks(entry.scramble_ticks, round_trip);

			if (!unit)
				continue;
			if (unit->unit.player_index != NONE && distributed_player_is_local(unit->unit.player_index))
				own = own_note(handle, TRUE);
			for (type = 0; type < 8 && type + 2 < mcc_grenades_type_count(); type++)
			{
				short current = mcc_grenades_get(handle, (short)(type + 2));

				if (ae_grenade_wire_count_taken(entry.counts[type], current, own != NULL, own ? own->changed[type] : NONE,
					game_time_get(), round_trip))
				{
					if (current != entry.counts[type])
						error(_error_silent, "grenades: the host says unit %08lX has %d of grenade slot %d", (unsigned long)handle,
							(int)entry.counts[type], (int)(type + 2));
					mcc_grenades_set(handle, (short)(type + 2), entry.counts[type]);
				}
			}
			if (own)
				own_note(handle, FALSE);
			if (ticks > 0 && !ae_grenades_scramble_ticks_left(handle))
				error(_error_silent, "grenades: the host scrambles unit %08lX: %ld ticks here", (unsigned long)handle,
					ticks);
			ae_grenades_state_set_scramble(handle, ticks);
		}
	}
}
