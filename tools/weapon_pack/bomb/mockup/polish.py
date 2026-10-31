"""Round 3: textured ("Bungie polish") SnD bombs.

A3 stock CE health kit IED, B3 TSC:E med pack IED (A/B), E1-P / D1-P (polished E1-S / D1-S),
G1-H / G2-H (40mm rounds at human scale).

usage: python3 polish.py views <ids...> | sheets | textures
Output: the work folder
"""
import sys, os, math, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import variants as V                       # noqa: E402
import mock2 as M2                         # noqa: E402
from variants import *                     # noqa: E402,F401
from scipy.spatial import ConvexHull       # noqa: E402

OUT3 = os.environ.get('AE_BOMB_SCRATCH', '.').rstrip('/') + '/'
HEK = os.environ['AE_BOMB_HEK_TAGS'].rstrip('/') + '/'

# ---------------------------------------------------------------- texture registry (for SPEC3)

USED = {}


def T(path, note=''):
    """decoded texture, recorded for the spec"""
    if path.startswith(TS):
        key, lic = 'TSCE:' + path[len(TS):], 'TSC:E (CMT, credit)'
    else:
        key = path
        lic = 'digsite (officially sanctioned)' if path.startswith('digsite') else 'stock CE (player disc / HEK)'
    USED.setdefault(key, [lic, set()])
    if note:
        USED[key][1].add(note)
    return tex(path)


def crop(t, x0, y0, x1, y1):
    return np.ascontiguousarray(t[y0:y1, x0:x1])


# texture paths
DET_DIRTY = TS + 'cmt/textures/detail_maps/detail_new_metal_dirty'
DET_BURNT = TS + 'cmt/textures/detail_maps/detail_new_metal_burnt'
NRM_SCR = TS + 'cmt/textures/normal_maps/detail_new_metal_scratch'
NRM_THIN = TS + 'cmt/textures/normal_maps/detail_scratches_thin'
NRM_PLATE = TS + 'cmt/textures/normal_maps/detail_boilerplate'
DET_PLATE = TS + 'cmt/textures/detail_maps/detail_boilerplate'
UNSC = TS + 'cmt/textures/decals/UNSC_Decal_Sheet_One'
PELD = TS + 'cmt/vehicles/_shared/pelican/bitmaps/pelican_decal_diffuse'
RLD = TS + 'cmt/weapons/_shared/rocket_launcher/bitmaps/rocket_launcher_decal'
PACK = TS + 'cmt/powerups/ammo_pack/bitmaps/ammo_pack_base_diff'
PACK_N = TS + 'cmt/powerups/ammo_pack/bitmaps/ammo_pack_base_nrml'
CORRODE = 'characters/cyborg/bitmaps/detail corroded metal light'
RW_DET = 'vehicles/rwarthog/bitmaps/rwarthog_detail_metal'
GUNMETAL = 'weapons/assault rifle/fp/bitmaps/diffuse gunmetal'
ELEC = 'levels/a10/decals-environment/bitmaps/decal danger electric'

# decal crops (pixel boxes in the source sheets)
D_EAGLE = (UNSC, (2, 5, 128, 180))
D_UNSC_WORD = (UNSC, (140, 6, 258, 47))
D_DIGITS = (UNSC, (140, 70, 358, 101))
D_ARMORY = (UNSC, (141, 104, 355, 129))
D_CAUTION_W = (UNSC, (141, 132, 251, 156))
D_CAUTION_Y = (UNSC, (141, 156, 251, 180))
D_CAUTION_R = (UNSC, (141, 180, 251, 204))
D_CHEVRON = (UNSC, (4, 180, 127, 348))
D_E419 = (PELD, (8, 150, 100, 180))
D_MAAV = (PELD, (106, 158, 248, 196))
D_D77 = (PELD, (108, 202, 244, 244))
D_DANGER = (PELD, (257, 152, 316, 253))
D_WARNING = (PELD, (325, 124, 429, 149))
D_NOSTEP = (PELD, (325, 99, 429, 124))
D_MANUF = (PELD, (321, 4, 509, 96))
D_ATTN = (RLD, (2, 66, 180, 96))
D_M41 = (RLD, (2, 182, 178, 256))
D_SELFTEST = (RLD, (187, 211, 254, 254))


def dtex(d, note):
    path, box = d
    return crop(T(path, note), *box)


# ---------------------------------------------------------------- materials

def paint(base=None, color=(0.3, 0.33, 0.22), tint=(1, 1, 1), spec=0.25, gloss=28, wear=1.0, det=DET_DIRTY,
          det_tile=(3, 3), det_amt=0.9, nrm=NRM_SCR, nrm_tile=(2, 2), nrm_amt=1.6, note=''):
    return mat(color=color, tex=base, tint=tint, spec=spec, gloss=gloss, wear=wear,
               detail=T(det, 'detail map: ' + note) if det else None, detail_tile=det_tile, detail_amt=det_amt,
               normal=T(nrm, 'normal map: ' + note) if nrm else None, normal_tile=nrm_tile, normal_amt=nrm_amt)


def gunmetal(note='timer housing'):
    return paint(color=(0.13, 0.135, 0.145), spec=0.45, gloss=40, det=DET_BURNT, det_tile=(1.5, 1.5), det_amt=1.0,
                 nrm=NRM_SCR, nrm_tile=(1, 1), nrm_amt=2.0, wear=1.2, note=note)


def olive_steel(note='mount plate'):
    return paint(color=(0.27, 0.30, 0.19), det=DET_DIRTY, det_tile=(1, 1), det_amt=1.0, nrm=NRM_PLATE,
                 nrm_tile=(2, 2), nrm_amt=0.8, wear=1.3, note=note)


def steel(note='bolts, buckles'):
    return paint(color=(0.48, 0.48, 0.47), spec=0.7, gloss=60, det=DET_BURNT, det_tile=(1, 1), det_amt=0.6,
                 nrm=None, wear=0.6, note=note)


def webbing(note='strap webbing'):
    w = crop(T(PACK, 'strap webbing (stitched strap strip)'), 1, 66, 27, 127).copy()
    g = w[..., :3].mean(-1, keepdims=True)
    w[..., :3] = (0.18 + g * 0.55) * np.array((0.62, 0.68, 0.45), np.float32)
    n = crop(T(PACK_N, 'strap webbing stitching'), 1, 66, 27, 127)
    return mat(tex=w, spec=0.08, gloss=10, normal=n, normal_amt=1.0, tile=(1, 1), normal_tile=(1, 1))


def tape(note='tape'):
    w = crop(T(PACK, 'tape (crumpled fabric, greyed)'), 44, 2, 128, 60).copy()
    g = w[..., :3].mean(-1, keepdims=True)
    w[..., :3] = 0.10 + g * 0.42
    n = crop(T(PACK_N, 'tape creases'), 44, 2, 128, 60)
    return mat(tex=w, tint=(0.95, 0.97, 1.0), spec=0.4, gloss=24, normal=n, normal_amt=0.9)


def decal_mat(d, note, tint=(1, 1, 1)):
    return mat(tex=dtex(d, note), tint=tint, alpha_test=True, spec=0.12, detail=T(DET_DIRTY, 'decal grime'),
               detail_tile=(1, 1), detail_amt=0.7)


RUBBER = mat(color=(0.035, 0.035, 0.04), spec=0.35, gloss=30)
W_RED2 = mat(color=(0.62, 0.05, 0.03), spec=0.5, gloss=45)
W_YEL2 = mat(color=(0.78, 0.6, 0.05), spec=0.5, gloss=45)
W_BLU2 = mat(color=(0.06, 0.16, 0.55), spec=0.5, gloss=45)
W_BLK2 = mat(color=(0.03, 0.03, 0.03), spec=0.5, gloss=45)
GLASS = mat(color=(0.05, 0.07, 0.08), spec=1.0, gloss=120, glass=0.10)


# ---------------------------------------------------------------- geometry helpers

def bevel_box(size, bevel, center=(0, 0, 0)):
    """box with chamfered edges, flat-shaded, box-projected UVs (1 UV unit = the box's largest side)"""
    sx, sy, sz = np.array(size, float) / 2
    b = min(bevel, sx * 0.9, sy * 0.9, sz * 0.9)
    pts = []
    for a in (-1, 1):
        for c in (-1, 1):
            for d in (-1, 1):
                pts += [(a * (sx - b), c * (sy - b), d * sz), (a * (sx - b), c * sy, d * (sz - b)),
                        (a * sx, c * (sy - b), d * (sz - b))]
    pts = np.array(pts)
    h = ConvexHull(pts)
    P, N, UV, Tt = [], [], [], []
    L = max(size)
    for simp, eq in zip(h.simplices, h.equations):
        n = eq[:3]
        tri = pts[simp]
        if np.dot(np.cross(tri[1] - tri[0], tri[2] - tri[0]), n) < 0:
            tri = tri[[0, 2, 1]]
        ax = np.argmax(np.abs(n))
        o = [k for k in range(3) if k != ax]
        base = len(P)
        for p in tri:
            P.append(p + np.array(center))
            N.append(n)
            UV.append((p[o[0]] / L + 0.5, p[o[1]] / L + 0.5))
        Tt.append((base, base + 1, base + 2))
    return np.array(P), np.array(N), np.array(UV), np.array(Tt)


def cyl(r, h, k=12, center=(0, 0, 0), caps=True):
    prof = [(0, 0), (r, 0), (r, h), (0, h)] if caps else [(r, 0), (r, h)]
    P, N, UV, Tt = V.lathe(prof, k)
    return P + np.array(center), N, UV, Tt


def add_decal(a, d, note, center, facing, up, width, tint=(1, 1, 1), lift=0.0006):
    t = dtex(d, note)
    h = width * t.shape[0] / t.shape[1]
    R = V.frame(facing, up)
    xf = Xf(1, R, center)
    a.add(*quad((lift, 0, 0), (0, width, 0), (0, 0, -h)), decal_mat(d, note, tint), xf, tag='decal', cast=False)
    return h


def strap_p(a, tags, axis, value, width=0.012, thick=0.0025, m=None, buckle=True, side_buckle=0):
    """V.strap with a textured band and a steel buckle"""
    n0 = len(a.items)
    V.strap(a, tags, axis, value, width=width, thick=thick, m=m or webbing(), buckle=False)
    P, N, UV, Tt, mm, cast, tag, grp = a.items[n0]
    L = np.linalg.norm(np.diff(P[::4], axis=0), axis=1).sum()
    UV = UV.copy()
    UV[:, 1] *= L / (width * 2.2)           # weave not stretched along the band
    a.items[n0] = (P, N, UV, Tt, mm, cast, tag, grp)
    if buckle:
        ring = P[::4]
        ax2 = [k for k in range(3) if k != axis]
        k = np.argmax(ring[:, ax2[side_buckle]] + 0.001 * ring[:, ax2[1 - side_buckle]])
        c = ring[k]
        sz = np.full(3, width * 1.45)
        nrm = np.zeros(3)
        nrm[ax2[side_buckle]] = 1
        sz[ax2[side_buckle]] = thick * 2.2
        a.add(*bevel_box(sz, thick * 0.6, c + nrm * thick), steel(), tag='buckle')
        sz2 = sz.copy()
        sz2[axis] *= 0.35
        a.add(*bevel_box(sz2 * np.where(nrm > 0, 1.0, 0.75), thick * 0.4, c + nrm * thick * 2.0), steel(), tag='buckle')


def cable(a, ctrl, m=None, r=0.0018):
    Pm, Nm, UVm, Tm = sweep(spline(ctrl, 10), circle(r, 8))
    a.add(Pm, Nm, UVm, Tm, m or W_BLK2, tag='wire')


def console(a, center, facing, up=(0, 0, 1), s=1.0, beacon=True, label=D_CAUTION_Y, side_label=D_MAAV,
            plate=True):
    """mounted timer console: bolted plate, chamfered gunmetal housing, recessed digsite timer face behind glass,
    keypad with an ARM key, caution label, caged beacon on top, cable gland. Returns gland exit point (world)."""
    st = a.state
    w, h, d = 0.05 * s, 0.028 * s, 0.014 * s
    R = V.frame(facing, up)
    xf = Xf(1, R, center)
    a.group = np.array(center, float)
    if plate:
        pw, ph = w + 0.03 * s, h + 0.034 * s
        a.add(*bevel_box((0.003 * s, pw, ph), 0.001 * s, (-d / 2 - 0.0012 * s, 0, -0.002 * s)), olive_steel(), xf)
        for yy in (-1, 1):
            for zz in (-1, 1):
                Pb, Nb, UVb, Tb = cyl(0.0032 * s, 0.0025 * s, 6)
                Pb = Pb @ rot('y', 90).T + np.array([-d / 2 + 0.0002 * s, yy * (pw / 2 - 0.0055 * s),
                                                     -0.002 * s + zz * (ph / 2 - 0.0055 * s)])
                Nb = Nb @ rot('y', 90).T
                a.add(Pb, Nb, UVb, Tb, steel(), xf)
    hh = h + 0.02 * s
    a.add(*bevel_box((d, w + 0.014 * s, hh), 0.0025 * s, (0, 0, -0.002 * s)), gunmetal(), xf, tag='timer')
    # recessed screen: dark well, face, glass
    a.add(*bevel_box((0.002 * s, w + 0.004 * s, h + 0.004 * s), 0.0006 * s, (d / 2 - 0.0004 * s, 0, 0.003 * s)),
          mat(color=(0.02, 0.02, 0.022), spec=0.2), xf)
    tt = V.timer_tex('planted' if st == 'planted' else 'armed')
    T(P_TIMER, 'timer face (self-illuminated)')
    face = mat(tex=tt, emissive=(1, 1, 1), emis_tex=tt, emis_gain=1.5 if st == 'planted' else 1.15)
    a.add(*quad((d / 2 + 0.0008 * s, 0, 0.003 * s), (0, w, 0), (0, 0, -h)), face, xf)
    a.add(*quad((d / 2 + 0.0016 * s, 0, 0.003 * s), (0, w + 0.002 * s, 0), (0, 0, -h - 0.002 * s)), GLASS, xf,
          cast=False)
    # keypad: 3 grey keys + red ARM key
    for i in range(4):
        km = mat(color=(0.55, 0.05, 0.03), spec=0.5, gloss=50) if i == 3 else steel('keys')
        a.add(*bevel_box((0.003 * s, 0.0075 * s, 0.0045 * s), 0.0008 * s,
                         (d / 2 + 0.001 * s, -w / 2 + 0.006 * s + i * 0.0115 * s, -h / 2 - 0.0065 * s)), km, xf,
              tag='key')
    # labels
    if label:
        t = dtex(label, 'console label')
        lw = 0.022 * s
        add_decal(a, label, 'console label', xf.p(np.array([[d / 2, w / 2 - 0.004 * s, h / 2 + 0.0058 * s]]))[0],
                  R[:, 0], R[:, 2], lw)
    if side_label:
        add_decal(a, side_label, 'console side serial', xf.p(np.array([[0, w / 2 + 0.007 * s, -0.001 * s]]))[0],
                  R[:, 1], R[:, 2], 0.012 * s, tint=(0.95, 0.95, 0.9))
    # LED
    g = 3.0 if st == 'planted' else 2.0
    pos = xf.p(np.array([[d / 2 + 0.001 * s, -w / 2 + 0.002 * s, h / 2 + 0.0062 * s]]))[0]
    a.add(*sphere(0.0025 * s, pos, 10), mat(color=V.RED, emissive=V.RED, emis_gain=g), tag='led')
    if st == 'planted':
        a.lights.append((pos + norm(facing) * 0.02, (1.0, 0.1, 0.05), 0.05))
    # beacon on a round base with a wire cage
    if beacon:
        top = xf.p(np.array([[0, 0.012 * s, hh / 2 - 0.002 * s]]))[0]
        upv = R[:, 2]
        Pb, Nb, UVb, Tb = cyl(0.0075 * s, 0.003 * s, 14)
        Rb = V.frame(R[:, 0], upv) @ rots(('y', 0))
        Rz = np.stack([R[:, 0], R[:, 1], upv], 1)
        a.add(Pb @ Rz.T + top, Nb @ Rz.T, UVb, Tb, gunmetal('beacon base'))
        bpos = top + upv * 0.003 * s
        gain = 2.6 if st == 'planted' else 1.6
        a.gbx(P_BEAC, Xf(0.17 * s, Rz, bpos), V.beacon_mats(gain), tag='beacon')
        T('levels/b30/devices/light fixtures/light fixture ceiling1/bitmaps/light fixtures', 'beacon frame (stock)')
        for ang in (0, 90):
            Rc = Rz @ rot('z', ang)
            arc = [bpos + Rc @ np.array([math.cos(t) * 0.0105 * s, 0, math.sin(t) * 0.0105 * s])
                   for t in np.linspace(0, math.pi, 9)]
            Pm, Nm, UVm, Tm = sweep(arc, circle(0.0008 * s, 6))
            a.add(Pm, Nm, UVm, Tm, steel('beacon cage'))
        lt = bpos + upv * 0.012 * s
        a.lights.append((lt, (1.0, 0.08, 0.04) if st == 'planted' else (0.6, 0.05, 0.03), 0.09 if st == 'planted' else 0.05))
    # cable gland on the side
    gl = xf.p(np.array([[0, -(w / 2 + 0.007 * s), -0.004 * s]]))[0]
    Pg, Ng, UVg, Tg = cyl(0.0035 * s, 0.004 * s, 10)
    Rg = np.stack([R[:, 0], R[:, 2], -R[:, 1]], 1)
    a.add(Pg @ Rg.T + gl, Ng @ Rg.T, UVg, Tg, steel('cable gland'))
    a.group = None
    return gl - R[:, 1] * 0.004 * s, R


# ---------------------------------------------------------------- bodies

def hp_stock_tex():
    """stock CE health kit diffuse: cross painted out, white -> olive drab (never reads as health)"""
    t = V.healthpack_tex(tint=(0.40, 0.44, 0.30))
    T('powerups/healthpack/bitmaps/healthpack', 'A body diffuse, recoloured by recipe')
    return t


def tsce_tex():
    t = V.tsce_hp_tex()
    T(TS + 'cmt/powerups/h_powerup_pack/_health/bitmaps/h_powerup_pack_health_diff', 'B body diffuse, recoloured by recipe')
    return t


def rl_mats(note):
    rt = T('powerups/rocket launcher ammo/bitmaps/rocket launcher ammo', note)
    base = dict(tex=rt, spec=0.3, gloss=30, detail=T(DET_DIRTY, 'detail: SPNKr case'), detail_tile=(3, 3),
                detail_amt=0.6, normal=T(NRM_THIN, 'normal: SPNKr case'), normal_tile=(2, 2), normal_amt=1.5, wear=0.8)
    return {0: mat(tex=rt, alpha_test=True), 1: mat(**base), 2: mat(**base)}


def mini_rl(a, pos, s, R, tag='rl'):
    lo, hi = bbox(M2.P_RL if hasattr(M2, 'P_RL') else V.P_RL)
    c = (lo + hi) / 2
    c[2] = lo[2]
    a.gbx(V.P_RL, Xf(s, R, np.array(pos) - R @ (c * s)), rl_mats('SPNKr case diffuse'), tag=tag)


def A3(st):
    """stock CE health kit IED"""
    a = Asm(st)
    m = paint(base=hp_stock_tex(), tint=(1, 1, 1), det=DET_DIRTY, det_tile=(3, 3), det_amt=0.8, nrm=NRM_SCR,
              nrm_tile=(2, 2), nrm_amt=1.6, wear=1.0, note='A body')
    a.gbx(V.P_HP, Xf(1.0, None, (0, 0, -zmin(V.P_HP))), {0: m}, tag='body')
    mini_rl(a, (0, 0, 0.0795), 0.3, rot('z', 90))
    for y in (-0.048, 0.048):
        strap_p(a, ('body', 'rl'), 1, y, width=0.013)
    gl, R = console(a, (0.08 + 0.0085, 0.0, 0.041), (1, 0, 0))
    # lid stencils
    add_decal(a, D_EAGLE, 'A lid emblem', (-0.055, 0.0, 0.0802), (0, 0, 1), (-1, 0, 0), 0.042, tint=(0.9, 0.9, 0.85))
    add_decal(a, D_CAUTION_Y, 'A lid caution', (0.054, 0.0, 0.0802), (0, 0, 1), (-1, 0, 0), 0.05)
    add_decal(a, D_E419, 'A side serial', (-0.0802, 0.0, 0.05), (-1, 0, 0), (0, 0, 1), 0.04, tint=(0.9, 0.9, 0.85))
    # cables: from the gland up over the lid edge into the case end
    cable(a, [gl, gl + np.array([0.004, -0.008, 0.012]), (0.07, -0.03, 0.09), (0.03, -0.05, 0.104), (0.0, -0.06, 0.1)], W_RED2)
    cable(a, [gl, gl + np.array([0.006, -0.004, 0.014]), (0.072, -0.02, 0.093), (0.035, -0.035, 0.108), (0.0, -0.04, 0.106)], W_BLK2)
    return a, dict(show=(1, 0, 0), up=(0, 0, 1))


def B3(st):
    """TSC:E med pack IED"""
    a = Asm(st)
    m = paint(base=tsce_tex(), det=DET_DIRTY, det_tile=(3, 3), det_amt=0.7, nrm=NRM_SCR, nrm_tile=(2, 2),
              nrm_amt=1.4, wear=1.0, note='B body')
    a.gbx(V.P_THP, Xf(1, None, (0, 0, -zmin(V.P_THP))), {0: m}, tag='body')
    mini_rl(a, (0, 0, 0.069), 0.3, rot('z', 90))
    for y in (-0.05, 0.05):
        strap_p(a, ('body', 'rl'), 1, y, width=0.013)
    gl, R = console(a, (0.092 + 0.0085, 0.0, 0.036), (1, 0, 0))
    add_decal(a, D_EAGLE, 'B lid emblem', (-0.06, 0.0, 0.0705), (0, 0, 1), (-1, 0, 0), 0.04, tint=(0.9, 0.9, 0.85))
    add_decal(a, D_WARNING, 'B lid warning', (0.06, 0.0, 0.0705), (0, 0, 1), (-1, 0, 0), 0.045)
    add_decal(a, D_E419, 'B side serial', (-0.0925, 0.0, 0.04), (-1, 0, 0), (0, 0, 1), 0.04, tint=(0.9, 0.9, 0.85))
    cable(a, [gl, gl + np.array([0.004, -0.008, 0.012]), (0.08, -0.03, 0.08), (0.03, -0.05, 0.094), (0.0, -0.06, 0.09)], W_RED2)
    cable(a, [gl, gl + np.array([0.006, -0.004, 0.014]), (0.082, -0.02, 0.083), (0.035, -0.035, 0.098), (0.0, -0.04, 0.096)], W_BLK2)
    return a, dict(show=(1, 0, 0), up=(0, 0, 1))


def E1P(st):
    """E1-S polished: 3 SPNKr cases at 0.355, webbing straps, mounted console on top"""
    a = Asm(st)
    s = 0.355
    w = 0.091 * s / 0.5 * 0.5 / 0.5 * 0.5   # case width at this scale (0.0646)
    cw = 0.182 * s
    ch = 0.1 * s
    for y in (-cw / 2 - 0.0005, cw / 2 + 0.0005):
        mini_rl(a, (0, y, 0), s, rot('z', 180), tag='body')
    mini_rl(a, (0, 0, ch), s, rot('z', 180), tag='body')
    for x in (-0.055, 0.055):
        strap_p(a, ('body',), 0, x, width=0.012, side_buckle=1)
    f = norm((0, 0.75, 1.0))
    gl, R = console(a, np.array((0.0, 0.006, 2 * ch)) + f * 0.009, f, up=(0, -1, 0.75), s=0.95)
    add_decal(a, D_CAUTION_Y, 'E tape caution', (0.0, -cw - 0.001, 0.035), (0, -1, 0), (0, 0, 1), 0.04)
    for y, m in ((-cw / 2, W_RED2), (cw / 2, W_YEL2), (0.0, W_BLK2)):
        z = ch / 2 if y else ch * 1.5
        cable(a, [(0.0872, y, z), (0.095, y * 0.9, z + 0.008), (0.06, y * 0.5 + 0.006, 2 * ch + 0.012), gl], m)
    return a, dict(show=f, up=(0, -1, 0.75))


def coil_p(a, xf, tag='coil'):
    """mini fusion coil, textured: worn steel body with detail/normal maps, gunmetal caps, glowing ribbed band,
    UNSC chevron + caution stencils"""
    H, R = 0.11, 0.032
    st = a.state
    body = paint(color=(0.62, 0.64, 0.66), spec=0.45, gloss=40, det=CORRODE, det_tile=(2, 1), det_amt=0.6,
                 nrm=NRM_SCR, nrm_tile=(2, 1), nrm_amt=1.5, wear=1.0, note='coil body')
    capm = paint(color=(0.10, 0.105, 0.11), spec=0.4, gloss=40, det=DET_BURNT, det_tile=(1, 1), det_amt=0.8,
                 nrm=NRM_PLATE, nrm_tile=(2, 2), nrm_amt=0.9, wear=1.3, note='coil caps')
    cap = [(0, 0), (R + 0.003, 0), (R + 0.003, 0.014), (R, 0.016)]
    cap2 = [(R, H - 0.016), (R + 0.003, H - 0.014), (R + 0.003, H), (0.012, H), (0.012, H + 0.006), (0.0, H + 0.006)]
    for prof, m in ((cap, capm), ([(R, 0.016), (R, 0.044)], body), ([(R, 0.068), (R, H - 0.016)], body), (cap2, capm)):
        a.add(*V.lathe(prof, k=28, u_scale=3), m, xf, tag=tag)
    g = 1.7 if st == 'planted' else 1.3
    bm = mat(tex=V.BAND, emissive=(1, 1, 1), emis_tex=V.BAND, emis_gain=g)
    Pb, Nb, UVb, Tb = V.lathe([(R + 0.0008, 0.044), (R + 0.0008, 0.068)], k=28)
    UVb[:, 1] = (UVb[:, 1] - 0.044) / 0.024
    a.add(Pb, Nb, UVb, Tb, bm, xf, tag=tag)
    # chevron stencil wrapped as a ring on the upper body (UNSC sheet chevrons, yellow paint)
    chm = mat(tex=V.HAZ, spec=0.2, detail=T(DET_DIRTY, 'detail: coil hazard ring'), detail_tile=(4, 1),
              detail_amt=0.9, wear=0.6)
    Ph, Nh, UVh, Th = V.lathe([(R + 0.0006, 0.074), (R + 0.0006, 0.086)], k=28, u_scale=8)
    UVh[:, 1] = (UVh[:, 1] - 0.074) / 0.012 * 0.5
    a.add(Ph, Nh, UVh, Th, chm, xf, tag=tag, cast=False)
    # caution stencil on the lower body, facing the coil's +x
    add_decal(a, D_CAUTION_R, 'coil caution', xf.p(np.array([[R + 0.0004, 0, 0.03]]))[0], xf.R[:, 0], xf.R[:, 2], 0.02)
    if st == 'planted':
        a.lights.append((xf.p(np.array([[0, 0, 0.056]]))[0], (0.5, 0.25, 0.05), 0.05))
    return xf.p(np.array([[0, 0, H + 0.006]]))[0]


def D1P(st):
    """D1-S polished: 3 textured mini coils in a row, webbing straps, mounted console"""
    a = Asm(st)
    k = M2.K.get('D1S') or 0.854
    tops = []
    for i, y in enumerate((-0.07, 0.0, 0.07)):
        tops.append(coil_p(a, Xf(1, rot('z', -10 + 10 * i), (0, y, 0))))
    for z in (0.009, 0.1):
        strap_p(a, ('coil',), 2, z, width=0.01)
    gl, R = console(a, (0.035 + 0.0085, 0.0, 0.034), (1, 0, 0), s=1.0)
    for t, m in zip(tops, (W_RED2, W_BLK2, W_YEL2)):
        cable(a, [t, t + np.array([0, 0, 0.012]), (0.02, t[1] * 0.6, 0.13), (0.045, t[1] * 0.3 - 0.02, 0.07), gl], m)
    return M2.scale_asm(a, k), dict(show=(1, 0, 0), up=(0, 0, 1))


GLT = None


def gl_mats():
    gt = T(M2.GL + 'bitmaps/grenade_launcher_grenade_diff', '40mm round diffuse')
    return {'default': mat(tex=gt, spec=0.5, gloss=45, detail=T(DET_DIRTY, 'detail: 40mm rounds'), detail_tile=(2, 2),
                           detail_amt=0.7, normal=T(NRM_THIN, 'normal: 40mm rounds'), normal_tile=(1, 1),
                           normal_amt=1.4, wear=0.7)}


def round40(a, pos, s, R, tag='shell'):
    """40mm round, nose +x in its frame; pos = case base on the axis"""
    a.gbx(M2.P_GL, Xf(s, R, np.array(pos) - R @ (np.array([-0.073, 0, 0]) * s)), gl_mats(), tag=tag)


S40 = 0.3          # 0.42 m -> 0.125 m long, 42 mm across: a real 40mm round in the cyborg's hands
D40 = 0.0465 * S40  # spacing


def stencil_40(a, center, facing, up, w):
    """'40' from the UNSC digit row, yellow, + a short MISRIAH ARMORY line"""
    dig = dtex(D_DIGITS, '40mm round stencil digits')
    cw = dig.shape[1] / 10
    four = dig[:, int(4 * cw):int(5 * cw)]
    zero = dig[:, int(0 * cw):int(1 * cw)]
    t = np.ascontiguousarray(np.concatenate([four, zero], 1))
    R = V.frame(facing, up)
    xf = Xf(1, R, center)
    h = w * t.shape[0] / t.shape[1]
    a.add(*quad((0.0004, 0, 0), (0, w, 0), (0, 0, -h)), mat(tex=t, tint=(0.95, 0.8, 0.15), alpha_test=True, spec=0.1),
          xf, tag='decal', cast=False)


def G1H(st):
    """18 x 40mm rounds (6 wide x 3 high) taped into a compact charge on a steel carrier plate"""
    a = Asm(st)
    L = 0.1364 * S40
    NW = 6
    plate_h = 0.003
    a.add(*bevel_box((L + 0.012, NW * D40 + 0.012, plate_h), 0.001, (L / 2 - 0.006, 0, plate_h / 2)),
          olive_steel('carrier plate'), tag='body')
    ys = [(i - (NW - 1) / 2) * D40 for i in range(NW)]
    for j in range(3):
        for i, y in enumerate(ys):
            z = plate_h + D40 / 2 + j * D40
            round40(a, (-0.006, y, z), S40, rot('x', 41 * i + 67 * j), tag='body')
    for x in (0.003, 0.017):
        strap_p(a, ('body',), 0, x, width=0.008, thick=0.0016, m=tape(), buckle=False)
    for j in range(3):
        stencil_40(a, (0.009, NW / 2 * D40 + 0.0003, plate_h + D40 / 2 + j * D40), (0, 1, 0), (0, 0, 1), 0.007)
    add_decal(a, D_CAUTION_Y, 'G1 tape caution', (0.010, -NW / 2 * D40 - 0.0028, plate_h + 1.5 * D40), (0, -1, 0),
              (0, 0, 1), 0.016)
    top = plate_h + 3 * D40
    f = norm((0, 0.6, 1.0))
    gl, R = console(a, np.array((0.006, 0.0, top + 0.002)) + f * 0.007, f, up=(0, -1, 0.6), s=0.55, plate=True,
                    side_label=None)
    for i, m in zip((0, 2, 3, 5), (W_RED2, W_YEL2, W_BLU2, W_BLK2)):
        y = ys[i]
        nose = np.array([-0.006 + L, y, plate_h + D40 / 2 + 2 * D40])
        cable(a, [nose, nose + np.array([0.004, 0, 0.004]), (L * 0.6, y * 0.6, top + 0.006), gl], m, r=0.0011)
    return a, dict(show=(0, 0, 1), up=(1, 0, 0), hold=f)


def G2H(st):
    """ammo can at pickup scale (0.40 m) + 6 x 40mm rounds taped to its face + console on the lid"""
    a = Asm(st)
    sc = 1.0
    can = paint(base=T('powerups/shotgun ammo/bitmaps/sg ammo', 'G2 ammo can diffuse'), det=DET_DIRTY, det_tile=(3, 3),
                det_amt=0.6, nrm=NRM_SCR, nrm_tile=(2, 2), nrm_amt=1.3, wear=1.0, note='ammo can')
    can_d = mat(tex=T('powerups/shotgun ammo/bitmaps/sg ammo'), alpha_test=True, spec=0.25,
                detail=T(DET_DIRTY), detail_tile=(3, 3), detail_amt=0.6, wear=1.0)
    a.gbx(V.P_AMMO, Xf(sc, None, (0, 0, -zmin(V.P_AMMO, sc))), {0: can, 1: can_d}, tag='body')
    yf = 0.025 * sc
    L = 0.1364 * S40
    for j in range(2):
        for i in range(3):
            x0 = -0.062 + i * (L + 0.002)
            round40(a, (x0, yf + D40 / 2, 0.012 + D40 / 2 + j * D40 * 1.02), S40, rot('x', 37 * i + 61 * j), tag='shell')
    for x in (-0.045, -0.002, 0.04):
        strap_p(a, ('body', 'shell'), 0, x, width=0.007, thick=0.0016, m=tape(), buckle=False)
    for i in range(3):
        stencil_40(a, (-0.062 + i * (L + 0.002) + L * 0.32, yf + D40 + 0.0003, 0.012 + D40 / 2 + D40 * 1.02),
                   (0, 1, 0), (0, 0, 1), 0.007)
    top = 0.1 * sc
    f = norm((0, 0.75, 1.0))
    gl, R = console(a, np.array((0.0, 0.005, top)) + f * 0.008, f, up=(0, -1, 0.75), s=0.75, side_label=None)
    for i, m in zip(range(3), (W_RED2, W_YEL2, W_BLK2)):
        x0 = -0.062 + i * (L + 0.002) + L
        nose = np.array([x0, yf + D40 / 2, 0.012 + D40 * 1.5 + 0.001])
        cable(a, [nose, nose + np.array([0.003, 0.006, 0.006]), (x0 * 0.5, yf + 0.004, top + 0.006), gl], m, r=0.0011)
    return a, dict(show=(0, 1, 0), up=(0, 0, 1))


BUILD3 = {
    'A3': (A3, 'A  Stock CE health kit IED', 'recoloured kit + mini SPNKr case + mounted console'),
    'B3': (B3, 'B  TSC:E med pack IED', 'recoloured med pack + mini SPNKr case + mounted console'),
    'E1P': (E1P, 'E1-S Rocket case bundle, polished', '3 SPNKr cases, webbing, mounted console'),
    'D1P': (D1P, 'D1-S Mini coil row, polished', '3 textured mini coils, webbing, mounted console'),
    'G1H': (G1H, 'G1-H 40mm compact charge', '18 rounds (6 x 3) at real 40mm size on a carrier plate'),
    'G2H': (G2H, 'G2-H 40mm rounds on ammo can', 'ammo can at pickup scale + 6 rounds + console'),
}


def setup():
    os.makedirs(OUT3, exist_ok=True)
    M2.compute_k()
    M2.OUT2 = OUT3
    M2.BUILD.update(BUILD3)


def sheets():
    from PIL import Image, ImageDraw, ImageFont
    B = ImageFont.truetype('/usr/share/fonts/noto/NotoSans-Bold.ttf', 26)
    Rf = ImageFont.truetype('/usr/share/fonts/noto/NotoSans-Regular.ttf', 18)
    BG = (16, 17, 21)
    tw, th = 600, 450
    views = ('hero', 'planted', 'carried', 'hands')

    def grid(ids, title, out, sub=''):
        im = Image.new('RGB', (len(views) * (tw + 10) + 10, 80 + len(ids) * (th + 10)), BG)
        d = ImageDraw.Draw(im)
        d.text((14, 12), title, font=B, fill=(230, 233, 240))
        d.text((14, 48), sub, font=Rf, fill=(150, 158, 172))
        for r, v in enumerate(ids):
            for c, k in enumerate(views):
                t = Image.open(OUT3 + f'{v}_{k}.png').resize((tw, th), Image.LANCZOS)
                im.paste(t, (10 + c * (tw + 10), 80 + r * (th + 10)))
        im.save(OUT3 + out)
        print(OUT3 + out, im.size)

    # AB: A and B side by side, one row per view
    im = Image.new('RGB', (2 * (tw + 10) + 10, 80 + 4 * (th + 10)), BG)
    d = ImageDraw.Draw(im)
    d.text((14, 12), 'Health-pack-sized IED: A stock CE health kit | B TSC:E med pack', font=B, fill=(230, 233, 240))
    d.text((14, 48), 'rows: hero 3/4 | planted | on the back | in the hands. Same payload (mini SPNKr case) and console on both.',
           font=Rf, fill=(150, 158, 172))
    for c, v in enumerate(('A3', 'B3')):
        for r, k in enumerate(views):
            t = Image.open(OUT3 + f'{v}_{k}.png').resize((tw, th), Image.LANCZOS)
            im.paste(t, (10 + c * (tw + 10), 80 + r * (th + 10)))
    im.save(OUT3 + 'AB.png')
    print(OUT3 + 'AB.png', im.size)
    grid(['E1P', 'D1P'], 'Polished favourites: E1-S rocket case bundle, D1-S mini coil row', 'polished-favourites.png',
         'hero | planted | on the back | in the hands')
    grid(['G1H', 'G2H'], '40mm rounds at human size: G1-H compact charge, G2-H rounds on the ammo can', 'G-H.png',
         'rounds 0.125 m long, 42 mm across (were 0.42 m); hero | planted | on the back | in the hands')


def textures_md():
    lines = []
    for k in sorted(USED):
        lic, notes = USED[k]
        lines.append('| `%s` | %s | %s |' % (k, lic, '; '.join(sorted(notes))))
    return '\n'.join(lines)


if __name__ == '__main__':
    setup()
    cmd = sys.argv[1]
    if cmd == 'views':
        for v in sys.argv[2:]:
            t0 = time.time()
            M2.run2(v)
            print(v, round(time.time() - t0, 1), 's', flush=True)
    elif cmd == 'one':
        M2.run2(sys.argv[2], tuple(sys.argv[3:]))
    elif cmd == 'sheets':
        sheets()
    elif cmd == 'textures':
        for k in BUILD3:
            for st in ('hero', 'planted'):
                BUILD3[k][0](st)
        print(textures_md())
