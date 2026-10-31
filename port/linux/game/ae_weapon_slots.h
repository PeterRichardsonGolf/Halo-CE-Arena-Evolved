/*
AE_WEAPON_SLOTS.H

Several weapon packs in one game: the pure rules (no engine includes; port/linux/tests/ae_weapon_slots_test.c).

A gametype's universal_variant.weapon_pack byte is a MASK of pack SLOTS: bit n-1 names slot n (1 the battle rifle,
2 the SMG, 3 the silenced SMG, 4 the carbine, 5 the silenced assault rifle, 6 the DMR, 7 the EVO RIFLE: 0.7.0; the
byte's seven bits are all slots now). 1 is the BR alone, exactly as before
the mask; 128 and over read as none (as before). Each named slot's pack is added to the map, in slot order.

pack_loadout's two bits a loadout slot (game_engine.h): 1 the LOWEST named slot's weapon 0 (for the BR alone: the BR,
as before), 2 the SECOND lowest named slot's, 3 a LATER one: the third, fourth or fifth named, as the loadout slot's two
bits of the universal variant's flags (bits 12-13 the PRIMARY's, 14-15 the SECONDARY's) say, 0, 1 or 2 (3: stock); 0
the stock value. So every named pack's weapon can start (owner 2026-10-23). A loadout slot's RANK is that place among
the named slots, 1 to 5 (0 stock): a game naming more than five packs starts only with one of its first five.
Builds before 0.3.9 read 3 as stock, builds before 0.3.6 read 2 as stock: they play
the slot's own weapon, the pack's stock fallback.

The join mark's capability (ae_silent_rules.h): 2 the machine has the BR's pack, 3 and the SMG's and the silenced
SMG's, 4 and the carbine's, 5 and the silenced assault rifle's, 6 and the DMR's, 7 and the EVO RIFLE's. A machine tells the highest level whose slots are all
loaded; a host refuses a machine below the level of the highest slot the game names.

MAP SPAWNS (pack_loadout bit 4, a test switch only: owner 2026-10-21): a map item that would spawn as a stock pistol
spawns the BR, one that would spawn as an assault rifle the SMG, where that pack is named and on the map.

Layout aliases (ae_layout_rules.h): battle_rifle (br), smg, smg_silenced (silenced_smg), carbine, ar_silenced
(silenced_ar), dmr, evo_rifle (evo): a pack weapon on a spot, made from the map's own fallback item (the rifles: the
pistol; the SMGs and the silenced assault rifle: the assault rifle), which is what spawns where the pack is not on the map.
*/

#ifndef __AE_WEAPON_SLOTS_H
#define __AE_WEAPON_SLOTS_H

#include "ae_pure_types.h"

enum ae_weapon_slot
{
	AE_WEAPON_SLOT_NONE = 0,
	AE_WEAPON_SLOT_BATTLE_RIFLE = 1,
	AE_WEAPON_SLOT_SMG,
	AE_WEAPON_SLOT_SMG_SILENCED,
	AE_WEAPON_SLOT_CARBINE,
	AE_WEAPON_SLOT_AR_SILENCED,
	AE_WEAPON_SLOT_DMR,
	AE_WEAPON_SLOT_EVO_RIFLE,
	AE_WEAPON_SLOT_COUNT = AE_WEAPON_SLOT_EVO_RIFLE
};

/* a slot's bit in the mask */
#define AE_WEAPON_SLOT_BIT(slot) (1u << ((slot) - 1))
/* the byte's largest value read as a mask (game_engine.h's WEAPON_PACK_MAXIMUM) */
#define AE_WEAPON_SLOTS_BYTE_MAXIMUM 127

/* the stock weapons a pack weapon falls back to, and a map spawn may be (game_engine.h's _loadout_weapon_* order) */
enum ae_weapon_stock
{
	AE_WEAPON_STOCK_NONE = 0,
	AE_WEAPON_STOCK_ASSAULT_RIFLE = 2,
	AE_WEAPON_STOCK_PISTOL = 3
};

/* the capability levels (ae_silent_rules.h): the BR's 2 is AE_RIFLE_CAPABILITY */
#define AE_WEAPON_SLOTS_CAPABILITY_BR 2
#define AE_WEAPON_SLOTS_CAPABILITY_SMG 3
#define AE_WEAPON_SLOTS_CAPABILITY_CARBINE 4
#define AE_WEAPON_SLOTS_CAPABILITY_AR_SILENCED 5
#define AE_WEAPON_SLOTS_CAPABILITY_DMR 6
#define AE_WEAPON_SLOTS_CAPABILITY_EVO_RIFLE 7
#define AE_WEAPON_SLOTS_CAPABILITY_LAST AE_WEAPON_SLOTS_CAPABILITY_EVO_RIFLE
/* the highest rank a loadout slot holds (two bits, then two later bits 0-2; 0.7.0: a later field of 3 is rank 6 or
7, which the variant's ball_melee bit AE_WEAPON_SLOTS_EXTRA_BIT tells: every pack can start) */
#define AE_WEAPON_SLOTS_RANK_MAXIMUM 7
/* (ball_melee's bit 6, free: BALL MELEE has the low nibble, SPAWNS bits 4-5 (ae_spawn_modern.h); pad5 is full:
COMBAT FEEDBACK bits 0-1, JUGGERNAUT bit 2, FRIENDLY SPLATTER bit 3, the SILENT options the high nibble).
One loadout slot at rank 6 or 7: set for 7. Both at 6 and 7: set when PRIMARY is the 7th. Older builds read a
later field of 3 as no pack (their stock weapon) */
#define AE_WEAPON_SLOTS_EXTRA_BIT 0x40u

/* the mask a gametype byte names (0 for none; 128 and over none; bits past the known slots dropped) */
unsigned int ae_weapon_slots_mask(unsigned char weapon_pack);
/* the source value that is a LATER rank (3-5), and where the flags hold which: two bits a loadout slot from bit 12 */
#define AE_WEAPON_SLOTS_SOURCE_LATER 3u
#define AE_WEAPON_SLOTS_LATER_SHIFT(loadout_slot) (12u + 2u * (unsigned int)(loadout_slot))
#define AE_WEAPON_SLOTS_LATER_MASK 0xF000UL

/* the slot the rank-th named slot of mask is (rank 1-5), or AE_WEAPON_SLOT_NONE */
short ae_weapon_slots_loadout(unsigned int mask, unsigned int rank);
/* the rank (1-5) of this pack slot among the named slots in mask, or 0: the slot is not named */
unsigned int ae_weapon_slots_source(unsigned int mask, short slot);
/* a loadout slot's (0 PRIMARY, 1 SECONDARY) rank from pack_loadout's two bits and the flags' later bits (0: stock) */
unsigned int ae_weapon_slots_rank(unsigned char pack_loadout, unsigned long flags, unsigned char extra,
	unsigned int loadout_slot);
/* writes a loadout slot's rank (0 stock, 1-7): its two bits of pack_loadout, its two later bits of the flags and
extra's (ball_melee's) AE_WEAPON_SLOTS_EXTRA_BIT; the other bits kept. Both slots at the same rank 6 or 7 cannot be held:
the other slot goes back to stock */
void ae_weapon_slots_rank_set(unsigned char *pack_loadout, unsigned long *flags, unsigned char *extra,
	unsigned int loadout_slot, unsigned int rank);
/* the two loadout slots after the mask changed from old_mask to new_mask: each keeps its pack slot's weapon where
that slot is still named, else goes back to stock (0). Other bits kept */
void ae_weapon_slots_loadout_remap(unsigned int old_mask, unsigned int new_mask, unsigned char *pack_loadout,
	unsigned long *flags, unsigned char *extra);
/* the lowest named slot (0 for none) */
short ae_weapon_slots_first(unsigned int mask);
/* the level a machine with these slots loaded tells */
int ae_weapon_slots_capability(unsigned int loaded_mask);
/* the level a game naming these slots needs (0: none) */
int ae_weapon_slots_required(unsigned int mask);
/* MAP SPAWNS: the slot whose weapon 0 spawns in place of a stock weapon, among the slots on the map, or NONE */
short ae_weapon_slots_spawn(short stock_weapon, unsigned int added_mask);
/* a layout alias's slot (case aside; NONE: not a pack weapon's name) */
short ae_weapon_slots_alias(char const *word);
/* the stock weapon a slot's weapon falls back to in a layout */
short ae_weapon_slots_fallback(short slot);
/* a slot's name (the log, the layout's): "battle_rifle", "smg", ... ("?" past them) */
char const *ae_weapon_slots_name(short slot);
/* a slot's display name ("BATTLE RIFLE", "SMG", ...) */
char const *ae_weapon_slots_display(short slot);

#endif /* __AE_WEAPON_SLOTS_H */
