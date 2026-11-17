"""SnD bomb mockups: 10 variants x 3 views (hero 3/4, planted, carried on the cyborg's back) + contact sheet.

usage: python3 variants.py [variant ids...]   (default: all)
Output: the work folder,planted,carried}.png
"""
import os, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from kit import *  # noqa
from scipy.spatial import ConvexHull

OUT = os.environ.get('AE_BOMB_SCRATCH', '.').rstrip('/') + '/'

# ---------------------------------------------------------------- textures

TIMER = tex(P_TIMER)


def timer_tex(state):
    t = TIMER.copy()
    red = t[:, 61].copy()
    if state == 'planted':
        t[:, 44:48] = red[:, None] * np.ones((1, 4, 1))      # bar 3 of 4 gone red
    return t


DECAL = tex(P_DECAL)[2:30].copy()                             # the stencil text, not the glow pill


def hazard_tex(n=64, k=4):
    y, x = np.mgrid[0:n, 0:n]
    s = ((x + y) // (n // (2 * k))) % 2
    t = np.zeros((n, n, 4), np.float32)
    t[..., 3] = 1
    t[s == 0, :3] = (0.95, 0.72, 0.08)
    t[s == 1, :3] = (0.05, 0.05, 0.05)
    return t


HAZ = hazard_tex()


def coil_band_tex():
    t = np.zeros((16, 64, 4), np.float32)
    t[..., 3] = 1
    t[..., :3] = (1.0, 0.62, 0.15)
    for k in range(10):
        t[:, k * 64 // 10: k * 64 // 10 + 2, :3] = (0.05, 0.04, 0.03)
    t[:2, :, :3] = t[-2:, :, :3] = (0.05, 0.04, 0.03)
    return t


BAND = coil_band_tex()


def healthpack_tex(tint=(0.36, 0.40, 0.28)):
    """the health pack's own diffuse, cross painted out, white -> olive drab (so it never reads as health)"""
    t = tex('powerups/healthpack/bitmaps/healthpack').copy()
    c = t[..., :3]
    cross = (c[..., 0] > 0.45) & (c[..., 1] < 0.35) & (c[..., 2] < 0.35)
    lum = c.mean(-1)
    lum[cross] = np.median(lum[~cross])
    t[..., :3] = lum[..., None] * np.array(tint, np.float32) * 1.05
    return t


# ---------------------------------------------------------------- materials

RED = (1.0, 0.10, 0.04)
M_GUN = mat(color=(0.09, 0.095, 0.10), spec=0.35, gloss=40)
M_GUN2 = mat(color=(0.16, 0.165, 0.17), spec=0.3, gloss=30)
M_STRAP = mat(color=(0.20, 0.21, 0.13), spec=0.05)
M_TAPE = mat(color=(0.30, 0.31, 0.32), spec=0.25, gloss=18)
M_BUCKLE = mat(color=(0.45, 0.45, 0.45), spec=0.6, gloss=60)
W_RED = mat(color=(0.75, 0.06, 0.04), spec=0.4, gloss=40)
W_BLK = mat(color=(0.04, 0.04, 0.04), spec=0.4, gloss=40)
W_YEL = mat(color=(0.85, 0.65, 0.05), spec=0.4, gloss=40)
W_BLU = mat(color=(0.08, 0.2, 0.7), spec=0.4, gloss=40)
M_HAZ = mat(tex=HAZ, spec=0.2)
M_STENCIL = mat(tex=DECAL, tint=(0.92, 0.88, 0.72), alpha_test=True, spec=0.1)


def batt_mats(glow=(1.0, 0.25, 0.08)):
    return {0: mat(tex=tex('levels/b30/bitmaps/metal strips wide'), spec=0.3),
            1: mat(tex=tex('levels/b30/bitmaps/metal flat generic'), spec=0.3),
            2: mat(tex=tex('levels/b30/bitmaps/metal panels generator'), spec=0.3, emissive=glow,
                   emis_tex=tex('levels/b40/bitmaps/b40_metal panels generator glow'), emis_gain=1.6),
            3: mat(tex=tex('levels/b30/bitmaps/metal strips narrow ext'), spec=0.3)}


FRAG_M = {0: mat(tex=tex('weapons/frag grenade/bitmaps/frag grenade'), spec=0.25),
          1: mat(tex=tex('weapons/frag grenade/bitmaps/frag grenade'), spec=0.25),
          2: mat(tex=tex('weapons/frag grenade/bitmaps/frag grenade decal'), alpha_test=True),
          3: mat(color=(1, .8, .3), emissive=(1, .7, .2))}
PLAS_M = {1: mat(tex=tex('weapons/plasma grenade/bitmaps/plasma grenade'), spec=0.5, gloss=40),
          2: mat(tex=tex('weapons/plasma grenade/bitmaps/plasma grenade')),
          3: mat(tex=tex('weapons/plasma grenade/bitmaps/plasma grenade'), emissive=(1, 1, 1), emis_gain=1.3)}
_AT = mat(tex=tex('powerups/shotgun ammo/bitmaps/sg ammo'), spec=0.25)
AMMO_M = {0: _AT, 1: mat(tex=tex('powerups/shotgun ammo/bitmaps/sg ammo'), spec=0.25, alpha_test=True)}
S_CUBE = 0.7
M_PANEL = mat(tex=tex('levels/b30/bitmaps/metal flat generic'), tint=(0.62, 0.68, 0.48), spec=0.3, gloss=30)
M_GLASS = mat(color=(0.10, 0.16, 0.18), spec=0.9, gloss=80, glass=0.22)
CUBE_EDGE = mat(tex=tex('powerups/active camoflage/bitmaps/powerups'), spec=0.4, gloss=40)


def cube_mats(st, glass=False):
    nz = tex('characters/jackal/bitmaps/shield noise')
    core = mat(tex=nz, tint=(1.0, 0.3, 0.08), emissive=(1.0, 0.3, 0.08), emis_tex=nz, tile=(2, 2),
               emis_gain=2.4 if st == 'planted' else 1.7)
    return {0: CUBE_EDGE, 1: core if glass else None, 2: M_GLASS if glass else M_PANEL, 'core': core}
HP_TEX = healthpack_tex()
HP_M = {0: mat(tex=HP_TEX, spec=0.3, gloss=30)}


def beacon_mats(gain):
    return {0: mat(tex=tex('levels/b30/devices/light fixtures/light fixture ceiling1/bitmaps/light fixtures'),
                   tint=(0.6, 0.6, 0.62)),
            1: mat(color=RED, emissive=RED, emis_gain=gain)}


# ---------------------------------------------------------------- assembly

def frame(facing, up):
    """rotation whose local x = facing, z = up"""
    a = norm(facing)
    c = np.array(up, float)
    c = norm(c - np.dot(c, a) * a)
    b = np.cross(c, a)
    return np.stack([a, b, c], 1)


class Asm:
    """parts in the bomb's local frame (resting on z = 0)"""

    def __init__(self, state):
        self.state = state            # 'hero' | 'planted' | 'carried'
        self.items = []               # (P, N, UV, T, mat, cast, tag)
        self.lights = []              # (pos, colour, radius)
        self.group = None             # anchor point of a fixed-size group (timer, beacon) for scale_asm

    def add(self, P, N, UV, T, m, xf=None, tag=None, cast=True):
        if xf is not None:
            P = xf.p(P)
            N = xf.n(N) if N is not None else None
        if N is None:
            N = vnormals(P, T)
        self.items.append((P, N, UV if UV is not None else np.zeros((len(P), 2)), np.asarray(T), m, cast, tag,
                           self.group))

    def gbx(self, path, xf, mats, tag=None, skip=()):
        parts, _ = load_gbx(path)
        for sh, P, N, UV, T in parts:
            if sh in skip:
                continue
            m = mats.get(sh, mats.get('default'))
            if m is None:
                continue
            self.add(P, N, UV, T, m, xf, tag)

    def pts(self, tags):
        sel = [(it[0], it[3]) for it in self.items if it[6] in tags]
        return sel

    def bounds(self):
        P = np.vstack([it[0] for it in self.items])
        return P.min(0), P.max(0)

    def emit(self, scene, xf=None):
        ids = {}
        for P, N, UV, T, m, cast, tag, grp in self.items:
            k = id(m)
            if k not in ids:
                ids[k] = scene.add_mat(m)
            if xf is not None:
                P2, N2 = xf.p(P), xf.n(N)
            else:
                P2, N2 = P, N
            scene.add(P2, N2, UV, T, ids[k], None, cast)
        for pos, size, rgb, st in getattr(self, 'sprites', []):
            p = np.array(pos, float)[None]
            if xf is not None:
                p = xf.p(p)
            scene.sprites.append((p[0], size * (xf.s[0] if xf is not None else 1), rgb, st))
        for pos, col, rad in self.lights:
            p = np.array(pos, float)[None]
            if xf is not None:
                p = xf.p(p)
            scene.point_lights.append((p[0], col, rad * (xf.s[0] if xf is not None else 1)))


def section(P, T, axis, value):
    """points where triangle edges cross the plane coord[axis] = value"""
    T = np.asarray(T)
    out = []
    for i, j in ((0, 1), (1, 2), (2, 0)):
        a, b = P[T[:, i]], P[T[:, j]]
        da, db = a[:, axis] - value, b[:, axis] - value
        m = (da * db) < 0
        t = (da[m] / (da[m] - db[m]))[:, None]
        out.append(a[m] + (b[m] - a[m]) * t)
    return np.vstack(out) if out else np.zeros((0, 3))


def strap(asm, tags, axis, value, width=0.012, thick=0.0025, off=0.002, m=M_STRAP, buckle=True):
    """a band around the cross-section of the tagged parts in the plane coord[axis] = value"""
    pts = []
    for P, T in asm.pts(tags):
        for dv in (-width / 2, 0, width / 2):
            s = section(P, T, axis, value + dv)
            if len(s):
                pts.append(s)
    pts = np.vstack(pts)
    ax2 = [k for k in range(3) if k != axis]
    p2 = pts[:, ax2]
    ang = np.linspace(0, 2 * math.pi, 12, endpoint=False)
    p2 = (p2[:, None, :] + off * np.stack([np.cos(ang), np.sin(ang)], 1)[None]).reshape(-1, 2)
    h = ConvexHull(p2)
    poly = p2[h.vertices]
    # resample
    seg = np.vstack([poly, poly[:1]])
    L = np.linalg.norm(np.diff(seg, axis=0), axis=1)
    cum = np.concatenate([[0], np.cumsum(L)])
    s = np.linspace(0, cum[-1], 240, endpoint=False)
    rs = np.stack([np.interp(s, cum, seg[:, 0]), np.interp(s, cum, seg[:, 1])], 1)
    P3 = np.zeros((len(rs), 3))
    P3[:, ax2[0]] = rs[:, 0]
    P3[:, ax2[1]] = rs[:, 1]
    P3[:, axis] = value
    side = np.zeros(3)
    side[axis] = 1
    Pm, Nm, UVm, Tm = sweep(P3, rect(width, thick), side=side, closed=True)
    asm.add(Pm, None, UVm, Tm, m, tag='strap')
    if buckle:
        # buckle on the segment with the largest coordinate along the first in-plane axis (the "front")
        k = np.argmax(P3[:, ax2[0]] + 0.001 * P3[:, ax2[1]])
        c = P3[k].copy()
        sz = np.full(3, width * 1.5)
        sz[ax2[0]] = thick * 2.5
        asm.add(*box(sz, c), M_BUCKLE, tag='buckle')


def timer_module(asm, center, facing, up=(0, 0, 1), w=0.05, h=0.028, d=0.012, led=True, tag='timer'):
    R = frame(facing, up)
    xf = Xf(1, R, center)
    asm.group = np.array(center, float)
    asm.add(*box((d, w + 0.012, h + 0.012)), M_GUN, xf, tag=tag)
    asm.add(*box((d * 0.4, w + 0.004, h + 0.004), (d / 2 + d * 0.15, 0, 0)), M_GUN2, xf, tag=tag)
    st = asm.state
    face = mat(tex=timer_tex('planted' if st == 'planted' else 'armed'), emissive=(1, 1, 1),
               emis_tex=timer_tex('planted' if st == 'planted' else 'armed'), emis_gain=1.5 if st == 'planted' else 1.15)
    asm.add(*quad((d / 2 + d * 0.36, 0, 0), (0, w, 0), (0, 0, -h)), face, xf, tag=tag)
    # small keypad row under the face
    for i in range(4):
        asm.add(*box((0.003, 0.006, 0.004), (d / 2 + 0.001, -w / 2 + 0.008 + i * 0.011, -h / 2 - 0.0035)), M_GUN2, xf)
    if led:
        g = 3.0 if st == 'planted' else 2.0
        pos = xf.p(np.array([[d / 2 + 0.001, w / 2 + 0.001, h / 2 + 0.002]]))[0]
        asm.add(*sphere(0.0035, pos, 10), mat(color=RED, emissive=RED, emis_gain=g), tag='led')
        if st == 'planted':
            asm.lights.append((pos + norm(facing) * 0.02, (1.0, 0.1, 0.05), 0.05))
    asm.group = None
    return xf


def beacon(asm, pos, s=0.35):
    st = asm.state
    gain = 2.6 if st == 'planted' else 1.6
    asm.group = np.array(pos, float)
    asm.gbx(P_BEAC, Xf(s, None, pos), beacon_mats(gain), tag='beacon')
    asm.group = None
    top = np.array(pos) + np.array([0, 0, 0.04 * s])
    if st == 'planted':
        asm.lights.append((top + np.array([0, 0, 0.03]), (1.0, 0.08, 0.04), 0.09))
    else:
        asm.lights.append((top + np.array([0, 0, 0.03]), (0.6, 0.05, 0.03), 0.05))


def wire(asm, ctrl, m, r=0.0018):
    Pm, Nm, UVm, Tm = sweep(spline(ctrl, 10), circle(r, 8))
    asm.add(Pm, Nm, UVm, Tm, m, tag='wire')


def decal_quad(asm, center, facing, w, h, m, up=(0, 0, 1), tag='decal'):
    R = frame(facing, up)
    xf = Xf(1, R, center)
    asm.add(*quad((0.0008, 0, 0), (0, w, 0), (0, 0, -h)), m, xf, tag=tag, cast=False)


def lathe(profile, k=24, u_scale=1.0):
    """profile [(r, z)...]; flat-shaded along the profile, smooth around. UV u = angle, v = z"""
    P, N, UV, T = [], [], [], []
    ang = np.linspace(0, 2 * math.pi, k + 1)
    for (r0, z0), (r1, z1) in zip(profile[:-1], profile[1:]):
        dr, dz = r1 - r0, z1 - z0
        nr, nz = dz, -dr
        ln = math.hypot(nr, nz) or 1
        nr, nz = nr / ln, nz / ln
        base = len(P)
        for a in ang:
            ca, sa = math.cos(a), math.sin(a)
            for r, z in ((r0, z0), (r1, z1)):
                P.append((r * ca, r * sa, z))
                N.append((nr * ca, nr * sa, nz))
                UV.append((a / (2 * math.pi) * u_scale, z))
        for i in range(k):
            a0, a1, b0, b1 = base + 2 * i, base + 2 * i + 1, base + 2 * i + 2, base + 2 * i + 3
            T += [(a0, b0, b1), (a0, b1, a1)]
    return np.array(P), np.array(N), np.array(UV), np.array(T)


M_COIL = mat(color=(0.52, 0.54, 0.56), spec=0.45, gloss=40)
M_COILCAP = mat(color=(0.11, 0.115, 0.12), spec=0.3, gloss=30)


def mini_coil(asm, xf, tag='coil'):
    """mini fusion coil, ~0.3x of a full-size one: 0.11 wu (0.34 m) tall, r 0.032 wu; local z up, base at 0"""
    H, R = 0.11, 0.032
    st = asm.state
    cap = [(0, 0), (R + 0.003, 0), (R + 0.003, 0.014), (R, 0.016)]
    body1 = [(R, 0.016), (R, 0.044)]
    band = [(R + 0.0008, 0.044), (R + 0.0008, 0.068)]
    body2 = [(R, 0.068), (R, H - 0.016)]
    cap2 = [(R, H - 0.016), (R + 0.003, H - 0.014), (R + 0.003, H), (0.012, H), (0.012, H + 0.006), (0.0, H + 0.006)]
    asm.add(*lathe(cap), M_COILCAP, xf, tag=tag)
    asm.add(*lathe(body1), M_COIL, xf, tag=tag)
    asm.add(*lathe(body2), M_COIL, xf, tag=tag)
    asm.add(*lathe(cap2), M_COILCAP, xf, tag=tag)
    g = 1.7 if st == 'planted' else 1.3
    bm = mat(tex=BAND, emissive=(1, 1, 1), emis_tex=BAND, emis_gain=g)
    Pb, Nb, UVb, Tb = lathe(band, u_scale=1.0)
    UVb[:, 1] = (UVb[:, 1] - 0.044) / 0.024
    asm.add(Pb, Nb, UVb, Tb, bm, xf, tag=tag)
    # hazard ring stripe on the upper body
    hz = [(R + 0.0005, 0.076), (R + 0.0005, 0.086)]
    Ph, Nh, UVh, Th = lathe(hz, u_scale=6)
    UVh[:, 1] = (UVh[:, 1] - 0.076) / 0.01 * 0.4
    asm.add(Ph, Nh, UVh, Th, M_HAZ, xf, tag=tag)
    top = xf.p(np.array([[0, 0, H + 0.006]]))[0]
    if st == 'planted':
        asm.lights.append((xf.p(np.array([[0, 0, 0.056]]))[0] + np.array([0, 0, 0.0]), (0.5, 0.25, 0.05), 0.05))
    return top


# ---------------------------------------------------------------- variants
# each builder returns (asm, info) ; info: show = direction the timer faces (for carrying), carry_up

S_BATT = 0.115
S_BIGB = 0.09
S_AMMO = 1.4


def body_battery(a):
    a.gbx(P_BATT, Xf(S_BATT, None, (0, 0, -zmin(P_BATT, S_BATT))), batt_mats(), tag='body')


def body_ammo(a, t=(0, 0, 0)):
    a.gbx(P_AMMO, Xf(S_AMMO, None, np.array(t) + np.array([0, 0, -zmin(P_AMMO, S_AMMO)])), AMMO_M, tag='body')


def body_hp(a, z=0.0, yaw=0):
    a.gbx(P_HP, Xf(1.0, rot('z', yaw), (0, 0, z - zmin(P_HP))), HP_M, tag='body')


def frag(a, pos, R=None, tag='gren'):
    lo, hi = bbox(P_FRAG)
    c = (lo + hi) / 2
    c[2] = lo[2]
    R = np.eye(3) if R is None else R
    a.gbx(P_FRAG, Xf(1, R, np.array(pos) - R @ c), FRAG_M, tag=tag)


def plasma(a, pos, R=None, tag='gren'):
    lo, hi = bbox(P_PLAS)
    c = (lo + hi) / 2
    c[2] = lo[2]
    R = np.eye(3) if R is None else R
    a.gbx(P_PLAS, Xf(1, R, np.array(pos) - R @ c), PLAS_M, tag=tag, skip=(0,))


def A1(st):
    a = Asm(st)
    body_battery(a)
    tx = timer_module(a, (0.042 + 0.006, -0.045, 0.1), (1, 0, 0), w=0.052, h=0.03)
    beacon(a, (0.0, -0.045, 0.143), 0.33)
    # wires from the two terminal bolts on top, over the front edge, into the timer housing top
    for i, (m, dy) in enumerate(((W_RED, -0.012), (W_BLK, 0.012))):
        wire(a, [(-0.012 + 0.0, -0.045 + dy * 2.2, 0.146), (0.02, -0.045 + dy * 1.6, 0.152), (0.045, -0.045 + dy, 0.135),
                 (0.05, -0.045 + dy * 0.8, 0.119)], m)
    return a, dict(show=(1, 0, 0), up=(0, 0, 1))


def A2(st):
    a = Asm(st)
    body_battery(a)
    # three frags standing against the +x faces
    frag(a, (0.042 + 0.024, -0.062, 0.0), rot('z', 180))
    frag(a, (0.042 + 0.024, -0.016, 0.0), rot('z', 180))
    frag(a, (0.054 + 0.022, 0.048, 0.0), rot('z', 180))
    for z in (0.022, 0.055):
        strap(a, ('body', 'gren'), 2, z, width=0.011, m=M_TAPE, buckle=False)
    timer_module(a, (0.042 + 0.006, -0.04, 0.115), (1, 0, 0), w=0.05, h=0.028)
    # wires from each grenade's fuse to the timer
    tops = [(0.06, -0.062, 0.078), (0.06, -0.016, 0.078), (0.072, 0.048, 0.078)]
    for (x, y, z), m in zip(tops, (W_RED, W_YEL, W_BLU)):
        wire(a, [(x, y, z), (x + 0.012, (y - 0.04) / 2, z + 0.012), (0.058, -0.04 + (y + 0.016) * 0.25, 0.095)], m)
    return a, dict(show=(1, 0, 0), up=(0, 0, 1))


def A3(st):
    a = Asm(st)
    a.gbx(P_BIGB, Xf(S_BIGB, None, (0, 0, -zmin(P_BIGB, S_BIGB))), batt_mats(), tag='body')
    timer_module(a, (0.042 + 0.006, 0.0, 0.07), (1, 0, 0), w=0.05, h=0.028)
    beacon(a, (0.0, 0.0, 0.1), 0.3)
    # stencil and hazard panels on the end blocks' fronts
    decal_quad(a, (0.0425, 0.093, 0.03), (1, 0, 0), 0.05, 0.022, M_STENCIL)
    decal_quad(a, (0.0425, -0.093, 0.03), (1, 0, 0), 0.05, 0.024, mat(tex=HAZ, tile=(2.5, 1.2), spec=0.2))
    # wires from the four top bolts to the timer
    bolts = [(-0.014, -0.02), (0.014, -0.02), (-0.014, 0.02), (0.014, 0.02)]
    for (bx, by), m in zip(bolts, (W_RED, W_BLK, W_YEL, W_RED)):
        wire(a, [(bx, by, 0.102), (bx + 0.012, by * 0.9, 0.108), (0.04, by * 0.6, 0.1), (0.052, by * 0.45, 0.088)], m)
    # a long cable along the top to the far terminals
    wire(a, [(0.048, 0.025, 0.08), (0.044, 0.06, 0.07), (0.03, 0.1, 0.058), (0.0, 0.11, 0.057)], W_BLK, r=0.0022)
    wire(a, [(0.048, -0.025, 0.08), (0.044, -0.06, 0.07), (0.03, -0.1, 0.058), (0.0, -0.11, 0.057)], W_RED, r=0.0022)
    return a, dict(show=(1, 0, 0), up=(0, 0, 1))


def body_cube(a, glass=False):
    m = cube_mats(a.state, glass)
    m[1] = None
    a.gbx(P_CUBE, Xf(S_CUBE, None, (0, 0, -zmin(P_CUBE, S_CUBE))), m, tag='body')


CF = 0.113 * S_CUBE            # glass face half-size (0.079)
CT = (0.225 + 0.007) * S_CUBE  # glass top (0.162)


def B1(st):
    a = Asm(st)
    body_cube(a)
    timer_module(a, (CF + 0.006, 0.0, 0.095), (1, 0, 0), w=0.056, h=0.03)
    beacon(a, (0.0, 0.0, CT), 0.42)
    decal_quad(a, (CF + 0.0005, 0.0, 0.035), (1, 0, 0), 0.13, 0.03, mat(tex=HAZ, tile=(5, 1), spec=0.2))
    decal_quad(a, (0.0, CF + 0.0005, 0.08), (0, 1, 0), 0.11, 0.047, M_STENCIL)
    wire(a, [(CF + 0.012, -0.016, 0.11), (CF + 0.01, -0.03, 0.15), (0.04, -0.03, CT + 0.004), (0.016, -0.012, CT + 0.006)], W_RED)
    wire(a, [(CF + 0.012, 0.016, 0.11), (CF + 0.01, 0.03, 0.15), (0.04, 0.03, CT + 0.004), (0.016, 0.012, CT + 0.006)], W_BLK)
    return a, dict(show=(1, 0, 0), up=(0, 0, 1))


def B2(st):
    a = Asm(st)
    body_cube(a)
    plasma(a, (0.0, -0.04, CT))
    plasma(a, (0.0, 0.04, CT), rot('z', 180))
    frag(a, (-0.035, CF + 0.022, 0.03), rot('z', -90))
    frag(a, (0.03, CF + 0.022, 0.03), rot('z', -90))
    for y in (-0.04, 0.04):
        strap(a, ('body', 'gren'), 1, y, width=0.012, m=M_STRAP)
    strap(a, ('body', 'gren'), 2, 0.065, width=0.011, m=M_TAPE, buckle=False)
    timer_module(a, (CF + 0.006, 0.0, 0.1), (1, 0, 0), w=0.05, h=0.028)
    for y, m in ((-0.04, W_RED), (0.04, W_YEL)):
        wire(a, [(CF + 0.012, y * 0.3, 0.116), (CF + 0.006, y * 0.7, 0.15), (0.03, y, CT + 0.05), (0.0, y, CT + 0.052)], m)
    for x, m in ((-0.035, W_BLU), (0.03, W_BLK)):
        wire(a, [(x, CF + 0.03, 0.105), (x * 0.6 + 0.03, CF + 0.02, 0.13), (CF + 0.012, 0.022, 0.116)], m)
    return a, dict(show=(1, 0, 0), up=(0, 0, 1))


def B3(st):
    a = Asm(st)
    body_cube(a, glass=True)
    c = (0, 0, (0.035 + 0.19) / 2 * S_CUBE + 0.007 * S_CUBE)
    Ps, Ns, UVs, Ts = sphere(0.056, c, 24)
    UVs = np.stack([np.arctan2(Ns[:, 1], Ns[:, 0]) / (2 * math.pi) + 0.5, np.arccos(np.clip(Ns[:, 2], -1, 1)) / math.pi], 1)
    a.add(Ps, Ns, UVs, Ts, cube_mats(st, True)['core'], tag='core')
    a.lights.append((c, (1.0, 0.25, 0.06) if st == 'planted' else (0.7, 0.18, 0.05), 0.07))
    timer_module(a, (CF + 0.006, 0.0, 0.118), (1, 0, 0), w=0.056, h=0.03)
    beacon(a, (0.0, 0.0, CT), 0.42)
    # dark plates with the stencil and hazard on the lower glass
    a.add(*box((0.004, 0.13, 0.042), (CF + 0.002, 0, 0.032)), M_GUN)
    decal_quad(a, (CF + 0.0045, 0.0, 0.032), (1, 0, 0), 0.12, 0.036, M_STENCIL)
    a.add(*box((0.13, 0.004, 0.042), (0, CF + 0.002, 0.032)), M_GUN)
    decal_quad(a, (0.0, CF + 0.0045, 0.032), (0, 1, 0), 0.12, 0.034, mat(tex=HAZ, tile=(4, 1), spec=0.2))
    # leads from the core up through the lid into the timer
    for dy, m in ((-0.012, W_RED), (0.0, W_YEL), (0.012, W_BLK)):
        wire(a, [(0.02, dy, c[2] + 0.04), (0.03, dy, CT - 0.01), (0.05, dy * 1.4, CT + 0.008), (CF + 0.012, dy, CT - 0.012),
                 (CF + 0.012, dy, 0.136)], m)
    return a, dict(show=(1, 0, 0), up=(0, 0, 1))


def B4(st):
    a = Asm(st)
    body_hp(a)
    plasma(a, (0.0, -0.045, 0.079))
    plasma(a, (0.0, 0.045, 0.079), rot('z', 180))
    for y in (-0.045, 0.045):
        strap(a, ('body', 'gren'), 1, y, width=0.012, m=M_STRAP)
    timer_module(a, (0.08 + 0.006, 0.0, 0.042), (1, 0, 0), w=0.05, h=0.028)
    for y, m in ((-0.045, W_RED), (0.045, W_YEL)):
        wire(a, [(0.086, y * 0.25, 0.058), (0.083, y * 0.6, 0.08), (0.03, y, 0.13), (0.0, y, 0.131)], m)
    return a, dict(show=(1, 0, 0), up=(0, 0, 1))


def C1(st):
    a = Asm(st)
    body_ammo(a)
    # angled timer on the lid, tilted toward +y (the stencil side)
    f = norm((0, 0.75, 1.0))
    timer_module(a, np.array((0.03, 0.018, 0.139)) + f * 0.006, f, up=(0, -1, 0.75), w=0.05, h=0.028)
    beacon(a, (-0.055, 0.0, 0.1385), 0.33)
    wire(a, [(0.01, 0.01, 0.142), (-0.01, 0.0, 0.146), (-0.035, 0.004, 0.144)], W_RED)
    wire(a, [(0.05, 0.01, 0.142), (0.07, 0.03, 0.13), (0.085, 0.036, 0.11)], W_BLK)
    return a, dict(show=(0, 1, 0), up=(0, 0, 1))


def C2(st):
    a = Asm(st)
    body_ammo(a)
    frag(a, (-0.045, 0.035 + 0.022, 0.0), rot('z', -90))
    frag(a, (0.04, 0.035 + 0.022, 0.0), rot('z', -90))
    plasma(a, (-0.002, 0.035 + 0.02, 0.0), rot('z', 90))
    for z in (0.02, 0.05):
        strap(a, ('body', 'gren'), 2, z, width=0.011, m=M_TAPE, buckle=False)
    f = norm((0, 0.75, 1.0))
    timer_module(a, np.array((0.0, 0.018, 0.139)) + f * 0.006, f, up=(0, -1, 0.75), w=0.05, h=0.028)
    for x, m in ((-0.045, W_RED), (0.04, W_YEL), (-0.002, W_BLU)):
        wire(a, [(x, 0.055, 0.075), (x * 0.8, 0.05, 0.11), (x * 0.4, 0.045, 0.135), (x * 0.25, 0.04, 0.138)], m)
    return a, dict(show=(0, 1, 0), up=(0, 0, 1))


def D1(st):
    a = Asm(st)
    tops = []
    for x in (-0.034, 0.034):
        for y in (-0.034, 0.034):
            tops.append(mini_coil(a, Xf(1, rot('z', 30 * (x > 0) + 15 * (y > 0)), (x, y, 0))))
    for z in (0.009, 0.1):
        strap(a, ('coil',), 2, z, width=0.01, m=M_STRAP)
    tx = timer_module(a, (0.066 + 0.006, 0.0, 0.03), (1, 0, 0), w=0.046, h=0.026)
    for t, m in zip(tops, (W_RED, W_BLK, W_YEL, W_RED)):
        wire(a, [t, t + np.array([0, 0, 0.012]), (t[0] * 0.3 + 0.04, t[1] * 0.4, 0.13), (0.07, t[1] * 0.2, 0.06),
                 (0.074, t[1] * 0.15, 0.045)], m)
    return a, dict(show=(1, 0, 0), up=(0, 0, 1))


def D2(st):
    a = Asm(st)
    body_ammo(a)
    tops = []
    for x in (-0.066, 0.0, 0.066):
        tops.append(mini_coil(a, Xf(1, rots(('z', 25 + x * 400), ('x', -90)), (x, -0.0605, 0.1395 + 0.035))))
    for y in (-0.036, 0.036):
        strap(a, ('body', 'coil'), 1, y, width=0.011, m=M_STRAP)
    timer_module(a, (0.0, 0.035 + 0.006, 0.075), (0, 1, 0), w=0.05, h=0.028)
    for t, m in zip(tops, (W_RED, W_BLK, W_YEL)):
        wire(a, [t, t + np.array([0, 0.012, -0.004]), (t[0] * 0.5, 0.06, 0.12), (t[0] * 0.15, 0.047, 0.092)], m)
    return a, dict(show=(0, 1, 0), up=(0, 0, 1))


P_RL = 'powerups/rocket launcher ammo/rocket launcher ammo'
_RT = tex('powerups/rocket launcher ammo/bitmaps/rocket launcher ammo')
RL_M = {0: mat(tex=_RT, alpha_test=True), 1: mat(tex=_RT, spec=0.3), 2: mat(tex=_RT, spec=0.3)}
P_THP = TS + 'cmt/powerups/h_powerup_pack/_health/_health'


def tsce_hp_tex():
    """TSC:E med pack diffuse: caduceus painted out, red stripes -> hazard orange, white -> gunmetal grey"""
    t = tex(TS + 'cmt/powerups/h_powerup_pack/_health/bitmaps/h_powerup_pack_health_diff').copy()
    c = t[..., :3]
    r, g, b = c[..., 0], c[..., 1], c[..., 2]
    yel = (r > 0.55) & (g > 0.45) & (b < 0.3)
    ys, xs = np.nonzero(yel)
    h, w = yel.shape
    y0, y1, x0, x1 = ys.min(), ys.max(), xs.min(), xs.max()
    py, px = int((y1 - y0) * 0.08), int((x1 - x0) * 0.08)
    box_ = np.zeros_like(yel)
    box_[max(0, y0 - py):y1 + py + 1, max(0, x0 - px):x1 + px + 1] = True
    lum = c.mean(-1)
    sat = c.max(-1) - c.min(-1)
    red = (r > 0.45) & (g < 0.3) & (b < 0.3)
    fill = np.median(lum[box_ & (sat < 0.1)])
    out = lum[..., None] * np.array((0.50, 0.52, 0.50), np.float32)
    out[box_ & (sat > 0.12)] = fill * np.array((0.50, 0.52, 0.50))
    o = red & ~box_
    out[o] = np.array((0.95, 0.50, 0.05)) * np.clip(lum[o] * 2.2, 0.6, 1.2)[:, None]
    t[..., :3] = out
    return t


THP_M = {0: mat(tex=tsce_hp_tex(), spec=0.35, gloss=30)}


def rl_case(a, pos, s=0.5, R=None, tag='rl'):
    lo, hi = bbox(P_RL)
    c = (lo + hi) / 2
    c[2] = lo[2]
    R = np.eye(3) if R is None else R
    a.gbx(P_RL, Xf(s, R, np.array(pos) - R @ (c * s)), RL_M, tag=tag)


def E1(st):
    a = Asm(st)
    s = 0.5
    rl_case(a, (0, -0.0465, 0), s, rot('z', 180), tag='body')
    rl_case(a, (0, 0.0465, 0), s, rot('z', 180), tag='body')
    rl_case(a, (0, 0, 0.05), s, rot('z', 180), tag='body')
    for x in (-0.075, 0.075):
        strap(a, ('body',), 0, x, width=0.013, m=M_TAPE, buckle=False)
    f = norm((0, 0.75, 1.0))
    timer_module(a, np.array((0.0, 0.012, 0.1)) + f * 0.006, f, up=(0, -1, 0.75), w=0.05, h=0.028)
    for (y, z), m in zip(((-0.0465, 0.025), (0.0465, 0.025), (0.0, 0.075)), (W_RED, W_YEL, W_BLK)):
        wire(a, [(0.123, y, z), (0.135, y * 0.9, z + 0.01), (0.1, y * 0.5 + 0.01, 0.112), (0.03, 0.02, 0.11)], m)
    return a, dict(show=f, up=(1, 0, 0))


def E2(st):
    a = Asm(st)
    body_ammo(a)
    rl_case(a, (0, 0, 0.1395), 0.5, rot('z', 180), tag='rl')
    for x in (-0.055, 0.055):
        strap(a, ('body', 'rl'), 0, x, width=0.012, m=M_STRAP)
    timer_module(a, (0.0, 0.035 + 0.006, 0.075), (0, 1, 0), w=0.05, h=0.028)
    for dx, m in ((-0.012, W_RED), (0.012, W_BLK)):
        wire(a, [(dx, 0.047, 0.09), (dx * 2, 0.05, 0.12), (dx * 4 + 0.06, 0.045, 0.165), (0.1, 0.03, 0.17), (0.124, 0.02, 0.165)], m)
    return a, dict(show=(0, 1, 0), up=(0, 0, 1))


def F1(st):
    a = Asm(st)
    a.gbx(P_THP, Xf(1, None, (0, 0, -zmin(P_THP))), THP_M, tag='body')
    f = norm((1, 0, 1.4))
    timer_module(a, np.array((0.025, 0.0, 0.07)) + f * 0.008, f, up=(-1, 0, 0.7), w=0.06, h=0.032)
    beacon(a, (-0.045, 0.0, 0.068), 0.38)
    wire(a, [(0.005, -0.012, 0.085), (-0.015, -0.02, 0.09), (-0.03, -0.012, 0.083)], W_RED)
    wire(a, [(0.005, 0.012, 0.085), (-0.015, 0.02, 0.09), (-0.03, 0.012, 0.083)], W_BLK)
    return a, dict(show=f, up=(-1, 0, 0.7))


def F2(st):
    a = Asm(st)
    a.gbx(P_THP, Xf(1, None, (0, 0, -zmin(P_THP))), THP_M, tag='body')
    rl_case(a, (0, 0, 0.069), 0.4, rot('z', 90), tag='rl')
    for y in (-0.055, 0.055):
        strap(a, ('body', 'rl'), 1, y, width=0.012, m=M_STRAP)
    timer_module(a, (0.092 + 0.006, 0.0, 0.034), (1, 0, 0), w=0.046, h=0.026)
    for dy, m in ((-0.01, W_RED), (0.01, W_YEL)):
        wire(a, [(0.1, dy, 0.05), (0.098, dy * 2, 0.075), (0.06, dy * 4, 0.1), (0.025, dy * 6, 0.112)], m)
    return a, dict(show=(1, 0, 0), up=(0, 0, 1))


VARIANTS = {
    'A1': (A1, 'A1 Battery charge', 'b40 battery + digsite timer + red beacon'),
    'A2': (A2, 'A2 Battery IED', 'b40 battery + 3 frags taped on + wired timer'),
    'A3': (A3, 'A3 Heavy battery', 'b40 bigbattery + timer + stencil + hazard + beacon'),
    'B1': (B1, 'B1 Armoured cube', 'powerup cube, steel panels + timer + hazard + stencil + beacon'),
    'B2': (B2, 'B2 Cube IED', 'powerup cube + 2 plasma on top + 2 frags taped + timer'),
    'B3': (B3, 'B3 Glass core cube', 'powerup cube, glass kept, red core + leads + timer + beacon'),
    'B4': (B4, 'B4 Med-kit IED', 'stock health pack (olive, cross painted out) + 2 plasma + timer'),
    'C1': (C1, 'C1 Ammo can charge', 'stock CE ammo can + angled timer on lid + beacon'),
    'C2': (C2, 'C2 Ammo can IED', 'stock CE ammo can + 2 frags + plasma taped + timer'),
    'D1': (D1, 'D1 Mini coil bundle', '4 mini fusion coils strapped + timer'),
    'E1': (E1, 'E1 Rocket case bundle', '3 SPNKr ammo cases taped + wires + angled timer'),
    'E2': (E2, 'E2 Rocket case on ammo can', 'ammo can + SPNKr case strapped on + timer'),
    'F1': (F1, 'F1 Med pack charge', 'TSC:E med pack (grey, orange stripes) + timer on top + beacon'),
    'F2': (F2, 'F2 Med pack IED', 'TSC:E med pack + small SPNKr case strapped + timer'),
    'D2': (D2, 'D2 Coils on ammo can', 'stock CE ammo can + 3 mini fusion coils + timer'),
}

# ---------------------------------------------------------------- views

_cyb = None


def cyborg(scene):
    m = mat(tex=tex('characters/cyborg/bitmaps/cyborg'), desat=1.0, tint=(0.62, 0.65, 0.70), spec=0.25, gloss=30)
    scene.add_gbx(P_CYB, Xf(1), {'default': m, 0: None, 1: None})


def cyborg_back():
    parts, _ = load_gbx(P_CYB)
    P = np.vstack([p[1] for p in parts])
    sel = (P[:, 2] > 0.42) & (P[:, 2] < 0.6) & (np.abs(P[:, 1]) < 0.07)
    return P[sel, 0].min()


def orbit(c, yaw, elev, dist):
    a, e = math.radians(yaw), math.radians(elev)
    return np.array(c) + np.array([math.cos(a) * math.cos(e), math.sin(a) * math.cos(e), math.sin(e)]) * dist


def fit_dist(lo, hi, fov, margin=1.25, aspect=4 / 3):
    rad = np.linalg.norm(hi - lo) / 2
    return rad / math.tan(math.radians(fov) / 2) * margin


def run(vid):
    fn, title, sub = VARIANTS[vid]
    out = []
    # hero
    a, info = fn('hero')
    sc = Scene()
    a.emit(sc)
    lo, hi = a.bounds()
    c = (lo + hi) / 2
    d = fit_dist(lo, hi, 28, 1.05)
    yaw = info.get('hero_yaw', 35 if info['show'][0] > 0.5 else 65 if info['show'][1] > 0.3 else -65)
    render(sc, OUT + f'{vid}_hero.png', orbit(c, yaw, 24, d), c, fov=28, exposure=1.12, amb=0.25, label=(title, sub + '  (3/4 view)'))
    out.append(OUT + f'{vid}_hero.png')
    # planted
    a, info = fn('planted')
    sc = Scene()
    a.emit(sc)
    c2 = c.copy()
    c2[2] *= 0.8
    render(sc, OUT + f'{vid}_planted.png', orbit(c2, yaw + 22, 34, d * 1.15), c2, fov=28, amb=0.12, exposure=0.72,
           key_col=(0.75, 0.75, 0.8), label=(title + ' - planted', 'fuse running: beacon lit, 2 of 4 timer bars red'))
    out.append(OUT + f'{vid}_planted.png')
    # carried
    a, info = fn('carried')
    sc = Scene()
    cyborg(sc)
    S = frame(info['show'], info['up'])
    Tg = frame((-1, 0, 0), (0, 0, 1))
    R = Tg @ S.T
    P = np.vstack([it[0] for it in a.items]) @ R.T
    bx = cyborg_back()
    t = np.array([bx + 0.012 - P[:, 0].max(), -(P[:, 1].max() + P[:, 1].min()) / 2,
                  0.5 - (P[:, 2].max() + P[:, 2].min()) / 2])
    a.emit(sc, Xf(1, R, t))
    tgt = np.array([0, 0, 0.375])
    render(sc, OUT + f'{vid}_carried.png', orbit(tgt, 205, 12, 1.5), tgt, fov=32, exposure=1.1, amb=0.26,
           label=(title + ' - carried', 'on the stock cyborg (2.15 m), rear 3/4 view'))
    out.append(OUT + f'{vid}_carried.png')
    return out


if __name__ == '__main__':
    ids = sys.argv[1:] or list(VARIANTS)
    for v in ids:
        t0 = time.time()
        run(v)
        print(v, round(time.time() - t0, 1), 's', flush=True)
