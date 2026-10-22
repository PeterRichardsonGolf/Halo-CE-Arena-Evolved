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
	{ "smg_silenced", "silenced_smg", "SILENCED SMG", AE_WEAPON_STOCK_ASSAULT_RIFLE, AE_WEAPON_SLOTS_CAPABILITY_SMG },
	/* (the carbine, a precision rifle: the pistol's role on a CE map) */
	{ "carbine", NULL, "CARBINE", AE_WEAPON_STOCK_PISTOL, AE_WEAPON_SLOTS_CAPABILITY_CARBINE },
	/* (the H2 Mappack Collection's silenced assault rifle: the assault rifle's role) */
	{ "ar_silenced", "silenced_ar", "SILENCED AR", AE_WEAPON_STOCK_ASSAULT_RIFLE,
		AE_WEAPON_SLOTS_CAPABILITY_AR_SILENCED },
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

short ae_weapon_slots_loadout(unsigned int mask, unsigned int source)
{
	short slot, seen = 0;

	if (source != 1 && source != 2)
		return AE_WEAPON_SLOT_NONE;
	for (slot = 1; slot <= AE_WEAPON_SLOT_COUNT; slot++)
	{
		if (!(mask & AE_WEAPON_SLOT_BIT(slot)))
			continue;
		if (++seen == (short)source)
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
	return rank <= 2 ? (unsigned int)rank : 0u;
}

unsigned char ae_weapon_slots_loadout_remap(unsigned int old_mask, unsigned int new_mask, unsigned char pack_loadout)
{
	unsigned char result = (unsigned char)(pack_loadout & ~0x0Fu);
	unsigned int loadout_slot;

	for (loadout_slot = 0; loadout_slot < 2; loadout_slot++)
	{
		short slot = ae_weapon_slots_loadout(old_mask, (pack_loadout >> (loadout_slot * 2)) & 3u);

		result = (unsigned char)(result | (ae_weapon_slots_source(new_mask, slot) << (loadout_slot * 2)));
	}
	return result;
}

int ae_weapon_slots_capability(unsigned int loaded_mask)
{
	int level, best = 0;

	/* (each level: every slot of it or below loaded) */
	for (level = AE_WEAPON_SLOTS_CAPABILITY_BR; level <= AE_WEAPON_SLOTS_CAPABILITY_AR_SILENCED; level++)
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
