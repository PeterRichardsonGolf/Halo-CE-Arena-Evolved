"""Backpack carry: the bomb upright along the spine, flush on the back, with a webbing harness
(2 shoulder straps over the shoulders to the chest, sternum strap, waist belt, buckles).

usage: python3 backpack.py <ids...>  (E1P D1P A3 B3) ; python3 backpack.py sheet <ids...>
Output: the work folder,side,front}.png, backpack.png
"""
import sys, os, math, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import polish as PL                        # noqa: E402
from polish import *                       # noqa: E402,F401
import cyb                                 # noqa: E402

OUT = PL.OUT3
ARM_NODES = [13, 14, 15, 16, 17, 18]

# ---------------------------------------------------------------- cyborg surface by ray casting

_TRIS = None


def cyb_tris(torso_only=False):
    parts = cyb.skinned_parts()
    out = []
    for sh, P, N, UV, T, n0, n1, w0 in parts:
        if sh in (0, 1):
            continue
        tt = T
        if torso_only:
            arm = np.isin(n0, ARM_NODES) & (w0 > 0.5)
            tt = T[~arm[T].any(1)]
        out.append(P[tt])
    return np.concatenate(out)


def raycast(o, d, tris):
    """first hit distance of rays o + t d (n,3) against tris (m,3,3); inf where no hit"""
    o = np.atleast_2d(o).astype(float)
    d = np.atleast_2d(d).astype(float)
    v0, v1, v2 = tris[:, 0], tris[:, 1], tris[:, 2]
    e1, e2 = v1 - v0, v2 - v0
    out = np.full(len(o), np.inf)
    for i in range(len(o)):
        p = np.cross(d[i], e2)
        det = (e1 * p).sum(1)
        ok = np.abs(det) > 1e-12
        inv = np.where(ok, 1 / np.where(ok, det, 1), 0)
        tv = o[i] - v0
        u = (tv * p).sum(1) * inv
        q = np.cross(tv, e1)
        v = (d[i] * q).sum(1) * inv
        t = (e2 * q).sum(1) * inv
        hit = ok & (u >= 0) & (v >= 0) & (u + v <= 1) & (t > 0)
        if hit.any():
            out[i] = t[hit].min()
    return out


TR = None
TRT = None


def surf(o, d):
    """surface point hit by a ray (or None)"""
    t = raycast(o, d, TR)[0]
    return None if not np.isfinite(t) else np.array(o, float) + np.array(d, float) * t


def back_x(y, z):
    p = surf((-1, y, z), (1, 0, 0))
    return p[0] if p is not None else None


def front_x(y, z):
    p = surf((1, y, z), (-1, 0, 0))
    return p[0] if p is not None else None


def top_z(x, y):
    p = surf((x, y, 1.5), (0, 0, -1))
    return p[2] if p is not None else None


def surf_t(o, d):
    t = raycast(o, d, TRT)[0]
    return None if not np.isfinite(t) else np.array(o, float) + np.array(d, float) * t


def side_y(x, z, sgn):
    p = surf_t((x, sgn * 1.0, z), (0, -sgn, 0))
    return p[1] if p is not None else None


# ---------------------------------------------------------------- ribbon on a surface

def ribbon(points, normals, width, thick):
    """flat strap through points; its broad face follows normals (pointing away from the body)"""
    pts = np.array(points, float)
    nr = np.array(normals, float)
    tang = np.gradient(pts, axis=0)
    tang /= np.maximum(np.linalg.norm(tang, axis=1, keepdims=True), 1e-9)
    nr = nr - (nr * tang).sum(1, keepdims=True) * tang
    nr /= np.maximum(np.linalg.norm(nr, axis=1, keepdims=True), 1e-9)
    side = np.cross(tang, nr)
    prof = [(-width / 2, thick / 2), (width / 2, thick / 2), (width / 2, -thick / 2), (-width / 2, -thick / 2)]
    pn = [(0, 1), (0, 1), (0, -1), (0, -1)]
    P, N, UV, T = [], [], [], []
    L = np.concatenate([[0], np.cumsum(np.linalg.norm(np.diff(pts, axis=0), axis=1))])
    for i in range(len(pts)):
        for j, ((a, b), (na, nb)) in enumerate(zip(prof, pn)):
            P.append(pts[i] + side[i] * a + nr[i] * b)
            N.append(nr[i] * nb + side[i] * na)
            UV.append((j / 4, L[i] / (width * 2.2)))
    k = 4
    for i in range(len(pts) - 1):
        for j in range(k):
            j2 = (j + 1) % k
            a, b, c, d = i * k + j, i * k + j2, (i + 1) * k + j2, (i + 1) * k + j
            T += [(a, b, c), (a, c, d)]
    return np.array(P), np.array(N), np.array(UV), np.array(T)


# ---------------------------------------------------------------- upright pack builders

def E1P_bp(st):
    """E1-P with the console mounted flat on the outer face, upright when the cases stand along the spine"""
    a = Asm(st)
    s = 0.355
    cw, ch = 0.182 * s, 0.1 * s
    for y in (-cw / 2 - 0.0005, cw / 2 + 0.0005):
        PL.mini_rl(a, (0, y, 0), s, rot('z', 180), tag='body')
    PL.mini_rl(a, (0, 0, ch), s, rot('z', 180), tag='body')
    for x in (-0.055, 0.055):
        PL.strap_p(a, ('body',), 0, x, width=0.012, side_buckle=1)
    gl, R = PL.console(a, (0.012, 0.0, 2 * ch + 0.0085), (0, 0, 1), up=(1, 0, 0), s=0.95)
    PL.add_decal(a, PL.D_CAUTION_Y, 'E tape caution', (-0.07, 0.0, 2 * ch + 0.0003), (0, 0, 1), (1, 0, 0), 0.04)
    for y, m in ((-cw / 2, PL.W_RED2), (cw / 2, PL.W_YEL2), (0.0, PL.W_BLK2)):
        z = ch / 2 if y else ch * 1.5
        cable(a, [(0.0872, y, z), (0.095, y * 0.9, z + 0.008), (0.07, y * 0.6, 2 * ch + 0.01), gl], m)
    return a, dict(show=(0, 0, 1), up=(1, 0, 0))


def D1P_bp(st):
    """D1-P with the coils lying across the back, stacked along the spine; console upright"""
    a = Asm(st)
    tops = []
    for i, y in enumerate((-0.07, 0.0, 0.07)):
        tops.append(PL.coil_p(a, Xf(1, rot('z', -10 + 10 * i), (0, y, 0))))
    for z in (0.009, 0.1):
        PL.strap_p(a, ('coil',), 2, z, width=0.01)
    gl, R = PL.console(a, (0.035 + 0.0085, 0.0, 0.055), (1, 0, 0), up=(0, 1, 0), s=1.0)
    for t, m in zip(tops, (PL.W_RED2, PL.W_BLK2, PL.W_YEL2)):
        cable(a, [t, t + np.array([0, 0, 0.012]), (0.03, t[1] * 0.6, 0.125), gl], m)
    k = PL.M2.K.get('D1S') or 0.854
    return PL.M2.scale_asm(a, k), dict(show=(1, 0, 0), up=(0, 1, 0))


def kit_bp(st, which):
    """A3 / B3 with the SPNKr case moved down the lid and the console on the lid's upper end"""
    a = Asm(st)
    if which == 'A':
        m = PL.paint(base=PL.hp_stock_tex(), det=PL.DET_DIRTY, det_tile=(3, 3), det_amt=0.8, nrm=PL.NRM_SCR,
                     nrm_tile=(2, 2), nrm_amt=1.6, wear=1.0, note='A body')
        a.gbx(V.P_HP, Xf(1.0, None, (0, 0, -zmin(V.P_HP))), {0: m}, tag='body')
        lid, xc, xk, ys = 0.0795, -0.036, 0.047, 0.048
    else:
        m = PL.paint(base=PL.tsce_tex(), det=PL.DET_DIRTY, det_tile=(3, 3), det_amt=0.7, nrm=PL.NRM_SCR,
                     nrm_tile=(2, 2), nrm_amt=1.4, wear=1.0, note='B body')
        a.gbx(V.P_THP, Xf(1, None, (0, 0, -zmin(V.P_THP))), {0: m}, tag='body')
        lid, xc, xk, ys = 0.069, -0.042, 0.052, 0.05
    PL.mini_rl(a, (xc, 0, lid), 0.3, rot('z', 90))
    for y in (-ys, ys):
        PL.strap_p(a, ('body', 'rl'), 1, y, width=0.013)
    gl, R = PL.console(a, (xk, 0.0, lid + 0.0085), (0, 0, 1), up=(1, 0, 0))
    PL.add_decal(a, PL.D_E419, which + ' serial', (-0.03 if which == 'A' else -0.035, 0.0, 0.0003), (0, 0, -1),
                 (1, 0, 0), 0.04, tint=(0.9, 0.9, 0.85))
    cable(a, [gl, gl + np.array([-0.004, -0.006, 0.004]), (xc + 0.02, -0.035, lid + 0.03), (xc, -0.06, lid + 0.02)], PL.W_RED2)
    cable(a, [gl, gl + np.array([-0.006, -0.002, 0.006]), (xc + 0.022, -0.02, lid + 0.034), (xc, -0.045, lid + 0.026)], PL.W_BLK2)
    return a, dict(show=(0, 0, 1), up=(1, 0, 0))


BP = {
    'E1P': (E1P_bp, 'E1-S Rocket case bundle, backpack'),
    'D1P': (D1P_bp, 'D1-S Mini coil row, backpack'),
    'A3': (lambda st: kit_bp(st, 'A'), 'A  Stock CE health kit IED, backpack'),
    'B3': (lambda st: kit_bp(st, 'B'), 'B  TSC:E med pack IED, backpack'),
}

# ---------------------------------------------------------------- placement + harness


def place_on_back(a, info, top_z=0.6):
    """rotate into carry pose, tilt to the back's slope, sit flush (touching) between the shoulder blades"""
    R = PL.M2.carry_R(info)
    P = np.vstack([it[0] for it in a.items]) @ R.T
    h = P[:, 2].max() - P[:, 2].min()
    zb, zt = top_z - h, top_z
    # back surface samples over the pack's footprint
    W = P[:, 1].max() - P[:, 1].min()
    samples = []
    for z in np.linspace(zb + 0.01, zt - 0.005, 7):
        for y in np.linspace(-0.04, 0.04, 5):
            x = back_x(y, z)
            if x is not None:
                samples.append((z, x))
    S = np.array(samples)
    b, a0 = np.polyfit(S[:, 0], S[:, 1], 1)          # x = b z + a0
    th = math.atan(b)                                 # tilt about y so the contact face follows the back
    Rt = rot('y', math.degrees(th))
    R2 = Rt @ R
    P2 = np.vstack([it[0] for it in a.items]) @ R2.T
    t = np.array([-0.6 - P2[:, 0].max(), -(P2[:, 1].max() + P2[:, 1].min()) / 2, zt - P2[:, 2].max()])
    # touch: ray-cast from the back surface toward the pack (exact on flat faces), slide the pack in by the
    # smallest distance
    tris = np.concatenate([(it[0] @ R2.T + t)[it[3]] for it in a.items])
    dmin = np.inf
    for z in np.linspace(zb + 0.008, zt - 0.008, 9):
        for y in np.linspace(-min(W / 2, 0.06) * 0.9, min(W / 2, 0.06) * 0.9, 7):
            x = back_x(y, z)
            if x is None:
                continue
            d = raycast((x, y, z), (-1, 0, 0), tris)[0]
            dmin = min(dmin, d)
    t[0] += dmin - 0.0015
    xf = Xf(1, R2, t)
    Pf = P2 + t
    return xf, Pf


def harness(sc, Pf, wm, belt=True):
    """2 shoulder straps (pack top -> over the shoulder -> down the chest -> under the arm -> pack bottom),
    sternum strap with buckle, waist belt with buckle"""
    top = Pf[:, 2].max()
    bot = Pf[:, 2].min()
    cx = Pf[:, 0].max()                     # contact side of the pack (against the back)
    width, thick, off = 0.014, 0.0024, 0.003
    stm = PL.steel('harness buckles')
    fronts = {}
    for sgn in (1, -1):
        ys = 0.058 * sgn
        pts, nrm = [], []
        # leaves the pack top, rides the upper back
        pts.append((cx - 0.004, ys * 0.85, top - 0.012)); nrm.append((-1, 0, 0.3))
        for z in (top + 0.004,):
            x = back_x(ys, min(z, 0.62))
            pts.append(((x if x is not None else cx) - off - 0.002, ys, z)); nrm.append((-1, 0, 0.6))
        # over the shoulder
        for x in (-0.05, -0.02, 0.01, 0.035):
            zt = top_z(x, ys)
            pts.append((x, ys, zt + off)); nrm.append((0, 0, 1))
        # down the chest
        for z in (0.58, 0.54, 0.50, 0.46):
            x = front_x(ys * (1.0 + (0.58 - z) * 0.6), z)
            pts.append((x + off, ys * (1.0 + (0.58 - z) * 0.6), z)); nrm.append((1, 0, 0))
        fronts[sgn] = pts[-2]
        # under the arm, back to the pack's lower corner
        for x, z in ((0.02, 0.43), (-0.02, 0.425), (-0.05, 0.43)):
            y = side_y(x, z, sgn)
            pts.append((x, (y if y is not None else 0.07 * sgn) + sgn * off, z)); nrm.append((0, sgn, 0))
        pts.append((cx - 0.004, sgn * (abs(Pf[:, 1]).max() * 0.8), bot + 0.02)); nrm.append((-0.3, sgn, 0))
        pts = np.array(pts)
        nrm = np.array(nrm, float)
        # smooth path
        sp = spline(pts, 6)
        nr = np.array([nrm[min(len(nrm) - 1, int(round(i / 6)))] for i in range(len(sp))])
        sc.add(*ribbon(sp, nr, width, thick), wm)
        # strap adjuster on the chest
        p = np.array(fronts[sgn])
        sc.add(*PL.bevel_box((0.004, width * 1.5, 0.006), 0.001, p + np.array([0.002, 0, 0.012])), stm)
    # sternum strap across the chest at the strap fronts
    zs = 0.5
    pts, nrm = [], []
    for y in np.linspace(-0.06, 0.06, 9):
        x = front_x(y, zs)
        pts.append((x + off + 0.002, y, zs)); nrm.append((1, 0, 0))
    sc.add(*ribbon(np.array(pts), np.array(nrm), 0.008, thick), wm)
    xb = front_x(0.0, zs)
    sc.add(*PL.bevel_box((0.005, 0.016, 0.011), 0.0015, (xb + off + 0.005, 0, zs)), stm)
    if not belt:
        return
    # waist belt round the torso (arms excluded) and the pack bottom
    zw = max(0.395, bot + 0.012)
    ang = np.linspace(0, 2 * math.pi, 48, endpoint=False)
    ring = []
    for a_ in ang:
        d = np.array([math.cos(a_), math.sin(a_), 0])
        o = np.array([0, 0, zw]) + d * 0.5
        p = surf_t(o, -d)
        r = np.linalg.norm(p[:2]) if p is not None else 0.07
        ring.append(r)
    ring = np.array(ring)
    pts = np.stack([np.cos(ang) * (ring + off + 0.002), np.sin(ang) * (ring + off + 0.002), np.full(len(ang), zw)], 1)
    # wrap over the pack: points behind the back go to the pack's outer side? the belt anchors at the pack's sides
    behind = pts[:, 0] < -0.02
    pts[behind, 0] = np.maximum(pts[behind, 0], cx + 0.002)
    from scipy.spatial import ConvexHull
    h = ConvexHull(pts[:, :2])
    loop = pts[h.vertices]
    seg = np.vstack([loop, loop[:1]])
    L = np.linalg.norm(np.diff(seg, axis=0), axis=1)
    cum = np.concatenate([[0], np.cumsum(L)])
    s = np.linspace(0, cum[-1], 160, endpoint=False)
    rs = np.stack([np.interp(s, cum, seg[:, 0]), np.interp(s, cum, seg[:, 1]), np.full(len(s), zw)], 1)
    Pm, Nm, UVm, Tm = sweep(rs, rect(0.012, thick), side=(0, 0, 1), closed=True)
    UVm[:, 1] *= cum[-1] / 0.026
    sc.add(Pm, Nm, UVm, Tm, wm)
    xf_ = front_x(0.0, zw)
    sc.add(*PL.bevel_box((0.005, 0.02, 0.014), 0.0015, ((xf_ if xf_ else 0.05) + off + 0.005, 0, zw)), stm)


def render_bp(vid):
    global TR, TRT
    if TR is None:
        TR = cyb_tris()
        TRT = cyb_tris(torso_only=True)
    fn, title = BP[vid]
    a, info = fn('carried')
    sc = Scene()
    V.cyborg(sc)
    xf, Pf = place_on_back(a, info)
    a.emit(sc, xf)
    harness(sc, Pf, PL.webbing())
    tgt = np.array([0, 0, 0.385])
    views = (('rear', 205, 12, 1.7, 'rear 3/4'), ('side', 92, 6, 1.7, 'side profile'), ('front', 28, 10, 1.7, 'front 3/4'))
    for name, yaw, el, dist, lab in views:
        render(sc, OUT + f'{vid}_backpack_{name}.png', V.orbit(tgt, yaw, el, dist), tgt, fov=30, exposure=1.1, amb=0.26,
               label=(title, 'worn as a backpack: upright, flush on the back, webbing harness. ' + lab))


def sheet(ids):
    from PIL import Image, ImageDraw, ImageFont
    B = ImageFont.truetype('/usr/share/fonts/noto/NotoSans-Bold.ttf', 26)
    Rf = ImageFont.truetype('/usr/share/fonts/noto/NotoSans-Regular.ttf', 18)
    tw, th = 600, 450
    im = Image.new('RGB', (3 * (tw + 10) + 10, 80 + len(ids) * (th + 10)), (16, 17, 21))
    d = ImageDraw.Draw(im)
    d.text((14, 12), 'Backpack carry: upright along the spine, flush on the back, webbing harness', font=B,
           fill=(230, 233, 240))
    d.text((14, 48), 'rear 3/4 | side profile | front 3/4 (shoulder straps, sternum strap, waist belt)', font=Rf,
           fill=(150, 158, 172))
    for r, v in enumerate(ids):
        for c, k in enumerate(('rear', 'side', 'front')):
            t = Image.open(OUT + f'{v}_backpack_{k}.png').resize((tw, th), Image.LANCZOS)
            im.paste(t, (10 + c * (tw + 10), 80 + r * (th + 10)))
    im.save(OUT + 'backpack.png')
    print(OUT + 'backpack.png', im.size)


if __name__ == '__main__':
    PL.setup()
    if sys.argv[1] == 'sheet':
        sheet(sys.argv[2:])
    else:
        for v in sys.argv[1:]:
            t0 = time.time()
            render_bp(v)
            print(v, round(time.time() - t0, 1), 's', flush=True)
