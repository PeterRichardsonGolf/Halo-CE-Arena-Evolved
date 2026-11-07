/*
AE_WEAPON_CACHE_FORMAT.H

The AE weapon cache file (`packs/ae_content.aecache`), format v1: the one file that holds every AE weapon, the
vehicle damage and bomb tag sets, the AE grenade types and the base layer the weapons borrow from. Interface freeze
(T0): constants and records only, no behaviour. The reader is ae_pack_format.c's (T2-1), the Python twin is
tools/weapon_pack/aepack.py's cache functions (T1-2); both hold to these numbers, and tools/test_weapon_cache_format.py
holds them to each other.

The container is aepack v1 grown (ae_pack_format.h): kinds 1-10 are v1's sections, with the same tag, fixup, part and
resource records; the cache adds DIRECTORY, CONFIGS and EXTERNS. Every record is little-endian, 4-aligned, no padding.
A 64-bit value is two unsigned ints, low word first (the game's ABI and the platform layer's are the same this way).

A cache is untrusted data as a pack is: the reader checks every length, offset and count against the file and these
limits before it hands anything out, and refuses the whole file on any failure.
*/

#ifndef AE_WEAPON_CACHE_FORMAT_H
#define AE_WEAPON_CACHE_FORMAT_H

#include "ae_pack_format.h"

#define AE_CACHE_MAGIC "AEWCACHE" /* exactly 8 bytes, no terminator */

enum
{
	AE_CACHE_FORMAT_VERSION = 1,
	AE_CACHE_HEADER_SIZE = 0x200,
	/* tag layout 1: the Xbox 2276 layouts, as aepack v1 */
	AE_CACHE_TAG_LAYOUT_XBOX = AE_PACK_TAG_LAYOUT_XBOX,
	AE_CACHE_MAXIMUM_SECTIONS = 16,
	AE_CACHE_MAXIMUM_PARTS = 4,
	AE_CACHE_VERSION_TEXT_SIZE = 16,
	AE_CACHE_PART_NAME_SIZE = 16,
	AE_CACHE_SHA256_SIZE = AE_PACK_SHA256_SIZE,
	/* the SHA-256 of the file with these 32 bytes zeroed */
	AE_CACHE_SHA256_OFFSET = 0x38,
	/* cache_id: the first bytes of the SHA-256, what the network carries and the logs print */
	AE_CACHE_ID_SIZE = 8,
	/* the join name block carries this many digest bytes (ae_silent_rules.h) */
	AE_CACHE_JOIN_DIGEST_SIZE = 4
};

/* header flags */
enum
{
	/* this file is an optional part, not the main cache */
	AE_CACHE_FLAG_PART = 1,
	AE_CACHE_KNOWN_HEADER_FLAGS = 1
};

/* a part's flags */
enum
{
	/* a missing or mismatched part fails soft: only its entries become unavailable */
	AE_CACHE_PART_OPTIONAL = 1,
	AE_CACHE_KNOWN_PART_FLAGS = 1
};

/* section kinds: 1-10 are aepack v1's (ae_pack_format.h; WEAPONS holds 0 records in a cache) */
enum
{
	AE_CACHE_SECTION_DIRECTORY = 11,
	AE_CACHE_SECTION_CONFIGS = 12,
	AE_CACHE_SECTION_EXTERNS = 13,
	AE_CACHE_SECTION_KIND_LIMIT = 14
};

/* the limits: a cache past one is refused whole (design section 3.4) */
enum
{
	AE_CACHE_MAXIMUM_FILE_SIZE = 1024 * 1024 * 1024,
	/* STRINGS: the pack's 256 KiB held a pack's names; a cache's tags are many times a pack's (the linked cache's is
	285 KB). Both readers take up to this */
	AE_CACHE_MAXIMUM_STRINGS = 4 * 1024 * 1024,
	AE_CACHE_MAXIMUM_TAGS = 8192,
	AE_CACHE_MAXIMUM_OWN_TAGS = 8192,
	AE_CACHE_MAXIMUM_TAG_BYTES = 48 * 1024 * 1024,
	AE_CACHE_MAXIMUM_TAG_SIZE = 4 * 1024 * 1024,
	AE_CACHE_MAXIMUM_RESOURCES_SIZE = 768 * 1024 * 1024,
	AE_CACHE_MAXIMUM_RESOURCE_SIZE = 8 * 1024 * 1024,
	AE_CACHE_MAXIMUM_GEOMETRY = 64 * 1024 * 1024,
	AE_CACHE_MAXIMUM_PARTS_RECORDS = 16384,
	AE_CACHE_MAXIMUM_FIXUPS = 1048576,
	AE_CACHE_MAXIMUM_DIRECTORY = 1024,
	/* the item message index is 16 bits signed: 0x7F00 + 2 x the weapon's dense ordinal (ae_weapon_cache.h) */
	AE_CACHE_MAXIMUM_WEAPONS = 128,
	AE_CACHE_MAXIMUM_CONFIGS = 4096,
	/* tag sets' root lists (DIRECTORY's tail) */
	AE_CACHE_MAXIMUM_ROOT_LIST = 4096
};

/* DIRECTORY record kinds */
enum
{
	AE_CACHE_KIND_WEAPON = 1,
	AE_CACHE_KIND_TAG_SET = 2,
	AE_CACHE_KIND_GRENADE = 3,
	AE_CACHE_KIND_BASE = 4
};

/* DIRECTORY record flags */
enum
{
	AE_CACHE_ENTRY_POWER = 1,
	/* the 0.7.5 DIRECT_PORT meaning: available only where the player's MCC install is found */
	AE_CACHE_ENTRY_NEEDS_MCC_INSTALL = 2,
	AE_CACHE_ENTRY_RETIRED = 4,
	AE_CACHE_KNOWN_ENTRY_FLAGS = 7
};

/* behaviour (a weapon record's): ae_pack_format.h's 0-3, then the PDW's and the ravager's */
enum
{
	AE_CACHE_BEHAVIOUR_PDW = 4,
	AE_CACHE_BEHAVIOUR_RAVAGER = 5,
	AE_CACHE_BEHAVIOUR_LAST = 5
};

/* replaces_map_own: the map's own weapon a game naming this one rewrites its references to (today's two rules) */
enum
{
	AE_CACHE_REPLACES_MAP_NONE = 0,
	AE_CACHE_REPLACES_MAP_BATTLE_RIFLE = 1,
	AE_CACHE_REPLACES_MAP_NEEDLER = 2
};

/* the ids (tools/weapon_cache/registry.json; design section 4.1): kind in the top byte, every id unique across kinds */
enum
{
	AE_CACHE_ID_KIND_SHIFT = 24,
	AE_CACHE_ID_KIND_WEAPON = 0x00,
	AE_CACHE_ID_KIND_CONFIG = 0x01,
	AE_CACHE_ID_KIND_TAG_SET = 0x02,
	AE_CACHE_ID_KIND_GRENADE = 0x03,
	AE_CACHE_ID_KIND_BASE = 0x04,
	/* the tag sets and the base layer */
	AE_CACHE_ID_VEHICLE_DAMAGE = 0x02000001,
	AE_CACHE_ID_BOMB = 0x02000002,
	AE_CACHE_ID_BASE = 0x04000001,
	/* a grenade type: AE_CACHE_ID_GRENADE_FIRST + the grenade catalogue's id (0 the EMP, 1 the smoke) */
	AE_CACHE_ID_GRENADE_FIRST = 0x03000000,
	/* weapon ids 1-23 are today's slots 1-23 (ae_weapon_slots.h); 24 the gravity hammer (held) */
		/* ids 1-16 ride the gametype's legacy bytes (ae_weapon_selection.h) */
	AE_CACHE_ID_LEGACY_BYTES_LAST = 16,
	AE_CACHE_ID_LEGACY_SLOT_LAST = 24
};

#define AE_CACHE_ID_KIND(id) ((unsigned int)(id) >> AE_CACHE_ID_KIND_SHIFT)

/* item message strings: the weapon pack's index scheme replaced (ae_weapon_pack.c ITEM_STRING_INDEX_FIRST):
first + 2 x the weapon's dense ordinal (pickup text, then ammo text), always below 0x8000 */
enum
{
	AE_CACHE_ITEM_STRING_INDEX_FIRST = 0x7F00
};

/* ---------- the file's records */

struct ae_cache_part
{
	char name[AE_CACHE_PART_NAME_SIZE]; /* "mcc": NUL-padded, [a-z0-9_] */
	unsigned int flags; /* AE_CACHE_PART_* */
	unsigned int pad;
	unsigned int size[2]; /* the part file's size, low word first */
	unsigned char sha256[AE_CACHE_SHA256_SIZE];
};

struct ae_cache_header
{
	char magic[8]; /* AE_CACHE_MAGIC */
	unsigned int format_version; /* AE_CACHE_FORMAT_VERSION */
	unsigned int header_size; /* AE_CACHE_HEADER_SIZE */
	unsigned int flags; /* AE_CACHE_FLAG_* */
	unsigned int cache_serial; /* the release sequence: 1 for 0.7.6 (registry "serial") */
	unsigned int schema_hash[2]; /* AE's compiled tag schema (design 3.6), low word first */
	unsigned int tag_layout; /* AE_CACHE_TAG_LAYOUT_XBOX */
	unsigned int registry_revision; /* registry.json's "revision" */
	char ae_version[AE_CACHE_VERSION_TEXT_SIZE]; /* "0.7.6": for messages only */
	unsigned char sha256[AE_CACHE_SHA256_SIZE]; /* of the whole file with these 32 bytes zeroed */
	unsigned int section_count; /* 1 to AE_CACHE_MAXIMUM_SECTIONS */
	unsigned int part_count; /* 0 to AE_CACHE_MAXIMUM_PARTS */
	struct ae_cache_part parts[AE_CACHE_MAXIMUM_PARTS];
	unsigned char reserved[0xA0];
};

/* the section table follows the header at AE_CACHE_HEADER_SIZE: section_count ae_pack_section entries (20 bytes) */

/* DIRECTORY: one record per registry entry in the cache, sorted by id, unique ids. Tag numbers are cache tag
indices (the cache's own TAGS records); first_tag and tag_count are the entry's closure, base tags excluded. Fields
a kind does not use are AE_PACK_NONE (tag and string fields) or 0 */
struct ae_cache_directory_record
{
	unsigned int id;
	unsigned char kind; /* AE_CACHE_KIND_* */
	unsigned char part; /* 0 the main file, n parts[n - 1] */
	unsigned short flags; /* AE_CACHE_ENTRY_* */
	unsigned int key; /* STRINGS: [a-z0-9_]{1,31} */
	unsigned int display; /* STRINGS */
	unsigned int first_tag, tag_count;
	/* weapons: the own 'weap' tag; tag sets: unused; grenades: the projectile tag (also below) */
	unsigned int root_tag;
	unsigned int item_collection_tag; /* weapons: an 'itmc' tag or none */
	unsigned int label; /* weapons: STRINGS, the animation label the player holds it by */
	unsigned int fallback_labels[2]; /* weapons: STRINGS or none */
	unsigned int menu_name; /* weapons: STRINGS */
	unsigned int weapon_pickup_text, ammo_pickup_text; /* weapons: STRINGS or none */
	unsigned int icon_bitmap_tag; /* weapons: a 'bitm' tag or none */
	unsigned int icon_sequence;
	unsigned char behaviour; /* weapons: 0 to AE_CACHE_BEHAVIOUR_LAST */
	unsigned char stock_fallback; /* weapons: ae_weapon_slots.h's ae_weapon_stock */
	unsigned char replaces_map_own; /* weapons: AE_CACHE_REPLACES_MAP_* */
	unsigned char legacy_slot; /* weapons: 1-24 or 0 */
	unsigned char grenade_catalogue_id; /* grenades: the catalogue's id */
	unsigned char pad[3];
	/* grenades: tags by cache index (or none), replacing the name lookups under ae\grenades\ */
	unsigned int projectile_tag, equipment_tag, collection_tag, hud_tag, field_effect_tag;
	/* tag sets: the root tags' list in DIRECTORY's tail (u32 tag indices, AE_CACHE_MAXIMUM_ROOT_LIST in all) */
	unsigned short root_list_first, root_list_count;
};

/* CONFIGS: one record per config, sorted by config id. A variant is (weapon id, config id): the config's values win,
else the weapon's own. override_mask names which of the values below apply (AE_CACHE_OVERRIDE_*) */
enum
{
	AE_CACHE_OVERRIDE_ROUNDS_LOADED = 1u << 0,
	AE_CACHE_OVERRIDE_ROUNDS_INITIAL = 1u << 1,
	AE_CACHE_OVERRIDE_ROUNDS_TOTAL = 1u << 2,
	AE_CACHE_OVERRIDE_RATE_OF_FIRE = 1u << 3,
	AE_CACHE_OVERRIDE_ERROR_ANGLE = 1u << 4,
	AE_CACHE_OVERRIDE_ZOOM = 1u << 5,
	AE_CACHE_OVERRIDE_AUTOAIM = 1u << 6,
	AE_CACHE_OVERRIDE_MAGNETISM = 1u << 7,
	AE_CACHE_OVERRIDE_BEHAVIOUR = 1u << 8,
	AE_CACHE_OVERRIDE_HUD = 1u << 9,
	AE_CACHE_OVERRIDE_MENU_NAME = 1u << 10,
	AE_CACHE_KNOWN_OVERRIDES = 0x7FF
};

struct ae_cache_config_record
{
	unsigned int config_id;
	unsigned int weapon_id;
	unsigned int key; /* STRINGS */
	unsigned int display; /* STRINGS or none */
	unsigned int override_mask;
	unsigned int rounds_loaded_maximum, rounds_total_initial, rounds_total_maximum;
	float rate_of_fire_minimum, rate_of_fire_maximum;
	float error_angle_minimum, error_angle_maximum; /* radians, the trigger's */
	unsigned int zoom_level_count;
	float magnification_maximum;
	float autoaim_angle, autoaim_range;
	float magnetism_angle, magnetism_range;
	unsigned int behaviour;
	unsigned int hud_interface_tag; /* a 'wphi' cache tag or none */
	unsigned int menu_name; /* STRINGS */
	unsigned int reserved[3];
};

/* EXTERNS: what is left to the map after the base layer (empty in a release build; dev builds and later parts) */
struct ae_cache_extern_record
{
	unsigned int group;
	unsigned int name; /* STRINGS */
	unsigned int required;
	unsigned int entry_first, entry_count; /* into the extern entry list: u32 directory ids, at the section's tail */
};

/* ---------- the sections' shapes (fixed with the tools lane; tools/weapon_pack/aepack.py's cache functions write the same)

DIRECTORY and EXTERNS start with a 16-byte PRELUDE of four little-endian words: the record count, the tail list's
length in words, and two zero words. The records follow (ae_cache_directory_record, ae_cache_extern_record), then the
tail list of u32s (DIRECTORY: the tag sets' root tag indices; EXTERNS: the directory ids each extern serves). The section
is exactly 16 + count x record size + tail x 4 bytes. CONFIGS is whole ae_cache_config_record records, no prelude. */
enum
{
	AE_CACHE_PRELUDE_SIZE = 16,
	AE_CACHE_MAXIMUM_EXTERNS = 8192,
	AE_CACHE_MAXIMUM_EXTERN_ENTRIES = 65536
};

/* the records' sizes, fixed by the freeze (the reader and the Python twin both assert them) */
typedef char ae_cache_part_size_check[sizeof(struct ae_cache_part) == 0x40 ? 1 : -1];
typedef char ae_cache_header_size_check[sizeof(struct ae_cache_header) == AE_CACHE_HEADER_SIZE ? 1 : -1];
typedef char ae_cache_directory_size_check[sizeof(struct ae_cache_directory_record) == 96 ? 1 : -1];
typedef char ae_cache_config_size_check[sizeof(struct ae_cache_config_record) == 96 ? 1 : -1];
typedef char ae_cache_extern_size_check[sizeof(struct ae_cache_extern_record) == 20 ? 1 : -1];

/* an open cache's extra records (struct ae_pack's `cache`; all pointers into the pack's resident sections) */
struct ae_cache_info
{
	struct ae_cache_header header;
	struct ae_cache_directory_record const *directory;
	unsigned int directory_count;
	unsigned int const *root_list;
	unsigned int root_list_count;
	struct ae_cache_config_record const *configs;
	unsigned int config_count;
	struct ae_cache_extern_record const *externs;
	unsigned int extern_count;
	unsigned int const *extern_entries;
	unsigned int extern_entry_count;
	unsigned int weapon_entries; /* directory records of kind weapon */
	/* weapon ordinal (the order of the weapon entries) to its directory index; the pack's `weapons` holds the same weapons'
	pack-style records in this order (made at open, so code written for a pack's weapons reads a cache's) */
	unsigned short weapon_directory[AE_CACHE_MAXIMUM_WEAPONS];
};

/* ---------- the network (ae_silent_rules.h has the join name block; NETCODE.md the rest at T3-2) */

/* the host's cache identity to every machine that told a cache block, reliably, when ready and when the selection
changes (network_distributed.h gets its enum value at T3-2, which checks that 107 is free); and the client's echo of
its own after a mismatch, so the host logs and drops it */
enum
{
	AE_CACHE_MESSAGE_HOST_ID = 109,
	AE_CACHE_MESSAGE_CLIENT_ECHO = 110
};

#endif /* AE_WEAPON_CACHE_FORMAT_H */
