"""Where each texel of ZTeam's heavy revolver (the h3b model) lands on the model: the base map's 512 x 512 texels,
each with the model-space point (x, y, z) of the surface drawn with it and the node that surface moves with (-1 for a
texel no surface uses). The h3b model's texture coordinates are a flat projection of the gun's side, so a texel holds
both sides of the gun at one (x, z); where faces stack, the side-facing one (the larger |normal y|) wins. The
revolver's art (revolver_art.py) reads this to carry lights, plates and trim to places on the model.

Our own work, from ZTeam's gbxmodel; reads the tag, writes a NumPy array file (float32, 512 x 512 x 4: x, y, z, node).
Needs numpy and Reclaimer (its mod2 definition): the art stage runs it (make_pack.py --derive-python).
Usage: revolver_texels.py <the h3b mauler_tp.gbxmodel> <out.npy>"""
import sys

import numpy as np
from reclaimer.hek.defs.mod2 import mod2_def

SIZE = 512


def triangles(part):
    """a part's vertices (position, normal, texture coordinates, node) and its strip as triangles"""
    vertices = [[float(x) for x in v[0:15]] for v in part.uncompressed_vertices.STEPTREE]
    flat = []
    for entry in part.triangles.STEPTREE:
        flat += [entry[0], entry[1], entry[2]]
    tris = []
    for i in range(len(flat) - 2):
        a, b, c = flat[i:i + 3]
        if -1 in (a, b, c) or a == b or b == c or a == c or max(a, b, c) >= len(vertices):
            continue
        tris.append([a, b, c] if i % 2 == 0 else [b, a, c])
    return vertices, tris


def texels(path):
    tag = mod2_def.build(filepath=path)
    data = tag.data.tagdata
    u_scale = float(data.base_map_u_scale) or 1.0
    v_scale = float(data.base_map_v_scale) or 1.0
    P, UV, NODE, T = [], [], [], []
    for part in data.geometries.STEPTREE[0].parts.STEPTREE:
        vertices, tris = triangles(part)
        base = len(P)
        for v in vertices:
            P.append(v[0:3])
            UV.append((v[12] * u_scale, v[13] * v_scale))
            NODE.append(v[14])
        T += [[base + a, base + b, base + c] for a, b, c in tris]
    P, UV, NODE, T = np.array(P, np.float64), np.array(UV, np.float64), np.array(NODE), np.array(T)
    out = np.full((SIZE, SIZE, 4), np.nan, np.float32)
    out[..., 3] = -1
    normals = np.cross(P[T[:, 1]] - P[T[:, 0]], P[T[:, 2]] - P[T[:, 0]])
    normals /= np.linalg.norm(normals, axis=1, keepdims=True) + 1e-12
    for t in np.argsort(np.abs(normals[:, 1]), kind="stable"):
        a, b, c = T[t]
        if np.ptp(UV[[a, b, c], 1]) > 0.5:
            continue    # (the rear knob's strips wrap the whole texture's height: they take no texel)
        A, B, C = UV[a] * SIZE, UV[b] * SIZE, UV[c] * SIZE
        m = np.array([[B[0] - A[0], C[0] - A[0]], [B[1] - A[1], C[1] - A[1]]])
        if abs(np.linalg.det(m)) < 1e-9:
            continue
        inverse = np.linalg.inv(m)
        x0, x1 = int(np.floor(min(A[0], B[0], C[0]))), int(np.ceil(max(A[0], B[0], C[0])))
        y0, y1 = int(np.floor(min(A[1], B[1], C[1]))), int(np.ceil(max(A[1], B[1], C[1])))
        xs, ys = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
        dx, dy = xs - A[0], ys - A[1]
        s = inverse[0, 0] * dx + inverse[0, 1] * dy
        r = inverse[1, 0] * dx + inverse[1, 1] * dy
        inside = (s >= -0.02) & (r >= -0.02) & (s + r <= 1.02)
        if not inside.any():
            continue
        X = np.floor(xs[inside]).astype(int) % SIZE
        Y = np.clip(np.floor(ys[inside]).astype(int), 0, SIZE - 1)
        S, R = s[inside][:, None], r[inside][:, None]
        out[Y, X, 0:3] = P[a] + S * (P[b] - P[a]) + R * (P[c] - P[a])
        out[Y, X, 3] = NODE[a]
    return out


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    with open(sys.argv[2], "wb") as handle:
        np.save(handle, texels(sys.argv[1]), allow_pickle=False)
