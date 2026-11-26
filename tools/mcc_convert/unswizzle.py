# unswizzle.py <bitmap tag> ...: deswizzle each swizzled bitmap in the tag and clear its swizzled flag.
# Only uncompressed 2D entries are handled; anything else, or a tag with nothing swizzled, exits non-zero without
# changing the file, so the caller can use the stock tag or stop.
import struct
import sys

HEAD = 64
ENTRY = 48
SWIZZLED = 0x08
BYTES_PER_PIXEL = {0: 1, 1: 1, 2: 1, 3: 2, 6: 2, 8: 2, 9: 2, 10: 4, 11: 4, 17: 1}


def _block(src, dst, mo, width, stride, counter, deswizzle):
    if width == 2:
        if not deswizzle:
            dst[counter] = src[mo]
            dst[counter + 1] = src[mo + 1]
            dst[counter + 2] = src[mo + stride]
            dst[counter + 3] = src[mo + 1 + stride]
        else:
            dst[mo] = src[counter]
            dst[mo + 1] = src[counter + 1]
            dst[mo + stride] = src[counter + 2]
            dst[mo + 1 + stride] = src[counter + 3]
        return counter + 4
    half = width // 2
    for off in (0, half, stride * half, stride * half + half):
        counter = _block(src, dst, mo + off, half, stride, counter, deswizzle)
    return counter


def swizzle_2d(pixels, width, height, deswizzle):
    """The pixels (a list of equal-size items) swizzled, or deswizzled when deswizzle is set; width and height are powers of two."""
    if width <= 2 or height <= 1:
        return list(pixels)
    out = [None] * (width * height)
    if width < height:
        for y in range(0, height, width):
            part = swizzle_2d(pixels[y * width:(y + width) * width], width, width, deswizzle)
            out[y * width:(y + width) * width] = part
        return out
    counter = 0
    for x in range(0, width, height):
        counter = _block(pixels, out, x, height, width, counter, deswizzle)
    return out


def _is_power_of_two(n):
    return n > 0 and n & (n - 1) == 0


def unswizzle(data):
    """The tag bytes with every swizzled entry deswizzled and its flag cleared, and the number of entries changed."""
    b = bytearray(data)
    ncc = struct.unpack_from('>I', b, HEAD + 28)[0]
    npx = struct.unpack_from('>I', b, HEAD + 48)[0]
    nseq = struct.unpack_from('>I', b, HEAD + 84)[0]
    nbd = struct.unpack_from('>I', b, HEAD + 96)[0]
    blob = HEAD + 108
    px0 = blob + ncc
    arr = px0 + npx
    sprites = sum(struct.unpack_from('>I', b, arr + i * 64 + 52)[0] for i in range(nseq))
    bd = arr + nseq * 64 + sprites * 32
    if bd + nbd * ENTRY != len(b):
        raise ValueError('layout not understood')
    changed = 0
    for i in range(nbd):
        o = bd + i * ENTRY
        w, h, d, t, f, flags = struct.unpack_from('>HHHHHH', b, o + 4)
        if not flags & SWIZZLED:
            continue
        mips = struct.unpack_from('>H', b, o + 20)[0]
        off = struct.unpack_from('>I', b, o + 24)[0]
        bpp = BYTES_PER_PIXEL.get(f)
        if t != 0 or bpp is None or d != 1:
            raise ValueError('swizzled entry %d is not an uncompressed 2D bitmap' % i)
        pos = px0 + off
        for level in range(mips + 1):
            lw, lh = max(1, w >> level), max(1, h >> level)
            if not (_is_power_of_two(lw) and _is_power_of_two(lh)):
                raise ValueError('swizzled entry %d has a non-power-of-two size' % i)
            size = lw * lh * bpp
            if pos + size > arr:
                raise ValueError('swizzled entry %d runs past its pixel data' % i)
            items = [bytes(b[pos + k * bpp:pos + (k + 1) * bpp]) for k in range(lw * lh)]
            out = swizzle_2d(items, lw, lh, True)
            b[pos:pos + size] = b''.join(out)
            pos += size
        struct.pack_into('>H', b, o + 14, flags & ~SWIZZLED)
        changed += 1
    return bytes(b), changed


if __name__ == '__main__':
    status = 0
    for path in sys.argv[1:]:
        with open(path, 'rb') as f:
            original = f.read()
        try:
            out, n = unswizzle(original)
        except ValueError as e:
            print('unswizzle failed', path, e)
            status = 1
            continue
        if n == 0:
            print('unswizzle failed', path, 'nothing swizzled')
            status = 1
            continue
        with open(path, 'wb') as f:
            f.write(out)
        print('unswizzled', path, n)
    sys.exit(status)
