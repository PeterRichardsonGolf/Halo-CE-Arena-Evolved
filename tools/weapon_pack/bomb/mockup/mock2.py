"""Round 2: favourites, F1-sized versions (E1-S, D1-S, C1-S, C2-S), hands-carried views, 40mm shells (G1, G2).

usage: python3 mock2.py views <ids...>   (ids: F1 E1S D1S C1S C2S G1 G2)
       python3 mock2.py shells | sheets
Output: the work folder
"""
import sys, os, math, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import variants as V                       # noqa: E402
from variants import *                     # noqa: E402,F401
import cyb                                 # noqa: E402

OUT2 = os.environ.get('AE_BOMB_SCRATCH', '.').rstrip('/') + '/'
MOCK = os.environ.get('AE_BOMB_SCRATCH', '.').rstrip('/') + '/'
GL = TS + 'cmt/weapons/_shared/grenade_launcher/'
P_GL = GL + 'projectiles/grenade_launcher_grenade/grenade_launcher_grenade'
P_GL_CASE = GL + 'projectiles/grenade_launcher_grenade/_case/_case'
P_GL_LESS = GL + 'projectiles/grenade_launcher_grenade/_caseless/_caseless'
GL_TEX = tex(GL + 'bitmaps/grenade_launcher_grenade_diff')
GL_M = {'default': mat(tex=GL_TEX, spec=0.45, gloss=40)}

# ---------------------------------------------------------------- scaling with fixed-size timer / beacon


def scale_asm(a, k):
    """body scaled by k about the origin (ground centre); timer and beacon groups keep their size and move with
    their anchor"""
    b = Asm(a.state)
    for P, N, UV, T, m, cast, tag, grp in a.items:
        P2 = k * grp + (P - grp) if grp is not None else P * k
        b.items.append((P2, N, UV, T, m, cast, tag, None if grp is None else grp * k))
    b.sprites = [(np.array(p) * k, s_, c, t_) for p, s_, c, t_ in getattr(a, 'sprites', [])]
    b.lights = [(np.array(p) * k, c, r * max(k, 0.8)) for p, c, r in a.lights]
    return b


def carry_R(info):
    S = V.frame(info['show'], info['up'])
    Tg = V.frame((-1, 0, 0), (0, 0, 1))
    return Tg @ S.T


def carried_wh(a, info):
    P = np.vstack([it[0] for it in a.items]) @ carry_R(info).T
    e = P.max(0) - P.min(0)
    return e[1], e[2], e[0]


_F1WH = None


def f1_wh():
    global _F1WH
    if _F1WH is None:
        a, info = V.F1('carried')
        _F1WH = carried_wh(a, info)
    return _F1WH


def fit_k(fn, info_over=None):
    a, info = fn('carried')
    if info_over:
        info = dict(info, **info_over)
    w, h, d = carried_wh(a, info)
    fw, fh, fd = f1_wh()
    return min(fw / w, fh / h)


# ---------------------------------------------------------------- new / resized builders


def E1S(st):
    a, info = V.E1(st)
    info = dict(show=norm((0, 0.75, 1.0)), up=(0, -1, 0.75))       # carried across the back, long axis level
    return scale_asm(a, K['E1S']), info


def D1S(st):
    """3 mini coils in a flat row (a back-pack shape), straps, timer on the front"""
    a = Asm(st)
    tops = []
    for i, y in enumerate((-0.07, 0.0, 0.07)):
        tops.append(V.mini_coil(a, Xf(1, rot('z', 20 + 35 * i), (0, y, 0))))
    for z in (0.009, 0.1):
        V.strap(a, ('coil',), 2, z, width=0.01, m=V.M_STRAP)
    V.timer_module(a, (0.035 + 0.006, 0.0, 0.03), (1, 0, 0), w=0.046, h=0.026)
    for t, m in zip(tops, (V.W_RED, V.W_BLK, V.W_YEL)):
        V.wire(a, [t, t + np.array([0, 0, 0.012]), (0.03, t[1] * 0.6, 0.125), (0.045, t[1] * 0.25, 0.06),
                   (0.043, t[1] * 0.15, 0.046)], m)
    info = dict(show=(1, 0, 0), up=(0, 0, 1))
    k = K.get('D1S')
    return (scale_asm(a, k) if k else a), info


def C1S(st):
    a, info = V.C1(st)
    return scale_asm(a, K['C1S']), info


def C2S(st):
    a, info = V.C2(st)
    return scale_asm(a, K['C2S']), info


def gl_shell(a, pos, R=None, tag='shell'):
    """40mm shell, nose along +x in its own frame, placed so its axis passes through pos (x = case base)"""
    R = np.eye(3) if R is None else R
    a.gbx(P_GL, Xf(1, R, np.array(pos) - R @ np.array([-0.073, 0, 0])), GL_M, tag=tag)


def G1(st):
    """8 x 40mm shells in a 4 x 2 block, taped, timer on top, leads to the top row's fuzes"""
    a = Asm(st)
    for j, z in enumerate((0.0232, 0.0696)):
        for i, y in enumerate((-0.0696, -0.0232, 0.0232, 0.0696)):
            gl_shell(a, (-0.073, y, z), rot('x', 37 * i + 71 * j), tag='body')
    for x in (-0.06, -0.022):
        V.strap(a, ('body',), 0, x, width=0.012, m=V.M_TAPE, buckle=False)
    f = norm((0, 0.75, 1.0))
    V.timer_module(a, np.array((-0.04, 0.0, 0.0928)) + f * 0.006, f, up=(0, -1, 0.75), w=0.05, h=0.028)
    for y, m in zip((-0.0696, -0.0232, 0.0232, 0.0696), (V.W_RED, V.W_YEL, V.W_BLU, V.W_BLK)):
        V.wire(a, [(0.04, y, 0.0928), (0.02, y * 0.8, 0.104), (-0.01, y * 0.4, 0.11), (-0.025, y * 0.2, 0.104)], m)
    return a, dict(show=(0, 0, 1), up=(1, 0, 0), hold=f)    # carried top to the back, rounds pointing up


def G2(st):
    """3 x 40mm shells taped along the ammo can's face, angled timer on the lid, beacon"""
    a = Asm(st)
    V.body_ammo(a)
    for i, z in enumerate((0.025, 0.071, 0.117)):
        gl_shell(a, (-0.075, 0.035 + 0.0235, z), rot('x', 50 * i), tag='shell')
    for x in (-0.05, 0.02):
        V.strap(a, ('body', 'shell'), 0, x, width=0.012, m=V.M_TAPE, buckle=False)
    f = norm((0, 0.75, 1.0))
    V.timer_module(a, np.array((-0.02, 0.018, 0.139)) + f * 0.006, f, up=(0, -1, 0.75), w=0.05, h=0.028)
    V.beacon(a, (0.06, 0.0, 0.1385), 0.33)
    for z, m in zip((0.025, 0.071, 0.117), (V.W_RED, V.W_YEL, V.W_BLK)):
        V.wire(a, [(0.065, 0.0585, z), (0.07, 0.07, z + 0.012), (0.04, 0.06, 0.15), (0.0, 0.035, 0.152)], m)
    return a, dict(show=(0, 1, 0), up=(0, 0, 1))


K = {}
BUILD = {
    'F1': (V.F1, 'F1 Med pack charge', 'TSC:E med pack: the size reference'),
    'E1S': (E1S, 'E1-S Rocket case bundle, F1 size', '3 SPNKr cases at 0.355 (was 0.5), carried across the back'),
    'D1S': (D1S, 'D1-S Mini coil row, F1 size', '3 mini fusion coils in a flat row + timer'),
    'C1S': (C1S, 'C1-S Ammo can charge, F1 size', 'already F1 size: can unchanged'),
    'C2S': (C2S, 'C2-S Ammo can IED, F1 size', 'already F1 size: can unchanged'),
    'G1': (G1, 'G1 40mm shell block', '8 TSC:E 40mm shells taped 4 x 2 + timer'),
    'G2': (G2, 'G2 40mm shells on ammo can', 'ammo can + 3 x 40mm shells taped + timer + beacon'),
}


def compute_k():
    K['E1S'] = fit_k(V.E1, dict(show=norm((0, 0.75, 1.0)), up=(0, -1, 0.75)))
    K['C1S'] = min(1.0, fit_k(V.C1))
    K['C2S'] = min(1.0, fit_k(V.C2))
    K['D1S'] = None
    K['D1S'] = fit_k(D1S)
    return K


# ---------------------------------------------------------------- posed cyborg (hands)

def posed_cyborg(scene, half_w, hold_z=0.48, fwd=0.0):
    """arms forward, hands at chest height, wrists at y = +-half_w. Returns the mean hand position (x, z)."""
    ns, J, _ = cyb.bind(True)
    idx = {n: i for i, (n, _, _, _) in enumerate(ns)}
    parts = cyb.skinned_parts()

    def pose_mats(side, a_up, a_el, a_ab):
        S, E, W = (J[idx['bip01 %s upperarm' % side]], J[idx['bip01 %s forearm' % side]], J[idx['bip01 %s hand' % side]])
        sgn = 1 if side == 'l' else -1
        Ru = rot('x', -sgn * a_ab) @ rot('y', -a_up)
        Re = rot('y', -a_el)
        up = lambda p: (p - S) @ Ru.T + S
        E2 = up(E[None])[0]
        # forearm: rotate about the posed elbow, in the posed frame
        def fore(p):
            q = up(p)
            return (q - E2) @ (Ru @ Re @ Ru.T).T + E2
        W2 = fore(W[None])[0]
        return up, fore, W2

    best = {}
    for side in ('l', 'r'):
        sgn = 1 if side == 'l' else -1
        target = np.array([0, sgn * half_w, hold_z])
        bestv = None
        for a_up in np.linspace(5, 75, 22):
            for a_el in np.linspace(20, 100, 33):
                for a_ab in np.linspace(-50, 30, 33):
                    _, _, W2 = pose_mats(side, a_up, a_el, a_ab)
                    err = (W2[1] - target[1]) ** 2 * 4 + (W2[2] - target[2]) ** 2 * 4 + max(0, 0.14 + fwd - W2[0]) ** 2
                    if bestv is None or err < bestv[0]:
                        bestv = (err, a_up, a_el, a_ab)
        best[side] = pose_mats(side, *bestv[1:])
    arm = {}
    for side in ('l', 'r'):
        up, fore, W2 = best[side]
        arm[idx['bip01 %s upperarm' % side]] = up
        arm[idx['bip01 %s forearm' % side]] = fore
        arm[idx['bip01 %s hand' % side]] = fore
    m = mat(tex=tex('characters/cyborg/bitmaps/cyborg'), desat=1.0, tint=(0.62, 0.65, 0.70), spec=0.25, gloss=30)
    mi = scene.add_mat(m)
    for sh, P, N, UV, T, n0, n1, w0 in parts:
        if sh in (0, 1):
            continue
        P2 = np.zeros_like(P)
        for nodes_, w in ((n0, w0), (n1, 1 - w0)):
            Q = P.copy()
            for ni, f in arm.items():
                s = nodes_ == ni
                if s.any():
                    Q[s] = f(P[s])
            P2 += Q * w[:, None]
        scene.add(P2, None, UV, T, mi)
    hands = np.array([best['l'][2], best['r'][2]])
    return hands


# ---------------------------------------------------------------- views

def hold_R(info):
    """hands carry: the timer side faces forward (+x), the bomb stays upright"""
    s = np.array(info.get('hold', info['show']), float)
    h = np.array([s[0], s[1], 0.0])
    if np.linalg.norm(h) < 1e-6:
        h = np.array([1.0, 0, 0])
    h = norm(h)
    S = V.frame(h, (0, 0, 1))
    Tg = V.frame((1, 0, 0), (0, 0, 1))
    return Tg @ S.T


def run2(vid, views=('hero', 'planted', 'carried', 'hands')):
    fn, title, sub = BUILD[vid]
    out = []
    if 'hero' in views or 'planted' in views:
        a, info = fn('hero')
        lo, hi = a.bounds()
        c = (lo + hi) / 2
        d = V.fit_dist(lo, hi, 28, 1.05)
        s = info.get('hold', info['show'])
        yaw = 35 if s[0] > 0.5 else 65 if s[1] > 0.3 else -65
        if 'hero' in views:
            sc = Scene()
            a.emit(sc)
            render(sc, OUT2 + f'{vid}_hero.png', V.orbit(c, yaw, 24, d), c, fov=28, exposure=1.12, amb=0.25,
                   label=(title, sub + '  (3/4 view)'))
        if 'planted' in views:
            a, info = fn('planted')
            sc = Scene()
            a.emit(sc)
            c2 = c.copy()
            c2[2] *= 0.8
            render(sc, OUT2 + f'{vid}_planted.png', V.orbit(c2, yaw + 22, 34, d * 1.15), c2, fov=28, amb=0.12,
                   exposure=0.72, key_col=(0.75, 0.75, 0.8),
                   label=(title + ' - planted', 'fuse running: beacon lit, 2 of 4 timer bars red'))
    if 'carried' in views:
        a, info = fn('carried')
        sc = Scene()
        V.cyborg(sc)
        R = carry_R(info)
        P = np.vstack([it[0] for it in a.items]) @ R.T
        bx = V.cyborg_back()
        t = np.array([bx + 0.012 - P[:, 0].max(), -(P[:, 1].max() + P[:, 1].min()) / 2,
                      0.5 - (P[:, 2].max() + P[:, 2].min()) / 2])
        a.emit(sc, Xf(1, R, t))
        tgt = np.array([0, 0, 0.375])
        render(sc, OUT2 + f'{vid}_carried.png', V.orbit(tgt, 205, 12, 1.5), tgt, fov=32, exposure=1.1, amb=0.26,
               label=(title + ' - carried', 'on the stock cyborg (2.15 m), rear 3/4 view'))
    if 'hands' in views:
        a, info = fn('carried')
        R = hold_R(info)
        P = np.vstack([it[0] for it in a.items]) @ R.T
        half_w = (P[:, 1].max() - P[:, 1].min()) / 2 + 0.006
        sc = Scene()
        hands = posed_cyborg(sc, half_w)
        hz = hands[:, 2].mean()
        hx = hands[:, 0].mean()
        # bomb between the hands: its centre at hand height, its back face just in front of the chest
        t = np.array([max(0.075 - P[:, 0].min(), hx - (P[:, 0].max() + P[:, 0].min()) / 2),
                      -(P[:, 1].max() + P[:, 1].min()) / 2, hz - (P[:, 2].max() + P[:, 2].min()) / 2])
        a.emit(sc, Xf(1, R, t))
        tgt = np.array([0.03, 0, 0.375])
        render(sc, OUT2 + f'{vid}_hands.png', V.orbit(tgt, 32, 12, 1.5), tgt, fov=32, exposure=1.1, amb=0.26,
               label=(title + ' - in the hands', 'held at chest height, both hands, front 3/4 view'))
    return out


def shells():
    """gl-shells.png: the three TSC:E 40mm models textured, 3/4 view, next to a stock frag grenade"""
    from PIL import Image, ImageDraw, ImageFont
    names = [(P_GL, 'grenade_launcher_grenade.gbxmodel', 'full round: case + projectile'),
             (P_GL_CASE, '_case/_case.gbxmodel', 'spent case'),
             (P_GL_LESS, '_caseless/_caseless.gbxmodel', 'projectile only')]
    tiles = []
    for p, n, d in names:
        a = Asm('hero')
        a.gbx(p, Xf(1, None, (0, 0.03, -zmin(p))), GL_M, tag='s')
        V.frag(a, (0.0, -0.06, 0.0))
        lo, hi = a.bounds()
        e = (hi - lo)
        parts, _ = load_gbx(p)
        Pp = np.vstack([q[1] for q in parts])
        ext = (Pp.max(0) - Pp.min(0)) * WU
        sc = Scene()
        a.emit(sc)
        c = (lo + hi) / 2
        dist = V.fit_dist(lo, hi, 28, 1.0)
        f = OUT2 + '_shell_%d.png' % len(tiles)
        render(sc, f, V.orbit(c, 50, 24, dist), c, fov=28, exposure=1.12, amb=0.25,
               label=(n, '%s: %.2f m long, %.2f m across (Halo scale); left: stock frag grenade' % (d, ext[0], ext[1])))
        tiles.append(Image.open(f))
        os.remove(f)
    W = sum(t.size[0] for t in tiles) // 2
    im = Image.new('RGB', (W, tiles[0].size[1] // 2 + 50), (16, 17, 21))
    dr = ImageDraw.Draw(im)
    dr.text((14, 10), 'TSC:E 40mm grenade launcher rounds (CMT art, credit), texture grenade_launcher_grenade_diff',
            font=ImageFont.truetype('/usr/share/fonts/noto/NotoSans-Bold.ttf', 22), fill=(230, 233, 240))
    x = 0
    for t in tiles:
        im.paste(t.resize((t.size[0] // 2, t.size[1] // 2), Image.LANCZOS), (x, 50))
        x += t.size[0] // 2
    im.save(OUT2 + 'gl-shells.png')
    print(OUT2 + 'gl-shells.png', im.size)


def sheets():
    from PIL import Image, ImageDraw, ImageFont
    B = ImageFont.truetype('/usr/share/fonts/noto/NotoSans-Bold.ttf', 26)
    Rf = ImageFont.truetype('/usr/share/fonts/noto/NotoSans-Regular.ttf', 18)
    BG = (16, 17, 21)
    # favourites
    fav = ['E1', 'D1', 'C1', 'C2']
    tw, th = 600, 450
    im = Image.new('RGB', (3 * tw + 4 * 10, 70 + len(fav) * (th + 10)), BG)
    d = ImageDraw.Draw(im)
    d.text((14, 16), "Owner's favourites by look: 1 E1, 2 D1, 3 C1, 4 C2  (hero | planted | carried)", font=B,
           fill=(230, 233, 240))
    for r, v in enumerate(fav):
        for c, k in enumerate(('hero', 'planted', 'carried')):
            t = Image.open(MOCK + f'{v}_{k}.png').resize((tw, th), Image.LANCZOS)
            im.paste(t, (10 + c * (tw + 10), 70 + r * (th + 10)))
        d.text((10 + 8, 70 + r * (th + 10) + th - 40), '#%d' % (r + 1), font=B, fill=(240, 200, 90))
    im.save(OUT2 + 'favourites.png')
    print(OUT2 + 'favourites.png', im.size)
    # small vs F1: back row and hands row, cropped around the cyborg
    ids = ['F1', 'E1S', 'D1S', 'C1S', 'C2S', 'G1', 'G2']
    cw, ch = 420, 640
    im = Image.new('RGB', (len(ids) * (cw + 8) + 8, 80 + 2 * (ch + 50)), BG)
    d = ImageDraw.Draw(im)
    d.text((14, 14), 'F1-sized versions vs F1 (left): on the back (top row), in the hands (bottom row)', font=B,
           fill=(230, 233, 240))
    d.text((14, 48), 'same camera and cyborg in every panel; timers and beacons keep their full size', font=Rf,
           fill=(150, 158, 172))
    for c, v in enumerate(ids):
        for r, k in enumerate(('carried', 'hands')):
            t = Image.open(OUT2 + f'{v}_{k}.png')
            cx = 600
            t = t.crop((cx - 259, 110, cx + 259, 900)).resize((cw, ch), Image.LANCZOS)
            x = 8 + c * (cw + 8)
            y = 80 + r * (ch + 50)
            d.text((x + 6, y + 8), BUILD[v][1].split(',')[0], font=ImageFont.truetype(
                '/usr/share/fonts/noto/NotoSans-Bold.ttf', 20), fill=(240, 200, 90) if v != 'F1' else (120, 200, 255))
            im.paste(t, (x, y + 40))
    im.save(OUT2 + 'small-vs-f1.png')
    print(OUT2 + 'small-vs-f1.png', im.size)
    # G views
    ids = ['G1', 'G2']
    im = Image.new('RGB', (4 * (tw + 10) + 10, 20 + len(ids) * (th + 10)), BG)
    for r, v in enumerate(ids):
        for c, k in enumerate(('hero', 'planted', 'carried', 'hands')):
            t = Image.open(OUT2 + f'{v}_{k}.png').resize((tw, th), Image.LANCZOS)
            im.paste(t, (10 + c * (tw + 10), 10 + r * (th + 10)))
    im.save(OUT2 + 'G-sheet.png')
    print(OUT2 + 'G-sheet.png', im.size)


if __name__ == '__main__':
    os.makedirs(OUT2, exist_ok=True)
    compute_k()
    cmd = sys.argv[1]
    if cmd == 'k':
        print({k: (round(v, 3) if v else v) for k, v in K.items()}, 'F1 carried wh (wu)', [round(x, 3) for x in f1_wh()])
    elif cmd == 'views':
        for v in sys.argv[2:]:
            t0 = time.time()
            run2(v)
            print(v, round(time.time() - t0, 1), 's', flush=True)
    elif cmd == 'one':
        run2(sys.argv[2], tuple(sys.argv[3:]))
    elif cmd == 'shells':
        shells()
    elif cmd == 'sheets':
        sheets()
