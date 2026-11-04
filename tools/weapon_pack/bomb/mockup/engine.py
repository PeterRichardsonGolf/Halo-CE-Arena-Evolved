"""Small numpy scene renderer for the SnD bomb mockups.

Loads CE gbxmodels (per-shader parts with UVs), places/scales/rotates them, adds generated parts (boxes, tubes,
ribbons, textured quads) and renders with a perspective camera, a 3-point light, a key-light shadow map, a soft
top-down contact shadow, emissive parts with bloom, an optional point light, and a dark backdrop with a ground disc.
Units: Halo world units (1 wu = 3.048 m), z up.
"""
import sys, os, math, re, struct
import numpy as np
from PIL import Image
from scipy.ndimage import gaussian_filter

sys.path.insert(0, os.path.dirname(__file__))
from gbx import read_gbx          # noqa: E402
from render import strip_tris     # noqa: E402
import texdec                     # noqa: E402

TAGS = os.environ['AE_BOMB_HEK_TAGS'].rstrip('/') + '/'    # the official HEK's tags (the build's input)
WU = 3.048  # metres per world unit

# ---------------------------------------------------------------- loading

_tex_cache = {}


def tex(path):
    """decoded bitmap tag (path relative to tags, no extension) as float RGBA array"""
    if path not in _tex_cache:
        _tex_cache[path] = texdec.decode((path if path.startswith('/') else TAGS + path) + '.bitmap').astype(np.float32)
    return _tex_cache[path]


def load_gbx(path):
    """parts of a gbxmodel (first permutation, highest LOD): list of (shader index, P, N, UV, T), plus the
    model's shader tag paths"""
    full = (path if path.startswith('/') else TAGS + path) + '.gbxmodel'
    b = open(full, 'rb').read()
    us, vs = struct.unpack_from('>2f', b, 0x40 + 48)
    us = us or 1.0
    vs = vs or 1.0
    shaders = [s.decode('latin1').rstrip('\0') for s in re.findall(rb'[A-Za-z0-9_ \-\\]{4,}\x00', b) if b'\\' in s]
    regions, geoms = read_gbx(full)
    parts = []
    for rn, perms in regions:
        if not perms:
            continue
        pn, lods = perms[0]
        gi = lods[4] if lods[4] >= 0 else 0
        for shader, verts, tris in geoms[gi]:
            if not verts:
                continue
            v = np.array(verts, dtype=np.float64)
            tt = strip_tris(tris)
            if not tt:
                continue
            uv = v[:, 12:14] * np.array([us, vs])
            parts.append((shader, v[:, 0:3].copy(), v[:, 3:6].copy(), uv, np.array(tt)))
    return parts, shaders


# ---------------------------------------------------------------- transforms

def rot(axis, deg):
    a = math.radians(deg)
    c, s = math.cos(a), math.sin(a)
    if axis == 'x':
        return np.array([[1, 0, 0], [0, c, -s], [0, s, c]])
    if axis == 'y':
        return np.array([[c, 0, s], [0, 1, 0], [-s, 0, c]])
    return np.array([[c, -s, 0], [s, c, 0], [0, 0, 1]])


def rots(*pairs):
    """rots(('z', 30), ('x', 10)) applies z first, then x"""
    R = np.eye(3)
    for ax, d in pairs:
        R = rot(ax, d) @ R
    return R


class Xf:
    """p' = R (s * p) + t"""

    def __init__(self, s=1.0, R=None, t=(0, 0, 0)):
        self.s = np.array(s, dtype=np.float64) * np.ones(3)
        self.R = np.eye(3) if R is None else np.asarray(R, dtype=np.float64)
        self.t = np.asarray(t, dtype=np.float64)

    def p(self, P):
        return (P * self.s) @ self.R.T + self.t

    def n(self, N):
        N2 = (N / self.s) @ self.R.T
        ln = np.linalg.norm(N2, axis=1, keepdims=True)
        ln[ln == 0] = 1
        return N2 / ln

    def then(self, other):
        """this transform followed by other (uniform scale only for other)"""
        o = Xf()
        o.s = self.s * other.s
        o.R = other.R @ self.R
        o.t = other.p(self.t[None])[0]
        return o


# ---------------------------------------------------------------- materials

def mat(color=(0.6, 0.6, 0.6), tex=None, tint=(1, 1, 1), tile=(1, 1), spec=0.15, gloss=24.0, emissive=None,
        emis_tex=None, emis_gain=1.0, alpha_test=False, desat=0.0, tex_fn=None, glass=None, detail=None,
        detail_tile=(4, 4), detail_amt=1.0, normal=None, normal_tile=(1, 1), normal_amt=1.0, wear=0.0, chip=None,
        chip_tile=(1, 1), chip_thr=0.45):
    return dict(color=np.array(color, np.float32), tex=tex, tint=np.array(tint, np.float32), tile=np.array(tile),
                spec=spec, gloss=gloss, emissive=None if emissive is None else np.array(emissive, np.float32),
                emis_tex=emis_tex, emis_gain=emis_gain, alpha_test=alpha_test, desat=desat,
                glass=glass, detail=detail, detail_tile=np.array(detail_tile, float), detail_amt=detail_amt,
                normal=normal, normal_tile=np.array(normal_tile, float), normal_amt=normal_amt, wear=wear,
                chip=chip, chip_tile=np.array(chip_tile, float), chip_thr=chip_thr)


def sample(T, uv):
    """bilinear, wrapped"""
    h, w = T.shape[:2]
    x = uv[:, 0] * w - 0.5
    y = uv[:, 1] * h - 0.5
    x0 = np.floor(x).astype(np.int64)
    y0 = np.floor(y).astype(np.int64)
    fx = (x - x0)[:, None].astype(np.float32)
    fy = (y - y0)[:, None].astype(np.float32)
    x0m, x1m = x0 % w, (x0 + 1) % w
    y0m, y1m = y0 % h, (y0 + 1) % h
    a = T[y0m, x0m] * (1 - fx) + T[y0m, x1m] * fx
    b = T[y1m, x0m] * (1 - fx) + T[y1m, x1m] * fx
    return a * (1 - fy) + b * fy


# ---------------------------------------------------------------- scene

class Scene:
    def __init__(self):
        self.P = []
        self.N = []
        self.UV = []
        self.T = []
        self.M = []        # per-triangle material index
        self.mats = []
        self.cast = []     # per-triangle: casts shadow
        self.nv = 0
        self.point_lights = []
        self.sprites = []      # (pos, size in wu, rgb, texture): camera-facing additive glow (lens flare bitmaps)

    def add_mat(self, m):
        self.mats.append(m)
        return len(self.mats) - 1

    def add(self, P, N, UV, T, m, xf=None, cast=True):
        if xf is not None:
            P = xf.p(P)
            N = xf.n(N) if N is not None else None
        if N is None:
            N = vnormals(P, T)
        mi = m if isinstance(m, int) else self.add_mat(m)
        self.P.append(P)
        self.N.append(N)
        self.UV.append(UV if UV is not None else np.zeros((len(P), 2)))
        self.T.append(np.asarray(T) + self.nv)
        self.M.append(np.full(len(T), mi))
        self.cast.append(np.full(len(T), cast))
        self.nv += len(P)
        return mi

    def add_gbx(self, path, xf, mats, cast=True, skip=()):
        """mats: dict shader index -> material (dict or index); missing index -> mats['default']"""
        parts, shaders = load_gbx(path)
        cache = {}
        for sh, P, N, UV, T in parts:
            if sh in skip:
                continue
            m = mats.get(sh, mats.get('default'))
            if m is None:
                continue
            key = id(m)
            if key not in cache:
                cache[key] = m if isinstance(m, int) else self.add_mat(m)
            self.add(P, N, UV, T, cache[key], xf, cast)
        return shaders

    def bounds(self, only_cast=False):
        P = np.vstack(self.P)
        return P.min(0), P.max(0)


def vnormals(P, T):
    T = np.asarray(T)
    N = np.zeros_like(P)
    fn = np.cross(P[T[:, 1]] - P[T[:, 0]], P[T[:, 2]] - P[T[:, 0]])
    for k in range(3):
        np.add.at(N, T[:, k], fn)
    ln = np.linalg.norm(N, axis=1, keepdims=True)
    ln[ln == 0] = 1
    return N / ln


# ---------------------------------------------------------------- generated parts

def box(size, center=(0, 0, 0)):
    """axis-aligned box, flat-shaded, per-face 0..1 UVs (u along first in-face axis, v along second)"""
    sx, sy, sz = np.array(size) / 2
    cx, cy, cz = center
    faces = [  # normal, u axis, v axis
        ((1, 0, 0), (0, -1, 0), (0, 0, -1)), ((-1, 0, 0), (0, 1, 0), (0, 0, -1)),
        ((0, 1, 0), (1, 0, 0), (0, 0, -1)), ((0, -1, 0), (-1, 0, 0), (0, 0, -1)),
        ((0, 0, 1), (0, 1, 0), (1, 0, 0)), ((0, 0, -1), (0, -1, 0), (1, 0, 0))]
    P, N, UV, T = [], [], [], []
    half = np.array([sx, sy, sz])
    for n, u, v in faces:
        n, u, v = np.array(n, float), np.array(u, float), np.array(v, float)
        base = len(P)
        for a, b in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
            p = (n + a * u + b * v) * half + np.array([cx, cy, cz])
            P.append(p)
            N.append(n)
            UV.append(((a + 1) / 2, (b + 1) / 2))
        T += [(base, base + 1, base + 2), (base, base + 2, base + 3)]
    return np.array(P), np.array(N), np.array(UV), np.array(T)


def quad(center, uvec, vvec, flip_v=False):
    """quad spanning center +- uvec/2 +- vvec/2; normal = u x v; UV (0,0) at -u,-v"""
    c, u, v = np.array(center, float), np.array(uvec, float), np.array(vvec, float)
    n = np.cross(u, v)
    n /= np.linalg.norm(n)
    P = np.array([c - u / 2 - v / 2, c + u / 2 - v / 2, c + u / 2 + v / 2, c - u / 2 + v / 2])
    UV = np.array([(0, 0), (1, 0), (1, 1), (0, 1)], float)
    if flip_v:
        UV[:, 1] = 1 - UV[:, 1]
    return P, np.tile(n, (4, 1)), UV, np.array([(0, 1, 2), (0, 2, 3)])


def spline(ctrl, n=12):
    """Catmull-Rom through control points"""
    c = np.array(ctrl, float)
    c = np.vstack([2 * c[0] - c[1], c, 2 * c[-1] - c[-2]])
    out = []
    for i in range(1, len(c) - 2):
        p0, p1, p2, p3 = c[i - 1], c[i], c[i + 1], c[i + 2]
        for t in np.linspace(0, 1, n, endpoint=False):
            t2, t3 = t * t, t * t * t
            out.append(0.5 * ((2 * p1) + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t2 +
                              (-p0 + 3 * p1 - 3 * p2 + p3) * t3))
    out.append(c[-2])
    return np.array(out)


def sweep(points, profile, side=None, closed=False, smooth=True):
    """sweep a 2D profile (k,2) along points. Frames: if side given (fixed vector), profile x = side,
    profile y = tangent x side; else parallel transport. Returns P, N, UV, T"""
    pts = np.array(points, float)
    m = len(pts)
    tang = np.gradient(pts, axis=0) if not closed else (np.roll(pts, -1, 0) - np.roll(pts, 1, 0))
    tang /= np.linalg.norm(tang, axis=1, keepdims=True)
    frames = []
    if side is not None:
        s = np.array(side, float)
        for t in tang:
            x = s - np.dot(s, t) * t
            x /= np.linalg.norm(x)
            frames.append((x, np.cross(t, x)))
    else:
        ref = np.array([0, 0, 1.0]) if abs(tang[0][2]) < 0.9 else np.array([1.0, 0, 0])
        x = np.cross(tang[0], ref)
        x /= np.linalg.norm(x)
        for t in tang:
            x = x - np.dot(x, t) * t
            x /= np.linalg.norm(x)
            frames.append((x, np.cross(t, x)))
    prof = np.array(profile, float)
    k = len(prof)
    # profile normals (outward, for a convex profile centred on 0)
    pn = prof / np.maximum(np.linalg.norm(prof, axis=1, keepdims=True), 1e-9)
    P, N, UV, T = [], [], [], []
    for i in range(m):
        x, y = frames[i]
        for j in range(k):
            P.append(pts[i] + prof[j, 0] * x + prof[j, 1] * y)
            N.append(pn[j, 0] * x + pn[j, 1] * y)
            UV.append((j / k, i / max(1, m - 1)))
    rings = m if closed else m - 1
    for i in range(rings):
        i2 = (i + 1) % m
        for j in range(k):
            j2 = (j + 1) % k
            a, b, c, d = i * k + j, i * k + j2, i2 * k + j2, i2 * k + j
            T += [(a, b, c), (a, c, d)]
    P = np.array(P)
    N = np.array(N)
    if not smooth:
        N = None
    return P, N, np.array(UV), np.array(T)


def circle(r, k=10):
    a = np.linspace(0, 2 * math.pi, k, endpoint=False)
    return np.stack([np.cos(a) * r, np.sin(a) * r], 1)


def rect(w, h):
    """rectangle profile with rounded-ish 8 points so normals look flat enough"""
    return np.array([(w / 2, h / 2), (-w / 2, h / 2), (-w / 2, -h / 2), (w / 2, -h / 2)])


def wire(ctrl, r, n=10):
    return sweep(spline(ctrl, n), circle(r, 8))


def sphere(r, center, k=16):
    th = np.linspace(0, math.pi, k // 2 + 1)
    ph = np.linspace(0, 2 * math.pi, k, endpoint=False)
    P, T = [], []
    for t in th:
        for p in ph:
            P.append((math.sin(t) * math.cos(p), math.sin(t) * math.sin(p), math.cos(t)))
    P = np.array(P)
    for i in range(len(th) - 1):
        for j in range(k):
            a, b = i * k + j, i * k + (j + 1) % k
            c, d = (i + 1) * k + (j + 1) % k, (i + 1) * k + j
            T += [(a, b, c), (a, c, d)]
    return P * r + np.array(center), P.copy(), np.zeros((len(P), 2)), np.array(T)


# ---------------------------------------------------------------- rasteriser

def raster(X, Y, Q, D, tris, W, H, alpha_fn=None):
    """X,Y screen coords per vertex, Q perspective weight (1/z or 1), D depth (smaller = closer).
    Returns depth (H,W), tri id (H,W), bary (H,W,3) perspective-correct."""
    depth = np.full((H, W), np.inf, np.float32)
    tid = np.full((H, W), -1, np.int32)
    bary = np.zeros((H, W, 3), np.float32)
    for ti, (a, b, c) in enumerate(tris):
        xs = X[[a, b, c]]
        ys = Y[[a, b, c]]
        x0, x1 = int(max(0, math.floor(xs.min()))), int(min(W - 1, math.ceil(xs.max())))
        y0, y1 = int(max(0, math.floor(ys.min()))), int(min(H - 1, math.ceil(ys.max())))
        if x1 < x0 or y1 < y0:
            continue
        den = (ys[1] - ys[2]) * (xs[0] - xs[2]) + (xs[2] - xs[1]) * (ys[0] - ys[2])
        if abs(den) < 1e-12:
            continue
        gx, gy = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
        l0 = ((ys[1] - ys[2]) * (gx - xs[2]) + (xs[2] - xs[1]) * (gy - ys[2])) / den
        l1 = ((ys[2] - ys[0]) * (gx - xs[2]) + (xs[0] - xs[2]) * (gy - ys[2])) / den
        l2 = 1 - l0 - l1
        m = (l0 >= -1e-4) & (l1 >= -1e-4) & (l2 >= -1e-4)
        if not m.any():
            continue
        q0, q1, q2 = l0 * Q[a], l1 * Q[b], l2 * Q[c]
        iq = q0 + q1 + q2
        b0, b1, b2 = q0 / iq, q1 / iq, q2 / iq
        z = b0 * D[a] + b1 * D[b] + b2 * D[c]
        sub = depth[y0:y1 + 1, x0:x1 + 1]
        w = m & (z < sub)
        if alpha_fn is not None and w.any():
            keep = alpha_fn(ti, b0[w], b1[w], b2[w])
            if keep is not None:
                w2 = np.zeros_like(w)
                w2[w] = keep
                w = w2
        if not w.any():
            continue
        sub[w] = z[w]
        tid[y0:y1 + 1, x0:x1 + 1][w] = ti
        bb = bary[y0:y1 + 1, x0:x1 + 1]
        bb[w] = np.stack([b0[w], b1[w], b2[w]], 1)
    return depth, tid, bary


def norm(v):
    v = np.asarray(v, float)
    return v / np.linalg.norm(v)


def smoothstep(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0, 1)
    return t * t * (3 - 2 * t)


def render(scene, out, eye, target, fov=30.0, size=(1200, 900), ss=2, key=(-0.55, -0.35, 0.75),
           key_col=(1.0, 0.96, 0.9), fill_col=(0.30, 0.36, 0.48), rim_col=(0.55, 0.6, 0.75), amb=0.20,
           ground=True, ground_r=None, ground_col=(0.20, 0.21, 0.24), bloom=1.0, exposure=1.0,
           bg_top=(0.105, 0.115, 0.14), bg_bot=(0.06, 0.065, 0.08), label=None, return_array=False,
           ground_layers=None, ground_fade=(0.45, 1.0)):
    if os.environ.get('BOMB_PREVIEW'):
        size, ss = (size[0] // 2, size[1] // 2), 1
    W, H = size[0] * ss, size[1] * ss
    P = np.vstack(scene.P).astype(np.float64)
    N = np.vstack(scene.N).astype(np.float64)
    UV = np.vstack(scene.UV).astype(np.float64)
    T = np.vstack(scene.T).astype(np.int64)
    M = np.concatenate(scene.M)
    CAST = np.concatenate(scene.cast)
    mats = scene.mats

    TAN, BIT = uv_tangents(P, UV, T)
    eye = np.array(eye, float)
    target = np.array(target, float)
    f = norm(target - eye)
    r = norm(np.cross(f, [0, 0, 1]))
    u = np.cross(r, f)
    rel = P - eye
    vx, vy, vz = rel @ r, rel @ u, rel @ f
    fpx = (H / 2) / math.tan(math.radians(fov) / 2)
    X = W / 2 + vx / vz * fpx
    Y = H / 2 - vy / vz * fpx
    Q = 1.0 / vz

    # alpha-tested materials
    at_tris = np.array([mats[m]['alpha_test'] for m in M])

    def alpha_fn(ti, b0, b1, b2):
        if not at_tris[ti]:
            return None
        a, b, c = T[ti]
        uv = b0[:, None] * UV[a] + b1[:, None] * UV[b] + b2[:, None] * UV[c]
        m = mats[M[ti]]
        al = sample(m['tex'], uv * m['tile'])[:, 3]
        if m['chip'] is not None:
            ch = sample(m['chip'], uv * m['chip_tile'])[:, :3].mean(1)
            al = al * (ch > m['chip_thr'])
        return al > 0.5

    is_glass = np.array([mats[m]['glass'] is not None for m in M])
    op_idx = np.nonzero(~is_glass)[0]
    gl_idx = np.nonzero(is_glass)[0]
    depth, tid0, bary = raster(X, Y, Q, vz, T[op_idx], W, H, lambda ti, b0, b1, b2: alpha_fn(op_idx[ti], b0, b1, b2))
    tid = np.where(tid0 >= 0, op_idx[np.maximum(tid0, 0)], -1)
    CAST = CAST & ~is_glass

    # ---- camera-relative lights
    def cam_dir(d):
        return norm(d[0] * r + d[1] * f * -1 + d[2] * u) if False else norm(d[0] * r - d[1] * f + d[2] * np.array([0, 0, 1.0]))
    # key: given in camera frame (x right, y toward camera, z up)
    Lk = norm(key[0] * r - key[1] * f + key[2] * np.array([0, 0, 1.0]))
    Lf = norm(0.7 * r - 0.5 * f + 0.25 * np.array([0, 0, 1.0]))
    Lr = norm(0.15 * r + 0.9 * f + 0.6 * np.array([0, 0, 1.0]))

    # ---- shadow map (key, orthographic)
    lmin, lmax = P.min(0), P.max(0)
    ctr = (lmin + lmax) / 2
    rad = np.linalg.norm(lmax - lmin) / 2 * 1.15
    lz = Lk
    lx = norm(np.cross([0, 0, 1.0] if abs(lz[2]) < 0.99 else [1, 0, 0], lz))
    ly = np.cross(lz, lx)
    SM = 2048
    sc = SM / (2 * rad)

    def to_light(p):
        rel = p - ctr
        return (rel @ lx) * sc + SM / 2, SM / 2 - (rel @ ly) * sc, -(rel @ lz)

    cmask = CAST
    lX, lY, lD = to_light(P)
    sdepth, _, _ = raster(lX, lY, np.ones(len(P)), lD, T[cmask], SM, SM)

    # top-down occlusion map for the soft contact shadow
    AO = 512
    aoc = rad * 1.6
    asc = AO / (2 * aoc)
    aX = (P[:, 0] - ctr[0]) * asc + AO / 2
    aY = AO / 2 - (P[:, 1] - ctr[1]) * asc
    adepth, aid, _ = raster(aX, aY, np.ones(len(P)), -P[:, 2], T[cmask], AO, AO)
    occ = (aid >= 0).astype(np.float32)
    hgt = np.where(aid >= 0, -adepth, 0).astype(np.float32)

    def shadow_at(wp):
        sx, sy, sd = to_light(wp)
        ix = np.clip(sx.astype(np.int64), 0, SM - 1)
        iy = np.clip(sy.astype(np.int64), 0, SM - 1)
        bias = 0.004 * (2 * rad)
        acc = np.zeros(len(wp), np.float32)
        cnt = 0
        for dx in (-2, 0, 2):
            for dy in (-2, 0, 2):
                jx = np.clip(ix + dx, 0, SM - 1)
                jy = np.clip(iy + dy, 0, SM - 1)
                acc += (sd - bias <= sdepth[jy, jx]).astype(np.float32)
                cnt += 1
        return acc / cnt

    # ---- backdrop
    yy = np.linspace(0, 1, H)[:, None, None]
    img = (np.array(bg_top) * (1 - yy) + np.array(bg_bot) * yy) * np.ones((1, W, 1))
    img = img.astype(np.float32)
    emis = np.zeros((H, W, 3), np.float32)

    # rays for ground
    gx, gy = np.meshgrid(np.arange(W) + 0.5, np.arange(H) + 0.5)
    dirs = ((gx - W / 2) / fpx)[..., None] * r + (-(gy - H / 2) / fpx)[..., None] * u + f
    if ground:
        with np.errstate(divide='ignore', invalid='ignore'):
            tg = -eye[2] / dirs[..., 2]
        hit = (tg > 0) & np.isfinite(tg)
        gp = eye + dirs * tg[..., None]
        gz_depth = tg  # view depth along f (dirs has unit f component)
        gmask = hit & ((tid < 0) | (gz_depth < depth))
        if ground_r is None:
            ground_r = rad * 2.2
        gpts = gp[gmask]
        dist = np.linalg.norm(gpts[:, :2] - ctr[:2], axis=1)
        fade = 1 - smoothstep(ground_r * ground_fade[0], ground_r * ground_fade[1], dist)
        sh = shadow_at(gpts)
        # contact shadow from the top-down map
        ix = np.clip(((gpts[:, 0] - ctr[0]) * asc + AO / 2).astype(int), 0, AO - 1)
        iy = np.clip((AO / 2 - (gpts[:, 1] - ctr[1]) * asc).astype(int), 0, AO - 1)
        occb = gaussian_filter(occ, AO * 0.012)
        occb2 = gaussian_filter(occ, AO * 0.035)
        ao = 1 - 0.55 * occb[iy, ix] - 0.25 * occb2[iy, ix]
        gl = np.array(ground_col, np.float32)
        if ground_layers:
            gl = np.tile(gl, (len(gpts), 1))
            for gt, gscale, gamt in ground_layers:
                d = sample(gt, gpts[:, :2] / gscale)[:, :3].mean(1, keepdims=True)
                gl = gl * np.clip(1 + (2 * d - 1) * gamt, 0.15, 2.2)
        lit = gl * (amb * 1.3 + 0.75 * max(Lk[2], 0) * sh[:, None] * np.array(key_col)) * ao[:, None]
        lit = lit + point_contrib(scene, gpts, np.tile([0, 0, 1.0], (len(gpts), 1))) * gl * 2.2
        base = img[gmask]
        img[gmask] = base * (1 - fade[:, None]) + lit * fade[:, None]

    # ---- geometry
    def shade(ti, bc):
        tv = T[ti]
        wp = (P[tv] * bc[:, :, None]).sum(1)
        nn = (N[tv] * bc[:, :, None]).sum(1)
        nn /= np.maximum(np.linalg.norm(nn, axis=1, keepdims=True), 1e-9)
        view = norm_rows(eye - wp)
        flipm = (nn * view).sum(1) < 0
        nn[flipm] *= -1
        uv = (UV[tv] * bc[:, :, None]).sum(1)
        tg = (TAN[tv] * bc[:, :, None]).sum(1)
        bt = (BIT[tv] * bc[:, :, None]).sum(1)
        tg = tg - (tg * nn).sum(1, keepdims=True) * nn
        tg /= np.maximum(np.linalg.norm(tg, axis=1, keepdims=True), 1e-9)
        bt = bt - (bt * nn).sum(1, keepdims=True) * nn - (bt * tg).sum(1, keepdims=True) * tg
        bt /= np.maximum(np.linalg.norm(bt, axis=1, keepdims=True), 1e-9)
        mid = M[ti]
        wear = np.zeros(len(ti), np.float32)
        alb = np.zeros((len(ti), 3), np.float32)
        em = np.zeros((len(ti), 3), np.float32)
        spec = np.zeros(len(ti), np.float32)
        gloss = np.ones(len(ti), np.float32)
        for k, m in enumerate(mats):
            s = mid == k
            if not s.any():
                continue
            if m['tex'] is not None:
                c = sample(m['tex'], uv[s] * m['tile'])[:, :3]
                if m['desat']:
                    g = c.mean(1, keepdims=True)
                    c = c * (1 - m['desat']) + g * m['desat']
                c = c * m['tint']
            else:
                c = np.tile(m['color'], (s.sum(), 1))
            if m['detail'] is not None:
                dd = sample(m['detail'], uv[s] * m['detail_tile'])[:, :3].mean(1, keepdims=True)
                c = c * np.clip(1 + (2 * dd - 1) * m['detail_amt'], 0.2, 2.0)
            if m['normal'] is not None:
                nm = sample(m['normal'], uv[s] * m['normal_tile'])[:, :3] * 2 - 1
                nm[:, :2] *= m['normal_amt']
                n2 = tg[s] * nm[:, :1] + bt[s] * nm[:, 1:2] + nn[s] * np.abs(nm[:, 2:3])
                nn[s] = n2 / np.maximum(np.linalg.norm(n2, axis=1, keepdims=True), 1e-9)
            wear[s] = m['wear']
            alb[s] = c
            if m['emissive'] is not None:
                if m['emis_tex'] is not None:
                    e = sample(m['emis_tex'], uv[s] * m['tile'])[:, :3]
                    em[s] = e * m['emissive'] * m['emis_gain']
                else:
                    em[s] = m['emissive'] * m['emis_gain'] * (c if m['tex'] is not None else 1)
            spec[s] = m['spec']
            gloss[s] = m['gloss']
        shk = shadow_at(wp)
        dk = np.clip(nn @ Lk, 0, 1) * shk
        df = np.clip(nn @ Lf, 0, 1)
        dr = np.clip(nn @ Lr, 0, 1)
        hemi = 0.5 + 0.5 * nn[:, 2]
        light = (amb * (0.55 + 0.45 * hemi))[:, None] * np.array([0.9, 0.95, 1.1]) + \
            dk[:, None] * np.array(key_col) + df[:, None] * np.array(fill_col) + dr[:, None] * np.array(rim_col)
        hv = norm_rows(Lk + view)
        sp = (np.clip((nn * hv).sum(1), 0, 1) ** gloss) * spec * shk
        fr = (1 - np.clip((nn * view).sum(1), 0, 1))
        fres = fr ** 3 * 0.25
        col = alb * light + sp[:, None] * np.array(key_col) + fres[:, None] * np.array(rim_col) * 0.5
        col += alb * point_contrib(scene, wp, nn) * 1.6
        col = col * exposure + em
        return col, em, fr, mid, nn, wear

    gm = tid >= 0
    if ground:
        gm = gm & ~gmask
    col, em, _, _, nnb, wearb = shade(tid[gm], bary[gm])
    img[gm] = col
    emis[gm] = em
    # edge wear: bright worn-paint lines where the surface normal turns sharply (screen space)
    if wearb.any():
        nbuf = np.zeros((H, W, 3), np.float32)
        wbuf = np.zeros((H, W), np.float32)
        tb = np.full((H, W), -1, np.int64)
        nbuf[gm] = nnb
        wbuf[gm] = wearb
        tb[gm] = M[tid[gm]]
        dzb = np.where(gm, depth, 1e9)
        e = np.zeros((H, W), np.float32)
        k = max(1, int(round(1.2 * ss)))
        for dy, dx in ((0, k), (k, 0), (0, -k), (-k, 0)):
            ns = np.roll(nbuf, (dy, dx), (0, 1))
            zs = np.roll(dzb, (dy, dx), (0, 1))
            ms = np.roll(tb, (dy, dx), (0, 1))
            dot = (nbuf * ns).sum(2)
            same = (np.abs(zs - dzb) < 0.004 * dzb) & (ms == tb)
            e = np.maximum(e, np.where(same, 1 - dot, 0))
        e = smoothstep(0.12, 0.5, e) * wbuf
        img = img + (e[..., None] * np.array([0.22, 0.22, 0.21], np.float32)).astype(np.float32)

    # ---- glass (one layer, the nearest)
    if len(gl_idx):
        gdepth, gt0, gbary = raster(X, Y, Q, vz, T[gl_idx], W, H)
        odepth = np.where(gm, depth, np.inf)
        vm = (gt0 >= 0) & (gdepth < odepth)
        gti = gl_idx[gt0[vm]]
        gcol, gem, fr, mid, _, _ = shade(gti, gbary[vm])
        alpha = np.array([mats[m]['glass'] for m in mid], np.float32)
        a = np.clip(alpha + fr ** 2 * 0.5, 0, 0.95)[:, None]
        img[vm] = img[vm] * (1 - a) + gcol * a
        emis[vm] = emis[vm] * (1 - a * 0.6)

    # ---- sprites (lens-flare bitmaps), depth-tested at their centre
    for pos, ssz, rgb, st in scene.sprites:
        rel = np.array(pos) - eye
        z = rel @ f
        if z <= 0:
            continue
        sx = W / 2 + (rel @ r) / z * fpx
        sy = H / 2 - (rel @ u) / z * fpx
        ix, iy = int(sx), int(sy)
        if not (0 <= ix < W and 0 <= iy < H):
            continue
        if tid[iy, ix] >= 0 and depth[iy, ix] < z - 0.015:
            continue
        rad = ssz * fpx / z
        x0, x1 = max(0, int(sx - rad)), min(W, int(sx + rad) + 1)
        y0, y1 = max(0, int(sy - rad)), min(H, int(sy + rad) + 1)
        if x1 <= x0 or y1 <= y0:
            continue
        gx_, gy_ = np.meshgrid(np.arange(x0, x1) + 0.5, np.arange(y0, y1) + 0.5)
        uvs = np.stack([((gx_ - sx) / (2 * rad) + 0.5).ravel(), ((gy_ - sy) / (2 * rad) + 0.5).ravel()], 1)
        inside = (uvs.min(1) >= 0) & (uvs.max(1) <= 1)
        c = sample(st, np.clip(uvs, 0, 0.999))[:, :3].mean(1) * inside
        add = (c[:, None] * np.array(rgb, np.float32)).reshape(y1 - y0, x1 - x0, 3)
        img[y0:y1, x0:x1] += add
        emis[y0:y1, x0:x1] += add * 0.6

    # ---- bloom
    if bloom:
        e = emis.clip(0, None)
        b = gaussian_filter(e, (W * 0.004, W * 0.004, 0)) * 0.9 + gaussian_filter(e, (W * 0.018, W * 0.018, 0)) * 0.7
        img = img + b * bloom
    img = np.clip(img, 0, 1)
    # downsample
    img = img.reshape(H // ss, ss, W // ss, ss, 3).mean((1, 3))
    im = Image.fromarray((img * 255 + 0.5).astype(np.uint8))
    if label:
        from PIL import ImageDraw, ImageFont
        d = ImageDraw.Draw(im)
        fnt = ImageFont.truetype('/usr/share/fonts/noto/NotoSans-Bold.ttf', 26 * size[0] // 1200)
        fnt2 = ImageFont.truetype('/usr/share/fonts/noto/NotoSans-Regular.ttf', 20 * size[0] // 1200)
        d.text((24, 18), label[0], font=fnt, fill=(225, 230, 238))
        if len(label) > 1:
            d.text((24, 52), label[1], font=fnt2, fill=(150, 160, 175))
    if out:
        im.save(out)
    return im


def norm_rows(v):
    return v / np.maximum(np.linalg.norm(v, axis=1, keepdims=True), 1e-9)


def point_contrib(scene, wp, nn):
    out = np.zeros((len(wp), 3), np.float32)
    for pos, colr, radius in scene.point_lights:
        d = np.array(pos) - wp
        dist = np.linalg.norm(d, axis=1)
        l = d / np.maximum(dist[:, None], 1e-9)
        lam = np.clip((nn * l).sum(1), 0, 1)
        att = 1.0 / (1.0 + (dist / radius) ** 2)
        out += (lam * att)[:, None] * np.array(colr)
    return out


def uv_tangents(P, UV, T):
    """per-vertex tangent (along +u) and bitangent (along +v) from the UV layout"""
    T = np.asarray(T)
    p0, p1, p2 = P[T[:, 0]], P[T[:, 1]], P[T[:, 2]]
    u0, u1, u2 = UV[T[:, 0]], UV[T[:, 1]], UV[T[:, 2]]
    e1, e2 = p1 - p0, p2 - p0
    d1, d2 = u1 - u0, u2 - u0
    det = d1[:, 0] * d2[:, 1] - d2[:, 0] * d1[:, 1]
    r = np.where(np.abs(det) > 1e-12, 1.0 / np.where(det == 0, 1, det), 0.0)[:, None]
    t = (e1 * d2[:, 1:2] - e2 * d1[:, 1:2]) * r
    b = (e2 * d1[:, 0:1] - e1 * d2[:, 0:1]) * r
    TA = np.zeros_like(P)
    BI = np.zeros_like(P)
    for k in range(3):
        np.add.at(TA, T[:, k], t)
        np.add.at(BI, T[:, k], b)
    return TA, BI
