/*
AE_GRENADE_CATALOGUE.H

Arena Evolved's grenade catalogue: the hand-grenade types a gametype can add beside Halo CE's frag and plasma, as
pure data and rules (no engine includes: port/linux/tests/ae_grenade_catalogue_test.c runs them). The engine side is
ae_grenades.c's.

IDS. A type's id is permanent and never reused: it is the bit of the gametype's grenade mask (bit n names id n).
0 the EMP, 1 the SMOKE; 2-7 free for later types. A mask of 0, every gametype from before, is Halo CE's frag and
plasma alone.

SLOTS. A unit's grenade slots are 0-1 the map's frag and plasma (in the unit, as Halo CE), then the map's own extra
types (MCC maps' only, today at most 2), then each catalogue id the mask names, in id order, up to slot 9: 8 extra
slots beside the stock pair, in mcc_grenades.c's side table. Every machine makes the same slot map from the map's
own count and the mask. An id that has no free slot, or whose row is not available, is refused for that map; the
game plays without it.

LAYER VERSION. The grenade layer's version a build tells in its join request (ae_silent_rules.h's feature mark).
Version v knows every id whose row's layer_version is v or less. A host whose mask names an id refuses a machine
below that id's version.

EFFECT MODULES. A row's `effects` is a mask of modules (AE_GRENADE_FX_*); each module is one file
(ae_grenade_fx_*.c) with one host entry point. A later grenade is a row (tags + numbers) that reuses modules, or adds
at most one module.

The rows themselves live in ae_grenade_catalogue_entries.inc.
*/

#ifndef __AE_GRENADE_CATALOGUE_H
#define __AE_GRENADE_CATALOGUE_H

enum
{
	AE_GRENADE_CATALOGUE_IDS = 8,
	/* the unit's slots: 2 stock + 8 extra */
	AE_GRENADE_SLOTS = 10,
	AE_GRENADE_STOCK_SLOTS = 2,
	AE_GRENADE_EXTRA_SLOTS = AE_GRENADE_SLOTS - AE_GRENADE_STOCK_SLOTS,
	/* this build's grenade layer version (the join's feature mark) */
	AE_GRENADE_LAYER_VERSION = 1,
	AE_GRENADE_LAYER_VERSION_MAXIMUM = 7
};

/* the ids: permanent, never reused */
enum
{
	_ae_grenade_emp = 0,
	_ae_grenade_smoke = 1
};

enum ae_grenade_glyph
{
	_ae_grenade_glyph_none = 0,
	_ae_grenade_glyph_bolt,
	_ae_grenade_glyph_smoke
};

enum ae_grenade_map_rule
{
	_ae_grenade_map_none = 0,
	_ae_grenade_map_near_vehicles,
	_ae_grenade_map_centre
};

/* the effect modules (a row's `effects`) */
enum
{
	AE_GRENADE_FX_STALL = 1u << 0,
	AE_GRENADE_FX_SCRAMBLE = 1u << 1,
	AE_GRENADE_FX_POWERUP_SHORT = 1u << 2,
	AE_GRENADE_FX_SMOKE_VOLUME = 1u << 3
};

struct ae_grenade_numbers
{
	float fuse_seconds, arming_seconds;                            /* EMP 1.2 / 0.2 */
	float full_radius, outer_radius, edge_fraction;                 /* EMP 2.0 / 3.5 wu, 0.40 at the outer edge */
	float stall_seconds, stall_edge_seconds, immunity_seconds;      /* EMP 4.0 / 1.6 / 2.0 */
	float scramble_seconds, scramble_edge_seconds, flicker_seconds; /* EMP 5.0 / 2.0 / 0.75 */
};

struct ae_grenade_entry
{
	char const *name, *display;      /* "emp", "EMP GRENADE" */
	/* the grenade pack's tag names WITHOUT the pack's prefix (ae\grenades\) and the group; NULL none (a NULL
	throwing_effect: the map's plasma entry's) */
	char const *projectile, *equipment, *collection, *hud_interface, *throwing_effect, *field_effect;
	short glyph, maximum, spawn_default, map_rule, respawn_seconds;
	unsigned effects;
	int bots;
	/* 0: the row is reserved (its id known, its tags not shipped yet): its mask bit is refused on every map */
	int available;
	/* the grenade layer version that first knows this id */
	int layer_version;
	struct ae_grenade_numbers numbers;
};

/* the row of an id (0-7), or 0 for an id with no row */
struct ae_grenade_entry const *ae_grenade_catalogue_entry(int id);
/* the id of a name ("emp"), or -1 */
int ae_grenade_catalogue_id(char const *name);
/* the mask of every id with an available row */
unsigned ae_grenade_catalogue_available(void);

/* ---- the slot map ---- */

struct ae_grenade_slot_map
{
	/* the slots in use: 2 + own extras + the catalogue's that fit (2-10); the map's own count when it is past 10 */
	int type_count;
	/* the map's own types (its globals' grenades), 2 for a Custom Edition map */
	int own_count;
	/* per slot: the catalogue id, or -1 (stock, the map's own, or unused) */
	signed char ids[AE_GRENADE_SLOTS];
	/* the mask's ids that are in a slot, and those refused (no row, not available, or no free slot) */
	unsigned placed, refused;
	/* of the refused: those for want of a free slot (the others: no available row) */
	unsigned no_slot;
};

/* the slot map of a map with own_count types of its own (its globals block's count: 2 on a Custom Edition map, up to
4 on an MCC map; fewer than 2 read as 2) and the mask */
void ae_grenade_slot_map_make(struct ae_grenade_slot_map *map, int own_count, unsigned mask);
/* the slot of an id in the map, or -1 */
int ae_grenade_slot_of(struct ae_grenade_slot_map const *map, int id);

/* ---- the join ----
A machine's feature mark bits (ae_silent_rules.h's character, 8 bits): bit 0 the grenade layer, bits 1-3 its version,
bits 4-7 the top 4 bits of its grenade pack's sha256 (the pack's tags lie in every machine's tag table at the same
indices only when the packs are the same). A machine tells them only when its grenade pack was read and taken. */

/* the layer version a mask needs: the highest of the ids this build has an available row for (an id it does not know,
or one not available, is played by no machine of this build: it never pushes the version up; 0 for none) */
int ae_grenade_layer_version_needed(unsigned mask);
/* the layer version a machine tells at the join: this build's when its grenade pack was read and taken, else 0 */
int ae_grenade_layer_version_told(int pack_loaded);
/* the feature bits of a layer version (0: none, the bits 0) and a pack hash's first byte; the version and the pack
nibble the bits tell (0 / -1 when no layer) */
unsigned ae_grenade_join_bits(int version, unsigned char pack_hash_first_byte);
int ae_grenade_join_bits_version(unsigned bits);
int ae_grenade_join_bits_pack(unsigned bits);
/* whether a host whose own bits are host_bits, playing the mask, refuses a machine that told told_bits: when the mask
names an id the host plays (a needed version) and the host has its layer, a machine must tell the host's own version
and pack. A host without the layer plays frag and plasma and refuses nobody for it; a local machine never */
int ae_grenade_join_refused(unsigned mask, unsigned host_bits, unsigned told_bits, int local);

/* ---- the effect numbers ---- */

/* the strength of a blast at a distance: 1 to full_radius, then down to edge_fraction at outer_radius, 0 past it */
float ae_grenade_falloff(struct ae_grenade_numbers const *numbers, float distance);
/* an effect's ticks at a strength: the full seconds at 1, the edge seconds at edge_fraction, in a line between
(30 ticks a second; 0 at a strength of 0) */
long ae_grenade_effect_ticks(float full_seconds, float edge_seconds, float edge_fraction, float strength);

/* a stall hit at `now` lasting `ticks` on a vehicle whose stall ends at `end` (now or before: not stalled) and whose
immunity ends at `immune_until`: the new end. Re-hits refresh, never stack (the later end of the two); a hit while
immune (after a stall ended) does nothing */
long ae_grenade_stall_refresh(long now, long end, long immune_until, long ticks);
/* the immunity's end after a stall that ends at `end` */
long ae_grenade_stall_immunity_end(long end, float immunity_seconds);

/* ---- the mask's storage ----
The gametype keeps the mask in game_variant_options' byte 0x2B, bit n = catalogue id n, under the AE weapon cache
(ae_weapon_selection.h's `grenades`, AE_WEAPON_SELECTION_GRENADE_BYTE_OFFSET; that byte was weapon_pack_top before the
cache). A build without the cache stores no mask: its grenade games are the host's test override alone
(ae_grenades.c). Every machine reads the mask through ae_grenades_mask_of. */
#define AE_GRENADE_STORED_IDS 8

#endif
