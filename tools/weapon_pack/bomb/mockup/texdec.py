"""decode the first bitmap (mip 0) of a HEK .bitmap tag to an RGBA numpy array"""
import os, sys, struct
import numpy as np
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
from hek import BitmapTag

def _565(c):
    r = ((c >> 11) & 31) * 255 // 31; g = ((c >> 5) & 63) * 255 // 63; b = (c & 31) * 255 // 31
    return np.stack([r, g, b], -1).astype(np.float32)

def _dxt_colour(blk, w, h, dxt1):
    # blk: (n, 8) uint8 colour blocks
    c0 = blk[:, 0].astype(np.int32) | (blk[:, 1].astype(np.int32) << 8)
    c1 = blk[:, 2].astype(np.int32) | (blk[:, 3].astype(np.int32) << 8)
    idx = (blk[:, 4].astype(np.uint32) | (blk[:, 5].astype(np.uint32) << 8) |
           (blk[:, 6].astype(np.uint32) << 16) | (blk[:, 7].astype(np.uint32) << 24))
    a, b = _565(c0), _565(c1)
    four = (c0 > c1) | (not dxt1)
    p2 = np.where(four[:, None], (2 * a + b) / 3, (a + b) / 2)
    p3 = np.where(four[:, None], (a + 2 * b) / 3, 0)
    pal = np.stack([a, b, p2, p3], 1)  # n,4,3
    alpha = np.ones((len(blk), 4), np.float32) * 255
    if dxt1: alpha[:, 3] = np.where(four, 255, 0)
    sel = np.stack([(idx >> (2 * k)) & 3 for k in range(16)], 1).astype(np.int64)  # n,16
    rgb = np.take_along_axis(pal, sel[:, :, None].repeat(3, 2), 1)
    al = np.take_along_axis(alpha, sel, 1)
    return rgb, al

def _blocks_to_img(px, w, h):
    bw, bh = max(1, (w + 3) // 4), max(1, (h + 3) // 4)
    img = px.reshape(bh, bw, 4, 4, -1).transpose(0, 2, 1, 3, 4).reshape(bh * 4, bw * 4, -1)
    return img[:h, :w]

def decode(path):
    tag = open(path, 'rb').read()
    lay = BitmapTag(tag)
    bm = lay.bitmaps[0]
    w, h, f = bm.width, bm.height, bm.format
    off = lay.pixels_start + (bm.pixels_offset if lay.bitmap_count > 1 else 0)
    if f in (14, 15, 16):
        bw, bh = max(1, (w + 3) // 4), max(1, (h + 3) // 4); n = bw * bh
        bs = 8 if f == 14 else 16
        raw = np.frombuffer(tag, np.uint8, n * bs, off).reshape(n, bs)
        if f == 14:
            rgb, al = _dxt_colour(raw, w, h, True)
        else:
            rgb, _ = _dxt_colour(raw[:, 8:], w, h, False)
            if f == 15:
                ab = raw[:, :8]
                al = np.stack([((ab[:, k // 2] >> (4 * (k % 2))) & 15) * 17 for k in range(16)], 1).astype(np.float32)
            else:
                a0 = raw[:, 0].astype(np.float32); a1 = raw[:, 1].astype(np.float32)
                bits = np.zeros(n, np.uint64)
                for k in range(6): bits |= raw[:, 2 + k].astype(np.uint64) << np.uint64(8 * k)
                sel = np.stack([(bits >> np.uint64(3 * k)) & np.uint64(7) for k in range(16)], 1).astype(np.int64)
                pal = np.zeros((n, 8), np.float32); pal[:, 0] = a0; pal[:, 1] = a1
                big = a0 > a1
                for k in range(1, 7):
                    pal[:, 1 + k] = np.where(big, ((7 - k) * a0 + k * a1) / 7, 0)
                for k in range(1, 5):
                    pal[:, 1 + k] = np.where(big, pal[:, 1 + k], ((5 - k) * a0 + k * a1) / 5)
                pal[:, 6] = np.where(big, pal[:, 6], 0); pal[:, 7] = np.where(big, pal[:, 7], 255)
                al = np.take_along_axis(pal, sel, 1)
        px = np.concatenate([rgb, al[:, :, None]], 2)
        return _blocks_to_img(px, w, h) / 255.0
    bpp = {0: 1, 1: 1, 2: 1, 3: 2, 6: 2, 8: 2, 9: 2, 10: 4, 11: 4, 17: 1}[f]
    raw = np.frombuffer(tag, np.uint8, w * h * bpp, off).reshape(h, w, bpp).astype(np.float32)
    if f in (10, 11):
        out = raw[:, :, [2, 1, 0, 3]].copy()
        if f == 10: out[:, :, 3] = 255
    elif f == 0:
        out = np.concatenate([np.full((h, w, 3), 255, np.float32), raw], 2)
    elif f == 1:
        out = np.concatenate([raw.repeat(3, 2), np.full((h, w, 1), 255, np.float32)], 2)
    elif f == 2:
        out = raw.repeat(4, 2)
    elif f == 3:
        out = np.concatenate([raw[:, :, :1].repeat(3, 2), raw[:, :, 1:2]], 2)
    elif f == 6:
        v = raw[:, :, 0].astype(np.int32) | (raw[:, :, 1].astype(np.int32) << 8)
        out = np.concatenate([_565(v), np.full((h, w, 1), 255, np.float32)], 2)
    else:
        raise ValueError('format %d not decoded' % f)
    return out / 255.0

if __name__ == '__main__':
    from PIL import Image
    for p in sys.argv[1:-1]:
        t = decode(p)
        print(p, t.shape)
    t = decode(sys.argv[1])
    Image.fromarray((t * 255).astype(np.uint8), 'RGBA').save(sys.argv[-1])
