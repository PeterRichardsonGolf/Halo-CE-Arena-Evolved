/*
AE_WEAPON_SLOTS.H

Several weapon packs in one game: the pure rules (no engine includes; port/linux/tests/ae_weapon_slots_test.c).

A gametype's universal_variant.weapon_pack byte is a MASK of pack SLOTS: bit n-1 names slot n (1 the battle rifle,
2 the SMG, 3 the silenced SMG, 4 the carbine, 5 the silenced assault rifle, 6 the DMR, 7 the EVO RIFLE: 0.7.0; the
byte's seven bits were all slots; a later release adds an 8th, the NEEDLER's, bit 7). 1 is the BR alone, exactly as before
the mask. Builds before the NEEDLER read 128 and over as none; they cannot join a game that names the NEEDLER (it needs
capability level 8). Each named slot's pack is added to the map, in slot order.

pack_loadout's two bits a loadout slot (game_engine.h): 1 the LOWEST named slot's weapon 0 (for the BR alone: the BR,
as before), 2 the SECOND lowest named slot's, 3 a LATER one: the third, fourth or fifth named, as the loadout slot's two
bits of the universal variant's flags (bits 12-13 the PRIMARY's, 14-15 the SECONDARY's) say, 0, 1 or 2 (3: stock); 0
the stock value. So every named pack's weapon can start (owner 2026-10-23). A loadout slot's RANK is that place among
the named slots, 1 to 5 (0 stock): a game naming more than five packs starts only with one of its first five.
Builds before 0.3.9 read 3 as stock, builds before 0.3.6 read 2 as stock: they play
the slot's own weapon, the pack's stock fallback.

The join mark's capability (ae_silent_rules.h): 2 the machine has the BR's pack, 3 and the SMG's and the silenced
SMG's, 4 and the carbine's, 5 and the silenced assault rifle's, 6 and the DMR's, 7 and the EVO RIFLE's, 8 and the NEEDLER's. A machine tells the highest level whose slots are all
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
	/* the NEEDLER pack (behaviour 0, as its tags say). It is the last slot: it replaces the map's own needler
	where a gametype names it, and it is never a loadout rank under the first seven (no loadout UI names it yet) */
	AE_WEAPON_SLOT_NEEDLER,
	/* 0.7.5's second mask byte (game_variant_options.weapon_pack_high: bit n-9 names slot n): the bolt-action sniper,
	the HJ magnum, the brute shot and the combat assault rifle. Builds before it read the first byte alone and
	never name these; a host whose game names one refuses them (their capability is below 9) */
	AE_WEAPON_SLOT_SNIPER_BOLT,
	AE_WEAPON_SLOT_MAGNUM_SCOPED,
	AE_WEAPON_SLOT_BRUTE_SHOT,
	AE_WEAPON_SLOT_AR_MODERN,
	/* the sidearm pack's slot and the energy sword's */
	AE_WEAPON_SLOT_SIDEARM,
	AE_WEAPON_SLOT_ENERGY_SWORD,
	AE_WEAPON_SLOT_COUNT = AE_WEAPON_SLOT_ENERGY_SWORD,
	/* the most slots the two mask bytes hold */
	AE_WEAPON_SLOT_MAXIMUM = 16,
	/* the slots the AE seeds name (revision 29: the first seven). The seed step names the NEEDLER in a later revision;
	until then ae_weapon_slots_all() stays these seven */
	AE_WEAPON_SLOT_SEEDED_COUNT = AE_WEAPON_SLOT_EVO_RIFLE
};

/* the first slot of the second byte */
#define AE_WEAPON_SLOT_HIGH_FIRST 9
/* a slot's bit in the mask */
#define AE_WEAPON_SLOT_BIT(slot) (1u << ((slot) - 1))
/* the byte's largest value read as a mask (game_engine.h's WEAPON_PACK_MAXIMUM) */
#define AE_WEAPON_SLOTS_BYTE_MAXIMUM 255

/* the stock weapons a pack weapon falls back to, and a map spawn may be (game_engine.h's _loadout_weapon_* order) */
enum ae_weapon_stock
{
	AE_WEAPON_STOCK_NONE = 0,
	AE_WEAPON_STOCK_ASSAULT_RIFLE = 2,
	AE_WEAPON_STOCK_PISTOL = 3,
	AE_WEAPON_STOCK_SHOTGUN = 4,
	AE_WEAPON_STOCK_SNIPER_RIFLE = 5,
	AE_WEAPON_STOCK_ROCKET_LAUNCHER = 6,
	/* (the NEEDLER pack's: the stock needler, _loadout_weapon_needler) */
	AE_WEAPON_STOCK_NEEDLER = 9
};

/* the capability levels (ae_silent_rules.h): the BR's 2 is AE_RIFLE_CAPABILITY */
#define AE_WEAPON_SLOTS_CAPABILITY_BR 2
#define AE_WEAPON_SLOTS_CAPABILITY_SMG 3
#define AE_WEAPON_SLOTS_CAPABILITY_CARBINE 4
#define AE_WEAPON_SLOTS_CAPABILITY_AR_SILENCED 5
#define AE_WEAPON_SLOTS_CAPABILITY_DMR 6
#define AE_WEAPON_SLOTS_CAPABILITY_EVO_RIFLE 7
#define AE_WEAPON_SLOTS_CAPABILITY_NEEDLER 8
/* (the second byte's slots, one level each. The join mark holds levels up to 15 (its low four bits:
ae_silent_rules.h): a 16th slot would share level 15 with the 15th) */
#define AE_WEAPON_SLOTS_CAPABILITY_SNIPER_BOLT 9
#define AE_WEAPON_SLOTS_CAPABILITY_MAGNUM_SCOPED 10
#define AE_WEAPON_SLOTS_CAPABILITY_BRUTE_SHOT 11
#define AE_WEAPON_SLOTS_CAPABILITY_AR_MODERN 12
/* (the levels are cumulative: the final numbers follow the order the packs ship in, and the release session decides them;
a pack shipped before a lower-level one would lock every joiner out) */
#define AE_WEAPON_SLOTS_CAPABILITY_SIDEARM 13
#define AE_WEAPON_SLOTS_CAPABILITY_ENERGY_SWORD 14
#define AE_WEAPON_SLOTS_CAPABILITY_LAST AE_WEAPON_SLOTS_CAPABILITY_ENERGY_SWORD
#define AE_WEAPON_SLOTS_CAPABILITY_MAXIMUM 15
/* the highest rank a loadout slot holds (two bits, then two later bits 0-2; 0.7.0: a later field of 3 is rank 6 or
7, which the variant's ball_melee bit AE_WEAPON_SLOTS_EXTRA_BIT tells: every pack can start) */
#define AE_WEAPON_SLOTS_RANK_MAXIMUM 7
/* (ball_melee's bit 6, free: BALL MELEE has the low two bits, bit 2 is free, FALL DAMAGE HALO 2 bit 3, SPAWNS bits 4-5 (ae_spawn_modern.h); pad5 is full:
COMBAT FEEDBACK bits 0-1, JUGGERNAUT bit 2, FRIENDLY SPLATTER bit 3, the SILENT options the high nibble).
One loadout slot at rank 6 or 7: set for 7. Both at 6 and 7: set when PRIMARY is the 7th. Older builds read a
later field of 3 as no pack (their stock weapon) */
#define AE_WEAPON_SLOTS_EXTRA_BIT 0x40u

/* the mask a gametype byte names (0 for none; bits past the known slots dropped) */
unsigned int ae_weapon_slots_mask(unsigned char weapon_pack);
/* the mask the gametype's two bytes name: the universal variant's weapon_pack (slots 1-8) and the PC options'
weapon_pack_high (slots 9-16); bits past the known slots dropped */
unsigned int ae_weapon_slots_mask_wide(unsigned char weapon_pack, unsigned char weapon_pack_high);
/* the two bytes of a mask: the first (slots 1-8) and the second (slots 9-16) */
unsigned char ae_weapon_slots_low_byte(unsigned int mask);
unsigned char ae_weapon_slots_high_byte(unsigned int mask);
/* the mask of the slots the AE seeds name (seed revision 29: the first seven; the NEEDLER's bit 7 joins them in the seed
step's next revision. 0xFF is the byte of a gametype that names the needler too) */
unsigned int ae_weapon_slots_all(void);
/* the source value that is a LATER rank (3-5), and where the flags hold which: two bits a loadout slot from bit 12 */
#define AE_WEAPON_SLOTS_SOURCE_LATER 3u
#define AE_WEAPON_SLOTS_LATER_SHIFT(loadout_slot) (12u + 2u * (unsigned int)(loadout_slot))
#define AE_WEAPON_SLOTS_LATER_MASK 0xF000UL

/* the slot the rank-th named slot of mask is (rank 1-5), or AE_WEAPON_SLOT_NONE */
short ae_weapon_slots_loadout(unsigned int mask, unsigned int rank);
/* the slot a loadout slot starts with: its START byte (the PC options' pack_start, a pack slot 1-16 named in mask:
0.7.5, any named pack) when set and the mask names a second-byte slot (so every build that joins reads it), else its
rank's (ae_weapon_slots_loadout); AE_WEAPON_SLOT_NONE for stock */
short ae_weapon_slots_start(unsigned int mask, unsigned int rank, unsigned char start);
/* whether a pack slot is held by the rank fields alone (a first-byte slot at rank 1-7), so builds without the START
byte start with it too; else the START byte holds it and the rank fields say stock */
int ae_weapon_slots_rank_holds(unsigned int mask, short slot);
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
/* the stock weapon a slot's weapon falls back to in a layout (the registry's stock_fallback column; slots 1-24) */
short ae_weapon_slots_fallback(short slot);
/* the same as the registry's word ("pistol", "assault_rifle", "shotgun", "sniper_rifle", "rocket_launcher", "needler",
"none") */
char const *ae_weapon_slots_fallback_word(short slot);
/* THE FACADE (weapon cache, design 6.5): slot n is the weapon cache's weapon id n for n = 1-24 (tools/weapon_cache/registry.json,
ae_weapon_registry.h). The names, display names, layout words, stock fallbacks and pack folders below answer for all of
them from the registry; the gametype's mask, ranks and capability levels are the legacy pack slots' (AE_WEAPON_SLOT_COUNT
of them) and go with the legacy bytes' own rules (ae_weapon_selection.h).
A slot's name (the log, the layout's): "battle_rifle", "smg", ... ("?" past 24) */
char const *ae_weapon_slots_name(short slot);
/* a slot's display name ("BATTLE RIFLE", "SMG", ...) */
char const *ae_weapon_slots_display(short slot);
/* the slot whose pack's tags lie under ae\packs\<id>\ (its name; the NEEDLER's "needler"), NONE for another */
short ae_weapon_slots_of_pack(char const *id);

#endif /* __AE_WEAPON_SLOTS_H */
