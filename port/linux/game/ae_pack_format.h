/*
AE_PACK_FORMAT.H

Weapon pack files, format v1 (`.aepack`, docs/weapon-packs.md): the reader
and validator, the writer, and the SHA-256 that names a pack.

A pack is untrusted data (players download them): ae_pack_open checks every
length, offset, count and string against the file and the format's limits,
and the SHA-256 of the whole file, before anything is handed out. What it
checks is the CONTAINERS: every tag's bytes, fixups, parts and resources lie
where they claim, no two own tags' bytes, no two of a tag's fixups and no
two of its parts' structs overlap (the parts' vertices and indices in
GEOMETRY and the resources' bytes in RESOURCE_DATA may: they are only
read, and each lies inside its section), every fixup lies inside its tag's bytes and names
a valid target, every string is terminated, the pickup texts are safe
formats. It does NOT check the structure inside a tag's bytes (block counts
and element sizes behind POINTER fixups, data sizes, a model part's own
fields against its PARTS record, pointers no fixup covers): a pack's tags
must go through the tag validator (tag_validate.c) once they are relocated,
as a map's tags do, before the engine reads them. RESOURCE_DATA stays in
the file: ae_pack_read_resource reads (and for zlib inflates) one resource
when it is asked for.

No global state: a pack is its own struct. A pack is read by one thread at a
time (its resource reads share the file).

Every type here is the same in the game's ABI and the platform layer's
(unsigned ints, pointers, no doubles, no structs returned by value), so the
check tool (tools/ae_pack_check.c) calls these functions across the two.
Little-endian hosts only, as the game.
*/

#ifndef AE_PACK_FORMAT_H
#define AE_PACK_FORMAT_H

/* ---------- constants */

#define AE_PACK_MAGIC "AEWPACK" /* and its terminator: 8 bytes */
#define AE_PACK_NONE 0xFFFFFFFFu

enum
{
	AE_PACK_FORMAT_VERSION = 1,
	AE_PACK_HEADER_SIZE = 0x100,
	AE_PACK_MAXIMUM_SECTIONS = 12,
	/* tag layout 1: the Xbox 01.10.12.2276 tag layouts, which every Xbox build the engine plays shares */
	AE_PACK_TAG_LAYOUT_XBOX = 1,
	/* sections and the items in TAG_BYTES, GEOMETRY and RESOURCE_DATA start on this */
	AE_PACK_ALIGNMENT = 16,
	AE_PACK_SHA256_SIZE = 32,
	AE_PACK_SHA256_OFFSET = 0x5C,
	AE_PACK_ID_SIZE = 32,
	AE_PACK_DISPLAY_NAME_SIZE = 32,
};

/* section kinds */
enum
{
	AE_PACK_SECTION_STRINGS = 1,
	AE_PACK_SECTION_TAGS,
	AE_PACK_SECTION_TAG_BYTES,
	AE_PACK_SECTION_FIXUPS,
	AE_PACK_SECTION_PARTS,
	AE_PACK_SECTION_GEOMETRY,
	AE_PACK_SECTION_RESOURCES,
	AE_PACK_SECTION_RESOURCE_DATA,
	AE_PACK_SECTION_WEAPONS,
	AE_PACK_SECTION_CREDITS,
	AE_PACK_SECTION_KIND_LIMIT /* (one past the last) */
};

/* compressions (R2: format v1's writer writes stored only; readers take zlib) */
enum
{
	AE_PACK_STORED = 0,
	AE_PACK_ZLIB = 1,
};

/* header flags */
enum
{
	/* a pack of tags only (no weapon entries): the vehicle damage pack (ae_vehicle_pack.c), whose tags the
	game finds by name; a weapon slot never takes one, and every pack with this bit clear has weapons */
	AE_PACK_FLAG_NO_WEAPONS = 1,
	AE_PACK_KNOWN_HEADER_FLAGS = 1,
};

/* tag kinds and flags (R9) */
enum
{
	AE_PACK_TAG_OWN = 0,
	AE_PACK_TAG_EXTERNAL = 1,

	/* a bitmap sampled linear with mips, as the high-res HUD's */
	AE_PACK_TAG_FLAG_SMOOTH = 1,
	/* a bitmap whose green channel is coverage (a meter) */
	AE_PACK_TAG_FLAG_METER_COVERAGE = 2,
	AE_PACK_TAG_KNOWN_FLAGS = 3,
};

/* fixup kinds */
enum
{
	/* a 4-byte Xbox address at offset: the tag's copy's base + value (value inside the tag's bytes) */
	AE_PACK_FIXUP_POINTER = 1,
	/* a 16-byte tag reference at offset: pack tag value, or AE_PACK_NONE */
	AE_PACK_FIXUP_REFERENCE = 2,
	/* a 4-byte tag index at offset: pack tag value */
	AE_PACK_FIXUP_TAG_INDEX = 3,
	AE_PACK_TAG_REFERENCE_SIZE = 16,
};

/* tag layout 1's model parts (model_geometry_part, 0x68 bytes; rasterizer_geometry.h's types) */
enum
{
	AE_PACK_MODEL_PART_SIZE = 0x68,
	AE_PACK_VERTEX_TYPES = 12,
	/* (a model part's vertices are one of these two) */
	AE_PACK_VERTEX_MODEL_UNCOMPRESSED = 4,
	AE_PACK_VERTEX_MODEL_COMPRESSED = 5,
	AE_PACK_TRIANGLES = 0,
	AE_PACK_TRIANGLE_STRIP = 1,
	AE_PACK_MAXIMUM_PART_COUNT = 0xFFFF,
};

/* the limits: a pack past one is refused whole */
enum
{
	AE_PACK_MAXIMUM_FILE_SIZE = 64 * 1024 * 1024,
	AE_PACK_MAXIMUM_TAGS = 1024,
	AE_PACK_MAXIMUM_OWN_TAGS = 512,
	AE_PACK_MAXIMUM_TAG_SIZE = 4 * 1024 * 1024,
	AE_PACK_MAXIMUM_TAG_BYTES = 16 * 1024 * 1024,
	AE_PACK_MAXIMUM_RESOURCE_SIZE = 8 * 1024 * 1024,
	AE_PACK_MAXIMUM_RESOURCES_SIZE = 48 * 1024 * 1024,
	AE_PACK_MAXIMUM_STRINGS = 256 * 1024,
	AE_PACK_MAXIMUM_FIXUPS = 262144,
	AE_PACK_MAXIMUM_PARTS = 4096,
	AE_PACK_MAXIMUM_GEOMETRY = 16 * 1024 * 1024,
	AE_PACK_MAXIMUM_RESOURCES = 4096,
	AE_PACK_MAXIMUM_WEAPONS = 8,
	AE_PACK_MAXIMUM_CREDITS = 64 * 1024,
	/* strings' lengths, without their terminators */
	AE_PACK_MAXIMUM_NAME = 255,
	AE_PACK_MAXIMUM_KEY = 31,
	AE_PACK_MAXIMUM_LABEL = 31,
	AE_PACK_MAXIMUM_MENU_NAME = 63,
	AE_PACK_MAXIMUM_PICKUP_TEXT = 127,
	AE_PACK_MAXIMUM_ICON_SEQUENCE = 0xFFFF,
};

/* weapon entries' values */
enum
{
	/* behaviour: as its tags say, or one of the AE rifles (ae_burst_rifle.c plays them, ae_rifle_profiles.c their
	numbers): the burst BATTLE RIFLE, the single-shot EVO RIFLE, the zero-bloom DMR (0.7.0; older builds refuse 2 and 3) */
	AE_PACK_BEHAVIOUR_TAGS = 0,
	AE_PACK_BEHAVIOUR_AE_BATTLE_RIFLE = 1,
	AE_PACK_BEHAVIOUR_AE_EVO_RIFLE = 2,
	AE_PACK_BEHAVIOUR_AE_DMR = 3,
	AE_PACK_BEHAVIOUR_LAST = AE_PACK_BEHAVIOUR_AE_DMR,
	/* replaces: the stock loadout weapon whose map spawns it may replace (tools/port_settings.py
	LOADOUT_WEAPONS: 0 none, 2 assault rifle ... 9 needler; 1, random, is not one) */
	AE_PACK_REPLACES_NONE = 0,
	AE_PACK_REPLACES_ASSAULT_RIFLE = 2,
	AE_PACK_REPLACES_LAST = 9,
};

#define AE_PACK_GROUP_WEAPON 0x77656170u /* 'weap' */
#define AE_PACK_GROUP_ITEM_COLLECTION 0x69746D63u /* 'itmc' */
#define AE_PACK_GROUP_BITMAP 0x6269746Du /* 'bitm' */

/* ---------- the file's records (little-endian; every field 4-aligned, no padding) */

struct ae_pack_header
{
	char magic[8];
	unsigned int format_version;
	unsigned int header_size;
	unsigned int flags; /* AE_PACK_FLAG_* */
	char pack_id[AE_PACK_ID_SIZE]; /* [a-z0-9_]{1,31}, NUL-padded */
	unsigned int pack_revision;
	char display_name[AE_PACK_DISPLAY_NAME_SIZE]; /* printable ASCII {1,31}, NUL-padded */
	unsigned int tag_layout;
	unsigned char sha256[AE_PACK_SHA256_SIZE]; /* of the whole file with these 32 bytes zeroed */
	unsigned int section_count;
	unsigned char reserved[0x80];
};

/* the section table follows the header: section_count entries */
struct ae_pack_section
{
	unsigned int kind;
	unsigned int offset;
	unsigned int stored_size;
	unsigned int inflated_size;
	unsigned int compression;
};

struct ae_pack_tag_record
{
	unsigned int group;
	unsigned int parent_groups[2];
	unsigned int name; /* STRINGS */
	unsigned char kind; /* AE_PACK_TAG_OWN, AE_PACK_TAG_EXTERNAL */
	unsigned char required; /* externals: 1 the map must have it, 0 NONE if missing; own tags 0 */
	unsigned char flags; /* AE_PACK_TAG_FLAG_* (own bitmaps only) */
	unsigned char pad;
	unsigned int source_path; /* STRINGS: provenance */
	unsigned int bytes_offset; /* TAG_BYTES: its root, blocks and data, address-free */
	unsigned int bytes_size;
	unsigned int fixups_first, fixups_count;
	unsigned int parts_first, parts_count;
	unsigned int resources_first, resources_count;
};

struct ae_pack_fixup_record
{
	unsigned int offset; /* in its tag's bytes */
	unsigned short kind; /* AE_PACK_FIXUP_* */
	unsigned short pad;
	unsigned int value;
};

struct ae_pack_part_record
{
	unsigned int offset; /* its model_geometry_part in its tag's bytes */
	unsigned short vertex_type;
	unsigned short triangle_type;
	unsigned int vertex_count;
	unsigned int triangle_count;
	unsigned int vertices_offset, vertices_size; /* GEOMETRY */
	unsigned int indices_offset, indices_size; /* GEOMETRY: 16-bit indices */
};

struct ae_pack_resource_record
{
	unsigned int tag; /* the tag whose resources hold it */
	unsigned int map_offset; /* where the tag's bytes say it is in the "map file" */
	unsigned int size; /* inflated */
	unsigned int data_offset, stored_size; /* RESOURCE_DATA */
	unsigned int compression;
};

struct ae_pack_weapon_record
{
	unsigned int key; /* STRINGS: [a-z0-9_]{1,31}, "battle_rifle" */
	unsigned int root_tag; /* an own 'weap' tag */
	unsigned int item_collection_tag; /* an 'itmc' tag or AE_PACK_NONE */
	unsigned int behaviour; /* AE_PACK_BEHAVIOUR_* */
	unsigned int label; /* STRINGS: the animation label the player holds it by ("h_br") */
	unsigned int fallback_labels[2]; /* STRINGS or AE_PACK_NONE, in order ("ar") */
	unsigned int menu_name; /* STRINGS: "BATTLE RIFLE" */
	/* STRINGS or AE_PACK_NONE (the engine's own text): the weapon picked up ("Picked up a battle rifle": no
	'%' at all), its ammo picked up ("Picked up %d rounds for battle rifle": exactly one "%d", no other '%';
	the engine fills in the rounds) */
	unsigned int weapon_pickup_text;
	unsigned int ammo_pickup_text;
	unsigned int icon_bitmap_tag; /* a 'bitm' tag or AE_PACK_NONE: the message icon (R10) */
	unsigned int icon_sequence;
	unsigned int replaces; /* AE_PACK_REPLACES_* */
	unsigned int power; /* 0 or 1 */
};

typedef char ae_pack_header_size_check[sizeof(struct ae_pack_header) == AE_PACK_HEADER_SIZE ? 1 : -1];
typedef char ae_pack_section_size_check[sizeof(struct ae_pack_section) == 20 ? 1 : -1];
typedef char ae_pack_tag_record_size_check[sizeof(struct ae_pack_tag_record) == 56 ? 1 : -1];
typedef char ae_pack_fixup_record_size_check[sizeof(struct ae_pack_fixup_record) == 12 ? 1 : -1];
typedef char ae_pack_part_record_size_check[sizeof(struct ae_pack_part_record) == 32 ? 1 : -1];
typedef char ae_pack_resource_record_size_check[sizeof(struct ae_pack_resource_record) == 24 ? 1 : -1];
typedef char ae_pack_weapon_record_size_check[sizeof(struct ae_pack_weapon_record) == 56 ? 1 : -1];

/* ---------- an open pack */

struct ae_pack
{
	struct ae_pack_header header;
	struct ae_pack_section sections[AE_PACK_MAXIMUM_SECTIONS];
	unsigned int file_size;

	/* the resident sections (empty ones: a valid pointer, size 0) */
	char const *strings;
	unsigned int strings_size;
	struct ae_pack_tag_record const *tags;
	unsigned int tag_count;
	unsigned int own_tag_count;
	unsigned char const *tag_bytes;
	unsigned int tag_bytes_size;
	struct ae_pack_fixup_record const *fixups;
	unsigned int fixup_count;
	struct ae_pack_part_record const *parts;
	unsigned int part_count;
	unsigned char const *geometry;
	unsigned int geometry_size;
	struct ae_pack_resource_record const *resources;
	unsigned int resource_count;
	unsigned int resources_size; /* inflated, all of them */
	struct ae_pack_weapon_record const *weapons;
	unsigned int weapon_count;
	char const *credits; /* terminated */
	unsigned int credits_size;

	/* RESOURCE_DATA, in the file */
	unsigned int resource_data_offset;
	unsigned int resource_data_size;

	/* (ae_pack_format.c's: the file, or the image ae_pack_write checks; the sections' memory) */
	void *file;
	unsigned char const *image;
	unsigned char *memory[AE_PACK_SECTION_KIND_LIMIT];
};

/* why a pack was refused (or a write failed) */
struct ae_pack_problem
{
	char text[256];
};

/* ---------- what a pack is written from (ae_pack_write) */

struct ae_pack_build_part
{
	unsigned int offset;
	unsigned short vertex_type;
	unsigned short triangle_type;
	unsigned int vertex_count;
	unsigned int triangle_count;
	void const *vertices;
	unsigned int vertices_size;
	void const *indices;
	unsigned int indices_size;
};

struct ae_pack_build_resource
{
	unsigned int map_offset;
	void const *data; /* written stored (R2) */
	unsigned int size;
};

struct ae_pack_build_tag
{
	unsigned int group;
	unsigned int parent_groups[2];
	char const *name;
	char const *source_path; /* NULL: "" */
	unsigned char kind;
	unsigned char required;
	unsigned char flags;
	void const *bytes; /* own tags */
	unsigned int bytes_size;
	struct ae_pack_fixup_record const *fixups;
	unsigned int fixup_count;
	struct ae_pack_build_part const *parts;
	unsigned int part_count;
	struct ae_pack_build_resource const *resources;
	unsigned int resource_count;
};

struct ae_pack_build_weapon
{
	char const *key;
	unsigned int root_tag;
	unsigned int item_collection_tag;
	unsigned int behaviour;
	char const *label;
	char const *fallback_labels[2]; /* NULL: none */
	char const *menu_name;
	char const *weapon_pickup_text; /* NULL: none */
	char const *ammo_pickup_text; /* NULL: none */
	unsigned int icon_bitmap_tag;
	unsigned int icon_sequence;
	unsigned int replaces;
	unsigned int power;
};

struct ae_pack_build_data
{
	char const *pack_id;
	unsigned int pack_revision;
	char const *display_name;
	struct ae_pack_build_tag const *tags; /* in the order they are written (breadth-first from the roots) */
	unsigned int tag_count;
	struct ae_pack_build_weapon const *weapons;
	unsigned int weapon_count; /* 0: a pack of tags only (AE_PACK_FLAG_NO_WEAPONS) */
	char const *credits; /* NULL: none */
};

/* ---------- SHA-256 (FIPS 180-4; R1: our own, streaming) */

struct ae_pack_sha256
{
	unsigned int state[8];
	unsigned int length_low; /* bytes added (64 bits) */
	unsigned int length_high;
	unsigned char block[64];
	unsigned int used;
};

void ae_pack_sha256_begin(struct ae_pack_sha256 *context);
void ae_pack_sha256_add(struct ae_pack_sha256 *context, void const *data, unsigned int size);
void ae_pack_sha256_end(struct ae_pack_sha256 *context, unsigned char *digest);
void ae_pack_sha256(void const *data, unsigned int size, unsigned char *digest);

/* ---------- the reader */

/* the pack at path read and checked: nonzero and *pack set (ae_pack_close it), or zero, *pack NULL and the
reason in problem */
int ae_pack_open(char const *path, struct ae_pack **pack, struct ae_pack_problem *problem);

/* size bytes of tag's resources from map_offset (inside one of its resources) into buffer, inflating a zlib
one: nonzero if read, zero if not (no such resource, the file changed, its stream lies) */
int ae_pack_read_resource(struct ae_pack *pack, unsigned int tag, unsigned int map_offset, unsigned int size,
	void *buffer);

/* every resource read (and inflated) whole: nonzero if all are as their records say (the check tool; the engine
may ask it once) */
int ae_pack_verify_resources(struct ae_pack *pack, struct ae_pack_problem *problem);

/* a string of the pack's STRINGS (checked at open), or NULL for AE_PACK_NONE */
char const *ae_pack_string(struct ae_pack const *pack, unsigned int offset);

/* a section kind's name ("TAGS"), or "?" */
char const *ae_pack_section_name(unsigned int kind);

void ae_pack_close(struct ae_pack *pack);

/* ---------- the writer */

/* the pack written to path, laid out as docs/weapon-packs.md "Layout" says (deterministic: the same data gives
the same bytes), every section and resource stored (R2); the pack is checked as ae_pack_open checks one before
anything is written: nonzero if written, zero and the reason in problem if not */
int ae_pack_write(char const *path, struct ae_pack_build_data const *data, struct ae_pack_problem *problem);

/* ae_pack_write's last step: the whole file at temporary put at path by move (the C library's rename: 0 if moved).
Where move does not replace an existing file (Windows), the old file is moved to <path>.old first and deleted only
once the new one is in place, or moved back: nonzero if the new file is at path; if not, path holds what it held
(the temporary is the caller's to remove). Exposed for its test (tools/ae_pack_check.c --writer-check). */
int ae_pack_replace_file(char const *temporary, char const *path, int (*move)(char const *from, char const *to));

#endif
