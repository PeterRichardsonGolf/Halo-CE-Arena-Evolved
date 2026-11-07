"""Generic recipe steps for CMT's MCC.6 models (the battle rifle pilot, formalized), and the bitmap format refusal.

mcc_display_shader_twins   a model step: the MCC display shader variants (shaders[2..5] of a weapon model: bullet
                           counter, compass, ammo numbers) are replaced by their classic chicago twins, because the
                           engine draws the classic ones. expand() turns the step into the plain `edit` step the
                           pilot wrote by hand; a model whose shader slots differ from what the step expects is
                           refused (check_model_shaders).
hud_subtree_swap           MCC.6's cmt/ui/hudv4.1/ art is BC7 / A8Y8 which the pack tools cannot read; the recipe's
                           source archive takes that subtree (and the shared impact effects, flag-changed in MCC.6) from the
                           1.4.1c release instead (the pinned instance is tools/weapon_pack/mcc6_source.py on
                           ae/mcc6-packs-076; this is its parameterized form).
bitmap_format_problems     the refusal for any own bitmap whose pixel format the engine cannot draw (BC7 and any
                           value outside the Xbox texture table), and A8Y8 on a HUD meter / smooth bitmap.
"""
import hashlib
import struct
import sys
import zipfile

try:
    import aepack as ap
except ImportError:  # imported from another folder
    import os
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import aepack as ap

OP_TWINS = "mcc_display_shader_twins"
CLASSIC_COMPASS = "cmt\\textures\\displays\\shaders\\cmt_unsc_display_compass.shader_transparent_chicago"
CLASSIC_NUMBERS = "cmt\\textures\\displays\\shaders\\cmt_unsc_display_numbers_100ct.shader_transparent_chicago"
# slot -> twin; slot 2 (the bullet counter) is the weapon's own chicago shader, named by the step
DEFAULT_TWINS = {3: CLASSIC_COMPASS, 4: CLASSIC_NUMBERS, 5: CLASSIC_NUMBERS}
FIRST_SLOT, LAST_SLOT = 2, 5
MCC_SHADER_SUFFIX = "_mcc"


class StepError(Exception):
    pass


def twin_edits(step):
    """the invader-edit `set` lines of one mcc_display_shader_twins step"""
    bullet = step.get("bullet")
    if not bullet or not bullet.endswith(".shader_transparent_chicago"):
        raise StepError(f"{OP_TWINS}: 'bullet' must name the weapon's classic chicago shader")
    twins = {2: bullet, **DEFAULT_TWINS}
    for slot, name in (step.get("twins") or {}).items():
        slot = int(slot)
        if not FIRST_SLOT <= slot <= LAST_SLOT or not name.endswith(".shader_transparent_chicago"):
            raise StepError(f"{OP_TWINS}: twin for slot {slot} must be a chicago shader in slots {FIRST_SLOT}-{LAST_SLOT}")
        twins[slot] = name
    return [["set", f"shaders[{slot}].shader", twins[slot]] for slot in sorted(twins)]


def expand(recipe):
    """the recipe with every mcc_display_shader_twins step turned into an `edit` step (a copy; others untouched)"""
    steps = []
    for step in recipe.get("steps", ()):
        if step.get("op") != OP_TWINS:
            steps.append(step)
            continue
        if not step.get("tag") or not step.get("from"):
            raise StepError(f"{OP_TWINS}: needs 'tag' and 'from'")
        new = {k: v for k, v in step.items() if k not in ("bullet", "twins", "expect")}
        new["op"] = "edit"
        new["edits"] = twin_edits(step)
        new.setdefault("why", "the MCC display shader variant is not carried; the classic display shader stands in")
        steps.append(new)
    out = dict(recipe)
    out["steps"] = steps
    return out


def check_model_shaders(shader_names, expect):
    """shader_names: the model's shader references in order (path strings); expect: the MCC names the step replaces,
    slots 2-5. Refuses a model whose list is shorter, or whose slot does not match what the pilot saw."""
    if len(shader_names) <= LAST_SLOT:
        raise StepError(f"the model has {len(shader_names)} shaders; {OP_TWINS} needs slots {FIRST_SLOT}-{LAST_SLOT}")
    for slot, want in zip(range(FIRST_SLOT, LAST_SLOT + 1), expect):
        if shader_names[slot] != want:
            raise StepError(f"shader slot {slot} is {shader_names[slot]!r}, the step expects {want!r}")


def hud_subtree_swap(mcc_zip, classic_zip, out_zip, *, mcc_root, classic_root, subtree=None, subtrees=None, expect,
                     stamp=(2026, 11, 5, 1, 3, 48)):
    """MCC archive with the classic archive's subtrees swapped in under mcc_root; every other member copied byte for
    byte; inputs and output pinned by `expect` = {"mcc": sha, "classic": sha, "out": sha}.
    subtrees: [(path, mode)], mode "all" (the whole subtree is the classic one's) or "shared" (only the members both
    archives have: the shared impact effects, where MCC.6 changed a flag); subtree= is one ("path", "all") pair.
    The pinned instance is mcc6_source.py (HUD art "all", cmt/effects/impacts/ "shared")."""
    subtrees = list(subtrees or [(subtree, "all")])

    def sha(path):
        h = hashlib.sha256()
        with open(path, "rb") as f:
            for block in iter(lambda: f.read(1 << 20), b""):
                h.update(block)
        return h.hexdigest()

    for path, key in ((mcc_zip, "mcc"), (classic_zip, "classic")):
        if sha(path) != expect[key]:
            raise StepError(f"{path}: not the pinned archive")
    with zipfile.ZipFile(mcc_zip) as a, zipfile.ZipFile(classic_zip) as c, \
            zipfile.ZipFile(out_zip, "w", zipfile.ZIP_DEFLATED) as o, open(mcc_zip, "rb") as raw:
        classic_names, mcc_names = set(c.namelist()), set(a.namelist())

        def swapped(name, in_mcc):
            """the subtree mode that covers this member, or None"""
            rest = name[len(mcc_root if in_mcc else classic_root):] if name.startswith(mcc_root if in_mcc else classic_root) else None
            if rest is None or name.endswith("/"):
                return False
            for path, mode in subtrees:
                if rest.startswith(path):
                    if mode == "all":
                        return True
                    other = (classic_root + rest) if in_mcc else (mcc_root + rest)
                    return other in (classic_names if in_mcc else mcc_names)
            return False

        infos = sorted(a.infolist(), key=lambda i: i.header_offset)
        ends = [i.header_offset for i in infos[1:]] + [a.start_dir]
        extents = {i.filename: (i.header_offset, e) for i, e in zip(infos, ends)}
        for i in a.infolist():
            if swapped(i.filename, True):
                continue
            start, end = extents[i.filename]
            raw.seek(start)
            data = raw.read(end - start)
            i.header_offset = o.fp.tell()
            o.fp.write(data)
            o.filelist.append(i)
            o.NameToInfo[i.filename] = i
            o.start_dir = o.fp.tell()
        for i in c.infolist():
            if not swapped(i.filename, False):
                continue
            zi = zipfile.ZipInfo(mcc_root + i.filename[len(classic_root):], stamp)
            zi.compress_type = zipfile.ZIP_DEFLATED
            zi.external_attr = 0o600 << 16
            o.writestr(zi, c.read(i))
    if sha(out_zip) != expect["out"]:
        raise StepError(f"output {out_zip}: not the pinned sha256")


# ---------- the bitmap format refusal

# the Xbox texture table (rasterizer_xbox_hardware_bitmaps.c): a8 y8 ay8 a8y8 - - r5g6b5 - a1r5g5b5 a4r4g4b4 x8r8g8b8
# a8r8g8b8 - - dxt1 dxt3 dxt5 p8; anything else (MCC's BC7 is past the end) draws nothing
DRAWABLE_FORMATS = {0: "a8", 1: "y8", 2: "ay8", 3: "a8y8", 6: "r5g6b5", 8: "a1r5g5b5", 9: "a4r4g4b4", 10: "x8r8g8b8",
                    11: "a8r8g8b8", 14: "dxt1", 15: "dxt3", 16: "dxt5", 17: "p8"}
A8Y8 = 3
BITMAPS_BLOCK_COUNT_OFFSET = 96   # bitmap_group.bitmaps: count (u32), then the elements' address at +4
BITMAP_DATA_SIZE = 48
BITMAP_DATA_FORMAT_OFFSET = 12


def bitmap_formats(tag):
    """the pixel format of every bitmap in an own 'bitm' tag (list of ints); None if the tag cannot be read"""
    if len(tag.bytes) < BITMAPS_BLOCK_COUNT_OFFSET + 12:
        return None
    count = struct.unpack_from("<I", tag.bytes, BITMAPS_BLOCK_COUNT_OFFSET)[0]
    if count == 0:
        return []
    pointer = [f for f in tag.fixups if f.offset == BITMAPS_BLOCK_COUNT_OFFSET + 4 and f.kind == ap.FIXUP_POINTER]
    if not pointer:
        return None
    base = pointer[0].value
    if base + count * BITMAP_DATA_SIZE > len(tag.bytes):
        return None
    return [struct.unpack_from("<h", tag.bytes, base + i * BITMAP_DATA_SIZE + BITMAP_DATA_FORMAT_OFFSET)[0]
            for i in range(count)]


def bitmap_format_problems(tags):
    """[(tag name, problem)] for the own bitmaps the engine cannot draw; a8y8 is refused on a HUD meter / smooth tag
    (the meter's coverage pass cannot read it)"""
    problems = []
    for tag in tags:
        if tag.kind != ap.OWN or tag.group != ap.GROUP_BITMAP:
            continue
        formats = bitmap_formats(tag)
        if formats is None:
            problems.append((tag.name, "bitmap tag cannot be read"))
            continue
        for index, fmt in enumerate(formats):
            if fmt not in DRAWABLE_FORMATS:
                problems.append((tag.name, f"bitmap {index}: format {fmt} is not one the engine draws (BC7 or unknown)"))
            elif fmt == A8Y8 and tag.flags & (ap.FLAG_SMOOTH | ap.FLAG_METER_COVERAGE):
                problems.append((tag.name, f"bitmap {index}: a8y8 on a meter/smooth bitmap"))
    return problems
