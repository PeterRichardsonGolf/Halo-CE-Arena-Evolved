/*
AE_WEAPON_SLOTS.C

Several weapon packs in one game: the pure rules (ae_weapon_slots.h). No engine includes.
*/

#include <ctype.h>
#include <string.h>

#include "ae_weapon_slots.h"
#include "ae_weapon_registry.h"

#define KNOWN_MASK ((1u << AE_WEAPON_SLOT_COUNT) - 1)

/* (the capability levels of the join mark, the slot packs' 0.7.x scheme: only the first AE_WEAPON_SLOT_COUNT slots have one;
the names, displays, layout words and stock fallbacks are the registry's (ae_weapon_registry.h), for slots 1-24) */
static int const slot_capability[AE_WEAPON_SLOT_COUNT + 1] =
{
	0,
	AE_WEAPON_SLOTS_CAPABILITY_BR,
	AE_WEAPON_SLOTS_CAPABILITY_SMG,
	AE_WEAPON_SLOTS_CAPABILITY_SMG,
	AE_WEAPON_SLOTS_CAPABILITY_CARBINE,
	AE_WEAPON_SLOTS_CAPABILITY_AR_SILENCED,
	AE_WEAPON_SLOTS_CAPABILITY_DMR,
	AE_WEAPON_SLOTS_CAPABILITY_EVO_RIFLE,
	AE_WEAPON_SLOTS_CAPABILITY_NEEDLER,
	AE_WEAPON_SLOTS_CAPABILITY_SNIPER_BOLT,
	AE_WEAPON_SLOTS_CAPABILITY_MAGNUM_SCOPED,
	AE_WEAPON_SLOTS_CAPABILITY_BRUTE_SHOT,
	AE_WEAPON_SLOTS_CAPABILITY_AR_MODERN,
	AE_WEAPON_SLOTS_CAPABILITY_SIDEARM,
	AE_WEAPON_SLOTS_CAPABILITY_ENERGY_SWORD
};

static int valid(short slot)
{
	return slot >= 1 && slot <= AE_WEAPON_SLOT_COUNT;
}

unsigned int ae_weapon_slots_mask(unsigned char weapon_pack)
{
	/* (every bit of the byte is a slot now: AE_WEAPON_SLOTS_BYTE_MAXIMUM is 255) */
	return (unsigned int)weapon_pack & KNOWN_MASK;
}

unsigned int ae_weapon_slots_mask_wide(unsigned char weapon_pack, unsigned char weapon_pack_high)
{
	return ((unsigned int)weapon_pack | ((unsigned int)weapon_pack_high << 8)) & KNOWN_MASK;
}

unsigned char ae_weapon_slots_low_byte(unsigned int mask)
{
	return (unsigned char)(mask & 0xFFu);
}

unsigned char ae_weapon_slots_high_byte(unsigned int mask)
{
	return (unsigned char)((mask >> 8) & 0xFFu);
}

unsigned int ae_weapon_slots_all(void)
{
	return (1u << AE_WEAPON_SLOT_SEEDED_COUNT) - 1;
}

short ae_weapon_slots_first(unsigned int mask)
{
	short slot;

	for (slot = 1; slot <= AE_WEAPON_SLOT_COUNT; slot++)
		if (mask & AE_WEAPON_SLOT_BIT(slot))
			return slot;
	return AE_WEAPON_SLOT_NONE;
}

short ae_weapon_slots_loadout(unsigned int mask, unsigned int rank)
{
	short slot, seen = 0;

	/* (a loadout rank is 1 to AE_WEAPON_SLOTS_RANK_MAXIMUM: the 8th named pack, the NEEDLER, is never a start) */
	if (rank < 1 || rank > AE_WEAPON_SLOTS_RANK_MAXIMUM)
		return AE_WEAPON_SLOT_NONE;
	for (slot = 1; slot <= AE_WEAPON_SLOT_COUNT; slot++)
	{
		if (!(mask & AE_WEAPON_SLOT_BIT(slot)))
			continue;
		if (++seen == (short)rank)
			return slot;
	}
	return AE_WEAPON_SLOT_NONE;
}

short ae_weapon_slots_start(unsigned int mask, unsigned int rank, unsigned char start)
{
	/* (the START byte counts only in a game naming a second-byte slot, which builds without it cannot join: in any
	other game every build plays by the rank fields alone, the same) */
	if (start && (mask & ~0xFFu))
		return valid((short)start) && (mask & AE_WEAPON_SLOT_BIT(start)) ? (short)start : AE_WEAPON_SLOT_NONE;
	return ae_weapon_slots_loadout(mask, rank);
}

int ae_weapon_slots_rank_holds(unsigned int mask, short slot)
{
	unsigned int rank = ae_weapon_slots_source(mask, slot);

	/* (the first byte's slots: builds without the START byte know them by their rank, 1-7) */
	return rank >= 1 && rank <= AE_WEAPON_SLOTS_RANK_MAXIMUM && slot < AE_WEAPON_SLOT_HIGH_FIRST;
}

unsigned int ae_weapon_slots_source(unsigned int mask, short slot)
{
	short other, rank = 0;

	if (!valid(slot) || !(mask & AE_WEAPON_SLOT_BIT(slot)))
		return 0;
	for (other = 1; other <= slot; other++)
		if (mask & AE_WEAPON_SLOT_BIT(other))
			rank++;
	return (unsigned int)rank;
}

/* a loadout slot's raw later field (0-3), or NONE when its two bits are not LATER */
static int later_field(unsigned char pack_loadout, unsigned long flags, unsigned int loadout_slot)
{
	if (((pack_loadout >> (loadout_slot * 2)) & 3u) != AE_WEAPON_SLOTS_SOURCE_LATER)
		return -1;
	return (int)((flags >> AE_WEAPON_SLOTS_LATER_SHIFT(loadout_slot)) & 3u);
}

unsigned int ae_weapon_slots_rank(unsigned char pack_loadout, unsigned long flags, unsigned char extra,
	unsigned int loadout_slot)
{
	int later;
	int bit = (extra & AE_WEAPON_SLOTS_EXTRA_BIT) != 0;

	if (loadout_slot > 1)
		return 0;
	later = later_field(pack_loadout, flags, loadout_slot);
	if (later < 0)
		return (pack_loadout >> (loadout_slot * 2)) & 3u;
	if (later <= 2)
		return 3u + (unsigned int)later;
	/* (rank 6 or 7: the bit for a slot alone at 6-7; both there, the bit says PRIMARY holds the 7th) */
	if (later_field(pack_loadout, flags, 1u - loadout_slot) == 3)
		return (loadout_slot == 0) == bit ? 7u : 6u;
	return bit ? 7u : 6u;
}

static void rank_bits_set(unsigned char *pack_loadout, unsigned long *flags, unsigned int loadout_slot, unsigned int rank)
{
	unsigned int source = rank <= 2 ? rank : rank <= AE_WEAPON_SLOTS_RANK_MAXIMUM ? AE_WEAPON_SLOTS_SOURCE_LATER : 0u;
	unsigned long later = source != AE_WEAPON_SLOTS_SOURCE_LATER ? 0ul : rank >= 6 ? 3ul : (unsigned long)(rank - 3);

	*pack_loadout = (unsigned char)((*pack_loadout & ~(3u << (loadout_slot * 2))) | (source << (loadout_slot * 2)));
	*flags = (*flags & ~(3ul << AE_WEAPON_SLOTS_LATER_SHIFT(loadout_slot))) |
		(later << AE_WEAPON_SLOTS_LATER_SHIFT(loadout_slot));
}

void ae_weapon_slots_rank_set(unsigned char *pack_loadout, unsigned long *flags, unsigned char *extra,
	unsigned int loadout_slot, unsigned int rank)
{
	unsigned int ranks[2];

	if (loadout_slot > 1)
		return;
	if (rank > AE_WEAPON_SLOTS_RANK_MAXIMUM)
		rank = 0;
	ranks[0] = ae_weapon_slots_rank(*pack_loadout, *flags, *extra, 0);
	ranks[1] = ae_weapon_slots_rank(*pack_loadout, *flags, *extra, 1);
	ranks[loadout_slot] = rank;
	/* (both slots the same 6th or 7th pack: not held; the other slot back to stock) */
	if (ranks[0] >= 6 && ranks[0] == ranks[1])
		ranks[1u - loadout_slot] = 0;
	rank_bits_set(pack_loadout, flags, 0, ranks[0]);
	rank_bits_set(pack_loadout, flags, 1, ranks[1]);
	*extra = (unsigned char)(*extra & ~AE_WEAPON_SLOTS_EXTRA_BIT);
	if ((ranks[0] >= 6 && ranks[1] >= 6) ? ranks[0] == 7 : (ranks[0] == 7 || ranks[1] == 7))
		*extra = (unsigned char)(*extra | AE_WEAPON_SLOTS_EXTRA_BIT);
}

void ae_weapon_slots_loadout_remap(unsigned int old_mask, unsigned int new_mask, unsigned char *pack_loadout,
	unsigned long *flags, unsigned char *extra)
{
	short slots[2];
	unsigned int loadout_slot;

	/* (both read first: a slot's rank set changes how the other's 6-7 reads) */
	for (loadout_slot = 0; loadout_slot < 2; loadout_slot++)
		slots[loadout_slot] = ae_weapon_slots_loadout(old_mask, ae_weapon_slots_rank(*pack_loadout, *flags, *extra,
			loadout_slot));
	for (loadout_slot = 0; loadout_slot < 2; loadout_slot++)
		ae_weapon_slots_rank_set(pack_loadout, flags, extra, loadout_slot, ae_weapon_slots_source(new_mask,
			slots[loadout_slot]));
}

int ae_weapon_slots_capability(unsigned int loaded_mask)
{
	int level, best = 0;

	/* (each level: every slot of it or below loaded) */
	for (level = AE_WEAPON_SLOTS_CAPABILITY_BR; level <= AE_WEAPON_SLOTS_CAPABILITY_LAST; level++)
	{
		short slot;
		int all = 1;

		for (slot = 1; slot <= AE_WEAPON_SLOT_COUNT; slot++)
			if (slot_capability[slot] <= level && !(loaded_mask & AE_WEAPON_SLOT_BIT(slot)))
				all = 0;
		if (!all)
			break;
		best = level;
	}
	return best;
}

int ae_weapon_slots_required(unsigned int mask)
{
	short slot;
	int level = 0;

	for (slot = 1; slot <= AE_WEAPON_SLOT_COUNT; slot++)
		if ((mask & AE_WEAPON_SLOT_BIT(slot)) && slot_capability[slot] > level)
			level = slot_capability[slot];
	return level;
}

short ae_weapon_slots_spawn(short stock_weapon, unsigned int added_mask)
{
	short slot = stock_weapon == AE_WEAPON_STOCK_PISTOL ? AE_WEAPON_SLOT_BATTLE_RIFLE :
		stock_weapon == AE_WEAPON_STOCK_ASSAULT_RIFLE ? AE_WEAPON_SLOT_SMG : AE_WEAPON_SLOT_NONE;

	return slot != AE_WEAPON_SLOT_NONE && (added_mask & AE_WEAPON_SLOT_BIT(slot)) ? slot : AE_WEAPON_SLOT_NONE;
}

static int same_word(char const *a, char const *b)
{
	for (; *a && *b; a++, b++)
		if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
			return 0;
	return *a == *b;
}

short ae_weapon_slots_alias(char const *word)
{
	unsigned int id = ae_weapon_registry_id_of_word(word);

	/* (the NEEDLER's word is the map's stock weapon in a layout; a weapon the slots do not reach has no layout word) */
	if (!id || id == AE_WEAPON_SLOT_NEEDLER || id > AE_WEAPON_REGISTRY_LEGACY_LAST ||
		!ae_weapon_registry_row(id)->active)
		return AE_WEAPON_SLOT_NONE;
	return (short)id;
}

short ae_weapon_slots_of_pack(char const *id)
{
	unsigned int found;
	size_t length;
	char word[64];

	if (!id || !id[0])
		return AE_WEAPON_SLOT_NONE;
	length = strlen(id);
	/* (its key, or its key less "_pack": the NEEDLER's folder is "needler", the older name "needler_pack" still read) */
	if (length >= sizeof(word))
		return AE_WEAPON_SLOT_NONE;
	memcpy(word, id, length + 1);
	if (length > 5 && !strcmp(word + length - 5, "_pack"))
		word[length - 5] = 0;
	found = ae_weapon_registry_id_of_word(word);
	if (!found || found > AE_WEAPON_REGISTRY_LEGACY_LAST || !ae_weapon_registry_row(found)->active)
		return AE_WEAPON_SLOT_NONE;
	/* (only the key: a layout alias such as "br" is not a pack's folder) */
	return same_word(word, ae_weapon_registry_row(found)->key) ? (short)found : AE_WEAPON_SLOT_NONE;
}

short ae_weapon_slots_fallback(short slot)
{
	struct ae_weapon_registry_row const *row = ae_weapon_registry_row_of_slot(slot);

	return row ? (short)ae_weapon_registry_stock(row->stock_fallback) : AE_WEAPON_STOCK_NONE;
}

char const *ae_weapon_slots_fallback_word(short slot)
{
	struct ae_weapon_registry_row const *row = ae_weapon_registry_row_of_slot(slot);

	return row ? row->stock_fallback : "none";
}

char const *ae_weapon_slots_name(short slot)
{
	struct ae_weapon_registry_row const *row = ae_weapon_registry_row_of_slot(slot);

	/* (the NEEDLER's log name stays "needler_pack": tools/test_weapon_pack_storage.py and the TEST RUN notes grep it) */
	return !row ? "?" : slot == AE_WEAPON_SLOT_NEEDLER ? "needler_pack" : row->key;
}

char const *ae_weapon_slots_display(short slot)
{
	struct ae_weapon_registry_row const *row = ae_weapon_registry_row_of_slot(slot);

	return row ? row->display : "?";
}
