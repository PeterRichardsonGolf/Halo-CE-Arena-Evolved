"""stock cyborg: nodes, skin weights, simple arm posing (FK about bind-pose joints)"""
import struct, math
import numpy as np
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from engine import TAGS, load_gbx, rot, strip_tris
from gbx import read_gbx

PATH = 'characters/cyborg/cyborg'


def nodes():
    b = open(TAGS + PATH + '.gbxmodel', 'rb').read()
    H = 0x40
    p = H + 232
    mk, nd = [struct.unpack_from('>I', b, H + 0xAC + 12 * i)[0] for i in range(2)]
    inst = [struct.unpack_from('>I', b, p + 64 * i + 52)[0] for i in range(mk)]
    p += 64 * mk + 32 * sum(inst)
    out = []
    for i in range(nd):
        o = p + 156 * i
        name = b[o:o + 32].split(b'\0')[0].decode()
        sib, ch, par = struct.unpack_from('>3h', b, o + 32)
        tr = np.array(struct.unpack_from('>3f', b, o + 40))
        q = np.array(struct.unpack_from('>4f', b, o + 52))
        out.append((name, par, tr, q))
    return out


def qmat(q, conj):
    x, y, z, w = q
    if conj:
        x, y, z = -x, -y, -z
    return np.array([[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                     [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                     [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]])


def bind(conj=True):
    ns = nodes()
    R = [None] * len(ns)
    T = [None] * len(ns)
    for i, (n, par, tr, q) in enumerate(ns):
        if par < 0:
            R[i], T[i] = qmat(q, conj), tr
        else:
            R[i] = R[par] @ qmat(q, conj)
            T[i] = T[par] + R[par] @ tr
    return ns, np.array(T), R


def skinned_parts():
    """[(shader, P, N, UV, T, n0, n1, w0)] first permutation, top LOD (raw vertex arrays)"""
    full = TAGS + PATH + '.gbxmodel'
    regions, geoms = read_gbx(full)
    out = []
    for rn, perms in regions:
        if not perms:
            continue
        pn, lods = perms[0]
        gi = lods[4] if lods[4] >= 0 else 0
        for sh, verts, tris in geoms[gi]:
            if not verts:
                continue
            v = np.array(verts, dtype=np.float64)
            tt = strip_tris(tris)
            if not tt:
                continue
            out.append((sh, v[:, 0:3], v[:, 3:6], v[:, 12:14], np.array(tt), v[:, 14].astype(int), v[:, 15].astype(int), v[:, 16]))
    return out
