#!/usr/bin/env python3
"""Merged content manifest of N weapon packs (.aepack): the analysis half of the weapon cache's packer.

usage: merge_donors.py [-o manifest.json] [--text] <pack.aepack>...

For every tag of every pack it computes a pack-independent content digest (group, flags, bytes, fixups with
tag-index targets turned into tag NAMES, geometry parts, inflated resources). Then it groups by tag name:
  - dedup group   : same name, same digest in several packs (store once)
  - collision     : same name, DIFFERENT digest (must be namespaced, or one chosen as the base copy)
  - external deps : tags a pack names but does not carry (they come from the map)
Byte accounting is the payload a tag really costs in the file: tag bytes + geometry (vertices, indices) +
resource data as stored.
"""
import argparse
import hashlib
import json
import os
import re
import sys
import zlib
from collections import defaultdict

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "weapon_pack"))
import aepack as ap  # noqa: E402


def _text(strings, offset):
    if offset == ap.NONE:
        return None
    return strings[offset:strings.index(b"\0", offset)].decode("utf-8")


def _group_text(group):
    return ap.struct.pack(">I", group).decode("latin-1")


PREFIX = re.compile(r"^ae\\(?:packs|pack)\\[^\\]+\\")


def tag_name(parsed, index):
    return _text(parsed.strings, parsed.tags[index][3])


def stripped(name):
    """the name without its per-pack namespace prefix (ae\\packs\\<dir>\\), so equal content compares equal"""
    return PREFIX.sub("", name)


def load(path):
    """(image, parsed, notes). A pack from a newer branch may use a weapon behaviour this tree's reader does not
    know (the wave packs' 4 and 5): read it with those behaviours zeroed in a copy, and say so in notes."""
    with open(path, "rb") as handle:
        image = handle.read()
    try:
        return image, ap.read(image), {}
    except ap.Refused as why:
        if "behaviour" not in str(why):
            raise
    copy = bytearray(image)
    start = ap.section_offset(copy, ap.WEAPONS)
    seen = []
    for entry in ap.section_entries(copy):
        if entry[0] == ap.WEAPONS:
            count = entry[3] // ap.WEAPON_RECORD.size
    for index in range(count):
        at = start + index * ap.WEAPON_RECORD.size + 12
        seen.append(int.from_bytes(copy[at:at + 4], "little"))
        copy[at:at + 4] = (0).to_bytes(4, "little")
    ap.rehash(copy)
    return image, ap.read(bytes(copy)), {"behaviours_unknown_to_reader": seen}


def tag_digest(parsed, index):
    """(digest hex, payload bytes) of one own tag, independent of the pack it sits in"""
    tag = parsed.tags[index]
    (group, p0, p1, _name, kind, required, flags, _pad, _src, bytes_offset, bytes_size,
     fix_first, fix_count, part_first, part_count, res_first, res_count) = tag
    sections = parsed.sections
    hasher = hashlib.sha256()
    hasher.update(repr((group, p0, p1, flags)).encode())
    payload = 0
    body = bytearray(sections[ap.TAG_BYTES][bytes_offset:bytes_offset + bytes_size])
    # a tag with resources holds the donor map's file offset of each (bitmap pixel offsets): pack-specific, so
    # zero every aligned word equal to one of its resources' map offsets before hashing
    offsets = {res[1] for res in parsed.resources[res_first:res_first + res_count]}
    for at in range(0, len(body) - 3, 4):
        if int.from_bytes(body[at:at + 4], "little") in offsets:
            body[at:at + 4] = b"\0\0\0\0"
    hasher.update(bytes(body))
    payload += bytes_size
    for fixup in parsed.fixups[fix_first:fix_first + fix_count]:
        offset, fkind, pad, value = fixup
        if fkind in (ap.FIXUP_REFERENCE, ap.FIXUP_TAG_INDEX) and value < len(parsed.tags):
            value = "tag:" + stripped(tag_name(parsed, value))
        hasher.update(repr((offset, fkind, pad, value)).encode())
    geometry = sections[ap.GEOMETRY]
    for part in parsed.parts[part_first:part_first + part_count]:
        poff, vtype, ttype, vcount, tcount, voff, vsize, ioff, isize = part
        hasher.update(repr((poff, vtype, ttype, vcount, tcount)).encode())
        hasher.update(geometry[voff:voff + vsize])
        hasher.update(geometry[ioff:ioff + isize])
        payload += vsize + isize
    data = sections[ap.RESOURCE_DATA]
    for res in parsed.resources[res_first:res_first + res_count]:
        _tag, map_offset, size, doff, dsize, compression = res
        raw = data[doff:doff + dsize]
        if compression == ap.ZLIB:
            raw = zlib.decompress(raw)
        hasher.update(repr(size).encode())  # (map_offset is the donor map's file offset: pack-specific, not content)
        hasher.update(hashlib.sha256(raw).digest())
        payload += dsize
    return hasher.hexdigest(), payload


def pack_summary(path, image, parsed):
    strings = parsed.strings
    header = parsed.header
    weapons = []
    for w in parsed.weapons:
        weapons.append({"key": _text(strings, w[0]), "label": _text(strings, w[4]), "menu": _text(strings, w[7]),
                        "behaviour": w[3], "replaces": w[12], "power": w[13],
                        "root": tag_name(parsed, w[1])})
    return {"file": os.path.basename(path), "id": header[4].rstrip(b"\0").decode(), "revision": header[5],
            "display": header[6].rstrip(b"\0").decode(), "flags": header[3], "size": len(image),
            "header_sha256": parsed.sha256.hex(), "file_sha256": hashlib.sha256(image).hexdigest(),
            "weapons": weapons}


def merge(paths):
    packs = []
    tags = defaultdict(lambda: {"variants": {}})
    externals = defaultdict(set)
    by_content = defaultdict(list)
    for path in paths:
        image, parsed, notes = load(path)
        info = pack_summary(path, image, parsed)
        info.update(notes)
        own = ext = payload = 0
        for index, tag in enumerate(parsed.tags):
            name = tag_name(parsed, index)
            key = f"{_group_text(tag[0])}:{stripped(name)}"
            if tag[4] == ap.EXTERNAL:
                ext += 1
                externals[key].add(info["id"])
                continue
            digest, size = tag_digest(parsed, index)
            own += 1
            payload += size
            variant = tags[key]["variants"].setdefault(digest, {"bytes": size, "packs": [], "names": []})
            variant["packs"].append(info["id"])
            variant["names"].append(name)
            by_content[digest].append((info["id"], name, size))
        info.update({"own_tags": own, "external_tags": ext, "payload_bytes": payload})
        packs.append(info)
    out_tags = {}
    unique = 0
    dedup_saving = 0
    dup_groups = 0
    collisions = []
    for key, entry in sorted(tags.items()):
        variants = [{"digest": d, "bytes": v["bytes"], "packs": v["packs"], "names": v["names"]} for d, v in entry["variants"].items()]
        out_tags[key] = variants
        for v in variants:
            unique += v["bytes"]
            if len(set(v["packs"])) > 1:
                dup_groups += 1
                dedup_saving += v["bytes"] * (len(set(v["packs"])) - 1)
        if len(variants) > 1:
            collisions.append(key)
    sum_packs = sum(p["payload_bytes"] for p in packs)
    # content-addressed totals: one copy per digest, whatever the tag is called
    content_unique = sum(items[0][2] for items in by_content.values())
    content_groups = {d: [{"pack": a, "name": b, "bytes": c} for a, b, c in items]
                      for d, items in by_content.items() if len(items) > 1}
    cross_name = sum(1 for items in content_groups.values() if len({stripped(i["name"]) for i in items}) > 1)
    summary = {"packs": len(packs), "sum_pack_file_bytes": sum(p["size"] for p in packs),
               "sum_payload_bytes": sum_packs, "unique_payload_bytes": content_unique, "unique_by_name_and_content_bytes": unique,
               "content_dup_groups": len(content_groups), "content_dup_groups_under_different_paths": cross_name,
               "content_dup_saving_bytes": sum_packs - content_unique,
               "same_path_dedup_saving_bytes": dedup_saving, "same_path_dedup_groups": dup_groups,
               "unique_tag_names": len(out_tags), "collision_names": len(collisions),
               "collision_variant_bytes": sum(v["bytes"] for k in collisions for v in out_tags[k]),
               "external_names": len(externals)}
    return {"summary": summary, "packs": packs, "tags": out_tags, "collisions": collisions, "content_groups": content_groups,
            "externals": {k: sorted(v) for k, v in sorted(externals.items())}}


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("packs", nargs="+")
    parser.add_argument("-o", "--output")
    args = parser.parse_args(argv)
    manifest = merge(args.packs)
    text = json.dumps(manifest, indent=1, sort_keys=True)
    if args.output:
        with open(args.output, "w") as handle:
            handle.write(text)
    print(json.dumps(manifest["summary"], indent=1))
    return 0


if __name__ == "__main__":
    sys.exit(main())
