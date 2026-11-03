/*
AE_WEAPON_SLOTS.C

Several weapon packs in one game: the pure rules (ae_weapon_slots.h). No engine includes.
*/

#include <ctype.h>
#include <string.h>

#include "ae_weapon_slots.h"

#define KNOWN_MASK ((1u << AE_WEAPON_SLOT_COUNT) - 1)

static struct
{
	char const *name;
	char const *other_name;
	char const *display;
	short fallback;
	int capability;
} const slots[AE_WEAPON_SLOT_COUNT + 1] =
{
	{ "none", NULL, "NONE", AE_WEAPON_STOCK_NONE, 0 },
	{ "battle_rifle", "br", "BATTLE RIFLE", AE_WEAPON_STOCK_PISTOL, AE_WEAPON_SLOTS_CAPABILITY_BR },
	{ "smg", NULL, "SMG", AE_WEAPON_STOCK_ASSAULT_RIFLE, AE_WEAPON_SLOTS_CAPABILITY_SMG },
	{ "smg_silenced", "silenced_smg", "SUPPRESSED SMG", AE_WEAPON_STOCK_ASSAULT_RIFLE, AE_WEAPON_SLOTS_CAPABILITY_SMG },
	/* (the carbine, a precision rifle: the pistol's role on a CE map) */
	{ "carbine", NULL, "CARBINE", AE_WEAPON_STOCK_PISTOL, AE_WEAPON_SLOTS_CAPABILITY_CARBINE },
	/* (the H2 Mappack Collection's silenced assault rifle: the assault rifle's role) */
	{ "ar_silenced", "silenced_ar", "SUPPRESSED AR", AE_WEAPON_STOCK_ASSAULT_RIFLE,
		AE_WEAPON_SLOTS_CAPABILITY_AR_SILENCED },
	/* (0.7.0: the zero-bloom DMR and the competitive EVO RIFLE, precision rifles: the pistol's role) */
	{ "dmr", NULL, "DMR", AE_WEAPON_STOCK_PISTOL, AE_WEAPON_SLOTS_CAPABILITY_DMR },
	{ "evo_rifle", "evo", "EVO RIFLE", AE_WEAPON_STOCK_PISTOL, AE_WEAPON_SLOTS_CAPABILITY_EVO_RIFLE },
};

static int valid(short slot)
{
	return slot >= 1 && slot <= AE_WEAPON_SLOT_COUNT;
}

unsigned int ae_weapon_slots_mask(unsigned char weapon_pack)
{
	return weapon_pack > AE_WEAPON_SLOTS_BYTE_MAXIMUM ? 0u : (unsigned int)weapon_pack & KNOWN_MASK;
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

	if (rank < 1 || rank > AE_WEAPON_SLOT_COUNT)
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
			if (slots[slot].capability <= level && !(loaded_mask & AE_WEAPON_SLOT_BIT(slot)))
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
		if ((mask & AE_WEAPON_SLOT_BIT(slot)) && slots[slot].capability > level)
			level = slots[slot].capability;
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
	short slot;

	if (!word)
		return AE_WEAPON_SLOT_NONE;
	for (slot = 1; slot <= AE_WEAPON_SLOT_COUNT; slot++)
		if (same_word(word, slots[slot].name) || (slots[slot].other_name && same_word(word, slots[slot].other_name)))
			return slot;
	return AE_WEAPON_SLOT_NONE;
}

short ae_weapon_slots_fallback(short slot)
{
	return valid(slot) ? slots[slot].fallback : AE_WEAPON_STOCK_NONE;
}

char const *ae_weapon_slots_name(short slot)
{
	return valid(slot) ? slots[slot].name : "?";
}

char const *ae_weapon_slots_display(short slot)
{
	return valid(slot) ? slots[slot].display : "?";
}
