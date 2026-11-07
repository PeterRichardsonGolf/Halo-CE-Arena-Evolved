#!/usr/bin/env python3
"""The weapon cache linker (design 5.5): fragments (.aepack, one per recipe) + base fragment + registry -> .aecache.

usage: link.py --fragments <dir> --out <ae_content.aecache> [--lock cache.lock.json] [--release] [--allow-missing]
               [--include-held] [--ae-version 0.7.6] [--schema-hash 0x...] [--check <ae_cache_check>]

Lock time: `--check ae_cache_check` stamps the schema hash and runs the C reader over the output. ae_cache_check has no
placing validation (that runs only in the game). OPTION, not built: a release flow could also run a headless game
attach through the engine lane's run-cache.sh as a belt-and-braces step before the lock is committed.

Fragments are named ae_<key>.aepack (a weapon's key, a tag set's key, ae_grenades, ae_base). Steps, each refusing:
read -> match the registry -> resolve externals to the base layer (ae\\base\\<name>) -> dedupe equal content -> order
-> assign the cache's own resource offsets -> write -> (engine check) -> lock.
"""
import argparse
import hashlib
import json
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "weapon_pack"))
sys.path.insert(0, HERE)
import aepack as ap  # noqa: E402
import merge_donors as md  # noqa: E402
import mcc6_steps  # noqa: E402
import registry as rg  # noqa: E402

# groups no AE code writes at runtime: only these may merge (design 5.5 step 5)
MERGE_GROUPS = {ap.group(g) for g in ("bitm", "snd!", "mode", "antr", "part", "pctl", "deca", "lens", "ligh", "effe",
                                      "pphy", "soso", "schi", "scex", "sgla", "smet", "senv", "swat", "spla", "shdr")}
STOCK = {"none": 0, "assault_rifle": 2, "pistol": 3, "shotgun": 4, "sniper_rifle": 5, "rocket_launcher": 6, "needler": 9}
REPLACES = {None: 0, "map_battle_rifle": 1, "map_needler": 2}
BASE_PREFIX = "ae\\base\\"
OVERRIDE_BITS = {"rounds_loaded": 1 << 0, "rounds_initial": 1 << 1, "rounds_total": 1 << 2, "rate_of_fire": 1 << 3,
                 "error_angle": 1 << 4, "zoom": 1 << 5, "autoaim": 1 << 6, "magnetism": 1 << 7, "behaviour": 1 << 8,
                 "hud": 1 << 9, "menu_name": 1 << 10}


class LinkError(Exception):
    pass


class Fragment:
    def __init__(self, label, path, row=None):
        self.label, self.path, self.row = label, path, row
        with open(path, "rb") as handle:
            self.image = handle.read()
        # (the fragment file's hash, "pack_sha256" in the recipes' expected blocks and the preflight's pins; the hash
        # a pack carries inside itself, over the file with its own field zeroed, is "pack_header_sha256")
        self.pack_sha256 = hashlib.sha256(self.image).hexdigest()
        self.image_for_read, self.parsed, self.notes = md.load(path)[0:3]
        p = self.parsed
        self.tags = [self._tag(i) for i in range(len(p.tags))]
        self.credits = p.credits.decode("utf-8")

    def _tag(self, index):
        p = self.parsed
        (group, p0, p1, name, kind, required, flags, pad, source, boff, bsize, ff, fc, pf, pc, rf, rc) = p.tags[index]
        tag = ap.Tag(group, md._text(p.strings, name), kind=kind, required=required, flags=flags,
                     source_path=md._text(p.strings, source) or "", parent_groups=(p0, p1))
        if kind == ap.OWN:
            tag.bytes = p.sections[ap.TAG_BYTES][boff:boff + bsize]
            tag.fixups = [ap.Fixup(f[0], f[1], f[3], f[2]) for f in p.fixups[ff:ff + fc]]
            geometry = p.sections[ap.GEOMETRY]
            tag.parts = [ap.Part(q[0], q[1], q[2], q[3], q[4], geometry[q[5]:q[5] + q[6]], geometry[q[7]:q[7] + q[8]])
                         for q in p.parts[pf:pf + pc]]
            tag.resources = []
            for r in p.resources[rf:rf + rc]:
                data = ap.read_resource_data(self.image, p, r)
                tag.resources.append(ap.Resource(r[1], data))
        return tag

    def string(self, offset):
        return None if offset == ap.NONE else md._text(self.parsed.strings, offset)


class JoinedBase(Fragment):
    """several base packs (ae_pack_build takes at most 64 roots a run) read as one fragment: tags in file order, the same
    (group, name) in two files kept once (its bytes must agree), references re-pointed"""

    def __init__(self, paths):
        parts = [Fragment("base", p) for p in paths]
        self.label, self.path, self.row = "base", paths[0], None
        self.pack_sha256 = hashlib.sha256("".join(p.pack_sha256 for p in parts).encode()).hexdigest()
        self.pack_sha256s = [p.pack_sha256 for p in parts]
        self.parsed = parts[0].parsed
        self.header_sha256s = [p.parsed.sha256.hex() for p in parts]
        self.credits = parts[0].credits
        self.tags = []
        seen = {}
        for part in parts:
            remap, fresh = {}, []
            for i, t in enumerate(part.tags):
                key = (t.group, t.name, t.kind)
                if key in seen:
                    old = self.tags[seen[key]]
                    if t.kind == ap.OWN and (old.bytes != t.bytes or [r.data for r in old.resources] != [r.data for r in t.resources]):
                        raise LinkError(f"base files disagree on {ap._group_text(t.group)}:{t.name}")
                    remap[i] = seen[key]
                else:
                    seen[key] = remap[i] = len(self.tags)
                    self.tags.append(t)
                    fresh.append(t)
            for t in fresh:
                for fx in t.fixups:
                    if fx.kind in (ap.FIXUP_REFERENCE, ap.FIXUP_TAG_INDEX) and fx.value != ap.NONE:
                        fx.value = remap[fx.value]


def find_fragments(directory, registry, include_held=False, allow_missing=False):
    """(base, [(row, fragment)]) in the cache's order: weapons, tag sets, grenades by id"""
    base_paths = sorted(os.path.join(directory, n) for n in os.listdir(directory)
                        if n == "ae_base.aepack" or (n.startswith("ae_base_") and n.endswith(".aepack")))
    base = (Fragment("base", base_paths[0]) if len(base_paths) == 1 else JoinedBase(base_paths)) if base_paths else None
    if base is None and not allow_missing:
        raise LinkError("the base fragment ae_base.aepack (or ae_base_*.aepack) is missing")
    out = []
    rows = sorted((r for r in registry["rows"] if r["kind"] in ("weapon", "tag_set", "grenade")), key=lambda r: r["id"])
    for row in rows:
        if row["status"] == "retired" or (row["status"] == "held" and not include_held):
            continue
        name = "ae_grenades" if row["kind"] == "grenade" else "ae_" + row["key"]
        path = os.path.join(directory, name + ".aepack")
        if not os.path.exists(path):
            if row["kind"] == "grenade" and row["status"] == "held":
                continue
            if allow_missing:
                continue
            raise LinkError(f"registry row {row['id']} ({row['key']}) has no fragment {name}.aepack")
        out.append((row, Fragment(row["key"], path, row)))
    return base, out


def match_registry(entries):
    """step 2: a weapon fragment has exactly the weapon its registry row names; a tag set has none"""
    for row, fragment in entries:
        weapons = fragment.parsed.weapons
        if row["kind"] == "weapon":
            keys = [md._text(fragment.parsed.strings, w[0]) for w in weapons]
            if keys != [row["fragment_weapon_key"]]:
                raise LinkError(f"fragment {fragment.label}: weapons {keys}, the registry row wants "
                                f"{row['fragment_weapon_key']!r}")
        elif weapons:
            raise LinkError(f"fragment {fragment.label}: a {row['kind']} fragment carries weapons")


# a bitmap group's layout (tag bytes): pixel_data's file_offset at 56, the bitmaps block's address field at 100 (a
# pointer fixup to the 48-byte bitmap_data elements), each element's pixels_offset 0x18 and pixels_size 0x1c in
# (struct bitmap_data: signature, six shorts, registration point, mipmap count and pad, then these)
PIXEL_FILE_OFFSET, BITMAPS_ADDRESS, BITMAP_DATA_SIZE = 56, 100, 0x30
PIXELS_OFFSET_AT, PIXELS_SIZE_AT = 0x18, 0x1c


def pixel_group(tag):
    """whether a bitmap tag with resources has the group layout above: the game reads a bitmap's pixels at the group's
    pixel_data.file_offset + the bitmap's pixels_offset, so a resource's map offset is that SUM (the pixels_offset of
    a group's bitmaps need not be 0), and the group's file_offset moves with the resources"""
    return (tag.group == ap.group("bitm") and tag.resources and len(tag.bytes) >= BITMAPS_ADDRESS + 4 and
            any(x.offset == BITMAPS_ADDRESS and x.kind == ap.FIXUP_POINTER for x in tag.fixups))


def pixel_reads(tag):
    """[(offset, size)] each bitmap's pixels as the game reads them: group file_offset + pixels_offset, pixels_size"""
    start = int.from_bytes(tag.bytes[PIXEL_FILE_OFFSET:PIXEL_FILE_OFFSET + 4], "little", signed=True)
    first = next(x.value for x in tag.fixups if x.offset == BITMAPS_ADDRESS)
    count = int.from_bytes(tag.bytes[BITMAPS_ADDRESS - 4:BITMAPS_ADDRESS], "little")
    reads = []
    for n in range(count):
        at = first + n * BITMAP_DATA_SIZE
        offset, size = (int.from_bytes(tag.bytes[at + k:at + k + 4], "little", signed=True)
                        for k in (PIXELS_OFFSET_AT, PIXELS_SIZE_AT))
        reads.append((start + offset, size))
    return reads


def check_pixel_reads(tag, where):
    """what the game's attach asks of a bitmap (ae_weapon_pack.c placed_element_check): each bitmap's pixels lie in
    one of the tag's resources; refused at link, not at attach. And what its reads do: the texture cache asks the pack
    for the bitmap's pixels_offset ALONE (texture_cache_bitmap_new, which would add the group's file_offset, never
    runs on a placed tag), so a group's file_offset must be 0, or the read lands file_offset below its resource and
    comes back as zeros"""
    start = int.from_bytes(tag.bytes[PIXEL_FILE_OFFSET:PIXEL_FILE_OFFSET + 4], "little", signed=True)
    if start != 0:
        raise LinkError(f"{where}: bitmap {tag.name}: pixel data file_offset {start} is not 0: the game reads the "
                        f"bitmaps' pixels_offset alone, so the pixels would read as zeros")
    for offset, size in pixel_reads(tag):
        if size > 0 and not any(r.map_offset <= offset and offset + size <= r.map_offset + len(r.data) for r in tag.resources):
            raise LinkError(f"{where}: bitmap {tag.name}: pixels at {offset} (+{size}) lie in none of its resources "
                            f"{[(r.map_offset, len(r.data)) for r in tag.resources]}")


class Tree:
    """all fragments' tags with digests (the Merkle key of design 5.5 step 5)"""

    def __init__(self, base, entries, release):
        self.frags = ([base] if base else []) + [f for _, f in entries]
        self.release = release
        self.base_index = {}
        if base:
            for i, t in enumerate(base.tags):
                if t.kind == ap.OWN:
                    self.base_index[(t.group, t.name)] = i
        self.memo = {}
        self.busy = set()

    def target(self, f, i):
        """what fragment f's tag i stands for: ('own', f, i), or a base tag, or ('ext', group, name)"""
        tag = f.tags[i]
        if tag.kind == ap.OWN:
            return ("own", f, i)
        name = tag.name
        j = self.base_index.get((tag.group, BASE_PREFIX + name))
        if j is not None and self.frags and f is not self.frags[0]:
            return ("own", self.frags[0], j)
        if self.release:
            raise LinkError(f"{f.label}: external {ap._group_text(tag.group)}:{name} is not in the base layer "
                            f"(add it to base/names.tsv)")
        return ("ext", tag.group, name)

    def digest(self, f, i):
        key = (id(f), i)
        if key in self.memo:
            return self.memo[key]
        if key in self.busy:
            return "cycle:%d:%d" % key  # a reference cycle never merges
        self.busy.add(key)
        t = f.tags[i]
        h = hashlib.sha256()
        h.update(repr((t.group, t.parent_groups, t.flags)).encode())
        h.update(b"S" if t.group in MERGE_GROUPS else b"U" + f.label.encode() + t.name.encode())
        body = bytearray(t.bytes)
        offsets = {r.map_offset for r in t.resources}
        if pixel_group(t):
            body[PIXEL_FILE_OFFSET:PIXEL_FILE_OFFSET + 4] = b"\0\0\0\0"  # (where the fragment had the pixels)
        for at in range(0, len(body) - 3, 4):
            if int.from_bytes(body[at:at + 4], "little") in offsets:
                body[at:at + 4] = b"\0\0\0\0"
        h.update(bytes(body))
        for fx in sorted(t.fixups, key=lambda x: x.offset):
            value = fx.value
            if fx.kind in (ap.FIXUP_REFERENCE, ap.FIXUP_TAG_INDEX) and value != ap.NONE:
                tgt = self.target(f, value)
                value = self.digest(tgt[1], tgt[2]) if tgt[0] == "own" else repr(tgt)
            h.update(repr((fx.offset, fx.kind, fx.pad, value)).encode())
        for p in t.parts:
            h.update(repr((p.offset, p.vertex_type, p.triangle_type, p.vertex_count, p.triangle_count)).encode())
            h.update(hashlib.sha256(p.vertices).digest() + hashlib.sha256(p.indices).digest())
        for r in t.resources:
            h.update(repr(len(r.data)).encode() + hashlib.sha256(r.data).digest())
        self.busy.discard(key)
        self.memo[key] = h.hexdigest()
        return self.memo[key]


def link(registry, base, entries, release=False, ae_version="0.7.6", schema_hash=0, parts=()):
    """the Cache and a report; raises LinkError"""
    match_registry(entries)
    for f in ([base] if base else []) + [f for _, f in entries]:
        bad = mcc6_steps.bitmap_format_problems(f.tags)
        if bad and release:
            raise LinkError(f"{f.label}: " + "; ".join(f"{name}: {why}" for name, why in bad[:5]))
        for name, why in bad:
            print(f"link: warning: {f.label}: {name}: {why}", file=sys.stderr)
    tree = Tree(base, entries, release)
    order = ([base] if base else []) + [f for _, f in entries]
    placed = {}          # digest -> cache index (mergeable only)
    names = {}           # (group, name) -> (cache index, digest)
    cache_tags = []
    loc = {}             # (id(fragment), local index) -> cache index
    ranges = {}          # fragment -> (first, count)
    merged = []          # (fragment, tag name, into) report
    for f in order:
        first = len(cache_tags)
        for i, t in enumerate(f.tags):
            if t.kind != ap.OWN:
                continue
            d = tree.digest(f, i)
            if t.group in MERGE_GROUPS and d in placed:
                loc[(id(f), i)] = placed[d]
                merged.append((f.label, t.name, cache_tags[placed[d]][0].name))
                continue
            key = (t.group, t.name)
            if key in names and names[key][1] != d:
                raise LinkError(f"{f.label}: tag {ap._group_text(t.group)}:{t.name} exists with different content "
                                f"(namespace it in the recipe)")
            if key in names:
                loc[(id(f), i)] = names[key][0]
                merged.append((f.label, t.name, t.name))
                continue
            index = len(cache_tags)
            cache_tags.append((t, f, i))
            loc[(id(f), i)] = index
            names[key] = (index, d)
            if t.group in MERGE_GROUPS:
                placed[d] = index
        ranges[id(f)] = (first, len(cache_tags) - first)
    # externals left (dev builds)
    externs = {}
    for f in order:
        for i, t in enumerate(f.tags):
            if t.kind == ap.EXTERNAL:
                tgt = tree.target(f, i)
                if tgt[0] == "own":
                    loc[(id(f), i)] = loc[(id(tgt[1]), tgt[2])]
                else:
                    key = (t.group, t.name)
                    if key not in externs:
                        externs[key] = [len(cache_tags) + len(externs), t.required, set()]
                    externs[key][1] = max(externs[key][1], t.required)
                    loc[(id(f), i)] = externs[key][0]
    # resource offsets: the cache's own, unique, keeping each offset's low 12 bits
    cursor = 0
    out_tags = []
    for t, f, i in cache_tags:
        nt = ap.Tag(t.group, t.name, t.kind, t.required, t.flags, t.source_path, bytes(t.bytes),
                    [ap.Fixup(x.offset, x.kind, x.value, x.pad) for x in t.fixups], list(t.parts),
                    [ap.Resource(r.map_offset, r.data) for r in t.resources], t.parent_groups, t.pad)
        for fx in nt.fixups:
            if fx.kind in (ap.FIXUP_REFERENCE, ap.FIXUP_TAG_INDEX) and fx.value != ap.NONE:
                fx.value = loc[(id(f), fx.value)]
        body = bytearray(nt.bytes)
        if pixel_group(nt):
            # (a group's resources keep their distances. The game reads a bitmap at its pixels_offset alone, so the
            # group's file_offset is folded into each bitmap's pixels_offset and left 0: the sum is the resource's
            # map offset, moved by the shift)
            lowest = min(r.map_offset for r in nt.resources)
            file_offset = int.from_bytes(body[PIXEL_FILE_OFFSET:PIXEL_FILE_OFFSET + 4], "little", signed=True)
            shift = (max(cursor - lowest, -lowest) + 4095) // 4096 * 4096
            body[PIXEL_FILE_OFFSET:PIXEL_FILE_OFFSET + 4] = b"\0\0\0\0"
            first = next(x.value for x in nt.fixups if x.offset == BITMAPS_ADDRESS)
            for n in range(int.from_bytes(body[BITMAPS_ADDRESS - 4:BITMAPS_ADDRESS], "little")):
                at = first + n * BITMAP_DATA_SIZE + PIXELS_OFFSET_AT
                folded = int.from_bytes(body[at:at + 4], "little", signed=True) + file_offset + shift
                if not 0 <= folded <= 0x7FFFFFFF:
                    raise LinkError(f"{f.label}: bitmap {nt.name}: pixels at {folded} outside the map's offsets")
                body[at:at + 4] = folded.to_bytes(4, "little", signed=True)
            for r in nt.resources:
                r.map_offset += shift
                cursor = max(cursor, r.map_offset + len(r.data))
            nt.bytes = bytes(body)
            if max(r.map_offset + len(r.data) for r in nt.resources) > 0x7FFFFFFF:
                raise LinkError(f"{f.label}: bitmap {nt.name}: offsets past 2 GiB")
            check_pixel_reads(nt, f"{f.label}")
            out_tags.append(nt)
            continue
        # (every resource moves at once: the new offsets are chosen first and the body is rewritten in ONE pass, so a
        # word that became one resource's new offset is never taken for another resource's old one; a resource
        # nothing in the tag points at is refused, the game would read its data from nowhere)
        moves = {}
        for r in nt.resources:
            cursor = (cursor + 4095) // 4096 * 4096
            new = cursor + (r.map_offset & 0xFFF)
            cursor = new + len(r.data)
            if r.map_offset in moves:
                raise LinkError(f"{f.label}: tag {nt.name}: two resources at map offset {r.map_offset}")
            moves[r.map_offset] = new
        taken = dict.fromkeys(moves, 0)
        for at in range(0, len(body) - 3, 4):
            word = int.from_bytes(body[at:at + 4], "little")
            if word in moves:
                body[at:at + 4] = moves[word].to_bytes(4, "little")
                taken[word] += 1
        unreferenced = sorted(old for old, count in taken.items() if not count)
        if unreferenced:
            raise LinkError(f"{f.label}: tag {ap._group_text(nt.group)}:{nt.name}: no word of the tag points at its "
                            f"resource at map offset {unreferenced[0]}")
        for r in nt.resources:
            r.map_offset = moves[r.map_offset]
        nt.bytes = bytes(body)
        out_tags.append(nt)
    for (group, name), (index, required, _) in sorted(externs.items(), key=lambda kv: kv[1][0]):
        out_tags.append(ap.Tag(group, name, kind=ap.EXTERNAL, required=required, bytes=b"", source_path=""))

    directory = []
    cache_strings_entries = []
    if base:
        first, count = ranges[id(base)]
        directory.append(ap.DirectoryEntry(0x04000001,
                                           ap.KIND_BASE, "base", "BASE LAYER", first_tag=first, tag_count=count))
    for row, f in entries:
        first, count = ranges[id(f)]
        if row["kind"] == "weapon":
            directory.append(weapon_entry(row, f, loc, first, count))
        elif row["kind"] == "tag_set":
            referenced = set()
            for t in f.tags:
                for fx in t.fixups:
                    if fx.kind in (ap.FIXUP_REFERENCE, ap.FIXUP_TAG_INDEX):
                        referenced.add(fx.value)
            roots = sorted({loc[(id(f), i)] for i, t in enumerate(f.tags) if t.kind == ap.OWN and i not in referenced})
            directory.append(ap.DirectoryEntry(row["id"], ap.KIND_TAG_SET, row["key"], row["display"], first_tag=first,
                                               tag_count=count, root_tags=roots[:4096]))
        else:
            directory.append(grenade_entry(row, f, loc, first, count, registry_rows=None))
    # grenade fragment serves every grenade row in the registry that is active
    cache = ap.Cache(out_tags, sorted(directory, key=lambda e: e.id), [], [], "", 1, schema_hash, 1, ae_version)
    cache.externs = [ap.ExternEntry(g, n, req, []) for (g, n), (idx, req, _) in sorted(externs.items(), key=lambda kv: kv[1][0])]
    credits = []
    for f in order:
        if f.credits.strip():
            credits.append(f"== {f.label} ==\n{f.credits.strip()}\n")
    cache.credits = "\n".join(credits).replace("\0", "")
    report = {"fragments": len(order), "tags_in": sum(1 for f in order for t in f.tags if t.kind == ap.OWN),
              "tags_out": len(cache_tags), "merged": merged, "externs_left": len(externs)}
    return cache, report


def weapon_entry(row, f, loc, first, count):
    p = f.parsed
    w = p.weapons[0]
    (key, root, items, behaviour, label, fb0, fb1, menu, pickup, ammo, icon, sequence, replaces, power) = w
    mapped = lambda v: ap.NONE if v == ap.NONE else loc[(id(f), v)]
    flags = (ap.ENTRY_POWER if row.get("power") else 0)
    return ap.DirectoryEntry(
        row["id"], ap.KIND_WEAPON, row["key"], row["display"], flags=flags, first_tag=first, tag_count=count,
        root_tag=mapped(root), item_collection_tag=mapped(items), label=f.string(label),
        fallback_labels=(f.string(fb0), f.string(fb1)), menu_name=f.string(menu), weapon_pickup_text=f.string(pickup),
        ammo_pickup_text=f.string(ammo), icon_bitmap_tag=mapped(icon), icon_sequence=sequence, behaviour=behaviour,
        stock_fallback=STOCK[row["stock_fallback"]], replaces_map_own=REPLACES[row.get("replaces_map_own")],
        legacy_slot=row.get("legacy_slot") or 0)


def grenade_entry(row, f, loc, first, count, registry_rows):
    """a grenade type's tags by group among the fragment's own tags (design 3.3)"""
    def find(group, suffix=None):
        for i, t in enumerate(f.tags):
            if t.kind == ap.OWN and t.group == ap.group(group) and (suffix is None or t.name.endswith(suffix)):
                return loc[(id(f), i)]
        return ap.NONE
    return ap.DirectoryEntry(row["id"], ap.KIND_GRENADE, row["key"], row["display"], first_tag=first, tag_count=count,
                             grenade_catalogue_id=row.get("catalogue_id", 0), projectile_tag=find("proj"),
                             equipment_tag=find("eqip"), collection_tag=find("itmc"), hud_tag=find("grhi"),
                             field_effect_tag=find("effe", "emp_field"))


def add_configs(cache, registry):
    """registry config rows -> CONFIGS records (several per weapon allowed; design 3.5, owner scope change)"""
    weapons = {r["id"] for r in registry["rows"] if r["kind"] == "weapon" and r["status"] == "active"}
    in_cache = {e.id for e in cache.directory}
    for row in sorted((r for r in registry["rows"] if r["kind"] == "config" and r["status"] == "active"),
                      key=lambda r: r["id"]):
        if row["weapon"] not in weapons or row["weapon"] not in in_cache:
            raise LinkError(f"config {row['id']} ({row['key']}): its weapon {row['weapon']} is not an active weapon in the cache")
        c = ap.ConfigEntry(row["id"], row["weapon"], row["key"], row.get("display"))
        for name, value in row.get("overrides", {}).items():
            if name not in OVERRIDE_BITS:
                raise LinkError(f"config {row['key']}: unknown override {name}")
            c.override_mask |= OVERRIDE_BITS[name]
            if name == "rounds_loaded":
                c.rounds_loaded_maximum = value
            elif name == "rounds_initial":
                c.rounds_total_initial = value
            elif name == "rounds_total":
                c.rounds_total_maximum = value
            elif name == "behaviour":
                c.behaviour = value
        cache.configs.append(c)


def schema_hash_from(checker):
    """the tag schema hash AE computes, from `ae_cache_check --schema-hash` (never hard-coded: a schema change must
    refuse loudly at build); the first 16-hex-digit word of its output"""
    result = subprocess.run([checker, "--schema-hash"], capture_output=True, text=True)
    match = re.search(r"\b([0-9a-fA-F]{16})\b", result.stdout)
    if result.returncode or not match:
        raise LinkError(f"{checker} --schema-hash gave no hash (exit {result.returncode}): {result.stdout.strip()[:200]}")
    return int(match.group(1), 16)


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("--fragments", required=True)
    parser.add_argument("--out", required=True)
    parser.add_argument("--lock")
    parser.add_argument("--registry", default=rg.DEFAULT)
    parser.add_argument("--release", action="store_true")
    parser.add_argument("--allow-missing", action="store_true")
    parser.add_argument("--include-held", action="store_true")
    parser.add_argument("--ae-version", default="0.7.6")
    parser.add_argument("--schema-hash", help="override (dev only); default: ae_cache_check --schema-hash")
    parser.add_argument("--check")
    args = parser.parse_args(argv)
    registry = rg.load(args.registry)
    problems = rg.check_rows(registry)
    if problems:
        raise SystemExit("registry: " + "; ".join(problems))
    base, entries = find_fragments(args.fragments, registry, args.include_held, args.allow_missing)
    if args.check:
        schema = schema_hash_from(args.check)
        if args.schema_hash and int(args.schema_hash, 0) != schema:
            raise LinkError("--schema-hash disagrees with ae_cache_check --schema-hash")
    elif args.schema_hash:
        schema = int(args.schema_hash, 0)
    elif args.release:
        raise LinkError("a release link needs --check <ae_cache_check> for the schema hash")
    else:
        schema = 0
        print("link: warning: no schema hash (a dev build of the game refuses this cache)", file=sys.stderr)
    cache, report = link(registry, base, entries, args.release, args.ae_version, schema)
    cache.serial, cache.registry_revision = registry["serial"], registry["revision"]
    add_configs(cache, registry)
    image = ap.build_cache(cache)
    ap.read_cache(image)
    with open(args.out, "wb") as handle:
        handle.write(image)
    lock = {"serial": cache.serial, "sha256": hashlib.sha256(image).hexdigest(), "cache_id": ap.cache_id(image).hex(),
            "schema_hash": cache.schema_hash, "registry_revision": cache.registry_revision,
            "lock_schema": "pack_sha256 = SHA-256 of the fragment file; pack_header_sha256 = the hash inside the pack's "
                           "header; cache sha256 = SHA-256 of the cache file",
            "fragments": {f.label: {"pack_sha256": f.pack_sha256, "pack_header_sha256": f.parsed.sha256.hex()}
                          for _, f in entries},
            "base": ({"pack_sha256": base.pack_sha256, "pack_header_sha256": base.parsed.sha256.hex(),
                      "files": getattr(base, "pack_sha256s", None)} if base else None),
            "size": len(image), "tags": len(cache.tags), "merged": len(report["merged"])}
    if args.lock:
        with open(args.lock, "w") as handle:
            json.dump(lock, handle, indent=1, sort_keys=True)
            handle.write("\n")
    print(json.dumps({k: v for k, v in lock.items() if k != "fragments"}, indent=1))
    if args.check:
        result = subprocess.run([args.check, args.out], capture_output=True, text=True)
        print(result.stdout)
        if result.returncode:
            raise SystemExit("ae_cache_check refused the cache")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except LinkError as why:
        raise SystemExit(f"link: {why}")
