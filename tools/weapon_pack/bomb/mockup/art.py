"""Round 4 art-direction pass: every surface, stencil, glow and colour from a real CE / digsite / TSC:E source.

- no invented text: only stencils that exist in a decal sheet or in a model's own diffuse
- materials: real diffuse + the detail map the real shader pairs with it; palettes sampled from textures
- effects: beacon glow = CE lens-flare bitmap (effects/particles/lens flare/bitmaps/flares_generic2, the bitmap
  of scenery/landing beacon.lens_flare that small red beacon.light uses); keys lit by the a10 tech rack glow map
- wires: two per bomb, sampled colours, slack between steel clips
- decals: sparse, at panel corners, chipped by a real grunge map
- ground: Blood Gulch's ground shader layers (b30 detail sand, a30 detail grass, b30 detail ground) on the
  blood ground base colour

usage: python3 art.py views <ids> | backpack <ids> | sheet | textures
"""
import sys, os, math, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import engine as E                         # noqa: E402
import variants as V                       # noqa: E402
import polish as PL                        # noqa: E402
import mock2 as M2                         # noqa: E402
import backpack as BPK                     # noqa: E402
from polish import *                       # noqa: E402,F401

OUT4 = os.environ.get('AE_BOMB_SCRATCH', '.').rstrip('/') + '/'
T = PL.T

# ---------------------------------------------------------------- sampled palette


def region_mean(t, box=None, mask=None):
    c = t[..., :3] if box is None else t[box[1]:box[3], box[0]:box[2], :3]
    if mask is not None:
        c = c[mask(c)]
    return np.asarray(c).reshape(-1, 3).mean(0)


WH = T('vehicles/warthog/bitmaps/warthog', 'palette: UNSC olive; console housing diffuse')
OLIVE = region_mean(WH, mask=lambda c: (c[..., 1] > c[..., 0] * 1.05) & (c[..., 1] > c[..., 2] * 1.1) & (c.mean(-1) > 0.15))
UNSC_T = T(PL.UNSC, 'palette: wire red/yellow from the CAUTION stencils')
RED_W = region_mean(UNSC_T, (141, 180, 251, 204), mask=lambda c: c[..., 0] > 0.35)
YEL_W = region_mean(UNSC_T, (141, 156, 251, 180), mask=lambda c: c[..., 0] > 0.35)
FRAG = T('weapons/frag grenade/bitmaps/frag grenade', 'palette: black wire = frag grenade plastic')
_fc = FRAG[..., :3].reshape(-1, 3)
_fl = _fc.mean(1)
_fs = _fc.max(1) - _fc.min(1)
_dark = (_fs < 0.08) & (_fl <= np.percentile(_fl[_fs < 0.08], 10))
BLK_W = _fc[_dark].mean(0)                                # darkest unsaturated frag plastic
PLATE = T('levels/a10/bitmaps/tech rack plate', 'mount plate / carrier plate / coil caps diffuse')
STEEL = region_mean(PLATE, (40, 40, 216, 216)) * 1.6
PACK_T = T(PL.PACK)
ORANGE = region_mean(PACK_T, (40, 70, 120, 120))       # the ammo pack's own orange panel
BG_T = T('levels/test/bloodgulch/bitmaps/blood ground', 'ground base colour (Blood Gulch)')
GROUND_COL = region_mean(BG_T)
GROUND = [(T('levels/b30/bitmaps/detail ground', 'ground detail (blood ground shader)'), 0.11, 1.0),
          (T('levels/a30/bitmaps/detail grass', 'ground detail (blood ground shader)'), 0.23, 0.8),
          (T('levels/b30/bitmaps/detail sand', 'ground detail (blood ground shader)'), 0.05, 0.6)]
FLARE = T('effects/particles/lens flare/bitmaps/flares_generic2', 'beacon glow sprite (landing beacon lens flare)')
CORRODE = T('characters/cyborg/bitmaps/detail corroded metal light', 'detail map (warthog shader pairing)')
GRUNGE = T(PL.DET_DIRTY, 'decal chipping mask')


def best_crop(t, size, score):
    h, w = t.shape[:2]
    best = None
    for y in range(0, h - size + 1, 16):
        for x in range(0, w - size + 1, 16):
            s = score(t[y:y + size, x:x + size, :3])
            if best is None or s > best[0]:
                best = (s, x, y)
    return best[1], best[2]


_wx, _wy = best_crop(WH, 96, lambda c: ((c[..., 1] > c[..., 0] * 1.05) & (c[..., 1] > c[..., 2] * 1.1)).mean() - c.std() * 2)
WH_PANEL = np.ascontiguousarray(WH[_wy:_wy + 96, _wx:_wx + 96])
RACK = T('levels/a10/bitmaps/tech rack mount', 'console keypad (amber key block)')
RACK_G = T('levels/a10/bitmaps/tech rack mount glow', 'console key glow (self-illumination)')
KEYS = np.ascontiguousarray(RACK[160:184, 0:96])
KEYS_G = np.ascontiguousarray(RACK_G[160:184, 0:96])
STRIPS = T('levels/test/bloodgulch/bitmaps/mp lights small strips red', 'coil band glow (Blood Gulch base light panel)')
BAND = np.ascontiguousarray(STRIPS[0:256, 612:1010])
CAPMETAL = T('levels/test/bloodgulch/bitmaps/cap_metal', 'coil body diffuse (Blood Gulch metal)')
DANGER_SRC = (PL.PELD, (257, 152, 316, 253))


# ---------------------------------------------------------------- materials (patched into polish)

def housing_mat(note='console housing'):
    return mat(tex=WH_PANEL, spec=0.4, gloss=36, detail=CORRODE, detail_tile=(3, 3), detail_amt=1.0,
               normal=T(PL.NRM_SCR, 'normal: console housing'), normal_tile=(1, 1), normal_amt=1.4, wear=1.0)


def plate_mat(note='mount plate'):
    return mat(tex=PLATE, spec=0.3, gloss=30, detail=CORRODE, detail_tile=(2, 2), detail_amt=0.6,
               normal=T(PL.NRM_PLATE, 'normal: plates'), normal_tile=(2, 2), normal_amt=0.7, wear=1.2)


def steel_mat(note='bolts, buckles'):
    return mat(color=tuple(np.clip(STEEL, 0, 1)), spec=0.7, gloss=60, detail=CORRODE, detail_tile=(1, 1),
               detail_amt=0.5, wear=0.6)


def decal_m(d, note, tint=(1, 1, 1)):
    return mat(tex=PL.dtex(d, note), tint=tint, alpha_test=True, spec=0.12, detail=CORRODE, detail_tile=(1, 1),
               detail_amt=0.6, chip=GRUNGE, chip_tile=(1.7, 1.7), chip_thr=0.36)


PL.gunmetal = housing_mat
PL.olive_steel = plate_mat
PL.steel = steel_mat
PL.decal_mat = decal_m

# wires: two per bomb, sampled colours, slack + clips
W_A = mat(color=tuple(np.clip(RED_W * 0.85, 0, 1)), spec=0.45, gloss=40)
W_B = mat(color=tuple(np.clip(BLK_W, 0, 1)), spec=0.45, gloss=40)
_cable0 = PL.cable


def cable_art(a, ctrl, m=None, r=0.0018):
    n = getattr(a, '_nwires', 0)
    if n >= 2:
        return
    a._nwires = n + 1
    pts = [np.array(p, float) for p in ctrl]
    out = [pts[0]]
    for p0, p1 in zip(pts[:-1], pts[1:]):
        mid = (p0 + p1) / 2
        sag = np.linalg.norm(p1 - p0) * 0.12
        out += [mid - np.array([0, 0, sag]), p1]
    m2 = W_A if n == 0 else W_B
    _cable0(a, out, m2, r * 1.15)
    # steel clips where the wire leaves the payload and at its second anchor
    for p in (pts[0], pts[1] if len(pts) > 2 else pts[-1]):
        a.add(*PL.bevel_box((0.006, 0.006, 0.004), 0.001, p), steel_mat(), tag='clip')


PL.cable = cable_art
BPK.cable = cable_art

# decals: sparse. Keep only the console's corner CAUTION and the per-builder 'allowed' ones.
ALLOWED = {'console label', 'coil danger', 'A unsc', 'B unsc'}
_add_decal0 = PL.add_decal


def add_decal_art(a, d, note, center, facing, up, width, tint=(1, 1, 1), lift=0.0006):
    if note not in ALLOWED:
        return 0
    return _add_decal0(a, d, note, center, facing, up, width, tint, lift)


PL.add_decal = add_decal_art
PL.stencil_40 = lambda *a, **k: None      # composed "40" was not a real stencil: removed

# console: real keypad block + its glow; beacon glow sprite from the lens-flare bitmap; no side serial
_console0 = PL.console


def console_art(a, center, facing, up=(0, 0, 1), s=1.0, beacon=True, label=PL.D_CAUTION_Y, side_label=None,
                plate=True):
    n0 = len(a.items)
    nl = len(a.lights)
    gl, R = _console0(a, center, facing, up, s, beacon, label, None, plate)
    # replace the 4 generated keys with the tech rack amber key row (+ its glow map)
    a.items[n0:] = [it for it in a.items[n0:] if it[6] != 'key']
    w, h, d = 0.05 * s, 0.028 * s, 0.014 * s
    xf = Xf(1, R, center)
    a.group = np.array(center, float)
    km = mat(tex=KEYS, spec=0.35, gloss=40, emissive=(1, 1, 1), emis_tex=KEYS_G, emis_gain=0.55, wear=0.4)
    a.add(*quad((d / 2 + 0.0006 * s, 0, -h / 2 - 0.0066 * s), (0, w, 0), (0, 0, -w / 4)), km, xf)
    a.group = None
    # quieter planted light: tighter and weaker than round 3
    a.lights[nl:] = [(p_, tuple(np.array(c_) * 0.55), r_ * 0.7) for p_, c_, r_ in a.lights[nl:]]
    if beacon and len(a.lights) > nl:
        pos = a.lights[nl][0] if a.state != 'planted' else a.lights[-1][0]
        if not hasattr(a, 'sprites'):
            a.sprites = []
        big = a.state == 'planted'
        a.sprites.append((pos, (0.03 if big else 0.017) * s, (0.9, 0.15, 0.07) if big else (0.6, 0.1, 0.05), FLARE))
    return gl, R


PL.console = console_art


# bodies: palettes sampled from real textures
def hp_stock_tex_art():
    t = T('powerups/healthpack/bitmaps/healthpack', 'A body diffuse, recoloured by recipe (warthog olive)').copy()
    c = t[..., :3]
    cross = (c[..., 0] > 0.45) & (c[..., 1] < 0.35) & (c[..., 2] < 0.35)
    lum = c.mean(-1)
    lum[cross] = np.median(lum[~cross])
    t[..., :3] = lum[..., None] * OLIVE * 1.3
    return t


def tsce_tex_art():
    t = T(TS + 'cmt/powerups/h_powerup_pack/_health/bitmaps/h_powerup_pack_health_diff', 'B body diffuse, recoloured by recipe').copy()
    c = t[..., :3]
    r, g, b = c[..., 0], c[..., 1], c[..., 2]
    yel = (r > 0.55) & (g > 0.45) & (b < 0.3)
    ys, xs = np.nonzero(yel)
    box_ = np.zeros_like(yel)
    py, px = int((ys.max() - ys.min()) * 0.08), int((xs.max() - xs.min()) * 0.08)
    box_[max(0, ys.min() - py):ys.max() + py + 1, max(0, xs.min() - px):xs.max() + px + 1] = True
    lum = c.mean(-1)
    sat = c.max(-1) - c.min(-1)
    red = (r > 0.45) & (g < 0.3) & (b < 0.3)
    gm = STEEL / STEEL.mean() * 0.5
    out = lum[..., None] * gm
    out[box_ & (sat > 0.12)] = np.median(lum[box_ & (sat < 0.1)]) * gm
    o = red & ~box_
    out[o] = ORANGE * np.clip(lum[o] * 1.6, 0.6, 1.1)[:, None]
    t[..., :3] = out
    return t


PL.hp_stock_tex = hp_stock_tex_art
PL.tsce_tex = tsce_tex_art


def coil_art(a, xf, tag='coil'):
    """mini coil from real textures: Blood Gulch cap_metal body (+ corroded detail), tech rack plate caps,
    Blood Gulch base light panel as the glowing band, a Pelican-sheet DANGER label on one coil"""
    H, R = 0.11, 0.032
    st = a.state
    body = mat(tex=CAPMETAL, spec=0.45, gloss=40, detail=CORRODE, detail_tile=(2, 1), detail_amt=0.7,
               normal=T(PL.NRM_SCR), normal_tile=(2, 1), normal_amt=1.3, wear=1.0)
    capm = plate_mat()
    cap = [(0, 0), (R + 0.003, 0), (R + 0.003, 0.014), (R, 0.016)]
    cap2 = [(R, H - 0.016), (R + 0.003, H - 0.014), (R + 0.003, H), (0.012, H), (0.012, H + 0.006), (0.0, H + 0.006)]
    for prof, m in ((cap, capm), ([(R, 0.016), (R, 0.044)], body), ([(R, 0.068), (R, H - 0.016)], body), (cap2, capm)):
        Pp, Np, UVp, Tp = V.lathe(prof, k=28, u_scale=1)
        UVp[:, 1] = UVp[:, 1] / H
        a.add(Pp, Np, UVp, Tp, m, xf, tag=tag)
    g = 0.8 if st == 'planted' else 0.6
    bm = mat(tex=BAND, emissive=(1, 1, 1), emis_tex=BAND, emis_gain=g)
    Pb, Nb, UVb, Tb = V.lathe([(R + 0.0008, 0.044), (R + 0.0008, 0.068)], k=28, u_scale=2)
    UVb[:, 1] = (UVb[:, 1] - 0.044) / 0.024
    a.add(Pb, Nb, UVb, Tb, bm, xf, tag=tag)
    if getattr(a, '_danger', 0) == 0:
        a._danger = 1
        PL.add_decal(a, DANGER_SRC, 'coil danger', xf.p(np.array([[R + 0.0004, 0, 0.086]]))[0], xf.R[:, 0],
                     xf.R[:, 2], 0.012)
    if st == 'planted':
        a.lights.append((xf.p(np.array([[0, 0, 0.056]]))[0], (0.55, 0.3, 0.08), 0.05))
    return xf.p(np.array([[0, 0, H + 0.006]]))[0]


PL.coil_p = coil_art

# A/B: one sparse UNSC word at a lid corner
_A3, _B3 = PL.A3, PL.B3


def A3_art(st):
    a, info = _A3(st)
    PL.add_decal(a, PL.D_UNSC_WORD, 'A unsc', (-0.05, 0.058, 0.0802), (0, 0, 1), (-1, 0, 0), 0.026, tint=(0.85, 0.85, 0.8))
    return a, info


def B3_art(st):
    a, info = _B3(st)
    PL.add_decal(a, PL.D_UNSC_WORD, 'B unsc', (-0.05, 0.055, 0.0712), (0, 0, 1), (-1, 0, 0), 0.026, tint=(0.85, 0.85, 0.8))
    return a, info


PL.A3, PL.B3 = A3_art, B3_art
PL.BUILD3['A3'] = (A3_art,) + PL.BUILD3['A3'][1:]
PL.BUILD3['B3'] = (B3_art,) + PL.BUILD3['B3'][1:]


# webbing / tape keep TSC:E ammo pack sources (real); harness uses the same webbing


# ---------------------------------------------------------------- rendering with real ground

def render_g(*args, **kw):
    kw.setdefault('ground_layers', GROUND)
    kw.setdefault('ground_col', tuple(np.clip(GROUND_COL * 0.62, 0, 1)))
    kw.setdefault('ground_fade', (0.55, 1.25))
    return E.render(*args, **kw)


M2.render = render_g
BPK.render = render_g


def setup():
    os.makedirs(OUT4, exist_ok=True)
    PL.setup()
    M2.OUT2 = OUT4
    BPK.OUT = OUT4


def sheet():
    from PIL import Image, ImageDraw, ImageFont
    B = ImageFont.truetype('/usr/share/fonts/noto/NotoSans-Bold.ttf', 26)
    Rf = ImageFont.truetype('/usr/share/fonts/noto/NotoSans-Regular.ttf', 18)
    rows = [('E1P', ['hero', 'planted', 'backpack_rear', 'backpack_side']), ('D1P', ['hero', 'planted', 'backpack_rear']),
            ('A3', ['hero', 'planted', 'backpack_rear']), ('B3', ['hero', 'planted', 'backpack_rear']),
            ('G1H', ['hero', 'planted', 'carried']), ('G2H', ['hero', 'planted', 'carried'])]
    tw, th = 540, 405
    im = Image.new('RGB', (4 * (tw + 10) + 10, 84 + len(rows) * (th + 10)), (16, 17, 21))
    d = ImageDraw.Draw(im)
    d.text((14, 12), 'Art-direction pass: real CE / digsite / TSC:E textures, stencils and effects only', font=B,
           fill=(230, 233, 240))
    d.text((14, 48), 'hero | planted on Blood Gulch ground | on the back (backpack) | E1-S side profile', font=Rf,
           fill=(150, 158, 172))
    for r, (v, ks) in enumerate(rows):
        for c, k in enumerate(ks):
            t = Image.open(OUT4 + f'{v}_{k}.png').resize((tw, th), Image.LANCZOS)
            im.paste(t, (10 + c * (tw + 10), 84 + r * (th + 10)))
    im.save(OUT4 + 'sheet.png')
    print(OUT4 + 'sheet.png', im.size)


if __name__ == '__main__':
    setup()
    cmd = sys.argv[1]
    if cmd == 'views':
        for v in sys.argv[2:]:
            t0 = time.time()
            M2.run2(v, ('hero', 'planted') if v not in ('G1H', 'G2H') else ('hero', 'planted', 'carried'))
            print(v, round(time.time() - t0, 1), 's', flush=True)
    elif cmd == 'backpack':
        for v in sys.argv[2:]:
            BPK.render_bp(v)
    elif cmd == 'sheet':
        sheet()
    elif cmd == 'textures':
        for k in PL.BUILD3:
            for st in ('hero', 'planted'):
                PL.BUILD3[k][0](st)
        for k in ('E1P', 'D1P', 'A3', 'B3'):
            BPK.BP[k][0]('carried')
        for k in sorted(PL.USED):
            lic, notes = PL.USED[k]
            print('| `%s` | %s | %s |' % (k, lic, '; '.join(sorted(notes))))
