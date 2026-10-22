/*
AE_WEAPON_SLOTS.H

Several weapon packs in one game: the pure rules (no engine includes; port/linux/tests/ae_weapon_slots_test.c).

A gametype's universal_variant.weapon_pack byte is a MASK of pack SLOTS: bit n-1 names slot n (1 the battle rifle,
2 the SMG, 3 the silenced SMG, 4 the carbine, 5 the silenced assault rifle). 1 is the BR alone, exactly as before
the mask; 128 and over read as none (as before). Each named slot's pack is added to the map, in slot order.

pack_loadout's two bits a loadout slot (game_engine.h): 1 the LOWEST named slot's weapon 0 (for the BR alone: the BR,
as before), 2 the SECOND lowest named slot's; anything else the stock value. Older builds read 2 as stock.

The join mark's capability (ae_silent_rules.h): 2 the machine has the BR's pack, 3 and the SMG's and the silenced
SMG's, 4 and the carbine's, 5 and the silenced assault rifle's. A machine tells the highest level whose slots are all
loaded; a host refuses a machine below the level of the highest slot the game names.

MAP SPAWNS (pack_loadout bit 4, a test switch only: owner 2026-10-21): a map item that would spawn as a stock pistol
spawns the BR, one that would spawn as an assault rifle the SMG, where that pack is named and on the map.

Layout aliases (ae_layout_rules.h): battle_rifle (br), smg, smg_silenced (silenced_smg), carbine, ar_silenced
(silenced_ar): a pack weapon on a spot, made from the map's own fallback item (BR and carbine: the pistol; the SMGs
and the silenced assault rifle: the assault rifle), which is what spawns where the pack is not on the map.
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
	AE_WEAPON_SLOT_COUNT = AE_WEAPON_SLOT_AR_SILENCED
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

/* the mask a gametype byte names (0 for none; 128 and over none; bits past the known slots dropped) */
unsigned int ae_weapon_slots_mask(unsigned char weapon_pack);
/* the slot a loadout slot's two source bits give (pack_loadout >> 2 * loadout slot & 3), or AE_WEAPON_SLOT_NONE */
short ae_weapon_slots_loadout(unsigned int mask, unsigned int source);
/* the source value (1, 2) that gives a loadout slot this pack slot's weapon 0 among the named slots in mask, or 0:
the slot is not named, or is the third named or later (no source reaches it) */
unsigned int ae_weapon_slots_source(unsigned int mask, short slot);
/* pack_loadout's two loadout slots (bits 0-3) after the mask changed from old_mask to new_mask: each keeps its
pack slot's weapon where that slot is still named and reachable, else goes back to stock (0). Other bits kept */
unsigned char ae_weapon_slots_loadout_remap(unsigned int old_mask, unsigned int new_mask, unsigned char pack_loadout);
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
