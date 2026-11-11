/*
AE_WEAPON_SELECTION.H

What a gametype names of the AE weapon cache (design section 7, as the owner's 11-05 sets decision reshaped it): the pure
rules (no engine includes), with their unit test, port/linux/tests/ae_weapon_selection_test.c.

IN MEMORY, struct ae_weapon_selection: `named`, the legacy weapon ids 1-16 the game names (the pre-cache slot packs, byte
for byte as 0.7.5); `start[2]`, (weapon id, config id) for the PRIMARY and SECONDARY loadout slots, (0, 0) for the rank
rules; `grenades`, the grenade catalogue's mask (bit n names catalogue id n); `ae_weapons`, the AE WEAPONS toggle (the
map's weapon layout is filled by a weapon SET: the pool is the set's, never capped by this file); `set_id`, which set
(0 casual, 1 comp, 2 BTB, 3-14 reserved, 15 custom later; 0 whenever the toggle is off); and the derived `uses_cache`, the
bit the join gate and the no-cache guard key on: any named weapon, a start weapon, a config, a grenade type, or the
toggle (every set can draw a cache weapon).

WHERE IT LIVES. Ids 1-16 and START 1-16 with config 0 stay in the legacy bytes, byte for byte as 0.7.5
(universal_variant.weapon_pack = ids 1-8, bit n-1; game_variant_options.weapon_pack_high = ids 9-16, bit n-9;
pack_start[2] = a START id 1-16; the old rank fields still read). The grenade mask is the options' byte 0x2B. The toggle,
the set id and a START with an id past 16 or a config live in the AECW block of the gametype file at offset 0x80 (VERSION
3) and in message 109. The writer's rule: START goes to pack_start when its id is at most 16 and its config 0, else to
AECW (and pack_start is 0). The reader takes START = AECW's when set, else pack_start's, else the rank fields.

The named[] array, the (weapon, config) pairs and the map mask of AECW versions 1 and 2 are GONE (the format moved
before its freeze): a reader accepts exactly version 3.
*/

#ifndef AE_WEAPON_SELECTION_H
#define AE_WEAPON_SELECTION_H

enum
{
	AE_WEAPON_SELECTION_MAX_NAMED = 16,
	AE_WEAPON_SELECTION_LEGACY_LAST = 16,
	/* the AECW block: file offset, magic, version, payload and whole sizes (8 header + 12 payload + 20 signature = 40
	bytes, 0x80-0xA8). The proven-free window is 0x7C-0x100 (tools/weapon_cache/AECW-OFFSET-VERDICT.md): a write clears
	all of it, so no byte of an older, longer block outlives a newer one */
	AE_WEAPON_SELECTION_AECW_OFFSET = 0x80,
	AE_WEAPON_SELECTION_AECW_WINDOW = 0x80,
	AE_WEAPON_SELECTION_AECW_VERSION = 3,
	AE_WEAPON_SELECTION_AECW_PAYLOAD_SIZE = 12,
	AE_WEAPON_SELECTION_AECW_SIZE = 8 + AE_WEAPON_SELECTION_AECW_PAYLOAD_SIZE + 20,
	/* the payload (little-endian): [0] flags (bit 0 AE WEAPONS; the others zero), [1] zero, [2] set id (0-15), [3] zero,
	[4..7] START 0 (u16 weapon, u16 config), [8..11] START 1. A config is stored as its sequential number: id
	0x01000000 + n with n at most 0xFFFF */
	AE_WEAPON_SELECTION_AECW_FLAG_AE_WEAPONS = 1,
	AE_WEAPON_SELECTION_SET_LAST = 15,
	AE_WEAPON_SELECTION_CONFIG_BASE = 0x01000000,
	/* the options byte that holds the grenade mask (game_variant_options 0x2B, the old weapon_pack_top) */
	AE_WEAPON_SELECTION_GRENADE_BYTE_OFFSET = 0x2B
};

#define AE_WEAPON_SELECTION_AECW_MAGIC 0x57434541u /* 'AECW' little-endian (the bytes A E C W, as 'AEXV' is 0x56584541) */

/* a sorted set of ids (ascending, no repeats) */
struct ae_weapon_id_set
{
	unsigned int count;
	unsigned int ids[AE_WEAPON_SELECTION_MAX_NAMED];
};

struct ae_weapon_start
{
	unsigned int weapon, config; /* (0, 0): the rank rules or stock decide */
};

struct ae_weapon_selection
{
	struct ae_weapon_id_set named; /* legacy ids 1-16 only */
	struct ae_weapon_start start[2];
	unsigned int ae_weapons; /* the AE WEAPONS toggle (0 or 1) */
	unsigned int set_id; /* 0-15; 0 when the toggle is off */
	unsigned int grenades;
	unsigned int uses_cache; /* derived: ae_weapon_selection_derive */
};

/* the stored bytes: the legacy ones plus the AECW payload (present only when it is not empty) */
struct ae_weapon_selection_bytes
{
	unsigned char weapon_pack; /* universal_variant.weapon_pack */
	unsigned char weapon_pack_high; /* game_variant_options.weapon_pack_high */
	unsigned char pack_start[2]; /* game_variant_options.pack_start */
	unsigned char grenades; /* game_variant_options byte 0x2B */
	unsigned int aecw_present;
	unsigned char aecw[AE_WEAPON_SELECTION_AECW_PAYLOAD_SIZE]; /* flags, 0, set id, 0, start[2] (u16 weapon, u16 config) */
};

/* ---------- sets */
void ae_weapon_id_set_clear(struct ae_weapon_id_set *set);
int ae_weapon_id_set_has(struct ae_weapon_id_set const *set, unsigned int id);
/* TRUE if added (FALSE: already there, or the set is full) */
int ae_weapon_id_set_add(struct ae_weapon_id_set *set, unsigned int id);
int ae_weapon_id_set_remove(struct ae_weapon_id_set *set, unsigned int id);

/* ---------- the selection */
void ae_weapon_selection_clear(struct ae_weapon_selection *selection);
/* uses_cache from the other fields */
void ae_weapon_selection_derive(struct ae_weapon_selection *selection);
/* named: ids 1-16 only; a start: weapon 1-0xFFFF, config 0 or in the stored range (0x01000001-0x0100FFFF), a legacy plain
start named; grenades one byte; set_id 0-15 and 0 when the toggle is off (a larger id reads as casual); derives. Whether
a config belongs to a weapon is the caller's check (the rules do not know the registry) */
void ae_weapon_selection_sanitize(struct ae_weapon_selection *selection);
/* whether a legacy id can be named (the menus' guard) */
int ae_weapon_selection_can_name(struct ae_weapon_selection const *selection, unsigned int weapon);

/* the selection a variant's bytes say. pack_start may still hold bits 5-7, the grenade mask 0.7.6 test builds wrote
there: read once (when the grenades byte is 0) and moved to the grenades byte on the next write; rank_start_slot is what
the rank fields give for each loadout slot (ae_weapon_slots.h; 0 none), used when no START and no AECW start is set.
bytes->aecw_present 0: no block */
void ae_weapon_selection_read(struct ae_weapon_selection *selection, struct ae_weapon_selection_bytes const *bytes,
	unsigned int const rank_start_slot[2]);
/* the bytes for a selection (sanitized first): the writer rule above. grenades goes to the 0x2B byte; the old pack_start
bits 5-7 are never written */
void ae_weapon_selection_write(struct ae_weapon_selection_bytes *bytes, struct ae_weapon_selection const *selection);
/* the grenade mask a 0.7.6 test build wrote in pack_start's bits 5-7 (0 for a file that never had it) */
unsigned int ae_weapon_selection_old_grenade_bits(unsigned char pack_start_byte);

/* the no-cache guard (design 8.5): TRUE when a host must refuse to create an object of a cache definition (a loadout,
layout, map spawn or dev give): a network game whose selection does not use the cache. Offline games are never refused.
The sites call ae_weapon_selection_guard (ae_weapon_selection_game.h), which asks the game in play */
int ae_weapon_selection_guard_refuses(int network_game, int game_uses_cache);

/* ---------- the AECW block (the 40-byte file form: magic, version u16, size u16, payload, 20-byte signature) */
/* fills block[0..19] (magic, version, size, payload); the caller signs bytes [0..20) into [20..40) with the game's
saved-game checksum. Returns the block's size, or 0 when the payload is empty (nothing to write) */
unsigned int ae_weapon_selection_aecw_encode(struct ae_weapon_selection_bytes const *bytes, unsigned char *block);
/* checks magic, version (exactly 3: versions 1 and 2 are refused, as 2 refused 1 and 3), size and the payload's zero bytes
and flag bits, and copies the payload; the signature is the caller's (a block that fails is none) */
int ae_weapon_selection_aecw_decode(struct ae_weapon_selection_bytes *bytes, unsigned char const *block);

#endif /* AE_WEAPON_SELECTION_H */
