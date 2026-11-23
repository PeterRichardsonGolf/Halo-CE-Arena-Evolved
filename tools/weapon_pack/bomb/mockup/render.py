import sys, os, math
import numpy as np
from PIL import Image
sys.path.insert(0, os.path.dirname(__file__))
from gbx import read_gbx
def strip_tris(tris):
    idx = [i for t in tris for i in t]
    out = []; run = []
    for i in idx + [-1]:
        if i == -1:
            for k in range(len(run) - 2):
                a, b, c = run[k], run[k+1], run[k+2]
                if len({a, b, c}) < 3: continue
                out.append((a, b, c) if k % 2 == 0 else (a, c, b))
            run = []
        else: run.append(i)
    return out
def collect(path):
    regions, geoms = read_gbx(path)
    P = []; N = []; T = []; off = 0
    for rn, perms in regions:
        if not perms: continue
        pn, lods = perms[0]
        gi = lods[4] if lods[4] >= 0 else max((l for l in lods), default=0)
        if gi < 0 or gi >= len(geoms): gi = 0
        for shader, verts, tris in geoms[gi]:
            if not verts: continue
            v = np.array(verts, dtype=np.float64)
            tt = strip_tris(tris)
            if not tt: continue
            P.append(v[:, 0:3]); N.append(v[:, 3:6]); T.append(np.array(tt) + off); off += len(v)
    if not P: return None
    return np.vstack(P), np.vstack(N), np.vstack(T)
def vnormals(P, T):
    N = np.zeros_like(P)
    fn = np.cross(P[T[:,1]] - P[T[:,0]], P[T[:,2]] - P[T[:,0]])
    for k in range(3): np.add.at(N, T[:,k], fn)
    return N
def render(path, out, **kw):
    c = collect(path)
    if c is None: return False
    return render_arrays(c[0], c[1], c[2], out, **kw)
def render_arrays(P, N, T, out, size=480, yaw=-30, pitch=18, color=(120, 150, 200), autoaxis=False, flip=False):
    if N is None: N = vnormals(P, T)
    if autoaxis:
        e = P.max(0) - P.min(0)
        if e[1] > e[0]:
            P = np.stack([P[:,1], -P[:,0], P[:,2]], 1); N = np.stack([N[:,1], -N[:,0], N[:,2]], 1)
    if flip:
        P = P * np.array([-1, -1, 1]); N = N * np.array([-1, -1, 1])
    ctr = (P.min(0) + P.max(0)) / 2; P = P - ctr
    # model: x forward, y left, z up. camera looks from the side (+y... view from -y) with yaw/pitch
    ya, pa = math.radians(yaw), math.radians(pitch)
    Rz = np.array([[math.cos(ya), -math.sin(ya), 0], [math.sin(ya), math.cos(ya), 0], [0, 0, 1]])
    # view axes: screen x = model x (rotated), screen y = model z, depth = model y
    P2 = P @ Rz.T; N2 = N @ Rz.T
    Rx = np.array([[1, 0, 0], [0, math.cos(pa), -math.sin(pa)], [0, math.sin(pa), math.cos(pa)]])
    P2 = P2 @ Rx.T; N2 = N2 @ Rx.T
    sx = P2[:, 0]; sy = P2[:, 2]; dz = P2[:, 1]   # depth: smaller y = closer to camera at -y
    ext = max(sx.max() - sx.min(), sy.max() - sy.min(), 1e-6)
    sc = (size * 0.88) / ext
    X = (sx - (sx.max() + sx.min()) / 2) * sc + size / 2
    Y = size / 2 - (sy - (sy.max() + sy.min()) / 2) * sc
    Z = dz
    L = np.array([-0.4, -0.7, 0.55]); L /= np.linalg.norm(L)
    nrm = np.linalg.norm(N2, axis=1, keepdims=True); nrm[nrm == 0] = 1
    shade = np.clip((N2 / nrm) @ L, 0, 1) * 0.75 + 0.25
    img = np.zeros((size, size, 3), np.float32) + np.array([30, 32, 38], np.float32)
    zb = np.full((size, size), 1e9, np.float32)
    col = np.array(color, np.float32)
    for a, b, c in T:
        xs = X[[a, b, c]]; ys = Y[[a, b, c]]
        x0, x1 = int(max(0, math.floor(xs.min()))), int(min(size - 1, math.ceil(xs.max())))
        y0, y1 = int(max(0, math.floor(ys.min()))), int(min(size - 1, math.ceil(ys.max())))
        if x1 < x0 or y1 < y0: continue
        den = (ys[1] - ys[2]) * (xs[0] - xs[2]) + (xs[2] - xs[1]) * (ys[0] - ys[2])
        if abs(den) < 1e-9: continue
        gx, gy = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
        l0 = ((ys[1] - ys[2]) * (gx - xs[2]) + (xs[2] - xs[1]) * (gy - ys[2])) / den
        l1 = ((ys[2] - ys[0]) * (gx - xs[2]) + (xs[0] - xs[2]) * (gy - ys[2])) / den
        l2 = 1 - l0 - l1
        m = (l0 >= -0.001) & (l1 >= -0.001) & (l2 >= -0.001)
        if not m.any(): continue
        z = l0 * Z[a] + l1 * Z[b] + l2 * Z[c]
        sh = l0 * shade[a] + l1 * shade[b] + l2 * shade[c]
        sub = zb[y0:y1+1, x0:x1+1]
        w = m & (z < sub)
        if not w.any(): continue
        sub[w] = z[w]
        img[y0:y1+1, x0:x1+1][w] = col * sh[w][:, None]
    Image.fromarray(np.clip(img, 0, 255).astype(np.uint8)).save(out)
    return True
if __name__ == '__main__':
    print(render(sys.argv[1], sys.argv[2]))
