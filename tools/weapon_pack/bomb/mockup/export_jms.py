"""E1-S (owner's pick, timer 90) to a JMS + one texture per material + shaders.json, for Invader on the box.

Prototype route (plan 2026-10-28-ae-snd-bomb.md STEP 8): the in-game model is the mockup's geometry, one node,
origin at its bounds' centre (it replaces the oddball ball, whose origin is its centre). Albedo is baked as the
mockup renderer computes it (tex * tint, desaturated, or the flat colour); the detail map goes to the shader.
JMS units are world units x 100. Output: the work folder

Timer animation (owner 10-28): the 4 timer bars are their own strips just in front of the face (shaders bomb_bar_a..d,
left to right) and the beacon's bulb its own shader (bomb_beacon): the game sets their self-illumination colour each
tick (port/linux/game/ae_bomb_pack.c). shaders.json gives each its "role".

usage: python3 export_jms.py [rot_deg=90]
"""
import sys, os, json, hashlib
import numpy as np
from PIL import Image
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import e1s                    # noqa: E402
import art as A               # noqa: E402
import variants as V          # noqa: E402

OUT = os.environ['AE_BOMB_DATA'].rstrip('/') + '/ae_pack/bomb/'    # the folder Invader reads (data/)
TAG = 'ae_pack\\bomb\\'


# the digsite timer face's bars: texture columns (of 64), left to right; full height
BAR_COLUMNS = (12, 28, 44, 60)
BAR_OFFSET = 0.001              # in front of the face (wu): 0.0003 z-fought the face at a few wu (owner 10-28)


def is_timer_face(m):
    t = m['tex']
    return t is not None and t.shape == V.TIMER.shape and np.allclose(t, V.TIMER)


def bar_strips(P, N, UV):
    """the face quad's 4 bar strips: (P, N, UV, T) each, UV over the whole strip"""
    o = np.asarray(P[0], float)
    # the face quad's corners: UV (0,0) (1,0) (1,1) (0,1) as engine.quad makes it
    i0 = int(np.argmin(np.abs(UV - (0, 0)).sum(1)))
    i1 = int(np.argmin(np.abs(UV - (1, 0)).sum(1)))
    i3 = int(np.argmin(np.abs(UV - (0, 1)).sum(1)))
    o, du, dv = P[i0], P[i1] - P[i0], P[i3] - P[i0]
    n = N[i0] / np.linalg.norm(N[i0])
    out = []
    for c in BAR_COLUMNS:
        u0, u1 = c / 64.0, (c + 4) / 64.0
        Q = np.array([o + du * u0, o + du * u1, o + du * u1 + dv, o + du * u0 + dv]) + n * BAR_OFFSET
        out.append((Q, np.tile(n, (4, 1)), np.array([(0, 0), (1, 0), (1, 1), (0, 1)], float),
                    np.array([(0, 1, 2), (0, 2, 3)])))
    return out


def pow2(n):
    p = 4
    while p < n and p < 512:
        p *= 2
    return p


def save_png(arr, path, alpha=False):
    # (alpha: RGBA, else the colour channels only: Pillow reads an RGBA array as 'RGB' byte by byte, scrambled)
    a = np.clip(arr, 0, 1)
    a = np.ascontiguousarray(a if alpha else a[..., :3])
    h, w = a.shape[:2]
    im = Image.fromarray((a * 255 + 0.5).astype(np.uint8))
    im = im.resize((pow2(w), pow2(h)), Image.LANCZOS)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    im.save(path)


def baked(m):
    """(albedo RGBA float array, alpha_test) as the mockup renderer shades it, before light"""
    if m['tex'] is not None:
        t = m['tex'].astype(np.float32)
        c = t[..., :3]
        if m['desat']:
            g = c.mean(-1, keepdims=True)
            c = c * (1 - m['desat']) + g * m['desat']
        c = c * m['tint']
        return np.concatenate([c, t[..., 3:4]], -1), bool(m['alpha_test'])
    c = np.ones((8, 8, 4), np.float32)
    c[..., :3] = m['color']
    return c, False


def main(rot_deg=90):
    A.setup()
    a, info = e1s.E1S('hero', rot_deg)
    lo, hi = a.bounds()
    centre = (lo + hi) / 2
    shaders, by_key, details = [], {}, {}
    V, F = [], []
    items = []
    for P, N, UV, T, m, cast, tag, grp in a.items:
        # (the timer's glass: the mockup draws it see-through; in game a shader_model is opaque and hid the face and
        # its bars, owner 10-28 "can't see animations": left out)
        if isinstance(m, dict) and m.get('glass') is not None:
            continue
        role = None
        if isinstance(m, dict) and tag == 'beacon' and m['emissive'] is not None:
            # (dark: the beacon off; the game swaps in the LED's red to flash it, ae_bomb_pack.c)
            role = 'beacon'
            m = dict(m, color=np.array((0.10, 0.03, 0.02), np.float32), emissive=np.array((0.10, 0.03, 0.02), np.float32))
        elif isinstance(m, dict) and tag == 'led':
            # (the LED's red, also the bars' and the beacon's lit red: a fixed name the game finds)
            role = 'led'
        face = isinstance(m, dict) and is_timer_face(m)
        if face:
            # (the face's own bars painted out: the strips in front of it are the bars, and where they z-fought the
            # face, its green and red showed through)
            t = m['tex'].copy()
            for c in BAR_COLUMNS:
                t[:, c:c + 4, :3] = 0.03
            m = dict(m, tex=t)
            # (engine.quad's normal is u x v: the face's points into the console. The mockup draws both sides; the
            # game culls it from the front, and the face and its bars were never seen, owner 10-28 "none present in
            # any state": turned outward, away from the bomb's middle)
            N = np.asarray(N, float)
            if np.dot(N[0], np.asarray(P, float).mean(0) - centre) < 0:
                N = -N
        items.append((P, N, UV, T, m, tag, role))
        if face:
            # (green: carried and loose; the game swaps in the LED's red as the fuse runs, ae_bomb_pack.c)
            bar = dict(m, tex=None, color=np.array((0.10, 1.0, 0.20), np.float32),
                       emissive=np.array((0.10, 1.0, 0.20), np.float32), detail=None, alpha_test=False)
            for k, (Pb, Nb, UVb, Tb) in enumerate(bar_strips(np.asarray(P), np.asarray(N), np.asarray(UV))):
                items.append((Pb, Nb, UVb, Tb, dict(bar, tile=np.array((1, 1))), 'timer', 'bar_' + 'abcd'[k]))
    roles = {}
    for P, N, UV, T, m, tag, role in items:
        if isinstance(m, int):
            raise SystemExit('integer material in the build: not expected')
        alb, at = baked(m)
        emissive = m['emissive'] is not None
        if emissive and m['tex'] is None and not role:
            alb[..., :3] = np.clip(m['emissive'], 0, 1)
        dkey = None
        if m['detail'] is not None:
            dkey = hashlib.sha1(np.ascontiguousarray(m['detail']).tobytes()).hexdigest()[:10]
            if dkey not in details:
                details[dkey] = 'detail_' + 'abcdefgh'[len(details)]
                save_png(m['detail'][..., :3], OUT + 'bitmaps/%s.png' % details[dkey])
        key = (hashlib.sha1(np.ascontiguousarray(alb).tobytes()).hexdigest()[:12], at, emissive, dkey,
               tuple(np.round(m['detail_tile'], 3)) if dkey else None, role)
        if key not in by_key:
            plain = sum(1 for x in shaders if not x['role'])
            name = 'bomb_' + (role or 'abcdefghijklmnopqrstuvwxyz'[plain])
            by_key[key] = len(shaders)
            save_png(alb, OUT + 'bitmaps/%s.png' % name, alpha=at)
            shaders.append(dict(name=name, bitmap=TAG + 'bitmaps\\' + name, alpha_test=at, emissive=emissive,
                                tag=tag, detail=(TAG + 'bitmaps\\' + details[dkey]) if dkey else None,
                                detail_scale=float(np.mean(m['detail_tile'])) if dkey else 0.0,
                                detail_amt=float(m['detail_amt']) if dkey else 0.0, role=role))
        si = by_key[key]
        base = len(V)
        uv = UV * m['tile']
        for p, n, t in zip(P, N, uv):
            V.append(((p - centre) * 100.0, n, t))
        # every triangle wound counter-clockwise seen from outside (its normals' side), as the game draws a JMS: the
        # stock gbx parts come the other way round, and the box builder mixes both
        T = np.asarray(T)
        c = np.cross(P[T[:, 1]] - P[T[:, 0]], P[T[:, 2]] - P[T[:, 0]])
        out = (c * N[T].mean(1)).sum(1) >= 0
        for tri, ok in zip(T, out):
            F.append((si, base + tri[0], base + tri[1], base + tri[2]) if ok else
                     (si, base + tri[0], base + tri[2], base + tri[1]))
    os.makedirs(OUT + 'models', exist_ok=True)
    L = ['8200', '3251', '1', 'frame', '-1', '-1', '0 0 0 1', '0 0 0', str(len(shaders))]
    for s in shaders:
        L += [s['name'], '<none>']
    L += ['0', '1', 'unnamed', str(len(V))]
    for p, n, t in V:
        L += ['0', '%.6f %.6f %.6f' % tuple(p), '%.6f %.6f %.6f' % tuple(n), '-1', '0',
              '%.6f %.6f' % (t[0], 1.0 - t[1]), '0']
    L += [str(len(F))]
    for si, i0, i1, i2 in F:
        L += ['0', str(si), '%d %d %d' % (i0, i1, i2)]
    open(OUT + 'models/bomb.jms', 'w').write('\n'.join(L) + '\n')
    json.dump(dict(shaders=shaders, centre=centre.tolist(), size=(hi - lo).tolist(), triangles=len(F),
                   vertices=len(V)), open(OUT + 'shaders.json', 'w'), indent=1)
    print('shaders', len(shaders), 'details', len(details), 'verts', len(V), 'tris', len(F),
          'size wu', np.round(hi - lo, 4))


if __name__ == '__main__':
    main(int(sys.argv[1]) if len(sys.argv) > 1 else 90)
