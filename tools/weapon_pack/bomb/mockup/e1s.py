"""E1-S (owner's pick, the AE SnD bomb): timer console rotated 45 / 90 degrees clockwise (seen from behind the
wearer), no grey clamp blocks (wire clips, strap buckles, cable gland removed), SPNKr latch plates recoloured
(they read as ID photos), pack raised to the shoulder-blade line and fitted flush, harness belt over the pack.

usage: python3 e1s.py <45|90> <views...>   views: rear side front hero planted ; python3 e1s.py compare
Output: the work folder*.png, E1S_timer90_rear.png, E1S_timer_compare.png
"""
import sys, os, math, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import art as A                            # noqa: E402
from art import *                          # noqa: E402,F401
BP = A.BPK
OUT4 = A.OUT4

# ---------------------------------------------------------------- latch plates: recolour the photo-like plates

_RLT = None


def rl_tex_fixed():
    global _RLT
    if _RLT is None:
        t = A.T('powerups/rocket launcher ammo/bitmaps/rocket launcher ammo', 'SPNKr case diffuse (latch plates recoloured)').copy()
        c = t[..., :3]
        lum = c.mean(-1)
        sat = c.max(-1) - c.min(-1)
        box = np.zeros(lum.shape, bool)
        for x0, x1 in ((60, 90), (166, 196)):
            for y0, y1 in ((68, 84), (148, 196)):
                box[y0:y1, x0:x1] = True
        m = box & (lum > 0.30) & (sat < 0.18)
        # dull steel latch: plate tone from the tech rack plate, keeping only a faint shading
        steel = A.STEEL * 1.2
        t[..., :3][m] = steel * (0.75 + 0.25 * (lum[m, None] - 0.3))
        _RLT = t
    return _RLT


def rl_mats_fixed(note):
    rt = rl_tex_fixed()
    base = dict(tex=rt, spec=0.3, gloss=30, detail=A.T(A.PL.DET_DIRTY), detail_tile=(3, 3), detail_amt=0.6,
                normal=A.T(A.PL.NRM_THIN), normal_tile=(2, 2), normal_amt=1.5, wear=0.8)
    return {0: mat(tex=rt, alpha_test=True), 1: mat(**base), 2: mat(**base)}


A.PL.rl_mats = rl_mats_fixed

# ---------------------------------------------------------------- no clamp blocks: wires without clips, no gland

_cable_raw = A._cable0


def cable_noclip(a, ctrl, m=None, r=0.0018):
    n = getattr(a, '_nwires', 0)
    if n >= 2:
        return
    a._nwires = n + 1
    pts = [np.array(p, float) for p in ctrl]
    out = [pts[0]]
    for p0, p1 in zip(pts[:-1], pts[1:]):
        mid = (p0 + p1) / 2
        out += [mid - np.array([0, 0, np.linalg.norm(p1 - p0) * 0.12]), p1]
    _cable_raw(a, out, A.W_A if n == 0 else A.W_B, r * 1.15)


A.PL.cable = cable_noclip
BP.cable = cable_noclip


def drop_gland(a, n_before):
    """the gland is the last item the console adds"""
    if len(a.items) > n_before:
        a.items.pop()


# ---------------------------------------------------------------- the build

TIDY_WIRES = True


def E1S(st, rot_deg=45):
    a = Asm(st)
    s = 0.355
    cw, ch = 0.182 * s, 0.1 * s
    for y in (-cw / 2 - 0.0005, cw / 2 + 0.0005):
        A.PL.mini_rl(a, (0, y, 0), s, rot('z', 180), tag='body')
    A.PL.mini_rl(a, (0, 0, ch), s, rot('z', 180), tag='body')
    for x in (-0.055, 0.055):
        A.PL.strap_p(a, ('body',), 0, x, width=0.012, buckle=False)
    # console flat on the outer face; up along the long axis (world up when worn), then clockwise as seen from
    # behind the wearer: viewer's right on the face is local -y
    th = math.radians(rot_deg)
    up = np.array([math.cos(th), -math.sin(th), 0.0])
    n0 = len(a.items)
    gl, R = A.PL.console(a, (0.012, 0.0, 2 * ch + 0.0085), (0, 0, 1), up=tuple(up), s=0.95)
    drop_gland(a, n0)
    # wires go straight into the console's lower edge (no gland, no clips)
    entry = np.array((0.012, 0.0, 2 * ch + 0.0085)) - up * 0.028 + np.array([0, 0, 0.004])
    if not TIDY_WIRES:
        for y, z in ((-cw / 2, ch / 2), (cw / 2, ch / 2)):
            cable_noclip(a, [(0.0872, y, z), (0.095, y * 0.9, z + 0.008), (0.07, y * 0.6, 2 * ch + 0.006), entry + np.array([0, y * 0.15, 0])])
    else:
        # owner 10-28 in game: "major slop with the wire placement" (a loop past the bundle's end, a diagonal across
        # the face, a sag into the top case). Tidy: both wires side by side out of the console's top edge, straight
        # along the top case over the strap, over its rounded end, into its end cap; no sag
        top = 2 * ch                                 # the top case's top line
        end = 0.0876                                 # the cases' end caps
        cz = 1.5 * ch                                # the top case's axis
        for k, y0 in enumerate((-0.0045, 0.0045)):
            pts = [(0.0415, y0, top + 0.0080), (0.0470, y0, top + 0.0055), (0.0560, y0, top + 0.0045),
                   (0.0700, y0, top + 0.0030), (0.0820, y0, top + 0.0012), (end + 0.0030, y0, cz + 0.0110),
                   (end + 0.0030, y0, cz + 0.0040), (end - 0.0010, y0, cz + 0.0015)]
            A._cable0(a, [np.array(p, float) for p in pts], A.W_A if k == 0 else A.W_B, 0.0018 * 1.15)
    return a, dict(show=(0, 0, 1), up=(1, 0, 0))


# ---------------------------------------------------------------- placement: higher, flush, belt over the pack

def place_flush(a, info, top_z=0.632):
    R = A.M2.carry_R(info)
    P0 = np.vstack([it[0] for it in a.items]) @ R.T
    h = P0[:, 2].max() - P0[:, 2].min()
    zb, zt = top_z - h, top_z
    W = P0[:, 1].max() - P0[:, 1].min()
    samples = []
    for z in np.linspace(zb + 0.008, zt - 0.008, 10):
        for y in np.linspace(-min(W / 2, 0.05) * 0.9, min(W / 2, 0.05) * 0.9, 7):
            x = BP.back_x(y, z)
            if x is not None:
                samples.append((x, y, z))
    best = None
    for deg in np.arange(-36, 13, 3):
        Rt = rot('y', deg)
        R2 = Rt @ R
        P2 = np.vstack([it[0] for it in a.items]) @ R2.T
        t = np.array([-0.6 - P2[:, 0].max(), -(P2[:, 1].max() + P2[:, 1].min()) / 2, zt - P2[:, 2].max()])
        tris = np.concatenate([(it[0] @ R2.T + t)[it[3]] for it in a.items])
        ds = np.array([BP.raycast((x, y, z), (-1, 0, 0), tris)[0] for x, y, z in samples])
        ds = ds[np.isfinite(ds)]
        if not len(ds):
            continue
        gap = (ds - ds.min()).mean()
        if best is None or gap < best[0]:
            best = (gap, deg, R2, t, ds.min())
    gap, deg, R2, t, dmin = best
    t[0] += dmin - 0.0015
    Pf = np.vstack([it[0] for it in a.items]) @ R2.T + t
    return Xf(1, R2, t), Pf, deg, gap


def back_pad(sc, a, xf, Pf, wm):
    """padded frame sheet between the back armour and the pack: fills the lumbar gap so the side profile is flush"""
    tris = np.concatenate([xf.p(it[0])[it[3]] for it in a.items])
    zb, zt = Pf[:, 2].min() + 0.01, Pf[:, 2].max() - 0.012
    W = min(np.abs(Pf[:, 1]).max() - 0.012, 0.055)
    ys = np.linspace(-W, W, 9)
    zs = np.linspace(zb, zt, 12)
    F = np.zeros((len(zs), len(ys), 3))
    B = np.zeros_like(F)
    for i, z in enumerate(zs):
        for j, y in enumerate(ys):
            x = BP.back_x(y, z)
            x = x if x is not None else Pf[:, 0].max()
            d = BP.raycast((x, y, z), (-1, 0, 0), tris)[0]
            d = d if np.isfinite(d) else 0.0
            F[i, j] = (x + 0.002, y, z)          # against the armour (slightly inside)
            B[i, j] = (x - d - 0.001, y, z)      # against the pack
    P, UV, T = [], [], []
    def grid(G, flip):
        base = len(P)
        nz, ny = G.shape[:2]
        for i in range(nz):
            for j in range(ny):
                P.append(G[i, j]); UV.append((j / (ny - 1), i / (nz - 1) * 3))
        for i in range(nz - 1):
            for j in range(ny - 1):
                a_, b_, c_, d_ = base + i * ny + j, base + i * ny + j + 1, base + (i + 1) * ny + j + 1, base + (i + 1) * ny + j
                T.extend([(a_, c_, b_), (a_, d_, c_)] if flip else [(a_, b_, c_), (a_, c_, d_)])
    grid(B, False)
    grid(F, True)
    # side walls (left, right, bottom, top) joining the two sheets
    edges = [B[:, 0], B[:, -1], B[0, :], B[-1, :]]
    fedges = [F[:, 0], F[:, -1], F[0, :], F[-1, :]]
    for eb, ef in zip(edges, fedges):
        base = len(P)
        for p in eb: P.append(p); UV.append((0, 0))
        for p in ef: P.append(p); UV.append((1, 0))
        n = len(eb)
        for k in range(n - 1):
            T.extend([(base + k, base + k + 1, base + n + k + 1), (base + k, base + n + k + 1, base + n + k)])
    P = np.array(P); T = np.array(T)
    sc.add(P, None, np.array(UV), T, wm)


def harness_snug(sc, Pf, wm):
    """shoulder straps + sternum strap + waist belt (BP.harness), plus a compression belt that runs across the
    chest, under both arms and round the pack's outer face, so the pack is visibly held"""
    BP.harness(sc, Pf, wm, belt=False)
    zc = Pf[:, 2].min() + 0.32 * (Pf[:, 2].max() - Pf[:, 2].min())
    band = np.abs(Pf[:, 2] - zc) < 0.02
    outer = Pf[band, 0].min() - 0.003
    cx = Pf[:, 0].max()
    yw = np.abs(Pf[band, 1]).max() + 0.003
    pts = []
    for y in np.linspace(0.07, -0.07, 7):
        x = BP.front_x(y, zc)
        pts.append(((x if x is not None else 0.05) + 0.004, y, zc))
    for x, y in ((0.0, -0.088), (-0.045, -0.086), (cx - 0.004, -yw), (outer, -yw * 0.85), (outer, 0.0),
                 (outer, yw * 0.85), (cx - 0.004, yw), (-0.045, 0.086), (0.0, 0.088)):
        pts.append((x, y, zc))
    loop = np.array(pts)
    seg = np.vstack([loop, loop[:1]])
    L = np.linalg.norm(np.diff(seg, axis=0), axis=1)
    cum = np.concatenate([[0], np.cumsum(L)])
    sm = np.linspace(0, cum[-1], 140, endpoint=False)
    rs = np.stack([np.interp(sm, cum, seg[:, k]) for k in range(3)], 1)
    Pm, Nm, UVm, Tm = sweep(rs, rect(0.012, 0.0024), side=(0, 0, 1), closed=True)
    UVm[:, 1] *= cum[-1] / 0.026
    sc.add(Pm, Nm, UVm, Tm, wm)


# ---------------------------------------------------------------- views

def render_views(rot_deg, views, prefix):
    if BP.TR is None:
        BP.TR = BP.cyb_tris()
        BP.TRT = BP.cyb_tris(True)
    lab = 'E1-S, timer console rotated %d deg clockwise' % rot_deg
    worn = [v for v in views if v in ('rear', 'side', 'front')]
    if worn:
        a, info = E1S('carried', rot_deg)
        sc = Scene()
        V.cyborg(sc)
        xf, Pf, deg, gap = place_flush(a, info)
        a.emit(sc, xf)
        wm = A.PL.webbing()
        back_pad(sc, a, xf, Pf, wm)
        harness_snug(sc, Pf, wm)
        print('tilt', deg, 'mean gap', round(gap, 4))
        tgt = np.array([0, 0, 0.40])
        cams = {'rear': (205, 12, 1.5, 'rear 3/4'), 'side': (92, 6, 1.5, 'side profile'), 'front': (28, 10, 1.5, 'front 3/4')}
        for v in worn:
            yaw, el, dist, d = cams[v]
            A.render_g(sc, OUT4 + f'{prefix}_{v}.png', V.orbit(tgt, yaw, el, dist), tgt, fov=30, exposure=1.1,
                       amb=0.26, label=(lab, 'worn: top at the shoulder-blade line, flush, harness + belt over the pack. ' + d))
    for v in ('hero', 'planted'):
        if v not in views:
            continue
        a, info = E1S('planted' if v == 'planted' else 'hero', rot_deg)
        sc = Scene()
        a.emit(sc, Xf(1, rot('z', -90), (0, 0, 0)))
        lo, hi = a.bounds()
        c = np.array([0, 0, 0.04])
        if v == 'hero':
            A.render_g(sc, OUT4 + f'{prefix}_hero.png', V.orbit(c, 20, 42, 0.68), c, fov=28, exposure=1.12, amb=0.25,
                       label=(lab, '3/4 view'))
        else:
            A.render_g(sc, OUT4 + f'{prefix}_planted.png', V.orbit(c, 40, 40, 0.72), c, fov=28, exposure=0.72, amb=0.12,
                       key_col=(0.75, 0.75, 0.8), label=(lab + ' - planted', 'fuse running: beacon lit, 2 of 4 timer bars red'))


def compare():
    from PIL import Image, ImageDraw, ImageFont
    B = ImageFont.truetype('/usr/share/fonts/noto/NotoSans-Bold.ttf', 24)
    Rf = ImageFont.truetype('/usr/share/fonts/noto/NotoSans-Regular.ttf', 17)
    tiles = [('E1S_timer45_rear', None, '45 deg, rear'), ('E1S_timer90_rear', None, '90 deg (bars horizontal), rear'),
             ('E1S_timer45_side', None, '45 deg, side profile (flush, raised)'),
             ('E1S_timer45_hero', None, '45 deg, hero'), ('E1S_timer45_planted', None, '45 deg, planted'),
             ('E1S_timer45_hero', (330, 260, 830, 640), 'full-res crop: wire entry, no clamps, latch plates')]
    tw, th = 600, 450
    im = Image.new('RGB', (3 * (tw + 10) + 10, 80 + 2 * (th + 44)), (16, 17, 21))
    d = ImageDraw.Draw(im)
    d.text((14, 12), 'E1-S timer console: 45 deg vs 90 deg clockwise (seen from behind)', font=B, fill=(230, 233, 240))
    d.text((14, 46), 'grey clamp blocks removed, SPNKr latch plates recoloured, pack raised and flush, belt over the pack',
           font=Rf, fill=(150, 158, 172))
    for i, (f, crop, cap) in enumerate(tiles):
        t = Image.open(OUT4 + f + '.png')
        if crop:
            t = t.crop(crop)
        t = t.resize((tw, th), Image.LANCZOS)
        x, y = 10 + (i % 3) * (tw + 10), 80 + (i // 3) * (th + 44)
        im.paste(t, (x, y + 34))
        d.text((x + 4, y + 6), cap, font=Rf, fill=(240, 200, 90))
    im.save(OUT4 + 'E1S_timer_compare.png')
    print(OUT4 + 'E1S_timer_compare.png', im.size)


if __name__ == '__main__':
    A.setup()
    if sys.argv[1] == 'compare':
        compare()
    else:
        deg = int(sys.argv[1])
        render_views(deg, sys.argv[2:], 'E1S_timer%d' % deg)
