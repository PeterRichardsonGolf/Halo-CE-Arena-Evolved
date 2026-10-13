# Weapon packs: file format v1

A weapon pack (`.aepack`) carries one or more weapons for Arena Evolved to add to a map that does not have
them: each weapon's tags (address-free), the geometry of its models, its bitmaps' pixels and sounds' samples,
and what the engine needs to know about it (its label, menu name, pickup texts, message icon, the stock weapon
whose spawns it may replace). Packs are untrusted data: players download them. This document is the format;
the reader that the game and the tools use is `port/linux/game/ae_pack_format.c` (`ae_pack_format.h`), and
`tools/weapon_pack/aepack.py` is the same reader and writer in Python, held to agree with it on every pack by
`tools/test_weapon_pack_format.py`.

`build/linux/ae_pack_check` (built by `ninja linux`) checks packs as the game does and prints them:

    build/linux/ae_pack_check [--no-resources] pack.aepack...
                                                  header, sections, tags with flags, weapons; every resource
                                                  read and inflated (unless --no-resources); exit 1 if refused
    build/linux/ae_pack_check --sha256 file...    our SHA-256 of files
    build/linux/ae_pack_check --rewrite in out    in written again by ae_pack_write
    build/linux/ae_pack_check --writer-check out  ae_pack_write's own test: a pack written and read back, bad
                                                  build data refused, a failed write leaving the old pack

**What the reader does not check.** It checks the containers: every tag's bytes, fixups, parts, resources and
strings lie where they claim, no two own tags' bytes, no two of a tag's fixups and no two of its parts' structs
overlap, and every fixup names a valid target. (The parts' vertices and indices in GEOMETRY and the resources'
bytes in RESOURCE_DATA may overlap each other: they are only read, and each lies inside its section.) It does NOT check the
structure inside a tag's bytes (block counts and element sizes behind POINTER fixups, data sizes, a model
part's own fields against its PARTS record, pointers no fixup covers). The engine must run a pack's tags
through the tag validator (`tag_validate.c`) once they are relocated into the map, as it does a map's own tags,
before anything reads them.

## The file

Little-endian throughout. Every offset is from the start of the file unless said otherwise, and every
"string" is an offset into STRINGS (`0xFFFFFFFF`, NONE, where a field may have none).

### Header (0x100 bytes)

| Offset | Field | Value |
|---|---|---|
| 0x00 | magic, char[8] | `AEWPACK\0` |
| 0x08 | format_version, u32 | 1 |
| 0x0C | header_size, u32 | 0x100 |
| 0x10 | flags, u32 | 0 |
| 0x14 | pack_id, char[32] | ASCII `[a-z0-9_]`, 1 to 31 characters, zero-padded (`ae_battle_rifle`) |
| 0x34 | pack_revision, u32 | 1, 2, ... |
| 0x38 | display_name, char[32] | printable ASCII, 1 to 31 characters, zero-padded (`BATTLE RIFLE`) |
| 0x58 | tag_layout, u32 | 1 = the Xbox 01.10.12.2276 tag layouts, which every Xbox build the engine plays shares |
| 0x5C | sha256, u8[32] | SHA-256 of the whole file with these 32 bytes zeroed |
| 0x7C | section_count, u32 | 1 to 12 |
| 0x80 | reserved, u8[0x80] | zeros |

The section table follows the header (at 0x100): `section_count` entries of 20 bytes (twelve of them would not
fit in the header itself).

| Offset | Field | |
|---|---|---|
| 0x00 | kind, u32 | 1 STRINGS, 2 TAGS, 3 TAG_BYTES, 4 FIXUPS, 5 PARTS, 6 GEOMETRY, 7 RESOURCES, 8 RESOURCE_DATA, 9 WEAPONS, 10 CREDITS |
| 0x04 | offset, u32 | 16-aligned, after the table |
| 0x08 | stored_size, u32 | bytes in the file |
| 0x0C | inflated_size, u32 | bytes once inflated (= stored_size when stored) |
| 0x10 | compression, u32 | 0 stored, 1 zlib (RFC 1950) |

A kind appears at most once; a kind not in the table is empty.

### Sections

- **STRINGS**: NUL-terminated UTF-8 strings, the section ending with a terminator.
- **TAGS**: one 56-byte record a tag:

  | Offset | Field | |
  |---|---|---|
  | 0x00 | group, u32 | the tag's group (`'weap'` = 0x77656170) |
  | 0x04 | parent_groups, u32[2] | as the map's tag table has them (NONE for none) |
  | 0x0C | name, string | the tag's name: printable ASCII, 1 to 255 characters |
  | 0x10 | kind, u8 | 0 own (its bytes are in the pack), 1 external (the map's own tag of that group and name) |
  | 0x11 | required, u8 | externals: 1 the map must have it, 0 NONE if missing; own tags: 0 |
  | 0x12 | flags, u8 | bit 0 SMOOTH: a bitmap sampled linear with mips as the high-res HUD's; bit 1 METER COVERAGE: its green channel is coverage. Own bitmaps only; other bits 0 |
  | 0x13 | pad, u8 | 0 |
  | 0x14 | source_path, string | provenance: the tag's name where it came from (UTF-8, up to 255 bytes, may be empty) |
  | 0x18 | bytes_offset, bytes_size, u32 | own tags: its root, blocks and data in TAG_BYTES, 1 byte to 4 MiB; externals: 0, 0 |
  | 0x20 | fixups first, count, u32 | its FIXUPS records |
  | 0x28 | parts first, count, u32 | its PARTS records |
  | 0x30 | resources first, count, u32 | its RESOURCES records |

- **TAG_BYTES**: each own tag's bytes, address-free: every pointer, tag reference and tag index in them is a
  fixup.
- **FIXUPS**: 12-byte records {offset u32 (in the tag's bytes), kind u16, pad u16 (0), value u32}, a tag's in
  offset order, none overlapping another:
  - kind 1 POINTER: a 4-byte Xbox address at offset becomes the tag's copy's base + value (value inside the
    tag's bytes);
  - kind 2 REFERENCE: a 16-byte tag reference at offset names pack tag value, or none (NONE);
  - kind 3 TAG_INDEX: a 4-byte tag index at offset names pack tag value.
- **PARTS**: 32-byte records, a model part's geometry {offset u32 (its 0x68-byte model_geometry_part in the tag's
  bytes; each at least 0x68 bytes after the previous), vertex_type u16 (4 model uncompressed, 5 model
  compressed), triangle_type u16 (0 triangles, 1 strip), vertex_count u32, triangle_count u32 (each 1..65535),
  vertices offset u32 and size u32, indices offset u32 and size u32 (in GEOMETRY)}. The vertices' size is the
  count times the type's size (rasterizer_geometry.c: 68 for type 4, 32 for type 5); the indices are 16-bit, 3 a triangle or count + 2 for a strip, each below the vertex count.
- **GEOMETRY**: the parts' vertices and indices.
- **RESOURCES**: 24-byte records {tag u32, map_offset u32, size u32 (inflated, 1 byte to 8 MiB), data_offset u32,
  stored_size u32 (in RESOURCE_DATA), compression u32 (0 stored, 1 zlib)}: a bitmap's pixels or a sound's
  samples, which the engine's caches ask for by the tag's index and the offset the tag's bytes give
  (`map_offset`, where they were in the map the tag came from).
- **RESOURCE_DATA**: the resources' bytes. It stays in the file: the engine reads one resource when it is asked
  for (`ae_pack_read_resource`), so a pack costs only its resident sections in memory. Always stored (each
  resource has its own compression).
- **WEAPONS**: 56-byte records, one a weapon:

  | Offset | Field | |
  |---|---|---|
  | 0x00 | key, string | `[a-z0-9_]`, 1 to 31 characters, unique in the pack (`battle_rifle`) |
  | 0x04 | root_tag, u32 | an own `'weap'` tag |
  | 0x08 | item_collection_tag, u32 | an `'itmc'` tag, or NONE |
  | 0x0C | behaviour, u32 | 0 as its tags say, 1 the AE Halo 2 battle rifle |
  | 0x10 | label, string | the animation label the player holds it by (`h_br`): printable ASCII, 1 to 31 |
  | 0x14 | fallback_labels, string[2] | tried in order when the map's biped lacks the label (`ar`), or NONE |
  | 0x1C | menu_name, string | UTF-8, 1 to 63 bytes (`BATTLE RIFLE`) |
  | 0x20 | weapon_pickup_text, string | the weapon picked up (`Picked up a battle rifle`): UTF-8, 1 to 127 bytes, **no `%` at all**; or NONE (the engine's own) |
  | 0x24 | ammo_pickup_text, string | its ammo picked up (`Picked up %d rounds for battle rifle`; the engine fills in the rounds): UTF-8, 1 to 127 bytes, **exactly one `%`, which is `%d`** (no flags, width, precision, other conversion or `%%`); or NONE |
  | 0x28 | icon_bitmap_tag, u32 | the message icon: a `'bitm'` tag, or NONE (then icon_sequence 0) |
  | 0x2C | icon_sequence, u32 | 0 to 65535: the bitmap's sequence the icon is a sprite of, which must have sprites (a sheet drawn whole would cover the screen): the builder refuses the pack; the engine refuses the icon for that map with one log line, and the prompt draws no icon (the same when the bitmap is an optional external the map lacks). The engine uses the icon only when the weapon's HUD is an own tag of the pack: a HUD shared with the map keeps the map's weapons' icon |
  | 0x30 | replaces, u32 | the stock loadout weapon whose map spawns it may replace: 0 none, 2 assault rifle ... 9 needler (tools/port_settings.py `LOADOUT_WEAPONS`; 1, random, is not one) |
  | 0x34 | power, u32 | 0 or 1 (a power weapon) |

- **CREDITS**: UTF-8 text, no NULs, shown in the log.

### Limits

A pack past any of these is refused whole:

| What | Limit |
|---|---|
| the file | 64 MiB |
| tags / own tags | 1024 / 512 |
| one tag's bytes | 4 MiB |
| TAG_BYTES (inflated) | 16 MiB |
| resources, inflated | 8 MiB each, 48 MiB in all |
| STRINGS | 256 KiB |
| fixups | 262,144 |
| parts / GEOMETRY | 4,096 / 16 MiB |
| resource records | 4,096 |
| weapons | 8 |
| CREDITS | 64 KiB |

(Every resident section has a limit, checked before it is read or inflated.)

## What the reader checks

In this order; the first failure refuses the pack, with its reason:

1. The header: every field as the table says (magic, version 1, header size 0x100, flags 0, pack id, revision
   at least 1, display name, tag layout 1, 1 to 12 sections, reserved bytes zero), and the section table there.
2. The file read once, through our own streaming SHA-256 (FIPS 180-4, `ae_pack_sha256*`; no library call,
   so no ABI question between the game's code and the platform layer's): its size at most 64 MiB, its hash
   with the 32 hash bytes counted as zeros.
3. Each section: a known kind, not repeated; compression 0 or 1 (RESOURCE_DATA: 0); a stored section's two
   sizes equal, a zlib one not empty; 16-aligned, after the table, inside the file; its inflated size within
   its kind's limit and whole records. No two non-empty sections overlap.
4. The SHA-256 equal to the header's.
5. The resident sections read; a zlib one inflated to exactly its inflated size (the stream ended, nothing
   after it); sizes are checked before anything is allocated or inflated.
6. STRINGS ends with a terminator; at least one tag and one weapon.
7. Each tag: its name and source path valid strings; its group not 0 or NONE; kind 0 or 1, pad 0; no unknown
   flag bit, flags on own bitmaps only; an own tag not required, its bytes 1 byte to 4 MiB inside TAG_BYTES,
   after the previous own tag's (no two own tags share or overlap bytes); an external tag without bytes, fixups, parts or resources; its fixups, parts and resources inside their
   sections and after the previous tag's (no two tags share a record); at most 512 own tags; every record
   owned by a tag; no two tags of the same group and name.
8. Each own tag's fixups (inside its bytes; pointers to a target inside them; references to a pack tag or
   NONE; tag indices to a pack tag; in offset order, none overlapping another: 4 bytes a pointer or tag index,
   16 a reference), parts (inside its bytes, each at least 0x68 bytes after the previous; a model's vertex
   type, 4 or 5; counts and sizes as above; inside GEOMETRY; every index below the vertex count) and resources (its own; 1 byte to 8 MiB; the
   offset plus size within 32 bits; stored sizes equal or a zlib stream not empty; inside RESOURCE_DATA; 48
   MiB in all).
9. Each weapon as its table says (the pickup texts' `%` rules included: they are formats the engine fills).
10. CREDITS: no NUL, valid UTF-8.

`ae_pack_verify_resources` (the check tool always; the engine may once) also reads every resource and inflates
the zlib ones, each to exactly its size. `ae_pack_read_resource` checks the same on every read: a stream that
lies returns nothing.

UTF-8 is checked as a strict decoder (Python's) takes it: no overlong forms, surrogates or values past
U+10FFFF.

## Layout (what a writer writes)

Format v1's writer (`ae_pack_write`; `aepack.build` the same, byte for byte) writes every section and resource
**stored** (the port's zlib inflates only, and other machines' zlib or zlib-ng deflate differently, which
would break "same inputs, same bytes"). Readers take zlib for later versions' writers.

- The header, then a table of all ten kinds in kind order, then each section in kind order at the next
  16-aligned offset (zeros between), empty ones too (their offset where the next would start). The file ends
  at the last section's end, or, when the last sections are empty, at their (aligned) offset: the padding up
  to it is written.
- STRINGS: each distinct string once, in the order they are first met: for each tag its name then its source
  path; then for each weapon its key, label, fallback labels, menu name, weapon pickup text, ammo pickup text.
- TAG_BYTES, GEOMETRY (each part's vertices then indices) and RESOURCE_DATA: each item at the next 16-aligned
  offset of its section, in tag order.
- A tag's records follow the previous tag's: `first` is the running count, for every tag. A tag's fixups are
  sorted by offset (then kind, value); its parts are written in the order given, which must be by offset.
- An external tag given bytes is refused, never written without them.
- The file is written as `<path>.tmp` and renamed over `<path>`. Where a rename does not replace a file
  (Windows), the old pack is first renamed to `<path>.old`, deleted only once the new one is in place, and renamed
  back if that fails: a write that fails leaves any pack already at `<path>` as it was.
- Tags in breadth-first order from the weapon roots as the pack builder's schema walk meets them; no
  timestamps anywhere.

So the same inputs give the same bytes and the same SHA-256. The writer checks its image as the reader does
before writing anything: it never writes a pack a reader would refuse.

## Memory

TAG_BYTES, FIXUPS, PARTS, GEOMETRY, STRINGS (and the small TAGS, RESOURCES, WEAPONS, CREDITS) are read at open:
a few MB. RESOURCE_DATA stays in the file, read per request; a pack keeps its file open until
`ae_pack_close`, and one thread at a time reads it (its reads share the file).

The hash is checked once, at open, by a pass of its own: a file changed after (or while) it is opened is used
with the identity it was opened with. Every read is still bounded by what open checked, and a resource read
from a changed file fails if it is short or its stream lies, but nothing re-hashes it. Packs are not expected
to change under a running game.

A zlib resource (no v1 writer makes one) is inflated whole on every read, even of a part of it.

## A pack on a map

A pack is added to a map when the map's tags have loaded and been checked, if the game about to be played
asks for it: the gametype's `weapon_pack` byte names its slot (the pending game variant, set by a network
game's host and clients and by a split-screen game just before the map loads). The menus' map and the campaign
have no game variant and never get a pack; nor does a map that is not a multiplayer map. The code is
`port/linux/game/ae_weapon_pack.c` (`ae_weapon_pack_tags_loaded`); it runs before the menus' own tags are added.

1. **External tags** are found on the map by group and name. A required one the map lacks refuses the pack for
   this map; an optional one becomes no tag.
2. **Own tags** each get a new place past the end of the map's tag table, in a grown copy of the table. An own
   tag is never the map's tag of the same name. Their bytes are copied into memory of the pack's own, laid out
   as a map's tags are (a tag header, the table, the names, the tags), and the fixups applied. Every fixup's
   offset and value is checked against its tag's bytes and the pack's tags again.
3. **The tag check.** Every own tag then goes through the tag validator's own walk, exactly as a map's tag
   does and against the real table (the map's tags and the pack's): blocks and data must lie in the pack's
   memory and overlap no other's, references must name tags of the right group, counts, indices and enums are
   cut or corrected, and the checks between tags run (`port/linux/game/ae_tag_validate.inc`, which is part of
   `tag_validate.c`). A pointer no fixup set, a block count past its bytes or a block lying over another
   fails here. Corrections are counted in the log line; a tag that fails refuses the pack.
4. **What the schemas do not say** is checked by hand: every PARTS record must sit on a model part of a model
   of the pack (only those get buffers), its sizes must be the ones the game draws its vertex type with, and
   every run of pixels a bitmap reads and samples a sound reads must lie inside one of that tag's resources.
5. **The checks against the map** (below), model parts get vertex and index buffers of their own, and only then
   does the grown table become the game's. Until that moment nothing of the map has changed.

### The checks against the map, and the fallback

Each failure is the refusal line with the reason shown; nothing of the map is changed.

| Check | Reason in the refusal line |
|---|---|
| the map's build is one the engine plays multiplayer on (2276, Halo 1: NHE's 2300, PAL's) | `build <b>` |
| the map's tags and the pack's own fit the table (49152 tags, salts to 0xfffe; tests: `debug.weapon_pack_table_limit`) | `tag table full: ...` |
| no own tag's group and name is already a tag of the map | `name collision: the map has <group> <name>` |
| every required external tag is on the map, by group and name | `missing <group> <name>` |
| the pack's own tag bytes and geometry are within 8 MiB, the memory is there, every model part gets its buffers | `no memory: ...` |
| the map's player (the globals' multiplayer unit's animation graph) has one of the weapon's labels, in order (`h_br`, `ar`), in every seat it holds weapons in | `the player cannot hold it: ...` |
| the weapon's first-person animations have every node of the map's first-person hands and of the weapon's first-person model | `first-person arms do not match: ...` |

A seat *holds weapons* when one of its weapon types has a label: the stock Spartan's driver's and gunner's seats
have only the unarmed type and are not counted (no weapon's label is found there, the assault rifle's neither).
The label chosen is written into the weapon's copy and logged (`weapon pack: battle_rifle is held by the label
'ar'`). Optional external tags the map lacks are no tag, with one line (`weapon pack: <map> lacks N optional tags
of the pack, first <group> <name>: they are no tag there`). A map's own battle rifle (CE+ X's) is left as the map
made it and the pack's is added beside it (`weapon pack: map has its own battle rifle: left as the map made it
(...)`).

**Custom Edition maps** (and Halo PC's and HaloMD's: every map family but the Xbox's) never get a pack yet:
`weapon pack: refused for <map>: not on Custom Edition maps yet; the game plays without it`. A protected map is
one of them and is refused the same way.

**The fallback.** On a map the pack was not added to, the loadout gives its stock value (an assault rifle for a
battle rifle primary, a pistol for a secondary) and BATTLE RIFLE ON MAP replaces nothing. Every machine says so
at game start, on the console and in the log: `BATTLE RIFLE unavailable on <map>: <reason>`, or `BATTLE RIFLE:
not on Custom Edition maps yet`. The host's PC lobby adds a line to its panel before the game where it already
knows: on a map of another family, with no pack on this machine, or after the pack was refused for that map
earlier in this run (`BATTLE RIFLE unavailable on <map>`).

A refusal is one line, `weapon pack: refused for <map>: <reason>; the game plays without it`, and the game
plays with the stock weapons (a loadout slot that asked for the pack keeps its stock value). When it is added:
`weapon pack: added to <map>: N tags new (B bytes), E the map's own, M optional ones the map lacks, C
corrections; its tag table X to Y; fingerprint <16 hex>`. The **fingerprint** is the first 8 bytes of the
SHA-256 of: the pack's SHA-256, the map's name with its terminator, the map's own tag count (32 bits), and each
pack tag's tag index on the map (32 bits each, in pack order; no tag is 0xFFFFFFFF). Machines that agree on it
have the pack's tags at the same indices.

**Resources at use time.** A bitmap's pixels and a sound's samples are read from the pack's open file when the
texture and sound caches ask for them, by the tag index they ask with, only inside the tag's resource records
(`ae_pack_read_resource`); anything else, or a read the file no longer gives, is zeros and one log line. The
file is hashed once, at start: if it is rewritten while the game runs, later reads give whatever it then holds
(wrong pixels or sound), but never more than the buffer asked for, and nothing of the tags, which are in
memory. The reads are counted in the line logged when the map's tags are let go: `weapon pack: let go from
<map>: N tags; B bitmap and S sound reads (bytes) were served from the pack, F could not be`.

## Building a pack

`build/linux/ae_pack_build` (built by `ninja linux`; tools/ae_pack_build.c with the game's objects of the tag
validator, the schema, the writer and port/linux/game/ae_pack_build.c, a unit no game links) takes a weapon out
of a DONOR map, an Xbox map of the game's build:

    build/linux/ae_pack_build --donor <map> --weapon <tag name> [--root <group>:<name>]...
        --external-names <file> --prefix 'ae\packs\battle_rifle\' --id ae_battle_rifle --revision <n>
        --display "BATTLE RIFLE" --weapon-key battle_rifle --labels h_br,ar [--behaviour 1] [--replaces 2]
        [--texts <file>] [--icon <bitmap>:<sequence>] [--smooth <bitmap>[:coverage]]... [--credits <file>]
        [--verbose] --out <pack>
    build/linux/ae_pack_build --list-names [--out <file>] <map>...

- The donor is read (at most 256 MiB, inflated as the game inflates a map, exactly its header's length) and its
  tags are put where the game reads a map's and checked by the tag validator as a map's are (a mod's map may
  share bytes); a donor the validator refuses is refused.
- From the weapon (a `'weap'` tag of that name, ASCII case aside), then each `--root` in order, every tag they
  lead to is walked through the schema, breadth-first: a tag's root, then its blocks and data, each pointer,
  tag reference and tag index a fixup (the bytes keep no address and no tag index of the donor's: references'
  names are 0 and their indices NONE, tag indices NONE, blocks' and data's addresses 0); a model part's
  vertices and indices (types 4 and 5 only) from the donor's buffers, in offset order; a bitmap's pixels (as
  many as the texture cache reads) and a sound's samples from the donor's file, each range once; a sound
  permutation's two own tag indices. A scenario or structure bsp is refused. The first `'itmc'` root is the
  weapon's item collection.
- A tag whose group and name (ASCII case aside) are in the external names is EXTERNAL: its name only, nothing
  below it walked; required, unless its group is one a map may lack (`'effe'`, `'snd!'`, `'deca'`, `'part'`,
  `'ligh'`: every schema field naming one may be NONE; ae_pack_build.c `optional_external_groups`, held to the
  schema by tools/test_weapon_pack_build.py). Every other tag is OWN, named `--prefix` + its donor name with a
  leading `cmt\` or `ae_pack\` dropped (the source path keeps the donor name). Two tags of a group named alike
  are refused.
- `--smooth` sets an own bitmap's SMOOTH flag (and with `:coverage` METER COVERAGE), by its donor name.
  `--icon` names an own bitmap and a sequence of it that has sprites; anything else is refused.
- The texts file has lines `menu_name=`, `weapon_pickup=`, `ammo_pickup=` (each optional; the menu name is
  `--display`'s without it; blank lines and `#` lines skipped). The weapon entry's power is 0.
- The external names are lines `<group>\t<name>`; `--list-names` writes them for the given maps (every tag of
  each, checked by the validator, one line a group and name, sorted by group then name).
- The pack goes out through `ae_pack_write`; the same donor and arguments give the same bytes. The tool prints
  the counts (with `--verbose` every tag and the validator's corrections of the donor) and exits 0, or prints
  `ae_pack_build: refused: <reason>` and exits 1 with nothing written; bad arguments exit 2.
