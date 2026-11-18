# Weapon packs: file format v1

> Not in 0.7.6: comes in 0.7.7. (This format page describes pulled content too: the tactical SMG, burst PDW, heavy revolver, focus beam, spiker, ravager, auto shotgun, grenade launcher, concussion rifle and gravity hammer recipes are held for 0.7.7.)

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
| 0x10 | flags, u32 | bit 0 (`NO_WEAPONS`): a pack of tags only, with no weapon entry (the vehicle damage pack); bit 1 (`DIRECT_PORT`, 0.7.5): a direct port of another game's asset, which the game loads only where it finds the player's MCC install (a community-made pack leaves it clear; builds before 0.7.5 refuse the bit) |
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
`ae_carbine`, 5 `ae_ar_silenced`, 6 `ae_dmr`, 7 `ae_evo_rifle`, 8 `ae_needler` (the mask's bit 7: it replaces the map's own needler weapon, every reference to it, as the battle rifle pack does a map's own battle rifle), each file taking only its own pack id. Slots 9-16 (0.7.5) are a second byte, the gametype's PC options' `weapon_pack_high` (taken from their reserved bytes, which older builds zero and ignore: they read the first byte alone): 9 `ae_sniper_bolt`, 10 `ae_magnum_scoped`, 11 `ae_brute_shot`, 12 `ae_ar_modern`, 13 `ae_sidearm`, 14 `ae_energy_sword`, 15-16 free. A loadout slot starts with a pack by its rank in the variant's fields (the first byte's slots, ranks 1-7) or, for any other named pack, by the options' `pack_start` byte (its slot number; the variant's fields then say stock, which older builds play). 1, the battle rifle alone, is the byte's
value from before the mask. Every named slot's pack is added, in slot order: each grows the tag table the one before left (its own tags
past the earlier packs', its externals the map's), each is checked and refused alone (one line, the slot's display
name in the fallback line: `SMG unavailable on <map>: <reason>`), and they are let go in reverse. A machine's join
request tells the highest level of packs it has (2 the BR's, 3 and both SMGs', 4 and the carbine's, 5 and the
silenced assault rifle's, 6 and the DMR's, 7 and the EVO RIFLE's, 8 and the needler's, 9 to 12 and the second byte's
packs, one level a slot, 13 the sidearm's and 14 the energy sword's; the join mark holds levels up to 15); a host refuses
a machine below its gametype's level (code 66, AE 0.3.6's text; the BR's alone keeps code 65). A game that names no slot
past 8 needs level 8 at most, so builds before 0.7.5 join it as before; one that names slot 9 or more refuses them. A packs folder
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
and, for the silenced one, the prompts' picture (86 texels wide); the SMG's prompts' picture is CMT's own, sequence
14 of CMT's pickup icon sheet (hudv4.1). tools/test_weapon_pack_smg.py holds the recipes to this.

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

**The bolt-action sniper's recipe** (`tools/weapon_pack/sniper_bolt/`, `ae_sniper_bolt`, slot 9, the second mask
byte's first; layout alias `sniper_bolt` or `bolt_sniper`, a sniper rifle where the pack is not) takes its source from a MAP, as the silenced assault
rifle's does: the sniper rifle of the CE+ X modded disc (VKMT's CE+ team; `dw2\weapons\sniper_rifle\e3_2000\sniper
rifle`, on every CE+ X multiplayer map), from its Blood Gulch. Its 23 own tags are taken as the disc has them (the
weapon, CE+'s bolt-action first-person model and animations, its trigger and HUD, and the third-person model with its
shaders and bitmaps from Digsite): no step changes any of them. Its 29 other tags (the stock sniper rifle's bullet,
damage, firing effect, melee, sounds, ammunition and scope pictures) are the stock map's own, left to the map: CE+ X's
copies of those carry the same samples and numbers (CE+ X changes only some sounds' audible distances). Held by the shotgun's label (`sg`), as on the disc. The game
corrects six HUD elements of CE+'s that name no bitmap (not drawn), as it does on CE+ X's own maps.
tools/test_weapon_pack_sniper_bolt.py holds the recipe to this.

**The HJ magnum's recipe** (`tools/weapon_pack/magnum_scoped/`, `ae_magnum_scoped`, slot 10; alias
`magnum_scoped` or `scoped_magnum`, a pistol where the pack is not) takes its source from the ZTeam tag pack (the zip as released; its `root` empty, the tags under `zteam/`):
ZTeam's black slide magnum (`magnum/h3o`) as an Xbox weapon. ZTeam's own tags are its models (converted), shaders,
bitmaps (the multipurpose maps reordered), first-person and third-person animations, collision and sounds (Xbox ADPCM
already: taken as they are); its flashlight (a light and a light volume) is left out. Everything the stock pistol has
is the map's (HUD with its zoom, melee, materials, impacts, ammunition pickup, empty click, kick; the 2x scope picture is ZTeam's round window drawn by hj_scope.py as a 2048 x 512 picture at one texel a HUD unit with a solid black surround, round and edge to edge at any aspect up to 4.2:1), and the round
(projectile and damage effect) and the firing effect are the stock pistol's, made again under the pack's names with
our numbers and ZTeam's shot (named under `stock_derived`). The weapon's numbers are ours: 12 a magazine, 48 to start
and in reserve, 2.4 rounds a second, the first round exact and the error growing to 1.25 degrees, one 2x zoom (the stock pistol HUD's scope reads 2x), 24 a
round at x1.6 on a player (two hits strip a full shield, the third to the head kills, four to the body), the round
reaching 120 units. Held by the pistol's label (`hp`), or the assault rifle's (`ar`) where a seat of the map's player lacks it (the stock player's alert seat is left out of that check, ae_weapon_seats_rules.h, so `hp` holds on the stock maps). tools/test_weapon_pack_magnum_scoped.py holds the recipe to this.

**The brute shot's recipe** (`tools/weapon_pack/brute_shot/`, `ae_brute_shot`, slot 11; alias `brute_shot`, a rocket
launcher where the pack is not) takes CMT's multiplayer brute shot (`brute_shot_mp`) from CMT's 1.4 stock-compatible
source release (no Open Sauce blocks to cut) as CMT made it: its models converted, a multipurpose map reordered, its
high-resolution HUD made the Xbox's; the explosion's underwater flash, whose bitmap no stock multiplayer map has, is
left out. tools/test_weapon_pack_brute_shot.py holds the recipe to this.

**The combat assault rifle's recipe** (`tools/weapon_pack/ar_modern/`, `ae_ar_modern`, slot 12; alias `ar_modern` or
`combat_ar`, an assault rifle where the pack is not) takes CMT's individual combat weapon system rifle from the same
release, with its own ammunition counter (36 rounds), reticle and one light zoom (1.4x), at AE's own numbers.
tools/test_weapon_pack_ar_modern.py holds the recipe to this.

**The energy sword's recipe** (`tools/weapon_pack/energy_sword/`, `ae_energy_sword`, slot 14; alias `energy_sword` or
`sword`, no stock weapon where the pack is not: it is a melee weapon) has two authors' work. The sword as other
players see it and as it lies on the ground is CMT's `energy_sword` (the evolved set's; the release has no `_mp` one) from
CMT's 1.4 stock-compatible source release, as CMT made it: the model converted, the pickup message index the stock one's,
the two glow widgets (whose sprite no Xbox map has) and the detonation's boom (a sound no Xbox map has) left out, a rumble
on its melee blow, and two flags cleared: the blade shaders' "draw before water" (the first-person view draws them, and the
transparent pass asserts on that) and the weapon's "detonates when dropped" (the game counts a weapon put away on a switch
as dropped: the detonation timer deleted the sword 13 ticks after a switch to another weapon). CMT's sword is the one its
characters carry: no first-person model, animations, HUD or triggers (a pack's weapon must have a first-person model and
animations that fit the map's hands, or the game refuses it). The first person is CMT's mesh on ZTeam's rig: the
`model_rig` step (`jms_rig.py`) recovers CMT's model as JMS, moves it uniformly (x1.0185, so its grip centre and blade tip
sit where ZTeam's first-person sword has them) and gives its two nodes ZTeam's frames ("frame handle", "frame blades" 8
units out along the blade), and compiles it again; ZTeam's first-person animations (idle, ready, moving, posing, melee,
lunge, lunge in the air, grenade throws), its melee and posing sounds, its HUD with the sword's reticle and its picture of
the sword for the prompts are taken as they are from ZTeam's tag pack (an extra source: `--extra-zip zteam=<zip>`, each
file copied by `copy_input.py`, a derived file pinned by its bytes); the picture goes through `source_icon.py` as the other
packs' ZTeam pictures do. The animations' table is ZTeam's: the primary fire message plays the ground lunge, the melee
message the bash; the table's secondary fire slot is made the air lunge, and the lunge sends that message when the swinger is
airborne. Its pack behaviour is 0 (the EVO RIFLE is 2); the swing and the lunge are the game's, not the pack's:
`port/linux/game/ae_sword_lunge.c` finds the sword by its slot (`AE_WEAPON_SLOT_ENERGY_SWORD`), turns its right trigger
into the melee button, and a swing with an enemy in reach (2.5 units, the reticle red) carries the player to him (7 units
a second, 6 to 10 ticks, the blow of the ZTeam lunge animation's key frame at tick 7) and lands one blow (200) on arrival.
The bots fight with it as brawlers do. tools/test_weapon_pack_energy_sword.py and tools/test_jms_rig.py hold the recipe and
the rig to this.

**The spiker's recipe** (`tools/weapon_pack/spiker/`, `ae_spiker`, slot 19 once registered; key `spiker`, held by its
own label or the assault rifle's) takes CMT's multiplayer spiker (`spiker_mp`) from CMT's 1.4 stock-compatible source
release: its three models converted, two multipurpose maps reordered, its high-resolution HUD made the Xbox's, one
Ogg Vorbis sound re-encoded and one shader's detail map (a campaign bitmap no multiplayer map has) left empty; nothing
else of CMT's tags changed but the numbers, which are AE's own: a close-to-mid automatic. CMT's 10 spikes a second and
magazine of 40, 80 in reserve to start and 160 at most; 10 a spike at x0.85 on a player's shield and x1.3 on the body
(9 spikes strip, 15 kill from full: 1.4 s); the spike 50 units a second slowing to 30 with CMT's arc, carried 60
units; error 0.6 to 2.4 degrees (at least 0.4); aim assist 3.5 degrees / 15 units and magnetism 8 / 20; a short pad
pulse with each spike. The held weapon's collision and melee are the stock assault rifle's; the prompts' picture is
CMT's own pickup icon sheet, sequence 11. tools/test_weapon_pack_spiker.py holds the recipe to this.

**The ravager's recipe** (`tools/weapon_pack/ravager/`, `ae_ravager`, slot 20 once registered; key `ravager`, held by
its own label or the assault rifle's) takes CMT's prototype ravager from the same release and sets it up as the
latest-generation ravager is, tuned for CE. It is CMT's spiker model, so it has its own look: `ravager_art.py` recolours
CMT's spiker body and clip pictures by numbers (one look of its `LOOKS`, named by the recipe's drawings: B3bd, crimson
panels, near-black steel trim, a tempered-steel blade, white-cyan glowing seams and panel lines through the body's
multipurpose self-illumination, cyan lights), and the ravager's own copies of the four shaders (self-illumination cyan)
and the two models (`ae_pack/ravager/`) name those pictures; a new look is a change of its numbers there and a re-pin.
Its numbers: three bolts a pull one after another, 4 ticks apart (the source's 3-round burst at 7.5 a second: CE has no
burst trigger, so the engine paces it: pack behaviour 5, the RAVAGER profile in port/linux/game/ae_rifle_profiles.c,
played by ae_burst_rifle.c; the tags' trigger fires one bolt a shot, a tap's first as the charging trigger is released,
and the engine fires the other two, then a 12-tick recovery: taps 23 ticks (0.77 s) apart at the quickest, 0.5 s from a
burst's last bolt to the next's first; the bolts, heat, battery, zoom, aim assist, kick and charged shot stay the
tags', and it is no rifle: no rifle kill feed name, medal role, shield finisher, bots' rifle play or HUD hint), no bloom; each bolt a 45 unit/s lob (gravity x5.2) that bursts on whatever it hits: 16 on the hit (x1
on a player's shield, x0.8 on the body, x2 on the head once the shield is down) and a splash of 13 to 2.6 over 0.3 to
0.95 units (x0.5 on shields), so 4 bolts strip a full shield and 7 kill (the third pull, with CE's spill). Hold the
trigger 0.75 s to charge (the second trigger, as the stock plasma pistol's; CMT's devourer charge glow) and release for
one charged bolt: 32 on the hit and 48 to 10.7 over 0.6 to 2.5 units, then nine burning pulses 0.25 s apart where it
burst (CMT's devourer burn effect; 6 to 2 over 1.5 units, x0.25 on shields). Heat 0.38 a pull (0.1267 a bolt) and 0.5 a charged
bolt against 0.33 lost a second (7 pulls back to back to overheat, the heat bleeding over a burst's 8 ticks; a 3 s
vent to 0.015), a battery of 36 bolts, no misfire, a 1.4x zoom,
aim assist 4.5 degrees / 20 units and magnetism 6.75 / 24, a hard pad pulse a bolt and a heavier one a charged bolt.
Its HUD's weapon picture and the prompts' picture are CMT's spiker picture (sequence 11), where CMT's prototype
borrowed another weapon's. tools/test_weapon_pack_ravager.py holds the recipe to this.
**The focus beam's recipe** (`tools/weapon_pack/focus_beam/`, `ae_focus_beam`, slot 18 once registered; key
`focus_beam`, held by its own label or the assault rifle's) takes CMT's prototype focus beam from CMT's 1.4
stock-compatible source release: its two models converted, a multipurpose map reordered, its high-resolution HUD
(heat meter, reticle, scope) made the Xbox's and its seven Ogg Vorbis sounds re-encoded, nothing else of CMT's tags
changed but the numbers, which are AE's own: a heat beam that strips shields and needs a finisher. CMT's 4 a round at
x1.1 on a player's shield and x0.2 on the body; 20 rounds a second rising to 30 over 0.3 s, the spread closing from
1.5 degrees to none; heat 0.034 a round against 0.5 lost a second (about 2 s of beam, 60 rounds, to overheat, then
1.6 s locked until the heat falls to 0.2); a battery of 333 rounds; 30 units of reach; one 2x zoom; aim assist 1.5
degrees / 30 units and magnetism 4 / 30 at every zoom; half the stock plasma bolt's slow for 0.15 s. The beam's two
contrails are narrower than CMT's (core 0.12 units, glow 0.08 to 0.14: at CMT's width thirty beams a second washed a
shot player's view). A full shield goes
in 18 rounds (about 0.67 s from the pull); a whole heat leaves a bare player about 41 of 75 for a melee blow, a burst
or a few pistol rounds. The firing rumble is CMT's brute shot trigger made a pad rumble only (no flash, kick or shake);
the held weapon's collision and melee are the stock assault rifle's; the prompts' picture is CMT's own pickup icon
sheet, sequence 3. tools/test_weapon_pack_focus_beam.py holds the recipe to this.

**The join levels** (`AE_WEAPON_SLOTS_CAPABILITY_*`, ae_weapon_slots.h) are cumulative: a host refuses a machine below the
highest level its game names, so a pack's level is the number of packs before it that a machine must have. The numbers
(13 for the sidearm, 14 for the sword) follow the order the packs are built in; the final numbers follow the order the packs
ship in, and the release session decides them. A pack shipped before a lower-level one would lock every joiner out of
games that name the lower one.

**The sidearm's recipe** (`tools/weapon_pack/sidearm/`, `ae_sidearm`, slot 13; alias `sidearm`, the pistol where the
pack is not; the weapon's name in the game is SIDEARM PISTOL) takes ZTeam's h2 magnum from the ZTeam tag pack (the zip as
downloaded) with its trigger guard cut off: the `model_cut` step has Invader recover each model's JMS files, `jms_cut.py`
drops every triangle bound to node 0 inside a box (x 0.5..3.9, y -0.25..0.25, z -1.15..2.6, in JMS units; all three
levels of detail of the third-person model, the first-person model's one), Invader compiles them again and the result
is converted as the `model` step does. Invader binds a shader by searching the tags for its name, so the allowlist holds
the h2 folder's shaders only (and the first-person model's h1 bullet) and the step names the shaders the compiled model
must bind, or the run stops. The textures are ZTeam's own, made by `sidearm_art.py`: the body picture desaturated, its
5th, 50th and 95th percentile brightness moved to 0, 7 and 22 and what is above squeezed to end at 24, the rear sight's
face (our mask `sidearm/face_mask.png`) filled with 7 and #66FF14 blended in by our mask `sidearm/glow_mask.png` (the
front post and two rear dots); the multipurpose map (Halo PC order: green = the glow mask, blue = half the specular, none
on the face) goes through the usual Xbox reorder; the small body picture gets the same remap. The metal shader's
brightness is 0.5 / 0.25 (perpendicular / parallel) and the small shader's parallel brightness 0.25. The numbers: 12
rounds a magazine (48 to start and in reserve), 5.6 rounds a second, a bloom wider than the stock pistol's (0.3 to 3
degrees over 0.35 s of fire, back over 0.25 s), one light 1.4x zoom with no scope picture, a round of 26 at x0.54 on a
player's shield and x0.6 on the body: six hits strip a full shield, eleven kill from full, the seventh to the head kills.
The round, firing effect and kick are the stock pistol's; the HUD is ZTeam's h2 magnum one, its rounds panel the map's,
its two aiming reticles (unzoomed, zoomed) CMT's pistol HUD's, grafted from it (an extra zip, `--extra-zip tsce=<tsce_1-4_source_release_stock.zip>`;
`copy_input.py` puts CMT's HUD and reticle sheet in a layer and the `hud_hires` step makes that HUD the Xbox's); the prompts'
picture is ZTeam's own (`source_icon.py`). The pack is held by the pistol's label `hp`: the stock player's alert seat has
the rifle and shotgun labels only, so the "every seat" check refused `hp` and a pistol was held by the rifle's two-handed
`ar`; the check now leaves the alert seat out for every pack (`ae_weapon_seats_rules.c`: a stance only actors take), so
each pack's own label wins (the pistols `hp`, the rifles `ar`). tools/test_weapon_pack_sidearm.py and tools/test_jms_cut.py hold
it to this.

**The grenade launcher's recipe** (`tools/weapon_pack/grenade_launcher/`, `ae_grenade_launcher`, slot 22 once the
slot table names it; the weapon's name in the game is GRENADE LAUNCHER) takes ZTeam's grenade launcher from the ZTeam
tag pack (grenade_launcher/hr: world and first-person models, the grenade's model, textures and the gun's screen,
collision, effects, sounds; the first-person animations and ready sound of its hr_cad folder, which ZTeam's weapon
names) with the HUD ZTeam gave it (its brute shot HUD: six-grenade meter and reticle), its rounds panel the map's,
made again under the pack's own name (`grenade_launcher\hud\grenade_launcher`: ZTeam's leftover test path is another
pack's too, with other content).
ZTeam's second trigger (a charged EMP grenade) is left out and the first trigger's charge with it, so a press fires.
The trigger's "tracks fired projectile" flag is set: **hold the trigger to keep the grenade from bursting, let go to
burst it** (`port/linux/game/ae_tracked_grenade.c`, below). ZTeam's grenade has one point of body and blows up when it
is gone, and each bounce dealt a 3-unit damage that set it off: the bounce's damage is left out and the grenade has no
collision model (a null reference: no stock projectile is something others hit, and the 0.7.6 preflight crashed once
in a projectile's hit on an object gone that tick), so the grenade bursts on its fuse (0.8 s once it comes to rest: the game starts an
after-the-first-bounce timer only at rest), at once on a player (ZTeam's material responses), or when let go. The
explosion's fire flash is drawn simple (ZTeam's type asks for rotational sprites its bitmap does not have, which the
game's tag check would turn off). The gun screen's icon sheet is ZTeam's with the identifier painted beside the
arming arrows cleared (`screen_blank.py`, the `screen_blank` drawing: rectangles of a source picture made transparent
black, pinned by pixels; compiled again as DXT3). The numbers are the role's (a latest-generation bouncing launcher)
made AE's for its 75 + 75 player: 6 a magazine, 12 to start, 24 at most, 1.1 a second, 20 world units a second at
1.3 times the gravity, a burst of 85 at full within 0.6 units and none past 1.6 (one strips a full shield, two close
ones kill), aim assist 3 degrees to 13 units and magnetism 7 to 30; materials and melee the stock assault rifle's; a
rumble on ZTeam's firing kick, edited under the pack's own name (`grenade_launcher\hr\effects\trigger`: ZTeam's
kick is a file its other weapons share). Held by `gl`, else the rifle's `ar`. **No picture**: no licensed picture of a grenade
launcher exists, so the pack names no message icon and its HUD's messaging icon is none (`sequence_index` -1): the
weapon's name is written instead (below). tools/test_weapon_pack_grenade_launcher.py holds it to this.

**A tracking trigger** (0.7.6, `ae_tracked_grenade.c`, rules `ae_tracked_grenade_rules.c`): a pack's own weapon whose
trigger has "tracks fired projectile" keeps the shot's first projectile as the weapon's tracked object (the game's
trigger state machine has a tracking state, but nothing ever recorded the object, and it reads the weapon's busy time
after a shot as the trigger let go: the projectile's own update reads the weapon's trigger from its control instead,
a hook in projectiles.c). While the trigger is held the projectile's fuse does not run (10 s at most); letting go after
at least 0.2 s sets it off where it is (its update's detonate action, as its fuse would); a quicker tap leaves it to
its fuse. A
stock or map weapon with the flag keeps the game's way. Every machine runs every player's weapon from the inputs the
host relays, so the release reaches each machine with the player's trigger and each sets off its own copy; the damage
is any projectile's (the firing player's machine's report, checked by the host). No network message of its own. A new
map forgets every tracked grenade (`ae_tracked_grenade_new_map`, weapons_initialize_for_new_map), and a row is used
only while its handles still name a projectile and a weapon of the tags recorded at the shot. The
first releases of a map are logged (`ae tracked grenade: player P let go after N ticks: set off at x y z, in the air, no bounce yet, S wu/s`).

**A pack weapon with no picture** (0.7.6, `ae_pack_names.c`, rules `ae_pack_names_rules.c`): a pack weapon whose own
HUD names no messaging icon (the recipe sets its `sequence_index` to -1 and the `pack` names no `icon`) shows its menu
name as text where a picture would be: HUD STYLE AE's swap and pickup prompt writes the name (as for every weapon),
the game's own prompt writes its text when the HUD has no icon, the kill feed writes the name in place of the
weapon's picture (ae_killfeed.c), and the HUD's weapon picture, its first always-shown overlay item (its overlay
elements in order), is drawn as the name in its place; every other overlay element and item (a second picture, a
reload or empty warning) is drawn as the game draws it (hooks in hud_weapon.c around each element and in hud_draw.c
per item). No pack format change. `pack_done.py` passes such a weapon's point 3 on the recipe's opt-in (no
`icon`, a HUD step setting `sequence_index` -1) and wants the game's load line to say `message icon none` and
`its HUD's icon sequence -1`.
**The burst PDW's recipe** (`tools/weapon_pack/pdw/`, `ae_pdw`, slot 16 once registered; weapon key `pdw`) takes ZTeam's
compact submachine gun from the ZTeam tag pack (`smg/h3`'s models, converted, bound to `smg/h3b`'s two shaders: the same
shaders but for their base map, ZTeam's h3b texture) with its first-person animations, collision, sounds and HUD (its
meter on ZTeam's own 36-round sequence of the same sheet, its rounds panel the map's, its aiming reticle CMT's SMG one,
grafted as the HJ magnum's; `--extra-zip tsce=<tsce_1-4_source_release_stock.zip>`). The round, firing effect and kick are
the stock assault rifle's at our numbers. The burst is the tags' own (pack behaviour 0): the latched trigger charges for
a frame and spews 4 ticks (3 rounds a tick apart, the first 3 ticks after the pull); a second, idle trigger keeps the
engine from firing a round before the charge; the next pull counts 8 ticks after a burst (14 ticks pull to pull). 20 a
round up close and 14 once the round has slowed (400 to 240 units a second over the SMG round's 2-unit span), x1.0 on a
Spartan: 3 bursts kill a full shield close, 4 at mid range, each with a round to spare; 36 a magazine, 144 to start and
in reserve, error 0.4 to 1.6 degrees, no zoom. Held by `h_smg`, or `ar`. tools/test_weapon_pack_pdw.py holds the recipe
to this.
**The heavy revolver's recipe** (`tools/weapon_pack/revolver/`, `ae_revolver`, slot 17 once the slot table names it;
the weapon's name in the game is HEAVY REVOLVER) takes ZTeam's heavy revolver from the ZTeam tag pack as its h3b models
(world and first-person), which ZTeam left untextured (one grey placeholder shader), on ZTeam's h3 version of the same
gun: its weapon tag, animations, collision, sounds, HUD and black metal shader (both models have the same four nodes, so
the h3 animations drive them). The h3b models' texture coordinates are a flat projection of the gun's side, so ZTeam's
h3 textures land off their panels there; the art is made again for them, nothing painted by hand. `revolver_texels.py`
(a derived file, run under `--derive-python`: it reads the second-version model with the Reclaimer library) writes where each of the base
map's 512 x 512 texels lands on the model (x, y, z and node; the side-facing face wins where faces stack); the
`revolver_diffuse` and `revolver_multi` drawings (`revolver_art.py`, drawn once that file is made) read it with ZTeam's
h3 base and multipurpose maps: ZTeam's lights (its self-illumination) are cleared where they fall and three are carried
to their points on the gun (the two slide dots, the rear II mark, the trigger dash); the drum and the receiver's side
panel take a dark red and the rest a dark metal, each a three-stop ramp of the texel's own brightness; the lights
are an energy red; the specular mask's contrast is lifted. The colours are numbers in the script (a sampled palette; no other source's texel is read). The
multipurpose picture goes through the usual Xbox reorder, and each gbxmodel is edited to bind the revolver's shader
(ZTeam's black metal on the two pictures, parallel brightness 1, its reflection tinted toward red) before the `model`
step converts it. The numbers are a latest-generation heavy revolver's (the role as it is, then the damage made AE's
for its 75 + 75 player): one slug a shot
(ZTeam's fires 16 pellets) of 29 at x1.6 on a player's shield and body (46.4: two strip a shield, the third to the head
kills, four to the body; under no shields one to the head kills, two to the body), 1.67 slugs a second, 8 a drum,
24 to start and 40 at most (32 in reserve), error 0.15 to 2 degrees over 0.3 s of fire and back in 0.2 s, aim assist
3 degrees to 15 units and magnetism 10 degrees to 25, a slow, falling slug (75 world units a second down to
45, gravity x2.1, gone at 150), and a heavy blow (76 at x1.4, 106.4: a slug and a blow kill from full, a blow alone
does not). The slug, firing effect (with ZTeam's shot), kick and melee are the stock pistol's with those numbers; the
HUD is ZTeam's (its round reticle, the reticle's height scale set to its width's: ZTeam's drew a tall oval; its
eight-shell meter, sequence 7 of ZTeam's meter and alpha sheets with the fill numbers ZTeam's h3 magnum HUD gives it,
in place of the five-shell one), its rounds panel the map's; the prompts' picture is ZTeam's
own picture of the revolver, the sixth on its HUD picture sheet (`source_icon.py`'s "bitmap": 5). Held by the pistol's
label `hp`. The first-person animations are ZTeam's with the whole view model set back (`revolver_fp.py`, a derived
file under `--derive-python`: the root node `frame bone24`, which the arms and the gun hang from, moved 0.045 forward,
0.025 right and 0.035 down in every base animation, in its static value or each frame; overlays left as they are), so at
the default field of view the gun sits where the HJ magnum does, not big in the middle of the screen.
tools/test_weapon_pack_revolver.py holds it to this.
**The auto-shotgun's recipe** (`tools/weapon_pack/auto_shotgun/`, `ae_auto_shotgun`, slot 21 once registered; weapon
key `auto_shotgun`, pack behaviour 0) takes ZTeam's modern combat shotgun from the ZTeam tag pack (`shotgun/h3_hunter`: its
one model, converted and worn under a third-person and a first-person tag, shaders, bitmaps and HUD; `shotgun/h3`'s
collision and reload and melee sounds, `shotgun/h2`'s shot). Its first-person animations are a derived file
(`auto_shotgun_anims.py`, run by the art stage with `--derive-python`, pinned by SHA-256): ZTeam's graph made
semi-automatic. A shot never works the forend (ZTeam's recoil 1.5 times as fast, the loading port's cycle and the spent
shell moved into it: 14 frames); the draw is ZTeam's, one rack with its sound, as a semi-automatic is charged; the
posing slot plays ZTeam's posing that leaves the forend alone; the reload's enter and shell-by-shell loop are ZTeam's
about a fifth faster (15 and 13 frames: the engine times each shell by them, so the first takes 0.5 s and each next
0.43 s, 2.67 s for all 6; the tag's reload time is set to the loop's 0.433 to agree), and both exit slots play ZTeam's
exit with its single forend stroke. The engine plays one exit for each reload that
finishes (exit-empty when the tube was empty at the start, exit-full otherwise: `first_person_weapons.c`), so the action
is worked exactly once per reload; a reload broken off by a shot plays none. The stroke sounds of the shot and the posing
leave the graph, and the weapon has no third-person animation graph. The pellet, firing effect and kick are the stock
shotgun's at our numbers: 12 pellets of 7.2 at full speed and 2.9 once slowed (750 to 90 units a second within the first
tick, over an 8-unit air damage span: full damage inside about 4 units, the lower bound past about 9.3, gone at 15), x1.0
on a Spartan: 2 shots kill a full player up close and 1 never does, 3 strip and 5 kill at range; 1.75 shots a second, 5
degrees of spread, no bloom, 6 shells loaded one at a time, 12 to start and 24 in reserve. Held by `sg`, or `ar`.
tools/test_weapon_pack_auto_shotgun.py holds the recipe to this.
**The concussion rifle's recipe** (`tools/weapon_pack/concussion_rifle/`, `ae_concussion_rifle`, slot 23 once the
slot table names it; the weapon's name in the game is CONCUSSION RIFLE) takes ZTeam's arc launcher (`concussion_rifle/hr`)
from the ZTeam tag pack (the zip as downloaded): its weapon tag, models (converted), shaders, bitmaps (two multipurpose
maps reordered: the rifle's and the plasma grenade one its bolt wears), first-person animations and their sounds (from
the `hr_cad` folder: the `hr` folder has none), collision, its explosive bolt with every effect, particle, decal, light
and sound of its flight and blast, its firing effect and kick, and the launcher HUD ZTeam gave it. Three things are
fixed: the blast's fire cloud drawn as plain sprites (ZTeam's rotational sprites take each state's next sequence, which
its bitmaps lack; the game would turn the cloud off, and a corrected donor makes no pack), the HUD's rounds panel the
map's stock one (ZTeam's chain ends in a full-screen visor frame) and its picture of a brute shot left out. ZTeam's HUD
and firing kick are test files its weapons share (`hud/temp/weapon_interfaces/test/h3_brute_shot`,
`trigger_rifle_kickback_strong`): the pack makes its edited copies under its own names (`concussion_rifle/hr/hud/`,
`concussion_rifle/hr/effects/trigger`), so another pack's copy of the same file meets none of them in a merged cache. The HUD's
reticle is ZTeam's launcher one with a drop ladder (sequence 17 of its sheet) at an even scale; its six-shell meter is
kept as ZTeam set it (it did not draw in the box's capture; the stock rounds panel shows the six loaded bolts). No licensed picture of this weapon exists (ZTeam's pack has none): the pack names no icon and its HUD no
messaging icon, so its name is written where a picture would be, in the prompts, the kill feed and its HUD (the grenade
launcher's opt-in, `ae_pack_names.c`). What the stock weapons have is the map's: materials, the assault rifle's melee, the rocket launcher's
drop and ammunition pickup sounds. The numbers are ours: 6 bolts a magazine, 18 to start, 24 in reserve, a semi-automatic
2 bolts a second, a bolt of 20 world units a second slowing to 16 at a quarter of the gravity, a blast of 30 at the
centre (inside 0.35 units) falling to 10 at its 1.5-unit edge (three direct hits strip a full shield, five kill: the
brute shot's harder, smaller blast kills in four) and a knockback of 5 (the damage effect's instantaneous acceleration:
the engine adds it times the object's acceleration scale, in units a second, along the blast's direction lifted by
0.45, halved for a living player: 1.75 on a Spartan a bolt, between a frag's 1.4 and a rocket's 2.1, the same anywhere
in the blast, two bolts a second stacking). Its "detonates explosives" flag is cleared: a warthog takes 1.5 a bolt (the
flag would double it; a rocket's is 3.6), grounded grenades are not set off. Tried in the game on an empty warthog: at 8
one bolt flipped it; at 5 one bolt rears it up and it lands on its wheels, a second flips it. The kick and the blast
carry rumbles (the blast's shortened to 0.5 s, its camera shake to 0.6 s). Held by the label `cr`, or the assault
rifle's (`ar`) where the map's player lacks it (the stock player has no `cr`: `ar` holds it on the stock maps).
tools/test_weapon_pack_concussion_rifle.py holds the recipe to this.
**The tactical SMG's recipe** (`tools/weapon_pack/smg_tactical/`, `ae_smg_tactical`; no slot yet: it waits for the slot table's
widening) takes CMT's `smg_tactical` (the 1.4.1c release's evolved set, `_egg/smg_tactical/`: a suppressed SMG with a red-dot
optic and a zoom; the models, textures and animations are the same bytes in the MCC.6 release, whose tags the bitmap
compiler would not read in places: the pack is built from the 1.4.1c zip the SMG packs use). CMT's one model serves both
views and has a part for each of its six shaders: the gun, the silencer, the scope, its first-person arms (the ODST rubber
material) and two flashlight cones. The `model_parts` step (`model_parts.py`) takes the last parts and their shaders out
of the tag file byte for byte (a JMS round trip through Invader's model compiler made the nodes' inverse transforms again
and the first-person rig drew wrong: the gun pointed at the player): both the third-person and the first-person model are it without the arms
and the cones (3 parts; the first-person arms are the stock cyborg's, as the other SMG packs'), each converted to the Xbox model under its own name
(`smg_tactical_tp`, `smg_tactical_fp`). Its first-person animations are the shared SMG's, whose 42 nodes are the model's.
The zoom overlay is CMT's own scope pictures (`smg_scope.py`: the lines with the zoom label and the small print erased and
the ring centred on a 512 canvas; the mask carried out to 2048 wide as the DMR's) without CMT's zoom screen effect (a green
night-vision tint and a blur, which the other packs' scopes do not have), the aiming reticle the SMG's drawn one, the
ammunition panel the SMG packs'. The numbers are provisional, for the owner to retune in play: 13 rounds a second, a
magazine of 48 (144 to start and in reserve), error 1 to 4 degrees, one zoom level at 2x, the SMG's bullet with a longer
reach (slowing to 220 over 55 units, 12 close, 6.5 slowed). tools/test_weapon_pack_smg_tactical.py and
tools/test_model_parts.py hold the recipe and the cutter to this.
**The gravity hammer's recipe** (`tools/weapon_pack/gravity_hammer/`, `ae_gravity_hammer`; the weapon cache's id 24,
no pack slot; the weapon's name in the game is GRAVITY HAMMER) takes the hammer of Grifball Beta (`grifball.map` by
SlappyThePirate, 2011, a Custom Edition map; the hammer art's original maker is not named) as the map has it: its
models, first-person animations, textures, sounds, effect, particles, HUD, reticle and weapon icon sheet (a community
model, shipped to every player). A Custom Edition map (version 609, whole and not deflated) is a map source as it is:
`make_pack.py` takes it without inflating and reads its tag count from its tag data. Its tags keep none of the map's
folder names: the source's `"renames"` (`{the pack's name: the map's name}`) moves every tag to `hammer/...` right
after extraction, by Invader's refactor (`invader-refactor -M move`, pinned in the recipe's tools when renames are
used), which changes each reference to a moved tag and nothing else; the allowlist, the steps and the pack name the
pack's paths only, and the recipe's `about` keeps the mapping. What an Xbox map needs is done (the two models
converted, the multipurpose bitmap reordered, the Ogg sound made ADPCM, the two HUDs without high-resolution scaling,
the stock message indices). AE's numbers are those of the classic-era hammer (no trigger of its own: the fire button and the melee button both
swing it; one blow, one kill), set for a game with no sprint. The swing is the trigger (one swing every 1.43 s, the map's
43-frame swing; no projectile, no ammunition: a melee pack): its blast (the map's push, which does not hurt its owner)
is a ball of 255 that falls to nothing at 2.6 units from a core of 1.3 (the classic shape: full damage in a core, falling to
the edge), a kill against a full shield and health out to 1.83 units of the hammer's head, the shield stripped out to
about 2.2 (the damage effect has no cone), with a push of 4, the map's rumble and the classic's camera shake; the
reticle turns red at 2.8 units (autoaim range 5.6, magnetism 2.8), the hammer ready after 1 s. The melee button is a blow
of the hammer too: the pack's own damage tag (`hammer/effects/melee.damage_effect`, the blast's tag edited a second time:
175 to the one it lands on, the same push and camera shake) in place of the rocket launcher's melee, at the game's melee
reach; the swing's blast is the trigger's.
The prompts' picture is the map's own picture of the hammer: sequence 8 of its HUD's icon sheet (a 16-bit A4R4G4B4
sheet of whole pictures, one a sequence: `source_icon.py` takes a drawing's `"sequence"`), resampled. The hammer is held
by the label `gh`, else `rl`, else `ar` (the stock player has no hammer animations). tools/test_weapon_pack_gravity_hammer.py
holds the recipe, the renames and the 16-bit sheet reading to this.

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
their places moved from CMT's markers to ones the stock models have (the wreck's model has CMT's own). Revision 31
adds a light loop per kind (`<kind>_damaged_light_loop`, played from 70% of the health): the kind's damaged loop
without its fire (and, for the ghost and the banshee, without the arcs and sparks) and with the smoke thinned; the
donor's second holder carries them. The warthog wreck's smoke is scaled by nothing (a wreck carried by the dead vehicle runs no functions of its own).
`tools/test_weapon_pack_vehicle_damage.py` holds the recipe to this.

**The approved donor and the departures from it.** The rifle the owner approved was a prototype's donor map
("v6e", SHA-256 0aa3ffcb..., `approved_reference` in the recipe). The recipe reproduces that donor's 96 fix tags
byte for byte except six it names under `departures`, which exist so that the pack carries no pixel of the game's
(with `--reference`, `fixes.tsv` compares every made tag with the prototype's and the run stops on any difference
that is not named, and on a named one that is not a difference). With `--approved-donor` the finished pack is
compared with the approved donor itself and may differ only in three tags (`departures.tsv`): the prompts' icon
sheet (the pack has none of its own now: its message icon is CMT's own pickup icon sheet, sequence 1, a source tag
the prototype's donor does not hold; the prototype's was a stock HUD sheet with the picture drawn in), one reference of
a shader on the rifle's model (the stock cyborg's mirror cube map by its own name, which every stock multiplayer
map has, in place of a copy of it), and one number of the ammunition HUD. One own tag is still made from a stock
tag: the ammunition HUD interface, which is layout numbers in the stock assault rifle HUD's structure and no art.

Pinned by SHA-256: the zip, the stock map, Invader's programs, the art scripts' inputs, the donor, the pack. Not
pinned: ffmpeg (its version is logged; other samples would change the donor's hash), Python, numpy and Pillow
(the art's pixel pins catch them), `ae_pack_build` (built from the same commit).

Nothing of CMT's, of the game's or made from them is in the repository: the work folder holds all of it.
`make_pack.py` refuses a work folder under `/tmp`, and one that holds files it did not make. Invader (GPL-3) and
ffmpeg are separate programs it runs.
