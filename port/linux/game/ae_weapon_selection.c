/*
AE_WEAPON_SELECTION.C

What a gametype names of the AE weapon cache: the pure rules of ae_weapon_selection.h (design section 7).
*/

#include <string.h>

#include "ae_weapon_selection.h"

/* ---------- sets */

void ae_weapon_id_set_clear(struct ae_weapon_id_set *set)
{
	memset(set, 0, sizeof(*set));
}

int ae_weapon_id_set_has(struct ae_weapon_id_set const *set, unsigned int id)
{
	unsigned int index;

	for (index = 0; index < set->count; index++)
	{
		if (set->ids[index] == id)
			return 1;
	}
	return 0;
}

int ae_weapon_id_set_add(struct ae_weapon_id_set *set, unsigned int id)
{
	unsigned int at;

	if (!id || set->count >= AE_WEAPON_SELECTION_MAX_NAMED || ae_weapon_id_set_has(set, id))
		return 0;
	for (at = set->count; at > 0 && set->ids[at - 1] > id; at--)
		set->ids[at] = set->ids[at - 1];
	set->ids[at] = id;
	set->count++;
	return 1;
}

int ae_weapon_id_set_remove(struct ae_weapon_id_set *set, unsigned int id)
{
	unsigned int at;

	for (at = 0; at < set->count; at++)
	{
		if (set->ids[at] == id)
		{
			for (; at + 1 < set->count; at++)
				set->ids[at] = set->ids[at + 1];
			set->count--;
			set->ids[set->count] = 0;
			return 1;
		}
	}
	return 0;
}

/* ---------- the stored range */

static int weapon_in_range(unsigned int weapon)
{
	return weapon >= 1 && weapon <= 0xFFFFu;
}

static int config_in_range(unsigned int config)
{
	return config > AE_WEAPON_SELECTION_CONFIG_BASE && config <= AE_WEAPON_SELECTION_CONFIG_BASE + 0xFFFFu;
}

/* ---------- the selection */

void ae_weapon_selection_clear(struct ae_weapon_selection *selection)
{
	memset(selection, 0, sizeof(*selection));
}

void ae_weapon_selection_derive(struct ae_weapon_selection *selection)
{
	selection->uses_cache = selection->named.count || selection->start[0].weapon || selection->start[1].weapon ||
		selection->start[0].config || selection->start[1].config || selection->grenades || selection->ae_weapons;
}

int ae_weapon_selection_can_name(struct ae_weapon_selection const *selection, unsigned int weapon)
{
	(void)selection;
	return weapon >= 1 && weapon <= AE_WEAPON_SELECTION_LEGACY_LAST;
}

int ae_weapon_selection_guard_refuses(int network_game, int game_uses_cache)
{
	return network_game && !game_uses_cache;
}

static void start_clear(struct ae_weapon_start *start)
{
	memset(start, 0, sizeof(*start));
}

void ae_weapon_selection_sanitize(struct ae_weapon_selection *selection)
{
	unsigned int index;

	/* (ids past the legacy 16 name nothing now: the set draws the rest; id 0 names nothing) */
	for (index = selection->named.count; index > 0; index--)
	{
		unsigned int id = selection->named.ids[index - 1];

		if (id < 1 || id > AE_WEAPON_SELECTION_LEGACY_LAST)
			ae_weapon_id_set_remove(&selection->named, id);
	}
	selection->grenades &= 0xFFu;
	selection->ae_weapons = selection->ae_weapons ? 1 : 0;
	/* (a set id past the shipped range reads as casual; no toggle, no set) */
	if (!selection->ae_weapons || selection->set_id > AE_WEAPON_SELECTION_SET_LAST)
		selection->set_id = 0;
	for (index = 0; index < 2; index++)
	{
		struct ae_weapon_start *start = &selection->start[index];

		if (!start->weapon || !weapon_in_range(start->weapon))
			start_clear(start);
		else if (start->config && !config_in_range(start->config))
			start->config = 0;
		/* (a plain legacy start names what it starts with) */
		if (start->weapon && !start->config && start->weapon <= AE_WEAPON_SELECTION_LEGACY_LAST)
			ae_weapon_id_set_add(&selection->named, start->weapon);
	}
	ae_weapon_selection_derive(selection);
}

static unsigned int get_u16(unsigned char const *at)
{
	return (unsigned int)at[0] | ((unsigned int)at[1] << 8);
}

static void put_u16(unsigned char *at, unsigned int value)
{
	at[0] = (unsigned char)value;
	at[1] = (unsigned char)(value >> 8);
}

static unsigned int get_u32(unsigned char const *at)
{
	return get_u16(at) | (get_u16(at + 2) << 16);
}

static void put_u32(unsigned char *at, unsigned int value)
{
	put_u16(at, value);
	put_u16(at + 2, value >> 16);
}

/* (the payload's places) */
enum
{
	AECW_FLAGS = 0,
	AECW_RESERVED_1 = 1,
	AECW_SET = 2,
	AECW_RESERVED_3 = 3,
	AECW_START = 4
};

static unsigned int config_stored(unsigned int config)
{
	return config ? config - AE_WEAPON_SELECTION_CONFIG_BASE : 0;
}

static unsigned int config_read(unsigned int stored)
{
	return stored ? AE_WEAPON_SELECTION_CONFIG_BASE + stored : 0;
}

unsigned int ae_weapon_selection_old_grenade_bits(unsigned char pack_start_byte)
{
	return (unsigned int)(pack_start_byte >> 5) & 7u;
}

void ae_weapon_selection_read(struct ae_weapon_selection *selection, struct ae_weapon_selection_bytes const *bytes,
	unsigned int const rank_start_slot[2])
{
	unsigned int index;
	unsigned int aecw_start_weapon[2] = { 0, 0 }, aecw_start_config[2] = { 0, 0 };

	ae_weapon_selection_clear(selection);
	for (index = 0; index < 8; index++)
	{
		if (bytes->weapon_pack & (1u << index))
			ae_weapon_id_set_add(&selection->named, index + 1);
		if (bytes->weapon_pack_high & (1u << index))
			ae_weapon_id_set_add(&selection->named, index + 9);
	}
	if (bytes->aecw_present)
	{
		selection->ae_weapons = (bytes->aecw[AECW_FLAGS] & AE_WEAPON_SELECTION_AECW_FLAG_AE_WEAPONS) ? 1 : 0;
		selection->set_id = bytes->aecw[AECW_SET];
		for (index = 0; index < 2; index++)
		{
			aecw_start_weapon[index] = get_u16(bytes->aecw + AECW_START + 4 * index);
			aecw_start_config[index] = config_read(get_u16(bytes->aecw + AECW_START + 4 * index + 2));
		}
	}
	/* (the grenade mask: its own byte, else the START bytes' high bits a 0.7.6 test build wrote, read once) */
	selection->grenades = bytes->grenades;
	if (!selection->grenades)
	{
		selection->grenades = ae_weapon_selection_old_grenade_bits(bytes->pack_start[0]) |
			(ae_weapon_selection_old_grenade_bits(bytes->pack_start[1]) << 3);
	}
	for (index = 0; index < 2; index++)
	{
		unsigned int stored = bytes->pack_start[index] & 0x1Fu;

		if (aecw_start_weapon[index])
		{
			selection->start[index].weapon = aecw_start_weapon[index];
			selection->start[index].config = aecw_start_config[index];
		}
		else if (stored)
		{
			selection->start[index].weapon = stored;
		}
		else if (rank_start_slot && rank_start_slot[index])
		{
			selection->start[index].weapon = rank_start_slot[index];
		}
	}
	ae_weapon_selection_sanitize(selection);
}

void ae_weapon_selection_write(struct ae_weapon_selection_bytes *bytes, struct ae_weapon_selection const *selection)
{
	struct ae_weapon_selection clean = *selection;
	unsigned int index;
	unsigned int aecw_start[2][2] = { { 0, 0 }, { 0, 0 } };

	ae_weapon_selection_sanitize(&clean);
	memset(bytes, 0, sizeof(*bytes));
	for (index = 0; index < clean.named.count; index++)
	{
		unsigned int id = clean.named.ids[index];

		if (id <= 8)
			bytes->weapon_pack |= (unsigned char)(1u << (id - 1));
		else
			bytes->weapon_pack_high |= (unsigned char)(1u << (id - 9));
	}
	bytes->grenades = (unsigned char)(clean.grenades & 0xFFu);
	for (index = 0; index < 2; index++)
	{
		struct ae_weapon_start const *start = &clean.start[index];

		if (!start->weapon)
			continue;
		if (start->weapon <= AE_WEAPON_SELECTION_LEGACY_LAST && !start->config)
		{
			bytes->pack_start[index] = (unsigned char)start->weapon;
		}
		else
		{
			aecw_start[index][0] = start->weapon;
			aecw_start[index][1] = start->config;
		}
	}
	for (index = 0; index < 2; index++)
	{
		put_u16(bytes->aecw + AECW_START + 4 * index, aecw_start[index][0]);
		put_u16(bytes->aecw + AECW_START + 4 * index + 2, config_stored(aecw_start[index][1]));
	}
	bytes->aecw[AECW_FLAGS] = (unsigned char)(clean.ae_weapons ? AE_WEAPON_SELECTION_AECW_FLAG_AE_WEAPONS : 0);
	bytes->aecw[AECW_SET] = (unsigned char)clean.set_id;
	bytes->aecw_present = clean.ae_weapons || aecw_start[0][0] || aecw_start[1][0];
	if (!bytes->aecw_present)
		memset(bytes->aecw, 0, sizeof(bytes->aecw));
}

unsigned int ae_weapon_selection_aecw_encode(struct ae_weapon_selection_bytes const *bytes, unsigned char *block)
{
	if (!bytes->aecw_present)
		return 0;
	memset(block, 0, AE_WEAPON_SELECTION_AECW_SIZE);
	put_u32(block, AE_WEAPON_SELECTION_AECW_MAGIC);
	put_u16(block + 4, AE_WEAPON_SELECTION_AECW_VERSION);
	put_u16(block + 6, AE_WEAPON_SELECTION_AECW_PAYLOAD_SIZE);
	memcpy(block + 8, bytes->aecw, AE_WEAPON_SELECTION_AECW_PAYLOAD_SIZE);
	return AE_WEAPON_SELECTION_AECW_SIZE;
}

int ae_weapon_selection_aecw_decode(struct ae_weapon_selection_bytes *bytes, unsigned char const *block)
{
	unsigned char const *payload = block + 8;

	/* (exactly version 3: 1 and 2 were never shipped, and are refused as 2 refused 1 and 3) */
	if (get_u32(block) != AE_WEAPON_SELECTION_AECW_MAGIC || get_u16(block + 4) != AE_WEAPON_SELECTION_AECW_VERSION ||
		get_u16(block + 6) != AE_WEAPON_SELECTION_AECW_PAYLOAD_SIZE)
		return 0;
	if ((payload[AECW_FLAGS] & ~AE_WEAPON_SELECTION_AECW_FLAG_AE_WEAPONS) || payload[AECW_RESERVED_1] ||
		payload[AECW_RESERVED_3] || payload[AECW_SET] > AE_WEAPON_SELECTION_SET_LAST)
		return 0;
	memcpy(bytes->aecw, payload, AE_WEAPON_SELECTION_AECW_PAYLOAD_SIZE);
	bytes->aecw_present = 1;
	return 1;
}
