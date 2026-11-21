# halve.py <tagsdir> <min_width> [prefix-exclude...]
# Drops mip 0 of big 32-bit 2D bitmaps (needed when a map exceeds the engine's cache size cap).
# Each bitmap entry is read by its own pixel data offset; the entries' offsets are rewritten after the cut.
# A bitmap whose entries do not tile its pixel data exactly as their sizes say is left alone and reported.
import glob
import os
import struct
import sys

HEAD = 64
INTERFACE_BITMAPS = 4  # the bitmap group type of interface bitmaps, which keep their size
ENTRY = 48
BYTES_PER_PIXEL = {0: 1, 1: 1, 2: 1, 3: 2, 6: 2, 8: 2, 9: 2, 10: 4, 11: 4, 17: 1}
BLOCK_BYTES = {14: 8, 15: 16, 16: 16, 18: 16}  # dxt1, dxt3, dxt5, bc7


def level_bytes(w, h, d, bitmap_type, fmt, level):
    lw, lh = max(1, w >> level), max(1, h >> level)
    layers = max(1, d >> level) if bitmap_type == 1 else (6 if bitmap_type == 2 else 1)
    if fmt in BLOCK_BYTES:
        return max(1, (lw + 3) // 4) * max(1, (lh + 3) // 4) * BLOCK_BYTES[fmt] * layers
    return lw * lh * BYTES_PER_PIXEL.get(fmt, 0) * layers


def entry_bytes(w, h, d, bitmap_type, fmt, mips):
    return sum(level_bytes(w, h, d, bitmap_type, fmt, l) for l in range(mips + 1))


def run(path, minw):
    """Bytes saved in one bitmap tag, or 0 when it is unchanged; None when its layout is not understood."""
    with open(path, 'rb') as f:
        b = f.read()
    ncc = struct.unpack_from('>I', b, HEAD + 28)[0]
    npx = struct.unpack_from('>I', b, HEAD + 48)[0]
    nseq = struct.unpack_from('>I', b, HEAD + 84)[0]
    nbd = struct.unpack_from('>I', b, HEAD + 96)[0]
    if not nbd or not npx or struct.unpack_from('>H', b, HEAD)[0] == INTERFACE_BITMAPS:
        return 0
    blob = HEAD + 108
    px = b[blob + ncc:blob + ncc + npx]
    arr = blob + ncc + npx
    sprites = sum(struct.unpack_from('>I', b, arr + i * 64 + 52)[0] for i in range(nseq))
    bd = arr + nseq * 64 + sprites * 32
    if bd + nbd * ENTRY != len(b):
        return None
    meta = []
    for i in range(nbd):
        o = bd + i * ENTRY
        w, h, d, t, f, _ = struct.unpack_from('>HHHHHH', b, o + 4)
        mips = struct.unpack_from('>H', b, o + 20)[0]
        off = struct.unpack_from('>I', b, o + 24)[0]
        meta.append([w, h, d, t, f, mips, off])
    pos = 0
    for m in meta:
        if m[6] != pos:
            return None
        pos += entry_bytes(*m[:6])
    if pos != len(px):
        return None
    out = bytearray(b)
    new_px = b''
    saved = 0
    for i, (w, h, d, t, f, mips, off) in enumerate(meta):
        seg = px[off:off + entry_bytes(w, h, d, t, f, mips)]
        o = bd + i * ENTRY
        if t == 0 and f in (10, 11) and w >= minw and h >= minw and mips >= 2:
            n0 = level_bytes(w, h, d, t, f, 0)
            seg = seg[n0:]
            w, h, mips = w // 2, h // 2, mips - 1
            struct.pack_into('>HH', out, o + 4, w, h)
            struct.pack_into('>H', out, o + 20, mips)
            saved += n0
        struct.pack_into('>I', out, o + 24, len(new_px))
        new_px += seg
    if not saved:
        return 0
    struct.pack_into('>I', out, HEAD + 48, len(new_px))
    with open(path, 'wb') as f:
        f.write(bytes(out[:blob + ncc]) + new_px + bytes(out[arr:]))
    return saved


if __name__ == '__main__':
    td = sys.argv[1]
    minw = int(sys.argv[2])
    exclude = sys.argv[3:]
    total = 0
    for p in glob.glob(td + '/**/*.bitmap', recursive=True):
        rel = os.path.relpath(p, td)
        if any(rel.startswith(e) for e in exclude):
            continue
        s = run(p, minw)
        if s is None:
            print('layout not understood, left alone', rel)
        elif s:
            print('halved', rel, s)
            total += s
    print('saved', total / 1048576, 'MiB')
