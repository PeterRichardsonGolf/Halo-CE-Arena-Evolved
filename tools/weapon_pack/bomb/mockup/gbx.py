import struct, sys
H = 0x40
class R:
    def __init__(s, b): s.b = b; s.p = H
def refl(b, off):
    return struct.unpack_from('>II', b, off)  # count, pointer (ignored)
def read_gbx(path):
    b = open(path, 'rb').read()
    p = H + 232
    base = H
    mk, nd, rg, ge, sh = [struct.unpack_from('>I', b, base + 0xAC + 12 * i)[0] for i in range(5)]
    # markers: array(64) then each instances (32)
    mk_inst = []
    for i in range(mk):
        mk_inst.append(struct.unpack_from('>I', b, p + 64 * i + 52)[0])
    p += 64 * mk
    for n in mk_inst: p += 32 * n
    p += 156 * nd
    # regions
    perm_counts = []
    for i in range(rg): perm_counts.append(struct.unpack_from('>I', b, p + 76 * i + 64)[0])
    rstart = p; p += 76 * rg
    regions = []
    for i, pc in enumerate(perm_counts):
        name = b[rstart + 76 * i: rstart + 76 * i + 32].split(b'\0')[0].decode('latin1')
        perms = []
        pstart = p; p += 88 * pc
        mcs = []
        for j in range(pc):
            o = pstart + 88 * j
            pname = b[o:o+32].split(b'\0')[0].decode('latin1')
            lods = struct.unpack_from('>5h', b, o + 64)  # super low, low, med, high, super high
            mc = struct.unpack_from('>I', b, o + 76)[0]
            perms.append((pname, lods)); mcs.append(mc)
        for mc in mcs: p += 80 * mc
        regions.append((name, perms))
    # geometries
    gstart = p; p += 48 * ge
    pcounts = [struct.unpack_from('>I', b, gstart + 48 * i + 36)[0] for i in range(ge)]
    geoms = []
    for g in range(ge):
        pstart = p; p += 132 * pcounts[g]
        parts = []
        meta = []
        for k in range(pcounts[g]):
            o = pstart + 132 * k
            shader = struct.unpack_from('>h', b, o + 4)[0]
            nu, = struct.unpack_from('>I', b, o + 32); nc, = struct.unpack_from('>I', b, o + 44); nt, = struct.unpack_from('>I', b, o + 56)
            meta.append((shader, nu, nc, nt))
        for shader, nu, nc, nt in meta:
            verts = []
            for v in range(nu):
                verts.append(struct.unpack_from('>3f3f3f3f2f2h2f', b, p)); p += 68
            p += 32 * nc
            tris = []
            for t in range(nt):
                tris.append(struct.unpack_from('>3h', b, p)); p += 6
            parts.append((shader, verts, tris))
        geoms.append(parts)
    return regions, geoms
if __name__ == '__main__':
    r, g = read_gbx(sys.argv[1])
    print(r)
    for gi, parts in enumerate(g):
        print(gi, [(s, len(v), len(t)) for s, v, t in parts])
