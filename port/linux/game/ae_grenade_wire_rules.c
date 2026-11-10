/*
AE_GRENADE_WIRE_RULES.C

AE's grenade message's entry (ae_grenade_wire_rules.h).
*/

#include <string.h>

#include "ae_grenade_wire_rules.h"

void ae_grenade_wire_put(unsigned char bytes[AE_GRENADE_WIRE_ENTRY_SIZE], struct ae_grenade_wire_entry const *entry)
{
	unsigned long handle;
	unsigned stall, scramble;
	int index;

	memset(bytes, 0, AE_GRENADE_WIRE_ENTRY_SIZE);
	if (!entry)
		return;
	handle = entry->handle & 0xFFFFFFFFUL;
	stall = entry->stall_ticks > 65535u ? 65535u : entry->stall_ticks;
	scramble = entry->scramble_ticks > 65535u ? 65535u : entry->scramble_ticks;
	bytes[0] = (unsigned char)entry->kind;
	bytes[1] = (unsigned char)entry->type_count;
	bytes[2] = (unsigned char)entry->mask;
	bytes[4] = (unsigned char)handle;
	bytes[5] = (unsigned char)(handle >> 8);
	bytes[6] = (unsigned char)(handle >> 16);
	bytes[7] = (unsigned char)(handle >> 24);
	for (index = 0; index < 8; index++)
		bytes[8 + index] = (unsigned char)(entry->counts[index] > 127 ? 127 : entry->counts[index]);
	bytes[16] = (unsigned char)stall;
	bytes[17] = (unsigned char)(stall >> 8);
	bytes[18] = (unsigned char)scramble;
	bytes[19] = (unsigned char)(scramble >> 8);
}

long ae_grenade_wire_handle(unsigned long handle)
{
	handle &= 0xFFFFFFFFUL;
	return handle & 0x80000000UL ? -(long)(0xFFFFFFFFUL - handle) - 1L : (long)handle;
}

int ae_grenade_wire_slot_map_matches(struct ae_grenade_wire_entry const *entry, int type_count, unsigned mask)
{
	return entry && entry->type_count == type_count && entry->mask == (mask & 0xFFu);
}

static float trip_of(float round_trip)
{
	if (!(round_trip > 0.0f))
		return 6.0f;
	return round_trip > 60.0f ? 60.0f : round_trip;
}

int ae_grenade_wire_count_taken(int host_count, int current, int own, long changed, long now, float round_trip)
{
	long trip = (long)trip_of(round_trip);

	if ((float)trip < trip_of(round_trip))
		trip++;
	return !own || host_count < current || changed == -1 || now - changed > trip + 5;
}

long ae_grenade_wire_effect_ticks(unsigned ticks, float round_trip)
{
	long left = (long)ticks - (long)(trip_of(round_trip) * 0.5f);

	return left > 0 ? left : 0;
}

int ae_grenade_wire_take(unsigned char const bytes[AE_GRENADE_WIRE_ENTRY_SIZE], struct ae_grenade_wire_entry *entry)
{
	struct ae_grenade_wire_entry taken;
	int index, any_count = 0;

	if (!bytes || !entry)
		return 0;
	memset(&taken, 0, sizeof(taken));
	taken.kind = bytes[0];
	taken.type_count = bytes[1];
	taken.mask = bytes[2];
	taken.handle = (unsigned long)bytes[4] | ((unsigned long)bytes[5] << 8) | ((unsigned long)bytes[6] << 16) |
		((unsigned long)bytes[7] << 24);
	for (index = 0; index < 8; index++)
	{
		if (bytes[8 + index] > 127)
			return 0;
		taken.counts[index] = bytes[8 + index];
		any_count |= bytes[8 + index];
	}
	taken.stall_ticks = (unsigned)bytes[16] | ((unsigned)bytes[17] << 8);
	taken.scramble_ticks = (unsigned)bytes[18] | ((unsigned)bytes[19] << 8);
	if (bytes[3] || taken.type_count < 2 || taken.type_count > 10)
		return 0;
	if (taken.kind == AE_GRENADE_WIRE_UNIT)
	{
		if (taken.stall_ticks)
			return 0;
	}
	else if (taken.kind == AE_GRENADE_WIRE_VEHICLE)
	{
		if (any_count || taken.scramble_ticks)
			return 0;
	}
	else
		return 0;
	*entry = taken;
	return 1;
}
