/*
AE_GRENADE_CATALOGUE.C

The grenade catalogue's table and rules (ae_grenade_catalogue.h).
*/

#include <stddef.h>
#include <string.h>

#include "ae_grenade_catalogue.h"

static struct ae_grenade_entry const entries[] =
{
#include "ae_grenade_catalogue_entries.inc"
};

typedef char verify_catalogue_rows[sizeof(entries) / sizeof(entries[0]) <= AE_GRENADE_CATALOGUE_IDS ? 1 : -1];

#define TICKS_PER_SECOND 30

struct ae_grenade_entry const *ae_grenade_catalogue_entry(int id)
{
	if (id < 0 || id >= (int)(sizeof(entries) / sizeof(entries[0])))
		return NULL;
	return &entries[id];
}

int ae_grenade_catalogue_id(char const *name)
{
	int id;

	if (!name)
		return -1;
	for (id = 0; id < (int)(sizeof(entries) / sizeof(entries[0])); id++)
		if (entries[id].name && !strcmp(entries[id].name, name))
			return id;
	return -1;
}

unsigned ae_grenade_catalogue_available(void)
{
	unsigned mask = 0;
	int id;

	for (id = 0; id < (int)(sizeof(entries) / sizeof(entries[0])); id++)
		if (entries[id].available)
			mask |= 1u << id;
	return mask;
}

void ae_grenade_slot_map_make(struct ae_grenade_slot_map *map, int own_count, unsigned mask)
{
	int slot, id;

	if (!map)
		return;
	memset(map, 0, sizeof(*map));
	for (slot = 0; slot < AE_GRENADE_SLOTS; slot++)
		map->ids[slot] = -1;
	if (own_count < AE_GRENADE_STOCK_SLOTS)
		own_count = AE_GRENADE_STOCK_SLOTS;
	map->own_count = own_count;
	/* (a map with more own types than the table holds: its own, as many as fit; nothing of the catalogue) */
	slot = own_count < AE_GRENADE_SLOTS ? own_count : AE_GRENADE_SLOTS;
	mask &= (1u << AE_GRENADE_CATALOGUE_IDS) - 1;
	for (id = 0; id < AE_GRENADE_CATALOGUE_IDS; id++)
	{
		struct ae_grenade_entry const *entry;

		if (!(mask & (1u << id)))
			continue;
		entry = ae_grenade_catalogue_entry(id);
		if (!entry || !entry->available)
		{
			map->refused |= 1u << id;
			continue;
		}
		if (slot >= AE_GRENADE_SLOTS)
		{
			map->refused |= 1u << id;
			map->no_slot |= 1u << id;
			continue;
		}
		map->ids[slot++] = (signed char)id;
		map->placed |= 1u << id;
	}
	map->type_count = slot;
}

int ae_grenade_slot_of(struct ae_grenade_slot_map const *map, int id)
{
	int slot;

	if (!map || id < 0)
		return -1;
	for (slot = AE_GRENADE_STOCK_SLOTS; slot < map->type_count && slot < AE_GRENADE_SLOTS; slot++)
		if (map->ids[slot] == id)
			return slot;
	return -1;
}

int ae_grenade_layer_version_needed(unsigned mask)
{
	int id, needed = 0;

	mask &= (1u << AE_GRENADE_CATALOGUE_IDS) - 1;
	for (id = 0; id < AE_GRENADE_CATALOGUE_IDS; id++)
	{
		struct ae_grenade_entry const *entry = ae_grenade_catalogue_entry(id);

		if (!(mask & (1u << id)) || !entry || !entry->available)
			continue;
		if (entry->layer_version > needed)
			needed = entry->layer_version;
	}
	return needed > AE_GRENADE_LAYER_VERSION_MAXIMUM ? AE_GRENADE_LAYER_VERSION_MAXIMUM : needed;
}

int ae_grenade_layer_version_told(int pack_loaded)
{
	return pack_loaded ? AE_GRENADE_LAYER_VERSION : 0;
}

unsigned ae_grenade_join_bits(int version, unsigned char pack_hash_first_byte)
{
	if (version < 1)
		return 0;
	if (version > AE_GRENADE_LAYER_VERSION_MAXIMUM)
		version = AE_GRENADE_LAYER_VERSION_MAXIMUM;
	return 1u | ((unsigned)version << 1) | ((unsigned)(pack_hash_first_byte >> 4) << 4);
}

int ae_grenade_join_bits_version(unsigned bits)
{
	return bits & 1u ? (int)((bits >> 1) & 7u) : 0;
}

int ae_grenade_join_bits_pack(unsigned bits)
{
	return bits & 1u ? (int)((bits >> 4) & 15u) : -1;
}

int ae_grenade_join_refused(unsigned mask, unsigned host_bits, unsigned told_bits, int local)
{
	int host_version = ae_grenade_join_bits_version(host_bits);

	if (local || !ae_grenade_layer_version_needed(mask) || !host_version)
		return 0;
	return ae_grenade_join_bits_version(told_bits) != host_version ||
		ae_grenade_join_bits_pack(told_bits) != ae_grenade_join_bits_pack(host_bits);
}

float ae_grenade_falloff(struct ae_grenade_numbers const *numbers, float distance)
{
	float full, outer;

	if (!numbers || !(distance >= 0.0f))
		return distance < 0.0f ? 1.0f : 0.0f;
	full = numbers->full_radius;
	outer = numbers->outer_radius;
	if (distance <= full)
		return 1.0f;
	if (distance > outer || !(outer > full))
		return 0.0f;
	return 1.0f - (1.0f - numbers->edge_fraction) * (distance - full) / (outer - full);
}

long ae_grenade_effect_ticks(float full_seconds, float edge_seconds, float edge_fraction, float strength)
{
	float seconds;

	if (!(strength > 0.0f) || !(full_seconds > 0.0f))
		return 0;
	if (strength >= 1.0f)
		seconds = full_seconds;
	else if (!(edge_fraction < 1.0f) || strength <= edge_fraction)
		seconds = edge_seconds * (edge_fraction > 0.0f && strength < edge_fraction ? strength / edge_fraction : 1.0f);
	else
		seconds = edge_seconds + (full_seconds - edge_seconds) * (strength - edge_fraction) / (1.0f - edge_fraction);
	return (long)(seconds * TICKS_PER_SECOND + 0.5f);
}

long ae_grenade_stall_refresh(long now, long end, long immune_until, long ticks)
{
	long hit_end;

	if (ticks <= 0)
		return end;
	/* (stalled now: refreshed; not stalled and still immune after the last: nothing) */
	if (end <= now && now < immune_until)
		return end;
	hit_end = now + ticks;
	return hit_end > end ? hit_end : end;
}

long ae_grenade_stall_immunity_end(long end, float immunity_seconds)
{
	return end + (long)(immunity_seconds * TICKS_PER_SECOND + 0.5f);
}

