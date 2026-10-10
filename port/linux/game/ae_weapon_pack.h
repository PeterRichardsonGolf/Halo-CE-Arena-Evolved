/*
AE_WEAPON_PACK.H

port: weapon pack. The weapon packs this game carries (docs/weapon-packs.md),
read once at start (game_initialize) and kept by SLOT: the value a
gametype's universal_variant.weapon_pack names (game_engine.h; 1 = the AE
battle rifle, packs\ae_battle_rifle.aepack beside the game's maps folder).

A pack is an untrusted download: one that is missing, damaged or of another
pack is refused with one "weapon pack:" line in debug.txt, its slot stays
empty, and the game plays without it. A game with no packs folder logs
nothing. A loaded pack's file stays open (one handle) for its resources,
which are read when a map asks for them.

Include after cseries.h.
*/

#ifndef __AE_WEAPON_PACK_H
#define __AE_WEAPON_PACK_H

struct ae_pack_weapon_record;

enum
{
	/* the slots are 1 to AE_WEAPON_PACK_SLOTS (universal_variant.weapon_pack's _weapon_pack_battle_rifle is 1) */
	AE_WEAPON_PACK_SLOTS = 1,
	AE_WEAPON_PACK_SHA256_SIZE = 32,
};

/* every slot's pack read and checked, each logged (game_initialize; once) */
void ae_weapon_pack_initialize(void);
/* every pack let go (game_dispose) */
void ae_weapon_pack_dispose(void);

/* whether the slot holds a pack that was read and accepted */
boolean ae_weapon_pack_slot_loaded(short slot);
/* the slot's pack's revision and SHA-256 (32 bytes; either may be NULL): TRUE, or FALSE (nothing written) when the
slot is empty */
boolean ae_weapon_pack_identity(short slot, unsigned int *revision, unsigned char *sha256);
/* how many weapons the slot's pack holds (0 when empty) */
short ae_weapon_pack_weapon_count(short slot);
/* the slot's pack's weapon entry (0 first: the one pack_loadout gives), or NULL; its strings through
ae_weapon_pack_string */
struct ae_pack_weapon_record const *ae_weapon_pack_weapon_entry(short slot, short weapon);
/* a string of the slot's pack (a weapon entry's key, label, menu name, ...): NULL for AE_PACK_NONE, an offset past
its STRINGS or an empty slot */
char const *ae_weapon_pack_string(short slot, unsigned int offset);

#endif
