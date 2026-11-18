#!/usr/bin/env python3
"""Rebuild the recipes' MCC.6 source archive from the two released archives.

CMT's MCC.6 source release has its cmt/ui/hudv4.1/ HUD art in a format the
pack tools cannot read (BC7 and A8Y8 bitmaps), so the packs take that one
subtree from the 1.4.1c release instead. The result is MCC.6 with every
member under evolved-mcc/tags/cmt/ui/hudv4.1/ removed and the 1.4.1c files of
the same subtree appended (renamed under evolved-mcc/tags/, deflated, in the
1.4.1c archive's order, with a fixed time stamp). The shared impact effects
(cmt/effects/impacts/*.effect that 1.4.1c also has) are taken from 1.4.1c the
same way: MCC.6 made the gore parts of 13 of them appear in the non-violent
mode too (their create-in-mode flag, either instead of violent only), and the
packs keep every pack's shared effects the same. Every other member is copied
byte for byte. The output is asserted against the pinned sha256.

    mcc6_source.py <MCC.6 source release zip> <1.4.1c source release zip> <out zip>
"""
import hashlib
import sys
import zipfile

MCC6_SHA256 = "042fefa5ad40aa7faa53a6ee3c551c585c377ed7d15ba7ce570626fff6282a8a"
C141_SHA256 = "311518659c4551342c867da2bb83c4f9c3b8c798604169a4301422970fede8a9"
OUT_SHA256 = "5e0d27e875d9f96335775ce91d8694b6de213dde82c64dfa68c3cba51fd00201"
SUBTREES = ("cmt/ui/hudv4.1/", "cmt/effects/impacts/")
MCC6_ROOT = "evolved-mcc/tags/"
C141_ROOT = "tsce_1-4-1c_source_release/tags/"
STAMP = (2026, 11, 5, 1, 3, 48)


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest()


def raw_extents(zf, path):
    """Return {name: (start, end)} byte ranges of each member's local record."""
    infos = sorted(zf.infolist(), key=lambda i: i.header_offset)
    ends = [i.header_offset for i in infos[1:]] + [zf.start_dir]
    return {i.filename: (i.header_offset, e) for i, e in zip(infos, ends)}


def build(mcc6, c141, out):
    for path, want in ((mcc6, MCC6_SHA256), (c141, C141_SHA256)):
        got = sha256(path)
        if got != want:
            raise SystemExit("%s: sha256 %s, expected %s" % (path, got, want))
    with zipfile.ZipFile(mcc6) as a, zipfile.ZipFile(c141) as c, \
            zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as o, \
            open(mcc6, "rb") as raw:
        ext = raw_extents(a, mcc6)
        c141_names = set(c.namelist())
        mcc6_names = set(a.namelist())
        for i in a.infolist():
            if i.filename.startswith(MCC6_ROOT + SUBTREES[0]) or (
                    i.filename.startswith(MCC6_ROOT + SUBTREES[1]) and not i.filename.endswith("/") and
                    C141_ROOT + i.filename[len(MCC6_ROOT):] in c141_names):
                continue
            s, e = ext[i.filename]
            raw.seek(s)
            data = raw.read(e - s)
            i.header_offset = o.fp.tell()
            o.fp.write(data)
            o.filelist.append(i)
            o.NameToInfo[i.filename] = i
            o.start_dir = o.fp.tell()
        for i in c.infolist():
            if not any(i.filename.startswith(C141_ROOT + t) for t in SUBTREES) or i.filename.endswith("/"):
                continue
            if i.filename.startswith(C141_ROOT + SUBTREES[1]) and (
                    MCC6_ROOT + i.filename[len(C141_ROOT):]) not in mcc6_names:
                continue
            zi = zipfile.ZipInfo(MCC6_ROOT + i.filename[len(C141_ROOT):], STAMP)
            zi.compress_type = zipfile.ZIP_DEFLATED
            zi.external_attr = 0o600 << 16
            o.writestr(zi, c.read(i))
    got = sha256(out)
    if got != OUT_SHA256:
        raise SystemExit("output sha256 %s, expected %s" % (got, OUT_SHA256))
    print("ok", out, got)


if __name__ == "__main__":
    if len(sys.argv) != 4:
        raise SystemExit(__doc__)
    build(*sys.argv[1:])
