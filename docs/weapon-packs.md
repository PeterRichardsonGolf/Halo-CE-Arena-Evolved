# Weapon packs: file format v1

A weapon pack (`.aepack`) carries one or more weapons for Arena Evolved to add to a map that does not have
them: each weapon's tags (address-free), the geometry of its models, its bitmaps' pixels and sounds' samples,
and what the engine needs to know about it (its label, menu name, pickup texts, message icon, the stock weapon
whose spawns it may replace). Packs are untrusted data: players download them. This document is the format;
the reader that the game and the tools use is `port/linux/game/ae_pack_format.c` (`ae_pack_format.h`), and
`tools/weapon_pack/aepack.py` is the same reader and writer in Python, held to agree with it on every pack by
`tools/test_weapon_pack_format.py`.

`build/linux/ae_pack_check` (built by `ninja linux`) checks packs as the game does and prints them:

    build/linux/ae_pack_check pack.aepack...      header, sections, tags with flags, weapons; exit 1 if refused
    build/linux/ae_pack_check --sha256 file...    our SHA-256 of files
    build/linux/ae_pack_check --rewrite in out    in written again by ae_pack_write

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
- **FIXUPS**: 12-byte records {offset u32 (in the tag's bytes), kind u16, pad u16 (0), value u32}:
  - kind 1 POINTER: a 4-byte Xbox address at offset becomes the tag's copy's base + value (value inside the
    tag's bytes);
  - kind 2 REFERENCE: a 16-byte tag reference at offset names pack tag value, or none (NONE);
  - kind 3 TAG_INDEX: a 4-byte tag index at offset names pack tag value.
- **PARTS**: 32-byte records, a model part's geometry {offset u32 (its 0x68-byte model_geometry_part in the tag's
  bytes), vertex_type u16 (0..11), triangle_type u16 (0 triangles, 1 strip), vertex_count u32, triangle_count u32
  (each 1..65535), vertices offset u32 and size u32, indices offset u32 and size u32 (in GEOMETRY)}. The
  vertices' size is the count times the type's size (rasterizer_geometry.c: 56, 32, 20, 8, 68, 32, 24, 36, 20,
  16, 16, 8); the indices are 16-bit, 3 a triangle or count + 2 for a strip, each below the vertex count.
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
  | 0x20 | pickup_texts, string[2] | picking it up, swapping for it: UTF-8, 1 to 127 bytes, or NONE |
  | 0x28 | icon_bitmap_tag, u32 | the message icon: a `'bitm'` tag, or NONE (then icon_sequence 0) |
  | 0x2C | icon_sequence, u32 | 0 to 65535; its sequence 0 must have sprites (the builder and the engine check it) |
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
   flag bit, flags on own bitmaps only; an own tag not required, its bytes 1 byte to 4 MiB inside TAG_BYTES; an
   external tag without bytes, fixups, parts or resources; its fixups, parts and resources inside their
   sections and after the previous tag's (no two tags share a record); at most 512 own tags; every record
   owned by a tag; no two tags of the same group and name.
8. Each own tag's fixups (inside its bytes; pointers to a target inside them; references to a pack tag or
   NONE; tag indices to a pack tag), parts (inside its bytes, in increasing order; types, counts and sizes as
   above; inside GEOMETRY; every index below the vertex count) and resources (its own; 1 byte to 8 MiB; the
   offset plus size within 32 bits; stored sizes equal or a zlib stream not empty; inside RESOURCE_DATA; 48
   MiB in all).
9. Each weapon as its table says.
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
  where the last section ends.
- STRINGS: each distinct string once, in the order they are first met: for each tag its name then its source
  path; then for each weapon its key, label, fallback labels, menu name, pickup texts.
- TAG_BYTES, GEOMETRY (each part's vertices then indices) and RESOURCE_DATA: each item at the next 16-aligned
  offset of its section, in tag order.
- A tag's records follow the previous tag's: `first` is the running count, for every tag.
- Tags in breadth-first order from the weapon roots as the pack builder's schema walk meets them; no
  timestamps anywhere.

So the same inputs give the same bytes and the same SHA-256. The writer checks its image as the reader does
before writing anything: it never writes a pack a reader would refuse.

## Memory

TAG_BYTES, FIXUPS, PARTS, GEOMETRY, STRINGS (and the small TAGS, RESOURCES, WEAPONS, CREDITS) are read at open:
a few MB. RESOURCE_DATA stays in the file, read per request; a pack keeps its file open until
`ae_pack_close`.
