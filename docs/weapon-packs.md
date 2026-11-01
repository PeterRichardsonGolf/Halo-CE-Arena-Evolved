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
| 0x10 | flags, u32 | 0, or 1 (`NO_WEAPONS`): a pack of tags only, with no weapon entry (the vehicle damage pack) |
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
  | 0x0C | behaviour, u32 | 0 as its tags say, 1 the AE battle rifle |
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
6. STRINGS ends with a terminator; at least one tag, and at least one weapon unless the header's flags say the
   pack is one of tags only (`NO_WEAPONS`, then no weapon at all); no other flag bit.
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
asks for it: the gametype's `weapon_pack` byte is a MASK of slots (bit n-1 names slot n; the pending game variant,
set by a network game's host and clients and by a split-screen game just before the map loads). The slots
(`port/linux/game/ae_weapon_slots.h`): 1 `packs/ae_battle_rifle.aepack`, 2 `ae_smg`, 3 `ae_smg_silenced`, 4
`ae_carbine`, 5 `ae_ar_silenced`, each file taking only its own pack id. 1, the battle rifle alone, is the byte's
value from before the mask. Every named slot's pack is added, in slot order: each grows the tag table the one before left (its own tags
past the earlier packs', its externals the map's), each is checked and refused alone (one line, the slot's display
name in the fallback line: `SMG unavailable on <map>: <reason>`), and they are let go in reverse. A machine's join
request tells the highest level of packs it has (2 the BR's, 3 and both SMGs', 4 and the carbine's, 5 and the
silenced assault rifle's); a host refuses
a machine below its gametype's level (code 66, AE 0.3.6's text; the BR's alone keeps code 65). A packs folder
without a later slot's file is one line at start, not a refusal. The menus' map and the campaign
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
of the pack, first <group> <name>: they are no tag there`). A map's own battle rifle (CE+ X's, a H2 demake's:
a weapon tag whose name holds `battle_rifle` or `battle rifle`) is REPLACED by the battle rifle's pack (owner
10-24): every reference of the map's tags to it (placements, item collections, starting profiles, the globals'
weapon list, script references) is made the pack's, one line per kind (`weapon pack: the map's own battle rifle
(<name>) replaced by the pack's (<name>): N references (<group> <structure>.<field>)`). The map's tag stays in the
table, unreferenced; the references are put back as the pack is let go.

**Custom Edition maps** (and Halo PC's and HaloMD's) take the packs as an Xbox map does (0.3.9), by the same
steps and checks. Their tags are made the Xbox's as they load (models, shaders, functions, HUD, indexed tags), and
the packs are added once that is done; the build check is not made (their build is Halo PC's). The lines name such
a map by its file, without `@ce`. The Gearbox maps (Timberland and the rest), and custom maps built on the stock
tags (h2_coagulation, h2_foundation, h2_relic, ascetic, gothra_mp, ...), have every stock tag the packs borrow.
Many custom maps do not (CMT's, h2_blood_gulch, h2_warlock, beavercreek_v2, ...: the assault rifle's collision,
melee or first-person animations): `weapon pack: refused for h2_blood_gulch: missing coll weapons\assault
rifle\assault rifle; the game plays without it`, and the map's own weapons are played. A protected map (its tags
renamed) is refused the same way. The vehicle damage pack follows the same path.

**The fallback.** On a map the pack was not added to, the loadout gives its stock value (an assault rifle for a
battle rifle primary, a pistol for a secondary) and BATTLE RIFLE ON MAP replaces nothing. Every machine says so
at game start, on the console and in the log: `BATTLE RIFLE unavailable on <map>: <reason>`. The host's PC lobby
adds a line to its panel before the game where it already knows: with no pack on this machine, or after the pack
was refused for that map earlier in this run (`BATTLE RIFLE unavailable on <map>`).

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

### The vehicle damage pack (a pack of tags only)

`packs/ae_vehicle_damage.aepack` (pack id `ae_vehicle_damage`, header flag `NO_WEAPONS`) holds CMT's TSC:E vehicle
damage effects and wrecks for VEHICLES DESTRUCTIBLE (`port/linux/game/ae_vehicle_pack_rules.h`). It has no slot of
the mask: it is read at start beside the weapon packs (one line, `vehicle pack: ...`; a weapon slot refuses a pack
of tags only), and added after them to a multiplayer map whose scenario places vehicles when the game about to be
played has VEHICLES DESTRUCTIBLE on (the pending variant's flags bit 11), checked and refused alone as a weapon
pack is (`vehicle pack: added to <map>: ...`, `vehicle pack: refused for <map>: ...`). Its tags come after every
weapon pack's, so the weapon packs' tag indices (their fingerprints) are the same on a machine without it. The game
finds its tags by name (`ae\packs\vehicle_damage\` and CMT's name without `cmt\`: `ae_vehicle_pack.c`). Nothing
of it is sent over the network (effects and scenery are each machine's own): a machine without it plays the core's
basic death, the game the same.

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
- A pack of tags only: no `--weapon` (and none of `--weapon-key`, `--labels`, `--texts`, `--icon`,
  `--behaviour`, `--replaces`), one `--root` at least (64 at most): the pack has no weapon entry and its header's
  `NO_WEAPONS` flag.
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

## The recipes (the battle rifle's, the SMGs')

`tools/weapon_pack/make_pack.py` builds `ae_battle_rifle.aepack` from its recipe,
`tools/weapon_pack/battle_rifle/recipe.json`: from CMT's source release (the zip as downloaded) and the player's
own stock Blood Gulch to the donor map and the pack.

    python3 tools/weapon_pack/make_pack.py --recipe tools/weapon_pack/battle_rifle/recipe.json --work <folder>
        --zip <tsce_1-4-1c_source_release.zip> --stock-map <bloodgulch.map>
        [--stock-maps <folder of the 13 stock multiplayer maps>]     (default: the stock map's folder)
        --invader <Invader 0.44.0's build folder> [--library-path <folder of the libraries Invader needs>]
        [--ffmpeg <program>] --builder build/linux/ae_pack_build
        [--reference <folder of the prototype's fix folders>] [--approved-donor <the approved donor map>]
        [--stage prepare|art|build|all]

- **prepare**: the programs, the zip and the map are checked against the recipe; exactly the recipe's allowlist
  (180 tag files) is unpacked from the zip by `extract_allowlist.py` (which refuses an empty list, and anything
  under the release's `util` and `scripts` folders); the stock map's tags are extracted; what the art scripts read
  goes to `art-in/`.
- **art**: our own art is drawn, each file as the recipe's `art_drawings` names it (make_pack.py `DRAWINGS`: a
  drawing, the script that draws it, its inputs and numbers): `screen_art.py` the gun screen's digits and compass,
  `scope_hd.py` and `scope_label.py` the sharp scope and its "[3x]", `counter_art.py` the ammunition counter (a
  `layout` of rounds, rows, groups and sizes; the battle rifle's numbers are its defaults), `reticle_art.py` the
  aiming reticle (a picture beside the recipe, or a `form` drawn from geometry), `hud_sheet.py` the prompts'
  picture of the weapon, from the model's geometry (a `width`; 86 texels the default). These need numpy and Pillow, so a machine without
  them takes `art/` from one that has them (`--stage art` on a folder holding `art-in/`). Each picture is pinned
  in the recipe by its decoded pixels, mode and size (`png_pixels.py`), not by its file: the same picture written
  by another library version or compression is taken, a different pixel is refused. The pixels come out of
  floating-point arithmetic, so another numpy or Pillow may draw one differently; it is then refused, never used.
- **build**: each of the recipe's 94 steps makes one tag of one layer (`os_strip.py`, `multipurpose_to_xbox.py`,
  `dxt_lift.py`, `sound_oggs.py` with ffmpeg and Invader, `hud_hires_to_xbox.py`, `hud_tag.py`, `tag_fields.py`'s
  field edits, Invader's model and bitmap compilers). Invader builds the donor from the layers in the recipe's
  order; a donor whose SHA-256 is not the recipe's stops the run. `ae_pack_build` makes the pack; a donor the
  validator corrected is refused. `audit.tsv` lists every own tag with where it came from and everything its
  steps read: a tag any of whose inputs is a stock tag passes only if the recipe names it under `stock_derived`.
  `externals.tsv` lists every tag left to the map: each must be the stock map's own tag, unchanged, and on all 13
  stock multiplayer maps. `pack_compare.py` then holds the pack to its donor: every tag byte but what the builder
  blanks (its header lists each class), every resource's bytes and every model part's vertices and indices. The
  pack's SHA-256 must be the recipe's recorded one; a pack refused at any point is set aside as `.refused`.

**Recipes without a prototype.** `reference`, `departures` and `approved_reference` belong to a recipe made
against a prototype's folders and donor (the battle rifle's): a recipe has all three or none. The SMG packs'
recipes (`tools/weapon_pack/smg/`, `tools/weapon_pack/smg_silenced/`) have none, so `--reference` and
`--approved-donor` are refused for them; everything else (the allowlist, the pins, the audit, the externals, the
comparison with the donor) holds as for the rifle.

**The SMGs' recipes.** CMT's TSC:E SMG and silenced SMG, each its own pack (`ae_smg`, slot 2, may replace the
assault rifle's spawns; `ae_smg_silenced`, slot 3), built as the rifle's (Open Sauce blocks cut, multipurpose maps
reordered, models converted, the body's base map lifted 1.8, its reflection 0.75 with the warthog's metal cube map,
campaign and grunt cube maps renamed to the stock cyborg's, the held weapon's collision the stock assault rifle's)
and set (15 rounds a second, 60 a magazine, 180 or 160 rounds to start, 180 in reserve at most,
the single-wield error and its minimum, 400 units a second over 40, aim assist 6 degrees / 12 units,
magnetism 12 / 18), 8 damage a round (tuned later by bot grids), no night vision. The silenced one's shots are
silent to the AI's hearing and its bullet has no contrail (no tracer); the SMG's two muzzle fireballs are one at
0.7 of its size. Our art: the counter (60 rounds, 3 rows of 20 in groups of 5), the reticle (the SMG form),
the prompts' picture (60 texels wide; the silenced one 86, at one scale). tools/test_weapon_pack_smg.py holds the
recipes to this.

**The carbine's recipe** (`tools/weapon_pack/carbine/`, `ae_carbine`, slot 4, replaces nothing): CMT's TSC:E
carbine the same way (its base map lifted 1.4 and its purple shell's reflection 0.6: the lighter shell otherwise
shows mint green in the sun), held by `c_cb` or `ar`, with these numbers: a magazine of 18, 36 rounds to start
and 72 in reserve at most, one round a pull at most every 0.35 s (the barrel names 2 shots per fire, a burst
its carbine never fires in play: single shots kept), error 0.3 to 0.6 degrees, one 2x zoom, aim assist 3 degrees /
17 units and magnetism 6 / 21, 400 units a second over 40, 14.3 a round (11 for CE's 150, the head bonus
kept). The scope is CMT's; the aiming reticle is ours (the carbine form), the counter 18 rounds in groups of 3.
tools/test_weapon_pack_carbine.py holds the recipe to this.

**The silenced assault rifle's recipe** (`tools/weapon_pack/ar_silenced/`, `ae_ar_silenced`, slot 5, replaces
nothing; layout alias `ar_silenced` or `silenced_ar`, the assault rifle where the pack is not) takes its source from
a MAP, not a zip: the "ma5s" of the Halo 2 Mappack Collection v2's Gemini (`h2d_gemini.map`, the collection's
`boardingaction.map`; credited to its makers). Its `source` has `"kind": "map"`, the map's file name, SHA-256 and
tag count, and the allowlist of the tags taken from it; `make_pack.py --source-map <the map>` in place of `--zip`.
A step reads the source as `"source"` (the first recipes' `"cmt"` is the same: the source's tags). In prepare the
map is checked against its SHA-256, written whole (`donor_map.whole_length_map`: Gemini's stream inflates 888
bytes short of its header's length, all past its tags, which the game takes; the reader takes it only when what
is missing lies past the tags) and deflated again, as Invader reads an Xbox map, and exactly the allowlist's tags
are extracted from it by Invader. The allowlist is the ma5s's own 8 tags and the stock assault rifle's bolt shader
in the group the map has it in; everything else the weapon names (the assault rifle's animations, HUD, bullet,
casing, impacts, melee, collision) is the stock map's own and left to the map. The weapon is the collection's (the
stock assault rifle's numbers, silent to the AI's hearing, one 2x zoom) but for one fix: its firing effect made the
casing at the marker `primary trigger`, the silencer's tip, and now makes it at `primary ejection`, as the stock
assault rifle's does (the brass left the barrel). No art of ours, no icon: its HUD is the stock assault rifle's.
tools/test_weapon_pack_ar_silenced.py holds the recipe to this.

**The vehicle damage pack's recipe** (`tools/weapon_pack/vehicle_damage/`, `ae_vehicle_damage`, a pack of tags
only): its `pack` names `roots` (`effe:` and `scen:` names) and no weapon, and has no art. The donor is Blood Gulch's
scenario without vehicles, the two wrecks (sceneries) and two holders in its scenery palette: the holders are stock
sceneries whose attachments (8 at most) are the pack's effects, in the donor only (no root leads to them). CMT's
warthog, ghost and banshee damage effects (the damage, its loop, the glass, the hood and the wings coming off, the
explosions) and the warthog's and the banshee's burning wrecks, with four changes the recipe names: Open Sauce's
blocks are cut from particle systems' particle states too (`os_strip.py particle_system`, which walks the stock
kit's layout and refuses a file that does not end where it does); the parts that name campaign tags no
multiplayer map has are left out (underwater bubbles, panel debris and its impact, the banshee wreck's
damaged-engine sound, the warthog wreck's lights) and so is the warthog's thrown wheel (a vehicle, which a network
game would send to machines without the pack); the death explosions deal no damage of their own (the map's
explosion, the core's, deals it, with the pack or without); the effects that play on the map's own vehicles have
their places moved from CMT's markers to ones the stock models have (the wreck's model has CMT's own). The
warthog wreck's smoke is scaled by nothing (a wreck carried by the dead vehicle runs no functions of its own).
`tools/test_weapon_pack_vehicle_damage.py` holds the recipe to this.

**The approved donor and the departures from it.** The rifle the owner approved was a prototype's donor map
("v6e", SHA-256 0aa3ffcb..., `approved_reference` in the recipe). The recipe reproduces that donor's 96 fix tags
byte for byte except six it names under `departures`, which exist so that the pack carries no pixel of the game's
(with `--reference`, `fixes.tsv` compares every made tag with the prototype's and the run stops on any difference
that is not named, and on a named one that is not a difference). With `--approved-donor` the finished pack is
compared with the approved donor itself and may differ only in three tags (`departures.tsv`): the prompts' icon
sheet (our own picture alone; the prototype's was a stock HUD sheet with the picture drawn in), one reference of
a shader on the rifle's model (the stock cyborg's mirror cube map by its own name, which every stock multiplayer
map has, in place of a copy of it), and one number of the ammunition HUD. One own tag is still made from a stock
tag: the ammunition HUD interface, which is layout numbers in the stock assault rifle HUD's structure and no art.

Pinned by SHA-256: the zip, the stock map, Invader's programs, the art scripts' inputs, the donor, the pack. Not
pinned: ffmpeg (its version is logged; other samples would change the donor's hash), Python, numpy and Pillow
(the art's pixel pins catch them), `ae_pack_build` (built from the same commit).

Nothing of CMT's, of the game's or made from them is in the repository: the work folder holds all of it.
`make_pack.py` refuses a work folder under `/tmp`, and one that holds files it did not make. Invader (GPL-3) and
ffmpeg are separate programs it runs.
