# hud_scale.py <tagsdir> [stocktagsdir]
# Brings the weapon, unit and grenade HUD tags of a converted MCC map to the Xbox canvas.
# MCC authors these tags for a doubled canvas: placements on 960p, bitmaps twice the size (four times for the
# half-HUD ones). The same rule set as the game's own MCC map loader (port/linux/game/mcc_hud.c):
#   - a bitmap with flags 0x10 or 0x80 is "half"
#   - a placement's offset is halved unless its scaling flag 4 (use_high_res_scale) is set; flag 4 is cleared
#   - a placement's width/height scale is multiplied by 0.25 for a half bitmap, 0.5 otherwise
#   - number elements, the unit's motion sensor centre and the grenade's number block get the offset rule only
# Each placement's bitmap is the one its element names; the items of a crosshair, overlay or grenade overlay element
# use that element's bitmap. A bitmap is looked up by its path in the tags tree, then in the stock tree.
# A HUD tag identical to the stock tree's is already on the Xbox canvas and is left alone.
# A tag whose layout is not understood, or that names a bitmap found in neither tree, is left alone and reported.
import glob
import os
import struct
import sys

HEAD = 64
GROUPS = {
    'weapon_hud_interface': b'wphi',
    'unit_hud_interface': b'unhi',
    'grenade_hud_interface': b'grhi',
}
PLACEMENT = 0x24  # offset x,y (int16), width/height scale (float32), scaling flags (uint16), pad, unused
FLAG_HIGH_RES = 4


class NotUnderstood(Exception):
    pass


class Dep:
    """A tag reference (16 bytes) at a struct offset; its path string follows the struct's data."""

    def __init__(self, off):
        self.off = off


class Block:
    """A tag block (12 bytes) at a struct offset. elem None: the block must be empty."""

    def __init__(self, off, elem=None):
        self.off = off
        self.elem = elem


class Struct:
    """size: bytes. items: Dep/Block in the struct. placements: (offset, mode, bitmap) where mode is 'full' (offset
    and scale) or 'offset' (offset only) and bitmap is the offset of a Dep in this struct, 'parent' (the bitmap the
    enclosing element names) or None. item_bitmap: the Dep offset whose bitmap the child items use."""

    def __init__(self, size, items=(), placements=(), item_bitmap=None):
        self.size = size
        self.items = sorted(items, key=lambda i: i.off)
        self.placements = list(placements)
        self.item_bitmap = item_bitmap


EFFECTOR = Struct(0xDC)
MULTITEXTURE = Struct(0x1E0, [Dep(0x64), Dep(0x74), Dep(0x84), Block(0x154, EFFECTOR)])


def static_items(p):
    """The bitmap reference and the multitexture overlay block of a static hud element starting at p."""
    return [Dep(p + 0x24), Block(p + 0x58, MULTITEXTURE)]


X_ITEM = Struct(0x6C, placements=[(0, 'full', 'parent')])
O_ITEM = Struct(0x88, placements=[(0, 'full', 'parent')])
BITMAP_ELEMENT = [Dep(0x24), Block(0x34)]

W_STATIC = Struct(0xB4, static_items(0x24), [(0x24, 'full', 0x48)])
W_METER = Struct(0xB4, [Dep(0x48)], [(0x24, 'full', 0x48)])
W_NUMBER = Struct(0xA0, placements=[(0x24, 'offset', None)])
W_CROSSHAIR = Struct(0x68, [Dep(0x24), Block(0x34, X_ITEM)], item_bitmap=0x24)
W_OVERLAY = Struct(0x68, [Dep(0x24), Block(0x34, O_ITEM)], item_bitmap=0x24)

WEAPON = Struct(0x17C, [
    Dep(0x0),
    Block(0x60, W_STATIC), Block(0x6C, W_METER), Block(0x78, W_NUMBER),
    Block(0x84, W_CROSSHAIR), Block(0x90, W_OVERLAY)])

U_AUX_OVERLAY = Struct(0x84, static_items(0), [(0, 'full', 0x24)])
U_AUX_METER = Struct(0x144, static_items(0x14) + [Dep(0x7C + 0x24)],
                     [(0x14, 'full', 0x14 + 0x24), (0x7C, 'full', 0x7C + 0x24)])
U_SOUND = Struct(0x38, [Dep(0)])

UNIT = Struct(0x56C,
              static_items(0x24) + static_items(0x8C) + [Dep(0xF4 + 0x24)] +
              static_items(0x17C) + [Dep(0x1E4 + 0x24)] +
              static_items(0x26C) + static_items(0x2D4) +
              [Block(0x3A4, U_AUX_OVERLAY), Block(0x3C0, U_SOUND), Block(0x3CC, U_AUX_METER)],
              [(0x24, 'full', 0x48), (0x8C, 'full', 0x8C + 0x24), (0xF4, 'full', 0xF4 + 0x24),
               (0x17C, 'full', 0x17C + 0x24), (0x1E4, 'full', 0x1E4 + 0x24),
               (0x26C, 'full', 0x26C + 0x24), (0x2D4, 'full', 0x2D4 + 0x24),
               (0x35C, 'offset', None)])

GRENADE = Struct(0x1F8,
                 static_items(0x24) + static_items(0x8C) + [Dep(0x14C), Block(0x15C, O_ITEM)],
                 [(0x24, 'full', 0x48), (0x8C, 'full', 0x8C + 0x24), (0xF4, 'offset', None)],
                 item_bitmap=0x14C)

SCHEMAS = {b'wphi': WEAPON, b'unhi': UNIT, b'grhi': GRENADE}


def u32(b, o):
    return struct.unpack_from('>I', b, o)[0]


def read_path(b, pos, length):
    """The NUL-terminated path string of a reference, and the position after it."""
    if not length:
        return None, pos
    end = pos + length + 1
    if end > len(b) or b[end - 1] != 0:
        raise NotUnderstood('reference path at %d is not NUL terminated' % pos)
    raw = b[pos:pos + length]
    if any(c < 32 or c > 126 for c in raw):
        raise NotUnderstood('reference path at %d is not text' % pos)
    return raw.decode('ascii'), end


def walk(b, schema, base, pos, parent_bitmap, found):
    """Reads the data that follows the struct at base (starting at pos); appends (placement offset, mode, bitmap
    path) to found; returns the position after the struct's data."""
    paths = {}
    for item in schema.items:
        if isinstance(item, Dep):
            if base + item.off + 16 > len(b):
                raise NotUnderstood('reference outside the tag')
            paths[item.off], pos = read_path(b, pos, u32(b, base + item.off + 8))
        else:
            count = u32(b, base + item.off)
            if count > 0x10000:
                raise NotUnderstood('implausible block count %d' % count)
            if count and item.elem is None:
                raise NotUnderstood('non-empty block at 0x%X' % item.off)
            if count:
                start = pos
                pos += count * item.elem.size
                if pos > len(b):
                    raise NotUnderstood('block data outside the tag')
                item_bitmap = paths.get(schema.item_bitmap) if schema.item_bitmap is not None else None
                # the children of each element follow all of the block's elements
                for n in range(count):
                    pos = walk(b, item.elem, start + n * item.elem.size, pos, item_bitmap, found)
    for off, mode, ref in schema.placements:
        if ref == 'parent':
            bitmap = parent_bitmap
        elif ref is None:
            bitmap = None
        else:
            bitmap = paths[ref]
        found.append((base + off, mode, bitmap))
    return pos


def understand(b, group):
    """The placements of a HUD tag as (offset, mode, bitmap path) tuples."""
    schema = SCHEMAS[group]
    if len(b) < HEAD + schema.size or b[0x3C:0x40] != b'blam' or b[0x24:0x28] != group:
        raise NotUnderstood('not a %s tag of the expected size' % group.decode())
    found = []
    end = walk(b, schema, HEAD, HEAD + schema.size, None, found)
    if end > len(b):
        raise NotUnderstood('data runs past the end of the tag')
    return found


def scale_tag(b, group, half_of):
    """Returns (new bytes, placements scaled, placements offset-only). half_of(path) -> True/False, or None when the
    bitmap cannot be found. Raises NotUnderstood."""
    placements = understand(b, group)
    out = bytearray(b)
    full = 0
    for off, mode, bitmap in placements:
        if mode == 'full':
            half = False
            if bitmap:
                half = half_of(bitmap)
                if half is None:
                    raise NotUnderstood('bitmap %s not found' % bitmap)
            factor = 0.25 if half else 0.5
            w, h = struct.unpack_from('>ff', out, off + 4)
            struct.pack_into('>ff', out, off + 4, w * factor, h * factor)
            full += 1
        flags = struct.unpack_from('>H', out, off + 12)[0]
        if not flags & FLAG_HIGH_RES:
            x, y = struct.unpack_from('>hh', out, off)
            struct.pack_into('>hh', out, off, int(x / 2), int(y / 2))
        struct.pack_into('>H', out, off + 12, flags & ~FLAG_HIGH_RES & 0xFFFF)
    return bytes(out), full, len(placements) - full


class Bitmaps:
    """Bitmap tags by path: the tags tree first, then the stock tree; case does not matter."""

    def __init__(self, *roots):
        self.index = {}
        for root in reversed([r for r in roots if r]):
            for p in glob.glob(os.path.join(root, '**', '*.bitmap'), recursive=True):
                rel = os.path.relpath(p, root)[:-len('.bitmap')].replace(os.sep, '/').lower()
                self.index[rel] = p
        self.cache = {}

    def half(self, path):
        key = path.replace('\\', '/').lower()
        if key not in self.cache:
            p = self.index.get(key)
            value = None
            if p:
                with open(p, 'rb') as f:
                    head = f.read(HEAD + 8)
                if len(head) == HEAD + 8:
                    value = bool(struct.unpack_from('>H', head, HEAD + 6)[0] & (0x10 | 0x80))
            self.cache[key] = value
        return self.cache[key]


def run(tagsdir, stockdir=None):
    bitmaps = Bitmaps(tagsdir, stockdir)
    done = 0
    for ext, group in GROUPS.items():
        for p in sorted(glob.glob(os.path.join(tagsdir, '**', '*.' + ext), recursive=True)):
            rel = os.path.relpath(p, tagsdir)
            with open(p, 'rb') as f:
                data = f.read()
            if stockdir:
                sp = os.path.join(stockdir, rel)
                if os.path.isfile(sp) and open(sp, 'rb').read() == data:
                    continue
            try:
                new, full, offsets = scale_tag(data, group, bitmaps.half)
            except NotUnderstood as e:
                print('layout not understood, left alone', rel, '(%s)' % e)
                continue
            if new != data:
                with open(p, 'wb') as f:
                    f.write(new)
                print('hud scaled', rel, full, 'placements,', offsets, 'offset-only')
                done += 1
    print('hud tags scaled', done)


if __name__ == '__main__':
    run(sys.argv[1], sys.argv[2] if len(sys.argv) > 2 else os.environ.get('STOCK_TAGS'))
